#include <arch.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/* Low-level helpers                                                          */
/* -------------------------------------------------------------------------- */

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

static inline uint64_t read_cr0(void)
{
    uint32_t value;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(value));
    return (uint64_t)value;
}

static inline void write_cr0(uint64_t value)
{
    uint32_t v32 = (uint32_t)value;
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(v32) : "memory");
}

static inline void wbinvd(void)
{
    __asm__ __volatile__("wbinvd" ::: "memory");
}


// Intel i386 (non-PAE) Page Table Entry (4 KiB page)
// Bit layout per Intel SDM Vol. 3A, Chapter 4 (Paging):
//  0 P, 1 RW, 2 US, 3 PWT, 4 PCD, 5 A, 6 D, 7 PAT, 8 G, 9-11 Avl, 12-31 Frame
typedef struct {
    uint32_t present : 1;        // [0]  Present
    uint32_t rw : 1;             // [1]  Read/Write
    uint32_t user : 1;           // [2]  User/Supervisor
    uint32_t write_through : 1;  // [3]  Page-level Write-Through (PWT)
    uint32_t cache_disabled : 1; // [4]  Page-level Cache Disable (PCD)
    uint32_t accessed : 1;       // [5]  Accessed
    uint32_t dirty : 1;          // [6]  Dirty
    uint32_t pat : 1;            // [7]  Page Attribute Table (for 4 KiB pages)
    uint32_t global : 1;         // [8]  Global (effective if CR4.PGE=1)
    uint32_t available : 3;      // [9:11] Available to software
    uint32_t frame : 20;         // [12:31] Physical frame base address (4 KiB aligned)
} __attribute__((packed)) PTE; // Page Table Entry (4 KiB)

// Intel i386 (non-PAE) Page Directory Entry
// If PS=0 (4 KiB pages via a Page Table):
//  0 P, 1 RW, 2 US, 3 PWT, 4 PCD, 5 A, 6 MBZ (must be 0), 7 PS=0, 8 Ign, 9-11 Avl, 12-31 PT Base
// If PS=1 (4 MiB page):
//  0 P, 1 RW, 2 US, 3 PWT, 4 PCD, 5 A, 6 D, 7 PS=1, 8 G, 9-11 Avl, 12 PAT, 13-20 MBZ, 22-31 Page Base
typedef struct {
    uint32_t present : 1;             // [0]  Present
    uint32_t rw : 1;                  // [1]  Read/Write
    uint32_t user : 1;                // [2]  User/Supervisor
    uint32_t write_through : 1;       // [3]  Page-level Write-Through (PWT)
    uint32_t cache_disabled : 1;      // [4]  Page-level Cache Disable (PCD)
    uint32_t accessed : 1;            // [5]  Accessed
    uint32_t zero_or_dirty : 1;       // [6]  0 if PS=0; Dirty if PS=1 (4 MiB page)
    uint32_t page_size : 1;           // [7]  PS: 0 -> 4 KiB via PT, 1 -> 4 MiB page
    uint32_t ignored_or_global : 1;   // [8]  Ignored if PS=0; Global if PS=1 (with CR4.PGE=1)
    uint32_t available : 3;           // [9:11] Available to software
    uint32_t frame : 20;              // [12:31] PT base (PS=0) or page base [31:12] (PS=1, note PAT at bit 12)
} __attribute__((packed)) PDE; // Page Directory Entry

PTE page_tables[1024][1024] __attribute__((aligned(4096)));
PDE page_directory[1024] __attribute__((aligned(4096)));

void paging_init() {

    for (size_t dir_id = 0; dir_id < 1024; dir_id++) {
        page_directory[dir_id].present = 1;
        page_directory[dir_id].rw = 1; // Read/Write
        page_directory[dir_id].user = 0; // Supervisor only
        page_directory[dir_id].write_through = 0;
        page_directory[dir_id].cache_disabled = 0;
        page_directory[dir_id].accessed = 0;
        page_directory[dir_id].zero_or_dirty = 0;
        page_directory[dir_id].page_size = 0; // Use 4 KiB pages
        page_directory[dir_id].ignored_or_global = 0;
        page_directory[dir_id].available = 0;
        page_directory[dir_id].frame = (uint32_t)&page_tables[dir_id] >> 12; // Set frame base address

        PTE* tables = page_tables[dir_id];

        for (size_t table_id = 0; table_id < 1024; table_id++) {
            tables[table_id].present = 1;
            tables[table_id].rw = 1; // Read/Write
            tables[table_id].user = 0; // Supervisor only
            tables[table_id].write_through = 0;
            tables[table_id].cache_disabled = 0;
            tables[table_id].accessed = 0;
            tables[table_id].dirty = 0;
            tables[table_id].pat = 0; // Use PAT for 4 KiB pages
            tables[table_id].global = 0;
            tables[table_id].available = 0;
            tables[table_id].frame = dir_id * 1024 + table_id;
        }

    }

}

/* === Architecture paging attribute extension (i386) ======================== */
static bool g_pat_initialized = false;

#define IA32_PAT_MSR 0x00000277u

static inline uint64_t pat_set_entry(uint64_t pat, unsigned index, uint8_t value)
{
    uint64_t mask = 0xFFull << (index * 8);
    return (pat & ~mask) | ((uint64_t)value << (index * 8));
}

bool arch_paging_pat_init(void)
{
    if (g_pat_initialized) return true;

    size_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    arch_cpuid(0x00000001u, &eax, &ebx, &ecx, &edx);
    bool has_pat = (edx & (1u << 16)) != 0;
    if (!has_pat) {
        return false;
    }

    uint64_t pat = rdmsr(IA32_PAT_MSR);
    pat = pat_set_entry(pat, 0, 0x06); // WB
    pat = pat_set_entry(pat, 1, 0x04); // WT
    pat = pat_set_entry(pat, 2, 0x07); // UC-
    pat = pat_set_entry(pat, 3, 0x00); // UC
    pat = pat_set_entry(pat, 4, 0x01); // WC
    pat = pat_set_entry(pat, 5, 0x05); // WP
    pat = pat_set_entry(pat, 6, 0x07); // UC- (spare slot)
    pat = pat_set_entry(pat, 7, 0x00); // UC
    wrmsr(IA32_PAT_MSR, pat);

    g_pat_initialized = true;
    return true;
}

/* -------------------------------------------------------------------------- */
/* MTRR management (i386)                                                     */
/* -------------------------------------------------------------------------- */

#define IA32_MTRR_CAP_MSR      0x000000FEu
#define IA32_MTRR_DEF_TYPE_MSR 0x000002FFu
#define IA32_MTRR_PHYSBASE(n) (0x00000200u + ((n) * 2u))
#define IA32_MTRR_PHYSMASK(n) (0x00000200u + ((n) * 2u) + 1u)
#define IA32_MTRR_DEF_ENABLE   (1ull << 11)
#define IA32_MTRR_DEF_FIXED    (1ull << 10)

static bool     g_mtrr_initialized = false;
static bool     g_mtrr_available   = false;
static uint8_t  g_mtrr_var_count   = 0;
static uint8_t  g_mtrr_phys_bits   = 36;
static uint64_t g_mtrr_usage_mask  = 0;

static inline uint8_t bitcount64(uint64_t value)
{
    uint8_t count = 0;
    while (value) {
        value &= (value - 1ull);
        count++;
    }
    return count;
}

static inline uint64_t mtrr_phys_mask_bits(void)
{
    if (g_mtrr_phys_bits >= 52) {
        return 0xFFFFFFFFFFFFF000ull;
    }
    uint64_t usable = (1ull << g_mtrr_phys_bits) - 1ull;
    return usable & ~0xFFFull;
}

static int mtrr_acquire_slot(void)
{
    for (uint8_t idx = 0; idx < g_mtrr_var_count; ++idx) {
        uint64_t bit = 1ull << idx;
        if ((g_mtrr_usage_mask & bit) == 0) {
            g_mtrr_usage_mask |= bit;
            return idx;
        }
    }
    return -1;
}

static void mtrr_release_slot(int slot)
{
    if (slot < 0) return;
    uint64_t bit = 1ull << (uint8_t)slot;
    g_mtrr_usage_mask &= ~bit;
}

static uint8_t arch_mtrr_type_from_memtype(arch_paging_memtype_t type)
{
    switch (type) {
        case ARCH_PAGING_MT_WC: return 0x01;
        case ARCH_PAGING_MT_UC: return 0x00;
        default: return 0xFF;
    }
}

bool arch_mtrr_init(void)
{
    if (g_mtrr_initialized) {
        return g_mtrr_available;
    }

    size_t eax = 0, ebx = 0, ecx = 0, edx = 0;
    arch_cpuid(0x00000001u, &eax, &ebx, &ecx, &edx);
    bool has_mtrr = (edx & (1u << 12)) != 0;
    if (!has_mtrr) {
        g_mtrr_initialized = true;
        g_mtrr_available = false;
        return false;
    }

    uint64_t cap = rdmsr(IA32_MTRR_CAP_MSR);
    g_mtrr_var_count = (uint8_t)(cap & 0xFFu);
    g_mtrr_available = g_mtrr_var_count != 0;

    arch_cpuid(0x80000000u, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000008u) {
        arch_cpuid(0x80000008u, &eax, &ebx, &ecx, &edx);
        g_mtrr_phys_bits = (uint8_t)(eax & 0xFFu);
        if (g_mtrr_phys_bits < 36) g_mtrr_phys_bits = 36;
    } else {
        g_mtrr_phys_bits = 36;
    }

    g_mtrr_usage_mask = 0;
    g_mtrr_initialized = true;
    return g_mtrr_available;
}

static void mtrr_program_slot(int slot, uintptr_t base, uint64_t size, uint8_t type)
{
    uint64_t phys_mask_bits = mtrr_phys_mask_bits();
    uint64_t base_val = ((uint64_t)base & phys_mask_bits) | type;
    uint64_t mask_val = ((~(size - 1ull) & phys_mask_bits) | 0x800ull);
    wrmsr(IA32_MTRR_PHYSBASE(slot), base_val);
    wrmsr(IA32_MTRR_PHYSMASK(slot), mask_val);
}

static uint64_t largest_power_of_two_aligned(uint64_t base, uint64_t length)
{
    unsigned leading = (unsigned)__builtin_clzll(length);
    unsigned highest_bit = (unsigned)(63 - leading);
    uint64_t candidate = 1ull << highest_bit;
    while (candidate > 0 && (base & (candidate - 1ull)) != 0) {
        candidate >>= 1;
    }
    return candidate;
}

bool arch_mtrr_set_range(uintptr_t phys_start, size_t length, arch_paging_memtype_t type)
{
    if (length == 0) return true;
    if (!arch_mtrr_init()) return false;
    if (!g_mtrr_available) return false;

    uint8_t mtrr_type = arch_mtrr_type_from_memtype(type);
    if (mtrr_type == 0xFF) return false;

    uintptr_t start = phys_start & ~0xFFFull;
    uintptr_t end   = (phys_start + length + 0xFFFull) & ~0xFFFull;
    uint64_t remaining = (uint64_t)(end - start);
    if (remaining == 0) return true;

    uint64_t phys_limit_mask = mtrr_phys_mask_bits();
    uint64_t max_address = phys_limit_mask | 0xFFFull;
    if (start > max_address) {
        return false;
    }
    if (remaining - 1ull > (max_address - start)) {
        return false;
    }

    typedef struct {
        uintptr_t base;
        uint64_t size;
    } mtrr_chunk_t;
    mtrr_chunk_t chunks[64];
    uintptr_t cursor = start;
    size_t chunk_count = 0;
    uint64_t temp_remaining = remaining;
    while (temp_remaining > 0) {
        uint64_t chunk = largest_power_of_two_aligned(cursor, temp_remaining);
        if (chunk < 0x1000ull) chunk = 0x1000ull;
        if (chunk > temp_remaining) chunk = temp_remaining;
        if (chunk_count >= sizeof(chunks) / sizeof(chunks[0])) {
            return false;
        }
        chunks[chunk_count].base = cursor;
        chunks[chunk_count].size = chunk;
        chunk_count++;
        cursor += chunk;
        temp_remaining -= chunk;
    }

    uint8_t used_slots = bitcount64(g_mtrr_usage_mask);
    uint8_t available_slots = (g_mtrr_var_count > used_slots) ? (g_mtrr_var_count - used_slots) : 0;
    if (chunk_count > available_slots) {
        return false;
    }

    int slots[64];
    for (size_t i = 0; i < chunk_count; ++i) {
        slots[i] = mtrr_acquire_slot();
        if (slots[i] < 0) {
            for (size_t j = 0; j < i; ++j) mtrr_release_slot(slots[j]);
            return false;
        }
    }

    uint64_t cr0 = read_cr0();
    uint64_t cr0_cache_off = cr0 | (1ull << 30) | (1ull << 29);
    write_cr0(cr0_cache_off);
    wbinvd();

    uint64_t def_type = rdmsr(IA32_MTRR_DEF_TYPE_MSR);
    bool was_enabled = (def_type & IA32_MTRR_DEF_ENABLE) != 0;
    uint64_t disabled_type = def_type & ~(IA32_MTRR_DEF_ENABLE | IA32_MTRR_DEF_FIXED);
    wrmsr(IA32_MTRR_DEF_TYPE_MSR, disabled_type);

    for (size_t i = 0; i < chunk_count; ++i) {
        mtrr_program_slot(slots[i], chunks[i].base, chunks[i].size, mtrr_type);
    }

    wbinvd();
    if (was_enabled) {
        wrmsr(IA32_MTRR_DEF_TYPE_MSR, def_type);
    } else {
        wrmsr(IA32_MTRR_DEF_TYPE_MSR, disabled_type);
    }
    write_cr0(cr0);
    return true;
}

void arch_tlb_flush_one(void* addr) {
    asm volatile ("invlpg (%0)" :: "r"(addr) : "memory");
}

void arch_tlb_flush_all(void) {
    uintptr_t cr3; asm volatile ("mov %%cr3, %0" : "=r"(cr3));
    asm volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

static inline PTE* __pte_from_virt(uintptr_t vaddr) {
    uint32_t dir = (vaddr >> 22) & 0x3FFu;
    uint32_t tbl = (vaddr >> 12) & 0x3FFu;
    return &page_tables[dir][tbl];
}

arch_paging_memtype_t arch_paging_get_memtype(uintptr_t virt_addr) {
    PTE* pte = __pte_from_virt(virt_addr);
    if (!pte->present) return ARCH_PAGING_MT_UC;
    uint8_t pat = pte->pat ? 1 : 0;
    uint8_t pcd = pte->cache_disabled ? 1 : 0;
    uint8_t pwt = pte->write_through ? 1 : 0;
    if (!pat && !pcd && !pwt) return ARCH_PAGING_MT_WB;
    if (!pat && !pcd &&  pwt) return ARCH_PAGING_MT_WT;
    if (!pat &&  pcd && !pwt) return ARCH_PAGING_MT_UC_MINUS;
    if (!pat &&  pcd &&  pwt) return ARCH_PAGING_MT_UC;
    if ( pat && !pcd && !pwt) return ARCH_PAGING_MT_WC;
    if ( pat && !pcd &&  pwt) return ARCH_PAGING_MT_WP;
    return ARCH_PAGING_MT_UC;
}

static void __apply_type_to_pte(PTE* pte, arch_paging_memtype_t type) {
    pte->write_through = 0;
    pte->cache_disabled = 0;
    pte->pat = 0;
    switch (type) {
        case ARCH_PAGING_MT_WB: break;
        case ARCH_PAGING_MT_WT: pte->write_through = 1; break;
        case ARCH_PAGING_MT_UC: pte->write_through = 1; pte->cache_disabled = 1; break;
        case ARCH_PAGING_MT_UC_MINUS: pte->cache_disabled = 1; break;
        case ARCH_PAGING_MT_WC: pte->pat = 1; break;
        case ARCH_PAGING_MT_WP: pte->pat = 1; pte->write_through = 1; break; /* heuristic */
    }
}

bool arch_paging_set_memtype(uintptr_t phys_start, size_t length, arch_paging_memtype_t type) {
    if (length == 0) return true;
    uintptr_t page_size = 4096u;
    uintptr_t start = phys_start & ~(page_size - 1);
    uintptr_t end = (phys_start + length + page_size - 1) & ~(page_size - 1);
    size_t count = (end - start) / page_size;
    size_t changed = 0;
    for (size_t i = 0; i < count; ++i) {
        uintptr_t cur = start + i * page_size;
        PTE* pte = __pte_from_virt(cur);
        if (!pte->present) continue;
        __apply_type_to_pte(pte, type);
        changed++;
        arch_tlb_flush_one((void*)cur);
    }
    return changed == count;
}

bool arch_paging_map_with_type(uintptr_t phys_start, uintptr_t virt_start, size_t length,
                               uint64_t base_flags, arch_paging_memtype_t type) {
    (void)base_flags;
    if (length == 0) return true;
    uintptr_t page_size = 4096u;
    uintptr_t phys = phys_start & ~(page_size - 1);
    uintptr_t virt = virt_start & ~(page_size - 1);
    uintptr_t end  = (phys_start + length + page_size - 1) & ~(page_size - 1);
    size_t count = (end - phys) / page_size;
    for (size_t i = 0; i < count; ++i) {
        uintptr_t p = phys + i * page_size;
        uintptr_t v = virt + i * page_size;
        PTE* pte = __pte_from_virt(v);
        if (!pte->present || pte->frame != (p >> 12)) {
            return false; /* Mapping mismatch (full implementation would create it) */
        }
        __apply_type_to_pte(pte, type);
        arch_tlb_flush_one((void*)v);
    }
    return true;
}
