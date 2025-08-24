#include <memory/memory.h>
#include <memory/heap.h>
#include <memory/pmm.h>

#define HEAP_MAGIC 0xDEADBEEF
#define HEAP_NODE_MIN_SIZE (sizeof(size_t) * 2)

typedef struct HeapNode
{
    uint32_t magic; // Magic number for validation
    bool is_free; // Flag to indicate if the block is free
    struct HeapNode* next; // Pointer to the next node in the free list
} HeapNode;

#define NODE_SIZE(node) (node->next ? ((size_t)node->next - (size_t)node - sizeof(HeapNode)) : 0)

// Linker-provided symbols that delimit the local heap region
// Declare as arrays to avoid array-bounds warnings and allow taking addresses safely.
extern uint8_t __local_heap_start[];
extern uint8_t __local_heap_end[];

HeapRegion* first_heap_region = NULL;

HeapRegion localHeapRegion;

static void initRegion(HeapRegion* region)
{
    if (region == NULL) return;

    if (region->base == 0 || region->size == 0) return;

    // Require at least room for a head node and a terminal end node
    if (region->size < 2 * sizeof(HeapNode)) return;

    HeapNode* initial_node = (HeapNode*)region->base;
    HeapNode* end_node = (HeapNode*)(region->base + region->size - sizeof(HeapNode));

    initial_node->magic = (uint32_t)HEAP_MAGIC;
    initial_node->is_free = true;
    initial_node->next = end_node;

    end_node->magic = 0; // Mark the end node with a magic value of 0
    end_node->next = NULL;
    end_node->is_free = false; // End node is not free

}

static void* alloc_region(HeapRegion* region, size_t size)
{
    HeapNode* node = (HeapNode*)(region->base);
    while (node && node->magic == HEAP_MAGIC)
    {
        
        if (node->is_free && NODE_SIZE(node) >= size)
        {
            size_t remaining_size = NODE_SIZE(node) - size;
            if (remaining_size >= HEAP_NODE_MIN_SIZE + sizeof(HeapNode))
            {
                // Split the block
                HeapNode* new_node = (HeapNode*)((char*)node + size + sizeof(HeapNode));
                new_node->magic = HEAP_MAGIC;
                new_node->is_free = true;
                new_node->next = node->next;

                node->is_free = false;
                node->next = new_node;

                return (void*)((char*)node + sizeof(HeapNode));
            }
            else
            {
                // Use the entire block
                node->is_free = false;
                return (void*)((char*)node + sizeof(HeapNode));
            }
        }
        // Advance to next node when not returning above
        node = node->next;
    }

    return NULL; // No suitable block found
}

static bool free_region(HeapRegion* region, void* ptr)
{
    if (ptr == NULL) return false;

    HeapNode* node = (HeapNode*)region->base;
    HeapNode* prev = NULL;

    while (node && node->magic == HEAP_MAGIC)
    {
        if (
            (size_t)node < (size_t)ptr &&
            (size_t)ptr < (size_t)node->next
        )
        {
            node->is_free = true;

            // Coalesce with next node if it's free
            if (node->next && node->next->is_free)
            {
                node->next = node->next->next;
            }

            // If have previous node and it's free, coalesce with it
            if (prev && prev->is_free)
            {
                prev->next = node->next;
                prev->is_free = true;
                prev->magic = HEAP_MAGIC;
            }

            return true;
        }
        prev = node;
        node = node->next;
    }

    return false;
}

void heap_init()
{
    localHeapRegion.base = (size_t)(uintptr_t)__local_heap_start;
    localHeapRegion.size = (size_t)((uintptr_t)__local_heap_end - (uintptr_t)__local_heap_start);
    localHeapRegion.next = NULL;

    first_heap_region = &localHeapRegion;

    initRegion(&localHeapRegion);

}

void* heap_alloc(size_t n) {
    if (n <= 0) return NULL;

    if (first_heap_region == NULL) {
        heap_init();
    }

    HeapRegion* region = first_heap_region;
    while (region) {
        void* ptr = alloc_region(region, n);
        if (ptr) {
            return ptr;
        }
        region = region->next;
    }

    return NULL; // No memory available
}

void heap_free(void* ptr) {
    if (ptr == NULL) return;

    if (first_heap_region == NULL) {
        heap_init();
    }

    HeapRegion* region = first_heap_region;
    while (region) {
        if (free_region(region, ptr)) {
            return; // Successfully freed
        }
        region = region->next;
    }
}

void *heap_realloc(void* ptr, size_t new_size) {
    if (new_size <= 0) {
        heap_free(ptr);
        return NULL;
    }

    if (ptr == NULL) {
        return heap_alloc(new_size);
    }

    HeapNode* node = (HeapNode*)((char*)ptr - sizeof(HeapNode));
    if (node->magic != HEAP_MAGIC) {
        return NULL; // Invalid pointer
    }

    size_t old_size = NODE_SIZE(node);
    if (new_size <= old_size) {
        return ptr; // No need to reallocate
    }

    void* new_ptr = heap_alloc(new_size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, old_size); // Copy old data to new location
        heap_free(ptr); // Free old memory
    }
    
    return new_ptr;
}

void *heap_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;

    void* ptr = heap_alloc(count * size);
    if (ptr) {
        memset(ptr, 0, count * size); // Zero out the allocated memory
    }
    
    return ptr;
}

void* heap_aligned_alloc(size_t alignment, size_t size)
{
    if (alignment == 0 || size == 0 || (alignment & (alignment - 1)) != 0) {
        return NULL; // Invalid alignment or size
    }

    void* ptr = heap_alloc(size + alignment - 1 + sizeof(HeapNode));
    if (!ptr) return NULL;

    // Align the pointer
    uintptr_t aligned_ptr = ((uintptr_t)ptr + sizeof(HeapNode) + alignment - 1) & ~(alignment - 1);
    
    HeapNode* node = (HeapNode*)((char*)aligned_ptr - sizeof(HeapNode));
    node->magic = HEAP_MAGIC;
    node->is_free = false;
    node->next = NULL;

    return (void*)aligned_ptr;
}
