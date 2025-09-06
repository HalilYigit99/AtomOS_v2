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

void gfx_draw_task();

extern void acpi_poweroff(); // ACPI power off function

extern unsigned char logo_128x128_bmp[];
extern unsigned int logo_128x128_bmp_len;

extern void efi_reset_to_firmware();

void kmain() {

    LOG("Welcome to AtomOS!");
    LOG("Booted in %s mode", mb2_is_efi_boot ? "EFI" : "BIOS");
    LOG("Framebuffer: %ux%u, %u bpp", mb2_framebuffer->framebuffer_width, mb2_framebuffer->framebuffer_height, mb2_framebuffer->framebuffer_bpp);

    gfx_clear_buffer(screen_buffer, (gfx_color){.argb = 0}); // Clear the hardware buffer with a black color

    gfx_bitmap* bitmap = bmp_load_from_memory(logo_128x128_bmp, logo_128x128_bmp_len);
    if (bitmap) {
        // Signed ve güvenli merkez hesaplama
        int x = ((int)screen_buffer->size.width - (int)bitmap->size.width) / 2;
        int y = ((int)screen_buffer->size.height - (int)bitmap->size.height) / 2;

        // Tamamen taşma durumunda erken dönüş yapan draw fonksiyonuna uygun olsun diye alt sınırı sıfıra sabitle
        if (x < 0) x = 0;
        if (y < 0) y = 0;

        // Piksel verisini geç (struct değil)
        gfx_draw_bitmap(screen_buffer, x, y, (void*)bitmap->pixels, bitmap->size.width, bitmap->size.height);
        // bitmap belleği sadece tekrar ihtiyaç olmayacaksa serbest bırakılabilir
        bmp_free(bitmap);
    } else {
        LOG("Failed to load logo bitmap from memory");
    }

    if (pit_timer){
        pit_timer->setFrequency(1); // Set PIT timer frequency to 1Hz
        pit_timer->add_callback(gfx_draw_task); // Add the cursor update task to the PIT timer callbacks
        LOG("PIT timer callbacks registered");
    }

    gfx_draw_task(); // Initial draw to show the logo

    // Clear keyboard input stream
    while (keyboardInputStream.available())
    {
        char c;
        keyboardInputStream.readChar(&c); // Read and discard characters
    }

    LOG("Press 's' to shutdown the system.");
    if (mb2_is_efi_boot) {
        LOG("Press 'r' to reboot to firmware (UEFI).");
    }
    LOG("Press 'j' to trigger a page fault (for testing).");
    while (1) {
        
        if (keyboardInputStream.available())
        {
            char c;
            keyboardInputStream.readChar(&c); // Read the character from the keyboard input stream

            if (c == 's')
            {
                // Shutdown the system
                LOG("Shutting down the system...");
                acpi_poweroff(); // Call the ACPI power off function
            }else
            if (c == 'j')
            {
                uint8_t* a = (uint8_t*)0xFFFFFFFF;
                *a = 5;
                if (*a == 5) {
                    LOG("???");
                }else {
                    LOG("OK!");
                }
            }
            else
            if (c == 'r' && mb2_is_efi_boot) {
                LOG("Rebooting to firmware (UEFI)...");
                efi_reset_to_firmware();
            }else {
                if (c) LOG("Key pressed : %c (%08x)", c, c);
            }
        }else asm volatile ("hlt"); // No input, wait
    }

}