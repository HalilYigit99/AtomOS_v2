#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <acpi/acpi.h>
#include <acpi/acpi_old.h>
#include <acpi/acpi_new.h>

typedef struct ACPI_PACKED {
    /* --- ACPI 1.0 Alanları --- */
    struct acpi_sdt_header Header;  // Signature = "FACP"
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;

    uint8_t  Reserved1;

    uint8_t  PreferredPMProfile;
    uint16_t SciInt;
    uint32_t SmiCmd;
    uint8_t  AcpiEnable;
    uint8_t  AcpiDisable;
    uint8_t  S4BiosReq;
    uint8_t  PstateCnt;
    uint32_t Pm1aEvtBlk;
    uint32_t Pm1bEvtBlk;
    uint32_t Pm1aCntBlk;
    uint32_t Pm1bCntBlk;
    uint32_t Pm2CntBlk;
    uint32_t PmTmrBlk;
    uint32_t Gpe0Blk;
    uint32_t Gpe1Blk;
    uint8_t  Pm1EvtLen;
    uint8_t  Pm1CntLen;
    uint8_t  Pm2CntLen;
    uint8_t  PmTmrLen;
    uint8_t  Gpe0BlkLen;
    uint8_t  Gpe1BlkLen;
    uint8_t  Gpe1Base;
    uint8_t  CstCnt;
    uint16_t PLvl2Lat;
    uint16_t PLvl3Lat;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t  DutyOffset;
    uint8_t  DutyWidth;
    uint8_t  DayAlarm;
    uint8_t  MonAlarm;
    uint8_t  Century;
    uint16_t IapcBootArch;
    uint8_t  Reserved2;
    uint32_t Flags;

    /* --- ACPI 2.0+ Genişletilmiş Alanlar --- */
    struct acpi_gas ResetReg;
    uint8_t  ResetValue;
    uint8_t  Reserved3[3];

    uint64_t XFirmwareCtrl;
    uint64_t XDsdt;

    struct acpi_gas XPm1aEvtBlk;
    struct acpi_gas XPm1bEvtBlk;
    struct acpi_gas XPm1aCntBlk;
    struct acpi_gas XPm1bCntBlk;
    struct acpi_gas XPm2CntBlk;
    struct acpi_gas XPmTmrBlk;
    struct acpi_gas XGpe0Blk;
    struct acpi_gas XGpe1Blk;

} acpi_fadt_unified;


#ifdef __cplusplus
}
#endif
