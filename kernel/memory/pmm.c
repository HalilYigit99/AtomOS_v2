#include <memory/pmm.h>
#include <boot/multiboot2.h>
#include <efi/efi.h>
#include <debug/debug.h>
#include <memory/heap.h>
#include <machine/machine.h>
#include <memory/memory.h>

uint32_t* pmm_bitmap;
// pmm_bitmap_size: bitmap'in byte cinsinden boyutu
size_t pmm_bitmap_size;
// pmm_bitmap_words: 32-bit word sayısı (pmm_bitmap_size / 4, yukarı yuvarlanmış)
size_t pmm_bitmap_words;

extern void* __kernel_end;

void pmm_print_status();

static inline size_t align_down(size_t x, size_t a) { return x & ~(a - 1); }
static inline size_t align_up(size_t x, size_t a) { return (x + a - 1) & ~(a - 1); }

static inline void pmm_set_bit_safe(size_t bitId) {
    size_t maxBits = pmm_bitmap_words * 32u;
    if (bitId >= maxBits) return;
    size_t bitIndex = bitId / 32u;
    size_t bitOffset = bitId % 32u;
    pmm_bitmap[bitIndex] |= (uint32_t)(1u << bitOffset);
}

static inline void pmm_clear_bit_safe(size_t bitId) {
    size_t maxBits = pmm_bitmap_words * 32u;
    if (bitId >= maxBits) return;
    size_t bitIndex = bitId / 32u;
    size_t bitOffset = bitId % 32u;
    pmm_bitmap[bitIndex] &= (uint32_t)~(1u << bitOffset);
}

void pmm_make_reserved(size_t addr) {
    // 4KB hizalama
    size_t alignedAddr = align_down(addr, 4096);
    size_t bitId = alignedAddr / 4096u; // 4KB sayfa
    pmm_set_bit_safe(bitId);
}

void pmm_make_usable(size_t addr) {
    size_t alignedAddr = align_down(addr, 4096); // Align to 4KB
    size_t bitId = alignedAddr / 4096u; // 4KB pages
    pmm_clear_bit_safe(bitId);
}

bool pmm_is_usable(size_t addr) {
    size_t alignedAddr = align_down(addr, 4096); // Align to 4KB
    size_t bitId = alignedAddr / 4096u; // 4KB pages
    size_t maxBits = pmm_bitmap_words * 32u;
    if (bitId >= maxBits) return false; // bitmap dışında olanı kullanılmaz say
    size_t bitIndex = bitId / 32u;
    size_t bitOffset = bitId % 32u;
    uint32_t bitmask = (1u << bitOffset);
    return ((pmm_bitmap[bitIndex] & bitmask) == 0u);
}

void pmm_make_reserved_region(size_t addr_1, size_t addr_2) {

    if (addr_1 == addr_2) return;

    // [addr_1, addr_2) olacak şekilde hizala
    if (addr_2 < addr_1) {
        size_t tmp = addr_1; addr_1 = addr_2; addr_2 = tmp;
    }
    addr_1 = align_down(addr_1, 4096);
    addr_2 = align_up(addr_2, 4096);

    for (size_t a = addr_1; a < addr_2; a += 4096u) {
        pmm_make_reserved(a);
    }

    // Kontrol: Bu aralık gerçekten reserved mı?
    for (size_t a = addr_1; a < addr_2; a += 4096u) {
        if (pmm_is_usable(a) == true) {
            ERROR("pmm_make_reserved_region: %p is not reserved after setting it!", (void*)a);
            asm volatile ("cli; hlt");
        }
    }

}

void pmm_make_usable_region(size_t addr_1, size_t addr_2) {

    if (addr_1 == addr_2) return;

    // [addr_1, addr_2) olacak şekilde hizala
    if (addr_2 < addr_1) {
        size_t tmp = addr_1; addr_1 = addr_2; addr_2 = tmp;
    }
    addr_1 = align_down(addr_1, 4096);
    addr_2 = align_up(addr_2, 4096);

    for (size_t a = addr_1; a < addr_2; a += 4096u) {
        pmm_make_usable(a);
    }

    // Kontrol: Bu aralık gerçekten usable mı?
    for (size_t a = addr_1; a < addr_2; a += 4096u) {
        if (pmm_is_usable(a) == false) {
            ERROR("pmm_make_usable_region: %p is not usable after setting it!", (void*)a);
            asm volatile ("cli; hlt");
        }
    }

}

void pmm_init()
{

    // Firstly allocate bitmap

    // Her 4KB sayfa için 1 bit:
    // sayfa_sayısı = RAM_KB / 4; bit = sayfa_sayısı; byte = ceil(bit/8)
    size_t page_count = (size_t)machine_ramSizeInKB / 4u;
    size_t bitmap_bytes_needed = (page_count + 7u) / 8u; // yuvarla
    pmm_bitmap_words = (bitmap_bytes_needed + 3u) / 4u;        // 32-bit word'a yuvarla
    pmm_bitmap_size = pmm_bitmap_words * sizeof(uint32_t);     // gerçek byte boyutu

    if (pmm_bitmap_size == 0){
        ERROR("PMM bitmap size zero!");
        asm volatile ("cli; hlt");
    }

    pmm_bitmap = (uint32_t*)heap_alloc(&kernel_heap, pmm_bitmap_size);

    if (!pmm_bitmap) {
        ERROR("PMM bitmap alloc failed!");
        asm volatile ("cli; hlt");
    }

    // Varsayılan: tamamı reserved (0xFF). Memory map ile Free yapılacak.
    memset((uint32_t*)pmm_bitmap, 0, pmm_bitmap_size);

    LOG("pmm_bitmap_words: %zu", pmm_bitmap_words);

    uint32_t memoryRegionCount = 0;
    struct multiboot_mmap_entry* regions = multiboot2_get_memory_map(&memoryRegionCount);

    if (!memoryRegionCount) {
        ERROR("memoryRegionCount is zero!");
        asm volatile ("cli; hlt");
    }

    for (size_t i = 0; i < memoryRegionCount; i++)
    {
        struct multiboot_mmap_entry* region = &(regions[i]);

        size_t r_start = (size_t)region->addr;
        size_t r_end = (size_t)((size_t)region->addr + (size_t)region->len);
        if (region->type == MULTIBOOT_MEMORY_AVAILABLE) {
            pmm_make_usable_region(r_start, r_end);
        } else {
            // Zaten 0xFF ile reserved; yine de belirginlik için işaretleyebiliriz
            pmm_make_reserved_region(r_start, r_end);
        }

    }

    // Make kernel reserved

    pmm_make_reserved_region(0x100000, (size_t)&__kernel_end);

    // Make kernel heap reserved

    pmm_make_reserved_region((size_t)kernel_heap.firstRegion->firstNode, (size_t)kernel_heap.firstRegion->firstNode + kernel_heap.firstRegion->size);

    LOG("pmm_bitmap_size: %zu bytes (%zu KB)", pmm_bitmap_size, pmm_bitmap_size / 1024u);

    // Print status
    pmm_print_status();


}

void __attribute__((optimize("O0"))) pmm_print_status()
{
    size_t PAGE_SIZE = 4096;

    if (!pmm_bitmap || pmm_bitmap_size == 0) {
        ERROR("PMM: bitmap init edilmemiş veya boyut 0");
        return;
    }

    size_t lastAddr = 0;
    size_t pmm_memory_size = pmm_bitmap_words * 32u * PAGE_SIZE; // Toplam bellek boyutu

    bool state = pmm_is_usable(lastAddr * PAGE_SIZE);

    LOG("PMM memory size: %zu bytes (%zu KB, %zu MB )", pmm_memory_size, pmm_memory_size / 1024u, pmm_memory_size / (1024u * 1024u));

    for (size_t addr = 0; addr < pmm_memory_size; addr += PAGE_SIZE) {
        bool cur = pmm_is_usable(addr);
        if (cur != state) {
            // lastAddr .. addr  aralığı previous state idi
            LOG("PMM: [%p - %p) is %s", (void*)(uintptr_t)lastAddr, (void*)(uintptr_t)addr, state ? "Usable" : "Reserved");
            state = cur;
            lastAddr = addr;
        }
    }

    // Son kalan aralığı yazdır
    if (lastAddr < pmm_memory_size) {
        LOG("PMM: [%p - %p) is %s", (void*)(uintptr_t)lastAddr, (void*)(uintptr_t)pmm_memory_size, state ? "Usable" : "Reserved");
    }
}
