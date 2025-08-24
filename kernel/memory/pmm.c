#include <memory/pmm.h>
#include <boot/multiboot2.h>
#include <util/assert.h>
#include <efi/efi.h>
#include <memory/memory.h>
#include <list.h>

List* memory_regions = NULL; // Bellek bölgelerinin başı

void efi_mr_init(void);
void bios_mr_init(void);

MemoryRegionType mb2_mrType_to_mrType(uint32_t mb2_type) {
    switch (mb2_type) {
        case 1: return MemoryRegionType_USABLE;
        case 2: return MemoryRegionType_RESERVED;
        case 3: return MemoryRegionType_ACPI_RECLAIMABLE;
        case 4: return MemoryRegionType_ACPI_NVS;
        case 5: return MemoryRegionType_BAD_MEMORY;
        default: return MemoryRegionType_UNKNOWN; // Bilinmeyen türler için döndür
    }
}

MemoryRegionType mb2_efiType_to_mrType(uint32_t mb2_type) {
    switch (mb2_type) {
        case 1: return MemoryRegionType_USABLE;
        case 2: return MemoryRegionType_RESERVED;
        case 3: return MemoryRegionType_PCI_RESOURCE;
        case 7: return MemoryRegionType_EFI_CODE;
        case 8: return MemoryRegionType_EFI_DATA;
        case 9: return MemoryRegionType_ACPI_RECLAIMABLE;
        case 10: return MemoryRegionType_ACPI_NVS;
        case 11: return MemoryRegionType_BAD_MEMORY;
        default: return MemoryRegionType_UNKNOWN; // Bilinmeyen türler için döndür
    }
}

void pmm_init(void) {
    // Fiziksel bellek yönetimi başlatma kodu buraya gelecek

    memory_regions = List_Create(); // Bellek bölgeleri listesini oluştur

    if (mb2_is_efi_boot)
    {
        efi_mr_init();
    }else
    {
        bios_mr_init();
    }

}

void bios_mr_init(void) {
    // BIOS tabanlı bellek bölgesi başlatma kodu buraya gelecek

    ASSERT(mb2_mmap != NULL, "Multiboot2 memory map is NULL in BIOS mode");

    size_t entry_count = (mb2_mmap->size - sizeof(struct multiboot_tag_mmap)) / mb2_mmap->entry_size;

    for (size_t i = 0; i < entry_count; i++) {
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)((uintptr_t)mb2_mmap->entries + i * mb2_mmap->entry_size);

        // Bellek bölgesi bilgilerini listeye ekle
        MemoryRegion* newRegion = (MemoryRegion*)malloc(sizeof(MemoryRegion));
        newRegion->base = entry->addr;
        newRegion->size = entry->len;
        newRegion->type = mb2_mrType_to_mrType(entry->type);

        List_Add(memory_regions, newRegion);
    }

}

void efi_mr_init(void) {
    // EFI tabanlı bellek bölgesi başlatma kodu buraya gelecek

    if (mb2_mmap) {
        bios_mr_init();
        return;
    }

    if (mb2_efi_mmap)
    {
        size_t entry_count = mb2_efi_mmap->size - sizeof(struct multiboot_tag_efi_mmap);
        entry_count /= mb2_efi_mmap->descr_size;

        for (size_t i = 0; i < entry_count; i++) {
            EFI_MEMORY_DESCRIPTOR *entry = (EFI_MEMORY_DESCRIPTOR *)((uintptr_t)mb2_efi_mmap->efi_mmap + i * mb2_efi_mmap->descr_size);

            // Bellek bölgesi bilgilerini listeye ekle
            MemoryRegion* newRegion = (MemoryRegion*)malloc(sizeof(MemoryRegion));
            newRegion->base = entry->physical_start;
            newRegion->size = entry->number_of_pages * 4096; // EFI sayfa boyutu genellikle 4KB'dir
            newRegion->type = mb2_efiType_to_mrType(entry->type);

            List_Add(memory_regions, newRegion);
        }
    }else {

        // Call GetMemoryMap from EFI System Table
        ASSERT(efi_system_table != NULL, "EFI System Table is NULL in EFI mode and no mmap provided by Multiboot2");

    }

}
