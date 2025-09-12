#include <arch.h>
#include <boot/multiboot2.h>
#include <debug/debug.h>
#include <memory/memory.h>

#ifdef ARCH_AMD
extern void amd_bios_int(uint8_t int_no, arch_processor_regs_t* in, arch_processor_regs_t* out);
#else
extern void i386_bios_int(uint8_t int_no, arch_processor_regs_t* in, arch_processor_regs_t* out);
#endif

bool arch_isEfiBoot(void)
{
    return mb2_is_efi_boot;
}

void arch_bios_int(uint8_t int_no, arch_processor_regs_t* in, arch_processor_regs_t* out)
{
    if (arch_isEfiBoot())
    {
        LOG("EFI boot detected, BIOS interrupts are not supported.\n");
    }else {
        LOG("BIOS interrupt 0x%02X called with AX=0x%04X\n", int_no, in->ax);
        #ifdef ARCH_AMD
        amd_bios_int(int_no, in, out);
        #else
        i386_bios_int(int_no, in, out);
        #endif
    }
}
