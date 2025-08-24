#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    MemoryRegionType_UNKNOWN = -1,
    MemoryRegionType_NULL = 0,
    MemoryRegionType_USABLE = 1,
    MemoryRegionType_RESERVED = 2,
    MemoryRegionType_EFI_CODE = 3,
    MemoryRegionType_EFI_DATA = 4,
    MemoryRegionType_ACPI_RECLAIMABLE = 5,
    MemoryRegionType_ACPI_NVS = 6,
    MemoryRegionType_BAD_MEMORY = 7,
    MemoryRegionType_PCI_RESOURCE = 8
    // Add more types as needed
} MemoryRegionType;

typedef struct MemoryRegion {
    size_t base;     // Base address of the memory region
    size_t size;     // Size of the memory region in bytes
    MemoryRegionType type; // Type of the memory region
} MemoryRegion;

#ifdef __cplusplus
}
#endif