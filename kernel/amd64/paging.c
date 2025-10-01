// AMD64 identity paging (now 4KiB granularity) with attribute hooks
#include <arch.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Page table entry flags
#define PTE_P   (1ull << 0)  // Present
#define PTE_RW  (1ull << 1)  // Read/Write
#define PTE_US  (1ull << 2)  // User/Supervisor
#define PTE_PWT (1ull << 3)  // Page-level Write-Through
#define PTE_PCD (1ull << 4)  // Page-level Cache Disable
#define PTE_A   (1ull << 5)  // Accessed
#define PTE_D   (1ull << 6)  // Dirty (for 4 KiB/2 MiB entries)
#define PTE_PS  (1ull << 7)  // Page Size (in PD: 2 MiB)
#define PTE_G   (1ull << 8)  // Global

// Statically allocated and 4KiB-aligned top-level tables
static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd0[512]  __attribute__((aligned(4096))); // covers 0..1GiB
static uint64_t pd1[512]  __attribute__((aligned(4096))); // covers 1..2GiB
static uint64_t pd2[512]  __attribute__((aligned(4096))); // covers 2..3GiB
static uint64_t pd3[512]  __attribute__((aligned(4096))); // covers 3..4GiB

// For full 4KiB mapping we need 512 page tables per 1GiB (512 * 2MiB / 4KiB = 512).
// We'll allocate them statically: 4 * 512 tables = 2048 PTs.
// Each PT has 512 entries (4KiB * 512 = 2MiB).
// Naming convention: pt_g<GB index>_<PT index within that GB>
static uint64_t pt_g0[512][512] __attribute__((aligned(4096))); // 0..1GiB
static uint64_t pt_g1[512][512] __attribute__((aligned(4096))); // 1..2GiB
static uint64_t pt_g2[512][512] __attribute__((aligned(4096))); // 2..3GiB
static uint64_t pt_g3[512][512] __attribute__((aligned(4096))); // 3..4GiB

static inline void write_cr3(uint64_t phys)
{
	__asm__ __volatile__("mov %0, %%cr3" :: "r"(phys) : "memory");
}

/* === Architecture paging attribute extension (amd64, 4KiB) ================= */

static bool g_pat_initialized = false;

bool arch_paging_pat_init(void) {
	if (g_pat_initialized) return true;
	/* TODO: Detect CPUID.PAT and write IA32_PAT MSR. For now mark initialized. */
	g_pat_initialized = true;
	return true;
}

void arch_tlb_flush_one(void* addr) {
	asm volatile ("invlpg (%0)" :: "r"(addr) : "memory");
}

void arch_tlb_flush_all(void) {
	uint64_t cr3; asm volatile ("mov %%cr3, %0" : "=r"(cr3));
	asm volatile ("mov %0, %%cr3" :: "r"(cr3) : "memory");
}

static inline uint64_t* __get_pml4e(uintptr_t va) { (void)va; return &pml4[0]; }
static inline uint64_t* __get_pdpte(uintptr_t va) { return &pdpt[(va >> 30) & 0x1FFull]; }
static inline uint64_t* __get_pde(uintptr_t va) {
	uint64_t gb = (va >> 30) & 0x3ull; // 0..3
	uint64_t pd_index = (va >> 21) & 0x1FFull;
	uint64_t* pd = (gb==0?pd0:gb==1?pd1:gb==2?pd2:pd3);
	return &pd[pd_index];
}
static inline uint64_t* __get_pte(uintptr_t va) {
	uint64_t gb = (va >> 30) & 0x3ull;
	uint64_t pt_index = (va >> 21) & 0x1FFull; // which PT inside that GB
	uint64_t entry_index = (va >> 12) & 0x1FFull; // which PTE inside PT
	uint64_t (*pt_block)[512] = (gb==0?pt_g0:gb==1?pt_g1:gb==2?pt_g2:pt_g3);
	return &pt_block[pt_index][entry_index];
}

static void __apply_type_to_pte(uint64_t* pte, arch_paging_memtype_t type) {
	// Clear PWT, PCD, PAT bit (bit 7 in 4KiB PTE high part -> actually bit 7 overall is PS for PDE; here normal PTE so bit7 is PAT)
	*pte &= ~(PTE_PWT | PTE_PCD | (1ull << 7));
	switch (type) {
		case ARCH_PAGING_MT_WB: /* nothing */ break;
		case ARCH_PAGING_MT_WT: *pte |= PTE_PWT; break;
		case ARCH_PAGING_MT_UC: *pte |= (PTE_PWT | PTE_PCD); break;
		case ARCH_PAGING_MT_UC_MINUS: *pte |= PTE_PCD; break;
		case ARCH_PAGING_MT_WC: *pte |= (1ull << 7); break; // PAT index with PAT=1, PWT=0, PCD=0
		case ARCH_PAGING_MT_WP: *pte |= (1ull << 7) | PTE_PWT; break; // placeholder mapping
	}
}

arch_paging_memtype_t arch_paging_get_memtype(uintptr_t virt_addr) {
	uint64_t* pte = __get_pte(virt_addr);
	if (!(*pte & PTE_P)) return ARCH_PAGING_MT_UC;
	bool pat = (*pte & (1ull<<7)) != 0;
	bool pcd = (*pte & PTE_PCD) != 0;
	bool pwt = (*pte & PTE_PWT) != 0;
	if (!pat && !pcd && !pwt) return ARCH_PAGING_MT_WB;
	if (!pat && !pcd &&  pwt) return ARCH_PAGING_MT_WT;
	if (!pat &&  pcd && !pwt) return ARCH_PAGING_MT_UC_MINUS;
	if (!pat &&  pcd &&  pwt) return ARCH_PAGING_MT_UC;
	if ( pat && !pcd && !pwt) return ARCH_PAGING_MT_WC;
	if ( pat && !pcd &&  pwt) return ARCH_PAGING_MT_WP;
	return ARCH_PAGING_MT_UC;
}

bool arch_paging_set_memtype(uintptr_t phys_start, size_t length, arch_paging_memtype_t type) {
	if (length == 0) return true;
	const uintptr_t page_size = 4096u;
	uintptr_t start = phys_start & ~(page_size - 1);
	uintptr_t end   = (phys_start + length + page_size - 1) & ~(page_size - 1);
	size_t count = (end - start) / page_size;
	size_t changed = 0;
	for (size_t i = 0; i < count; ++i) {
		uintptr_t cur = start + i * page_size;
		uint64_t* pte = __get_pte(cur); // identity virt==phys
		if (!(*pte & PTE_P)) continue; // skip unmapped
		__apply_type_to_pte(pte, type);
		arch_tlb_flush_one((void*)cur);
		changed++;
	}
	return changed == count;
}

bool arch_paging_map_with_type(uintptr_t phys_start, uintptr_t virt_start, size_t length,
							   uint64_t base_flags, arch_paging_memtype_t type) {
	if (length == 0) return true;
	const uintptr_t page_size = 4096u;
	uintptr_t phys = phys_start & ~(page_size - 1);
	uintptr_t virt = virt_start & ~(page_size - 1);
	uintptr_t end  = (phys_start + length + page_size - 1) & ~(page_size - 1);
	size_t count = (end - phys) / page_size;
	for (size_t i = 0; i < count; ++i) {
		uintptr_t p = phys + i * page_size;
		uintptr_t v = virt + i * page_size;
		uint64_t* pte = __get_pte(v);
		if (!(*pte & PTE_P)) {
			*pte = (p & 0x000FFFFFFFFFF000ULL) | PTE_P | PTE_RW | (base_flags & (PTE_US|PTE_G));
		}
		__apply_type_to_pte(pte, type);
		arch_tlb_flush_one((void*)v);
	}
	return true;
}

// Build identity 4KiB page tables for 0..4GiB
void amd64_map_identity_low_4g(void)
{
	static bool done = false;
	if (done) return;
	done = true;

    // Zero top structures
    for (int i=0;i<512;i++){ pml4[i]=0; pdpt[i]=0; pd0[i]=0; pd1[i]=0; pd2[i]=0; pd3[i]=0; }

    // Link PML4 -> PDPT
    pml4[0] = ((uint64_t)(uintptr_t)pdpt) | PTE_P | PTE_RW;

    // Link PDPT to PDs
    pdpt[0] = ((uint64_t)(uintptr_t)pd0) | PTE_P | PTE_RW;
    pdpt[1] = ((uint64_t)(uintptr_t)pd1) | PTE_P | PTE_RW;
    pdpt[2] = ((uint64_t)(uintptr_t)pd2) | PTE_P | PTE_RW;
    pdpt[3] = ((uint64_t)(uintptr_t)pd3) | PTE_P | PTE_RW;

    // Each PD entry points to a 4KiB PT (remove use of large pages)
    for (int gb=0; gb<4; ++gb) {
        uint64_t *pd = (gb==0?pd0:gb==1?pd1:gb==2?pd2:pd3);
        uint64_t (*pt_block)[512] = (gb==0?pt_g0:gb==1?pt_g1:gb==2?pt_g2:pt_g3);
        for (int pt_index=0; pt_index<512; ++pt_index) {
            pd[pt_index] = ((uint64_t)(uintptr_t)pt_block[pt_index]) | PTE_P | PTE_RW; // PS=0
            // Fill the PT
            uint64_t base_phys = ((uint64_t)gb * 1024ull * 1024ull * 1024ull) + ((uint64_t)pt_index * 2ull * 1024ull * 1024ull);
            for (int e=0; e<512; ++e) {
                uint64_t phys = base_phys + ((uint64_t)e * 4096ull);
                pt_block[pt_index][e] = phys | PTE_P | PTE_RW; // default WB
            }
        }
    }

    // Mark IOAPIC & LAPIC pages uncacheable
    uint64_t ioapic_phys = 0xFEC00000ull;
    uint64_t lapic_phys  = 0xFEE00000ull;
    arch_paging_set_memtype(ioapic_phys, 4096, ARCH_PAGING_MT_UC);
    arch_paging_set_memtype(lapic_phys,  4096, ARCH_PAGING_MT_UC);

    write_cr3((uint64_t)(uintptr_t)pml4);
}

