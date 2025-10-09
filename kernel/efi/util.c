#include <efi/efi.h>
#include <efi/util.h>
#include <debug/debug.h>

void* efi_gop_get_framebuffer()
{
    ASSERT(efi_system_table != NULL, "EFI system table is NULL");

    EFI_SYSTEM_TABLE* st = efi_system_table;
    ASSERT(st->boot_services != NULL, "EFI boot services are NULL");

    EFI_GUID gop_guid = {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}};
    void* gop_interface = NULL;

    EFI_STATUS status = st->boot_services->locate_protocol(&gop_guid, NULL, &gop_interface);
    if (status != EFI_SUCCESS || gop_interface == NULL) {
        ERROR("Failed to locate Graphics Output Protocol (GOP)");
        return NULL;
    }

    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = (EFI_GRAPHICS_OUTPUT_PROTOCOL*)gop_interface;
    if (gop->mode == NULL || gop->mode->frame_buffer_base == 0) {
        ERROR("GOP mode is NULL or framebuffer base is 0");
        return NULL;
    }

    return (void*)(uintptr_t)(gop->mode->frame_buffer_base);

}
