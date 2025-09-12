#include <graphics/screen.h>
#include <boot/multiboot2.h>
#include <memory/memory.h>
#include <panic.h>
#include <driver/efi_gop/efi_gop.h>
#include <debug/debug.h>
#include <driver/DriverBase.h>
#include <graphics/gfx.h>

ScreenInfo main_screen = {0};

List *screen_list = NULL;

extern gfx_buffer* hardware_buffer;

extern DriverBase efi_gop_driver;

void screen_init()
{
    screen_list = List_Create();

    main_screen.id = 0;
    main_screen.name = "Main Screen";
    main_screen.video_modes = List_Create();
    main_screen.mode = NULL;

    // EFI modunda GOP üzerinden enumerate
    if (mb2_is_efi_boot)
    {
        system_driver_register(&efi_gop_driver);
        system_driver_enable(&efi_gop_driver);
    }

    // EFI başarısızsa veya BIOS modundaysak Multiboot2 framebuffer bilgisini kullan
    if (mb2_framebuffer)
    {
        ScreenVideoModeInfo *mode = (ScreenVideoModeInfo *)malloc(sizeof(ScreenVideoModeInfo));
        mode->framebuffer = (void *)(uintptr_t)mb2_framebuffer->framebuffer_addr;
        mode->width = mb2_framebuffer->framebuffer_width;
        mode->height = mb2_framebuffer->framebuffer_height;
        mode->pitch = mb2_framebuffer->framebuffer_pitch;
        mode->bpp = mb2_framebuffer->framebuffer_bpp;
        mode->mode_number = 0;
        mode->linear_framebuffer = true;

        main_screen.mode = mode;
        if (!mb2_is_efi_boot) List_Add(main_screen.video_modes, mode);
    }
    else
    {
        PANIC("No framebuffer provided by bootloader!");
    }

    List_Add(screen_list, &main_screen);

    LOG("screen: %u screen(s) initialized", (unsigned)screen_list->count);
    LOG("screen: main screen resolution %ux%u, %zubpp, pitch=%zu, fb=%p",
        (unsigned)main_screen.mode->width,
        (unsigned)main_screen.mode->height,
        main_screen.mode->bpp,
        main_screen.mode->pitch,
        main_screen.mode->framebuffer);
}

extern void efi_gop_setVideoMode(ScreenInfo* screen, ScreenVideoModeInfo* mode);

void screen_changeVideoMode(ScreenInfo* screen, ScreenVideoModeInfo* mode)
{
    if (!screen || !mode)
    {
        ERROR("screen_changeVideoMode: Invalid parameters");
        return;
    }

    bool found = false;
    for (ListNode* node = screen->video_modes->head; node != NULL; node = node->next)
    {
        if (node->data == mode)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        ERROR("screen_changeVideoMode: Specified mode not found in screen's mode list");
        return;
    }

    if (mb2_is_efi_boot)
    {
        efi_gop_setVideoMode(screen, mode);
        hardware_buffer->bpp = main_screen.mode->bpp;
        hardware_buffer->size.width = main_screen.mode->width;
        hardware_buffer->size.height = main_screen.mode->height;
        hardware_buffer->buffer = main_screen.mode->framebuffer;
    }else {
        WARN("screen_changeVideoMode: Changing video modes is not supported in BIOS mode");
    }

}