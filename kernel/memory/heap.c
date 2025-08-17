#include <memory/heap.h>
#include <memory/pmm.h>
#include <boot/multiboot2.h>
#include <debug/debug.h>
#include <util/string.h>

Heap kernel_heap = {0};

extern void* __kernel_end;

static HeapRegion firstRegion;

// Küçük kuyruk parçalarını engellemek için minimum bölme artığı
#define HEAP_MIN_SPLIT_REMAINDER 16

void* heap_alloc_in_region(HeapRegion* region, size_t size) {
    HeapNode* current = region->firstNode;
    while (current) {
        // Liste bütünlüğü kontrolü
        if (current->magic != HEAP_NODE_MAGIC) {
            ERROR("heap: corrupted node header at %p", (void*)current);
            return NULL;
        }

        HeapNode* next = current->next;
        if (next && next->magic != HEAP_NODE_MAGIC) {
            ERROR("heap: corrupted next header at %p", (void*)next);
            return NULL;
        }

        if (!current->allocated && next) {
            size_t capacity = (size_t)((uint8_t*)next - (uint8_t*)current) - sizeof(HeapNode);
            if (capacity >= size) {
                // Bölme kararını ver
                if (capacity >= size + sizeof(HeapNode) + HEAP_MIN_SPLIT_REMAINDER) {
                    // Yeni boş düğüm oluştur
                    uint8_t* payload = (uint8_t*)current + sizeof(HeapNode);
                    HeapNode* newNode = (HeapNode*)(payload + size);
                    newNode->allocated = false;
                    newNode->next = next;
                    newNode->magic = HEAP_NODE_MAGIC;

                    current->next = newNode;
                    current->allocated = true;
                    return (void*)payload;
                } else {
                    // Tüm bloğu tahsis et (kalan küçük kuyruk kullanılamaz)
                    current->allocated = true;
                    return (void*)((uint8_t*)current + sizeof(HeapNode));
                }
            }
        }
        current = current->next;
    }
    return NULL; // No suitable block found
}

void heap_free_in_region(HeapRegion* region, void* ptr) {
    if (!ptr) return;

    HeapNode* node = (HeapNode*)region->firstNode;
    HeapNode* prev = NULL;

    // Sahip düğümü tara
    while (node && node->magic == HEAP_NODE_MAGIC) {
        if ((void*)node < ptr && node->next && (void*)node->next > ptr) {
            break; // Found the owner node
        }
        prev = node;
        node = node->next;
    }

    if (!node || node->magic != HEAP_NODE_MAGIC) {
        ERROR("heap_free: pointer %p does not belong to region %p", ptr, (void*)region);
        return;
    }

    // Serbest bırak
    node->allocated = false;

    // İleri doğru tam koalesce
    while (node->next && node->next->magic == HEAP_NODE_MAGIC && !node->next->allocated) {
        node->next = node->next->next;
    }

    // Geriye doğru koalesce (prev de boşsa birleştir)
    if (prev && prev->magic == HEAP_NODE_MAGIC && !prev->allocated) {
        // prev ile node (ve node'un ileri zinciri) birleşsin
        prev->next = node->next;
    }
}

void heap_init_region(HeapRegion* region) {

    HeapNode* firstNode = (HeapNode*)region->firstNode;
    HeapNode* endNode = (HeapNode*)((uint8_t*)region->firstNode + region->size - sizeof(HeapNode));

    firstNode->allocated = false;
    firstNode->next = endNode;
    firstNode->magic = HEAP_NODE_MAGIC;

    endNode->allocated = true; // Sentinel node
    endNode->next = NULL;
    endNode->magic = HEAP_NODE_MAGIC;

}

void heap_init() {

    LOG("Initializing kernel heap");

    LOG("Kernel end address: %p", (void*)&__kernel_end);

    uint32_t memoryRegionCount = 0;
    struct multiboot_mmap_entry* entries = multiboot2_get_memory_map(&memoryRegionCount);

    if (memoryRegionCount == 0) {
        ERROR("No memory regions detected!!");
        asm volatile ("cli; hlt");
    }

    LOG("Detected %u memory regions", memoryRegionCount);

    size_t firstAvailableBase = 0;
    size_t firstAvailableEnd = 0;
    
    for (size_t i = 0; i < memoryRegionCount; i++) {
        struct multiboot_mmap_entry* entry = &entries[i];
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {

            size_t entryBase = (size_t)entry->addr;
            size_t entryEnd = entryBase + entry->len;

            if (entryBase < (size_t)&__kernel_end) {
                if (entryEnd > (size_t)&__kernel_end) {
                    entryBase = (size_t)&__kernel_end; // yalnızca yerel değişkeni ayarla, MMAP'i değiştirme
                } else {
                    LOG("Skipping memory region below kernel end: %p - %p", (void*)entryBase, (void*)entryEnd);
                    continue; // Skip regions below the kernel end
                }
            }

            // Bitişik ve kullanılabilir bölgeleri güvenli şekilde birleştir
            size_t k = i;
            while ((k + 1) < memoryRegionCount) {
                struct multiboot_mmap_entry* n = &entries[k + 1];
                if (n->type != MULTIBOOT_MEMORY_AVAILABLE) break;
                size_t cur_end = (size_t)entries[k].addr + entries[k].len;
                if (cur_end != (size_t)n->addr) break; // sadece bitişikse birleştir
                k++;
            }
            if (k > i) {
                entryEnd = (size_t)entries[k].addr + entries[k].len;
            }

            size_t size = entryEnd - entryBase;

            if (size >= 64 * 1024 * 1024)
            {
                entryEnd = entryBase + 64 * 1024 * 1024; // Limit to 64MB for the first region
                LOG("Limiting memory region to 64MB: %p - %p", (void*)entryBase, (void*)entryEnd);
            }

            if (size < 16 * 1024 * 1024) {
                LOG("Skipping small memory region: %p - %p (size: %zu bytes)", (void*)entryBase, (void*)entryEnd, size);
                continue; // Skip regions smaller than 16MB
            }

            firstAvailableBase = entryBase;
            firstAvailableEnd = entryEnd;

            break;

        }
    }

    if (firstAvailableBase == 0 ||firstAvailableEnd == 0) {
        ERROR("No suitable memory region found for heap initialization");
        asm volatile ("cli; hlt");
    }

    size_t bytes = firstAvailableEnd - firstAvailableBase;

    LOG("Using first available memory region: %p - %p", (void*)firstAvailableBase, (void*)firstAvailableEnd);
    LOG("Region size: %zu KB ( %zu MB )", bytes / 1024, bytes / (1024 * 1024));

    firstRegion = (HeapRegion){
        .firstNode = (HeapNode*)firstAvailableBase,
        .size = firstAvailableEnd - firstAvailableBase,
        .next = NULL
    };

    kernel_heap.firstRegion = &firstRegion;

    heap_init_region(&firstRegion);

}

void* heap_alloc(Heap* heap, size_t n) {
    if (!heap || n == 0) return NULL;

    HeapRegion* region = heap->firstRegion;
    while (region) {
        void* ptr = heap_alloc_in_region(region, n);
        if (ptr) {
            LOG("Allocated %zu bytes at %p in region %p", n, ptr, (void*)region);
            return ptr;
        }
        region = region->next;
    }

    ERROR("Failed to allocate %zu bytes from heap", n);
    return NULL; // No suitable block found
}

void heap_free(Heap* heap, void* ptr) {
    if (!heap || !ptr) return;

    HeapRegion* region = heap->firstRegion;
    while (region) {
        if ((void*)region->firstNode <= ptr && (void*)((uint8_t*)region->firstNode + region->size) > ptr) {
            heap_free_in_region(region, ptr);
            return;
        }
        region = region->next;
    }

    ERROR("Pointer %p not found in any heap region", ptr);
}

void* heap_realloc(Heap* heap, void* ptr, size_t new_size) {
    // Follow alloc/free semantics: if new_size==0, free and return NULL; if ptr==NULL, allocate new.
    if (!heap) return NULL;
    if (new_size == 0) {
        if (ptr) heap_free(heap, ptr);
        return NULL;
    }
    if (!ptr) return heap_alloc(heap, new_size);

    // Find the region and the containing node by scanning (same idea as heap_free).
    HeapRegion* region = heap->firstRegion;
    while (region) {
        uint8_t* region_begin = (uint8_t*)region->firstNode;
        uint8_t* region_end = region_begin + region->size;
        if ((uint8_t*)ptr > region_begin && (uint8_t*)ptr < region_end) {
            HeapNode* node = region->firstNode;
            while (node && node->magic == HEAP_NODE_MAGIC) {
                if ((void*)node < ptr && (void*)node->next > ptr) {
                    break;
                }
                node = node->next;
            }

            if (!node || node->magic != HEAP_NODE_MAGIC || !node->allocated) {
                ERROR("realloc: pointer %p does not refer to a valid allocated block", ptr);
                return NULL;
            }

            size_t current_size = (size_t)((uint8_t*)node->next - (uint8_t*)node - sizeof(HeapNode));
            if (new_size <= current_size) {
                return ptr; // Already big enough
            }

            // Try to grow in-place by merging with next if it's free.
            HeapNode* nxt = node->next;
            if (nxt && nxt->magic == HEAP_NODE_MAGIC && !nxt->allocated && nxt->next) {
                size_t merged_size = (size_t)((uint8_t*)nxt->next - (uint8_t*)node - sizeof(HeapNode));
                if (merged_size >= new_size) {
                    node->next = nxt->next; // consume the next free block entirely
                    return ptr;
                }
            }

            // Fallback: allocate new, copy, free old
            void* new_ptr = heap_alloc(heap, new_size);
            if (!new_ptr) {
                ERROR("realloc: failed to allocate %zu bytes for %p", new_size, ptr);
                return NULL;
            }
            // Copy out the old content up to current_size
            memcpy(new_ptr, ptr, current_size);
            heap_free(heap, ptr);
            return new_ptr;
        }
        region = region->next;
    }

    ERROR("realloc: pointer %p not found in any heap region", ptr);
    return NULL;
}

void* heap_calloc(Heap* heap, size_t count, size_t size) {
    if (!heap || count == 0 || size == 0) return NULL;
    // Detect overflow in multiplication
    if (size != 0 && count > (SIZE_MAX / size)) {
        ERROR("calloc: size overflow for count=%zu size=%zu", count, size);
        return NULL;
    }
    size_t total = count * size;
    void* ptr = heap_alloc(heap, total);
    if (!ptr) {
        ERROR("calloc: failed to allocate %zu bytes", total);
        return NULL;
    }
    memset(ptr, 0, total);
    return ptr;
}

void* heap_alloc_aligned(Heap* heap, size_t size, size_t alignment) {
    // alignment must be power-of-two and non-zero; keep behavior aligned with heap_alloc/free
    if (!heap || size == 0 || alignment == 0 || (alignment & (alignment - 1)) != 0) return NULL;

    // Strategy: over-allocate, then bump the returned pointer to an aligned address within the same block.
    // Freeing works because heap_free scans nodes and finds the owning node by range.
    size_t request = size + (alignment - 1);
    void* base = heap_alloc(heap, request);
    if (!base) {
        ERROR("aligned alloc: failed to allocate %zu bytes (req=%zu, align=%zu)", size, request, alignment);
        return NULL;
    }
    uintptr_t addr = (uintptr_t)base;
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    return (void*)aligned;
}

// Backward-compat wrapper to match existing callers (alignment, size order)
void* heap_aligned_alloc(Heap* heap, size_t alignment, size_t size) {
    return heap_alloc_aligned(heap, size, alignment);
}
