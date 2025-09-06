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
    MemoryRegionType_EFI_RT_CODE = 3,
    MemoryRegionType_EFI_RT_DATA = 4,
    MemoryRegionType_ACPI_RECLAIMABLE = 5,
    MemoryRegionType_ACPI_NVS = 6,
    MemoryRegionType_BAD_MEMORY = 7,
    MemoryRegionType_PCI_RESOURCE = 8,
    MemoryRegionType_EFI_BS_CODE = 9,
    MemoryRegionType_EFI_BS_DATA = 10,
    MemoryRegionType_EFI_LOADER_CODE = 11,
    MemoryRegionType_EFI_LOADER_DATA = 12,
    // Add more types as needed
} MemoryRegionType;

typedef struct MemoryRegion {
    size_t base;     // Base address of the memory region
    size_t size;     // Size of the memory region in bytes
    MemoryRegionType type; // Type of the memory region
} MemoryRegion;

// Ard arda gelen USABLE blokları birleştir 
void pmm_maintain();

// Fiziksel bellekten blok tahsis et
void* pmm_alloc(size_t sizeInKB);

// Fiziksel bellekteki bloğu serbest bırak
void pmm_free(void* ptr);

#ifdef __cplusplus
}
#endif