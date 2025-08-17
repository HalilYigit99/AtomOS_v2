#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <efi/efi.h>
#include <boot/multiboot2.h>

// EFI Memory Map functions
bool efi_get_manual_memory_map(void);
const char* efi_memory_type_to_string(EFI_MEMORY_TYPE type);
struct multiboot_mmap_entry* efi_create_multiboot_memory_map(uint32_t* entry_count);
struct multiboot_mmap_entry* efi_fallback_get_memory_map(uint32_t *entry_count);

// Memory map bilgilerine erişim
extern uint32_t efi_memory_descriptor_count;
extern uint32_t efi_descriptor_size;

#ifdef __cplusplus
}
#endif