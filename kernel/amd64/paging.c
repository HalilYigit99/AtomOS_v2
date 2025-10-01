// Minimal AMD64 identity paging to ensure MMIO like LAPIC/IOAPIC are mapped
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

// Statically allocated and 4KiB-aligned page tables
static uint64_t pml4[512] __attribute__((aligned(4096)));
static uint64_t pdpt[512] __attribute__((aligned(4096)));
static uint64_t pd0[512]  __attribute__((aligned(4096)));
static uint64_t pd1[512]  __attribute__((aligned(4096)));
static uint64_t pd2[512]  __attribute__((aligned(4096)));
static uint64_t pd3[512]  __attribute__((aligned(4096)));

static inline void write_cr3(uint64_t phys)
{
	__asm__ __volatile__("mov %0, %%cr3" :: "r"(phys) : "memory");
}

/* === Architecture paging attribute extension (amd64) ======================= */
/* Skeleton implementation: fine-grained 4KiB mapping upgrade and PAT use is  */
/* left as future enhancement.                                                */

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

arch_paging_memtype_t arch_paging_get_memtype(uintptr_t virt_addr) {
	(void)virt_addr;
	/* Without 4KiB page tables constructed, only coarse inference: return WB. */
	return ARCH_PAGING_MT_WB;
}

bool arch_paging_set_memtype(uintptr_t phys_start, size_t length, arch_paging_memtype_t type) {
	/* Current identity map uses 2MiB pages; to change a sub-range we'd need to split. */
	(void)phys_start; (void)length; (void)type;
	return false; /* Indicate unsupported for now. */
}

bool arch_paging_map_with_type(uintptr_t phys_start, uintptr_t virt_start, size_t length,
							   uint64_t base_flags, arch_paging_memtype_t type) {
	(void)phys_start; (void)virt_start; (void)length; (void)base_flags; (void)type;
	return false; /* Placeholder until full 4KiB mapping logic added. */
}

// Map identity 0..4GiB using 2MiB pages; mark IOAPIC/LAPIC pages as uncacheable
void amd64_map_identity_low_4g(void)
{
	static bool done = false;
	if (done) return;
	done = true;

	// Zero tables
	for (int i = 0; i < 512; ++i) {
		pml4[i] = 0; pdpt[i] = 0; pd0[i] = 0; pd1[i] = 0; pd2[i] = 0; pd3[i] = 0;
	}

	// Link PML4 -> PDPT
	pml4[0] = ((uint64_t)(uintptr_t)pdpt) | PTE_P | PTE_RW;

	// Link PDPT entries to 4 PDs, each maps 1GiB with 2MiB pages
	pdpt[0] = ((uint64_t)(uintptr_t)pd0) | PTE_P | PTE_RW;
	pdpt[1] = ((uint64_t)(uintptr_t)pd1) | PTE_P | PTE_RW;
	pdpt[2] = ((uint64_t)(uintptr_t)pd2) | PTE_P | PTE_RW;
	pdpt[3] = ((uint64_t)(uintptr_t)pd3) | PTE_P | PTE_RW;

	// Fill PDs with 2MiB identity mappings
	const uint64_t twoMiB = 2ull * 1024 * 1024;
	for (int i = 0; i < 512; ++i) {
		uint64_t base0 = ((uint64_t)i) * twoMiB;                //   0 .. 1GiB-2MiB
		uint64_t base1 = (((uint64_t)i) + 512ull) * twoMiB;     //   1 .. 2GiB-2MiB
		uint64_t base2 = (((uint64_t)i) + 1024ull) * twoMiB;    //   2 .. 3GiB-2MiB
		uint64_t base3 = (((uint64_t)i) + 1536ull) * twoMiB;    //   3 .. 4GiB-2MiB

		pd0[i] = base0 | PTE_P | PTE_RW | PTE_PS;
		pd1[i] = base1 | PTE_P | PTE_RW | PTE_PS;
		pd2[i] = base2 | PTE_P | PTE_RW | PTE_PS;
		pd3[i] = base3 | PTE_P | PTE_RW | PTE_PS;
	}

	// Mark IOAPIC (0xFEC0_0000) and LAPIC (0xFEE0_0000) 2MiB pages as uncacheable
	uint64_t ioapic_phys = 0xFEC00000ull;
	uint64_t lapic_phys  = 0xFEE00000ull;
	// Compute PDPT index (each 1GiB) and PD index (each 2MiB) for both
	uint64_t ioapic_gb = ioapic_phys >> 30; // 0..3
	uint64_t ioapic_pd_index = (ioapic_phys >> 21) & 0x1FFull; // 0..511
	uint64_t lapic_gb = lapic_phys >> 30;
	uint64_t lapic_pd_index = (lapic_phys >> 21) & 0x1FFull;

	uint64_t *pd_ioapic = (ioapic_gb == 0 ? pd0 : ioapic_gb == 1 ? pd1 : ioapic_gb == 2 ? pd2 : pd3);
	uint64_t *pd_lapic  = (lapic_gb  == 0 ? pd0 : lapic_gb  == 1 ? pd1 : lapic_gb  == 2 ? pd2 : pd3);

	// Set PCD|PWT to disable caching on these MMIO pages
	pd_ioapic[ioapic_pd_index] |= (PTE_PCD | PTE_PWT);
	pd_lapic [lapic_pd_index ] |= (PTE_PCD | PTE_PWT);

	// Load CR3 with our PML4 physical address (assumes identity mapping of kernel image)
	write_cr3((uint64_t)(uintptr_t)pml4);
}

