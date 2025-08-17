// kernel/efi/efi.c - güncellenmiş versiyon

#include <efi/efi.h>
#include <efi/efi_memory.h>
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

    // Exit boot services for get free memory
    if (efi_system_table->boot_services && efi_system_table->boot_services->exit_boot_services) {
        typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE ImageHandle, uint64_t MapKey);
        EFI_EXIT_BOOT_SERVICES exit_boot_services = (EFI_EXIT_BOOT_SERVICES)efi_system_table->boot_services->exit_boot_services;
        
        // Memory map key'i 0 ile deniyoruz (bazı sistemlerde çalışır)
        EFI_STATUS status = exit_boot_services(efi_image_handle, efi_memory_map_key);
        if (EFI_ERROR(status)) {
            WARN("ExitBootServices failed: 0x%016llX", status);
        } else {
            LOG("EFI Boot Services successfully exited");
        }
    }

    // Calculate RAM size: sadece kullanılabilir bellekleri topla
    machine_ramSizeInKB = 0;
    uint32_t entryCount = 0;
    struct multiboot_mmap_entry* memory_map = multiboot2_get_memory_map(&entryCount);

    for (size_t i = 0; i < entryCount; i++) {
        struct multiboot_mmap_entry* entry = &memory_map[i];
        machine_ramSizeInKB += (size_t)(entry->len / 1024u); // KB cinsinden ekle
    }

    machine_ramSizeInKB += 1024;
    
    LOG("EFI subsystem initialization complete");
}