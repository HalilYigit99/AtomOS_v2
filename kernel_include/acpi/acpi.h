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
#define ACPI_SIG_SPCR "SPCR"

/* ACPI 2.0+: Generic Address Structure (GAS) */
typedef struct ACPI_PACKED acpi_gas {
    uint8_t  AddressSpaceId;   /* 0: System Memory, 1: System I/O, vb. */
    uint8_t  RegisterBitWidth;
    uint8_t  RegisterBitOffset;
    uint8_t  AccessSize;       /* 0: undefined, 1: byte, 2: word, 3: dword, 4: qword */
    uint64_t Address;          /* Fiziksel adres */
} acpi_gas;

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

typedef struct ACPI_PACKED {
    acpi_sdt_header Header;

    /* ACPI 1.0 fields */
    uint32_t FirmwareCtrl;     /* Physical address of FACS */
    uint32_t Dsdt;             /* Physical address of DSDT */
    uint8_t  Reserved1;        /* ACPI 1.0: reserved byte (offset uyumu için) */

    uint8_t  PreferredPMProfile; /* 2.0+: preferred profile */
    uint16_t SciInterrupt;
    uint32_t SmiCommandPort;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BiosReq;
    uint8_t  PstateControl;

    uint32_t Pm1aEventBlock;
    uint32_t Pm1bEventBlock;
    uint32_t Pm1aControlBlock;
    uint32_t Pm1bControlBlock;
    uint32_t Pm2ControlBlock;
    uint32_t PmTimerBlock;
    uint32_t Gpe0Block;
    uint32_t Gpe1Block;

    uint8_t  Pm1EventLength;
    uint8_t  Pm1ControlLength;
    uint8_t  Pm2ControlLength;
    uint8_t  PmTimerLength;
    uint8_t  Gpe0Length;
    uint8_t  Gpe1Length;
    uint8_t  Gpe1Base;
    uint8_t  CstControl;

    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;

    uint16_t FlushSize;
    uint16_t FlushStride;

    uint8_t  DutyOffset;
    uint8_t  DutyWidth;

    uint8_t  DayAlarm;
    uint8_t  MonthAlarm;
    uint8_t  Century;

    /* 2.0+: Boot architecture and flags */
    uint16_t BootArchitectureFlags;
    uint8_t  Reserved2;
    uint32_t Flags;

    /* 2.0+: Reset register (GAS) and value */
    acpi_gas ResetReg;
    uint8_t  ResetValue;
    uint8_t  Reserved3[3];

    /* 2.0+: 64-bit pointers */
    uint64_t XFirmwareCtrl; /* 64-bit FACS */
    uint64_t XDsdt;         /* 64-bit DSDT */

    /* 2.0+: GAS versions of fixed hardware blocks */
    acpi_gas XPm1aEventBlock;
    acpi_gas XPm1bEventBlock;
    acpi_gas XPm1aControlBlock;
    acpi_gas XPm1bControlBlock;
    acpi_gas XPm2ControlBlock;
    acpi_gas XPmTimerBlock;
    acpi_gas XGpe0Block;
    acpi_gas XGpe1Block;
} ACPI_FADT;

/* Basit ACPI init ve tablo erişim fonksiyonları */
void acpi_init(void);
const struct acpi_sdt_header* acpi_get_xsdt(void);
const struct acpi_sdt_header* acpi_get_rsdt(void);
const struct acpi_madt* acpi_get_madt(void);
const struct acpi_sdt_header* acpi_get_fadt(void);
const struct acpi_hpet* acpi_get_hpet(void);
const struct acpi_sdt_header* acpi_get_mcfg(void);

extern size_t acpi_version; /* ACPI sürümü (1.0, 2.0, 3.0, 4.0, 5.0, 6.0) */

extern void* acpi_fadt_ptr; /* FADT tablosu */
extern void* acpi_madt_ptr; /* MADT tablosu */
extern void* acpi_hpet_ptr; /* HPET tablosu */
extern void* acpi_mcfg_ptr; /* MCFG tablosu */

#ifdef __cplusplus
}
#endif
