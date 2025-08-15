#include <stdint.h>
#include <stddef.h>
#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <arch.h>

extern uint32_t mb2_signature;
extern uint32_t mb2_tagptr;

extern void multiboot2_parse();
extern void pmm_init();

extern void efi_init();
extern void bios_init();

void __boot_kernel_start(void)
{
    debugStream->Open();

    LOG("Booting AtomOS Kernel");

    LOG("Multiboot2 Signature: 0x%08X", mb2_signature);
    LOG("Multiboot2 Tag Pointer: 0x%08X", mb2_tagptr);

    multiboot2_parse();

    if (mb2_is_efi_boot) {
        efi_init();
    }else {
        bios_init();
    }

    pmm_init();

}
