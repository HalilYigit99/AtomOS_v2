#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define HEAP_NODE_MAGIC 0xDEADBEEF

typedef struct HeapNode_t {
    bool allocated;
    struct HeapNode_t* next;
    uint32_t magic; // Magic number for debugging
} HeapNode;

typedef struct HeapRegion_t
{
    HeapNode* firstNode;
    size_t size;
    struct HeapRegion_t* next;
} HeapRegion;

typedef struct
{
    HeapRegion* firstRegion;
} Heap;

extern Heap kernel_heap;

void* heap_alloc(Heap* heap, size_t size);
void heap_free(Heap* heap, void* ptr);
void* heap_realloc(Heap* heap, void* ptr, size_t new_size);
void* heap_calloc(Heap* heap, size_t count, size_t size);
void* heap_alloc_aligned(Heap* heap, size_t size, size_t alignment);
void* heap_aligned_alloc(Heap* heap, size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif
