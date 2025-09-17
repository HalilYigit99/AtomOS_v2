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
#include <task/PeriodicTask.h>

#define MODERN_HARDWARE

extern DriverBase pic8259_driver;
extern DriverBase ps2kbd_driver;
extern DriverBase ps2mouse_driver;
extern DriverBase pit_driver;
extern DriverBase apic_driver;
extern DriverBase ahci_driver;
extern DriverBase ata_driver;
extern DriverBase hpet_driver;
extern bool hpet_supported();

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

extern void screen_init();

PeriodicTask* gfx_task = NULL;

void uptime_counter_task()
{
    uptimeMs++;
    periodic_task_run_all();
}

void __boot_kernel_start(void)
{
    debugStream->Open();

    i386_processor_exceptions_init();

    LOG("Booting AtomOS Kernel");

    LOG("Multiboot2 Signature: 0x%08X", mb2_signature);
    LOG("Multiboot2 Tag Pointer: 0x%08X", mb2_tagptr);

    multiboot2_parse();

    heap_init(); // Initialize local heap

    if (mb2_is_efi_boot)
    {
        efi_init();
    }
    else
    {
        bios_init();
    }

    pmm_init(); // Initialize physical memory manager

    screen_init();

    /* ACPI tablolarını multiboot üzerinden başlat */
    acpi_init();

    // Don't exit boot services yet; some modules may need it ( ChangeVideoMode function, AHCI driver, etc.)
    // if (mb2_is_efi_boot)
    // {
    //     LOG("Exiting boot services");
    //     efi_exit_boot_services();
    // }

    void *table = pmm_alloc(1); // Kernel için ilk sayfa tablosu

    if (table)
        LOG("Initial page table allocated at %p", table);
    else
        ERROR("Failed to allocate initial page table");

    print_memory_regions();

    // APIC varsa onu kullan, yoksa PIC'e düş
#ifdef MODERN_HARDWARE
    if (apic_supported())
    {
        LOG("Using APIC interrupt controller");
        system_driver_register(&apic_driver);
        system_driver_enable(&apic_driver);
    }
    else
#endif
    {
        LOG("Using PIC8259 interrupt controller");
        system_driver_register(&pic8259_driver);
        system_driver_enable(&pic8259_driver);
    }

    // System tick: prefer HPET if available, else PIT
#ifdef MODERN_HARDWARE
    if (hpet_supported()) {
        LOG("HPET supported – using HPET for system tick");
        system_driver_register(&hpet_driver);
        system_driver_enable(&hpet_driver);
    } else 
#endif
    {
        LOG("HPET not available – falling back to PIT");
        system_driver_register(&pit_driver);
        system_driver_enable(&pit_driver);
    }

    irq_controller->acknowledge(0); // Acknowledge IRQ0 (PIT)

    asm volatile ("sti"); // Enable interrupts

    // Hook uptime tick to the active hardware timer
    if (hpet_supported() && hpet_timer) {
        hpet_timer->setFrequency(1000);
        hpet_timer->add_callback(uptime_counter_task);
    } else if (pit_timer) {
        pit_timer->setFrequency(1000);
        pit_timer->add_callback(uptime_counter_task);
    }

    gfx_init();

    gfx_task = periodic_task_create("GFX Task", gfx_draw_task, NULL, 16); // Yaklaşık 60 FPS
    if (!gfx_task)
    {
        ERROR("Failed to create GFX task");
    }
    periodic_task_start(gfx_task);

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

    mouse_enabled = true;

    // Storage drivers (AHCI first, then legacy ATA/PATA)
    LOG("Loading storage drivers...");
    system_driver_register(&ahci_driver);
    system_driver_enable(&ahci_driver);

    system_driver_register(&ata_driver);
    system_driver_enable(&ata_driver);
}
