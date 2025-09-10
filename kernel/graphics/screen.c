#include <graphics/screen.h>
#include <boot/multiboot2.h>
#include <memory/memory.h>
#include <panic.h>
#include <driver/efi_gop/efi_gop.h>
#include <debug/debug.h>

ScreenInfo main_screen = {0};

List *screen_list = NULL;

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
        efi_gop_detect(&main_screen);
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
