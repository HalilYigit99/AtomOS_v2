#include <boot/multiboot2.h>
#include <stream/OutputStream.h>
#include <efi/efi.h>
#include <arch.h>
#include <acpi/acpi.h>
#include <irq/IRQ.h>
#include <debug/debug.h>
#include <driver/DriverBase.h>

extern DebugStream uartDebugStream;

extern void heap_init(void);
extern void efi_init(void);
extern void bios_init(void);
extern void acpi_init(void);
extern void pmm_init(void);

extern void acpi_poweroff(); // ACPI power off function

void __boot_kernel_start()
{

    multiboot2_parse();

    heap_init(); // Initialize local heap

    gos_addStream(&nullOutputStream); // Add null output stream to avoid null pointer dereference
    gds_addStream(&nullDebugStream); // Add UART debug stream

    if (mb2_is_efi_boot)
    {
        efi_init();
    }else {
        bios_init();
        gds_addStream(&uartDebugStream); // Add UART debug stream
    }

    acpi_init();

    pmm_init(); // Initialize physical memory manager

    acpi_poweroff(); // ACPI power off function (for testing purposes; remove in production)

}
