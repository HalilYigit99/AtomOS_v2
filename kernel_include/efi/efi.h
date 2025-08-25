#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Platform algılama ve UINTN/INTN tanımlamaları */
#if defined(__x86_64__) || defined(__aarch64__) || defined(_M_X64) || defined(_M_ARM64)
    #define EFI_64BIT
    typedef uint64_t UINTN;
    typedef int64_t  INTN;
    #define MAX_UINTN 0xFFFFFFFFFFFFFFFFULL
    #define MAX_INTN  0x7FFFFFFFFFFFFFFFLL
#else
    #define EFI_32BIT
    typedef uint32_t UINTN;
    typedef int32_t  INTN;
    #define MAX_UINTN 0xFFFFFFFF
    #define MAX_INTN  0x7FFFFFFF
#endif

/* EFI temel tip tanımları - Platform bağımsız */
typedef void* EFI_HANDLE;
typedef void* EFI_EVENT;

/* Platform bağımlı tipler */
typedef UINTN EFI_STATUS;
typedef UINTN EFI_TPL;

/* Adres tipleri - EFI spesifikasyonuna göre */
#ifdef EFI_64BIT
    typedef uint64_t EFI_PHYSICAL_ADDRESS;
    typedef uint64_t EFI_VIRTUAL_ADDRESS;
#else
    /* EFI32'de bile fiziksel adresler 64-bit olabilir (PAE desteği için) */
    typedef uint64_t EFI_PHYSICAL_ADDRESS;
    typedef uint32_t EFI_VIRTUAL_ADDRESS;
#endif

/* EFI GUID yapısı - Platform bağımsız */
typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t  data4[8];
} EFI_GUID;

/* EFI temel durum kodları - Platform bağımlı değerler */
#ifdef EFI_64BIT
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
    #define EFI_ERROR_MASK              0x8000000000000000ULL
#else
    #define EFI_SUCCESS                 0x00000000
    #define EFI_LOAD_ERROR              0x80000001
    #define EFI_INVALID_PARAMETER       0x80000002
    #define EFI_UNSUPPORTED             0x80000003
    #define EFI_BAD_BUFFER_SIZE         0x80000004
    #define EFI_BUFFER_TOO_SMALL        0x80000005
    #define EFI_NOT_READY               0x80000006
    #define EFI_DEVICE_ERROR            0x80000007
    #define EFI_WRITE_PROTECTED         0x80000008
    #define EFI_OUT_OF_RESOURCES        0x80000009
    #define EFI_NOT_FOUND               0x8000000E
    #define EFI_ERROR_MASK              0x80000000
#endif

/* Calling convention */
#if defined(__x86_64__)
    #define EFIAPI __attribute__((ms_abi))
#elif defined(__i386__)
    #define EFIAPI __attribute__((stdcall))
#elif defined(__aarch64__)
    #define EFIAPI
#elif defined(__arm__)
    #define EFIAPI
#else
    #define EFIAPI
#endif

/* EFI Memory Types - Platform bağımsız */
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

/* Allocation Types */
typedef enum {
    AllocateAnyPages,
    AllocateMaxAddress,
    AllocateAddress,
    MaxAllocateType
} EFI_ALLOCATE_TYPE;

/* Timer Delay Types */
typedef enum {
    TimerCancel,
    TimerPeriodic,
    TimerRelative
} EFI_TIMER_DELAY;

/* EFI Memory Descriptor - Platform uyumlu */
typedef struct {
    uint32_t            type;           /* EFI_MEMORY_TYPE */
    uint32_t            padding;        /* 64-bit hizalama için */
    EFI_PHYSICAL_ADDRESS physical_start;
    EFI_VIRTUAL_ADDRESS  virtual_start;
    uint64_t            number_of_pages; /* Her zaman 64-bit */
    uint64_t            attribute;       /* Her zaman 64-bit */
} EFI_MEMORY_DESCRIPTOR;

/* EFI Table Header - Platform uyumlu */
typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

/* EFI Time yapısı */
typedef struct {
    uint16_t year;       // 1900 - 9999
    uint8_t  month;      // 1 - 12
    uint8_t  day;        // 1 - 31
    uint8_t  hour;       // 0 - 23
    uint8_t  minute;     // 0 - 59
    uint8_t  second;     // 0 - 59
    uint8_t  pad1;
    uint32_t nanosecond; // 0 - 999,999,999
    int16_t  timezone;   // -1440 to 1440 or 2047
    uint8_t  daylight;
    uint8_t  pad2;
} EFI_TIME;

/* EFI Time Capabilities */
typedef struct {
    uint32_t resolution;
    uint32_t accuracy;
    bool     sets_to_zero;
} EFI_TIME_CAPABILITIES;

/* Input Key yapısı */
typedef struct {
    uint16_t scan_code;
    uint16_t unicode_char;
} EFI_INPUT_KEY;

/* Forward declarations */
typedef struct _EFI_SYSTEM_TABLE EFI_SYSTEM_TABLE;
typedef struct _EFI_BOOT_SERVICES EFI_BOOT_SERVICES;
typedef struct _EFI_RUNTIME_SERVICES EFI_RUNTIME_SERVICES;
typedef struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
typedef struct _EFI_DEVICE_PATH_PROTOCOL EFI_DEVICE_PATH_PROTOCOL;
/* Forward declare Graphics Output Protocol to be visible in function typedefs */
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

/* ============================================================================
 * BOOT SERVICES FUNCTION TYPEDEFS
 * ============================================================================ */

/* Task Priority Services */
typedef EFI_TPL (EFIAPI *EFI_RAISE_TPL)(
    EFI_TPL new_tpl
);

typedef void (EFIAPI *EFI_RESTORE_TPL)(
    EFI_TPL old_tpl
);

/* Memory Services */
typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_PAGES)(
    EFI_ALLOCATE_TYPE    type,
    EFI_MEMORY_TYPE      memory_type,
    UINTN                pages,
    EFI_PHYSICAL_ADDRESS *memory
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_PAGES)(
    EFI_PHYSICAL_ADDRESS memory,
    UINTN                pages
);

typedef EFI_STATUS (EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN                 *memory_map_size,
    EFI_MEMORY_DESCRIPTOR *memory_map,
    UINTN                 *map_key,
    UINTN                 *descriptor_size,
    uint32_t              *descriptor_version
);

typedef EFI_STATUS (EFIAPI *EFI_ALLOCATE_POOL)(
    EFI_MEMORY_TYPE pool_type,
    UINTN           size,
    void            **buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FREE_POOL)(
    void *buffer
);

/* Event & Timer Services */
typedef void (EFIAPI *EFI_EVENT_NOTIFY)(
    EFI_EVENT event,
    void      *context
);

typedef EFI_STATUS (EFIAPI *EFI_CREATE_EVENT)(
    uint32_t           type,
    EFI_TPL            notify_tpl,
    EFI_EVENT_NOTIFY   notify_function,
    void               *notify_context,
    EFI_EVENT          *event
);

typedef EFI_STATUS (EFIAPI *EFI_SET_TIMER)(
    EFI_EVENT         event,
    EFI_TIMER_DELAY   type,
    uint64_t          trigger_time
);

typedef EFI_STATUS (EFIAPI *EFI_WAIT_FOR_EVENT)(
    UINTN     number_of_events,
    EFI_EVENT *event,
    UINTN     *index
);

typedef EFI_STATUS (EFIAPI *EFI_SIGNAL_EVENT)(
    EFI_EVENT event
);

typedef EFI_STATUS (EFIAPI *EFI_CLOSE_EVENT)(
    EFI_EVENT event
);

typedef EFI_STATUS (EFIAPI *EFI_CHECK_EVENT)(
    EFI_EVENT event
);

/* Protocol Handler Services */
typedef EFI_STATUS (EFIAPI *EFI_INSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE *handle,
    EFI_GUID   *protocol,
    uint32_t   interface_type,
    void       *interface
);

typedef EFI_STATUS (EFIAPI *EFI_REINSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE handle,
    EFI_GUID   *protocol,
    void       *old_interface,
    void       *new_interface
);

typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_PROTOCOL_INTERFACE)(
    EFI_HANDLE handle,
    EFI_GUID   *protocol,
    void       *interface
);

typedef EFI_STATUS (EFIAPI *EFI_HANDLE_PROTOCOL)(
    EFI_HANDLE handle,
    EFI_GUID   *protocol,
    void       **interface
);

typedef EFI_STATUS (EFIAPI *EFI_REGISTER_PROTOCOL_NOTIFY)(
    EFI_GUID  *protocol,
    EFI_EVENT event,
    void      **registration
);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE)(
    uint32_t       search_type,
    EFI_GUID       *protocol,
    void           *search_key,
    UINTN          *buffer_size,
    EFI_HANDLE     *buffer
);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_DEVICE_PATH)(
    EFI_GUID                  *protocol,
    EFI_DEVICE_PATH_PROTOCOL  **device_path,
    EFI_HANDLE                *device
);

typedef EFI_STATUS (EFIAPI *EFI_INSTALL_CONFIGURATION_TABLE)(
    EFI_GUID *guid,
    void     *table
);

/* Image Services */
typedef EFI_STATUS (EFIAPI *EFI_LOAD_IMAGE)(
    bool                     boot_policy,
    EFI_HANDLE               parent_image_handle,
    EFI_DEVICE_PATH_PROTOCOL *device_path,
    void                     *source_buffer,
    UINTN                    source_size,
    EFI_HANDLE               *image_handle
);

typedef EFI_STATUS (EFIAPI *EFI_START_IMAGE)(
    EFI_HANDLE image_handle,
    UINTN      *exit_data_size,
    uint16_t   **exit_data
);

typedef EFI_STATUS (EFIAPI *EFI_EXIT)(
    EFI_HANDLE image_handle,
    EFI_STATUS exit_status,
    UINTN      exit_data_size,
    uint16_t   *exit_data
);

typedef EFI_STATUS (EFIAPI *EFI_UNLOAD_IMAGE)(
    EFI_HANDLE image_handle
);

typedef EFI_STATUS (EFIAPI *EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE image_handle,
    UINTN      map_key
);

/* Miscellaneous Services */
typedef EFI_STATUS (EFIAPI *EFI_GET_NEXT_MONOTONIC_COUNT)(
    uint64_t *count
);

typedef EFI_STATUS (EFIAPI *EFI_STALL)(
    UINTN microseconds
);

typedef EFI_STATUS (EFIAPI *EFI_SET_WATCHDOG_TIMER)(
    UINTN    timeout,
    uint64_t watchdog_code,
    UINTN    data_size,
    uint16_t *watchdog_data
);

/* Driver Support Services */
typedef EFI_STATUS (EFIAPI *EFI_CONNECT_CONTROLLER)(
    EFI_HANDLE               controller_handle,
    EFI_HANDLE               *driver_image_handle,
    EFI_DEVICE_PATH_PROTOCOL *remaining_device_path,
    bool                     recursive
);

typedef EFI_STATUS (EFIAPI *EFI_DISCONNECT_CONTROLLER)(
    EFI_HANDLE controller_handle,
    EFI_HANDLE driver_image_handle,
    EFI_HANDLE child_handle
);

/* Open and Close Protocol Services */
typedef EFI_STATUS (EFIAPI *EFI_OPEN_PROTOCOL)(
    EFI_HANDLE handle,
    EFI_GUID   *protocol,
    void       **interface,
    EFI_HANDLE agent_handle,
    EFI_HANDLE controller_handle,
    uint32_t   attributes
);

typedef EFI_STATUS (EFIAPI *EFI_CLOSE_PROTOCOL)(
    EFI_HANDLE handle,
    EFI_GUID   *protocol,
    EFI_HANDLE agent_handle,
    EFI_HANDLE controller_handle
);

typedef EFI_STATUS (EFIAPI *EFI_OPEN_PROTOCOL_INFORMATION)(
    EFI_HANDLE handle,
    EFI_GUID   *protocol,
    void       **entry_buffer,
    UINTN      *entry_count
);

/* Library Services */
typedef EFI_STATUS (EFIAPI *EFI_PROTOCOLS_PER_HANDLE)(
    EFI_HANDLE handle,
    EFI_GUID   ***protocol_buffer,
    UINTN      *protocol_buffer_count
);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_HANDLE_BUFFER)(
    uint32_t   search_type,
    EFI_GUID   *protocol,
    void       *search_key,
    UINTN      *no_handles,
    EFI_HANDLE **buffer
);

typedef EFI_STATUS (EFIAPI *EFI_LOCATE_PROTOCOL)(
    EFI_GUID *protocol,
    void     *registration,
    void     **interface
);

typedef EFI_STATUS (EFIAPI *EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    EFI_HANDLE *handle,
    ...
);

typedef EFI_STATUS (EFIAPI *EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES)(
    EFI_HANDLE handle,
    ...
);

/* Miscellaneous Services */
typedef EFI_STATUS (EFIAPI *EFI_CALCULATE_CRC32)(
    void     *data,
    UINTN    data_size,
    uint32_t *crc32
);

typedef void (EFIAPI *EFI_COPY_MEM)(
    void  *destination,
    void  *source,
    UINTN length
);

typedef void (EFIAPI *EFI_SET_MEM)(
    void  *buffer,
    UINTN size,
    uint8_t value
);

typedef EFI_STATUS (EFIAPI *EFI_CREATE_EVENT_EX)(
    uint32_t         type,
    EFI_TPL          notify_tpl,
    EFI_EVENT_NOTIFY notify_function,
    void             *notify_context,
    EFI_GUID         *event_group,
    EFI_EVENT        *event
);

/* ============================================================================
 * RUNTIME SERVICES FUNCTION TYPEDEFS
 * ============================================================================ */

/* Time Services */
typedef EFI_STATUS (EFIAPI *EFI_GET_TIME)(
    EFI_TIME              *time,
    EFI_TIME_CAPABILITIES *capabilities
);

typedef EFI_STATUS (EFIAPI *EFI_SET_TIME)(
    EFI_TIME *time
);

typedef EFI_STATUS (EFIAPI *EFI_GET_WAKEUP_TIME)(
    bool     *enabled,
    bool     *pending,
    EFI_TIME *time
);

typedef EFI_STATUS (EFIAPI *EFI_SET_WAKEUP_TIME)(
    bool     enable,
    EFI_TIME *time
);

/* Virtual Memory Services */
typedef EFI_STATUS (EFIAPI *EFI_SET_VIRTUAL_ADDRESS_MAP)(
    UINTN                 memory_map_size,
    UINTN                 descriptor_size,
    uint32_t              descriptor_version,
    EFI_MEMORY_DESCRIPTOR *virtual_map
);

typedef EFI_STATUS (EFIAPI *EFI_CONVERT_POINTER)(
    UINTN debug_disposition,
    void  **address
);

/* Variable Services */
typedef EFI_STATUS (EFIAPI *EFI_GET_VARIABLE)(
    uint16_t *variable_name,
    EFI_GUID *vendor_guid,
    uint32_t *attributes,
    UINTN    *data_size,
    void     *data
);

typedef EFI_STATUS (EFIAPI *EFI_GET_NEXT_VARIABLE_NAME)(
    UINTN    *variable_name_size,
    uint16_t *variable_name,
    EFI_GUID *vendor_guid
);

typedef EFI_STATUS (EFIAPI *EFI_SET_VARIABLE)(
    uint16_t *variable_name,
    EFI_GUID *vendor_guid,
    uint32_t attributes,
    UINTN    data_size,
    void     *data
);

/* Miscellaneous Services */
typedef EFI_STATUS (EFIAPI *EFI_GET_NEXT_HIGH_MONO_COUNT)(
    uint32_t *high_count
);

typedef void (EFIAPI *EFI_RESET_SYSTEM)(
    uint32_t reset_type,
    EFI_STATUS reset_status,
    UINTN    data_size,
    void     *reset_data
);

/* Capsule Services */
typedef EFI_STATUS (EFIAPI *EFI_UPDATE_CAPSULE)(
    void     **capsule_header_array,
    UINTN    capsule_count,
    EFI_PHYSICAL_ADDRESS scatter_gather_list
);

typedef EFI_STATUS (EFIAPI *EFI_QUERY_CAPSULE_CAPABILITIES)(
    void     **capsule_header_array,
    UINTN    capsule_count,
    uint64_t *maximum_capsule_size,
    uint32_t *reset_type
);

typedef EFI_STATUS (EFIAPI *EFI_QUERY_VARIABLE_INFO)(
    uint32_t attributes,
    uint64_t *maximum_variable_storage_size,
    uint64_t *remaining_variable_storage_size,
    uint64_t *maximum_variable_size
);

/* ============================================================================
 * TEXT PROTOCOL FUNCTION TYPEDEFS
 * ============================================================================ */

/* Simple Text Output Protocol Functions */
typedef EFI_STATUS (EFIAPI *EFI_TEXT_RESET)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    bool extended_verification
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    uint16_t *string
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_TEST_STRING)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    uint16_t *string
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_QUERY_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    UINTN mode_number,
    UINTN *columns,
    UINTN *rows
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_MODE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    UINTN mode_number
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_ATTRIBUTE)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    UINTN attribute
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_CLEAR_SCREEN)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_SET_CURSOR_POSITION)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    UINTN column,
    UINTN row
);

typedef EFI_STATUS (EFIAPI *EFI_TEXT_ENABLE_CURSOR)(
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this,
    bool visible
);

/* Simple Text Input Protocol Functions */
typedef EFI_STATUS (EFIAPI *EFI_INPUT_RESET)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *this,
    bool extended_verification
);

typedef EFI_STATUS (EFIAPI *EFI_INPUT_READ_KEY)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *this,
    EFI_INPUT_KEY *key
);

/* ============================================================================
 * MAIN STRUCTURES WITH FUNCTION POINTERS
 * ============================================================================ */

/* EFI Boot Services */
struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER hdr;
    
    /* Task Priority Services */
    EFI_RAISE_TPL                              raise_tpl;
    EFI_RESTORE_TPL                            restore_tpl;
    
    /* Memory Services */
    EFI_ALLOCATE_PAGES                         allocate_pages;
    EFI_FREE_PAGES                             free_pages;
    EFI_GET_MEMORY_MAP                         get_memory_map;
    EFI_ALLOCATE_POOL                          allocate_pool;
    EFI_FREE_POOL                              free_pool;
    
    /* Event & Timer Services */
    EFI_CREATE_EVENT                           create_event;
    EFI_SET_TIMER                              set_timer;
    EFI_WAIT_FOR_EVENT                         wait_for_event;
    EFI_SIGNAL_EVENT                           signal_event;
    EFI_CLOSE_EVENT                            close_event;
    EFI_CHECK_EVENT                            check_event;
    
    /* Protocol Handler Services */
    EFI_INSTALL_PROTOCOL_INTERFACE             install_protocol_interface;
    EFI_REINSTALL_PROTOCOL_INTERFACE           reinstall_protocol_interface;
    EFI_UNINSTALL_PROTOCOL_INTERFACE           uninstall_protocol_interface;
    EFI_HANDLE_PROTOCOL                        handle_protocol;
    void*                                      reserved;
    EFI_REGISTER_PROTOCOL_NOTIFY               register_protocol_notify;
    EFI_LOCATE_HANDLE                          locate_handle;
    EFI_LOCATE_DEVICE_PATH                     locate_device_path;
    EFI_INSTALL_CONFIGURATION_TABLE            install_configuration_table;
    
    /* Image Services */
    EFI_LOAD_IMAGE                             load_image;
    EFI_START_IMAGE                            start_image;
    EFI_EXIT                                   exit;
    EFI_UNLOAD_IMAGE                           unload_image;
    EFI_EXIT_BOOT_SERVICES                     exit_boot_services;
    
    /* Miscellaneous Services */
    EFI_GET_NEXT_MONOTONIC_COUNT               get_next_monotonic_count;
    EFI_STALL                                  stall;
    EFI_SET_WATCHDOG_TIMER                     set_watchdog_timer;
    
    /* DriverSupport Services */
    EFI_CONNECT_CONTROLLER                     connect_controller;
    EFI_DISCONNECT_CONTROLLER                  disconnect_controller;
    
    /* Open and Close Protocol Services */
    EFI_OPEN_PROTOCOL                          open_protocol;
    EFI_CLOSE_PROTOCOL                         close_protocol;
    EFI_OPEN_PROTOCOL_INFORMATION              open_protocol_information;
    
    /* Library Services */
    EFI_PROTOCOLS_PER_HANDLE                   protocols_per_handle;
    EFI_LOCATE_HANDLE_BUFFER                   locate_handle_buffer;
    EFI_LOCATE_PROTOCOL                        locate_protocol;
    EFI_INSTALL_MULTIPLE_PROTOCOL_INTERFACES   install_multiple_protocol_interfaces;
    EFI_UNINSTALL_MULTIPLE_PROTOCOL_INTERFACES uninstall_multiple_protocol_interfaces;
    
    /* 32-bit CRC Services */
    EFI_CALCULATE_CRC32                        calculate_crc32;
    
    /* Miscellaneous Services */
    EFI_COPY_MEM                               copy_mem;
    EFI_SET_MEM                                set_mem;
    EFI_CREATE_EVENT_EX                        create_event_ex;
};

/* EFI Runtime Services */
struct _EFI_RUNTIME_SERVICES {
    EFI_TABLE_HEADER hdr;
    
    /* Time Services */
    EFI_GET_TIME                    get_time;
    EFI_SET_TIME                    set_time;
    EFI_GET_WAKEUP_TIME             get_wakeup_time;
    EFI_SET_WAKEUP_TIME             set_wakeup_time;
    
    /* Virtual Memory Services */
    EFI_SET_VIRTUAL_ADDRESS_MAP     set_virtual_address_map;
    EFI_CONVERT_POINTER             convert_pointer;
    
    /* Variable Services */
    EFI_GET_VARIABLE                get_variable;
    EFI_GET_NEXT_VARIABLE_NAME      get_next_variable_name;
    EFI_SET_VARIABLE                set_variable;
    
    /* Miscellaneous Services */
    EFI_GET_NEXT_HIGH_MONO_COUNT    get_next_high_mono_count;
    EFI_RESET_SYSTEM                reset_system;
    
    /* UEFI 2.0 Capsule Services */
    EFI_UPDATE_CAPSULE              update_capsule;
    EFI_QUERY_CAPSULE_CAPABILITIES  query_capsule_capabilities;
    
    /* Miscellaneous UEFI 2.0 Service */
    EFI_QUERY_VARIABLE_INFO         query_variable_info;
};

/* Simple Text Output Mode */
typedef struct {
    int32_t max_mode;
    int32_t mode;
    int32_t attribute;
    int32_t cursor_column;
    int32_t cursor_row;
    bool    cursor_visible;
} EFI_SIMPLE_TEXT_OUTPUT_MODE;

/* Simple Text Output Protocol */
struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL {
    EFI_TEXT_RESET                reset;
    EFI_TEXT_STRING               output_string;
    EFI_TEXT_TEST_STRING          test_string;
    EFI_TEXT_QUERY_MODE           query_mode;
    EFI_TEXT_SET_MODE             set_mode;
    EFI_TEXT_SET_ATTRIBUTE        set_attribute;
    EFI_TEXT_CLEAR_SCREEN         clear_screen;
    EFI_TEXT_SET_CURSOR_POSITION  set_cursor_position;
    EFI_TEXT_ENABLE_CURSOR        enable_cursor;
    EFI_SIMPLE_TEXT_OUTPUT_MODE   *mode;
};

/* Simple Text Input Protocol */
struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET               reset;
    EFI_INPUT_READ_KEY            read_key_stroke;
    EFI_EVENT                     wait_for_key;
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
    UINTN                           number_of_table_entries;
    EFI_CONFIGURATION_TABLE*        configuration_table;
};

/* EFI Loaded Image Protocol */
typedef struct {
    uint32_t            revision;
    EFI_HANDLE          parent_handle;
    EFI_SYSTEM_TABLE*   system_table;
    
    /* Source location of the image */
    EFI_HANDLE          device_handle;
    EFI_DEVICE_PATH_PROTOCOL *file_path;
    void*               reserved;
    
    /* Image load options */
    uint32_t            load_options_size;
    void*               load_options;
    
    /* Location where image was loaded */
    void*               image_base;
    uint64_t            image_size;
    EFI_MEMORY_TYPE     image_code_type;
    EFI_MEMORY_TYPE     image_data_type;
    
    /* Unload function */
    EFI_STATUS (EFIAPI *unload)(EFI_HANDLE image_handle);
} EFI_LOADED_IMAGE_PROTOCOL;

/* ============================================================================
 * GRAPHICS OUTPUT PROTOCOL
 * ============================================================================ */

/* Pixel formats */
typedef enum {
    PixelRedGreenBlueReserved8BitPerColor,
    PixelBlueGreenRedReserved8BitPerColor,
    PixelBitMask,
    PixelBltOnly,
    PixelFormatMax
} EFI_GRAPHICS_PIXEL_FORMAT;

/* Graphics Output Protocol Mode Info */
typedef struct {
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    EFI_GRAPHICS_PIXEL_FORMAT pixel_format;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
    uint32_t reserved_mask;
    uint32_t pixels_per_scan_line;
} EFI_GRAPHICS_OUTPUT_MODE_INFO;

/* Graphics Output Protocol Mode */
typedef struct {
    uint32_t                        max_mode;
    uint32_t                        mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFO*  info;
    UINTN                           size_of_info;
    EFI_PHYSICAL_ADDRESS            frame_buffer_base;
    UINTN                           frame_buffer_size;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

/* Graphics Output Blt Pixel */
typedef struct {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} EFI_GRAPHICS_OUTPUT_BLT_PIXEL;

/* Graphics Output Blt Operation */
typedef enum {
    EfiBltVideoFill,
    EfiBltVideoToBltBuffer,
    EfiBltBufferToVideo,
    EfiBltVideoToVideo,
    EfiGraphicsOutputBltOperationMax
} EFI_GRAPHICS_OUTPUT_BLT_OPERATION;

/* Graphics Output Protocol Functions */
typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *this,
    uint32_t mode_number,
    UINTN *size_of_info,
    EFI_GRAPHICS_OUTPUT_MODE_INFO **info
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *this,
    uint32_t mode_number
);

typedef EFI_STATUS (EFIAPI *EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT)(
    struct _EFI_GRAPHICS_OUTPUT_PROTOCOL *this,
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *blt_buffer,
    EFI_GRAPHICS_OUTPUT_BLT_OPERATION blt_operation,
    UINTN source_x,
    UINTN source_y,
    UINTN destination_x,
    UINTN destination_y,
    UINTN width,
    UINTN height,
    UINTN delta
);

/* Graphics Output Protocol */
typedef struct _EFI_GRAPHICS_OUTPUT_PROTOCOL {
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE query_mode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE   set_mode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_BLT        blt;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE       *mode;
} EFI_GRAPHICS_OUTPUT_PROTOCOL;

/* ============================================================================
 * FILE PROTOCOL
 * ============================================================================ */

/* File modes */
#define EFI_FILE_MODE_READ   0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE  0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL

/* File attributes */
#define EFI_FILE_READ_ONLY  0x0000000000000001ULL
#define EFI_FILE_HIDDEN     0x0000000000000002ULL
#define EFI_FILE_SYSTEM     0x0000000000000004ULL
#define EFI_FILE_RESERVED   0x0000000000000008ULL
#define EFI_FILE_DIRECTORY  0x0000000000000010ULL
#define EFI_FILE_ARCHIVE    0x0000000000000020ULL
#define EFI_FILE_VALID_ATTR 0x0000000000000037ULL

/* File Protocol */
typedef struct _EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_FILE_OPEN)(
    EFI_FILE_PROTOCOL *this,
    EFI_FILE_PROTOCOL **new_handle,
    uint16_t *file_name,
    uint64_t open_mode,
    uint64_t attributes
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_CLOSE)(
    EFI_FILE_PROTOCOL *this
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_DELETE)(
    EFI_FILE_PROTOCOL *this
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_READ)(
    EFI_FILE_PROTOCOL *this,
    UINTN *buffer_size,
    void *buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_WRITE)(
    EFI_FILE_PROTOCOL *this,
    UINTN *buffer_size,
    void *buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_POSITION)(
    EFI_FILE_PROTOCOL *this,
    uint64_t *position
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_POSITION)(
    EFI_FILE_PROTOCOL *this,
    uint64_t position
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_GET_INFO)(
    EFI_FILE_PROTOCOL *this,
    EFI_GUID *information_type,
    UINTN *buffer_size,
    void *buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_SET_INFO)(
    EFI_FILE_PROTOCOL *this,
    EFI_GUID *information_type,
    UINTN buffer_size,
    void *buffer
);

typedef EFI_STATUS (EFIAPI *EFI_FILE_FLUSH)(
    EFI_FILE_PROTOCOL *this
);

struct _EFI_FILE_PROTOCOL {
    uint64_t           revision;
    EFI_FILE_OPEN      open;
    EFI_FILE_CLOSE     close;
    EFI_FILE_DELETE    delete;
    EFI_FILE_READ      read;
    EFI_FILE_WRITE     write;
    EFI_FILE_GET_POSITION get_position;
    EFI_FILE_SET_POSITION set_position;
    EFI_FILE_GET_INFO  get_info;
    EFI_FILE_SET_INFO  set_info;
    EFI_FILE_FLUSH     flush;
};

/* Simple File System Protocol */
typedef struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

typedef EFI_STATUS (EFIAPI *EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME)(
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *this,
    EFI_FILE_PROTOCOL **root
);

struct _EFI_SIMPLE_FILE_SYSTEM_PROTOCOL {
    uint64_t revision;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_OPEN_VOLUME open_volume;
};

/* ============================================================================
 * DEVICE PATH PROTOCOL
 * ============================================================================ */

struct _EFI_DEVICE_PATH_PROTOCOL {
    uint8_t type;
    uint8_t sub_type;
    uint8_t length[2];
};

/* ============================================================================
 * GUIDS
 * ============================================================================ */

/* Temel EFI GUID'leri */
#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    { 0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    { 0x9042A9DE, 0x23DC, 0x4A38, {0x96, 0xFB, 0x7A, 0xDE, 0xD0, 0x80, 0x51, 0x6A} }

#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID \
    { 0x387477C1, 0x69C7, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_GUID \
    { 0x387477C2, 0x69C7, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    { 0x964E5B22, 0x6459, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_FILE_INFO_GUID \
    { 0x09576E92, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

#define EFI_FILE_SYSTEM_INFO_GUID \
    { 0x09576E93, 0x6D3F, 0x11D2, {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B} }

/* ============================================================================
 * SIGNATURES
 * ============================================================================ */

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

/* ============================================================================
 * MACROS
 * ============================================================================ */

/* EFI makroları - Platform uyumlu */
#ifdef EFI_64BIT
    #define EFI_ERROR(Status)        (((int64_t)(Status)) < 0)
#else
    #define EFI_ERROR(Status)        (((int32_t)(Status)) < 0)
#endif

#define IS_EFI_ERROR(status)         EFI_ERROR(status)
#define IS_EFI_SUCCESS(status)       (!EFI_ERROR(status))

/* Alignment makroları */
#define EFI_PAGE_SIZE 0x1000
#define EFI_PAGE_MASK 0xFFF
#define EFI_PAGES_TO_SIZE(pages) ((pages) << 12)
#define EFI_SIZE_TO_PAGES(size) (((size) + EFI_PAGE_MASK) >> 12)

/* Memory allocation alignment */
#ifdef EFI_64BIT
    #define EFI_MIN_ALIGNMENT 8
#else
    #define EFI_MIN_ALIGNMENT 4
#endif

/* Attribute codes for console */
#define EFI_BLACK        0x00
#define EFI_BLUE         0x01
#define EFI_GREEN        0x02
#define EFI_CYAN         0x03
#define EFI_RED          0x04
#define EFI_MAGENTA      0x05
#define EFI_BROWN        0x06
#define EFI_LIGHTGRAY    0x07
#define EFI_BRIGHT       0x08
#define EFI_DARKGRAY     0x08
#define EFI_LIGHTBLUE    0x09
#define EFI_LIGHTGREEN   0x0A
#define EFI_LIGHTCYAN    0x0B
#define EFI_LIGHTRED     0x0C
#define EFI_LIGHTMAGENTA 0x0D
#define EFI_YELLOW       0x0E
#define EFI_WHITE        0x0F

#define EFI_BACKGROUND_BLACK     0x00
#define EFI_BACKGROUND_BLUE      0x10
#define EFI_BACKGROUND_GREEN     0x20
#define EFI_BACKGROUND_CYAN      0x30
#define EFI_BACKGROUND_RED       0x40
#define EFI_BACKGROUND_MAGENTA   0x50
#define EFI_BACKGROUND_BROWN     0x60
#define EFI_BACKGROUND_LIGHTGRAY 0x70

/* Text attribute macro */
#define EFI_TEXT_ATTR(fg, bg) ((fg) | ((bg) << 4))

/* ============================================================================
 * GLOBAL VARIABLES
 * ============================================================================ */

extern EFI_SYSTEM_TABLE* efi_system_table;
extern EFI_HANDLE efi_image_handle;

/* ============================================================================
 * HELPER FUNCTIONS (OPTIONAL)
 * ============================================================================ */

#ifdef EFI_IMPLEMENTATION

static inline UINTN efi_strlen(const uint16_t* str) {
    UINTN len = 0;
    while (*str++) len++;
    return len;
}

static inline int efi_strcmp(const uint16_t* s1, const uint16_t* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

static inline void* efi_memcpy(void* dest, const void* src, UINTN n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dest;
}

static inline void* efi_memset(void* s, int c, UINTN n) {
    uint8_t* p = (uint8_t*)s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

static inline int efi_memcmp(const void* s1, const void* s2, UINTN n) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

/* Wide string to ASCII conversion helper */
static inline void efi_wstr_to_str(char* dest, const uint16_t* src, UINTN max_len) {
    UINTN i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = (char)(src[i] & 0xFF);
        i++;
    }
    dest[i] = '\0';
}

/* ASCII to wide string conversion helper */
static inline void efi_str_to_wstr(uint16_t* dest, const char* src, UINTN max_len) {
    UINTN i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = (uint16_t)src[i];
        i++;
    }
    dest[i] = 0;
}

#endif /* EFI_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif