#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>
#include <graphics/gfx.h>
#include <graphics/bmp.h>
#include <irq/IRQ.h>
#include <time/timer.h>
#include <pci/PCI.h>
#include <memory/memory.h>
#include <arch.h>
#include <gfxterm/gfxterm.h>
#include <stream/OutputStream.h>

void gfx_draw_task();

extern void acpi_poweroff(); // ACPI power off function
extern void acpi_restart();  // ACPI restart function

extern unsigned char logo_128x128_bmp[];
extern unsigned int logo_128x128_bmp_len;

extern GFXTerminal* debug_terminal;

extern void efi_reset_to_firmware();

void kmain()
{
    LOG("Welcome to AtomOS!");
    LOG("Booted in %s mode", mb2_is_efi_boot ? "EFI" : "BIOS");
    LOG("Framebuffer: %ux%u, %u bpp", mb2_framebuffer->framebuffer_width, mb2_framebuffer->framebuffer_height, mb2_framebuffer->framebuffer_bpp);

    void* test = malloc(16 * 1024 * 1024);
    if (test) free(test);
    else LOG("Heap expand failed!");

    while (1)
    {
        while (keyboardInputStream.available() == 0) asm volatile ("hlt");
        char c;
        if (keyboardInputStream.readChar(&c) > 0)
        {
            if (c == 27) // ESC key to exit
            {
                LOG("ESC pressed, exiting to firmware...");
                if (mb2_is_efi_boot)
                {
                    efi_reset_to_firmware();
                }else {
                    acpi_poweroff();
                }
                while (1) asm volatile ("cli; hlt");
            }else
            if (c == 'w')
            {
                gfxterm_scroll(debug_terminal, -1);
            }else
            if (c == 's')
            {
                gfxterm_scroll(debug_terminal, 1);
            }else
            if (c == 'p')
            {
                acpi_poweroff();
            }else
            if (c == 'r')
            {
                LOG("Restart requested via keyboard");
                acpi_restart();
            }
        }
    }

}
