#include <driver/apic/apic.h>
#include <driver/DriverBase.h>
#include <irq/IRQ.h>
#include <acpi/acpi.h>
#include <arch.h>
#include <debug/debug.h>

/* Basit tek IOAPIC ve tek LAPIC varsayımı ile minimal APIC sürücüsü */

static uint32_t s_gsi_base = 0;      // IOAPIC'in GSI base'i (MADT'den)
static uint32_t s_gsi_count = 24;    // tahmini; IOAPIC ver registerinden okunur
static uint8_t  s_lapic_id = 0;
static bool     s_apic_ready = false;

typedef struct { uint32_t gsi; uint32_t flags; } irq_route_t;
static irq_route_t s_irq_map[24]; // ISA (0..15) + pay if needed
static uint32_t s_gsi_to_irq[256]; // simple reverse map for GSIs within small range
#define GSI_UNMAPPED 0xFFFFFFFFu

bool apic_supported()
{
    size_t regA, regB, regC, regD;
    arch_cpuid(1, &regA, &regB, &regC, &regD);

    return (regD & (1 << 9)) != 0; // APIC bit
}

static bool apic_madt_setup(void)
{
    const acpi_madt* madt = acpi_get_madt();
    if (!madt) {
        WARN("APIC: MADT not found, cannot init");
        return false;
    }

    uintptr_t lapic_base = madt->LocalApicAddress;
    lapic_set_base(lapic_base);
    s_lapic_id = lapic_get_id();
    LOG("APIC: LAPIC id=%u base=%p", s_lapic_id, (void*)lapic_base);

    /* MADT entryleri içinde IOAPIC ve (varsa) LAPIC address override ara */
    const uint8_t* p = madt->Entries;
    const uint8_t* end = ((const uint8_t*)madt) + madt->Header.Length;

    uintptr_t ioapic_base = 0;
    uint32_t  ioapic_gsi_base = 0;

    // Varsayılan IRQ->GSI eşlemeleri ve bayraklar (ISA: IRQ==GSI kabul edilir)
    for (uint32_t i = 0; i < 24; ++i) {
        s_irq_map[i].gsi = i;
        s_irq_map[i].flags = 0; // edge/high default
    }
    // GSI reverse map başlangıçta boş
    for (uint32_t i = 0; i < 256; ++i) s_gsi_to_irq[i] = GSI_UNMAPPED;

    while (p + sizeof(acpi_madt_entry_header) < end) {
        const acpi_madt_entry_header* h = (const acpi_madt_entry_header*)p;
        if (h->Length == 0) break;

        switch (h->Type) {
            case ACPI_MADT_IO_APIC: {
                if (h->Length >= 12) {
                    const struct { uint8_t Type, Length; uint8_t IoApicId; uint8_t Reserved; uint32_t Address; uint32_t GsiBase; } __attribute__((packed)) *e = (void*)p;
                    ioapic_base = e->Address;
                    ioapic_gsi_base = e->GsiBase;
                    LOG("APIC: IOAPIC id=%u base=%p gsi_base=%u", e->IoApicId, (void*)(uintptr_t)e->Address, e->GsiBase);
                }
                break;
            }
            case ACPI_MADT_LOCAL_APIC_ADDRESS_OVERRIDE: {
                if (h->Length >= 12) {
                    const struct { uint8_t Type, Length; uint16_t Reserved; uint64_t Address; } __attribute__((packed)) *e = (void*)p;
                    lapic_set_base((uintptr_t)e->Address);
                    LOG("APIC: LAPIC address override -> %p", (void*)(uintptr_t)e->Address);
                }
                break;
            }
            case ACPI_MADT_INTERRUPT_SOURCE_OVERRIDE: {
                if (h->Length >= 10) {
                    const struct { uint8_t Type, Length; uint8_t Bus; uint8_t SourceIrq; uint32_t Gsi; uint16_t Flags; } __attribute__((packed)) *e = (void*)p;
                    uint8_t src = e->SourceIrq;
                    if (src < 24) {
                        s_irq_map[src].gsi = e->Gsi;
                        // Flags bitleri: [1:0] polarity, [3:2] trigger
                        uint32_t f = 0;
                        uint16_t fl = e->Flags;
                        uint16_t pol = (fl & 0x3);
                        uint16_t trg = (fl >> 2) & 0x3;
                        if (pol == 3 /* active low */) f |= IOAPIC_REDIR_ACTIVE_LOW;
                        if (trg == 3 /* level */) f |= IOAPIC_REDIR_LEVEL;
                        s_irq_map[src].flags = f;
                        LOG("APIC: ISO IRQ%u -> GSI %u (flags=0x%x)", src, e->Gsi, (unsigned)f);
                    }
                }
                break;
            }
            default:
                break;
        }

        p += h->Length;
    }

    if (!ioapic_base) {
        ERROR("APIC: No IOAPIC found in MADT");
        return false;
    }

    ioapic_set_base(ioapic_base, ioapic_gsi_base);
    s_gsi_base = ioapic_gsi_base;
    s_gsi_count = ioapic_max_redirs();
    if (s_gsi_count == 0) {
        ERROR("APIC: IOAPIC not responding (MMIO unmapped?) base=%p", (void*)ioapic_base);
        return false;
    }
    // Not: IOAPIC GSI base, ISA IRQ numaralarıyla aynı olmak zorunda değil;
    // ISO varsa onu kullanırız, yoksa çoğu sistemde GSI==IRQ kabul edilebilir.
    for (uint32_t i = 0; i < 16; ++i) {
        LOG("APIC: map IRQ%u -> GSI%u (flags=0x%x)", i, s_irq_map[i].gsi, (unsigned)s_irq_map[i].flags);
    }

    return true;
}

/* Pre-sanitize: mask PIC and sanitize LAPIC before enabling */
static inline void pic_mask_all(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);
    LOG("APIC: PIC masked (pre)");
}

static void lapic_sanitize_state(void)
{
    /* Block everything until we re-enable */
    lapic_write(0x080, 0xFF); /* TPR = 0xFF */
    /* Mask LVTs we touch */
    uint32_t v;
    v = lapic_read(LAPIC_REG_LVT_LINT0); v |= (1u << 16); lapic_write(LAPIC_REG_LVT_LINT0, v);
    v = lapic_read(LAPIC_REG_LVT_LINT1); v |= (1u << 16); lapic_write(LAPIC_REG_LVT_LINT1, v);
    v = lapic_read(LAPIC_REG_LVT_TIMER); v |= (1u << 16); lapic_write(LAPIC_REG_LVT_TIMER, v);
    v = lapic_read(LAPIC_REG_LVT_ERROR); v |= (1u << 16); lapic_write(LAPIC_REG_LVT_ERROR, v);
    /* Disable SVR and set spurious vector to 0xFF */
    v = lapic_read(LAPIC_REG_SVR);
    v &= ~LAPIC_SVR_APIC_ENABLE;
    v = (v & ~0xFFu) | 0xFFu;
    lapic_write(LAPIC_REG_SVR, v);
    lapic_eoi();
    LOG("APIC: LAPIC sanitized");
}
static void apic_route_legacy_to_apic(void)
{
    // IMCR üzerinden PIC -> APIC yönlendirmesi (eski uyumlu sistemler için)
    // Not: Modern sistemlerde yok sayılabilir; zararsızdır.
    outb(0x22, 0x70);
    outb(0x23, 0x01); // Route to APIC
}

static bool apic_program_legacy_irqs(void)
{
    /* 8259 legacy IRQ'leri (0..15) -> IDT vektörleri 32..47 olarak programla */
    for (uint32_t irq = 0; irq < 16; ++irq) {
        uint8_t vector = 32 + irq;
        uint32_t flags = s_irq_map[irq].flags; // ISO ile güncellenmiş olabilir
        uint32_t gsi = s_irq_map[irq].gsi;
        // Aynı GSI'ye birden fazla IRQ denk geliyorsa (ör. ISO: IRQ0->GSI2, IRQ2 varsayılan GSI2),
        // ilk eşleyen IRQ'yu (küçük numaralıyı) tercih et ve diğerini atla.
        if (gsi < 256 && s_gsi_to_irq[gsi] != GSI_UNMAPPED && s_gsi_to_irq[gsi] != irq) {
            LOG("APIC: GSI%u already mapped to IRQ%u, skipping IRQ%u", gsi, s_gsi_to_irq[gsi], irq);
            continue;
        }

        ioapic_set_redir(gsi, vector, s_lapic_id, flags, true /* start masked */);
        LOG("APIC: route IRQ%u -> GSI%u vector=%u flags=0x%x", irq, gsi, vector, (unsigned)flags);
        if (gsi < 256) s_gsi_to_irq[gsi] = irq;
        idt_reset_gate(vector); // default ISR; gerçek handler register_handler ile yazılır
    }
    return true;
}

/* IRQController callbacks */
static void apic_irqc_init(void)
{
    /* nothing special here; real work in apic_init */
}

static void apic_irqc_enable(uint32_t irq)
{
    /* Unmask corresponding GSI */
    if (irq < 24) ioapic_mask_gsi(s_irq_map[irq].gsi, false);
    LOG("APIC: IRQ%u enabled", (size_t)irq);
    if (irq < 24) ioapic_debug_dump_gsi(s_irq_map[irq].gsi, "after enable");
}

static void apic_irqc_disable(uint32_t irq)
{
    if (irq < 24) ioapic_mask_gsi(s_irq_map[irq].gsi, true);
    LOG("APIC: IRQ%u disabled", (size_t)irq);
    if (irq < 24) ioapic_debug_dump_gsi(s_irq_map[irq].gsi, "after disable");
}

static void apic_irqc_ack(uint32_t irq)
{
    (void)irq; // IOAPIC doesn't need EOI; LAPIC does
    lapic_eoi();
}

static void apic_irqc_setprio(uint32_t irq, uint8_t prio)
{
    (void)irq; (void)prio; // not implemented
}

static uint8_t apic_irqc_getprio(uint32_t irq)
{
    (void)irq; return 0; // not implemented
}

static bool apic_irqc_isen(uint32_t irq)
{
    if (irq >= 24) return false;
    return !ioapic_is_masked(s_irq_map[irq].gsi);
}

static void apic_irqc_reg(uint32_t irq, void (*handler)(void))
{
    if (!handler) return;
    uint8_t vector = 32 + (uint8_t)irq;
    LOG("APIC: register handler IRQ%u -> vector %u @ %p", irq, vector, handler);
    idt_set_gate(vector, (size_t)(uintptr_t)handler);
}

static void apic_irqc_unreg(uint32_t irq)
{
    uint8_t vector = 32 + (uint8_t)irq;
    idt_reset_gate(vector);
}

/* GSI-based ops */
static void apic_irqc_enable_gsi(uint32_t gsi) {
    ioapic_mask_gsi(gsi, false);
}
static void apic_irqc_disable_gsi(uint32_t gsi) {
    ioapic_mask_gsi(gsi, true);
}
static void apic_irqc_ack_gsi(uint32_t gsi) {
    (void)gsi; lapic_eoi();
}
static void apic_irqc_setprio_gsi(uint32_t gsi, uint8_t prio) {
    (void)gsi; (void)prio;
}
static uint8_t apic_irqc_getprio_gsi(uint32_t gsi) {
    (void)gsi; return 0;
}
static bool apic_irqc_isen_gsi(uint32_t gsi) {
    return !ioapic_is_masked(gsi);
}
static void apic_irqc_reg_gsi(uint32_t gsi, void (*handler)(void)) {
    if (!handler) return;
    uint32_t irq = (gsi < 256 && s_gsi_to_irq[gsi] != GSI_UNMAPPED) ? s_gsi_to_irq[gsi] : gsi; // map back if known
    uint8_t vector = 32 + (uint8_t)irq;
    LOG("APIC: register handler GSI%u -> IRQ%u vector %u @ %p", gsi, (unsigned)irq, vector, handler);
    idt_set_gate(vector, (size_t)(uintptr_t)handler);
}
static void apic_irqc_unreg_gsi(uint32_t gsi) {
    uint32_t irq = (gsi < 256 && s_gsi_to_irq[gsi] != GSI_UNMAPPED) ? s_gsi_to_irq[gsi] : gsi;
    uint8_t vector = 32 + (uint8_t)irq;
    idt_reset_gate(vector);
}

/* DriverBase callbacks */
static bool apic_drv_init(void)
{
    /* Ensure PIC masked before we touch APICs */
    pic_mask_all();
    if (!apic_madt_setup()) return false;
    /* Sanitize any firmware state */
    lapic_sanitize_state();
    ioapic_mask_all();
    /* Now enable LAPIC cleanly */
    lapic_enable_controller();
    // Verify LAPIC enable via SVR bit
    uint32_t svr_chk = lapic_read(LAPIC_REG_SVR);
    if (!(svr_chk & LAPIC_SVR_APIC_ENABLE)) {
        ERROR("APIC: LAPIC not enabled (SVR=0x%x)", svr_chk);
        return false;
    }
    // Re-read LAPIC ID after enabling controller; MMIO read may have failed earlier
    // if firmware left the CPU in x2APIC mode. Using a stale 0 would break IOAPIC routing
    // on multi-core VMs by targeting a non-existent APIC ID.
    s_lapic_id = lapic_get_id();
    LOG("APIC: Using LAPIC id=%u for IOAPIC routing", s_lapic_id);
    if (!apic_program_legacy_irqs()) return false;
    apic_route_legacy_to_apic();
    // PIC already masked; keep all GSIs masked until drivers enable
    s_apic_ready = true;
    irq_controller = &apic_irq_controller;
    LOG("APIC: initialized (LAPIC id=%u, GSIs=%u from %u)", s_lapic_id, s_gsi_count, s_gsi_base);
    // Do not unmask any line here
    return true;
}

static void apic_drv_enable(void)
{
    /* nothing specific; enabling an IRQ happens via IRQController */
    apic_driver.enabled = true;
}

static void apic_drv_disable(void)
{
    /* mask all legacy lines */
    for (uint32_t irq = 0; irq < 16; ++irq) {
        uint32_t gsi = (irq < 24) ? s_irq_map[irq].gsi : (s_gsi_base + irq);
        ioapic_mask_gsi(gsi, true);
    }
    apic_driver.enabled = false;
}

/* Public wrappers to match apic.h high-level API */
bool apic_init(void) { return apic_drv_init(); }
void apic_enable(void) { apic_drv_enable(); }
void apic_disable(void) { apic_drv_disable(); }

void apic_enable_irq(uint32_t irq) { apic_irqc_enable(irq); }
void apic_disable_irq(uint32_t irq) { apic_irqc_disable(irq); }
void apic_acknowledge_irq(uint32_t irq) { apic_irqc_ack(irq); }
void apic_set_priority(uint32_t irq, uint8_t prio) { apic_irqc_setprio(irq, prio); }
uint8_t apic_get_priority(uint32_t irq) { return apic_irqc_getprio(irq); }
bool apic_is_enabled(uint32_t irq) { return apic_irqc_isen(irq); }
void apic_register_handler(uint32_t irq, void (*handler)(void)) { apic_irqc_reg(irq, handler); }
void apic_unregister_handler(uint32_t irq) { apic_irqc_unreg(irq); }

/* Exported driver/controller objects */
DriverBase apic_driver = {
    .name = "APIC",
    .version = 1,
    .context = NULL,
    .enabled = false,
    .init = apic_drv_init,
    .enable = apic_drv_enable,
    .disable = apic_drv_disable,
    .type = DRIVER_TYPE_ANY
};

IRQController apic_irq_controller = {
    .name = "APIC IRQ Controller",
    .specific_data = NULL,
    .init = apic_irqc_init,
    .enable = apic_irqc_enable,
    .disable = apic_irqc_disable,
    .acknowledge = apic_irqc_ack,
    .set_priority = apic_irqc_setprio,
    .get_priority = apic_irqc_getprio,
    .is_enabled = apic_irqc_isen,
    .register_handler = apic_irqc_reg,
    .unregister_handler = apic_irqc_unreg,
    .enable_gsi = apic_irqc_enable_gsi,
    .disable_gsi = apic_irqc_disable_gsi,
    .acknowledge_gsi = apic_irqc_ack_gsi,
    .set_priority_gsi = apic_irqc_setprio_gsi,
    .get_priority_gsi = apic_irqc_getprio_gsi,
    .is_enabled_gsi = apic_irqc_isen_gsi,
    .register_handler_gsi = apic_irqc_reg_gsi,
    .unregister_handler_gsi = apic_irqc_unreg_gsi
};
