#include <driver/apic/apic.h>
#include <acpi/acpi.h>
#include <debug/debug.h>
#include <arch.h>

static volatile uint32_t* lapic_mmio = 0; // identity-mapped phys assumed
static uintptr_t lapic_base_phys = 0;

/* IA32_APIC_BASE MSR */
#define IA32_APIC_BASE_MSR       0x1B
#define IA32_APIC_BASE_ENABLE    (1ull << 11)
#define IA32_APIC_BASE_X2APIC    (1ull << 10)

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline void wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t lo = (uint32_t)(value & 0xFFFFFFFFu);
    uint32_t hi = (uint32_t)(value >> 32);
    __asm__ __volatile__("wrmsr" :: "c"(msr), "a"(lo), "d"(hi));
}

void lapic_set_base(uintptr_t phys)
{
    lapic_base_phys = phys;
    lapic_mmio = (volatile uint32_t*)(phys);
    LOG("LAPIC base set: %p", (void*)phys);
}

static inline void lapic_mmio_write(uint32_t reg, uint32_t value)
{
    lapic_mmio[reg / 4] = value;
    (void)lapic_mmio[LAPIC_REG_ID / 4]; // read to force posting
}

static inline uint32_t lapic_mmio_read(uint32_t reg)
{
    return lapic_mmio[reg / 4];
}

void lapic_write(uint32_t reg, uint32_t value)
{
    if (!lapic_mmio) return;
    lapic_mmio_write(reg, value);
}

uint32_t lapic_read(uint32_t reg)
{
    if (!lapic_mmio) return 0;
    return lapic_mmio_read(reg);
}

void lapic_enable_controller(void)
{
    /* Ensure APIC globally enabled via MSR and base programmed */
    uint64_t apic_base = rdmsr(IA32_APIC_BASE_MSR);
    bool x2apic_was_enabled = (apic_base & IA32_APIC_BASE_X2APIC) != 0;
    if (x2apic_was_enabled) {
        /* Our LAPIC ops use xAPIC MMIO; ensure x2APIC is off so MMIO works. */
        apic_base &= ~IA32_APIC_BASE_X2APIC;
        wrmsr(IA32_APIC_BASE_MSR, apic_base);
        LOG("LAPIC: x2APIC was enabled, disabling to use xAPIC MMIO");
        apic_base = rdmsr(IA32_APIC_BASE_MSR);
    }
    if (!(apic_base & IA32_APIC_BASE_ENABLE)) {
        apic_base |= IA32_APIC_BASE_ENABLE;
        wrmsr(IA32_APIC_BASE_MSR, apic_base);
    }

    /* If MSR base present and we don't have a base yet, adopt it */
    uintptr_t msr_base_phys = (uintptr_t)(apic_base & 0xFFFFF000ULL);
    if (!lapic_mmio && msr_base_phys) {
        lapic_set_base(msr_base_phys);
    }
    if (!lapic_mmio) return;

    /* TPR=0 to allow all priorities */
    lapic_write(0x080, 0x00);

    /* Mask LINT0/LINT1 to avoid spurious ExtINT/NMI unless configured */
    uint32_t lvt;
    lvt = lapic_read(LAPIC_REG_LVT_LINT0); lvt |= (1u << 16); lapic_write(LAPIC_REG_LVT_LINT0, lvt);
    lvt = lapic_read(LAPIC_REG_LVT_LINT1); lvt |= (1u << 16); lapic_write(LAPIC_REG_LVT_LINT1, lvt);

    /* Set Spurious Interrupt Vector Register: enable APIC with vector 0xFF */
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    svr &= ~0xFFu; // set vector explicitly
    svr |= 0xFF;   // spurious vector 0xFF
    svr |= LAPIC_SVR_APIC_ENABLE;
    lapic_write(LAPIC_REG_SVR, svr);
}

void lapic_disable_controller(void)
{
    if (!lapic_mmio) return;
    uint32_t svr = lapic_read(LAPIC_REG_SVR);
    svr &= ~LAPIC_SVR_APIC_ENABLE;
    lapic_write(LAPIC_REG_SVR, svr);
}

void lapic_eoi(void)
{
    if (!lapic_mmio) return;
    lapic_write(LAPIC_REG_EOI, 0);
}

uint8_t lapic_get_id(void)
{
    if (!lapic_mmio) return 0;
    uint32_t v = lapic_read(LAPIC_REG_ID);
    return (uint8_t)(v >> 24);
}
