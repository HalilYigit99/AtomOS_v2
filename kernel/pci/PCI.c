#include <pci/PCI.h>
#include <arch.h>
#include <memory/memory.h>
#include <memory/heap.h>
#include <debug/debug.h>

#define BDF_FMT "%02x:%02x.%u"

static List* g_pciDevices = NULL;
static uint32_t g_epoch = 0;

static inline uint32_t pci_make_config_address(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
	return (uint32_t)(0x80000000u | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (offset & 0xFC));
}

uint32_t PCI_ConfigRead32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
	outl(PCI_CONFIG_ADDRESS, pci_make_config_address(bus, dev, func, offset));
	return inl(PCI_CONFIG_DATA);
}

uint16_t PCI_ConfigRead16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
	uint32_t shift = (offset & 2) * 8;
	return (uint16_t)((PCI_ConfigRead32(bus, dev, func, offset) >> shift) & 0xFFFF);
}

uint8_t PCI_ConfigRead8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset)
{
	uint32_t shift = (offset & 3) * 8;
	return (uint8_t)((PCI_ConfigRead32(bus, dev, func, offset) >> shift) & 0xFF);
}

void PCI_ConfigWrite32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value)
{
	outl(PCI_CONFIG_ADDRESS, pci_make_config_address(bus, dev, func, offset));
	outl(PCI_CONFIG_DATA, value);
}

void PCI_ConfigWrite16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint16_t value)
{
	uint32_t alignedOffset = offset & ~3;
	uint32_t shift = (offset & 2) * 8;
	uint32_t cur = PCI_ConfigRead32(bus, dev, func, alignedOffset);
	cur &= ~(0xFFFFu << shift);
	cur |= ((uint32_t)value) << shift;
	PCI_ConfigWrite32(bus, dev, func, alignedOffset, cur);
}

void PCI_ConfigWrite8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint8_t value)
{
	uint32_t alignedOffset = offset & ~3;
	uint32_t shift = (offset & 3) * 8;
	uint32_t cur = PCI_ConfigRead32(bus, dev, func, alignedOffset);
	cur &= ~(0xFFu << shift);
	cur |= ((uint32_t)value) << shift;
	PCI_ConfigWrite32(bus, dev, func, alignedOffset, cur);
}

static PCIDevice* pci_find_in_list(uint8_t bus, uint8_t dev, uint8_t func)
{
	if (!g_pciDevices) return NULL;
	LIST_FOR_EACH(g_pciDevices, it) {
		PCIDevice* d = (PCIDevice*)it->data;
		if (d && d->bus == bus && d->device == dev && d->function == func) return d;
	}
	return NULL;
}

static void pci_remove_not_seen(void)
{
	if (!g_pciDevices) return;
	size_t index = 0;
	ListNode* node = g_pciDevices->head;
	while (node) {
		PCIDevice* d = (PCIDevice*)node->data;
		ListNode* next = node->next;
		if (d && d->lastSeenEpoch != g_epoch) {
			// Device disappeared; remove and free
			List_RemoveAt(g_pciDevices, index);
			free(d);
			// Do not advance index; list shrank
		} else {
			index++;
		}
		node = next;
	}
}

static void pci_parse_bars(PCIDevice* dev)
{
	dev->barCount = 0;

	// Only parse for header type 0 (endpoints). For bridges, BARs at 0x10-0x18 exist but are rarely used.
	uint8_t type = dev->headerType & 0x7F;
	uint8_t maxBars = (type == PCI_HEADER_TYPE_PCI_TO_PCI) ? 2 : 6;

	for (uint8_t i = 0; i < maxBars && dev->barCount < 6; ) {
		uint8_t off = 0x10 + i * 4;
		uint32_t barVal = PCI_ConfigRead32(dev->bus, dev->device, dev->function, off);
		if (barVal == 0) { i++; continue; }

		PCIBAR* b = &dev->bars[dev->barCount];
		b->size = 0; b->prefetch = false; b->is64 = false; b->isIO = false; b->address = 0;

		if (barVal & 0x1) {
			// I/O space BAR
			b->isIO = true;
			b->address = (uint16_t)(barVal & ~0x3u);
		} else {
			// Memory space BAR
			uint32_t typeBits = (barVal >> 1) & 0x3;
			b->prefetch = ((barVal & (1u << 3)) != 0);
			if (typeBits == 0x2) {
				// 64-bit BAR
				uint32_t low = barVal & ~0xFu;
				uint32_t high = PCI_ConfigRead32(dev->bus, dev->device, dev->function, off + 4);
				b->address = ((uint64_t)high << 32) | low;
				b->is64 = true;
				i += 2;
			} else {
				b->address = (uint32_t)(barVal & ~0xFu);
				i += 1;
			}
		}
		dev->barCount++;
		if (!b->is64) i += 0; // already incremented for 32-bit
	}
}

static void pci_enable_bridge_if_requested(PCIDevice* dev, bool enable)
{
	if (!enable) return;
	if (!dev->isBridge) return;
	// Enable I/O, Memory, and Bus Mastering on bridges to allow downstream scanning/access
	uint16_t cmd = PCI_ConfigRead16(dev->bus, dev->device, dev->function, 0x04);
	uint16_t newCmd = cmd | PCI_CMD_IO_SPACE | PCI_CMD_MEMORY_SPACE | PCI_CMD_BUS_MASTER;
	if (newCmd != cmd) {
		PCI_ConfigWrite16(dev->bus, dev->device, dev->function, 0x04, newCmd);
		dev->command = newCmd;
	}
}

static void pci_visit_function(uint8_t bus, uint8_t dev, uint8_t func, bool enableBridges);
static void pci_scan_bus(uint8_t bus, bool enableBridges);

static void pci_scan_slot(uint8_t bus, uint8_t dev, bool enableBridges)
{
	uint16_t vendor = PCI_ConfigRead16(bus, dev, 0, 0x00);
	if (vendor == 0xFFFF) return; // no device

	uint8_t headerType = PCI_ConfigRead8(bus, dev, 0, 0x0E);
	bool multi = (headerType & 0x80) != 0;
	uint8_t functions = multi ? 8 : 1;
	for (uint8_t func = 0; func < functions; ++func) {
		pci_visit_function(bus, dev, func, enableBridges);
	}
}

static void pci_visit_function(uint8_t bus, uint8_t dev, uint8_t func, bool enableBridges)
{
	uint16_t vendor = PCI_ConfigRead16(bus, dev, func, 0x00);
	if (vendor == 0xFFFF) return;

	uint16_t deviceId = PCI_ConfigRead16(bus, dev, func, 0x02);
	uint8_t  classCode = PCI_ConfigRead8 (bus, dev, func, 0x0B);
	uint8_t  subclass  = PCI_ConfigRead8 (bus, dev, func, 0x0A);
	uint8_t  progIF    = PCI_ConfigRead8 (bus, dev, func, 0x09);
	uint8_t  revision  = PCI_ConfigRead8 (bus, dev, func, 0x08);
	uint16_t command   = PCI_ConfigRead16(bus, dev, func, 0x04);
	uint16_t status    = PCI_ConfigRead16(bus, dev, func, 0x06);
	uint8_t  header    = PCI_ConfigRead8 (bus, dev, func, 0x0E);

	PCIDevice* d = pci_find_in_list(bus, dev, func);
	if (!d) {
		d = (PCIDevice*)malloc(sizeof(PCIDevice));
		if (!d) return; // OOM
		memset(d, 0, sizeof(*d));
		d->bus = bus; d->device = dev; d->function = func;
		List_Add(g_pciDevices, d);
	}

	d->vendorID = vendor;
	d->deviceID = deviceId;
	d->classCode = classCode;
	d->subclass = subclass;
	d->progIF = progIF;
	d->revision = revision;
	d->command = command;
	d->status = status;
	d->headerType = header;
	d->lastSeenEpoch = g_epoch;

	// Decode header
	uint8_t type = header & 0x7F;
	d->isBridge = (type == PCI_HEADER_TYPE_PCI_TO_PCI);
	d->secondaryBus = 0;
	d->subordinateBus = 0;

	if (d->isBridge) {
		d->secondaryBus   = PCI_ConfigRead8(bus, dev, func, 0x19);
		d->subordinateBus = PCI_ConfigRead8(bus, dev, func, 0x1A);
		pci_enable_bridge_if_requested(d, enableBridges);
	}

	pci_parse_bars(d);

	// Recurse into secondary bus for bridges
	if (d->isBridge && d->secondaryBus > 0 && d->secondaryBus <= d->subordinateBus) {
		pci_scan_bus(d->secondaryBus, enableBridges);
	}
}

static void pci_scan_bus(uint8_t bus, bool enableBridges)
{
	for (uint8_t dev = 0; dev < 32; ++dev) {
		pci_scan_slot(bus, dev, enableBridges);
	}
}

void PCI_Init(void)
{
	if (!g_pciDevices) {
		g_pciDevices = List_Create();
	}
}

List* PCI_GetDeviceList(void)
{
	if (!g_pciDevices) PCI_Init();
	return g_pciDevices;
}

void PCI_Rescan(bool enableBridges)
{
	if (!g_pciDevices) PCI_Init();
	g_epoch++;
	pci_scan_bus(0, enableBridges);
	pci_remove_not_seen();
}

PCIDevice* PCI_FindByBDF(uint8_t bus, uint8_t device, uint8_t function)
{
	return pci_find_in_list(bus, device, function);
}

PCIDevice* PCI_FindByVendorDevice(uint16_t vendor, uint16_t deviceId)
{
	if (!g_pciDevices) return NULL;
	LIST_FOR_EACH(g_pciDevices, it) {
		PCIDevice* d = (PCIDevice*)it->data;
		if (d && d->vendorID == vendor && d->deviceID == deviceId) return d;
	}
	return NULL;
}

PCIDevice* PCI_FindByClass(uint8_t classCode, uint8_t subclass, int8_t progIF)
{
	if (!g_pciDevices) return NULL;
	LIST_FOR_EACH(g_pciDevices, it) {
		PCIDevice* d = (PCIDevice*)it->data;
		if (!d) continue;
		if (d->classCode != classCode) continue;
		if (subclass != 0xFF && d->subclass != subclass) continue;
		if (progIF >= 0 && d->progIF != (uint8_t)progIF) continue;
		return d;
	}
	return NULL;
}

void PCI_EnableBusMastering(PCIDevice* dev)
{
	if (!dev) return;
	uint16_t cmd = PCI_ConfigRead16(dev->bus, dev->device, dev->function, 0x04);
	cmd |= PCI_CMD_BUS_MASTER;
	PCI_ConfigWrite16(dev->bus, dev->device, dev->function, 0x04, cmd);
	dev->command = cmd;
}

void PCI_EnableIOAndMemory(PCIDevice* dev)
{
	if (!dev) return;
	uint16_t cmd = PCI_ConfigRead16(dev->bus, dev->device, dev->function, 0x04);
	cmd |= PCI_CMD_IO_SPACE | PCI_CMD_MEMORY_SPACE;
	PCI_ConfigWrite16(dev->bus, dev->device, dev->function, 0x04, cmd);
	dev->command = cmd;
}

void PCI_DisableDevice(PCIDevice* dev)
{
	if (!dev) return;
	uint16_t cmd = PCI_ConfigRead16(dev->bus, dev->device, dev->function, 0x04);
	cmd &= ~(PCI_CMD_IO_SPACE | PCI_CMD_MEMORY_SPACE | PCI_CMD_BUS_MASTER);
	PCI_ConfigWrite16(dev->bus, dev->device, dev->function, 0x04, cmd);
	dev->command = cmd;
}

