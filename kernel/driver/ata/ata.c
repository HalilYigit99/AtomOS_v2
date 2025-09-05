// Legacy ATA (PATA/ATAPI) driver with PIO support for HDD/SSD and CD/DVD
#include <driver/DriverBase.h>
#include <driver/ata/ata.h>
#include <arch.h>
#include <debug/debug.h>
#include <memory/memory.h>
#include <storage/BlockDevice.h>
#include <pci/PCI.h>
#include <irq/IRQ.h>

static ata_device_t s_ata_devs[4]; // primary: master/slave, secondary: master/slave
static BlockDevice* s_ata_blkdevs[4];
static bool s_ata_controller_present = false;
static volatile uint8_t s_ata_irq_event[2] = {0,0}; // [0]=primary (IRQ14), [1]=secondary (IRQ15)

typedef struct {
    uint16_t io_base;
    uint16_t ctrl_base;
    uint8_t  irq_compat; // 14 or 15 in compatibility mode; 0xFF otherwise
    uint16_t bm_base;     // Bus Master IDE base for this channel (0 if unavailable)
    ata_prd_t* prdt;      // PRD table (virt == phys under identity mapping)
} ata_channel_t;

static ata_channel_t s_channels[2] = {
    { ATA_PRIM_IO, ATA_PRIM_CTRL, 14, 0, NULL },
    { ATA_SEC_IO,  ATA_SEC_CTRL,  15, 0, NULL }
};

static uint16_t s_bmide_base = 0; // BAR4 (I/O)

static inline void ata_delay_400ns(uint16_t ctrl_base)
{
    // 400ns delay: read Alternate Status port 4 times
    (void)inb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS));
    (void)inb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS));
    (void)inb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS));
    (void)inb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS));
}

static inline uint8_t ata_status(uint16_t io_base)
{
    return inb((uint16_t)(io_base + ATA_REG_STATUS));
}

static inline int ata_channel_from_io(uint16_t io_base)
{
    if (io_base == ATA_PRIM_IO) return 0;
    if (io_base == ATA_SEC_IO) return 1;
    return -1;
}

static void ata_channel_soft_reset(uint16_t ctrl_base)
{
    // Assert SRST (bit2) then deassert, with required delays
    outb((uint16_t)(ctrl_base + ATA_REG_DEVCTRL), ATA_DEVCTRL_SRST | ATA_DEVCTRL_NIEN);
    ata_delay_400ns(ctrl_base);
    for (volatile int i = 0; i < 100000; ++i) (void)inb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS));
    outb((uint16_t)(ctrl_base + ATA_REG_DEVCTRL), 0x00);
    // Allow device to settle
    for (volatile int i = 0; i < 100000; ++i) (void)inb((uint16_t)(ctrl_base + ATA_REG_ALTSTATUS));
}

// --- PCI discovery for legacy IDE controllers (PIIX3/PIIX4 and others) ---
static void ata_setup_channels_from_pci(void)
{
    // Default to legacy fixed I/O ports
    s_channels[0].io_base = ATA_PRIM_IO; s_channels[0].ctrl_base = ATA_PRIM_CTRL; s_channels[0].irq_compat = 14;
    s_channels[1].io_base = ATA_SEC_IO;  s_channels[1].ctrl_base = ATA_SEC_CTRL;  s_channels[1].irq_compat = 15;

    PCI_Init();
    PCIDevice* ide = PCI_FindByClass(0x01 /*Mass Storage*/, 0x01 /*IDE*/, -1);
    if (!ide) return; // Keep defaults

    // Ensure I/O and BusMaster enable bits are set
    PCI_EnableIOAndMemory(ide);
    PCI_EnableBusMastering(ide);

    // Decode programming interface: native vs compatibility mode
    uint8_t prog = ide->progIF;
    bool prim_native = (prog & 0x01) != 0;
    bool sec_native  = (prog & 0x04) != 0;
    // BAR0/1 -> primary command/control (I/O), BAR2/3 -> secondary, BAR4 -> Bus Master IDE
    if (prim_native && ide->barCount >= 2 && ide->bars[0].isIO && ide->bars[1].isIO && ide->bars[0].address) {
        s_channels[0].io_base = (uint16_t)ide->bars[0].address;
        s_channels[0].ctrl_base = (uint16_t)ide->bars[1].address;
        s_channels[0].irq_compat = 0xFF; // PCI INTx in native mode (we will use polling for now)
        LOG("ATA: Primary channel native I/O @ %x ctrl @ %x", s_channels[0].io_base, s_channels[0].ctrl_base);
    }
    if (sec_native && ide->barCount >= 4 && ide->bars[2].isIO && ide->bars[3].isIO && ide->bars[2].address) {
        s_channels[1].io_base = (uint16_t)ide->bars[2].address;
        s_channels[1].ctrl_base = (uint16_t)ide->bars[3].address;
        s_channels[1].irq_compat = 0xFF; // PCI INTx in native mode
        LOG("ATA: Secondary channel native I/O @ %x ctrl @ %x", s_channels[1].io_base, s_channels[1].ctrl_base);
    }

    // Bus Master IDE (BAR4) for DMA
    if (ide->barCount >= 5 && ide->bars[4].isIO && ide->bars[4].address) {
        s_bmide_base = (uint16_t)ide->bars[4].address;
        s_channels[0].bm_base = s_bmide_base + 0x00;
        s_channels[1].bm_base = s_bmide_base + ATA_BM_CH_SECONDARY;
        // Allocate small PRDT for each channel (up to 4 entries suffices for <= 256 KiB, 64KiB boundary aware)
        s_channels[0].prdt = (ata_prd_t*)malloc_aligned(16, sizeof(ata_prd_t) * 4);
        s_channels[1].prdt = (ata_prd_t*)malloc_aligned(16, sizeof(ata_prd_t) * 4);
        LOG("ATA: BMIDE present at %x (PRDT allocated)", s_bmide_base);
    } else {
        LOG("ATA: BMIDE (BAR4) not present; using PIO only");
    }
}

static inline uint16_t ata_bm_reg_cmd(uint8_t ch)    { return (uint16_t)(s_channels[ch].bm_base + ATA_BM_REG_CMD); }
static inline uint16_t ata_bm_reg_stat(uint8_t ch)   { return (uint16_t)(s_channels[ch].bm_base + ATA_BM_REG_STATUS); }
static inline uint16_t ata_bm_reg_prdt(uint8_t ch)   { return (uint16_t)(s_channels[ch].bm_base + ATA_BM_REG_PRDT); }

static uint32_t ata_build_prdt(uint8_t ch, void* buf, uint32_t bytes)
{
    ata_prd_t* prdt = s_channels[ch].prdt;
    if (!prdt) return 0;
    uint32_t built = 0;
    uint32_t remaining = bytes;
    uintptr_t p = (uintptr_t)buf; // phys==virt
    int idx = 0;
    while (remaining && idx < 4) {
        // Do not cross 64 KiB boundary per PRD entry
        uint32_t offset_in_64k = (uint32_t)(p & 0xFFFFu);
        uint32_t space = 0x10000u - offset_in_64k;
        uint32_t chunk = (remaining < space) ? remaining : space;
        // PRD count: 0 means 64KiB
        prdt[idx].base = (uint32_t)p;
        prdt[idx].byte_count = (uint16_t)((chunk & 0xFFFFu) ? (chunk & 0xFFFFu) : 0);
        prdt[idx].flags = 0x0000;
        built += chunk;
        remaining -= chunk;
        p += chunk;
        idx++;
    }
    if (idx == 0) return 0;
    prdt[idx - 1].flags |= 0x8000; // EOT
    return built;
}

static bool ata_dma_rw(ata_device_t* dev, uint64_t lba, uint16_t sects, void* buffer, bool is_write)
{
    int ch = ata_channel_from_io(dev->io_base);
    if (ch < 0) return false;
    if (s_channels[ch].bm_base == 0 || s_channels[ch].prdt == NULL) return false;

    uint32_t bytes = (uint32_t)sects * 512u;
    uint32_t prepared = ata_build_prdt((uint8_t)ch, buffer, bytes);
    if (prepared != bytes) return false;

    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    // Program PRDT base
    outl(ata_bm_reg_prdt((uint8_t)ch), (uint32_t)(uintptr_t)s_channels[ch].prdt);

    // Clear BM status (write 1 to clear IRQ and ERR)
    uint8_t st = inb(ata_bm_reg_stat((uint8_t)ch));
    outb(ata_bm_reg_stat((uint8_t)ch), (uint8_t)(st | ATA_BM_ST_IRQ | ATA_BM_ST_ERR));

    // Prepare drive registers
    if (dev->lba48_supported) {
        // Select drive
        outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4)));
        ata_delay_400ns(ctl);
        // High bytes first
        outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)((sects >> 8) & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)((lba >> 24) & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 32) & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 40) & 0xFF));
        // Low bytes
        outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)(sects & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    } else {
        uint32_t lba28 = (uint32_t)lba;
        outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4) | ((lba28 >> 24) & 0x0F)));
        ata_delay_400ns(ctl);
        outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)(sects & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba28 & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba28 >> 8) & 0xFF));
        outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba28 >> 16) & 0xFF));
    }

    // Set BM command (direction + start)
    uint8_t cmd = inb(ata_bm_reg_cmd((uint8_t)ch));
    cmd &= ~ATA_BM_CMD_WRITE;
    if (is_write) cmd |= ATA_BM_CMD_WRITE; // direction
    outb(ata_bm_reg_cmd((uint8_t)ch), cmd);

    // Start BM DMA engine
    outb(ata_bm_reg_cmd((uint8_t)ch), (uint8_t)(cmd | ATA_BM_CMD_START));

    // Issue ATA command
    if (dev->lba48_supported) {
        outb((uint16_t)(io + ATA_REG_COMMAND), is_write ? ATA_CMD_WRITE_DMA_EXT : ATA_CMD_READ_DMA_EXT);
    } else {
        outb((uint16_t)(io + ATA_REG_COMMAND), is_write ? ATA_CMD_WRITE_DMA : ATA_CMD_READ_DMA);
    }

    // Wait for completion by polling BM status IRQ or ATA status
    uint32_t spin = 5000000;
    bool ok = false;
    while (spin--) {
        uint8_t bst = inb(ata_bm_reg_stat((uint8_t)ch));
        if (bst & ATA_BM_ST_ERR) { ok = false; break; }
        if (bst & ATA_BM_ST_IRQ) { ok = true; break; }
    }

    // Stop BM DMA engine
    cmd = inb(ata_bm_reg_cmd((uint8_t)ch));
    outb(ata_bm_reg_cmd((uint8_t)ch), (uint8_t)(cmd & ~ATA_BM_CMD_START));

    // Clear IRQ and check device status
    uint8_t bst = inb(ata_bm_reg_stat((uint8_t)ch));
    outb(ata_bm_reg_stat((uint8_t)ch), (uint8_t)(bst | ATA_BM_ST_IRQ | ATA_BM_ST_ERR));

    uint8_t st2 = inb((uint16_t)(io + ATA_REG_STATUS));
    if (st2 & (ATA_SR_ERR | ATA_SR_DF)) ok = false;
    return ok;
}

// --- Identify device (ATA or ATAPI) ---
static bool ata_identify(ata_device_t* dev)
{
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;
    uint8_t drvsel = (uint8_t)(0xA0 | (dev->drive << 4));

    LOG("ATA: identify start io=%x ctl=%x drive=%u", io, ctl, dev->drive);
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), drvsel);
    ata_delay_400ns(ctl);
    // Read initial status to detect floating bus
    uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
    LOG("ATA: status after drive select = 0x%02x", st);
    if (st == 0xFF) { LOG("ATA: floating bus (no device)"); return false; }

    // Poll for BSY clear
    uint32_t spin = 1000000;
    while ((st & ATA_SR_BSY) && spin--) st = ata_status(io);
    if (st & ATA_SR_BSY) { LOG("ATA: timeout waiting BSY clear (st=0x%02x)", st); return false; }

    // Detect device type via LBA1/LBA2 signature (after BSY clear)
    uint8_t lba1 = inb((uint16_t)(io + ATA_REG_LBA1));
    uint8_t lba2 = inb((uint16_t)(io + ATA_REG_LBA2));
    if (lba1 == ATA_SIG_ATAPI_LBA1 && lba2 == ATA_SIG_ATAPI_LBA2) {
        dev->type = ATA_TYPE_ATAPI;
        outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_IDENTIFY_PACKET);
    } else {
        dev->type = ATA_TYPE_ATA;
        outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_IDENTIFY);
    }

    // Wait for DRQ
    spin = 1000000;
    while (((st = ata_status(io)) & (ATA_SR_BSY | ATA_SR_DRQ)) != ATA_SR_DRQ && spin--) {
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return false;
    }
    if ((st & ATA_SR_DRQ) == 0) { LOG("ATA: DRQ not set (st=0x%02x)", st); return false; }

    // Read IDENTIFY data
    for (int i = 0; i < 256; ++i) dev->identify[i] = inw((uint16_t)(io + ATA_REG_DATA));

    // Parse sector size and total sectors
    dev->sector_size = 512;
    if (dev->type == ATA_TYPE_ATA) {
        uint16_t w106 = dev->identify[106];
        if (w106 & (1u << 12)) {
            uint32_t sz = ((uint32_t)dev->identify[118] << 16) | dev->identify[117];
            if (sz >= 512 && (sz % 512) == 0) dev->sector_size = sz;
        }
        // Total sectors
        uint32_t lba28 = ((uint32_t)dev->identify[61] << 16) | dev->identify[60];
        dev->lba48_supported = (dev->identify[83] & (1u << 10)) != 0;
        uint64_t lba48_cnt = 0;
        if (dev->lba48_supported) {
            lba48_cnt = ((uint64_t)dev->identify[103] << 48) |
                        ((uint64_t)dev->identify[102] << 32) |
                        ((uint64_t)dev->identify[101] << 16) |
                        ((uint64_t)dev->identify[100]);
        }
        dev->total_sectors = dev->lba48_supported ? lba48_cnt : lba28;
    } else {
        dev->lba48_supported = false;
        dev->total_sectors = 0; // Will be filled via READ CAPACITY(10)
    }
    return true;
}

static void ata_probe_channel(uint16_t io_base, uint16_t ctrl_base, uint8_t ch)
{
    // Soft reset the channel to ensure a sane starting state
    ata_channel_soft_reset(ctrl_base);
    for (uint8_t drv = 0; drv < 2; ++drv) {
        ata_device_t* d = &s_ata_devs[ch * 2 + drv];
        d->present = false;
        d->type = ATA_TYPE_NONE;
        d->io_base = io_base;
        d->ctrl_base = ctrl_base;
        d->drive = drv;

        if (ata_identify(d)) {
            d->present = true;
            const char* t = (d->type == ATA_TYPE_ATAPI) ? "ATAPI" : "ATA";
            LOG("ATA: %s device at %s %s", t,
                ch == 0 ? "primary" : "secondary",
                drv == 0 ? "master" : "slave");
            LOG("ATA: sectors=%u sector_size=%u", (unsigned)d->total_sectors, d->sector_size);
        }
    }
}

// --- PIO helpers (28-bit only for now) ---
static bool ata_wait_not_busy(uint16_t io, uint32_t spin)
{
    uint8_t st;
    do { st = inb((uint16_t)(io + ATA_REG_STATUS)); } while ((st & ATA_SR_BSY) && spin--);
    return (st & ATA_SR_BSY) == 0;
}

static bool ata_wait_drq_set(uint16_t io, uint32_t spin)
{
    uint8_t st;
    int ch = ata_channel_from_io(io);
    do {
        st = inb((uint16_t)(io + ATA_REG_STATUS));
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return false;
        if (st & ATA_SR_DRQ) return true;
        // If IRQ signaled for this channel, recheck quickly
        if (ch >= 0 && s_ata_irq_event[ch]) {
            s_ata_irq_event[ch] = 0;
            st = inb((uint16_t)(io + ATA_REG_STATUS));
            if (st & ATA_SR_DRQ) return true;
        }
    } while (spin--);
    return false;
}

static bool ata_pio_read28(ata_device_t* dev, uint32_t lba, uint8_t count, void* buffer)
{
    if (count == 0) return true;
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    // Select drive/head with LBA and high LBA nibble
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F)));
    ata_delay_400ns(ctl);

    outb((uint16_t)(io + ATA_REG_SECCOUNT0), count);
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_READ_SECTORS);

    uint16_t* out = (uint16_t*)buffer;
    for (uint8_t s = 0; s < count; ++s) {
        if (!ata_wait_not_busy(io, 1000000)) return false;
        if (!ata_wait_drq_set(io, 1000000)) return false;
        for (int i = 0; i < 256; ++i) {
            out[i] = inw((uint16_t)(io + ATA_REG_DATA));
        }
        out += 256;
    }
    return true;
}

static bool ata_pio_read48(ata_device_t* dev, uint64_t lba, uint16_t count, void* buffer)
{
    if (count == 0) return true;
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    // Select drive (LBA bit set)
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4)));
    ata_delay_400ns(ctl);

    // High bytes first
    outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)((count >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)((lba >> 24) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 32) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 40) & 0xFF));

    // Low bytes
    outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)(count & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));

    // READ SECTORS EXT (PIO)
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_READ_SECTORS_EXT);

    uint16_t* out = (uint16_t*)buffer;
    for (uint16_t s = 0; s < count; ++s) {
        if (!ata_wait_not_busy(io, 1000000)) return false;
        if (!ata_wait_drq_set(io, 1000000)) return false;
        for (int i = 0; i < 256; ++i) {
            out[i] = inw((uint16_t)(io + ATA_REG_DATA));
        }
        out += 256;
    }
    return true;
}

static bool ata_pio_write28(ata_device_t* dev, uint32_t lba, uint8_t count, const void* buffer)
{
    if (count == 0) return true;
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F)));
    ata_delay_400ns(ctl);

    outb((uint16_t)(io + ATA_REG_SECCOUNT0), count);
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_WRITE_SECTORS);

    const uint16_t* in = (const uint16_t*)buffer;
    for (uint8_t s = 0; s < count; ++s) {
        if (!ata_wait_not_busy(io, 1000000)) return false;
        if (!ata_wait_drq_set(io, 1000000)) return false;
        for (int i = 0; i < 256; ++i) {
            outw((uint16_t)(io + ATA_REG_DATA), in[i]);
        }
        in += 256;
    }
    return true;
}

static bool ata_pio_write48(ata_device_t* dev, uint64_t lba, uint16_t count, const void* buffer)
{
    if (count == 0) return true;
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    // Select drive (LBA bit set)
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4)));
    ata_delay_400ns(ctl);

    // High bytes first
    outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)((count >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)((lba >> 24) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 32) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 40) & 0xFF));

    // Low bytes
    outb((uint16_t)(io + ATA_REG_SECCOUNT0), (uint8_t)(count & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA0), (uint8_t)(lba & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)((lba >> 8) & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((lba >> 16) & 0xFF));

    // WRITE SECTORS EXT (PIO)
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_WRITE_SECTORS_EXT);

    const uint16_t* in = (const uint16_t*)buffer;
    for (uint16_t s = 0; s < count; ++s) {
        if (!ata_wait_not_busy(io, 1000000)) return false;
        if (!ata_wait_drq_set(io, 1000000)) return false;
        for (int i = 0; i < 256; ++i) {
            outw((uint16_t)(io + ATA_REG_DATA), in[i]);
        }
        in += 256;
    }
    return true;
}

// IRQ handlers for primary (IRQ14) and secondary (IRQ15) channels
void ata_irq14(void)
{
    // Read status to acknowledge device interrupt, then flag event
    (void)inb((uint16_t)(ATA_PRIM_IO + ATA_REG_STATUS));
    s_ata_irq_event[0] = 1;
    if (irq_controller && irq_controller->acknowledge) irq_controller->acknowledge(14);
}

void ata_irq15(void)
{
    (void)inb((uint16_t)(ATA_SEC_IO + ATA_REG_STATUS));
    s_ata_irq_event[1] = 1;
    if (irq_controller && irq_controller->acknowledge) irq_controller->acknowledge(15);
}

// ---- ATAPI support (PIO) ----
static bool ata_atapi_packet_cmd(ata_device_t* dev, const uint8_t* cdb, uint32_t cdb_len, void* buf, uint32_t byte_count, bool is_write)
{
    if (!dev || dev->type != ATA_TYPE_ATAPI) return false;
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    // Select drive
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xA0 | (dev->drive << 4)));
    ata_delay_400ns(ctl);

    // Set byte count (Cylinder Low/High)
    // Clamp to 0xFFFF (some devices interpret 0 as 65536, using 0xFFFF is safe)
    uint32_t bc32 = byte_count;
    if (bc32 == 0 || bc32 > 0xFFFFu) bc32 = 0xFFFFu;
    uint16_t bc = (uint16_t)bc32;
    outb((uint16_t)(io + ATA_REG_FEATURES), 0x00);
    outb((uint16_t)(io + ATA_REG_LBA1), (uint8_t)(bc & 0xFF));
    outb((uint16_t)(io + ATA_REG_LBA2), (uint8_t)((bc >> 8) & 0xFF));

    // PACKET command
    outb((uint16_t)(io + ATA_REG_COMMAND), ATA_CMD_PACKET);

    // Wait for DRQ
    if (!ata_wait_not_busy(io, 1000000)) return false;
    if (!ata_wait_drq_set(io, 2000000)) return false;

    // Write CDB (12 or 16 bytes) to data port as words
    uint16_t cdb_words = (uint16_t)((cdb_len + 1) / 2);
    for (uint16_t i = 0; i < cdb_words; ++i) {
        uint16_t w = (uint16_t)cdb[i * 2];
        if (((uint32_t)i * 2u + 1u) < cdb_len) w |= (uint16_t)cdb[i * 2 + 1] << 8;
        outw((uint16_t)(io + ATA_REG_DATA), w);
    }

    // If data phase expected, transfer PIO data
    uint8_t* p = (uint8_t*)buf;
    uint32_t remaining = byte_count;
    while (remaining) {
        // Wait for DRQ or completion
        if (!ata_wait_not_busy(io, 1000000)) return false;
        uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return false;
        if ((st & ATA_SR_DRQ) == 0) break; // device may finish with smaller xfer

        // Read device-reported transfer size from LBA1/LBA2
        uint16_t w_lo = (uint16_t)inb((uint16_t)(io + ATA_REG_LBA1));
        uint16_t w_hi = (uint16_t)inb((uint16_t)(io + ATA_REG_LBA2));
        uint32_t words = (uint32_t)w_lo | ((uint32_t)w_hi << 8);
        if (words == 0) words = 0x10000u; // 0 means 65536 words
        uint32_t bytes = words * 2u;
        if (bytes > remaining) bytes = remaining;

        // Perform the data in/out
        if (!is_write) {
            uint16_t* dst = (uint16_t*)p;
            for (uint32_t i = 0; i < bytes/2; ++i) dst[i] = inw((uint16_t)(io + ATA_REG_DATA));
        } else {
            const uint16_t* src = (const uint16_t*)p;
            for (uint32_t i = 0; i < bytes/2; ++i) outw((uint16_t)(io + ATA_REG_DATA), src[i]);
        }
        p += bytes;
        remaining -= bytes;
    }

    // Final status check
    if (!ata_wait_not_busy(io, 1000000)) return false;
    {
        uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
        if (st & (ATA_SR_ERR | ATA_SR_DF)) return false;
    }
    return true;
}

static void ata_atapi_request_sense(ata_device_t* dev)
{
    uint8_t sense[18];
    memset(sense, 0, sizeof(sense));
    uint8_t cdb[12] = {0};
    cdb[0] = ATAPI_CMD_REQUEST_SENSE;
    cdb[4] = (uint8_t)sizeof(sense);
    if (ata_atapi_packet_cmd(dev, cdb, 12, sense, sizeof(sense), false)) {
        uint8_t key = sense[2] & 0x0F;
        uint32_t ascq = ((uint32_t)sense[12] << 8) | sense[13];
        LOG("ATAPI: REQUEST SENSE -> key=%u ASC/ASCQ=0x%04x", key, (unsigned)ascq);
    } else {
        WARN("ATAPI: REQUEST SENSE failed");
    }
}

static bool ata_atapi_read_capacity(ata_device_t* dev, uint32_t* last_lba, uint32_t* block_len)
{
    uint8_t cap[8];
    uint8_t cdb[12] = {0};
    cdb[0] = ATAPI_CMD_READ_CAPACITY10;
    if (!ata_atapi_packet_cmd(dev, cdb, 12, cap, sizeof(cap), false)) return false;
    *last_lba = (uint32_t)cap[0] << 24 | (uint32_t)cap[1] << 16 | (uint32_t)cap[2] << 8 | (uint32_t)cap[3];
    *block_len = (uint32_t)cap[4] << 24 | (uint32_t)cap[5] << 16 | (uint32_t)cap[6] << 8 | (uint32_t)cap[7];
    return true;
}

static bool ata_atapi_read_blocks(ata_device_t* dev, uint32_t lba, uint32_t blocks, void* buf)
{
    if (blocks == 0) return true;
    uint32_t byte_count = blocks * 2048u;
    // Prefer READ(10)
    uint8_t cdb[12] = {0};
    cdb[0] = ATAPI_CMD_READ10;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >> 8) & 0xFF);
    cdb[5] = (uint8_t)(lba & 0xFF);
    cdb[7] = (uint8_t)((blocks >> 8) & 0xFF);
    cdb[8] = (uint8_t)(blocks & 0xFF);
    if (ata_atapi_packet_cmd(dev, cdb, 12, buf, byte_count, false)) return true;

    // Fallback READ(12)
    memset(cdb, 0, sizeof(cdb));
    cdb[0] = ATAPI_CMD_READ12;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >> 8) & 0xFF);
    cdb[5] = (uint8_t)(lba & 0xFF);
    cdb[6] = (uint8_t)((blocks >> 16) & 0xFF);
    cdb[7] = (uint8_t)((blocks >> 8) & 0xFF);
    cdb[8] = (uint8_t)(blocks & 0xFF);
    if (ata_atapi_packet_cmd(dev, cdb, 12, buf, byte_count, false)) return true;
    ata_atapi_request_sense(dev);
    return false;
}

// BlockDevice ops wrappers
static bool ata_blk_read(struct BlockDevice* bdev, uint64_t lba, uint32_t count, void* buf)
{
    ata_device_t* dev = (ata_device_t*)bdev->driver_ctx;
    if (!dev) return false;
    if (dev->type == ATA_TYPE_ATA) {
        if (bdev->logical_block_size != 512) return false;
        if ((lba >> 28) != 0 && !dev->lba48_supported) return false;
        uint8_t* out = (uint8_t*)buf;
        while (count) {
            // Prefer BMIDE DMA when available; fall back to PIO
            uint32_t nmax = dev->lba48_supported ? 65535u : 255u;
            uint32_t n = (count > nmax) ? nmax : count;
            if (s_bmide_base && ata_dma_rw(dev, lba, (uint16_t)n, out, false)) {
                lba += n; out += n * 512u; count -= n;
            } else {
                if (dev->lba48_supported) {
                    uint16_t nn = (uint16_t)n;
                    if (!ata_pio_read48(dev, lba, nn, out)) return false;
                    lba += nn; out += (uint32_t)nn * 512u; count -= nn;
                } else {
                    uint32_t lba32 = (uint32_t)lba;
                    uint8_t nn = (uint8_t)n;
                    if (!ata_pio_read28(dev, lba32, nn, out)) return false;
                    lba32 += nn; lba = lba32; out += (uint32_t)nn * 512u; count -= nn;
                }
            }
        }
        return true;
    } else if (dev->type == ATA_TYPE_ATAPI) {
        if (bdev->logical_block_size != 2048) return false;
        uint8_t* out = (uint8_t*)buf;
        while (count) {
            uint32_t n = (count > 16) ? 16 : count; // reasonable chunk
            if (!ata_atapi_read_blocks(dev, (uint32_t)lba, n, out)) return false;
            lba += n; out += n * 2048u; count -= n;
        }
        return true;
    }
    return false;
}

static bool ata_blk_write(struct BlockDevice* bdev, uint64_t lba, uint32_t count, const void* buf)
{
    ata_device_t* dev = (ata_device_t*)bdev->driver_ctx;
    if (!dev) return false;
    if (dev->type != ATA_TYPE_ATA) return false; // CDROM not supported
    if (bdev->logical_block_size != 512) return false;
    if ((lba >> 28) != 0 && !dev->lba48_supported) return false;
    const uint8_t* in = (const uint8_t*)buf;
    while (count) {
        uint32_t nmax = dev->lba48_supported ? 65535u : 255u;
        uint32_t n = (count > nmax) ? nmax : count;
        if (s_bmide_base && ata_dma_rw(dev, lba, (uint16_t)n, (void*)in, true)) {
            lba += n; in += n * 512u; count -= n;
        } else {
            if (dev->lba48_supported) {
                uint16_t nn = (uint16_t)n;
                if (!ata_pio_write48(dev, lba, nn, in)) return false;
                lba += nn; in += (uint32_t)nn * 512u; count -= nn;
            } else {
                uint32_t lba32 = (uint32_t)lba;
                uint8_t nn = (uint8_t)n;
                if (!ata_pio_write28(dev, lba32, nn, in)) return false;
                lba32 += nn; lba = lba32; in += (uint32_t)nn * 512u; count -= nn;
            }
        }
    }
    return true;
}

static bool ata_blk_flush(struct BlockDevice* bdev)
{
    ata_device_t* dev = (ata_device_t*)bdev->driver_ctx;
    if (!dev) return false;
    if (dev->type != ATA_TYPE_ATA) return true; // nothing to flush on ATAPI
    uint16_t io = dev->io_base;
    uint16_t ctl = dev->ctrl_base;

    // Select drive
    outb((uint16_t)(io + ATA_REG_HDDEVSEL), (uint8_t)(0xE0 | (dev->drive << 4)));
    ata_delay_400ns(ctl);

    // Issue FLUSH CACHE (use EXT if LBA48 supported)
    outb((uint16_t)(io + ATA_REG_COMMAND), dev->lba48_supported ? ATA_CMD_FLUSH_CACHE_EXT : ATA_CMD_FLUSH_CACHE);

    // Poll until not busy and check errors
    if (!ata_wait_not_busy(io, 2000000)) return false;
    uint8_t st = inb((uint16_t)(io + ATA_REG_STATUS));
    return (st & (ATA_SR_ERR | ATA_SR_DF)) == 0;
}

static const BlockDeviceOps s_ata_blk_ops = {
    .read = ata_blk_read,
    .write = ata_blk_write,
    .flush = ata_blk_flush,
};

bool ata_init(void)
{
    // Check PCI for a legacy IDE/ATA controller presence before probing
    PCI_Init();
    PCIDevice* ide = PCI_FindByClass(0x01 /*Mass Storage*/, 0x01 /*IDE*/, -1);
    PCIDevice* ata = PCI_FindByClass(0x01 /*Mass Storage*/, 0x05 /*ATA*/, -1);
    s_ata_controller_present = (ide != NULL) || (ata != NULL);

    if (!s_ata_controller_present) {
        WARN("ATA: No PCI IDE/ATA controller present; skipping legacy PATA probe");
        return false; // init failure as requested
    }

    // Determine channels (native vs compat) and base addresses
    ata_setup_channels_from_pci();

    LOG("ATA: Probing ATA/ATAPI devices");
    // Register legacy IRQ handlers only when in compatibility mode
    if (irq_controller && irq_controller->register_handler && irq_controller->enable) {
        extern void ata_irq14_stub(void);
        extern void ata_irq15_stub(void);
        if (s_channels[0].irq_compat != 0xFF) {
            irq_controller->register_handler(14, ata_irq14_stub);
            irq_controller->enable(14);
        }
        if (s_channels[1].irq_compat != 0xFF) {
            irq_controller->register_handler(15, ata_irq15_stub);
            irq_controller->enable(15);
        }
        LOG("ATA: IRQ handlers configured (compat mode where applicable)");
    } else {
        WARN("ATA: IRQ controller not ready; using polling only");
    }
    ata_probe_channel(s_channels[0].io_base, s_channels[0].ctrl_base, 0);
    ata_probe_channel(s_channels[1].io_base, s_channels[1].ctrl_base, 1);

    // Register found devices as block devices (ATA disks + ATAPI CD/DVD)
    BlockDevice_InitRegistry();
    for (int i = 0; i < 4; ++i) {
        ata_device_t* d = &s_ata_devs[i];
        if (!d->present) continue;
        if (d->type == ATA_TYPE_ATA) {
            const char* name = (i==0) ? "ata0" : (i==1) ? "ata1" : (i==2) ? "ata2" : "ata3";
            uint32_t bsz = d->sector_size ? d->sector_size : 512;
            uint64_t total = d->total_sectors;
            s_ata_blkdevs[i] = BlockDevice_Register(name, BLKDEV_TYPE_DISK, bsz, total, &s_ata_blk_ops, d);
        } else if (d->type == ATA_TYPE_ATAPI) {
            // Discover capacity to report correct geometry
            uint32_t last=0, blen=2048;
            if (!ata_atapi_read_capacity(d, &last, &blen)) {
                blen = 2048; last = 0; // still register; reads will work
            }
            const char* name = (i==0) ? "cd0" : (i==1) ? "cd1" : (i==2) ? "cd2" : "cd3";
            s_ata_blkdevs[i] = BlockDevice_Register(name, BLKDEV_TYPE_CDROM, blen ? blen : 2048, (uint64_t)last + 1u, &s_ata_blk_ops, d);
        }
    }
    return true; // not fatal if no devices
}

void ata_enable(void)
{
    // Only enable if we detected a controller during init
    if (!s_ata_controller_present) {
        WARN("ATA: enable() called but no controller present");
        ata_driver.enabled = false;
        return;
    }
    // Enable device interrupts (clear nIEN) for both channels
    outb((uint16_t)(s_channels[0].ctrl_base + ATA_REG_DEVCTRL), 0x00);
    outb((uint16_t)(s_channels[1].ctrl_base + ATA_REG_DEVCTRL),  0x00);
    ata_driver.enabled = true;
}

void ata_disable(void)
{
    ata_driver.enabled = false;
}

DriverBase ata_driver = (DriverBase){
    .name = "ATA",
    .enabled = false,
    .version = 1,
    .context = NULL,
    .init = ata_init,
    .enable = ata_enable,
    .disable = ata_disable,
    .type = DRIVER_TYPE_STORAGE
};
