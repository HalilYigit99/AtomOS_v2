// AHCI storage driver skeleton compliant with DriverBase
#include <driver/DriverBase.h>
#include <driver/ahci/ahci.h>
#include <pci/PCI.h>
#include <debug/debug.h>
#include <memory/memory.h>
#include <memory/heap.h>
#include <storage/BlockDevice.h>
#include <irq/IRQ.h>

// Local helpers
static const char* sig_to_str(uint32_t sig)
{
    switch (sig) {
        case SATA_SIG_ATA:   return "SATA";
        case SATA_SIG_ATAPI: return "ATAPI";
        case SATA_SIG_SEMB:  return "SEMB";
        case SATA_SIG_PM:    return "PM";
        default:             return "UNKNOWN";
    }
}

typedef struct {
    volatile hba_port_t* port;
    uint8_t port_no;
    void* clb_mem; // 1K aligned
    void* fb_mem;  // 256B aligned
    void* ctba0;   // command table for slot 0 (aligned 128B+)
    BlockDevice* blk; // registered block device
    volatile uint32_t irq_events; // last PxIS observed by IRQ handler
} ahci_port_ctx_t;

static volatile hba_mem_t* s_hba = NULL;
static ahci_port_ctx_t s_ports[32];
static uint8_t s_ahci_irq_line = 0xFF; // legacy INTx line (0..15)

void ahci_irq_isr(void)
{
    if (!s_hba || s_ahci_irq_line == 0xFF) return;
    uint32_t his = s_hba->is;
    if (his) {
        for (uint8_t pi = 0; pi < 32; ++pi) {
            if ((his & (1u << pi)) == 0) continue;
            volatile hba_port_t* pp = &s_hba->ports[pi];
            uint32_t pis = pp->is;
            pp->is = pis; // write-to-clear
            if (pi < 32) s_ports[pi].irq_events |= pis;
        }
        s_hba->is = his; // write-to-clear summary
    }
    if (irq_controller && irq_controller->acknowledge) irq_controller->acknowledge(s_ahci_irq_line);
}

static inline void mmio_wmb(void) { (void)s_hba->is; }

static void ahci_dump_port(volatile hba_port_t* p, uint8_t i, const char* tag)
{
    uint32_t ssts = p->ssts;
    uint8_t det = (uint8_t)(ssts & HBA_SSTS_DET_MASK);
    uint8_t spd = HBA_SSTS_SPD(ssts);
    uint8_t ipm = HBA_SSTS_IPM(ssts);
    LOG("AHCI: Port %u [%s] CMD=0x%08x IS=0x%08x TFD=0x%08x SSTS=0x%08x (DET=%u SPD=%u IPM=%u) SERR=0x%08x SIG=0x%08x CLB=%08x:%08x FB=%08x:%08x",
        i, tag, p->cmd, p->is, p->tfd, ssts, det, spd, ipm, p->serr, p->sig,
        p->clbu, p->clb, p->fbu, p->fb);
}

static void ahci_dump_hba(volatile hba_mem_t* hba, const char* tag)
{
    LOG("AHCI: HBA [%s] CAP=0x%08x GHC=0x%08x IS=0x%08x PI=0x%08x VS=%u.%u",
        tag, hba->cap, hba->ghc, hba->is, hba->pi, (hba->vs >> 16) & 0xFFFF, hba->vs & 0xFFFF);
}

static void ahci_port_stop(volatile hba_port_t* p)
{
    // Clear ST
    p->cmd &= ~HBA_PxCMD_ST;
    // Wait until CR cleared
    {
        uint32_t spin = 1000000;
        while ((p->cmd & HBA_PxCMD_CR) && spin--) {asm volatile ("hlt");}
        if (p->cmd & HBA_PxCMD_CR) WARN("AHCI: port stop timeout (CR still set)");
    }
    // Clear FRE and wait FR cleared
    p->cmd &= ~HBA_PxCMD_FRE;
    {
        uint32_t spin = 1000000;
        while ((p->cmd & HBA_PxCMD_FR) && spin--) {asm volatile ("hlt"); }
        if (p->cmd & HBA_PxCMD_FR) WARN("AHCI: port stop timeout (FR still set)");
    }
}

static void ahci_port_start(volatile hba_port_t* p)
{
    // Power on + spin-up
    p->cmd |= HBA_PxCMD_POD;
    p->cmd |= HBA_PxCMD_SUD;

    // Enable FIS receive and wait FR asserts
    p->cmd |= HBA_PxCMD_FRE;
    {
        uint32_t spin = 1000000;
        while (((p->cmd & HBA_PxCMD_FR) == 0) && spin--) {asm volatile ("hlt"); }
        if ((p->cmd & HBA_PxCMD_FR) == 0) WARN("AHCI: PxCMD.FR did not assert after FRE");
    }

    // Start command processing and wait CR reflects engine state
    p->cmd |= HBA_PxCMD_ST;
    {
        uint32_t spin = 1000000;
        while (((p->cmd & HBA_PxCMD_CR) == 0) && spin--) { asm volatile ("hlt"); }
        // If CR doesn't set immediately it's still ok on some controllers
    }
}

static void ahci_port_comreset(volatile hba_port_t* p)
{
    // Clear errors
    p->serr = 0xFFFFFFFFu;
    // Issue COMRESET: set DET=1 then 0
    uint32_t sctl = p->sctl;
    sctl &= ~0x0Fu; sctl |= 0x1u; p->sctl = sctl;
    for (volatile int i=0;i<200000;i++) (void)p->ssts;
    sctl &= ~0x0Fu; p->sctl = sctl;
    for (volatile int i=0;i<200000;i++) (void)p->ssts;
}

static void ahci_port_recover(ahci_port_ctx_t* ctx, const char* tag)
{
    volatile hba_port_t* p = ctx->port;
    LOG("AHCI: Port %u recover begin (%s)", ctx->port_no, tag ? tag : "");
    // Clear interrupts and errors
    p->is = 0xFFFFFFFFu;
    p->serr = 0xFFFFFFFFu;
    mmio_wmb();
    // Short settle
    for (volatile int i = 0; i < 200000; ++i) (void)p->ssts;

    // If bus appears wedged (CI still set later), perform light engine restart
    uint32_t cmd = p->cmd;
    if ((cmd & (HBA_PxCMD_ST | HBA_PxCMD_FRE)) != 0) {
        // Stop engine
        p->cmd &= ~HBA_PxCMD_ST;
        {
            uint32_t spin = 1000000; while ((p->cmd & HBA_PxCMD_CR) && spin--) { asm volatile ("hlt"); }
        }
        p->cmd &= ~HBA_PxCMD_FRE;
        {
            uint32_t spin = 1000000; while ((p->cmd & HBA_PxCMD_FR) && spin--) { asm volatile ("hlt"); }
        }
        // Restart
        p->is = 0xFFFFFFFFu; p->serr = 0xFFFFFFFFu; mmio_wmb();
        for (volatile int i = 0; i < 100000; ++i) (void)p->ssts;
        p->cmd |= HBA_PxCMD_FRE;
        p->cmd |= HBA_PxCMD_ST;
    }
    ahci_dump_port(p, ctx->port_no, "after-recover");
}

static bool ahci_port_configure(ahci_port_ctx_t* ctx)
{
    volatile hba_port_t* p = ctx->port;
    ahci_port_stop(p);

    // Allocate CLB (1K aligned) and FB (256B aligned)
    ctx->clb_mem = heap_aligned_alloc(1024, 1024);
    ctx->fb_mem  = heap_aligned_alloc(256, 256);
    if (!ctx->clb_mem || !ctx->fb_mem) return false;
    memset(ctx->clb_mem, 0, 1024);
    memset(ctx->fb_mem, 0, 256);
    uintptr_t clb = (uintptr_t)ctx->clb_mem;
    uintptr_t fb  = (uintptr_t)ctx->fb_mem;
    p->clb = (uint32_t)((uint64_t)clb & 0xFFFFFFFFu);
    p->clbu = (uint32_t)(((uint64_t)clb >> 32) & 0xFFFFFFFFu);
    p->fb  = (uint32_t)((uint64_t)fb & 0xFFFFFFFFu);
    p->fbu = (uint32_t)(((uint64_t)fb >> 32) & 0xFFFFFFFFu);

    // Command header for slot 0
    hba_cmd_header_t* hdr = (hba_cmd_header_t*)ctx->clb_mem;
    memset(hdr, 0, sizeof(hba_cmd_header_t));
    hdr->prdtl = 1; // single PRDT

    // Command table (align to 128)
    ctx->ctba0 = heap_aligned_alloc(128, sizeof(hba_cmd_table_t));
    if (!ctx->ctba0) return false;
    memset(ctx->ctba0, 0, sizeof(hba_cmd_table_t));
    uintptr_t ctba = (uintptr_t)ctx->ctba0;
    hdr->ctba = (uint32_t)((uint64_t)ctba & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)(((uint64_t)ctba >> 32) & 0xFFFFFFFFu);

    // Clear pending interrupts
    p->is = 0xFFFFFFFFu;

    ahci_port_start(p);
    ahci_dump_port(p, ctx->port_no, "after-start");
    return true;
}

static bool ahci_read_sector(ahci_port_ctx_t* ctx, uint64_t lba, uint32_t count, void* buf)
{
    if (count == 0) return true;
    volatile hba_port_t* p = ctx->port;
    // Wait if busy (bounded)
    {
        uint32_t spin = 1000000;
        while ((p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) && spin--) { asm volatile ("pause"); }
        if (p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) {
            ERROR("AHCI: Port %u busy before READ DMA (TFD=0x%08x)", ctx->port_no, p->tfd);
            return false;
        }
    }

    // Build command in slot 0
    hba_cmd_header_t* hdr = (hba_cmd_header_t*)ctx->clb_mem;
    // Program CTBA in case controller expects it each time
    uintptr_t ctba = (uintptr_t)ctx->ctba0;
    hdr->ctba  = (uint32_t)((uint64_t)ctba & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)(((uint64_t)ctba >> 32) & 0xFFFFFFFFu);
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4; // FIS length in dwords
    hdr->a = 0; // ATA
    hdr->w = 0; // read
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    hba_cmd_table_t* tbl = (hba_cmd_table_t*)ctx->ctba0;
    memset(tbl, 0, sizeof(hba_cmd_table_t));

    // PRDT sized by logical block size (default 512 when unknown)
    uintptr_t bufp = (uintptr_t)buf;
    tbl->prdt[0].dba = (uint32_t)((uint64_t)bufp & 0xFFFFFFFFu);
    tbl->prdt[0].dbau = (uint32_t)(((uint64_t)bufp >> 32) & 0xFFFFFFFFu);
    uint32_t bsz = (ctx->blk && ctx->blk->logical_block_size) ? ctx->blk->logical_block_size : 512u;
    uint32_t byte_count = count * bsz;
    tbl->prdt[0].dbc_i = ((byte_count - 1) & 0x003FFFFFu) | (1u << 31); // ioc=1

    // CFIS: READ DMA EXT (0x25)
    fis_reg_h2d_t* cfis = (fis_reg_h2d_t*)tbl->cfis;
    memset(cfis, 0, sizeof(*cfis));
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = 0x25; // READ DMA EXT
    cfis->device = 1 << 6; // LBA mode
    // LBA48
    cfis->lba0 = (uint8_t)(lba & 0xFF);
    cfis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    cfis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    cfis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    cfis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    cfis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
    cfis->countl = (uint8_t)(count & 0xFF);
    cfis->counth = (uint8_t)((count >> 8) & 0xFF);

    // Issue command on slot 0
    p->is = 0xFFFFFFFFu; // clear
    mmio_wmb();
    p->ci = 1u; // slot 0

    // Wait for completion: prefer IRQ event, fall back to CI polling
    {
        uint32_t spin = 5000000; // generous spin
        while (spin--) {
            if ((p->ci & 1u) == 0) break; // done
            if (ctx->irq_events) break;   // IRQ signaled
            if (p->is & HBA_PxIS_TFES) {
                ERROR("AHCI: TFES error on port %u (IS=0x%08x TFD=0x%08x)", ctx->port_no, p->is, p->tfd);
                return false;
            }
            asm volatile ("pause");  
        }
        // Clear any latched irq events
        ctx->irq_events = 0;
        if (p->ci & 1u) {
            ERROR("AHCI: READ DMA timeout on port %u (IS=0x%08x TFD=0x%08x)", ctx->port_no, p->is, p->tfd);
            return false;
        }
    }
    // success
    LOG("AHCI: READ DMA ok port %u lba=%llu count=%u PRDBC=%u", ctx->port_no, (unsigned long long)lba, count, ((hba_cmd_header_t*)ctx->clb_mem)->prdbc);
    return true;
}

static bool ahci_issue_flush(ahci_port_ctx_t* ctx, uint8_t opcode)
{
    volatile hba_port_t* p = ctx->port;
    // Wait if busy
    {
        uint32_t spin = 1000000;
        while ((p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) && spin--) { asm volatile ("pause"); }
        if (p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) return false;
    }

    hba_cmd_header_t* hdr = (hba_cmd_header_t*)ctx->clb_mem;
    uintptr_t ctba = (uintptr_t)ctx->ctba0;
    hdr->ctba  = (uint32_t)((uint64_t)ctba & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)(((uint64_t)ctba >> 32) & 0xFFFFFFFFu);
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    hdr->a = 0; // ATA
    hdr->w = 0;
    hdr->prdtl = 0; // no data
    hdr->prdbc = 0;

    hba_cmd_table_t* tbl = (hba_cmd_table_t*)ctx->ctba0;
    memset(tbl, 0, sizeof(hba_cmd_table_t));

    fis_reg_h2d_t* cfis = (fis_reg_h2d_t*)tbl->cfis;
    memset(cfis, 0, sizeof(*cfis));
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = opcode; // 0xEA FLUSH CACHE EXT or 0xE7 FLUSH CACHE
    cfis->device = 1 << 6; // LBA mode

    p->is = 0xFFFFFFFFu;
    mmio_wmb();
    p->ci = 1u;

    uint32_t spin = 5000000;
    while (spin--) {
        if ((p->ci & 1u) == 0) break;
        if (ctx->irq_events) break;
        if (p->is & HBA_PxIS_TFES) return false;
        asm volatile ("pause"); 
    }
    ctx->irq_events = 0;
    if (p->ci & 1u) return false;
    return true;
}

static bool ahci_blk_read(struct BlockDevice* bdev, uint64_t lba, uint32_t count, void* buffer)
{
    ahci_port_ctx_t* ctx = (ahci_port_ctx_t*)bdev->driver_ctx;
    // Read in chunks if count is large (limit to 128 sectors per command)
    uint8_t* out = (uint8_t*)buffer;
    while (count) {
        uint32_t n = (count > 128) ? 128 : count;
        if (!ahci_read_sector(ctx, lba, n, out)) return false;
        lba += n; out += n * 512; count -= n;
        asm volatile ("pause"); 
    }
    return true;
}

static bool ahci_blk_write(struct BlockDevice* bdev, uint64_t lba, uint32_t count, const void* buffer)
{
    ahci_port_ctx_t* ctx = (ahci_port_ctx_t*)bdev->driver_ctx;
    // Write in chunks (limit to 128 sectors per command)
    const uint8_t* in = (const uint8_t*)buffer;
    while (count) {
        uint32_t n = (count > 128) ? 128 : count;
        // Issue WRITE DMA EXT
        if (!ctx) return false;
        volatile hba_port_t* p = ctx->port;

        // Wait if busy
        {
            uint32_t spin = 1000000;
            while ((p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) && spin--) { asm volatile ("hlt"); }
            if (p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) {
                ERROR("AHCI: Port %u busy before WRITE DMA (TFD=0x%08x)", ctx->port_no, p->tfd);
                return false;
            }
        }

        hba_cmd_header_t* hdr = (hba_cmd_header_t*)ctx->clb_mem;
        // Ensure CTBA is programmed (some controllers require this per command)
        uintptr_t ctba = (uintptr_t)ctx->ctba0;
        hdr->ctba  = (uint32_t)((uint64_t)ctba & 0xFFFFFFFFu);
        hdr->ctbau = (uint32_t)(((uint64_t)ctba >> 32) & 0xFFFFFFFFu);
        hdr->cfl = sizeof(fis_reg_h2d_t) / 4; // 5 dwords
        hdr->w = 1; // write
        hdr->a = 0; // ATA
        hdr->prdtl = 1;
        hdr->prdbc = 0;

        hba_cmd_table_t* tbl = (hba_cmd_table_t*)ctx->ctba0;
        memset(tbl, 0, sizeof(hba_cmd_table_t));

        // PRDT sized by logical block size (default 512 when unknown)
        uintptr_t bufp = (uintptr_t)in;
        tbl->prdt[0].dba = (uint32_t)((uint64_t)bufp & 0xFFFFFFFFu);
        tbl->prdt[0].dbau = (uint32_t)(((uint64_t)bufp >> 32) & 0xFFFFFFFFu);
        uint32_t bsz = (ctx->blk && ctx->blk->logical_block_size) ? ctx->blk->logical_block_size : 512u;
        uint32_t byte_count = n * bsz;
        tbl->prdt[0].dbc_i = ((byte_count - 1) & 0x003FFFFFu) | (1u << 31); // ioc=1

        // CFIS: WRITE DMA EXT (0x35)
        fis_reg_h2d_t* cfis = (fis_reg_h2d_t*)tbl->cfis;
        memset(cfis, 0, sizeof(*cfis));
        cfis->fis_type = FIS_TYPE_REG_H2D;
        cfis->c = 1;
        cfis->command = 0x35; // WRITE DMA EXT
        cfis->device = 1 << 6; // LBA mode
        // LBA48
        cfis->lba0 = (uint8_t)(lba & 0xFF);
        cfis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
        cfis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
        cfis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
        cfis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
        cfis->lba5 = (uint8_t)((lba >> 40) & 0xFF);
        cfis->countl = (uint8_t)(n & 0xFF);
        cfis->counth = (uint8_t)((n >> 8) & 0xFF);

        // Issue command
        p->is = 0xFFFFFFFFu; // clear
        mmio_wmb();
        p->ci = 1u; // slot 0

        // Wait for completion
        {
            uint32_t spin = 5000000;
            while (spin--) {
                if ((p->ci & 1u) == 0) break;
                if (ctx->irq_events) break;
                if (p->is & HBA_PxIS_TFES) {
                    ERROR("AHCI: TFES error on WRITE port %u (IS=0x%08x TFD=0x%08x)", ctx->port_no, p->is, p->tfd);
                    return false;
                }
                asm volatile ("pause"); 
            }
            ctx->irq_events = 0;
            if (p->ci & 1u) {
                ERROR("AHCI: WRITE DMA timeout on port %u (IS=0x%08x TFD=0x%08x)", ctx->port_no, p->is, p->tfd);
                return false;
            }
        }

        lba += n; in += n * bsz; count -= n;
    }
    return true;
}

static bool ahci_blk_flush(struct BlockDevice* bdev)
{
    ahci_port_ctx_t* ctx = (ahci_port_ctx_t*)bdev->driver_ctx;
    if (!ctx) return false;
    // Try FLUSH CACHE EXT first; fall back to FLUSH CACHE if needed.
    if (ahci_issue_flush(ctx, 0xEA)) return true;
    return ahci_issue_flush(ctx, 0xE7);
}

static const BlockDeviceOps s_ahci_blk_ops = {
    .read = ahci_blk_read,
    .write = ahci_blk_write,
    .flush = ahci_blk_flush,
};

// ---- AHCI ATAPI (CD/DVD) support (READ(12), 2048B sectors) ----
static bool ahci_atapi_packet_cmd(ahci_port_ctx_t* ctx, const uint8_t* cdb, uint32_t cdb_len, void* buf, uint32_t byte_count, bool is_write)
{
    volatile hba_port_t* p = ctx->port;
    // Wait if busy
    {
        uint32_t spin = 1000000;
        while ((p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) && spin--) { asm volatile ("pause"); }
        if (p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) {
            ERROR("AHCI: ATAPI busy before PACKET (TFD=0x%08x)", p->tfd);
            return false;
        }
    }

    hba_cmd_header_t* hdr = (hba_cmd_header_t*)ctx->clb_mem;
    // Do NOT clear the header entirely, CTBA must remain valid.
    // Ensure CTBA points to our command table.
    uintptr_t ctba = (uintptr_t)ctx->ctba0;
    hdr->ctba  = (uint32_t)((uint64_t)ctba & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)(((uint64_t)ctba >> 32) & 0xFFFFFFFFu);
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4; // 5 dwords
    hdr->a = 1; // ATAPI
    hdr->w = is_write ? 1 : 0;
    hdr->c = 1; // clear BSY on R_OK (safer for some controllers)
    hdr->prdtl = (byte_count > 0) ? 1 : 0;
    hdr->prdbc = 0;

    hba_cmd_table_t* tbl = (hba_cmd_table_t*)ctx->ctba0;
    memset(tbl, 0, sizeof(hba_cmd_table_t));

    if (byte_count) {
        uintptr_t bufp = (uintptr_t)buf;
        tbl->prdt[0].dba = (uint32_t)((uint64_t)bufp & 0xFFFFFFFFu);
        tbl->prdt[0].dbau = (uint32_t)(((uint64_t)bufp >> 32) & 0xFFFFFFFFu);
        tbl->prdt[0].dbc_i = ((byte_count - 1) & 0x003FFFFFu) | (1u << 31);
    }

    // PACKET CFIS
    fis_reg_h2d_t* cfis = (fis_reg_h2d_t*)tbl->cfis;
    memset(cfis, 0, sizeof(*cfis));
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = 0xA0; // PACKET
    // ATAPI byte count in Feature[15:0]
    cfis->featurel = (uint8_t)(byte_count & 0xFF);
    cfis->featureh = (uint8_t)((byte_count >> 8) & 0xFF);

    // Copy CDB (12 or 10 bytes typical)
    memcpy(tbl->acmd, cdb, cdb_len);

    // Issue command
    p->is = 0xFFFFFFFFu;
    mmio_wmb();
    p->ci = 1u; // slot 0
    LOG("AHCI: ATAPI PACKET issued (byte_count=%u, opcode=0x%02x CI=0x%08x)", byte_count, cdb ? cdb[0] : 0xFF, p->ci);

    // Completion: prefer IRQ event
    {
        uint32_t spin = 5000000;
        while (spin--) {
            if ((p->ci & 1u) == 0) break;
            if (ctx->irq_events) break;
            if (p->is & HBA_PxIS_TFES) {
                WARN("AHCI: ATAPI TFES (IS=0x%08x TFD=0x%08x)", p->is, p->tfd);
                return false;
            }
            asm volatile ("pause");
        }
        ctx->irq_events = 0;
        if (p->ci & 1u) {
            ERROR("AHCI: ATAPI PACKET timeout (IS=0x%08x TFD=0x%08x PRDBC=%u)", p->is, p->tfd, hdr->prdbc);
            return false;
        }
    }
    return true;
}

static void ahci_atapi_request_sense(ahci_port_ctx_t* ctx)
{
    uint8_t sense[32];
    memset(sense, 0, sizeof(sense));
    uint8_t cdb[12] = {0};
    cdb[0] = 0x03; // REQUEST SENSE (6)
    cdb[4] = 18;   // allocation length
    ahci_dump_port(ctx->port, ctx->port_no, "before-sense");
    if (ahci_atapi_packet_cmd(ctx, cdb, 12, sense, 18, false)) {
        LOG("AHCI: ATAPI sense: key=0x%02x asc=0x%02x ascq=0x%02x", sense[2] & 0x0F, sense[12], sense[13]);
    } else {
        WARN("AHCI: REQUEST SENSE failed");
    }
    ahci_dump_port(ctx->port, ctx->port_no, "after-sense");
}

static bool ahci_atapi_read_blocks(ahci_port_ctx_t* ctx, uint32_t lba, uint32_t blocks, void* buf)
{
    if (blocks == 0) return true;
    uint32_t byte_count = blocks * 2048u;
    // Prefer READ(10); many emulations behave better with it.
    uint8_t cdb10[12] = {0};
    cdb10[0] = 0x28; // READ(10)
    cdb10[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb10[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb10[4] = (uint8_t)((lba >> 8) & 0xFF);
    cdb10[5] = (uint8_t)(lba & 0xFF);
    cdb10[7] = (uint8_t)((blocks >> 8) & 0xFF);
    cdb10[8] = (uint8_t)(blocks & 0xFF);

    if (ahci_atapi_packet_cmd(ctx, cdb10, 12, buf, byte_count, false)) return true;
    ahci_port_recover(ctx, "READ10-TFES");
    ahci_atapi_request_sense(ctx);

    // Fallback READ(12)
    uint8_t cdb12[12] = {0};
    cdb12[0] = 0xA8; // READ(12)
    cdb12[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb12[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb12[4] = (uint8_t)((lba >> 8) & 0xFF);
    cdb12[5] = (uint8_t)(lba & 0xFF);
    cdb12[6] = (uint8_t)((blocks >> 16) & 0xFF);
    cdb12[7] = (uint8_t)((blocks >> 8) & 0xFF);
    cdb12[8] = (uint8_t)(blocks & 0xFF);

    if (ahci_atapi_packet_cmd(ctx, cdb12, 12, buf, byte_count, false)) return true;
    ahci_port_recover(ctx, "READ12-TFES");
    ahci_atapi_request_sense(ctx);
    return false;
}

static bool ahci_atapi_blk_read(struct BlockDevice* bdev, uint64_t lba, uint32_t count, void* buffer)
{
    ahci_port_ctx_t* ctx = (ahci_port_ctx_t*)bdev->driver_ctx;
    // Read in chunks
    uint8_t* out = (uint8_t*)buffer;
    while (count) {
        uint32_t n = (count > 16) ? 16 : count; // limit chunk size
        if (!ahci_atapi_read_blocks(ctx, (uint32_t)lba, n, out)) return false;
        lba += n; out += n * 2048; count -= n;
        asm volatile ("pause"); 
    }
    return true;
}

static const BlockDeviceOps s_ahci_atapi_ops = {
    .read = ahci_atapi_blk_read,
    .write = ahci_blk_write, // not supported for CDROM
    .flush = ahci_blk_flush,
};

// ---- Geometry helpers ----
static bool ahci_identify_ata(ahci_port_ctx_t* ctx, uint16_t* id512)
{
    volatile hba_port_t* p = ctx->port;
    // Wait if busy
    {
        uint32_t spin = 1000000;
        while ((p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) && spin--) { asm volatile ("hlt"); }
        if (p->tfd & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)) return false;
    }

    hba_cmd_header_t* hdr = (hba_cmd_header_t*)ctx->clb_mem;
    uintptr_t ctba = (uintptr_t)ctx->ctba0;
    hdr->ctba  = (uint32_t)((uint64_t)ctba & 0xFFFFFFFFu);
    hdr->ctbau = (uint32_t)(((uint64_t)ctba >> 32) & 0xFFFFFFFFu);
    hdr->cfl = sizeof(fis_reg_h2d_t) / 4;
    hdr->a = 0; // ATA
    hdr->w = 0;
    hdr->c = 1;
    hdr->prdtl = 1;
    hdr->prdbc = 0;

    hba_cmd_table_t* tbl = (hba_cmd_table_t*)ctx->ctba0;
    memset(tbl, 0, sizeof(hba_cmd_table_t));

    uintptr_t bufp = (uintptr_t)id512;
    tbl->prdt[0].dba = (uint32_t)((uint64_t)bufp & 0xFFFFFFFFu);
    tbl->prdt[0].dbau = (uint32_t)(((uint64_t)bufp >> 32) & 0xFFFFFFFFu);
    tbl->prdt[0].dbc_i = (512 - 1) | (1u << 31);

    fis_reg_h2d_t* cfis = (fis_reg_h2d_t*)tbl->cfis;
    memset(cfis, 0, sizeof(*cfis));
    cfis->fis_type = FIS_TYPE_REG_H2D;
    cfis->c = 1;
    cfis->command = 0xEC; // IDENTIFY DEVICE
    cfis->device = 1 << 6; // LBA mode

    p->is = 0xFFFFFFFFu;
    mmio_wmb();
    p->ci = 1u;

    uint32_t spin = 5000000;
    while (spin--) {
        if ((p->ci & 1u) == 0) break;
        if (p->is & HBA_PxIS_TFES) return false;
        asm volatile ("pause"); 
    }
    if (p->ci & 1u) return false;
    return true;
}

static bool ahci_atapi_read_capacity(ahci_port_ctx_t* ctx, uint32_t* last_lba, uint32_t* block_len)
{
    uint8_t cap[8]; memset(cap, 0, sizeof(cap));
    uint8_t cdb[12] = {0};
    cdb[0] = 0x25; // READ CAPACITY(10)
    if (!ahci_atapi_packet_cmd(ctx, cdb, 12, cap, 8, false)) return false;
    // Big endian fields
    *last_lba = (uint32_t)cap[0] << 24 | (uint32_t)cap[1] << 16 | (uint32_t)cap[2] << 8 | (uint32_t)cap[3];
    *block_len = (uint32_t)cap[4] << 24 | (uint32_t)cap[5] << 16 | (uint32_t)cap[6] << 8 | (uint32_t)cap[7];
    LOG("AHCI: ATAPI READ CAPACITY -> last_lba=%u block_len=%u", *last_lba, *block_len);
    return true;
}

static void ahci_probe_controller(void)
{
    PCI_Init();
    PCIDevice* dev = PCI_FindByClass(0x01 /*Mass Storage*/, 0x06 /*SATA*/, 0x01 /*AHCI*/);
    if (!dev) {
        WARN("AHCI: No AHCI controller found (PCI class 0x01/0x06/0x01)");
        return;
    }

    // Enable memory/IO + bus mastering
    PCI_EnableIOAndMemory(dev);
    PCI_EnableBusMastering(dev);

    if (dev->barCount < 6) {
        WARN("AHCI: Unexpected BAR count %u on %02x:%02x.%u", dev->barCount, dev->bus, dev->device, dev->function);
    }

    // Per spec, ABAR is at BAR5
    uint64_t abar_phys = dev->bars[5].address;
    if (abar_phys == 0 || dev->bars[5].isIO) {
        ERROR("AHCI: Invalid ABAR at BAR5 (addr=%p isIO=%d)", (void*)(uintptr_t)abar_phys, dev->bars[5].isIO);
        return;
    }

    volatile hba_mem_t* hba = (volatile hba_mem_t*)(uintptr_t)abar_phys; // identity mapped
    s_hba = hba;
    ahci_dump_hba(hba, "before-enable");
    // Optionally reset HBA if needed
    if ((hba->ghc & HBA_GHC_AE) == 0) {
        hba->ghc |= HBA_GHC_AE;
    }
    // BIOS/OS handoff when BIOS still owns the controller
    if (hba->bohc & HBA_BOHC_BOS) {
        LOG("AHCI: BOHC BIOS-owned detected; requesting OS ownership");
        hba->bohc |= HBA_BOHC_OOS;
        // Spec allows BIOS up to 1 second typically; we spin bounded
        uint32_t spin = 5000000; // ~ a few ms worth of MMIO polls
        while ((hba->bohc & HBA_BOHC_BOS) && spin--) { asm volatile ("hlt"); }
        if (hba->bohc & HBA_BOHC_BOS) {
            WARN("AHCI: BIOS did not release ownership; continuing anyway");
        } else {
            LOG("AHCI: BOHC ownership transferred to OS");
        }
    }
    // Global HBA reset if controller seems wedged (rare)
    // hba->ghc |= HBA_GHC_HR; while (hba->ghc & HBA_GHC_HR) {}
    // Clear any pending interrupts and enable global interrupt
    hba->is = 0xFFFFFFFFu;
    hba->ghc |= HBA_GHC_IE; // global interrupt enable
    ahci_dump_hba(hba, "after-enable");
    uint32_t cap = hba->cap;
    uint32_t vs  = hba->vs;
    uint32_t pi  = hba->pi;
    LOG("AHCI: ABAR=%p CAP=0x%08x VS=%u.%u PI=0x%08x", (void*)hba, cap, (vs >> 16) & 0xFFFF, vs & 0xFFFF, pi);

    // Register legacy INTx interrupt handler (best-effort) before port scan
    uint8_t irq_line = PCI_ConfigRead8(dev->bus, dev->device, dev->function, 0x3C);
    if (irq_line != 0xFF && irq_controller) {
        s_ahci_irq_line = irq_line;
        extern void ahci_isr_stub(void);
        irq_controller->register_handler(irq_line, ahci_isr_stub);
        irq_controller->disable(irq_line);
        LOG("AHCI: Registered IRQ handler on IRQ%u", irq_line);
    } else {
        WARN("AHCI: No legacy IRQ line reported; continuing with polling");
    }

    // Iterate ports implemented
    for (uint8_t i = 0; i < 32; ++i) {
        if ((pi & (1u << i)) == 0) continue;
        volatile hba_port_t* p = &hba->ports[i];

        ahci_port_ctx_t* ctx = &s_ports[i];
        ctx->port = p; ctx->port_no = i; ctx->blk = NULL;
        ctx->irq_events = 0;
        if (!ahci_port_configure(ctx)) {
            WARN("AHCI: Port %u configuration failed", i);
            continue;
        }
        // Clear and enable all port interrupts
        p->is = 0xFFFFFFFFu; p->ie = 0xFFFFFFFFu;

        // Issue COMRESET and wait a bit for device detection
        ahci_port_comreset(p);
        for (volatile int dly = 0; dly < 100000; ++dly) asm volatile ("pause"); // small settle

        uint32_t ssts = p->ssts;
        uint8_t det = (uint8_t)(ssts & HBA_SSTS_DET_MASK);
        uint8_t spd = HBA_SSTS_SPD(ssts);
        uint8_t ipm = HBA_SSTS_IPM(ssts);
        uint32_t sig = p->sig;
        LOG("AHCI: Port %u SSTS=0x%08x DET=%u SPD=%u IPM=%u SIG=0x%08x (%s)", i, ssts, det, spd, ipm, sig, sig_to_str(sig));
        if (det != HBA_DET_PRESENT) continue;

        // Register BlockDevice for ATA disks
        if (sig == SATA_SIG_ATA) {
            uint16_t id[256]; memset(id, 0, sizeof(id));
            uint32_t bsz = 512; uint64_t total = 0;
            if (ahci_identify_ata(ctx, id)) {
                uint16_t w106 = id[106];
                if (w106 & (1u << 12)) {
                    uint32_t sz = ((uint32_t)id[118] << 16) | id[117];
                    if (sz >= 512 && (sz % 512) == 0) bsz = sz;
                }
                uint32_t lba28 = ((uint32_t)id[61] << 16) | id[60];
                bool lba48 = (id[83] & (1u << 10)) != 0;
                uint64_t lba48_cnt = 0;
                if (lba48) {
                    lba48_cnt = ((uint64_t)id[103] << 48) | ((uint64_t)id[102] << 32) | ((uint64_t)id[101] << 16) | id[100];
                }
                total = lba48 ? lba48_cnt : lba28;
                LOG("AHCI: IDENTIFY -> sector=%u total=%u (lba48=%d)", bsz, (unsigned)total, (int)lba48);
            } else {
                WARN("AHCI: IDENTIFY ATA failed; using defaults");
            }
            BlockDevice_InitRegistry();
            char* nm = (char*)malloc(8);
            if (nm) { nm[0]='a'; nm[1]='h'; nm[2]='c'; nm[3]='i'; nm[4]='0'+(i%10); nm[5]='\0'; }
            ctx->blk = BlockDevice_Register(nm ? nm : "ahci", BLKDEV_TYPE_DISK, bsz, total, &s_ahci_blk_ops, ctx);
        } else if (sig == SATA_SIG_ATAPI) {
            uint32_t last=0, blen=2048;
            (void)ahci_atapi_read_capacity(ctx, &last, &blen);
            BlockDevice_InitRegistry();
            char* nm = (char*)malloc(6);
            if (nm) { nm[0]='c'; nm[1]='d'; nm[2]='0'+(i%10); nm[3]='\0'; }
            ctx->blk = BlockDevice_Register(nm ? nm : "cd", BLKDEV_TYPE_CDROM, blen ? blen : 2048, (uint64_t)last + 1u, &s_ahci_atapi_ops, ctx);
            LOG("AHCI: Port %u ATAPI device registered as BlockDevice (block=%u total=%u)", i, blen, last+1u);
        }
    }

    // (IRQ already registered earlier)
}

bool ahci_init(void)
{
    ahci_probe_controller();
    return true; // return true even if no controller; not fatal
}

void ahci_enable(void)
{
    ahci_driver.enabled = true;
}

void ahci_disable(void)
{
    ahci_driver.enabled = false;
}

DriverBase ahci_driver = (DriverBase){
    .name = "AHCI",
    .enabled = false,
    .version = 1,
    .context = NULL,
    .init = ahci_init,
    .enable = ahci_enable,
    .disable = ahci_disable,
    .type = DRIVER_TYPE_STORAGE
};
