#include <stddef.h>
#include <stdint.h>
#include <machine/machine.h>
#include <debug/debug.h>
#include <boot/multiboot2.h>

static __attribute__((optimize("O0"))) bool RamLocationAvailable(size_t addr)
{
    uint8_t *ptr = (uint8_t *)addr;

    const uint8_t testValue = 0xAA; // Arbitrary test value
    uint8_t old = *ptr;

    *ptr = testValue; // Write test value

    if (*ptr == testValue)
    {
        *ptr = old;  // Restore original value
        return true; // Memory is writable
    }
    else
    {
        return false; // Memory is not available
    }
}

void bios_init(void)
{

    // Check if the Multiboot2 has basic memory information
    if (mb2_basic_meminfo)
    {
        // Set the RAM size in KB from the Multiboot2 tag
        machine_ramSizeInKB = mb2_basic_meminfo->mem_upper + mb2_basic_meminfo->mem_lower;
    }
    else
    {
        // Brute force scan for memory size

        LOG("No Multiboot2 basic memory info found, scanning for available RAM...");

        machine_ramSizeInKB = 0; // Default to 0 if no memory info

        static size_t testGears[] = {1024 * 1024 * 1024, // 1 GB
                                    512 * 1024 * 1024,  // 512 MB
                                    256 * 1024 * 1024,  // 256 MB
                                    128 * 1024 * 1024,  // 128 MB
                                    64 * 1024 * 1024,   // 64 MB
                                    32 * 1024 * 1024,   // 32 MB
                                    16 * 1024 * 1024,   // 16 MB
                                    8 * 1024 * 1024,    // 8 MB
                                    4 * 1024 * 1024,    // 4 MB
                                    2 * 1024 * 1024,    // 2 MB
                                    1 * 1024 * 1024};   // 1 MB

        size_t currentTestAddress = 0;

        for (size_t i = 0; i < 11; i++)
        {
            bool failed = false;
            while (!failed)
            {
                size_t testAddr = currentTestAddress + testGears[i];

                if (RamLocationAvailable(testAddr))
                {
                    machine_ramSizeInKB += testGears[i] / 1024; // Convert bytes to KB
                    currentTestAddress = testAddr;
                }
                else
                {
                    LOG("No RAM found at: %p KB", testAddr);
                    failed = true; // Stop scanning this gear
                }
            }
        }
    }

    machine_ramSizeInKB += 1024; // Add 1 MB for kernel and other overhead

    LOG("BIOS RAM Size: %zu KB ( %zu MB )", machine_ramSizeInKB, machine_ramSizeInKB / 1024);

    LOG("BIOS initialized successfully");
}
