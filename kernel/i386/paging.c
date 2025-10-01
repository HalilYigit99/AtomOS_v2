#include <arch.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>


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

bool arch_paging_pat_init(void) {
    if (g_pat_initialized) return true;
    g_pat_initialized = true; /* Placeholder: real PAT MSR programming omitted */
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
