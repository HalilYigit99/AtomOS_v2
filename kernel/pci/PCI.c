#include <pci/PCI.h>
#include <arch.h>
#include <memory/memory.h>
#include <memory/heap.h>
#include <debug/debug.h>

#define BDF_FMT "%02x:%02x.%u"

static List* g_pciDevices = NULL;
static uint32_t g_epoch = 0;
// Next bus number to assign when encountering unconfigured PCI-to-PCI bridges
static uint8_t g_nextBus = 1;

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

	for (uint8_t i = 0; i < maxBars && dev->barCount < 6; ++i) {
		uint8_t off = 0x10 + i * 4;
		uint32_t barVal = PCI_ConfigRead32(dev->bus, dev->device, dev->function, off);
		if (barVal == 0) continue;

		PCIBAR* b = &dev->bars[dev->barCount];
		b->size = 0; b->prefetch = false; b->is64 = false; b->isIO = false; b->address = 0;

		if (barVal & 0x1) {
			// I/O space BAR (32-bit)
			b->isIO = true;
			b->address = (uint16_t)(barVal & ~0x3u);
			// nothing else to skip; i will be incremented by loop
		} else {
			// Memory space BAR
			uint32_t typeBits = (barVal >> 1) & 0x3;
			b->prefetch = ((barVal & (1u << 3)) != 0);
			if (typeBits == 0x2 && (i + 1) < maxBars) {
				// 64-bit BAR consumes two slots
				uint32_t low = barVal & ~0xFu;
				uint32_t high = PCI_ConfigRead32(dev->bus, dev->device, dev->function, off + 4);
				b->address = ((uint64_t)high << 32) | low;
				b->is64 = true;
				++i; // skip the next BAR slot
			} else {
				b->address = (uint32_t)(barVal & ~0xFu);
			}
		}
		dev->barCount++;
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
		// Read current bus numbering
		d->secondaryBus   = PCI_ConfigRead8(bus, dev, func, 0x19);
		d->subordinateBus = PCI_ConfigRead8(bus, dev, func, 0x1A);

		// Enable basic forwarding on the bridge if requested
		pci_enable_bridge_if_requested(d, enableBridges);

		// If firmware didn't assign bus numbers, assign them dynamically
		if (enableBridges && (d->secondaryBus == 0 || d->secondaryBus > d->subordinateBus)) {
			uint8_t newSecondary = g_nextBus++;
			// Program Primary/Secondary/Subordinate bus numbers
			PCI_ConfigWrite8(bus, dev, func, 0x18, bus);          // Primary
			PCI_ConfigWrite8(bus, dev, func, 0x19, newSecondary); // Secondary
			PCI_ConfigWrite8(bus, dev, func, 0x1A, 0xFF);         // Temporary subordinate (max)
			PCI_ConfigWrite8(bus, dev, func, 0x1B, 0x20);         // Secondary Latency Timer (arbitrary)

			// Update local record
			d->secondaryBus = newSecondary;
			d->subordinateBus = 0xFF;
		}
	}

	pci_parse_bars(d);

	// Recurse into secondary bus for bridges
	if (d->isBridge && d->secondaryBus > 0 && d->secondaryBus <= d->subordinateBus) {
		pci_scan_bus(d->secondaryBus, enableBridges);

		// If we assigned bus numbers dynamically, tighten the subordinate bus number
		if (enableBridges) {
			uint8_t lastUsed = (uint8_t)(g_nextBus - 1);
			if (lastUsed < d->secondaryBus) lastUsed = d->secondaryBus;
			if (lastUsed != d->subordinateBus) {
				PCI_ConfigWrite8(bus, dev, func, 0x1A, lastUsed);
				d->subordinateBus = lastUsed;
			}
		}
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

	PCI_Rescan(true);
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
	// Start assigning new bus numbers from 1 for any unconfigured bridges
	g_nextBus = 1;
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

char* PCI_GetClassName(PCIDeviceClass class)
{
	switch (class)
	{
	case PCI_DEVICE_CLASS_UNKNOWN:
		return "Unknown";
	case PCI_DEVICE_CLASS_STORAGE:
		return "Storage Controller";
	case PCI_DEVICE_CLASS_NETWORK:
		return "Network Controller";
	case PCI_DEVICE_CLASS_DISPLAY:
		return "Display Controller";
	case PCI_DEVICE_CLASS_MULTIMEDIA:
		return "Multimedia Device";
	case PCI_DEVICE_CLASS_MEMORY:
		return "Memory Controller";
	case PCI_DEVICE_CLASS_BRIDGE:
		return "Bridge Device";
	case PCI_DEVICE_CLASS_SIMPLE_COMM:
		return "Simple Communication Controller";
	case PCI_DEVICE_CLASS_BASE_PERIPH:
		return "Base System Peripheral";
	case PCI_DEVICE_CLASS_INPUT:
		return "Input Device";
	case PCI_DEVICE_CLASS_DOCKING:
		return "Docking Station";
	case PCI_DEVICE_CLASS_PROCESSOR:
		return "Processor";
	case PCI_DEVICE_CLASS_SERIAL_BUS:
		return "Serial Bus Controller";
	case PCI_DEVICE_CLASS_WIRELESS:
		return "Wireless Controller";
	case PCI_DEVICE_CLASS_INTELLIGENT_IO:
		return "Intelligent I/O Controller";
	case PCI_DEVICE_CLASS_SATELLITE_COMM:
		return "Satellite Communication Controller";
	case PCI_DEVICE_CLASS_ENCRYPTION:
		return "Encryption/Decryption Controller";
	case PCI_DEVICE_CLASS_SIGNAL_PROCESSING:
		return "Signal Processing Controller";
	case PCI_DEVICE_CLASS_OTHER:
		return "Other Device";
	default:
		return "Unknown";
	}
}

char* PCI_GetSubClassName(uint8_t classCode, uint8_t subclass)
{
	switch (classCode)
	{
	case 0x00: // Unclassified
		switch (subclass)
		{
		case 0x00: return "Non-VGA-Compatible Device";
		case 0x01: return "VGA-Compatible Device";
		default:   return "Unknown";
		}
	case 0x01: // Mass Storage Controller
		switch (subclass)
		{
		case 0x00: return "SCSI Bus Controller";
		case 0x01: return "IDE Controller";
		case 0x02: return "Floppy Disk Controller";
		case 0x03: return "IPI Bus Controller";
		case 0x04: return "RAID Controller";
		case 0x05: return "ATA Controller";
		case 0x06: return "Serial ATA Controller";
		case 0x07: return "Serial Attached SCSI Controller";
		case 0x08: return "Non-Volatile Memory Controller";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	case 0x02: // Network Controller
		switch (subclass)
		{
		case 0x00: return "Ethernet Controller";
		case 0x01: return "Token Ring Controller";
		case 0x02: return "FDDI Controller";
		case 0x03: return "ATM Controller";
		case 0x04: return "ISDN Controller";
		case 0x05: return "WorldFip Controller";
		case 0x06: return "PICMG 2.14 Multi Computing";
		case 0x07: return "Infiniband Controller";
		case 0x08: return "Fabric Controller";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	case 0x03: // Display Controller
		switch (subclass)
		{
		case 0x00: return "VGA Compatible Controller";
		case 0x01: return "XGA Controller";
		case 0x02: return "3D Controller";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	case 0x04: // Multimedia Controller
		switch (subclass)
		{
		case 0x00: return "Multimedia Video Controller";
		case 0x01: return "Multimedia Audio Controller";
		case 0x02: return "Computer Telephony Device";
		case 0x03: return "Audio Device";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	case 0x05: // Memory Controller
		switch (subclass)
		{
		case 0x00: return "RAM Controller";
		case 0x01: return "Flash Controller";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	case 0x06: // Bridge
		switch (subclass)
		{
		case 0x00: return "Host Bridge";
		case 0x01: return "ISA Bridge";
		case 0x02: return "EISA Bridge";
		case 0x03: return "MicroChannel Bridge";
		case 0x04: return "PCI-to-PCI Bridge";
		case 0x05: return "PCMCIA Bridge";
		case 0x06: return "NuBus Bridge";
		case 0x07: return "CardBus Bridge";
		case 0x08: return "RACEway Bridge";
		case 0x09: return "Semi-Transparent PCI-to-PCI Bridge";
		case 0x0A: return "InfiniBand-to-PCI Host Bridge";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	case 0x0C: // Serial Bus Controller
		switch (subclass)
		{
		case 0x00: return "FireWire Controller";
		case 0x01: return "ACCESS Bus";
		case 0x02: return "SSA";
		case 0x03: return "USB Controller";
		case 0x04: return "Fibre Channel";
		case 0x05: return "SMBus";
		case 0x06: return "InfiniBand";
		case 0x07: return "IPMI Interface";
		case 0x08: return "SERCOS Interface";
		case 0x09: return "CANbus";
		case 0x80: return "Other";
		default:   return "Unknown";
		}
	default:
		return "Unknown Subclass";
	}
}