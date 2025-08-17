#include <efi/efi_memory.h>
#include <efi/efi.h>
#include <debug/debug.h>
#include <util/string.h>

// EFI Memory Map buffer - statik olarak ayıralım
static uint8_t efi_memory_map_buffer[16384]; // 16KB buffer
static EFI_MEMORY_DESCRIPTOR* efi_memory_descriptors = NULL;
static uint32_t efi_memory_map_size = 0;
uint32_t efi_descriptor_size = 0;
static uint32_t efi_descriptor_version = 0;
uint32_t efi_memory_map_key = 0;
uint32_t efi_memory_descriptor_count = 0;

// EFI GetMemoryMap function pointer typedef (respect UEFI calling convention)
typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    uint64_t* MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR* MemoryMap,
    uint64_t* MapKey,
    uint64_t* DescriptorSize,
    uint32_t* DescriptorVersion
);

bool efi_get_manual_memory_map(void) {
    if (!efi_system_table) {
        ERROR("EFI System Table not available");
        return false;
    }
    
    LOG("EFI System Table at: %p", efi_system_table);
    
    // EFI System Table signature kontrolü
    if (efi_system_table->hdr.signature != EFI_SYSTEM_TABLE_SIGNATURE) {
        ERROR("Invalid EFI System Table signature: 0x%016llX", efi_system_table->hdr.signature);
        return false;
    }
    
    LOG("EFI System Table signature OK, revision: 0x%08X", efi_system_table->hdr.revision);
    
    // Boot Services pointer kontrolü
    if (!efi_system_table->boot_services) {
        ERROR("EFI Boot Services not available");
        return false;
    }
    
    LOG("EFI Boot Services at: %p", efi_system_table->boot_services);
    
    // Boot Services signature kontrolü
    if (efi_system_table->boot_services->hdr.signature != EFI_BOOT_SERVICES_SIGNATURE) {
        ERROR("Invalid EFI Boot Services signature: 0x%016llX", 
              efi_system_table->boot_services->hdr.signature);
        return false;
    }
    
    LOG("EFI Boot Services signature OK");
    
    // GetMemoryMap function pointer'ını al (tiplenmiş alandan)
    EFI_GET_MEMORY_MAP get_memory_map = (EFI_GET_MEMORY_MAP)efi_system_table->boot_services->get_memory_map;
    
    LOG("GetMemoryMap function at: %p", get_memory_map);
    
    // Memory map parametrelerini hazırla
    uint64_t memory_map_size = 0; // probe ile başlayacağız
    uint64_t map_key = 0;
    uint64_t descriptor_size = 0;
    uint32_t descriptor_version = 0;
    
    LOG("Calling GetMemoryMap...");
    
    // İlk çağrı: buffer NULL, beklenen sonuç EFI_BUFFER_TOO_SMALL
    EFI_STATUS status = get_memory_map(
        &memory_map_size,
        NULL,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    if (status != EFI_BUFFER_TOO_SMALL && status != EFI_SUCCESS) {
        ERROR("GetMemoryMap(probe) failed: 0x%016llX", status);
        return false;
    }

    if (memory_map_size == 0) {
        // Bazı firmware'ler 0 dönebilir; statik buffer'ı kullan
        memory_map_size = sizeof(efi_memory_map_buffer);
    }

    if (memory_map_size > sizeof(efi_memory_map_buffer)) {
        WARN("Memory map requires %llu bytes, clamping to %u", memory_map_size, (unsigned)sizeof(efi_memory_map_buffer));
        memory_map_size = sizeof(efi_memory_map_buffer);
    }

    // İkinci çağrı: gerçek buffer ile
    status = get_memory_map(
        &memory_map_size,
        (EFI_MEMORY_DESCRIPTOR*)efi_memory_map_buffer,
        &map_key,
        &descriptor_size,
        &descriptor_version
    );

    LOG("GetMemoryMap ret=0x%016llX size=%llu desc=%llu ver=%u key=%llu",
        status, memory_map_size, descriptor_size, descriptor_version, map_key);

    if (EFI_ERROR(status)) {
        ERROR("GetMemoryMap failed: 0x%016llX", status);
        return false;
    }
    
    // Sonuçları global değişkenlere kaydet
    efi_memory_descriptors = (EFI_MEMORY_DESCRIPTOR*)efi_memory_map_buffer;
    efi_memory_map_size = (uint32_t)memory_map_size;
    efi_descriptor_size = (uint32_t)descriptor_size;
    efi_descriptor_version = descriptor_version;
    efi_memory_map_key = (uint32_t)map_key;
    if (efi_descriptor_size == 0) {
        ERROR("Descriptor size is zero");
        return false;
    }
    efi_memory_descriptor_count = efi_memory_map_size / efi_descriptor_size;
    
    LOG("Successfully obtained EFI memory map:");
    LOG("  Total descriptors: %u", efi_memory_descriptor_count);
    LOG("  Descriptor size: %u bytes", efi_descriptor_size);
    
    // Memory map'i parse et ve logla
    uint8_t* desc_ptr = (uint8_t*)efi_memory_descriptors;
    
    for (uint32_t i = 0; i < efi_memory_descriptor_count; i++) {
        // EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)desc_ptr;
        
        // uint64_t start_addr = desc->physical_start;
        // uint64_t size_bytes = desc->number_of_pages * 4096; // EFI page = 4KB
        // uint64_t end_addr = start_addr + size_bytes - 1;
        
        // const char* type_str = efi_memory_type_to_string(desc->type);
        
        // LOG("  [%2u] 0x%016llX-0x%016llX (%8llu KB) %s", 
        //     i, start_addr, end_addr, size_bytes / 1024, type_str);
        
        desc_ptr += efi_descriptor_size;
    }
    
    return true;
}

// EFI memory type'ları string'e çevir
const char* efi_memory_type_to_string(EFI_MEMORY_TYPE type) {
    switch (type) {
        case EfiReservedMemoryType: return "Reserved";
        case EfiLoaderCode: return "LoaderCode";
        case EfiLoaderData: return "LoaderData";
        case EfiBootServicesCode: return "BootServicesCode";
        case EfiBootServicesData: return "BootServicesData";
        case EfiRuntimeServicesCode: return "RuntimeServicesCode";
        case EfiRuntimeServicesData: return "RuntimeServicesData";
        case EfiConventionalMemory: return "Conventional";
        case EfiUnusableMemory: return "Unusable";
        case EfiACPIReclaimMemory: return "ACPIReclaim";
        case EfiACPIMemoryNVS: return "ACPINVS";
        case EfiMemoryMappedIO: return "MMIO";
        case EfiMemoryMappedIOPortSpace: return "MMIOPortSpace";
        case EfiPalCode: return "PalCode";
        case EfiPersistentMemory: return "Persistent";
        default: return "Unknown";
    }
}

extern struct multiboot_mmap_entry multiboot_mmap_entries[];

// Multiboot-compatible memory map oluştur
struct multiboot_mmap_entry* efi_create_multiboot_memory_map(uint32_t* entry_count) {
    if (!efi_memory_descriptors || !entry_count) {
        if (entry_count) *entry_count = 0;
        return NULL;
    }
    
    struct multiboot_mmap_entry* mb_entries = multiboot_mmap_entries;
    uint32_t mb_count = 0;
    
    uint8_t* desc_ptr = (uint8_t*)efi_memory_descriptors;
    
    for (uint32_t i = 0; i < efi_memory_descriptor_count && mb_count < 256; i++) {
        EFI_MEMORY_DESCRIPTOR* desc = (EFI_MEMORY_DESCRIPTOR*)desc_ptr;
        
        uint64_t start_addr = desc->physical_start;
        uint64_t size_bytes = desc->number_of_pages * 4096;
        
        // EFI type'ını Multiboot type'ına çevir
        uint32_t mb_type;
        switch (desc->type) {
            case EfiConventionalMemory:
            case EfiBootServicesCode:
            case EfiBootServicesData:
            case EfiLoaderCode:
            case EfiLoaderData:
                mb_type = MULTIBOOT_MEMORY_AVAILABLE;
                break;
            case EfiACPIReclaimMemory:
                mb_type = MULTIBOOT_MEMORY_ACPI_RECLAIMABLE;
                break;
            case EfiACPIMemoryNVS:
                mb_type = MULTIBOOT_MEMORY_NVS;
                break;
            case EfiUnusableMemory:
                mb_type = MULTIBOOT_MEMORY_BADRAM;
                break;
            default:
                mb_type = MULTIBOOT_MEMORY_RESERVED;
                break;
        }
        
        mb_entries[mb_count].addr = start_addr;
        mb_entries[mb_count].len = size_bytes;
        mb_entries[mb_count].type = mb_type;
        mb_entries[mb_count].reserved = 0;
        
        // LOG("EFI->MB: 0x%016llX-0x%016llX %s->%s", 
        //     start_addr, start_addr + size_bytes - 1,
        //     efi_memory_type_to_string(desc->type),
        //     multiboot2_memory_type_to_string(mb_type));
        
        mb_count++;
        desc_ptr += efi_descriptor_size;
    }
    
    *entry_count = mb_count;
    LOG("Created %u Multiboot memory map entries from EFI data", mb_count);
    
    return mb_entries;
}

// efi.c'de çağırılacak
void efi_init_memory_detection(void) {
    LOG("Initializing EFI memory detection");
    
    if (efi_get_manual_memory_map()) {
        LOG("EFI memory map successfully obtained");
    } else {
        ERROR("Failed to get EFI memory map");
    }
}

// multiboot2.c'den çağırılacak fallback
struct multiboot_mmap_entry* efi_fallback_get_memory_map(uint32_t *entry_count) {
    if (!efi_system_table) {
        if (entry_count) *entry_count = 0;
        return NULL;
    }
    
    LOG("Using EFI fallback memory map");
    
    // Eğer daha önce alınmamışsa şimdi al
    if (!efi_memory_descriptors) {
        if (!efi_get_manual_memory_map()) {
            if (entry_count) *entry_count = 0;
            return NULL;
        }
    }
    
    return efi_create_multiboot_memory_map(entry_count);
}
