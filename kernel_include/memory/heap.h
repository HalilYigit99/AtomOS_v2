#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct HeapRegion
{
    size_t base;
    size_t size;
    struct HeapRegion* next;
} HeapRegion;

void heap_init();

void* heap_alloc(size_t size);
void heap_free(void* ptr);
void* heap_realloc(void* ptr, size_t size);
void* heap_calloc(size_t count, size_t size);
void* heap_aligned_alloc(size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif
