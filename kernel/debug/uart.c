#include <debug/uart.h>
#include <util/VPrintf.h>
#include <util/string.h>
#include <util/formatf.h>
#include <arch.h>
#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <acpi/acpi.h>
#include <acpi/acpi_old.h>
#include <acpi/acpi_new.h>
#include <acpi/spcr.h>
#include <memory/memory.h>
#include <memory/mmio.h>
#include <pci/PCI.h>
#include <list.h>

#define UART_MAX_DEVICES          8u
#define UART_DEFAULT_CLOCK        1843200u
#define UART_DEFAULT_BAUD         115200u
#define UART_IO_DEFAULT_SPAN      8u
#define UART_MMIO_DEFAULT_SPAN    0x1000u

#define UART_REG_RBR 0
#define UART_REG_THR 0
#define UART_REG_DLL 0
#define UART_REG_DLM 1
#define UART_REG_IER 1
#define UART_REG_IIR 2
#define UART_REG_FCR 2
#define UART_REG_LCR 3
#define UART_REG_MCR 4
#define UART_REG_LSR 5
#define UART_REG_MSR 6
#define UART_REG_SCR 7

#define UART_LCR_8N1      0x03
#define UART_LCR_DLAB     0x80
#define UART_MCR_DTR      0x01
#define UART_MCR_RTS      0x02
#define UART_MCR_OUT2     0x08
#define UART_MCR_LOOPBACK 0x10
#define UART_FCR_ENABLE   0x01
#define UART_FCR_CLEAR    0x06
#define UART_LSR_THR_EMPTY 0x20

#define ACPI_ASID_SYSTEM_MEMORY 0u
#define ACPI_ASID_SYSTEM_IO     1u

#define UART_ENUM_STAGE_LEGACY  (1u << 0)
#define UART_ENUM_STAGE_ACPI    (1u << 1)
#define UART_ENUM_STAGE_PCI     (1u << 2)

#define UART_ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct {
    UARTBusType    bus;
    uint16_t       io_port;
    uintptr_t      mmio_phys;
    volatile uint8_t* mmio_virt;
    size_t         span;
    uint8_t        reg_shift;
    uint8_t        access_size;
    uint32_t       clock_hz;
    uint32_t       requested_baud;
    uint8_t        interface_type;
    uint8_t        priority;
    bool           preferred;
    bool           is_console;
    bool           present;
    bool           configured;
    bool           claimed;
    const char*    source;
    char           name[32];
} uart_device;

static uart_device g_uart_devices[UART_MAX_DEVICES];
static size_t g_uart_device_count = 0;
static uart_device* g_uart_active = NULL;
static uint8_t g_uart_completed_stages = 0;
static bool g_uart_initialized = false;

static uart_device_info g_uart_info_cache[UART_MAX_DEVICES];
static size_t g_uart_info_cache_count = 0;
static bool g_uart_info_cache_dirty = true;

extern uint32_t mb2_signature;
extern uint32_t mb2_tagptr;

static void uart_mark_info_dirty(void)
{
    g_uart_info_cache_dirty = true;
}

static void uart_sync_public_info(void)
{
    if (!g_uart_info_cache_dirty) {
        return;
    }

    for (size_t i = 0; i < g_uart_device_count && i < UART_MAX_DEVICES; ++i) {
        const uart_device* dev = &g_uart_devices[i];
        uart_device_info* out = &g_uart_info_cache[i];
        out->bus = dev->bus;
        out->present = dev->present;
        out->is_console = dev->is_console;
        out->preferred = dev->preferred;
        out->io_port = dev->io_port;
        out->mmio_phys = dev->mmio_phys;
        out->mmio_virt = (uintptr_t)dev->mmio_virt;
        out->span = dev->span;
        out->reg_shift = dev->reg_shift;
        out->interface_type = dev->interface_type;
        out->clock_hz = dev->clock_hz;
        out->default_baud = dev->requested_baud;
        out->priority = dev->priority;
        out->source = dev->source;
        out->name = dev->name;
    }

    g_uart_info_cache_count = g_uart_device_count;
    g_uart_info_cache_dirty = false;
}

static inline uintptr_t uart_reg_offset(const uart_device* dev, uint8_t reg)
{
    return ((uintptr_t)reg) << dev->reg_shift;
}

static uint8_t uart_reg_read(const uart_device* dev, uint8_t reg)
{
    if (!dev) {
        return 0xFF;
    }

    if (dev->bus == UART_BUS_IO_PORT) {
        return inb((uint16_t)(dev->io_port + reg));
    }

    if (!dev->mmio_virt) {
        return 0xFF;
    }

    volatile uint8_t* ptr = dev->mmio_virt + uart_reg_offset(dev, reg);
    return *ptr;
}

static void uart_reg_write(const uart_device* dev, uint8_t reg, uint8_t value)
{
    if (!dev) {
        return;
    }

    if (dev->bus == UART_BUS_IO_PORT) {
        outb((uint16_t)(dev->io_port + reg), value);
        return;
    }

    if (!dev->mmio_virt) {
        return;
    }

    volatile uint8_t* ptr = dev->mmio_virt + uart_reg_offset(dev, reg);
    *ptr = value;
}

static bool uart_prepare_device(uart_device* dev)
{
    if (!dev) {
        return false;
    }

    if (dev->bus == UART_BUS_MMIO) {
        if (!dev->mmio_virt) {
            dev->mmio_virt = (volatile uint8_t*)(dev->mmio_phys);
        }
        size_t span = dev->span ? dev->span : UART_MMIO_DEFAULT_SPAN;
        if (!mmio_configure_region(dev->mmio_phys, span)) {
            return false;
        }
    }

    return true;
}

static uint32_t uart_spcr_baud_to_rate(uint8_t field)
{
    switch (field) {
        case 3: return 9600u;
        case 4: return 19200u;
        case 6: return 57600u;
        case 7: return 115200u;
        case 8: return 230400u;
        default: return UART_DEFAULT_BAUD;
    }
}

static uint8_t uart_access_size_to_shift(uint8_t access_size)
{
    switch (access_size) {
        case 0:
        case 1:
            return 0;
        case 2:
            return 1;
        case 3:
            return 2;
        case 4:
            return 3;
        default:
            return 0;
    }
}

static uint8_t uart_checksum8(const void* data, size_t len)
{
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += bytes[i];
    }
    return (uint8_t)(sum & 0xFFu);
}

static bool uart_validate_sdt(const acpi_sdt_header* hdr)
{
    if (!hdr) {
        return false;
    }
    if (hdr->Length < sizeof(acpi_sdt_header)) {
        return false;
    }
    return uart_checksum8(hdr, hdr->Length) == 0;
}

static const acpi_rsdp_v1* uart_find_rsdp(const acpi_rsdp_v2** out_rsdp_v2)
{
    if (!mb2_tagptr || mb2_signature != MULTIBOOT2_BOOTLOADER_MAGIC) {
        if (out_rsdp_v2) {
            *out_rsdp_v2 = NULL;
        }
        return NULL;
    }

    const acpi_rsdp_v1* rsdp_v1 = NULL;
    const acpi_rsdp_v2* rsdp_v2 = NULL;

    struct multiboot_tag* tag = (struct multiboot_tag*)(uintptr_t)(mb2_tagptr + 8u);
    while (tag && tag->type != MULTIBOOT_TAG_TYPE_END) {
        if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_NEW && !rsdp_v2) {
            const struct multiboot_tag_new_acpi* new_acpi = (const struct multiboot_tag_new_acpi*)tag;
            rsdp_v2 = (const acpi_rsdp_v2*)(uintptr_t)&new_acpi->rsdp[0];
            rsdp_v1 = (const acpi_rsdp_v1*)rsdp_v2;
        } else if (tag->type == MULTIBOOT_TAG_TYPE_ACPI_OLD && !rsdp_v1) {
            const struct multiboot_tag_old_acpi* old_acpi = (const struct multiboot_tag_old_acpi*)tag;
            rsdp_v1 = (const acpi_rsdp_v1*)(uintptr_t)&old_acpi->rsdp[0];
        }

        uintptr_t next = (uintptr_t)tag;
        next += (uintptr_t)((tag->size + 7u) & ~7u);
        tag = (struct multiboot_tag*)next;
    }

    if (out_rsdp_v2) {
        *out_rsdp_v2 = rsdp_v2;
    }
    return rsdp_v1;
}

static const acpi_spcr* uart_find_spcr_in_root(const acpi_sdt_header* root, bool is_xsdt)
{
    if (!root || !uart_validate_sdt(root)) {
        return NULL;
    }

    size_t entry_size = is_xsdt ? 8u : 4u;
    size_t entry_count = (root->Length - sizeof(acpi_sdt_header)) / entry_size;
    const uint8_t* entries = (const uint8_t*)(root + 1);

    for (size_t i = 0; i < entry_count; ++i) {
        uintptr_t phys;
        if (is_xsdt) {
            const uint64_t* ptr = (const uint64_t*)entries;
            phys = (uintptr_t)ptr[i];
        } else {
            const uint32_t* ptr = (const uint32_t*)entries;
            phys = (uintptr_t)ptr[i];
        }

        if (!phys) {
            continue;
        }

        const acpi_sdt_header* hdr = (const acpi_sdt_header*)phys;
        if (!hdr) {
            continue;
        }
        if (strncmp(hdr->Signature, ACPI_SIG_SPCR, 4) != 0) {
            continue;
        }
        if (!uart_validate_sdt(hdr)) {
            continue;
        }
        return (const acpi_spcr*)hdr;
    }

    return NULL;
}

static const acpi_spcr* uart_find_spcr(void)
{
    const acpi_rsdp_v2* rsdp_v2 = NULL;
    const acpi_rsdp_v1* rsdp_v1 = uart_find_rsdp(&rsdp_v2);
    if (!rsdp_v1) {
        return NULL;
    }

    if (rsdp_v2 && rsdp_v2->Length >= sizeof(acpi_rsdp_v2) && uart_checksum8(rsdp_v2, rsdp_v2->Length) == 0) {
        if (rsdp_v2->XsdtAddress) {
            const acpi_sdt_header* xsdt = (const acpi_sdt_header*)(uintptr_t)rsdp_v2->XsdtAddress;
            const acpi_spcr* spcr = uart_find_spcr_in_root(xsdt, true);
            if (spcr) {
                return spcr;
            }
        }
    }

    if (rsdp_v1->RsdtAddress) {
        const acpi_sdt_header* rsdt = (const acpi_sdt_header*)(uintptr_t)rsdp_v1->RsdtAddress;
        return uart_find_spcr_in_root(rsdt, false);
    }

    return NULL;
}

static uart_device* uart_find_device(UARTBusType bus, uintptr_t key)
{
    for (size_t i = 0; i < g_uart_device_count; ++i) {
        uart_device* dev = &g_uart_devices[i];
        if (dev->bus != bus) {
            continue;
        }
        if (bus == UART_BUS_IO_PORT) {
            if (dev->io_port == (uint16_t)key) {
                return dev;
            }
        } else {
            if (dev->mmio_phys == key) {
                return dev;
            }
        }
    }
    return NULL;
}

static bool uart_device_loopback_test(uart_device* dev)
{
    dev->present = false;

    uint8_t original_mcr = uart_reg_read(dev, UART_REG_MCR);
    uint8_t original_lcr = uart_reg_read(dev, UART_REG_LCR);
    uint8_t original_ier = uart_reg_read(dev, UART_REG_IER);
    uint8_t original_fcr = uart_reg_read(dev, UART_REG_FCR);
    uint8_t original_scr = uart_reg_read(dev, UART_REG_SCR);

    bool had_dlab = (original_lcr & UART_LCR_DLAB) != 0;
    uint8_t original_dll = 0;
    uint8_t original_dlm = 0;

    if (!had_dlab) {
        uart_reg_write(dev, UART_REG_LCR, original_lcr | UART_LCR_DLAB);
    }
    original_dll = uart_reg_read(dev, UART_REG_DLL);
    original_dlm = uart_reg_read(dev, UART_REG_DLM);
    if (!had_dlab) {
        uart_reg_write(dev, UART_REG_LCR, original_lcr);
    }

    uart_reg_write(dev, UART_REG_IER, 0x00);
    uart_reg_write(dev, UART_REG_FCR, 0x00);

    uart_reg_write(dev, UART_REG_SCR, 0x5Au);
    if (uart_reg_read(dev, UART_REG_SCR) != 0x5Au) {
        goto restore;
    }
    uart_reg_write(dev, UART_REG_SCR, 0xA5u);
    if (uart_reg_read(dev, UART_REG_SCR) != 0xA5u) {
        goto restore;
    }

    uart_reg_write(dev, UART_REG_MCR, UART_MCR_LOOPBACK | UART_MCR_DTR | UART_MCR_RTS);
    uart_reg_write(dev, UART_REG_THR, 0xAEu);
    if (uart_reg_read(dev, UART_REG_RBR) != 0xAEu) {
        goto restore;
    }

    dev->present = true;

restore:
    uart_reg_write(dev, UART_REG_MCR, original_mcr);
    uart_reg_write(dev, UART_REG_SCR, original_scr);

    uart_reg_write(dev, UART_REG_LCR, original_lcr | UART_LCR_DLAB);
    uart_reg_write(dev, UART_REG_DLL, original_dll);
    uart_reg_write(dev, UART_REG_DLM, original_dlm);
    uart_reg_write(dev, UART_REG_LCR, original_lcr);

    uart_reg_write(dev, UART_REG_IER, original_ier);
    uart_reg_write(dev, UART_REG_FCR, original_fcr);

    return dev->present;
}

static bool uart_device_detect(uart_device* dev)
{
    if (!dev) {
        return false;
    }

    if (!uart_prepare_device(dev)) {
        return false;
    }

    uint8_t lsr = uart_reg_read(dev, UART_REG_LSR);
    if (lsr == 0xFFu) {
        return false;
    }

    dev->present = uart_device_loopback_test(dev);
    return dev->present;
}

static void uart_device_configure(uart_device* dev)
{
    if (!dev || !dev->present) {
        return;
    }

    if (!uart_prepare_device(dev)) {
        return;
    }

    uint32_t clock = dev->clock_hz ? dev->clock_hz : UART_DEFAULT_CLOCK;
    uint32_t baud = dev->requested_baud ? dev->requested_baud : UART_DEFAULT_BAUD;
    uint32_t divisor = (clock + (baud * 8u)) / (baud * 16u);
    if (divisor == 0) {
        divisor = 1;
    }
    if (divisor > 0xFFFFu) {
        divisor = 0xFFFFu;
    }

    uart_reg_write(dev, UART_REG_IER, 0x00);
    uart_reg_write(dev, UART_REG_FCR, UART_FCR_ENABLE | UART_FCR_CLEAR);
    uart_reg_write(dev, UART_REG_LCR, UART_LCR_DLAB);
    uart_reg_write(dev, UART_REG_DLL, (uint8_t)(divisor & 0xFFu));
    uart_reg_write(dev, UART_REG_DLM, (uint8_t)((divisor >> 8u) & 0xFFu));
    uart_reg_write(dev, UART_REG_LCR, UART_LCR_8N1);
    uart_reg_write(dev, UART_REG_MCR, UART_MCR_DTR | UART_MCR_RTS | UART_MCR_OUT2);

    dev->configured = true;
}

static bool uart_register_device(uart_device* candidate)
{
    uart_device* existing = NULL;
    uintptr_t key = (candidate->bus == UART_BUS_IO_PORT)
        ? (uintptr_t)candidate->io_port
        : candidate->mmio_phys;

    existing = uart_find_device(candidate->bus, key);
    if (existing) {
        existing->preferred |= candidate->preferred;
        existing->is_console |= candidate->is_console;
        if (candidate->priority > existing->priority) {
            bool was_active = (existing == g_uart_active);
            bool was_configured = existing->configured;
            bool detected = existing->present || uart_device_detect(candidate);
            if (!detected) {
                return false;
            }
            *existing = *candidate;
            existing->present = true;
            existing->configured = was_configured && was_active;
            if (was_active) {
                g_uart_active = existing;
            }
        }
        uart_mark_info_dirty();
        return existing->present;
    }

    if (g_uart_device_count >= UART_MAX_DEVICES) {
        WARN("UART: device list full, ignoring %s", candidate->source ? candidate->source : "device");
        return false;
    }

    if (!uart_device_detect(candidate)) {
        return false;
    }

    g_uart_devices[g_uart_device_count] = *candidate;
    uart_device* stored = &g_uart_devices[g_uart_device_count];
    stored->present = true;
    stored->configured = false;
    g_uart_device_count++;
    uart_mark_info_dirty();
    return true;
}

static void uart_probe_acpi_spcr(void)
{
    const acpi_spcr* spcr = uart_find_spcr();
    if (!spcr) {
        return;
    }

    const acpi_gas* gas = &spcr->BaseAddress;
    if (gas->Address == 0) {
        return;
    }

    uart_device dev = {0};
    dev.clock_hz = UART_DEFAULT_CLOCK;
    dev.requested_baud = uart_spcr_baud_to_rate(spcr->BaudRate);
    dev.interface_type = spcr->InterfaceType;
    dev.priority = 80;
    dev.preferred = true;
    dev.is_console = true;
    dev.source = "ACPI SPCR";
    dev.span = UART_MMIO_DEFAULT_SPAN;

    dev.reg_shift = uart_access_size_to_shift(gas->AccessSize);
    dev.access_size = gas->AccessSize;

    if (gas->AddressSpaceId == ACPI_ASID_SYSTEM_IO) {
        dev.bus = UART_BUS_IO_PORT;
        dev.io_port = (uint16_t)gas->Address;
        dev.span = UART_IO_DEFAULT_SPAN;
        strncpy(dev.name, "SPCR (I/O)", sizeof(dev.name) - 1);
    } else if (gas->AddressSpaceId == ACPI_ASID_SYSTEM_MEMORY) {
        dev.bus = UART_BUS_MMIO;
        dev.mmio_phys = (uintptr_t)gas->Address;
        dev.mmio_virt = (volatile uint8_t*)(uintptr_t)gas->Address;
        strncpy(dev.name, "SPCR (MMIO)", sizeof(dev.name) - 1);
    } else {
        return;
    }

    uart_register_device(&dev);
}

static void uart_probe_legacy(void)
{
    static const struct {
        uint16_t port;
        const char* label;
    } legacy_ports[] = {
        {0x3F8u, "Legacy COM1"},
        {0x2F8u, "Legacy COM2"},
        {0x3E8u, "Legacy COM3"},
        {0x2E8u, "Legacy COM4"},
    };

    for (size_t i = 0; i < UART_ARRAY_COUNT(legacy_ports); ++i) {
        uart_device dev = {0};
        dev.bus = UART_BUS_IO_PORT;
        dev.io_port = legacy_ports[i].port;
        dev.clock_hz = UART_DEFAULT_CLOCK;
        dev.requested_baud = UART_DEFAULT_BAUD;
        dev.priority = 20;
        dev.preferred = false;
        dev.is_console = false;
        dev.source = "Legacy";
        dev.span = UART_IO_DEFAULT_SPAN;
        strncpy(dev.name, legacy_ports[i].label, sizeof(dev.name) - 1);
        uart_register_device(&dev);
    }
}

static void uart_probe_pci(void)
{
    List* list = PCI_GetDeviceList();
    if (!list) {
        return;
    }

    LIST_FOR_EACH(list, node) {
        PCIDevice* pci = (PCIDevice*)node->data;
        if (!pci) {
            continue;
        }

        if (pci->classCode != 0x07 || pci->subclass != 0x00) {
            continue;
        }

        for (uint8_t i = 0; i < pci->barCount; ++i) {
            PCIBAR* bar = &pci->bars[i];
            if (!bar->address) {
                continue;
            }

            uart_device dev = {0};
            dev.clock_hz = UART_DEFAULT_CLOCK;
            dev.requested_baud = UART_DEFAULT_BAUD;
            dev.interface_type = pci->progIF;
            dev.priority = 40;
            dev.preferred = false;
            dev.is_console = false;
            dev.source = "PCI";
            dev.span = bar->size ? (size_t)bar->size : UART_MMIO_DEFAULT_SPAN;

            if (bar->isIO) {
                dev.bus = UART_BUS_IO_PORT;
                dev.io_port = (uint16_t)bar->address;
                dev.span = UART_IO_DEFAULT_SPAN;
                strncpy(dev.name, "PCI UART (I/O)", sizeof(dev.name) - 1);
            } else {
                dev.bus = UART_BUS_MMIO;
                dev.mmio_phys = (uintptr_t)bar->address;
                dev.mmio_virt = (volatile uint8_t*)(uintptr_t)bar->address;
                strncpy(dev.name, "PCI UART (MMIO)", sizeof(dev.name) - 1);
            }

            char* formatted = formatf("%s %02x:%02x.%u",
                                      dev.name,
                                      pci->bus,
                                      pci->device,
                                      pci->function);
            if (formatted) {
                strncpy(dev.name, formatted, sizeof(dev.name) - 1);
                free(formatted);
            }

            uart_register_device(&dev);
        }
    }
}

static void uart_discover(uint8_t stages)
{
    if (stages & UART_ENUM_STAGE_ACPI) {
        uart_probe_acpi_spcr();
        g_uart_completed_stages |= UART_ENUM_STAGE_ACPI;
    }
    if (stages & UART_ENUM_STAGE_LEGACY) {
        uart_probe_legacy();
        g_uart_completed_stages |= UART_ENUM_STAGE_LEGACY;
    }
    if (stages & UART_ENUM_STAGE_PCI) {
        uart_probe_pci();
        g_uart_completed_stages |= UART_ENUM_STAGE_PCI;
    }
}

static void uart_select_active_device(void)
{
    uart_device* best = g_uart_active;

    for (size_t i = 0; i < g_uart_device_count; ++i) {
        uart_device* dev = &g_uart_devices[i];
        if (!dev->present) {
            continue;
        }
        if (!best) {
            best = dev;
            continue;
        }
        if (dev->preferred && !best->preferred) {
            best = dev;
            continue;
        }
        if (dev->is_console && !best->is_console) {
            best = dev;
            continue;
        }
        if (dev->priority > best->priority) {
            best = dev;
            continue;
        }
    }

    g_uart_active = best;
}

void uart_open(void)
{
    if (g_uart_initialized) {
        return;
    }

    uart_discover(UART_ENUM_STAGE_ACPI | UART_ENUM_STAGE_LEGACY);
    uart_select_active_device();

    if (!g_uart_active) {
        WARN("UART: no active device detected");
        return;
    }

    uart_device_configure(g_uart_active);
    g_uart_initialized = g_uart_active->configured;
}

void uart_close(void)
{
    if (!g_uart_active) {
        return;
    }

    uart_reg_write(g_uart_active, UART_REG_IER, 0x00);
    uart_reg_write(g_uart_active, UART_REG_FCR, 0x00);
    uart_reg_write(g_uart_active, UART_REG_MCR, 0x00);
    g_uart_initialized = false;
}

static void uart_wait_transmit_ready(const uart_device* dev)
{
    while ((uart_reg_read(dev, UART_REG_LSR) & UART_LSR_THR_EMPTY) == 0) {
        asm volatile("pause");
    }
}

void uart_write_char(char c)
{
    if (!g_uart_active || !g_uart_active->present) {
        return;
    }

    uart_wait_transmit_ready(g_uart_active);
    if (c == '\n') {
        uart_reg_write(g_uart_active, UART_REG_THR, '\r');
        uart_wait_transmit_ready(g_uart_active);
    }
    uart_reg_write(g_uart_active, UART_REG_THR, (uint8_t)c);
}

void uart_write_string(const char* str)
{
    if (!str) {
        return;
    }
    while (*str) {
        uart_write_char(*str++);
    }
}

void uart_print(const char* str)
{
    uart_write_string(str);
}

void uart_printf(const char* format, ...)
{
    if (!format) {
        return;
    }
    va_list args;
    va_start(args, format);
    vprintf(uart_write_char, format, args);
    va_end(args);
}

bool uart_supported(void)
{
    uart_discover(UART_ENUM_STAGE_ACPI | UART_ENUM_STAGE_LEGACY);
    uart_select_active_device();
    return g_uart_active && g_uart_active->present;
}

size_t uart_get_devices(const uart_device_info** out_devices)
{
    uart_discover(UART_ENUM_STAGE_ACPI | UART_ENUM_STAGE_LEGACY | UART_ENUM_STAGE_PCI);
    uart_sync_public_info();
    if (out_devices) {
        *out_devices = g_uart_info_cache;
    }
    return g_uart_info_cache_count;
}

const uart_device_info* uart_get_active_device(void)
{
    uart_sync_public_info();
    if (!g_uart_active) {
        return NULL;
    }
    size_t index = (size_t)(g_uart_active - g_uart_devices);
    if (index >= g_uart_info_cache_count) {
        return NULL;
    }
    return &g_uart_info_cache[index];
}

bool uart_select_device(size_t index)
{
    if (index >= g_uart_device_count) {
        return false;
    }
    uart_device* dev = &g_uart_devices[index];
    if (!dev->present) {
        return false;
    }

    g_uart_active = dev;
    if (g_uart_initialized) {
        uart_device_configure(dev);
    }
    return true;
}

void uart_refresh_devices(void)
{
    uart_discover(UART_ENUM_STAGE_ACPI | UART_ENUM_STAGE_LEGACY | UART_ENUM_STAGE_PCI);
    uart_select_active_device();
    if (g_uart_active && (g_uart_initialized || !g_uart_active->configured)) {
        uart_device_configure(g_uart_active);
        g_uart_initialized = g_uart_active->configured;
    }
}

OutputStream uartOutputStream = {
    .Open = uart_open,
    .Close = uart_close,
    .WriteChar = uart_write_char,
    .WriteString = uart_write_string,
    .print = uart_print,
    .printf = uart_printf
};

DebugStream uartDebugStream = {
    .Open = uart_open,
    .Close = uart_close,
    .WriteChar = uart_write_char,
    .WriteString = uart_write_string,
    .print = uart_print,
    .printf = uart_printf
};
