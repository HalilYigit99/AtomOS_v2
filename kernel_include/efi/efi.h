#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* EFI temel tip tanımları */
typedef void* EFI_HANDLE;
typedef uint64_t EFI_STATUS;
typedef void* EFI_EVENT;
typedef uint64_t EFI_TPL;
typedef uint64_t EFI_PHYSICAL_ADDRESS;
typedef uint64_t EFI_VIRTUAL_ADDRESS;

/* EFI GUID yapısı */
typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];
} EFI_GUID;

/* EFI temel durum kodları */
#define EFI_SUCCESS                 0x0000000000000000ULL
#define EFI_LOAD_ERROR              0x8000000000000001ULL
#define EFI_INVALID_PARAMETER       0x8000000000000002ULL
#define EFI_UNSUPPORTED             0x8000000000000003ULL
#define EFI_BAD_BUFFER_SIZE         0x8000000000000004ULL
#define EFI_BUFFER_TOO_SMALL        0x8000000000000005ULL
#define EFI_NOT_READY               0x8000000000000006ULL
#define EFI_DEVICE_ERROR            0x8000000000000007ULL
#define EFI_WRITE_PROTECTED         0x8000000000000008ULL
#define EFI_OUT_OF_RESOURCES        0x8000000000000009ULL
#define EFI_NOT_FOUND               0x800000000000000EULL

/* EFI Memory Types */
typedef enum {
    EfiReservedMemoryType,
    EfiLoaderCode,
    EfiLoaderData,
    EfiBootServicesCode,
    EfiBootServicesData,
    EfiRuntimeServicesCode,
    EfiRuntimeServicesData,
    EfiConventionalMemory,
    EfiUnusableMemory,
    EfiACPIReclaimMemory,
    EfiACPIMemoryNVS,
    EfiMemoryMappedIO,
    EfiMemoryMappedIOPortSpace,
    EfiPalCode,
    EfiPersistentMemory,
    EfiMaxMemoryType
} EFI_MEMORY_TYPE;

/* EFI Memory Descriptor */
typedef struct {
    EFI_MEMORY_TYPE     type;
    EFI_PHYSICAL_ADDRESS physical_start;
    EFI_VIRTUAL_ADDRESS  virtual_start;
    uint64_t            number_of_pages;
    uint64_t            attribute;
} EFI_MEMORY_DESCRIPTOR;

/* EFI Table Header */
typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

/* Forward declarations for function pointers */
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

/* EFI Boot Services - temel fonksiyonlar */
struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    
    /* Task Priority Services */
    void* raise_tpl;
    void* restore_tpl;
    
    /* Memory Services */
    void* allocate_pages;
    void* free_pages;
    void* get_memory_map;
    void* allocate_pool;
    void* free_pool;
    
    /* Event & Timer Services */
    void* create_event;
    void* set_timer;
    void* wait_for_event;
    void* signal_event;
    void* close_event;
    void* check_event;
    
    /* Protocol Handler Services */
    void* install_protocol_interface;
    void* reinstall_protocol_interface;
    void* uninstall_protocol_interface;
    void* handle_protocol;
    void* reserved;
    void* register_protocol_notify;
    void* locate_handle;
    void* locate_device_path;
    void* install_configuration_table;
    
    /* Image Services */
    void* load_image;
    void* start_image;
    void* exit;
    void* unload_image;
    void* exit_boot_services;
    
    /* Miscellaneous Services */
    void* get_next_monotonic_count;
    void* stall;
    void* set_watchdog_timer;
    
    /* DriverSupport Services */
    void* connect_controller;
    void* disconnect_controller;
    
    /* Open and Close Protocol Services */
    void* open_protocol;
    void* close_protocol;
    void* open_protocol_information;
    
    /* Library Services */
    void* protocols_per_handle;
    void* locate_handle_buffer;
    void* locate_protocol;
    void* install_multiple_protocol_interfaces;
    void* uninstall_multiple_protocol_interfaces;
    
    /* 32-bit CRC Services */
    void* calculate_crc32;
    
    /* Miscellaneous Services */
    void* copy_mem;
    void* set_mem;
    void* create_event_ex;
};

/* EFI Runtime Services - temel fonksiyonlar */
struct _EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER hdr;
    
    /* Time Services */
    void* get_time;
    void* set_time;
    void* get_wakeup_time;
    void* set_wakeup_time;
    
    /* Virtual Memory Services */
    void* set_virtual_address_map;
    void* convert_pointer;
    
    /* Variable Services */
    void* get_variable;
    void* get_next_variable_name;
    void* set_variable;
    
    /* Miscellaneous Services */
    void* get_next_high_mono_count;
    void* reset_system;
    
    /* UEFI 2.0 Capsule Services */
    void* update_capsule;
    void* query_capsule_capabilities;
    
    /* Miscellaneous UEFI 2.0 Service */
    void* query_variable_info;
};

/* Simple Text Output Protocol - temel fonksiyonlar */
struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    void* reset;
    void* output_string;
    void* test_string;
    void* query_mode;
    void* set_mode;
    void* set_attribute;
    void* clear_screen;
    void* set_cursor_position;
    void* enable_cursor;
    void* mode;
};

/* Simple Text Input Protocol - temel fonksiyonlar */
struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    void* reset;
    void* read_key_stroke;
    EFI_EVENT wait_for_key;
};

/* Configuration Table */
typedef struct {
    EFI_GUID vendor_guid;
    void*    vendor_table;
} EFI_CONFIGURATION_TABLE;

/* EFI System Table */
struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER                hdr;
    uint16_t*                       firmware_vendor;
    uint32_t                        firmware_revision;
    EFI_HANDLE                      console_in_handle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL* con_in;
    EFI_HANDLE                      console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* con_out;
    EFI_HANDLE                      standard_error_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL* std_err;
    EFI_RUNTIME_SERVICES*           runtime_services;
    EFI_BOOT_SERVICES*              boot_services;
    uint64_t                        number_of_table_entries;
    EFI_CONFIGURATION_TABLE*        configuration_table;
};

/* EFI Loaded Image Protocol */
typedef struct {
    uint32_t            revision;
    EFI_HANDLE          parent_handle;
    EFI_SYSTEM_TABLE*   system_table;
    
    /* Source location of the image */
    EFI_HANDLE          device_handle;
    void*               file_path;
    void*               reserved;
    
    /* Image load options */
    uint32_t            load_options_size;
    void*               load_options;
    
    /* Location where image was loaded */
    void*               image_base;
    uint64_t            image_size;
    EFI_MEMORY_TYPE     image_code_type;
    EFI_MEMORY_TYPE     image_data_type;
    void*               unload;
} EFI_LOADED_IMAGE_PROTOCOL;

/* Temel EFI GUID'leri */
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { 0x9042A9DE, 0x23DC, 0x4A38, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A} }

#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID \
    { 0x387477C1, 0x69C7, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
    { 0x387477C2, 0x69C7, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

/* EFI System Table signature */
#define EFI_SYSTEM_TABLE_SIGNATURE 0x5453595320494249ULL
#define EFI_2_80_SYSTEM_TABLE_REVISION ((2 << 16) | (80))
#define EFI_2_70_SYSTEM_TABLE_REVISION ((2 << 16) | (70))
#define EFI_2_60_SYSTEM_TABLE_REVISION ((2 << 16) | (60))

/* Boot Services signature */
#define EFI_BOOT_SERVICES_SIGNATURE 0x56524553544f4f42ULL
#define EFI_BOOT_SERVICES_REVISION EFI_2_80_SYSTEM_TABLE_REVISION

/* Runtime Services signature */
#define EFI_RUNTIME_SERVICES_SIGNATURE 0x56524553544e5552ULL
#define EFI_RUNTIME_SERVICES_REVISION EFI_2_80_SYSTEM_TABLE_REVISION

/* EFI makroları */
#define EFI_ERROR(status) ((int64_t)(status) < 0)
#undef EFI_SUCCESS
#define EFI_SUCCESS(status) (!EFI_ERROR(status))

/* Alignment makroları */
#define EFI_PAGE_SIZE 0x1000
#define EFI_PAGE_MASK 0xFFF
#define EFI_PAGES_TO_SIZE(pages) ((pages) << 12)
#define EFI_SIZE_TO_PAGES(size) (((size) + EFI_PAGE_MASK) >> 12)

extern EFI_SYSTEM_TABLE* efi_system_table;
extern EFI_HANDLE efi_image_handle;

#ifdef __cplusplus
}
#endif