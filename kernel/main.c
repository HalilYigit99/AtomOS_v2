#include <sleep.h>
#include <debug/debug.h>
#include <graphics/gfx.h>
#include <stream/OutputStream.h>
#include <graphics/bmp.h>
#include <graphics/screen.h>
#include <gfxterm/gfxterm.h>

extern GFXTerminal* debug_terminal;

extern unsigned char logo_128x128_bmp[];
extern unsigned int logo_128x128_bmp_len;

extern void acpi_poweroff(); // ACPI power off function
extern void acpi_restart();
extern void gfx_draw_task();

void Main(){

    gfx_bitmap* bitmap = bmp_load_from_memory(logo_128x128_bmp, logo_128x128_bmp_len);

    gfxterm_visible(debug_terminal, false);

    gfx_resize_buffer(screen_buffer, main_screen.mode->width, main_screen.mode->height);

    gfx_draw_bitmap(screen_buffer, main_screen.mode->width / 2 - bitmap->size.width / 2, main_screen.mode->height / 2 - bitmap->size.height / 2, bitmap->pixels, bitmap->size.width, bitmap->size.height);

    uint64_t end = uptimeMs + 10000;
    while (uptimeMs < end)
    {
       
    }

    gfx_screen_unregister_buffer(screen_buffer);

    LOG("Shutting down...");

    sleep_ms(10000);

    acpi_poweroff();

}
