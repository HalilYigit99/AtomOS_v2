#include <boot/multiboot2.h>
#include <stddef.h>
#include <util/string.h>
#include <efi/efi.h>
#include <debug/debug.h>

/* Not using local copies anymore; we only store pointers to original tags */

/* Global tag pointers - will point to local copies */
struct multiboot_tag_string *mb2_cmdline = NULL;
struct multiboot_tag_string *mb2_bootloader_name = NULL;
struct multiboot_tag_module *mb2_module = NULL;
struct multiboot_tag_basic_meminfo *mb2_basic_meminfo = NULL;
struct multiboot_tag_bootdev *mb2_bootdev = NULL;
struct multiboot_tag_mmap *mb2_mmap = NULL;
struct multiboot_tag_vbe *mb2_vbe = NULL;
struct multiboot_tag_framebuffer *mb2_framebuffer = NULL;
struct multiboot_tag_elf_sections *mb2_elf_sections = NULL;
struct multiboot_tag_apm *mb2_apm = NULL;
struct multiboot_tag_efi32 *mb2_efi32 = NULL;
struct multiboot_tag_efi64 *mb2_efi64 = NULL;
struct multiboot_tag_smbios *mb2_smbios = NULL;
struct multiboot_tag_old_acpi *mb2_acpi_old = NULL;
struct multiboot_tag_new_acpi *mb2_acpi_new = NULL;
struct multiboot_tag_network *mb2_network = NULL;
struct multiboot_tag_efi_mmap *mb2_efi_mmap = NULL;
struct multiboot_tag_efi32_ih *mb2_efi32_ih = NULL;
struct multiboot_tag_efi64_ih *mb2_efi64_ih = NULL;
struct multiboot_tag_load_base_addr *mb2_load_base_addr = NULL;

bool mb2_is_efi_boot = false; // Global flag for EFI boot

extern uint32_t mb2_tagptr; // Pointer to the multiboot2 tags

struct multiboot_mmap_entry* efi_mmap_fallback_get_memory_map(uint32_t* entry_count);

void multiboot2_parse() {
    struct multiboot_tag *tag;
    
    /* Skip the first 8 bytes (total_size and reserved) */
    tag = (struct multiboot_tag *)(size_t)(mb2_tagptr + 8);
    
    /* Parse all tags */
    while (tag->type != MULTIBOOT_TAG_TYPE_END) {
        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE:
                LOG("Multiboot2: CMDLINE tag found");
                if (!mb2_cmdline) {
                    mb2_cmdline = (struct multiboot_tag_string *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                LOG("Multiboot2: Bootloader name tag found");
                if (!mb2_bootloader_name) {
                    mb2_bootloader_name = (struct multiboot_tag_string *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_MODULE:
                LOG("Multiboot2: Module tag found");
                if (!mb2_module) {
                    mb2_module = (struct multiboot_tag_module *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                LOG("Multiboot2: Basic memory info tag found");
                if (!mb2_basic_meminfo) {
                    mb2_basic_meminfo = (struct multiboot_tag_basic_meminfo *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_BOOTDEV:
                LOG("Multiboot2: Boot device tag found");
                if (!mb2_bootdev) {
                    mb2_bootdev = (struct multiboot_tag_bootdev *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_MMAP:
                LOG("Multiboot2: Memory map tag found");
                if (!mb2_mmap) {
                    mb2_mmap = (struct multiboot_tag_mmap *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_VBE:
                LOG("Multiboot2: VBE tag found");
                if (!mb2_vbe) {
                    mb2_vbe = (struct multiboot_tag_vbe *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                LOG("Multiboot2: Framebuffer tag found");
                if (!mb2_framebuffer) {
                    mb2_framebuffer = (struct multiboot_tag_framebuffer *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                LOG("Multiboot2: ELF sections tag found");
                if (!mb2_elf_sections) {
                    mb2_elf_sections = (struct multiboot_tag_elf_sections *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_APM:
                LOG("Multiboot2: APM tag found");
                if (!mb2_apm) {
                    mb2_apm = (struct multiboot_tag_apm *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_EFI32:
                LOG("Multiboot2: EFI32 tag found");
                if (!mb2_efi32) {
                    mb2_efi32 = (struct multiboot_tag_efi32 *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_EFI64:
                LOG("Multiboot2: EFI64 tag found");
                if (!mb2_efi64) {
                    mb2_efi64 = (struct multiboot_tag_efi64 *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_SMBIOS:
                LOG("Multiboot2: SMBIOS tag found");
                if (!mb2_smbios) {
                    mb2_smbios = (struct multiboot_tag_smbios *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_ACPI_OLD:
                LOG("Multiboot2: Old ACPI tag found");
                if (!mb2_acpi_old) {
                    mb2_acpi_old = (struct multiboot_tag_old_acpi *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_ACPI_NEW:
                LOG("Multiboot2: New ACPI tag found");
                if (!mb2_acpi_new) {
                    mb2_acpi_new = (struct multiboot_tag_new_acpi *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_NETWORK:
                LOG("Multiboot2: Network tag found");
                if (!mb2_network) {
                    mb2_network = (struct multiboot_tag_network *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_EFI_MMAP:
                LOG("Multiboot2: EFI memory map tag found");
                if (!mb2_efi_mmap) {
                    mb2_efi_mmap = (struct multiboot_tag_efi_mmap *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_EFI_BS:
                LOG("Multiboot2: EFI boot services tag found");
                /* EFI boot services - typically not needed after boot */
                break;
                
            case MULTIBOOT_TAG_TYPE_EFI32_IH:
                LOG("Multiboot2: EFI32 image handle tag found");
                if (!mb2_efi32_ih) {
                    mb2_efi32_ih = (struct multiboot_tag_efi32_ih *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_EFI64_IH:
                LOG("Multiboot2: EFI64 image handle tag found");
                if (!mb2_efi64_ih) {
                    mb2_efi64_ih = (struct multiboot_tag_efi64_ih *)tag;
                }
                break;
                
            case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR:
                LOG("Multiboot2: Load base address tag found");
                if (!mb2_load_base_addr) {
                    mb2_load_base_addr = (struct multiboot_tag_load_base_addr *)tag;
                }
                break;
                
            default:
                /* Unknown tag, skip */
                WARN("Multiboot2: Unknown tag type %u found", tag->type);
                break;
        }
        
        /* Move to next tag (tags are 8-byte aligned) */
        tag = (struct multiboot_tag *)((uint8_t *)tag + ((tag->size + 7) & ~7));
    }

    if (
        mb2_framebuffer == NULL ||
        mb2_cmdline == NULL
    ) {

        // Set msg for textmode terminal
        char* msg = (char*)"Multiboot2 tags are missing required information!\n";

        char* txtBfr = (char*)0xB8000;

        char* msg_tmp = msg;

        while (*msg_tmp) {
            *txtBfr++ = *msg_tmp; // Write character
            *txtBfr++ = 0x1F; // White on black attribute
            msg_tmp++;
        }

    ERROR("%s", msg);
    }

    mb2_is_efi_boot = multiboot2_is_efi_boot();

    if (mb2_is_efi_boot) {
        efi_image_handle = (EFI_HANDLE)multiboot2_get_efi_image_handle();
        efi_system_table = (EFI_SYSTEM_TABLE*)multiboot2_get_efi_system_table();
    }

    // Get memory map
    size_t count;
    if (multiboot2_get_memory_map((uint32_t*)&count)) {
        LOG("Memory map get succeed. Entry Count=%u", count);
    }else {
        ERROR("Memory map failed!");
        asm volatile ("cli; hlt");
    }

}

int multiboot2_is_efi_boot(void) {
    return (mb2_efi32 != NULL || mb2_efi64 != NULL);
}

void* multiboot2_get_efi_system_table(void) {
    if (mb2_efi64) {
        return (void*)(uintptr_t)mb2_efi64->pointer;
    } else if (mb2_efi32) {
        return (void*)(uintptr_t)mb2_efi32->pointer;
    }
    return NULL;
}

void* multiboot2_get_efi_image_handle(void) {
    if (mb2_efi64_ih) {
        return (void*)(uintptr_t)mb2_efi64_ih->pointer;
    } else if (mb2_efi32_ih) {
        return (void*)(uintptr_t)mb2_efi32_ih->pointer;
    }
    return NULL;
}

const char* multiboot2_get_cmdline(void) {
    if (mb2_cmdline && mb2_cmdline->string[0]) {
    return mb2_cmdline->string;
    }
    return NULL;
}

const char* multiboot2_get_bootloader_name(void) {
    if (mb2_bootloader_name && mb2_bootloader_name->string[0]) {
    return mb2_bootloader_name->string;
    }
    return NULL;
}

// EFI specific functions
struct multiboot_tag_efi_mmap* multiboot2_get_efi_memory_map(void) {
    return mb2_efi_mmap;
}

int multiboot2_efi_memory_map_iterate(void (*callback)(uint64_t addr, uint64_t len, uint32_t type)) {
    if (!mb2_efi_mmap || !callback) {
        return 0;
    }
    
    uint32_t entry_size = mb2_efi_mmap->descr_size;
    uint32_t map_size = mb2_efi_mmap->size - sizeof(struct multiboot_tag_efi_mmap);
    uint32_t entry_count = map_size / entry_size;
    
    uint8_t *entry_ptr = mb2_efi_mmap->efi_mmap;
    
    for (uint32_t i = 0; i < entry_count; i++) {
        // EFI memory descriptor genelde şu formatta:
        // Type (32-bit), Padding (32-bit), PhysicalStart (64-bit), VirtualStart (64-bit), 
        // NumberOfPages (64-bit), Attribute (64-bit)
        
        uint32_t *type_ptr = (uint32_t*)entry_ptr;
        uint64_t *addr_ptr = (uint64_t*)(entry_ptr + 8);
        uint64_t *pages_ptr = (uint64_t*)(entry_ptr + 24);
        
        uint32_t type = *type_ptr;
        uint64_t addr = *addr_ptr;
        uint64_t len = *pages_ptr * 4096; // EFI page size is 4KB
        
        callback(addr, len, type);
        
        entry_ptr += entry_size;
    }
    
    return entry_count;
}

// Framebuffer functions
struct multiboot_tag_framebuffer* multiboot2_get_framebuffer(void) {
    return mb2_framebuffer;
}

int multiboot2_has_framebuffer(void) {
    return (mb2_framebuffer != NULL);
}

// VBE functions
struct multiboot_tag_vbe* multiboot2_get_vbe(void) {
    return mb2_vbe;
}

int multiboot2_has_vbe(void) {
    return (mb2_vbe != NULL);
}

// Ek yardımcı fonksiyonlar
struct multiboot_tag_module* multiboot2_get_module(void) {
    return mb2_module;
}

struct multiboot_tag_bootdev* multiboot2_get_bootdev(void) {
    return mb2_bootdev;
}

struct multiboot_tag_elf_sections* multiboot2_get_elf_sections(void) {
    return mb2_elf_sections;
}

struct multiboot_tag_apm* multiboot2_get_apm(void) {
    return mb2_apm;
}

struct multiboot_tag_smbios* multiboot2_get_smbios(void) {
    return mb2_smbios;
}

struct multiboot_tag_old_acpi* multiboot2_get_acpi_old(void) {
    return mb2_acpi_old;
}

struct multiboot_tag_new_acpi* multiboot2_get_acpi_new(void) {
    return mb2_acpi_new;
}

struct multiboot_tag_network* multiboot2_get_network(void) {
    return mb2_network;
}

struct multiboot_tag_load_base_addr* multiboot2_get_load_base_addr(void) {
    return mb2_load_base_addr;
}

// Memory map yardımcı fonksiyonları
const char* multiboot2_memory_type_to_string(uint32_t type) {
    switch (type) {
        case MULTIBOOT_MEMORY_AVAILABLE:
            return "Available";
        case MULTIBOOT_MEMORY_RESERVED:
            return "Reserved";
        case MULTIBOOT_MEMORY_ACPI_RECLAIMABLE:
            return "ACPI Reclaimable";
        case MULTIBOOT_MEMORY_NVS:
            return "ACPI NVS";
        case MULTIBOOT_MEMORY_BADRAM:
            return "Bad RAM";
        default:
            return "Unknown";
    }
}

// EFI memory type yardımcı fonksiyonu
const char* multiboot2_efi_memory_type_to_string(uint32_t type) {
    switch (type) {
        case EFI_RESERVED_MEMORY_TYPE:
            return "Reserved";
        case EFI_LOADER_CODE:
            return "Loader Code";
        case EFI_LOADER_DATA:
            return "Loader Data";
        case EFI_BOOT_SERVICES_CODE:
            return "Boot Services Code";
        case EFI_BOOT_SERVICES_DATA:
            return "Boot Services Data";
        case EFI_RUNTIME_SERVICES_CODE:
            return "Runtime Services Code";
        case EFI_RUNTIME_SERVICES_DATA:
            return "Runtime Services Data";
        case EFI_CONVENTIONAL_MEMORY:
            return "Conventional Memory";
        case EFI_UNUSABLE_MEMORY:
            return "Unusable Memory";
        case EFI_ACPI_RECLAIM_MEMORY:
            return "ACPI Reclaim Memory";
        case EFI_ACPI_MEMORY_NVS:
            return "ACPI Memory NVS";
        case EFI_MEMORY_MAPPED_IO:
            return "Memory Mapped IO";
        case EFI_MEMORY_MAPPED_IO_PORT_SPACE:
            return "Memory Mapped IO Port Space";
        case EFI_PAL_CODE:
            return "PAL Code";
        case EFI_PERSISTENT_MEMORY:
            return "Persistent Memory";
        default:
            return "Unknown";
    }
}

// EFI fallback memory map function
extern struct multiboot_mmap_entry* efi_fallback_get_memory_map(uint32_t *entry_count);

static struct multiboot_mmap_entry* mb2_mmap_entry_ptr;
static uint32_t mb2_mmap_entry_count;

struct multiboot_mmap_entry* multiboot2_get_memory_map(uint32_t *entry_count) {
    if (!entry_count) {
        return NULL;
    }

    // Detected before. Return them
    if (mb2_mmap_entry_ptr && (mb2_mmap_entry_count != 0)) {
        *entry_count = mb2_mmap_entry_count;
        return mb2_mmap_entry_ptr;
    }
    
    *entry_count = 0;
    
    // Önce normal Multiboot2 memory map'i dene
    if (mb2_mmap) {
        *entry_count = (mb2_mmap->size - sizeof(struct multiboot_tag_mmap)) / mb2_mmap->entry_size;
        LOG("Using Multiboot2 BIOS memory map: %u entries", *entry_count);
        mb2_mmap_entry_ptr =  (struct multiboot_mmap_entry*)mb2_mmap->entries;
        mb2_mmap_entry_count = *entry_count;
        return mb2_mmap_entry_ptr;
    }
    
    // Multiboot2 EFI memory map'i dene
    if (mb2_efi_mmap) {
        LOG("Using Multiboot2 EFI memory map");
        mb2_mmap_entry_ptr = efi_mmap_fallback_get_memory_map(entry_count);
        mb2_mmap_entry_count = *entry_count;
        return mb2_mmap_entry_ptr;
    }
    
    // Son çare: Manuel EFI memory map
    if (multiboot2_is_efi_boot() && efi_system_table) {
        LOG("No Multiboot2 memory maps available, trying manual EFI detection");
        mb2_mmap_entry_ptr = efi_fallback_get_memory_map(entry_count);
        mb2_mmap_entry_count = *entry_count;
        return mb2_mmap_entry_ptr;
    }
    
    ERROR("No memory map available (BIOS, EFI Multiboot2, or manual EFI)");
    return (struct multiboot_mmap_entry*)NULL;
}

// Basic memory info için de fallback
uint32_t multiboot2_get_memory_lower(void) {
    if (mb2_basic_meminfo) {
        return mb2_basic_meminfo->mem_lower;
    }
    
    // EFI fallback: İlk 1MB'ı conventional memory olarak say
    if (multiboot2_is_efi_boot()) {
        LOG("Using EFI fallback for lower memory: 640KB");
        return 640; // Standart lower memory
    }
    
    return 0;
}

uint32_t multiboot2_get_memory_upper(void) {
    if (mb2_basic_meminfo) {
        return mb2_basic_meminfo->mem_upper;
    }
    
    // EFI fallback: Memory map'den hesapla
    if (multiboot2_is_efi_boot()) {
        uint32_t entry_count;
        struct multiboot_mmap_entry* mmap = efi_fallback_get_memory_map(&entry_count);
        
        if (mmap) {
            uint64_t total_upper = 0;
            
            for (uint32_t i = 0; i < entry_count; i++) {
                if (mmap[i].type == MULTIBOOT_MEMORY_AVAILABLE && mmap[i].addr >= 0x100000) {
                    total_upper += mmap[i].len;
                }
            }
            
            LOG("EFI fallback upper memory: %llu KB", total_upper / 1024);
            return (uint32_t)(total_upper / 1024);
        }
    }
    
    return 0;
}

struct multiboot_mmap_entry multiboot_mmap_entries[256];

struct multiboot_mmap_entry* efi_mmap_fallback_get_memory_map(uint32_t* entry_count)
{
    
    if (!entry_count) {
        return NULL;
    }
    
    *entry_count = 0;
    
    // EFI memory map'i al
    if (mb2_efi_mmap) {
        uint32_t descr_size = mb2_efi_mmap->descr_size;
        uint32_t map_size = mb2_efi_mmap->size - sizeof(struct multiboot_tag_efi_mmap);
        uint32_t efi_entry_count = map_size / descr_size;

        if (efi_entry_count == 0) {
            WARN("EFI memory map present but empty");
            return NULL;
        }

        uint8_t *ptr = mb2_efi_mmap->efi_mmap;
        uint32_t out = 0;

        for (uint32_t i = 0; i < efi_entry_count; i++) {
            if (out >= 256) {
                ERROR("EFI memory map has too many entries (%u), truncating to 256", efi_entry_count);
                break;
            }

            // UEFI Memory Descriptor yerleşimi:
            // UINT32 Type; UINT32 Pad; UINT64 PhysicalStart; UINT64 VirtualStart;
            // UINT64 NumberOfPages; UINT64 Attribute;
            uint32_t type = *(uint32_t *)(void*)ptr;
            uint64_t phys = *(uint64_t *)(void*)(ptr + 8);
            uint64_t pages = *(uint64_t *)(void*)(ptr + 24);

            uint64_t len = pages * 4096ULL; // EFI sayfa boyutu 4KB
            if (len == 0) {
                ptr += descr_size;
                continue; // sıfır uzunluklu girdileri atla
            }

            multiboot_mmap_entries[out].addr = phys;
            multiboot_mmap_entries[out].len = len;

            // EFI -> Multiboot tür eşlemesi
            uint32_t mb2_type;
            switch (type) {
                case EFI_CONVENTIONAL_MEMORY:
                    mb2_type = MULTIBOOT_MEMORY_AVAILABLE; break;
                case EFI_ACPI_RECLAIM_MEMORY:
                    mb2_type = MULTIBOOT_MEMORY_ACPI_RECLAIMABLE; break;
                case EFI_ACPI_MEMORY_NVS:
                    mb2_type = MULTIBOOT_MEMORY_NVS; break;
                case EFI_UNUSABLE_MEMORY:
                    mb2_type = MULTIBOOT_MEMORY_BADRAM; break;
                case EFI_RESERVED_MEMORY_TYPE:
                case EFI_LOADER_CODE:
                case EFI_LOADER_DATA:
                case EFI_BOOT_SERVICES_CODE:
                case EFI_BOOT_SERVICES_DATA:
                case EFI_RUNTIME_SERVICES_CODE:
                case EFI_RUNTIME_SERVICES_DATA:
                case EFI_MEMORY_MAPPED_IO:
                case EFI_MEMORY_MAPPED_IO_PORT_SPACE:
                case EFI_PAL_CODE:
                case EFI_PERSISTENT_MEMORY:
                default:
                    mb2_type = MULTIBOOT_MEMORY_RESERVED; break;
            }
            multiboot_mmap_entries[out].type = mb2_type;
            multiboot_mmap_entries[out].reserved = 0;
            out++;

            ptr += descr_size;
        }

        *entry_count = out;
        LOG("Converted EFI memory map: in=%u out=%u entries", efi_entry_count, *entry_count);

        if (*entry_count == 0) {
            WARN("EFI memory map conversion produced no entries");
            return NULL;
        }

        return multiboot_mmap_entries;
    }
    
    ERROR("No EFI memory map available");
    return NULL;

}
