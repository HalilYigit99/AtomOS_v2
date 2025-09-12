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
#include <graphics/screen.h>
#include <sleep.h>

void gfx_draw_task();

extern void acpi_poweroff(); // ACPI power off function
extern void acpi_restart();  // ACPI restart function

extern unsigned char logo_128x128_bmp[];
extern unsigned int logo_128x128_bmp_len;
extern void print_memory_regions(void);
extern GFXTerminal *debug_terminal;

extern void efi_reset_to_firmware();

static size_t nextVideoModeIndex = 0;

void kmain()
{
    LOG("Welcome to AtomOS!");
    LOG("Booted in %s mode", mb2_is_efi_boot ? "EFI" : "BIOS");
    LOG("Framebuffer: %ux%u, %u bpp", main_screen.mode->width, main_screen.mode->height, main_screen.mode->bpp);

    void *test = malloc(16 * 1024 * 1024);
    if (test)
        free(test);
    else
        LOG("Heap expand failed!");

    // Print PCI devices
    LOG("Scanning PCI bus...");
    PCI_Rescan(true);

    List *pciDevices = PCI_GetDeviceList();

    for (ListNode *node = pciDevices->head; node != NULL; node = node->next)
    {
        PCIDevice *dev = (PCIDevice *)node->data;
        char *class = PCI_GetClassName(dev->classCode);
        char *subclass = PCI_GetSubClassName(dev->classCode, dev->subclass);

        LOG("PCI %02X:%02X.%X - %04X:%04X - %s / %s", dev->bus, dev->device, dev->function, dev->vendorID, dev->deviceID, class, subclass);
    }

    LOG("Supported video modes:");
    for (ListNode *node = main_screen.video_modes->head; node != NULL;
         node = node->next)
    {
        ScreenVideoModeInfo *mode = (ScreenVideoModeInfo *)node->data;
        LOG(" Mode %u: %ux%u, %u bpp", mode->mode_number, mode->width, mode->height, mode->bpp);
    }
    LOG("Current video mode: %ux%u, %u bpp", main_screen.mode->width, main_screen.mode->height, main_screen.mode->bpp);

    LOG("Press ESC to exit to firmware, P to power off, R to restart, T to show uptime, N to list memory regions, M to toggle mouse, W/S to scroll, K to change video mode");

    while (1)
    {
        while (keyboardInputStream.available() == 0)
            asm volatile("hlt");
        char c;
        if (keyboardInputStream.readChar(&c) > 0)
        {
            if (c == 27) // ESC key to exit
            {
                LOG("ESC pressed, exiting to firmware...");
                if (mb2_is_efi_boot)
                {
                    efi_reset_to_firmware();
                }
                else
                {
                    LOG("Not in EFI mode, powering off instead");
                    sleep_ms(1000);
                    acpi_poweroff();
                }
                while (1)
                    asm volatile("cli; hlt");
            }
            else if (c == 'w')
            {
                gfxterm_scroll(debug_terminal, -1);
            }
            else if (c == 's')
            {
                gfxterm_scroll(debug_terminal, 1);
            }
            else if (c == 'p')
            {
                acpi_poweroff();
            }
            else if (c == 'r')
            {
                LOG("Restart requested via keyboard");
                acpi_restart();
            }
            else if (c == 't')
            {
                LOG("Current uptime: %llu ms", uptimeMs);
            }
            else if (c == 'm')
            {
                mouse_enabled = !mouse_enabled;
                LOG("Mouse %s", mouse_enabled ? "enabled" : "disabled");
            }
            else if (c == 'n')
            {
                print_memory_regions();
            }else if (c == 'k')
            {
                LOG("Changing video mode...");
                
                if (main_screen.video_modes->count > 1)
                {
                    nextVideoModeIndex = (nextVideoModeIndex + 1) % main_screen.video_modes->count;
                    ListNode *node = main_screen.video_modes->head;
                    for (size_t i = 0; i < nextVideoModeIndex; i++)
                    {
                        if (node->next)
                            node = node->next;
                    }
                    ScreenVideoModeInfo *nextMode = (ScreenVideoModeInfo *)node->data;
                    screen_changeVideoMode(&main_screen, nextMode);
                    LOG("Current mode info:\n Mode %u: %ux%u, %u bpp", nextMode->mode_number, nextMode->width, nextMode->height, nextMode->bpp);
                    gfxterm_resize(debug_terminal, (gfx_size){nextMode->width / debug_terminal->font->size.width, nextMode->height / debug_terminal->font->size.height});
                    gfxterm_redraw(debug_terminal);
                }else {
                    LOG("Only one video mode available, cannot switch");
                }
            }
        }
    }
}
