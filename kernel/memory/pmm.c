#include <memory/pmm.h>
#include <boot/multiboot2.h>
#include <efi/efi.h>
#include <debug/debug.h>
#include <memory/heap.h>
#include <machine/machine.h>

uint32_t* pmm_bitmap;
size_t pmm_bitmap_size;

extern void* __kernel_end;

void pmm_print_status();

void pmm_make_reserved(size_t addr) {

    size_t alignedAddr = (addr >> 12) << 12; // 4KB hizalama
    size_t bitId = alignedAddr / 4096; // 4KB sayfa
    size_t bitIndex = bitId / 32;
    size_t bitOffset = bitId % 32;

    uint32_t* bit = &pmm_bitmap[bitIndex];

    *bit |= (1u << bitOffset);

}

void pmm_make_usable(size_t addr) {

    size_t alignedAddr = (addr >> 12) << 12; // Align to 4KB
    size_t bitId = alignedAddr / 4096; // 4KB pages
    size_t bitIndex = bitId / 32;
    size_t bitOffset = bitId % 32;

    uint32_t* bit = &pmm_bitmap[bitIndex];

    *bit &= ~(1 << bitOffset);

}

bool pmm_is_usable(size_t addr) {
    size_t alignedAddr = (addr >> 12) << 12; // Align to 4KB
    size_t bitId = alignedAddr / 4096; // 4KB pages
    size_t bitIndex = bitId / 32;
    size_t bitOffset = bitId % 32;

    uint32_t* bit = &pmm_bitmap[bitIndex];
    uint32_t state = (1u << bitOffset);

    return ((*bit & state) == 0u);
}

void pmm_make_reserved_region(size_t addr_1, size_t addr_2) {
    addr_1 = (addr_1 >> 12) << 12;
    addr_2 = (addr_2 >> 12) << 12;

    if (addr_2 < addr_1){
        uint32_t h = addr_1;
        uint32_t l = addr_2;
        addr_1 = l;
        addr_2 = h;
    }

    while (addr_1 <= addr_2) {
        pmm_make_reserved(addr_1);
        addr_1 += 4096;
    }

}

void pmm_make_usable_region(size_t addr_1, size_t addr_2) {
    addr_1 = (addr_1 >> 12) << 12;
    addr_2 = (addr_2 >> 12) << 12;

    if (addr_2 < addr_1){
        uint32_t h = addr_1;
        uint32_t l = addr_2;
        addr_1 = l;
        addr_2 = h;
    }

    while (addr_1 <= addr_2) {
        pmm_make_usable(addr_1);
        addr_1 += 4096;
    }

}

void pmm_init()
{

    // Firstly allocate bitmap

    // Her 4KB sayfa için 1 bit: RAM_KB / 4 (sayfa sayısı), /8 (bit->byte) = RAM_KB / 32
    size_t bitmap_size = machine_ramSizeInKB / 32;

    pmm_bitmap = (uint32_t*)heap_alloc(&kernel_heap, bitmap_size);

    if (!pmm_bitmap) {
        ERROR("PMM bitmap alloc failed!");
        asm volatile ("cli; hlt");
    }

    pmm_bitmap_size = bitmap_size;

    uint32_t memoryRegionCount = 0;
    struct multiboot_mmap_entry* regions = multiboot2_get_memory_map(&memoryRegionCount);

    if (!memoryRegionCount) {
        ERROR("memoryRegionCount is zero!");
        asm volatile ("cli; hlt");
    }

    for (size_t i = 0; i < memoryRegionCount; i++)
    {
        struct multiboot_mmap_entry* region = &(regions[i]);

        if (region->type == MULTIBOOT_MEMORY_AVAILABLE) {
            pmm_make_usable_region((size_t)region->addr, (size_t)((size_t)region->addr + region->len));
        }else {
            pmm_make_reserved_region((size_t)region->addr, (size_t)((size_t)region->addr + region->len));
        }

    }

    // Make kernel reserved

    pmm_make_reserved_region(0x100000, (size_t)&__kernel_end);

    // Make kernel heap reserved

    pmm_make_reserved_region((size_t)kernel_heap.firstRegion->firstNode, (size_t)kernel_heap.firstRegion->firstNode + kernel_heap.firstRegion->size);

    // Print status
    pmm_print_status();


}

void pmm_print_status()
{
    const size_t PAGE_SIZE = 4096;

    if (!pmm_bitmap || pmm_bitmap_size == 0) {
        LOG("PMM: bitmap init edilmemiş veya boyut 0");
        return;
    }

    size_t start = 0;
    size_t end = 0;

    // İlk bitin durumuna göre başlangıç durumu (0=Free, 1=Reserved)
    bool is_free = ((pmm_bitmap[0] & 1u) == 0u);

    // Önce tam uint32_t kelimeleri tara
    const size_t words = pmm_bitmap_size / sizeof(uint32_t);
    for (size_t wi = 0; wi < words; ++wi) {
        uint32_t v = pmm_bitmap[wi];

        // Hız için: tamamen Free (0x00000000) ya da tamamen Reserved (0xFFFFFFFF) kontrolü
        if (v == 0x00000000u || v == 0xFFFFFFFFu) {
            bool word_free = (v == 0x00000000u);
            if (word_free == is_free) {
                // Mevcut aralığı uzat
                end += 32 * PAGE_SIZE;
            } else {
                // Kelime sınırında durum değişimi
                LOG("PMM: [%p - %p) %s", (void*)start, (void*)end, is_free ? "Free" : "Reserved");
                start = end;
                is_free = word_free;
                end += 32 * PAGE_SIZE;
            }
            continue;
        }

        // Karışık kelime: tek tek bit tara
        for (unsigned bi = 0; bi < 32; ++bi) {
            bool bit_free = ((v & (1u << bi)) == 0u);
            if (bit_free != is_free) {
                // Önceki aralığı kapat ve yaz
                LOG("PMM: [%p - %p) %s", (void*)start, (void*)end, is_free ? "Free" : "Reserved");
                start = end;
                is_free = bit_free;
            }
            end += PAGE_SIZE;
        }
    }

    // Kalan tam olmayan baytları tara (eğer varsa)
    size_t consumed_bytes = words * sizeof(uint32_t);
    size_t rem_bytes = pmm_bitmap_size - consumed_bytes;
    if (rem_bytes) {
        const uint8_t* bytes = (const uint8_t*)((const uint8_t*)pmm_bitmap + consumed_bytes);
        for (size_t bj = 0; bj < rem_bytes; ++bj) {
            uint8_t b = bytes[bj];
            if (b == 0x00u || b == 0xFFu) {
                bool byte_free = (b == 0x00u);
                if (byte_free == is_free) {
                    end += 8 * PAGE_SIZE;
                } else {
                    LOG("PMM: [%p - %p) %s", (void*)start, (void*)end, is_free ? "Free" : "Reserved");
                    start = end;
                    is_free = byte_free;
                    end += 8 * PAGE_SIZE;
                }
                continue;
            }

            for (unsigned bi = 0; bi < 8; ++bi) {
                bool bit_free = ((b & (1u << bi)) == 0u);
                if (bit_free != is_free) {
                    LOG("PMM: [%p - %p) %s", (void*)start, (void*)end, is_free ? "Free" : "Reserved");
                    start = end;
                    is_free = bit_free;
                }
                end += PAGE_SIZE;
            }
        }
    }

    // Son aralığı yaz
    LOG("PMM: [%p - %p) %s", (void*)start, (void*)end, is_free ? "Free" : "Reserved");

}