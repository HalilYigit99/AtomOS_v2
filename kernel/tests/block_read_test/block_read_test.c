#include <storage/BlockDevice.h>
#include <debug/debug.h>
#include <memory/memory.h>
#include <stdint.h>
#include <stddef.h>

// static void hexdump(const void* data, size_t len)
// {
//     const uint8_t* p = (const uint8_t*)data;
//     size_t lines = (len + 15) / 16;
//     for (size_t l = 0; l < lines; ++l) {
//         size_t off = l * 16;
//         size_t n = (off + 16 <= len) ? 16 : (len - off);
//         debugStream->printf("%04zx: ", off);
//         for (size_t i = 0; i < 16; ++i) {
//             if (i < n) debugStream->printf("%02x ", p[off + i]);
//             else debugStream->printf("   ");
//         }
//         debugStream->printf(" |");
//         for (size_t i = 0; i < n; ++i) {
//             uint8_t c = p[off + i];
//             debugStream->printf("%c", (c >= 32 && c <= 126) ? (char)c : '.');
//         }
//         debugStream->printf("|\n");
//     }
// }

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
    }
}
