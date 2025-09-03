#include <efi/efi.h>
#include <debug/debug.h>
#include <machine/machine.h>

extern UINTN bs_map_key;

EFI_SYSTEM_TABLE* efi_system_table = NULL;
EFI_HANDLE efi_image_handle = NULL;

void efi_init(void) {
    LOG("Initializing EFI subsystem");

    LOG("EFI Image Handle: %p", efi_image_handle);
    LOG("EFI System Table: %p", efi_system_table);

    if (!efi_image_handle || !efi_system_table) {
        ERROR("EFI initialization failed: image handle or system table is NULL");
        return;
    }
    
    // EFI Console output test
    if (efi_system_table->con_out) {
        LOG("EFI Console Output available at: %p", efi_system_table->con_out);
    }
    
    // EFI Runtime Services
    if (efi_system_table->runtime_services) {
        LOG("EFI Runtime Services available at: %p", efi_system_table->runtime_services);
    }

    // EFI Boot Services
    if (efi_system_table->boot_services) {
        LOG("EFI Boot Services available at: %p", efi_system_table->boot_services);
    }
    
    LOG("EFI subsystem initialization complete");
}

void efi_exit_boot_services() {
    if (!efi_system_table || !efi_system_table->boot_services) {
        ERROR("Cannot exit EFI boot services: system table or boot services is NULL");
        return;
    }

    if (bs_map_key == 0) {
        ERROR("Cannot exit EFI boot services: map key is zero");
        return;
    }

    EFI_STATUS status = efi_system_table->boot_services->exit_boot_services(
        efi_image_handle, bs_map_key);
    
    if (status != EFI_SUCCESS) {
        ERROR("Failed to exit EFI boot services: status code %lu", status);
    } else {
        LOG("Successfully exited EFI boot services");
    }
}
