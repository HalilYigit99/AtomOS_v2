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

    LOG("Multiboot2 passed arguments: %s", multiboot2_get_cmdline());

    mb2_is_efi_boot = multiboot2_is_efi_boot();

    if (mb2_is_efi_boot) {
        efi_image_handle = (EFI_HANDLE)multiboot2_get_efi_image_handle();
        efi_system_table = (EFI_SYSTEM_TABLE*)multiboot2_get_efi_system_table();
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