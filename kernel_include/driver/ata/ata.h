#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <driver/DriverBase.h>

// Legacy ATA I/O base addresses (primary/secondary channels)
#define ATA_PRIM_IO   0x1F0
#define ATA_PRIM_CTRL 0x3F6
#define ATA_SEC_IO    0x170
#define ATA_SEC_CTRL  0x376

// ATA register offsets (from IO base)
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01 // read
#define ATA_REG_FEATURES   0x01 // write
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07 // write
#define ATA_REG_STATUS     0x07 // read

// Control register (from CTRL base)
#define ATA_REG_ALTSTATUS  0x00 // read (CTRL base)
#define ATA_REG_DEVCTRL    0x00 // write (CTRL base)

// Status bits
#define ATA_SR_BSY     0x80
#define ATA_SR_DRDY    0x40
#define ATA_SR_DF      0x20
#define ATA_SR_DSC     0x10
#define ATA_SR_DRQ     0x08
#define ATA_SR_CORR    0x04
#define ATA_SR_IDX     0x02
#define ATA_SR_ERR     0x01

// Device control bits
#define ATA_DEVCTRL_SRST 0x04
#define ATA_DEVCTRL_NIEN 0x02

// ATA/ATAPI Commands
#define ATA_CMD_IDENTIFY           0xEC
#define ATA_CMD_IDENTIFY_PACKET    0xA1 // ATAPI identify
#define ATA_CMD_READ_SECTORS       0x20 // PIO, 28-bit
#define ATA_CMD_READ_SECTORS_EXT   0x24 // PIO, 48-bit
#define ATA_CMD_WRITE_SECTORS      0x30 // PIO, 28-bit
#define ATA_CMD_WRITE_SECTORS_EXT  0x34 // PIO, 48-bit
#define ATA_CMD_PACKET             0xA0 // ATAPI PACKET
#define ATA_CMD_FLUSH_CACHE        0xE7
#define ATA_CMD_FLUSH_CACHE_EXT    0xEA
#define ATA_CMD_READ_DMA           0xC8
#define ATA_CMD_READ_DMA_EXT       0x25
#define ATA_CMD_WRITE_DMA          0xCA
#define ATA_CMD_WRITE_DMA_EXT      0x35

// PCI IDE Bus Master (BMIDE) I/O registers (BAR4)
#define ATA_BM_REG_CMD        0x00  // Command register (per channel)
#define ATA_BM_REG_STATUS     0x02  // Status register (per channel)
#define ATA_BM_REG_PRDT       0x04  // PRDT physical address
// Secondary channel register block is at +0x08 from BAR4
#define ATA_BM_CH_SECONDARY   0x08

// BM Command bits
#define ATA_BM_CMD_START      0x01  // 1=start, 0=stop
#define ATA_BM_CMD_WRITE      0x08  // 1=write to device, 0=read from device

// BM Status bits
#define ATA_BM_ST_ACTIVE      0x01
#define ATA_BM_ST_ERR         0x02
#define ATA_BM_ST_IRQ         0x04

// Physical Region Descriptor (PRD) entry
typedef struct __attribute__((packed)) {
    uint32_t base;       // Physical base address
    uint16_t byte_count; // Byte count (0 means 64KiB)
    uint16_t flags;      // bit15=1 -> end of table
} ata_prd_t;

// ATAPI SCSI packet opcodes
#define ATAPI_CMD_INQUIRY          0x12
#define ATAPI_CMD_REQUEST_SENSE    0x03
#define ATAPI_CMD_READ_CAPACITY10  0x25
#define ATAPI_CMD_READ10           0x28
#define ATAPI_CMD_READ12           0xA8

// Device signatures in LBA1/LBA2 after detect
#define ATA_SIG_ATAPI_LBA1 0x14
#define ATA_SIG_ATAPI_LBA2 0xEB
#define ATA_SIG_ATA_LBA1   0x00
#define ATA_SIG_ATA_LBA2   0x00

typedef enum {
    ATA_TYPE_NONE = 0,
    ATA_TYPE_ATA,
    ATA_TYPE_ATAPI
} ata_device_type_t;

typedef struct {
    bool present;
    ata_device_type_t type;
    uint16_t io_base;   // e.g., 0x1F0 or 0x170
    uint16_t ctrl_base; // e.g., 0x3F6 or 0x376
    uint8_t  drive;     // 0=master, 1=slave
    uint16_t identify[256]; // raw identify data (optional)
    uint64_t total_sectors; // derived from IDENTIFY (LBA28 or LBA48)
    uint32_t sector_size;   // logical sector size (default 512)
    bool     lba48_supported; // IDENTIFY word 83 bit 10
} ata_device_t;

// Exported driver instance
extern DriverBase ata_driver;

// Lifecycle API (DriverBase-compatible)
bool ata_init(void);
void ata_enable(void);
void ata_disable(void);

#ifdef __cplusplus
}
#endif
