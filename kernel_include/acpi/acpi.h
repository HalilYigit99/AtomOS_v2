#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * ACPI ortak tanımlar ve tablolar için temel yapılar
 * - Sürümden bağımsız (v1/v2+) SDT başlığı ve imza sabitleri.
 */

#ifndef ACPI_PACKED
#define ACPI_PACKED __attribute__((packed))
#endif

/* İmza sabitleri */
#define ACPI_SIG_RSDP "RSD PTR " /* 8 byte */
#define ACPI_SIG_RSDT "RSDT"
#define ACPI_SIG_XSDT "XSDT"
#define ACPI_SIG_FADT "FACP"
#define ACPI_SIG_MADT "APIC"
#define ACPI_SIG_HPET "HPET"
#define ACPI_SIG_MCFG "MCFG"
#define ACPI_SIG_DSDT "DSDT"
#define ACPI_SIG_SSDT "SSDT"

/* SDT başlığı - tüm ACPI tabloları için ortak */
typedef struct ACPI_PACKED acpi_sdt_header {
    char     Signature[4];
    uint32_t Length;           /* Başlık dahil toplam uzunluk */
    uint8_t  Revision;
    uint8_t  Checksum;         /* 8-bit checksum */
    char     OEMID[6];
    char     OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} acpi_sdt_header;

/* MADT (APIC) tablo iskeleti ve giriş başlığı */
typedef struct ACPI_PACKED acpi_madt {
    acpi_sdt_header Header;    /* "APIC" */
    uint32_t LocalApicAddress; /* LAPIC MMIO base */
    uint32_t Flags;
    uint8_t  Entries[];        /* Değişken uzunlukta girişler */
} acpi_madt;

typedef struct ACPI_PACKED acpi_madt_entry_header {
    uint8_t Type;
    uint8_t Length;
} acpi_madt_entry_header;

/* Seçme MADT giriş türleri (tam liste değildir) */
typedef enum acpi_madt_entry_type {
    ACPI_MADT_PROCESSOR_LOCAL_APIC      = 0,
    ACPI_MADT_IO_APIC                   = 1,
    ACPI_MADT_INTERRUPT_SOURCE_OVERRIDE = 2,
    ACPI_MADT_NMI_SOURCE                = 3,
    ACPI_MADT_LOCAL_APIC_NMI            = 4,
    ACPI_MADT_LOCAL_APIC_ADDRESS_OVERRIDE = 5,
    ACPI_MADT_IO_SAPIC                  = 6,
    ACPI_MADT_LOCAL_SAPIC               = 7,
    ACPI_MADT_PLATFORM_INTERRUPT_SOURCES = 8,
    ACPI_MADT_PROCESSOR_LOCAL_X2APIC    = 9,
} acpi_madt_entry_type;

/* Basit ACPI init ve tablo erişim fonksiyonları */
void acpi_init(void);
const struct acpi_sdt_header* acpi_get_xsdt(void);
const struct acpi_sdt_header* acpi_get_rsdt(void);
const struct acpi_madt* acpi_get_madt(void);
const struct acpi_sdt_header* acpi_get_fadt(void);
const struct acpi_hpet* acpi_get_hpet(void);
const struct acpi_sdt_header* acpi_get_mcfg(void);

#ifdef __cplusplus
}
#endif
