#include <util/assert.h>
#include <debug/debug.h>
#include <acpi/acpi.h>
#include <sleep.h>
#include <graphics/gfx.h>

void __assert_v1(const char* condition, const char* file, int line, const char* message);

void(*___assert_func)(const char* condition, const char* file, int line, const char* message) = __assert_v1;

extern void gfx_draw_task();

extern void acpi_poweroff();
void __assert_v1(const char* condition, const char* file, int line, const char* message) {

    asm volatile ("cli"); // Disable interrupts

    LOG("ASSERTION FAILED: %s\nFile: %s, Line: %d\nMessage: %s\n", condition, file, line, message);
    LOG("Disabling interrupts...");

    gfx_draw_task();

    if (acpi_version != 0)
    {
        for (size_t i = 0; i < 0xFFFFFFFF; i++) asm volatile ("pause"); // Basit bir bekleme döngüsü
        acpi_poweroff();
    }else{
        // Acpi not initialized
        while (1) {
            asm volatile ("hlt");
        }
    }

}
