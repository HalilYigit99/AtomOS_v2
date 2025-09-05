#include <storage/BlockDevice.h>
#include <debug/debug.h>
#include <memory/memory.h>
#include <stdint.h>
#include <stddef.h>

static void hexdump(const void* data, size_t len)
{
    const uint8_t* p = (const uint8_t*)data;
    size_t lines = (len + 15) / 16;
    for (size_t l = 0; l < lines; ++l) {
        size_t off = l * 16;
        size_t n = (off + 16 <= len) ? 16 : (len - off);
        debugStream->printf("%04zx: ", off);
        for (size_t i = 0; i < 16; ++i) {
            if (i < n) debugStream->printf("%02x ", p[off + i]);
            else debugStream->printf("   ");
        }
        debugStream->printf(" |");
        for (size_t i = 0; i < n; ++i) {
            uint8_t c = p[off + i];
            debugStream->printf("%c", (c >= 32 && c <= 126) ? (char)c : '.');
        }
        debugStream->printf("|\n");
    }
}

void block_read_test_run(void)
{
    // Ensure registry exists (drivers may already have created it).
    BlockDevice_InitRegistry();

    size_t count = BlockDevice_Count();
    LOG("BlockDevice count: %zu", count);
    if (count == 0) {
        WARN("No block devices found. Ensure ATA/AHCI drivers initialized.");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        BlockDevice* dev = BlockDevice_GetAt(i);
        if (!dev) continue;
        LOG("[%zu] name=%s type=%u block=%u total=%u", i, dev->name, (unsigned)dev->type, dev->logical_block_size, (unsigned)dev->total_blocks);

        uint32_t bsz = dev->logical_block_size ? dev->logical_block_size : 512;
        if (!dev->ops || !dev->ops->read) {
            WARN("Device %s has no read op", dev->name);
            continue;
        }

        void* buf = malloc(bsz);
        if (!buf) {
            ERROR("Out of memory allocating %u bytes", bsz);
            continue;
        }

        // Read LBA 0 for disks; LBA 16 for CDROM (ISO9660 PVD)
        uint64_t lba_to_read = 0;
        if (dev->type == BLKDEV_TYPE_CDROM && bsz == 2048) lba_to_read = 16;
        if (!BlockDevice_Read(dev, lba_to_read, 1, buf)) {
            ERROR("Read LBA %llu failed on %s", (unsigned long long)lba_to_read, dev->name);
            free(buf);
            continue;
        }

        LOG("%s: LBA%llu first %u bytes:", dev->name, (unsigned long long)lba_to_read, (unsigned)((bsz < 64) ? bsz : 64));
        hexdump(buf, bsz);

        // Optionally, read next sector as a second check
        if (dev->total_blocks > 1) {
            if (BlockDevice_Read(dev, 1, 1, buf)) {
                LOG("%s: LBA1 first %u bytes:", dev->name, (size_t)bsz);
                hexdump(buf, bsz);
            }
        }

        free(buf);
    }
}
