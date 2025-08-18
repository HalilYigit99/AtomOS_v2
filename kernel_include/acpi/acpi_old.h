#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "acpi.h"

#ifndef ACPI_PACKED
#define ACPI_PACKED __attribute__((packed))
#endif

/*
 * ACPI v1 (Eski) yapıları: RSDP v1 ve RSDT
 */

/* RSDP v1 yapısı (20 bayt) */
typedef struct ACPI_PACKED acpi_rsdp_v1 {
    char     Signature[8];   /* "RSD PTR " */
    uint8_t  Checksum;       /* İlk 20 bayt için 8-bit checksum */
    char     OEMID[6];
    uint8_t  Revision;       /* 0 = ACPI 1.0 */
    uint32_t RsdtAddress;    /* Fiziksel RSDT adresi */
} acpi_rsdp_v1;

/* RSDT: SDT başlığını takiben 32-bit fiziksel adres dizisi */
typedef struct ACPI_PACKED acpi_rsdt {
    struct acpi_sdt_header Header; /* "RSDT" */
    uint32_t TablePointer[];       /* 32-bit SDT adresleri */
} acpi_rsdt;

#ifdef __cplusplus
}
#endif
