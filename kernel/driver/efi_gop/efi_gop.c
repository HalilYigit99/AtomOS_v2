#include <driver/DriverBase.h>
#include <boot/multiboot2.h>
#include <efi/efi.h>
#include <debug/debug.h>

extern DriverBase efi_gop_driver;


static void __attribute__((unused)) efi_gop_init()
{

    ASSERT(mb2_is_efi_boot, "EFI GOP driver initialized in non-EFI mode");
    ASSERT(efi_system_table, "EFI System Table is NULL");
    ASSERT(efi_system_table->boot_services, "EFI Boot Services is NULL");



}





DriverBase efi_gop_driver = {
    .name = "EFI Graphics Output Protocol Driver",
    .enabled = false,
    .version = 1,
    .context = NULL,
    .init = NULL,    // Initialization function can be assigned here
    .enable = NULL,  // Enable function can be assigned here
    .disable = NULL, // Disable function can be assigned here
    .type = DRIVER_TYPE_DISPLAY
};
