#include <driver/apic/apic.h>
#include <debug/debug.h>

typedef struct {
    volatile uint8_t* mmio;
    uint32_t gsi_base;
    uint32_t redirs; // number of redirection entries
} ioapic_desc_t;

static ioapic_desc_t s_ioapics[8];
static uint32_t s_ioapic_count = 0;

void ioapic_set_base(uintptr_t phys, uint32_t gsi_base)
{
    if (s_ioapic_count >= 8) {
        WARN("IOAPIC: too many controllers, ignoring %p", (void*)phys);
        return;
    }
    // Aynı fiziki tabanı iki kez eklemeyi önle
    for (uint32_t i = 0; i < s_ioapic_count; ++i) {
        if ((uintptr_t)s_ioapics[i].mmio == phys) {
            LOG("IOAPIC: base %p already registered (index %u), skipping", (void*)phys, i);
            return;
        }
    }
    s_ioapics[s_ioapic_count].mmio = (volatile uint8_t*)phys;
    s_ioapics[s_ioapic_count].gsi_base = gsi_base;
    // read version to determine redirection entries
    uint32_t ver;
    // temp helpers use this mmio
    *(volatile uint32_t*)(s_ioapics[s_ioapic_count].mmio + IOAPIC_MMIO_IOREGSEL) = IOAPIC_REG_VER;
    ver = *(volatile uint32_t*)(s_ioapics[s_ioapic_count].mmio + IOAPIC_MMIO_IOWIN);
    s_ioapics[s_ioapic_count].redirs = ((ver >> 16) & 0xFF) + 1;
    LOG("IOAPIC[%u]: base=%p GSI base=%u entries=%u", s_ioapic_count, (void*)phys, gsi_base, s_ioapics[s_ioapic_count].redirs);
    s_ioapic_count++;
}

static inline void ioapic_write_sel(volatile uint8_t* mmio, uint32_t reg) { *(volatile uint32_t*)(mmio + IOAPIC_MMIO_IOREGSEL) = reg; }
static inline void ioapic_write_win(volatile uint8_t* mmio, uint32_t value) { *(volatile uint32_t*)(mmio + IOAPIC_MMIO_IOWIN) = value; }
static inline uint32_t ioapic_read_win(volatile uint8_t* mmio) { return *(volatile uint32_t*)(mmio + IOAPIC_MMIO_IOWIN); }

static int ioapic_index_for_gsi(uint32_t gsi)
{
    for (uint32_t i = 0; i < s_ioapic_count; ++i) {
        uint32_t base = s_ioapics[i].gsi_base;
        uint32_t end = base + s_ioapics[i].redirs; // exclusive
        if (gsi >= base && gsi < end) return (int)i;
    }
    return -1;
}

uint32_t ioapic_read(uint32_t reg)
{
    if (s_ioapic_count == 0) return 0;
    // default to first ioapic
    ioapic_write_sel(s_ioapics[0].mmio, reg);
    return ioapic_read_win(s_ioapics[0].mmio);
}

void ioapic_write(uint32_t reg, uint32_t value)
{
    if (s_ioapic_count == 0) return;
    ioapic_write_sel(s_ioapics[0].mmio, reg);
    ioapic_write_win(s_ioapics[0].mmio, value);
}

uint32_t ioapic_max_redirs(void)
{
    if (s_ioapic_count == 0) return 0;
    return s_ioapics[0].redirs;
}

static inline uint64_t ioapic_read_redir_idx(int apic_idx, uint32_t index)
{
    volatile uint8_t* mmio = s_ioapics[apic_idx].mmio;
    ioapic_write_sel(mmio, IOAPIC_REG_REDIR(index));
    uint32_t low = ioapic_read_win(mmio);
    ioapic_write_sel(mmio, IOAPIC_REG_REDIR(index) + 1);
    uint32_t high = ioapic_read_win(mmio);
    return ((uint64_t)high << 32) | low;
}

static inline void ioapic_write_redir_idx(int apic_idx, uint32_t index, uint64_t value)
{
    volatile uint8_t* mmio = s_ioapics[apic_idx].mmio;
    ioapic_write_sel(mmio, IOAPIC_REG_REDIR(index));
    ioapic_write_win(mmio, (uint32_t)(value & 0xFFFFFFFF));
    ioapic_write_sel(mmio, IOAPIC_REG_REDIR(index) + 1);
    ioapic_write_win(mmio, (uint32_t)(value >> 32));
}

void ioapic_set_redir(uint32_t gsi, uint8_t vector, uint8_t lapic_id, uint32_t flags, bool mask)
{
    int apic_idx = ioapic_index_for_gsi(gsi);
    if (apic_idx < 0) { WARN("IOAPIC: no controller for GSI %u", gsi); return; }
    uint32_t index = gsi - s_ioapics[apic_idx].gsi_base;
    if (index >= s_ioapics[apic_idx].redirs) { WARN("IOAPIC: redir index OOB %u for GSI %u", index, gsi); return; }
    uint64_t entry = 0;

    // Low dword
    entry |= (uint64_t)vector;          // vector
    entry |= (uint64_t)flags;           // delivery/polarity/trigger
    if (mask) entry |= (uint64_t)IOAPIC_REDIR_MASKED;

    // High dword: destination APIC ID
    entry |= ((uint64_t)lapic_id) << 56;

    ioapic_write_redir_idx(apic_idx, index, entry);
}

void ioapic_mask_gsi(uint32_t gsi, bool mask)
{
    int apic_idx = ioapic_index_for_gsi(gsi);
    if (apic_idx < 0) return;
    uint32_t index = gsi - s_ioapics[apic_idx].gsi_base;
    uint64_t e = ioapic_read_redir_idx(apic_idx, index);
    if (mask) e |= (uint64_t)IOAPIC_REDIR_MASKED; else e &= ~((uint64_t)IOAPIC_REDIR_MASKED);
    ioapic_write_redir_idx(apic_idx, index, e);
}

bool ioapic_is_masked(uint32_t gsi)
{
    int apic_idx = ioapic_index_for_gsi(gsi);
    if (apic_idx < 0) return true;
    uint32_t index = gsi - s_ioapics[apic_idx].gsi_base;
    uint64_t e = ioapic_read_redir_idx(apic_idx, index);
    return (e & IOAPIC_REDIR_MASKED) != 0;
}
