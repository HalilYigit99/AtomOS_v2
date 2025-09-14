#include <memory/pmm.h>
#include <boot/multiboot2.h>
#include <debug/debug.h>
#include <util/assert.h>
#include <efi/efi.h>
#include <memory/memory.h>
#include <list.h>
#include <graphics/screen.h>

List *memory_regions = NULL; // Bellek bölgelerinin başı

// Given by linker script
extern char __kernel_start[];
extern char __kernel_end[];
extern char __kernel_size[];

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
        return MemoryRegionType_EFI_LOADER_CODE; // Loader Code
    case 2:
        return MemoryRegionType_EFI_LOADER_DATA; // Loader Data
    case 3:
        return MemoryRegionType_EFI_BS_CODE; // Boot Services Code (ExitBootServices sonrası reclaim edilebilir)
    case 4:
        return MemoryRegionType_EFI_BS_DATA; // Boot Services Data  (ExitBootServices sonrası reclaim edilebilir)
    case 5:
        return MemoryRegionType_EFI_RT_CODE; // Runtime Code (reclaim edilmez)
    case 6:
        return MemoryRegionType_EFI_RT_DATA; // Runtime Data (reclaim edilmez)
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

    // 0x0 - kernel_end arasını reserved yap ( halihazırda usable olmayanlara dokunma )
    size_t kernel_end = (size_t)__kernel_end;

    for (ListNode* node = memory_regions->head; node != NULL; node = node->next) {
        MemoryRegion* region = (MemoryRegion*)node->data;
        if (region->base < kernel_end) {
            size_t region_end = region->base + region->size;
            if (region_end > kernel_end) {
                // Bölge kernel_end'den sonra devam ediyor, böl
                region->size = kernel_end - region->base; // Bölgeyi kernel_end'e kadar kısalt
                // Yeni bir bölge oluştur ve kalan kısmı USABLE yap
                MemoryRegion* newRegion = (MemoryRegion*)malloc(sizeof(MemoryRegion));
                newRegion->base = kernel_end;
                newRegion->size = region_end - kernel_end;
                newRegion->type = MemoryRegionType_USABLE;
                List_InsertAt(memory_regions, List_IndexOf(memory_regions, region) + 1, newRegion);
            }
            // Bölge kernel alanı ile kesişiyor, türünü RESERVED yap
            region->type = MemoryRegionType_RESERVED;
        }
    }

    // Video framebuffer reserve et

    if (main_screen.mode->framebuffer)
    {
        void* addr = (void*)(uintptr_t)main_screen.mode->framebuffer;
        size_t size = main_screen.mode->pitch * main_screen.mode->height;
        if (size % 4096 != 0)
            size = (size / 4096 + 1) * 4096; // Sayfa hizala

        // Framebuffer bölgesini listeye ekle
        // Adım 1: Framebuffer bölgesini kapsayan tüm bölgeleri bul ve gerekirse böl
        size_t fb_start = (size_t)addr;
        size_t fb_end = fb_start + size;

        for (ListNode* node = memory_regions->head; node != NULL; node = node->next) {
            MemoryRegion* region = (MemoryRegion*)node->data;
            size_t region_end = region->base + region->size;

            if (region->base < fb_end && region_end > fb_start) {
                // Kesişim var
                if (region->base < fb_start && region_end > fb_end) {
                    // Bölge framebuffer'ın ortasından geçiyor, böl
                    region->size = fb_start - region->base; // İlk kısmı kısalt
                    // Yeni bir bölge oluştur ve kalan kısmı ekle
                    MemoryRegion* newRegion = (MemoryRegion*)malloc(sizeof(MemoryRegion));
                    newRegion->base = fb_end;
                    newRegion->size = region_end - fb_end;
                    newRegion->type = region->type; // Orijinal türü koru
                    List_InsertAt(memory_regions, List_IndexOf(memory_regions, region) + 1, newRegion);
                    // Şimdi mevcut bölge framebuffer ile tam olarak kesişiyor
                    MemoryRegion* fbRegion = (MemoryRegion*)malloc(sizeof(MemoryRegion));
                    fbRegion->base = fb_start;
                    fbRegion->size = size;
                    fbRegion->type = MemoryRegionType_RESERVED;
                    List_InsertAt(memory_regions, List_IndexOf(memory_regions, region) + 1, fbRegion);
                } else if (region->base < fb_start && region_end <= fb_end) {
                    // Bölge framebuffer'ın başlangıcını kapsıyor, sonunu kısalt
                    region->size = fb_start - region->base;
                } else if (region->base >= fb_start && region_end > fb_end) {
                    // Bölge framebuffer'ın sonunu kapsıyor, başlangıcını kısalt
                    size_t overlap = region_end - fb_end;
                    region->base = fb_end;
                    region->size = overlap;
                } else {
                    // Bölge tamamen framebuffer içinde, türünü RESERVED yap
                    region->type = MemoryRegionType_RESERVED;
                }
            }
        }
    }

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
    case MemoryRegionType_EFI_RT_CODE:
        return "EFI_RT_CODE";
    case MemoryRegionType_EFI_RT_DATA:
        return "EFI_RT_DATA";
    case MemoryRegionType_ACPI_RECLAIMABLE:
        return "ACPI_RECLAIMABLE";
    case MemoryRegionType_ACPI_NVS:
        return "ACPI_NVS";
    case MemoryRegionType_BAD_MEMORY:
        return "BAD_MEMORY";
    case MemoryRegionType_PCI_RESOURCE:
        return "PCI_RESOURCE";
    case MemoryRegionType_EFI_BS_CODE:
        return "EFI_BS_CODE";
    case MemoryRegionType_EFI_BS_DATA:
        return "EFI_BS_DATA";
    case MemoryRegionType_EFI_LOADER_CODE:
        return "EFI_LOADER_CODE";
    case MemoryRegionType_EFI_LOADER_DATA:
        return "EFI_LOADER_DATA";
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

// Ard arda gelen USABLE blokları birleştir
void pmm_maintain()
{
    if (!memory_regions || List_IsEmpty(memory_regions))
        return;

    // Not: Liste adres sirasina gore sirali varsayiliyor. Sirali degilse, sadece
    // bitisik/ortusen USABLE bloklar birlesir; digerleri dokunulmaz.
    ListNode *node = memory_regions->head;
    while (node && node->next)
    {
        MemoryRegion *current = (MemoryRegion *)node->data;
        MemoryRegion *next = (MemoryRegion *)node->next->data;

        // Sadece USABLE olanlari birlestir
        if (current->type == MemoryRegionType_USABLE && next->type == MemoryRegionType_USABLE)
        {
            size_t current_end = current->base + current->size;
            size_t next_end = next->base + next->size;

            // Bitişik veya örtüşen aralık kontrolü
            // Koşul: next->base <= current_end (bitişik veya overlap)
            if (next->base <= current_end)
            {
                // Kapsayıcı aralığa genişlet
                size_t new_base = (current->base < next->base) ? current->base : next->base;
                size_t new_end = (current_end > next_end) ? current_end : next_end;

                // current'i güncelle
                current->base = new_base;
                current->size = new_end - new_base;

                // next düğümünü listeden çıkar ve verisini serbest bırak
                MemoryRegion *to_free = next;
                List_Remove(memory_regions, to_free); // sadece düğümü free eder
                free(to_free);                        // bölge verisini free et

                // Aynı current ile yeni next'i tekrar dene (node'u ilerletme)
                continue;
            }
        }

        // Birlestirme yapilmadiysa bir sonraki dugume gec
        node = node->next;
    }
}

void *pmm_alloc(size_t sizeInKB)
{
    if (!memory_regions || sizeInKB == 0)
    {
        LOG("pmm_alloc: invalid parameters (memory_regions=%p, sizeInKB=%zu)", (void *)memory_regions, sizeInKB);
        return NULL;
    }

    // KB -> bytes, overflow guard
    if (sizeInKB > (SIZE_MAX / 1024))
    {
        LOG("pmm_alloc: sizeInKB too large: %zu", sizeInKB);
        return NULL; // overflow korumasi
    }
    size_t bytes = sizeInKB * 1024;

    // 4KiB hizalama (EFI_PAGE_SIZE)
    const size_t PAGE = EFI_PAGE_SIZE; // 4096
    size_t alloc_size = (bytes + PAGE - 1) & ~(PAGE - 1);
    if (alloc_size == 0)
    {
        LOG("pmm_alloc: alloc_size overflow for sizeInKB=%zu", sizeInKB);
        return NULL; // overflow korumasi
    }

    // Birlestirme uygula ki duzgun buyuk blok bulalim
    pmm_maintain();

    size_t index = 0;
    for (ListNode *node = memory_regions->head; node; node = node->next, ++index)
    {
        MemoryRegion *cur = (MemoryRegion *)node->data;
        if (!cur || cur->type != MemoryRegionType_USABLE)
            continue;

        // Tahsis baslangic adresini 4KiB'e hizala
        size_t aligned_base = (cur->base + PAGE - 1) & ~(PAGE - 1);
        if (aligned_base < cur->base)
            continue; // overflow korumasi

        size_t gap = aligned_base - cur->base; // on kisim (USABLE kalacak)
        if (cur->size < gap + alloc_size)
            continue; // bu bolge yeterli degil

        // Durum 1: gap == 0 ve tam uyan blok -> mevcut dugumu RESERVED yap
        if (gap == 0 && cur->size == alloc_size)
        {
            void *ret = (void *)(uintptr_t)cur->base;
            cur->type = MemoryRegionType_RESERVED;
            return ret;
        }

        size_t cur_end = cur->base + cur->size;
        size_t alloc_end = aligned_base + alloc_size;

        // gap == 0 ise: mevcut dugumu ALLOCATED yap, sadece suffix icin tek malloc kullan
        if (gap == 0)
        {
            void *ret = (void *)(uintptr_t)cur->base;
            // Mevcut dugumu allocated'a cevir
            cur->type = MemoryRegionType_RESERVED;
            cur->size = alloc_size;

            // Suffix var mi?
            if (alloc_end < cur_end)
            {
                MemoryRegion *suffix = (MemoryRegion *)malloc(sizeof(MemoryRegion));
                if (!suffix)
                {
                    ERROR("pmm_alloc: suffix allocation failed for size %lu", (unsigned long)alloc_size);
                    return NULL;
                }
                suffix->base = alloc_end;
                suffix->size = cur_end - alloc_end;
                suffix->type = MemoryRegionType_USABLE;

                // allocated nodenin hemen arkasina suffix ekle
                if (!List_InsertAt(memory_regions, index + 1, suffix))
                {
                    ERROR("pmm_alloc: List_InsertAt failed while inserting suffix");
                    free(suffix);
                    return NULL;
                }
            }
            return ret;
        }

        // gap > 0 ise: cur prefix olarak kalir; ayrica alloc ve gerekirse suffix nodelari eklenir
        MemoryRegion *alloc_region = (MemoryRegion *)malloc(sizeof(MemoryRegion));
        if (!alloc_region)
        {
            ERROR("pmm_alloc: alloc_region allocation failed");
            return NULL;
        }
        alloc_region->base = aligned_base;
        alloc_region->size = alloc_size;
        alloc_region->type = MemoryRegionType_RESERVED;

        // cur'u prefixe daralt
        cur->size = gap;
        if (!List_InsertAt(memory_regions, index + 1, alloc_region))
        {
            ERROR("pmm_alloc: List_InsertAt failed while inserting alloc_region");
            free(alloc_region);
            return NULL;
        }

        // Suffix var mi?
        if (alloc_end < cur_end)
        {
            MemoryRegion *suffix = (MemoryRegion *)malloc(sizeof(MemoryRegion));
            if (!suffix)
            {
                ERROR("pmm_alloc: suffix allocation failed");
                return NULL;
            }
            suffix->base = alloc_end;
            suffix->size = cur_end - alloc_end;
            suffix->type = MemoryRegionType_USABLE;
            if (!List_InsertAt(memory_regions, index + 2, suffix))
            {
                ERROR("pmm_alloc: List_InsertAt failed while inserting suffix (gap>0)");
                free(suffix);
                return NULL;
            }
        }

        return (void *)(uintptr_t)alloc_region->base;
    }

    ERROR("pmm_alloc: no suitable block found for sizeInKB=%zu", sizeInKB);
    return NULL; // uygun blok yok
}

void pmm_free(void *ptr)
{
    if (!ptr || !memory_regions)
        return;

    size_t addr = (size_t)(uintptr_t)ptr;

    if (addr % 4096 != 0)
    {
        // Align down to page size
        LOG("pmm_free: address is not page-aligned: 0x%lX", (unsigned long)addr);
        addr = addr & ~(4096 - 1);
        LOG("pmm_free: aligned address: 0x%lX", (unsigned long)addr);
    }

    // Ilgili bolgeyi bul
    ListNode *node = memory_regions->head;
    MemoryRegion *freed = NULL;
    while (node)
    {
        MemoryRegion *r = (MemoryRegion *)node->data;
        if (r && r->base == addr)
        {
            freed = r;
            break;
        }
        node = node->next;
    }

    if (!freed)
    {
        LOG("pmm_free: adres bulunamadi: 0x%lX", (unsigned long)addr);
        return;
    }

    if (freed->type == MemoryRegionType_USABLE)
    {
        LOG("pmm_free: cift free veya zaten USABLE: 0x%lX", (unsigned long)addr);
        return;
    }

    // Serbest birak ve USABLE yap
    freed->type = MemoryRegionType_USABLE;

    // Hedefe komsu/dahil USABLE bloklarla birlestir (liste sirali olmasa bile tum listeyi tara)
    bool merged;
    do
    {
        merged = false;
        size_t freed_end = freed->base + freed->size;

        for (ListNode *n = memory_regions->head; n; n = n->next)
        {
            MemoryRegion *mr = (MemoryRegion *)n->data;
            if (!mr || mr == freed || mr->type != MemoryRegionType_USABLE)
                continue;

            size_t mr_end = mr->base + mr->size;

            // Bitişik ya da örtüşen durum
            if (!(mr->base > freed_end || mr_end < freed->base))
            {
                // kapsayici aralik
                size_t new_base = (freed->base < mr->base) ? freed->base : mr->base;
                size_t new_end = (freed_end > mr_end) ? freed_end : mr_end;
                freed->base = new_base;
                freed->size = new_end - new_base;

                // mr dugumunu listeden cikar ve verisini serbest birak
                MemoryRegion *to_free = mr;
                List_Remove(memory_regions, to_free);
                free(to_free);

                // birlesme oldu; tekrar tara
                merged = true;
                break;
            }

            // Sol bitisik: mr_end == freed->base
            if (mr_end == freed->base)
            {
                freed->base = mr->base;
                freed->size += mr->size;
                MemoryRegion *to_free = mr;
                List_Remove(memory_regions, to_free);
                free(to_free);
                merged = true;
                break;
            }

            // Sag bitisik: freed_end == mr->base
            if (freed_end == mr->base)
            {
                freed->size += mr->size;
                MemoryRegion *to_free = mr;
                List_Remove(memory_regions, to_free);
                free(to_free);
                merged = true;
                break;
            }
        }

    } while (merged);
}
