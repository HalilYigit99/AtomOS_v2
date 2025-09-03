#include <memory/pmm.h>
#include <boot/multiboot2.h>
#include <debug/debug.h>
#include <util/assert.h>
#include <efi/efi.h>
#include <memory/memory.h>
#include <list.h>

List *memory_regions = NULL; // Bellek bölgelerinin başı

void efi_mr_init(void);
void bios_mr_init(void);
void print_memory_regions();

UINTN bs_map_key;
UINTN bs_mr_memory_map_size = 0; // İlk çağrıda 0
EFI_MEMORY_DESCRIPTOR *bs_mr_memory_map = NULL;
UINTN bs_mr_descriptor_size = 0;
uint32_t bs_mr_descriptor_version = 0;

MemoryRegionType mb2_mrType_to_mrType(uint32_t mb2_type)
{
    switch (mb2_type)
    {
    case 1:
        return MemoryRegionType_USABLE;
    case 2:
        return MemoryRegionType_RESERVED;
    case 3:
        return MemoryRegionType_ACPI_RECLAIMABLE;
    case 4:
        return MemoryRegionType_ACPI_NVS;
    case 5:
        return MemoryRegionType_BAD_MEMORY;
    default:
        return MemoryRegionType_UNKNOWN;
    }
}

MemoryRegionType efiType_to_mrType(uint32_t efi_type)
{
    switch (efi_type)
    {
    // UEFI Specification (MemoryType):
    //  0: EfiReservedMemoryType
    //  1: EfiLoaderCode
    //  2: EfiLoaderData
    //  3: EfiBootServicesCode
    //  4: EfiBootServicesData
    //  5: EfiRuntimeServicesCode
    //  6: EfiRuntimeServicesData
    //  7: EfiConventionalMemory (OS tarafından kullanılabilir)
    //  8: EfiUnusableMemory
    //  9: EfiACPIReclaimMemory
    // 10: EfiACPIMemoryNVS
    // 11: EfiMemoryMappedIO
    // 12: EfiMemoryMappedIOPortSpace
    // 13: EfiPalCode
    // 14: EfiPersistentMemory
    case 0:
        return MemoryRegionType_RESERVED;
    case 1:
        return MemoryRegionType_EFI_CODE; // Loader Code
    case 2:
        return MemoryRegionType_EFI_DATA; // Loader Data
    case 3:
        return MemoryRegionType_EFI_CODE; // Boot Services Code (ExitBootServices sonrası reclaim edilebilir)
    case 4:
        return MemoryRegionType_EFI_DATA; // Boot Services Data  (ExitBootServices sonrası reclaim edilebilir)
    case 5:
        return MemoryRegionType_RESERVED; // Runtime Code (reclaim edilmez)
    case 6:
        return MemoryRegionType_RESERVED; // Runtime Data (reclaim edilmez)
    case 7:
        return MemoryRegionType_USABLE; // Conventional Memory
    case 8:
        return MemoryRegionType_BAD_MEMORY; // Unusable
    case 9:
        return MemoryRegionType_ACPI_RECLAIMABLE;
    case 10:
        return MemoryRegionType_ACPI_NVS;
    case 11:
        return MemoryRegionType_PCI_RESOURCE; // MMIO
    case 12:
        return MemoryRegionType_PCI_RESOURCE; // MMIO Port Space
    case 13:
        return MemoryRegionType_RESERVED; // PAL Code (Itanium, pratikte yok)
    case 14:
        return MemoryRegionType_RESERVED; // Persistent/NVDIMM, özel işleme yoksa RESERVED
    default:
        return MemoryRegionType_UNKNOWN;
    }
}

void pmm_init(void)
{
    // Fiziksel bellek yönetimi başlatma kodu buraya gelecek

    memory_regions = List_Create(); // Bellek bölgeleri listesini oluştur

    if (mb2_is_efi_boot)
    {
        efi_mr_init();
    }
    else
    {
        bios_mr_init();
    }

    print_memory_regions();
}

void bios_mr_init(void)
{
    // BIOS tabanlı bellek bölgesi başlatma kodu buraya gelecek

    if (memory_regions == NULL)
    {
        memory_regions = List_Create(); // Bellek bölgeleri listesini oluştur
    }

    List_Clear(memory_regions, true); // Önceki listeyi temizle ve elemanları serbest bırak

    ASSERT(mb2_mmap != NULL, "Multiboot2 memory map is NULL in BIOS mode");

    size_t entry_count = (mb2_mmap->size - sizeof(struct multiboot_tag_mmap)) / mb2_mmap->entry_size;

    for (size_t i = 0; i < entry_count; i++)
    {
        struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)((uintptr_t)mb2_mmap->entries + i * mb2_mmap->entry_size);

        // Bellek bölgesi bilgilerini listeye ekle
        MemoryRegion *newRegion = (MemoryRegion *)malloc(sizeof(MemoryRegion));
        newRegion->base = entry->addr;
        newRegion->size = entry->len;
        newRegion->type = mb2_mrType_to_mrType(entry->type);

        List_Add(memory_regions, newRegion);
    }
}

void efi_mr_init(void)
{
    // EFI tabanlı bellek bölgesi başlatma kodu buraya gelecek

    if (mb2_mmap)
    {
        bios_mr_init();
        return;
    }

    // Call GetMemoryMap from EFI System Table
    ASSERT(efi_system_table != NULL, "EFI System Table is NULL in EFI mode and no mmap provided by Multiboot2");

    // EFI Boot Services kısayolu
    EFI_BOOT_SERVICES *bs = efi_system_table->boot_services;

    EFI_STATUS status;

    // 1) Boyutu öğrenmek için NULL buffer ile çağır
    status = bs->get_memory_map(
        &bs_mr_memory_map_size,   // Çıkış: ihtiyaç duyulan byte sayısı
        bs_mr_memory_map,         // NULL (ilk prob çağrısı)
        &bs_map_key,              // Çıkış: map_key (ExitBootServices için)
        &bs_mr_descriptor_size,   // Çıkış: descriptor başına byte
        &bs_mr_descriptor_version // Çıkış: descriptor versiyonu
    );

    // Spes'e uygun olarak EFI_BUFFER_TOO_SMALL beklenir
    ASSERT(status == EFI_BUFFER_TOO_SMALL,
           "EFI GetMemoryMap first probe should return EFI_BUFFER_TOO_SMALL");
    ASSERT(bs_mr_memory_map_size > 0 && bs_mr_descriptor_size > 0,
           "EFI GetMemoryMap returned invalid sizes");

    // Bundan sonra 2. çağrıyı gerçek veri için yapacaksınız:
    // status = bs->get_memory_map(&memory_map_size, memory_map, &map_key, &descriptor_size, &descriptor_version);
    // if (status == EFI_BUFFER_TOO_SMALL) { yeniden allocate_pool + tekrar dene... }

    bs_mr_memory_map = (EFI_MEMORY_DESCRIPTOR *)malloc(bs_mr_memory_map_size);

    ASSERT(bs_mr_memory_map != NULL, "Failed to allocate memory for EFI memory map");

    // 2) Gerçek çağrı

    status = bs->get_memory_map(
        &bs_mr_memory_map_size,   // Giriş/Çıkış: buffer boyutu
        bs_mr_memory_map,         // Çıkış: memory map buffer
        &bs_map_key,              // Çıkış: map_key (ExitBootServices için)
        &bs_mr_descriptor_size,   // Çıkış: descriptor başına byte
        &bs_mr_descriptor_version // Çıkış: descriptor versiyonu
    );

    ASSERT(status == EFI_SUCCESS, "EFI GetMemoryMap second call failed");
    ASSERT(bs_mr_memory_map_size > 0 && bs_mr_descriptor_size > 0,
           "EFI GetMemoryMap returned invalid sizes");

    LOG("EFI Memory Map obtained: size=%lu, descriptor_size=%lu, version=%u",
        bs_mr_memory_map_size, bs_mr_descriptor_size, bs_mr_descriptor_version);

    for (UINTN offset = 0; offset < bs_mr_memory_map_size; offset += bs_mr_descriptor_size)
    {
        EFI_MEMORY_DESCRIPTOR *entry = (EFI_MEMORY_DESCRIPTOR *)((uintptr_t)bs_mr_memory_map + offset);

        // Bellek bölgesi bilgilerini listeye ekle
        MemoryRegion *newRegion = (MemoryRegion *)malloc(sizeof(MemoryRegion));
        newRegion->base = entry->physical_start;
        newRegion->size = entry->number_of_pages * 4096; // EFI sayfa boyutu genellikle 4KB'dir
        newRegion->type = efiType_to_mrType(entry->type);
        List_Add(memory_regions, newRegion);
    }
}

char *mrTypeToString(MemoryRegionType type)
{
    switch (type)
    {
    case MemoryRegionType_UNKNOWN:
        return "UNKNOWN";
    case MemoryRegionType_NULL:
        return "NULL";
    case MemoryRegionType_USABLE:
        return "USABLE";
    case MemoryRegionType_RESERVED:
        return "RESERVED";
    case MemoryRegionType_EFI_CODE:
        return "EFI_CODE";
    case MemoryRegionType_EFI_DATA:
        return "EFI_DATA";
    case MemoryRegionType_ACPI_RECLAIMABLE:
        return "ACPI_RECLAIMABLE";
    case MemoryRegionType_ACPI_NVS:
        return "ACPI_NVS";
    case MemoryRegionType_BAD_MEMORY:
        return "BAD_MEMORY";
    case MemoryRegionType_PCI_RESOURCE:
        return "PCI_RESOURCE";
    default:
        return "INVALID_TYPE";
    }
}

void print_memory_regions(void)
{
    if (memory_regions == NULL)
    {
        LOG("Memory regions list is NULL");
        return;
    }

    if (List_Size(memory_regions) == 0)
    {
        LOG("No memory regions available");
        return;
    }

    LOG("Memory Regions:");
    for (ListNode *node = memory_regions->head; node; node = node->next)
    {
        MemoryRegion *region = (MemoryRegion *)node->data;
        if (region)
        {
            LOG("Base: 0x%016lX, Size: 0x%016lX, Type: %s", region->base, region->size, mrTypeToString(region->type));
        }
    }
}
