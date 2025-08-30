#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "acpi.h"

// ACPI ortak packed makrosu ve acpi_gas artık acpi.h içinde tanımlı
#ifndef ACPI_PACKED
#define ACPI_PACKED __attribute__((packed))
#endif

/*
 * ACPI v2+ (yeni) yapıları: RSDP v2, XSDT, Generic Address Structure (GAS), HPET
 */

/* acpi_gas tanımı acpi.h içinde (AddressSpaceId, RegisterBitWidth, RegisterBitOffset, AccessSize, Address) */

/* RSDP v2 (36+ bayt) - ACPI 2.0 ve sonrası */
typedef struct ACPI_PACKED acpi_rsdp_v2 {
    char     Signature[8];    /* "RSD PTR " */
    uint8_t  Checksum;        /* İlk 20 bayt checksum */
    char     OEMID[6];
    uint8_t  Revision;        /* >= 2 */
    uint32_t RsdtAddress;     /* RSDT (uyumluluk için) */
    /* ACPI 2.0 alanları */
    uint32_t Length;          /* Bu yapının toplam uzunluğu */
    uint64_t XsdtAddress;     /* XSDT 64-bit adresi */
    uint8_t  ExtendedChecksum;/* Tüm yapı için 8-bit checksum */
    uint8_t  Reserved[3];
} acpi_rsdp_v2;

/* XSDT: SDT başlığını takiben 64-bit adresler */
typedef struct ACPI_PACKED acpi_xsdt {
    struct acpi_sdt_header Header; /* "XSDT" */
    uint64_t TablePointer[];       /* 64-bit SDT adresleri */
} acpi_xsdt;

/* HPET tablo yapısı (özet) - ayrıntılı alanlar için ihtiyaca göre genişletilebilir */
typedef struct ACPI_PACKED acpi_hpet {
    struct acpi_sdt_header Header; /* "HPET" */
    uint8_t  HardwareRevID;
    uint8_t  ComparatorCount:5;
    uint8_t  CounterSize:1;
    uint8_t  Reserved:1;
    uint8_t  LegacyReplacement:1;
    uint16_t PCIVendorID;
    acpi_gas BaseAddress;          /* HPET MMIO base */
    uint8_t  HpetNumber;
    uint16_t MinClockTick;
    uint8_t  PageProtection;       /* Bit alanları */
} acpi_hpet;

#ifdef __cplusplus
}
#endif
