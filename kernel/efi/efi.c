#include <efi/efi.h>
#include <debug/debug.h>
#include <machine/machine.h>

extern uint32_t efi_memory_map_key;

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
    
    LOG("EFI subsystem initialization complete");
}