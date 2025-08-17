#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    size_t base;     // Base address of the memory region
    size_t size;     // Size of the memory region in bytes
    bool is_free;    // Indicates if the memory region is free
} MemoryRegion;

#ifdef __cplusplus
}
#endif