#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <list.h>

typedef enum {
    BLKDEV_TYPE_DISK = 0,  // Fixed or removable block device (HDD/SSD)
    BLKDEV_TYPE_CDROM = 1, // ATAPI/optical (2048-byte sectors typical)
    BLKDEV_TYPE_VIRTUAL = 2
} BlockDeviceType;

struct BlockDevice;

typedef struct BlockDeviceOps {
    bool (*read)(struct BlockDevice* dev, uint64_t lba, uint32_t count, void* buffer);
    bool (*write)(struct BlockDevice* dev, uint64_t lba, uint32_t count, const void* buffer);
    bool (*flush)(struct BlockDevice* dev);
} BlockDeviceOps;

typedef struct BlockDevice {
    const char* name;
    BlockDeviceType type;
    uint32_t logical_block_size; // bytes per logical block (e.g., 512 or 2048)
    uint64_t total_blocks;       // total logical blocks
    const BlockDeviceOps* ops;   // function table
    void* driver_ctx;            // driver-private context
} BlockDevice;

// Registry API
void BlockDevice_InitRegistry(void);
BlockDevice* BlockDevice_Register(const char* name,
                                  BlockDeviceType type,
                                  uint32_t logical_block_size,
                                  uint64_t total_blocks,
                                  const BlockDeviceOps* ops,
                                  void* driver_ctx);

size_t BlockDevice_Count(void);
BlockDevice* BlockDevice_GetAt(size_t index);

// Convenience shims
bool BlockDevice_Read(BlockDevice* dev, uint64_t lba, uint32_t count, void* buffer);
bool BlockDevice_Write(BlockDevice* dev, uint64_t lba, uint32_t count, const void* buffer);
bool BlockDevice_Flush(BlockDevice* dev);

#ifdef __cplusplus
}
#endif

