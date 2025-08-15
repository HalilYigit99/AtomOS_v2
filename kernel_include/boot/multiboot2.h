#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Multiboot2 header magic */
#define MULTIBOOT2_HEADER_MAGIC 0xe85250d6
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

/* Multiboot2 header flags */
#define MULTIBOOT_ARCHITECTURE_I386 0
#define MULTIBOOT_ARCHITECTURE_MIPS32 4

/* Multiboot2 tag types */
#define MULTIBOOT_TAG_TYPE_END                 0
#define MULTIBOOT_TAG_TYPE_CMDLINE             1
#define MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME    2
#define MULTIBOOT_TAG_TYPE_MODULE              3
#define MULTIBOOT_TAG_TYPE_BASIC_MEMINFO       4  
#define MULTIBOOT_TAG_TYPE_BOOTDEV             5
#define MULTIBOOT_TAG_TYPE_MMAP                6
#define MULTIBOOT_TAG_TYPE_VBE                 7
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER         8
#define MULTIBOOT_TAG_TYPE_ELF_SECTIONS        9
#define MULTIBOOT_TAG_TYPE_APM                 10
#define MULTIBOOT_TAG_TYPE_EFI32               11
#define MULTIBOOT_TAG_TYPE_EFI64               12
#define MULTIBOOT_TAG_TYPE_SMBIOS              13
#define MULTIBOOT_TAG_TYPE_ACPI_OLD            14
#define MULTIBOOT_TAG_TYPE_ACPI_NEW            15
#define MULTIBOOT_TAG_TYPE_NETWORK             16
#define MULTIBOOT_TAG_TYPE_EFI_MMAP            17
#define MULTIBOOT_TAG_TYPE_EFI_BS              18
#define MULTIBOOT_TAG_TYPE_EFI32_IH            19
#define MULTIBOOT_TAG_TYPE_EFI64_IH            20
#define MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR      21

/* Memory map types */
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4  
#define MULTIBOOT_MEMORY_BADRAM           5

/* EFI memory types */
#define EFI_RESERVED_MEMORY_TYPE     0
#define EFI_LOADER_CODE              1
#define EFI_LOADER_DATA              2
#define EFI_BOOT_SERVICES_CODE       3
#define EFI_BOOT_SERVICES_DATA       4
#define EFI_RUNTIME_SERVICES_CODE    5
#define EFI_RUNTIME_SERVICES_DATA    6
#define EFI_CONVENTIONAL_MEMORY      7
#define EFI_UNUSABLE_MEMORY          8
#define EFI_ACPI_RECLAIM_MEMORY      9
#define EFI_ACPI_MEMORY_NVS          10
#define EFI_MEMORY_MAPPED_IO         11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE                 13
#define EFI_PERSISTENT_MEMORY        14

/* Base multiboot tag structure */
struct multiboot_tag {
    uint32_t type;
    uint32_t size;
};

/* String tags */
struct multiboot_tag_string {
    uint32_t type;
    uint32_t size;
    char string[];
};

/* Module tag */
struct multiboot_tag_module {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    char cmdline[0];
};

/* Basic memory info */
struct multiboot_tag_basic_meminfo {
    uint32_t type;
    uint32_t size;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

/* Boot device */
struct multiboot_tag_bootdev {
    uint32_t type;
    uint32_t size;
    uint32_t biosdev;
    uint32_t slice;
    uint32_t part;
};

/* Memory map entry */
struct multiboot_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
};

/* Memory map tag */
struct multiboot_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    struct multiboot_mmap_entry entries[0];
};

/* VBE info */
struct multiboot_tag_vbe {
    uint32_t type;
    uint32_t size;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint8_t vbe_control_info[512];
    uint8_t vbe_mode_info[256];
};

/* Framebuffer info */
struct multiboot_tag_framebuffer {
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
    
    union {
        struct {
            uint16_t framebuffer_palette_num_colors;
            struct multiboot_color {
                uint8_t red;
                uint8_t green;
                uint8_t blue;
            } framebuffer_palette[0];
        };
        struct {
            uint8_t framebuffer_red_field_position;
            uint8_t framebuffer_red_mask_size;
            uint8_t framebuffer_green_field_position;
            uint8_t framebuffer_green_mask_size;
            uint8_t framebuffer_blue_field_position;
            uint8_t framebuffer_blue_mask_size;
        };
    };
};

/* ELF sections */
struct multiboot_tag_elf_sections {
    uint32_t type;
    uint32_t size;
    uint32_t num;
    uint32_t entsize;
    uint32_t shndx;
    char sections[0];
};

/* APM table */
struct multiboot_tag_apm {
    uint32_t type;
    uint32_t size;
    uint16_t version;
    uint16_t cseg;
    uint32_t offset;
    uint16_t cseg_16;
    uint16_t dseg;
    uint16_t flags;
    uint16_t cseg_len;
    uint16_t cseg_16_len;
    uint16_t dseg_len;
};

/* EFI system table pointers */
struct multiboot_tag_efi32 {
    uint32_t type;
    uint32_t size;
    uint32_t pointer;
};

struct multiboot_tag_efi64 {
    uint32_t type;
    uint32_t size;
    uint64_t pointer;
};

/* SMBIOS tables */
struct multiboot_tag_smbios {
    uint32_t type;
    uint32_t size;
    uint8_t major;
    uint8_t minor;
    uint8_t reserved[6];
    uint8_t tables[0];
};

/* ACPI tables */
struct multiboot_tag_old_acpi {
    uint32_t type;
    uint32_t size;
    uint8_t rsdp[0];
};

struct multiboot_tag_new_acpi {
    uint32_t type;
    uint32_t size;
    uint8_t rsdp[0];
};

/* Network info */
struct multiboot_tag_network {
    uint32_t type;
    uint32_t size;
    uint8_t dhcpack[0];
};

/* EFI memory map */
struct multiboot_tag_efi_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t descr_size;
    uint32_t descr_vers;
    uint8_t efi_mmap[0];
};

/* EFI image handle pointers */
struct multiboot_tag_efi32_ih {
    uint32_t type;
    uint32_t size;
    uint32_t pointer;
};

struct multiboot_tag_efi64_ih {
    uint32_t type;
    uint32_t size;
    uint64_t pointer;
};

/* Load base address */
struct multiboot_tag_load_base_addr {
    uint32_t type;
    uint32_t size;
    uint32_t load_base_addr;
};

/* Global tag pointers - externally defined */
extern struct multiboot_tag_string *mb2_cmdline;
extern struct multiboot_tag_string *mb2_bootloader_name;
extern struct multiboot_tag_module *mb2_module;
extern struct multiboot_tag_basic_meminfo *mb2_basic_meminfo;
extern struct multiboot_tag_bootdev *mb2_bootdev;
extern struct multiboot_tag_mmap *mb2_mmap;
extern struct multiboot_tag_vbe *mb2_vbe;
extern struct multiboot_tag_framebuffer *mb2_framebuffer;
extern struct multiboot_tag_elf_sections *mb2_elf_sections;
extern struct multiboot_tag_apm *mb2_apm;
extern struct multiboot_tag_efi32 *mb2_efi32;
extern struct multiboot_tag_efi64 *mb2_efi64;
extern struct multiboot_tag_smbios *mb2_smbios;
extern struct multiboot_tag_old_acpi *mb2_acpi_old;
extern struct multiboot_tag_new_acpi *mb2_acpi_new;
extern struct multiboot_tag_network *mb2_network;
extern struct multiboot_tag_efi_mmap *mb2_efi_mmap;  
extern struct multiboot_tag_efi32_ih *mb2_efi32_ih;
extern struct multiboot_tag_efi64_ih *mb2_efi64_ih;
extern struct multiboot_tag_load_base_addr *mb2_load_base_addr;

/* Boot mode detection */
extern bool mb2_is_efi_boot;

/* Function prototypes */
void multiboot2_parse();
int multiboot2_is_efi_boot(void);
void* multiboot2_get_efi_system_table(void);
void* multiboot2_get_efi_image_handle(void);
const char* multiboot2_get_cmdline(void);
const char* multiboot2_get_bootloader_name(void);

/* Memory information functions */
uint32_t multiboot2_get_memory_lower(void);
uint32_t multiboot2_get_memory_upper(void);
struct multiboot_mmap_entry* multiboot2_get_memory_map(uint32_t *entry_count);

/* EFI specific functions */
struct multiboot_tag_efi_mmap* multiboot2_get_efi_memory_map(void);
int multiboot2_efi_memory_map_iterate(void (*callback)(uint64_t addr, uint64_t len, uint32_t type));

/* Framebuffer functions */
struct multiboot_tag_framebuffer* multiboot2_get_framebuffer(void);
int multiboot2_has_framebuffer(void);

/* VBE functions */
struct multiboot_tag_vbe* multiboot2_get_vbe(void);
int multiboot2_has_vbe(void);

/* Additional getter functions */
struct multiboot_tag_module* multiboot2_get_module(void);
struct multiboot_tag_bootdev* multiboot2_get_bootdev(void);
struct multiboot_tag_elf_sections* multiboot2_get_elf_sections(void);
struct multiboot_tag_apm* multiboot2_get_apm(void);
struct multiboot_tag_smbios* multiboot2_get_smbios(void);
struct multiboot_tag_old_acpi* multiboot2_get_acpi_old(void);
struct multiboot_tag_new_acpi* multiboot2_get_acpi_new(void);
struct multiboot_tag_network* multiboot2_get_network(void);
struct multiboot_tag_load_base_addr* multiboot2_get_load_base_addr(void);

/* Helper functions */
const char* multiboot2_memory_type_to_string(uint32_t type);
const char* multiboot2_efi_memory_type_to_string(uint32_t type);

#ifdef __cplusplus
}
#endif