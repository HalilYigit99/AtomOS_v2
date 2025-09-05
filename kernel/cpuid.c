#include <arch.h>

void arch_cpuid(uint32_t leaf, size_t* regA, size_t* regB, size_t* regC, size_t* regD)
{
    __asm__ __volatile__ (
        "cpuid"
        : "=a" (*regA), "=b" (*regB), "=c" (*regC), "=d" (*regD)
        : "a" (leaf)
    );
}
