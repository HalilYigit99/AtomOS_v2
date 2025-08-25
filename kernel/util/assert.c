#include <util/assert.h>
#include <debug/debug.h>
#include <acpi/acpi.h>

void __assert_v1(const char* condition, const char* file, int line, const char* message);

void(*___assert_func)(const char* condition, const char* file, int line, const char* message) = __assert_v1;


extern void acpi_poweroff();
void __assert_v1(const char* condition, const char* file, int line, const char* message) {
    debugStream->printf("ASSERTION FAILED: %s\nFile: %s, Line: %d\nMessage: %s\n", condition, file, line, message);
    // Burada sistemin durdurulması veya başka bir hata işleme mekanizması eklenebilir
    asm volatile ("cli");

    for (size_t i = 0; i < 0xFFFFFFF; i++); // Basit bir bekleme döngüsü

    if (acpi_version != 0)
    {
        // Acpi initialized
        acpi_poweroff();
    }else{
        // Acpi not initialized
        while (1) {
            asm volatile ("hlt");
        }
    }

}
