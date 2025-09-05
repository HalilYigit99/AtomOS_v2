#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <driver/DriverBase.h>

// Minimal AHCI HBA structures (spec 1.3.1). Only fields we use are defined.
typedef volatile struct {
    uint32_t clb;       // 0x00, Command List Base Address
    uint32_t clbu;      // 0x04, Command List Base Address Upper 32 bits
    uint32_t fb;        // 0x08, FIS Base Address
    uint32_t fbu;       // 0x0C, FIS Base Address Upper 32 bits
    uint32_t is;        // 0x10, Interrupt Status
    uint32_t ie;        // 0x14, Interrupt Enable
    uint32_t cmd;       // 0x18, Command and Status
    uint32_t rsv0;      // 0x1C, Reserved
    uint32_t tfd;       // 0x20, Task File Data
    uint32_t sig;       // 0x24, Signature
    uint32_t ssts;      // 0x28, SATA Status (SStatus)
    uint32_t sctl;      // 0x2C, SATA Control (SControl)
    uint32_t serr;      // 0x30, SATA Error (SError)
    uint32_t sact;      // 0x34, SATA Active (SActive)
    uint32_t ci;        // 0x38, Command Issue
    uint32_t sntf;      // 0x3C, SATA Notification (SNotification)
    uint32_t fbs;       // 0x40, FIS-based Switching Control
    uint32_t rsv1[11];  // 0x44..0x6F, Reserved
    uint32_t vendor[4]; // 0x70..0x7F, Vendor specific
} hba_port_t;

typedef volatile struct {
    // 0x00 - 0x2B
    uint32_t cap;       // 0x00, Host Capabilities
    uint32_t ghc;       // 0x04, Global Host Control
    uint32_t is;        // 0x08, Interrupt Status
    uint32_t pi;        // 0x0C, Ports Implemented
    uint32_t vs;        // 0x10, Version
    uint32_t ccc_ctl;   // 0x14, Command Completion Coalescing Control
    uint32_t ccc_pts;   // 0x18, Command Completion Coalescing Ports
    uint32_t em_loc;    // 0x1C, Enclosure Management Location
    uint32_t em_ctl;    // 0x20, Enclosure Management Control
    uint32_t cap2;      // 0x24, Extended Capabilities
    uint32_t bohc;      // 0x28, BIOS/OS Handoff Control and Status

    uint8_t  rsv[0xA0 - 0x2C]; // 0x2C..0x9F
    uint8_t  vendor[0x60];     // 0xA0..0xFF vendor/reserved
    hba_port_t ports[32];      // 0x100, Port control registers (max 32)
} hba_mem_t;

// SATA signatures
#define SATA_SIG_ATA   0x00000101u
#define SATA_SIG_ATAPI 0xEB140101u
#define SATA_SIG_SEMB  0xC33C0101u
#define SATA_SIG_PM    0x96690101u

// SStatus bits
#define HBA_SSTS_DET_MASK 0x0Fu
#define  HBA_DET_NO_DEVICE 0x0u
#define  HBA_DET_PRESENT   0x3u // device present, Phy communication established

// PxCMD bits
#define HBA_PxCMD_ST   (1u << 0)
#define HBA_PxCMD_SUD  (1u << 1)  // Spin-Up Device
#define HBA_PxCMD_POD  (1u << 2)  // Power On Device
#define HBA_PxCMD_FRE  (1u << 4)
#define HBA_PxCMD_FR   (1u << 14)
#define HBA_PxCMD_CR   (1u << 15)

// PxIS Task File Error Status
#define HBA_PxIS_TFES  (1u << 30)

// PxTFD bits
#define HBA_PxTFD_BSY  (1u << 7)
#define HBA_PxTFD_DRQ  (1u << 3)

// GHC bits
#define HBA_GHC_HR     (1u << 0)   // HBA reset
#define HBA_GHC_IE     (1u << 1)   // Interrupt enable (global)
#define HBA_GHC_AE     (1u << 31)  // AHCI enable

// BOHC (BIOS/OS Handoff) bits
#define HBA_BOHC_BOS   (1u << 0)   // BIOS Owned Semaphore
#define HBA_BOHC_OOS   (1u << 1)   // OS Owned Semaphore

// SStatus helpers
#define HBA_SSTS_SPD(x)   (((x) >> 4) & 0x0F)
#define HBA_SSTS_IPM(x)   (((x) >> 8) & 0x0F)

// AHCI command structures
typedef struct {
    // DW0
    uint8_t  cfl:5;   // Command FIS length in DWORDS
    uint8_t  a:1;     // ATAPI
    uint8_t  w:1;     // Write (1: H2D write to device)
    uint8_t  p:1;     // Prefetchable
    uint8_t  r:1;     // Reset
    uint8_t  b:1;     // BIST
    uint8_t  c:1;     // Clear busy upon R_OK
    uint8_t  rsv0:1;
    uint8_t  pmp:4;   // Port multiplier port
    uint16_t prdtl;   // Physical region descriptor table length
    // DW1
    volatile uint32_t prdbc; // PRDT byte count transferred
    // DW2-3
    uint32_t ctba;    // Command table descriptor base address (low)
    uint32_t ctbau;   // Command table descriptor base address (high)
    // DW4-7 reserved
    uint32_t rsv1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    uint32_t dba;     // data base address (low)
    uint32_t dbau;    // data base address (high)
    uint32_t rsv0;    // reserved
    uint32_t dbc_i;   // [21:0]dbc (byte count-1), [31] ioc
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct {
    uint8_t  cfis[64];   // Command FIS
    uint8_t  acmd[16];   // ATAPI command (not used for ATA)
    uint8_t  rsv[48];
    hba_prdt_entry_t prdt[1]; // We use a single PRDT for simple transfers
} __attribute__((packed)) hba_cmd_table_t;

// FIS types and structures
#define FIS_TYPE_REG_H2D 0x27

typedef struct {
    // DWORD 0
    uint8_t fis_type; // 0x27
    uint8_t pmport:4; // Port multiplier
    uint8_t rsv0:3;
    uint8_t c:1;      // Command (1) or control (0)
    uint8_t command;  // ATA command
    uint8_t featurel; // feature low
    // DWORD 1
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;
    // DWORD 2
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t featureh; // feature high
    // DWORD 3
    uint8_t countl;
    uint8_t counth;
    uint8_t icc;
    uint8_t control;
    // DWORD 4
    uint8_t rsv1[4];
} __attribute__((packed)) fis_reg_h2d_t;

// Exported driver instance
extern DriverBase ahci_driver;

// Lifecycle API (DriverBase-compatible)
bool ahci_init(void);
void ahci_enable(void);
void ahci_disable(void);

#ifdef __cplusplus
}
#endif
