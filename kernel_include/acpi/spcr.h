#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <acpi/acpi.h>

/*
 * Serial Port Console Redirection (SPCR) table definition (ACPI 2.0+).
 * We only model the common subset required for console discovery.
 */
typedef struct ACPI_PACKED acpi_spcr {
    acpi_sdt_header Header; /* "SPCR" */
    uint8_t         InterfaceType;
    uint8_t         Reserved1[3];
    acpi_gas        BaseAddress;
    uint8_t         InterruptType;
    uint8_t         PcInterrupt;
    uint32_t        Interrupt;
    uint8_t         BaudRate;
    uint8_t         Parity;
    uint8_t         StopBits;
    uint8_t         FlowControl;
    uint8_t         TerminalType;
    uint8_t         Language;
    uint16_t        PciDeviceId;
    uint16_t        PciVendorId;
    uint8_t         PciBus;
    uint8_t         PciDevice;
    uint8_t         PciFunction;
    uint32_t        PciFlags;
    uint8_t         PciSegment;
    uint32_t        Reserved2;
} acpi_spcr;

#ifdef __cplusplus
}
#endif

