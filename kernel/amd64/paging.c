// Minimal AMD64 identity paging to ensure MMIO like LAPIC/IOAPIC are mapped
#include <arch.h>
#include <stdint.h>
#include <stdbool.h>

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

