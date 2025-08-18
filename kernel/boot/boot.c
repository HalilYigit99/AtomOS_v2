#include <stdint.h>
#include <stddef.h>
#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <arch.h>
#include <acpi/acpi.h>
#include <driver/DriverBase.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>

extern DriverBase pic8259_driver;
extern DriverBase ps2kbd_driver;
extern DriverBase ps2mouse_driver;

extern uint32_t mb2_signature;
extern uint32_t mb2_tagptr;

extern void multiboot2_parse();
extern void pmm_init();

extern void efi_init();
extern void bios_init();

extern void heap_init();

void printMemoryRegions();

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

    printMemoryRegions();

    heap_init();

    pmm_init();

    /* ACPI tablolarını multiboot üzerinden başlat */
    acpi_init();

    // TODO: APIC or PIC selection and initialization
    // INFO: APIC is not implemented yet, so we will use PIC for now.
    system_driver_register(&pic8259_driver);
    system_driver_enable(&pic8259_driver);

    // Enable HID drivers
    LOG("Loading HID drivers...");

    system_driver_register(&ps2kbd_driver);
    system_driver_register(&ps2mouse_driver);

    system_driver_enable(&ps2kbd_driver);
    system_driver_enable(&ps2mouse_driver);

}

void printMemoryRegions() {
    LOG("Printing memory regions...");

    uint32_t memoryRegionCount = 0;
    struct multiboot_mmap_entry* entries = multiboot2_get_memory_map(&memoryRegionCount);

    if (memoryRegionCount == 0) {
        ERROR("No memory regions detected!");
        return;
    }

    LOG("Detected %u memory regions", memoryRegionCount);
    
    for (uint32_t i = 0; i < memoryRegionCount; i++) {
        struct multiboot_mmap_entry* entry = &entries[i];
        LOG("Memory Region %u: Type: %s, Base: 0x%08X, Length: %zu bytes ( %zu kb | %zu mb )", i, multiboot2_memory_type_to_string(entry->type), (size_t)entry->addr, (size_t)entry->len, (size_t)entry->len / 1024, (size_t)entry->len / (1024 * 1024));
    }
}
