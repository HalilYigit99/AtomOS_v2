#include <stdint.h>
#include <stddef.h>
#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <arch.h>
#include <acpi/acpi.h>
#include <driver/DriverBase.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>
#include <driver/apic/apic.h>
#include <memory/pmm.h>
#include <time/timer.h>
#include <stream/OutputStream.h>

extern DriverBase pic8259_driver;
extern DriverBase ps2kbd_driver;
extern DriverBase ps2mouse_driver;
extern DriverBase pit_driver;
extern DriverBase apic_driver;
extern DriverBase ahci_driver;
extern DriverBase ata_driver;

extern uint32_t mb2_signature;
extern uint32_t mb2_tagptr;

extern DebugStream genericDebugStream;
extern DebugStream dbgGFXTerm;
extern DebugStream uartDebugStream;

extern OutputStream dbgGFXTermStream;
extern OutputStream genericOutputStream;
extern OutputStream uartOutputStream;

extern void multiboot2_parse();
extern void pmm_init();

extern void efi_init();
extern void bios_init();

extern void heap_init();
extern void gfx_init();
extern void acpi_sci_init();
extern void efi_exit_boot_services();

extern bool apic_supported();

extern void i386_processor_exceptions_init();
extern void i386_tss_install(void);
extern void print_memory_regions();

extern void gfx_draw_task();

void __boot_kernel_start(void)
{
    debugStream->Open();

    i386_processor_exceptions_init();
    
    LOG("Booting AtomOS Kernel");

    LOG("Multiboot2 Signature: 0x%08X", mb2_signature);
    LOG("Multiboot2 Tag Pointer: 0x%08X", mb2_tagptr);

    multiboot2_parse();

    heap_init(); // Initialize local heap

    if (mb2_is_efi_boot) {
        efi_init();
    }else {
        bios_init();
    }

    pmm_init(); // Initialize physical memory manager

    /* ACPI tablolarını multiboot üzerinden başlat */
    acpi_init();

    if (mb2_is_efi_boot)
    {
        LOG("Exiting boot services");
        efi_exit_boot_services();
    }

    void* table = pmm_alloc(1); // Kernel için ilk sayfa tablosu

    if (table) LOG("Initial page table allocated at %p", table);
    else ERROR("Failed to allocate initial page table");

    print_memory_regions();

    // APIC varsa onu kullan, yoksa PIC'e düş
    if (apic_supported()) 
    {
        LOG("Using APIC interrupt controller");
        system_driver_register(&apic_driver);
        system_driver_enable(&apic_driver);
    } else {
        LOG("Using PIC8259 interrupt controller");
        system_driver_register(&pic8259_driver);
        system_driver_enable(&pic8259_driver);
    }

    // PIT (system tick)
    system_driver_register(&pit_driver);
    system_driver_enable(&pit_driver);

    gfx_init();

    if (pit_timer)
    {
        pit_timer->setFrequency(10); // 10 Hz
        pit_timer->add_callback(gfx_draw_task); // Add the gfx redraw task to the PIT timer callbacks
    }

    currentOutputStream = &genericOutputStream;

    gos_addStream(&uartOutputStream);
    gos_addStream(&dbgGFXTermStream);

    currentOutputStream->Open();

    debugStream = &genericDebugStream;

    gds_addStream(&dbgGFXTerm);
    gds_addStream(&uartDebugStream);

    debugStream->Open();

    // Enable HID drivers
    LOG("Loading HID drivers...");

    system_driver_register(&ps2kbd_driver);
    system_driver_register(&ps2mouse_driver);

    system_driver_enable(&ps2kbd_driver);
    system_driver_enable(&ps2mouse_driver);
    
    // Storage drivers (AHCI first, then legacy ATA/PATA)
    LOG("Loading storage drivers...");
    system_driver_register(&ahci_driver);
    system_driver_enable(&ahci_driver);

    system_driver_register(&ata_driver);
    system_driver_enable(&ata_driver);

}
