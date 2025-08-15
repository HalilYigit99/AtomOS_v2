#include <efi/efi.h>
#include <debug/debug.h>

EFI_SYSTEM_TABLE* efi_system_table = NULL;
EFI_HANDLE efi_image_handle = NULL;

void efi_init(void) {

    if (!efi_image_handle || !efi_system_table) {
        // If EFI image handle or system table is not set, we cannot proceed
        ERROR("EFI initialization failed: image handle or system table is NULL");
        LOG("EFI Image Handle: %p, EFI System Table: %p", efi_image_handle, efi_system_table);
        return;
    }

    // Initialize the EFI system table and image handle

    

    LOG("EFI Image Handle: %p\n", efi_image_handle);
    LOG("EFI System Table: %p\n", efi_system_table);

}

