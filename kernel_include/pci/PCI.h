#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <list.h>

// PCI Config I/O ports (legacy mechanism #1)
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// PCI Command register bits
#define PCI_CMD_IO_SPACE      (1u << 0)
#define PCI_CMD_MEMORY_SPACE  (1u << 1)
#define PCI_CMD_BUS_MASTER    (1u << 2)

// PCI Header Types
#define PCI_HEADER_TYPE_GENERAL   0x00
#define PCI_HEADER_TYPE_PCI_TO_PCI 0x01
#define PCI_HEADER_TYPE_CARDBUS   0x02

// PCI Class codes (subset)
#define PCI_CLASS_BRIDGE     0x06
#define PCI_SUBCLASS_PCI_TO_PCI 0x04

typedef struct PCIBAR {
	uint64_t address;    // Physical address for MMIO or I/O port base
	uint32_t size;       // Optional, 0 if unknown
	bool     isIO;       // I/O space (true) vs Memory space (false)
	bool     is64;       // 64-bit BAR
	bool     prefetch;   // Prefetchable (for MMIO)
} PCIBAR;

typedef struct PCIDevice {
	// Stable identity (BDF)
	uint8_t  bus;
	uint8_t  device;
	uint8_t  function;

	// Identification
	uint16_t vendorID;
	uint16_t deviceID;
	uint8_t  classCode;
	uint8_t  subclass;
	uint8_t  progIF;
	uint8_t  revision;

	// Common registers
	uint16_t command;
	uint16_t status;
	uint8_t  headerType;   // raw header type (bit7 = multifunction)

	// Decoded
	bool     isBridge;
	uint8_t  secondaryBus;   // valid if isBridge
	uint8_t  subordinateBus; // valid if isBridge

	// BARs (Type 0 up to 6; Type 1 up to 2)
	PCIBAR   bars[6];
	uint8_t  barCount;

	// Internal scanning epoch to perform delta updates without pointer churn
	uint32_t lastSeenEpoch;
} PCIDevice;

// Global device list accessor (persistent between rescans)
List* PCI_GetDeviceList(void);

// Initialize PCI layer (idempotent)
void PCI_Init(void);

// Rescan the PCI hierarchy from bus 0, enabling bridges if requested.
// This performs a delta update: existing PCIDevice pointers remain stable; new
// devices are added; devices no longer present are removed and freed.
void PCI_Rescan(bool enableBridges);

// Convenience helpers
PCIDevice* PCI_FindByBDF(uint8_t bus, uint8_t device, uint8_t function);
PCIDevice* PCI_FindByVendorDevice(uint16_t vendor, uint16_t deviceId);
PCIDevice* PCI_FindByClass(uint8_t classCode, uint8_t subclass, int8_t progIF /* -1 to ignore */);

// Device control
void PCI_EnableBusMastering(PCIDevice* dev);
void PCI_EnableIOAndMemory(PCIDevice* dev);
void PCI_DisableDevice(PCIDevice* dev);

// Raw config space accessors (bus/dev/func addressing)
uint32_t PCI_ConfigRead32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t PCI_ConfigRead16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint8_t  PCI_ConfigRead8 (uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);

void PCI_ConfigWrite32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value);
void PCI_ConfigWrite16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value);
void PCI_ConfigWrite8 (uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t  value);

#ifdef __cplusplus
}
#endif

