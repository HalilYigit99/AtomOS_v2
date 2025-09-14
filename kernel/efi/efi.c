#include <efi/efi.h>
#include <debug/debug.h>
#include <machine/machine.h>
#include <memory/pmm.h>
#include <list.h>

extern UINTN bs_map_key;

extern List *memory_regions;

EFI_SYSTEM_TABLE* efi_system_table = NULL;
EFI_HANDLE efi_image_handle = NULL;

/* UEFI Global Variable GUID */
static const EFI_GUID EFI_GLOBAL_VARIABLE_GUID = EFI_GLOBAL_VARIABLE;

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

    // After this point, boot services are no longer available
    // After that set 'usable' EFI CODE & EFI DATA sections.

    for (ListNode* node = memory_regions->head; node != NULL; node = node->next) {
        MemoryRegion* region = (MemoryRegion*)node->data;
        if (region->type == MemoryRegionType_EFI_BS_CODE || region->type == MemoryRegionType_EFI_BS_DATA) {
            region->type = MemoryRegionType_USABLE;
        }
    }
    
    pmm_maintain(); // Rebuild free memory list after changes

}

void efi_reset_system(EFI_RESET_TYPE reset_type) {
    if (!efi_system_table || !efi_system_table->runtime_services) {
        ERROR("Cannot reset system: system table or runtime services is NULL");
        return;
    }

    efi_system_table->runtime_services->reset_system(
        reset_type, EFI_SUCCESS, 0, NULL);
    
    // Should never reach here
    ERROR("System reset failed or did not occur");
}

void efi_reset_to_firmware() {
    if (!efi_system_table || !efi_system_table->runtime_services) {
        ERROR("Cannot reset to firmware: runtime services unavailable");
        return;
    }

    /* Prepare L"OsIndications" name */
    static const uint16_t OsIndicationsVar[] = {
        'O','s','I','n','d','i','c','a','t','i','o','n','s', 0
    };

    /* Read existing OsIndications (if present) */
    uint64_t indications = 0;
    UINTN data_size = sizeof(indications);
    uint32_t attrs = 0;
    EFI_STATUS st = efi_system_table->runtime_services->get_variable(
        (uint16_t*)OsIndicationsVar,
        (EFI_GUID*)&EFI_GLOBAL_VARIABLE_GUID,
        &attrs,
        &data_size,
        &indications
    );

    if (st == EFI_NOT_FOUND) {
        /* Not present: we'll create it with default attributes */
        attrs = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
        indications = 0;
    } else if (IS_EFI_ERROR(st) && st != EFI_BUFFER_TOO_SMALL) {
        WARN("GetVariable(OsIndications) failed: %lu; proceeding to set anyway", st);
        attrs = EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;
        indications = 0;
    }

    /* Set the Boot to Firmware UI bit */
    indications |= EFI_OS_INDICATIONS_BOOT_TO_FW_UI;

    st = efi_system_table->runtime_services->set_variable(
        (uint16_t*)OsIndicationsVar,
        (EFI_GUID*)&EFI_GLOBAL_VARIABLE_GUID,
        attrs,
        sizeof(indications),
        &indications
    );

    if (IS_EFI_ERROR(st)) {
        ERROR("SetVariable(OsIndications) failed: %lu; falling back to cold reset", st);
    } else {
        LOG("OsIndications updated; resetting to firmware UI");
    }

    /* Per spec, a cold reset is sufficient; firmware should honor OsIndications */
    efi_reset_system(EfiResetCold);
}
