#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__)
#define ARCH_AMD
#elif defined(__i386__)
#define ARCH_INTEL
#else
#error "Unsupported architecture"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {

    // Register A
    union {
        uint64_t rax;
        uint32_t eax;
        uint16_t ax;
        struct {
            uint8_t al;
            uint8_t ah;
        };
    };

    // Register B
    union {
        uint64_t rbx;
        uint32_t ebx;
        uint16_t bx;
        struct {
            uint8_t bl;
            uint8_t bh;
        };
    };

    // Register C
    union {
        uint64_t rcx;
        uint32_t ecx;
        uint16_t cx;
        struct {
            uint8_t cl;
            uint8_t ch;
        };
    };

    // Register D
    union {
        uint64_t rdx;
        uint32_t edx;
        uint16_t dx;
        struct {
            uint8_t dl;
            uint8_t dh;
        };
    };

    // Register  SI
    union {
        uint64_t rsi;
        uint32_t esi;
        uint16_t si;
        struct {
            uint8_t sil;
            uint8_t sih; // Note: sih is not commonly used
        };
    };

    // Register DI
    union {
        uint64_t rdi;
        uint32_t edi;
        uint16_t di;
        struct {
            uint8_t dil;
            uint8_t dih; // Note: dih is not commonly used
        };
    };
    
    // SP
    union {
        uint64_t rsp;
        uint32_t esp;
        uint16_t sp;
        struct {
            uint8_t spl;
            uint8_t sph; // Note: sph is not commonly used
        };
    };

    // BP
    union {
        uint64_t rbp;
        uint32_t ebp;
        uint16_t bp;
        struct {
            uint8_t bpl;
            uint8_t bph; // Note: bph is not commonly used
        };
    };

    /* Segment registers and flags for BIOS/V86/real-mode interactions */
    union {
        uint64_t rflags;
        uint32_t eflags;
        uint16_t flags;
    };

    uint16_t cs;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint16_t ss;

} arch_processor_regs_t;

void idt_set_gate(uint8_t vector, size_t offset);
void idt_reset_gate(uint8_t vector);

size_t idt_get_gate(uint8_t vector);

// Common port I/O interface (architecture-specific implementation)
extern uint8_t  inb(uint16_t port);
extern void     outb(uint16_t port, uint8_t value);
extern uint16_t inw(uint16_t port);
extern void     outw(uint16_t port, uint16_t value);
extern uint32_t inl(uint16_t port);
extern void     outl(uint16_t port, uint32_t value);
extern void     io_wait(void);

extern bool arch_isEfiBoot(void);

extern void arch_cpuid(uint32_t leaf, size_t* regA, size_t* regB, size_t* regC, size_t* regD);

extern void arch_bios_int(uint8_t int_no, arch_processor_regs_t* in, arch_processor_regs_t* out);

/* -------------------------------------------------------------------------- */
/* Paging Memory Type / Attribute Control (architecture-level interface)      */
/* -------------------------------------------------------------------------- */

typedef enum {
    ARCH_PAGING_MT_WB = 0,      /* Write-Back (default) */
    ARCH_PAGING_MT_WT,          /* Write-Through */
    ARCH_PAGING_MT_UC,          /* Uncacheable */
    ARCH_PAGING_MT_UC_MINUS,    /* UC- (Uncacheable, weakly ordered) */
    ARCH_PAGING_MT_WC,          /* Write-Combining */
    ARCH_PAGING_MT_WP           /* Write-Protected */
} arch_paging_memtype_t;

/* Initialize PAT MSR and internal structures (idempotent). */
bool arch_paging_pat_init(void);

/* Initialize IA32 MTRR interface (idempotent). */
bool arch_mtrr_init(void);

/* Configure an MTRR range to the requested memory type (best-effort). */
bool arch_mtrr_set_range(uintptr_t phys_start, size_t length, arch_paging_memtype_t type);

/* Set memory type attributes for an existing mapped physical region.
 * phys_start / length: physical address range (will be page-aligned internally)
 * type: one of arch_paging_memtype_t
 * Returns true on full success; false if any page not present or unsupported.
 */
bool arch_paging_set_memtype(uintptr_t phys_start, size_t length, arch_paging_memtype_t type);

/* Map a physical range to a virtual range with desired base flags + memory type.
 * If the mapping already exists, attributes are updated.
 */
bool arch_paging_map_with_type(uintptr_t phys_start, uintptr_t virt_start, size_t length,
                               uint64_t base_flags, arch_paging_memtype_t type);

/* Query the memory type for a given virtual address (best-effort). */
arch_paging_memtype_t arch_paging_get_memtype(uintptr_t virt_addr);

/* Flush address from TLB (single page) */
void arch_tlb_flush_one(void* addr);

/* Invalidate entire TLB by reloading CR3 (fallback for large batches) */
void arch_tlb_flush_all(void);


#ifdef __cplusplus
}
#endif
