#include <boot/multiboot2.h>
#include <debug/debug.h>
#include <stream/OutputStream.h>
#include <memory/heap.h>
#include <driver/DriverBase.h>
#include <time/timer.h>
#include <irq/IRQ.h>
#include <task/PeriodicTask.h>
#include <efi/efi.h>
#include <pci/PCI.h>
#include <memory/memory.h>
#include <gfxterm/gfxterm.h>

extern DriverBase pic8259_driver;
extern DriverBase ps2kbd_driver;
extern DriverBase ps2mouse_driver;
extern DriverBase pit_driver;
extern DriverBase apic_driver;
extern DriverBase ahci_driver;
extern DriverBase ata_driver;
extern DriverBase hpet_driver;

extern GFXTerminal* debug_terminal;

extern bool hpet_supported();
extern void heap_init();
extern void acpi_poweroff(); // ACPI power off function
extern void acpi_restart(); 
extern void efi_init();
extern void bios_init();
extern void pmm_init();
extern bool apic_supported();
extern void screen_init();
extern void gfx_init();
extern void acpi_init();
extern void gfx_draw_task();
extern void i386_processor_exceptions_init();

extern uint32_t mb2_signature;
extern uint32_t mb2_tagptr;

extern DebugStream uartDebugStream;
extern DebugStream journald_debugStream;

extern OutputStream uartOutputStream;
extern OutputStream journald_outputStream;

extern OutputStream dbgGFXTermStream;
extern DebugStream dbgGFXTerm;

extern char* journald_getBuffer();

PeriodicTask* gfxTask;

void uptime_counter_task()
{
    uptimeMs++;
    periodic_task_run_all();
}

void __boot_kernel_start(void)
{

    i386_processor_exceptions_init();

    heap_init();

    gds_addStream(&journald_debugStream);
    gds_addStream(&uartDebugStream);

    debugStream->Open();
    currentOutputStream->Open();

    multiboot2_parse();

    LOG("Booting AtomOS Kernel");
    
    if (mb2_is_efi_boot)
    {
        efi_init();
    }
    else
    {
        bios_init();
    }

    screen_init();

    pmm_init();
    LOG("Physical Memory Manager initialized");

    acpi_init();
    LOG("ACPI initialized");

    void* large_alloc = malloc(1024 * 1024 * 10); // 10 MB test
    if (large_alloc)
    {
        free(large_alloc);
        LOG("Large memory allocation test passed");
    }
    else
    {
        ERROR("Large memory allocation test failed");
        asm volatile ("cli; hlt"); // Halt the system
    }

    PCI_Init();
    LOG("PCI bus initialized");

    // APIC varsa onu kullan, yoksa PIC'e düş
    if (apic_supported())
    {
        LOG("Using APIC interrupt controller");
        system_driver_register(&apic_driver);
        system_driver_enable(&apic_driver);
    }
    else
    {
        LOG("Using PIC8259 interrupt controller");
        system_driver_register(&pic8259_driver);
        system_driver_enable(&pic8259_driver);
    }

    system_driver_register(&pit_driver);
    system_driver_enable(&pit_driver);
    irq_controller->acknowledge(0); // Acknowledge PIT IRQ

    LOG("PIT enabled");

    if (hpet_supported()) {
        LOG("HPET supported – using HPET for system tick");
        system_driver_register(&hpet_driver);
        system_driver_enable(&hpet_driver);
        hpet_timer->setFrequency(1000);
        hpet_timer->add_callback(uptime_counter_task);
    }else
    {
        LOG("HPET not available – falling back to PIT");
        // Hook uptime tick to the active hardware timer
        pit_timer->setFrequency(1000);
        pit_timer->add_callback(uptime_counter_task);
    }

    asm volatile ("sti"); // Enable interrupts

    gfx_init();

    gfxTask = periodic_task_create("gfx_draw_task", gfx_draw_task, NULL, 16);
    periodic_task_start(gfxTask);

    dbgGFXTermStream.Open();
    dbgGFXTerm.Open();

    dbgGFXTermStream.WriteString(journald_getBuffer());

    gds_addStream(&dbgGFXTerm);
    gos_addStream(&dbgGFXTermStream);

    gfxterm_visible(debug_terminal, true);

    system_driver_register(&ps2kbd_driver);
    if (system_driver_is_available(&ps2kbd_driver)) system_driver_enable(&ps2kbd_driver);

    system_driver_register(&ps2mouse_driver);
    if (system_driver_is_available(&ps2mouse_driver)) system_driver_enable(&ps2mouse_driver);

}
