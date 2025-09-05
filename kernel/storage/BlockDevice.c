#include <storage/BlockDevice.h>
#include <memory/memory.h>
#include <debug/debug.h>

static List* s_blkdev_list = NULL;

void BlockDevice_InitRegistry(void)
{
    if (!s_blkdev_list) s_blkdev_list = List_Create();
}

BlockDevice* BlockDevice_Register(const char* name,
                                  BlockDeviceType type,
                                  uint32_t logical_block_size,
                                  uint64_t total_blocks,
                                  const BlockDeviceOps* ops,
                                  void* driver_ctx)
{
    if (!s_blkdev_list) BlockDevice_InitRegistry();
    if (!ops || !ops->read) {
        ERROR("BlockDevice_Register('%s'): invalid ops", name ? name : "<noname>");
        return NULL;
    }
    BlockDevice* d = (BlockDevice*)malloc(sizeof(BlockDevice));
    if (!d) return NULL;
    d->name = name;
    d->type = type;
    d->logical_block_size = logical_block_size ? logical_block_size : 512;
    d->total_blocks = total_blocks;
    d->ops = ops;
    d->driver_ctx = driver_ctx;
    List_Add(s_blkdev_list, d);
    LOG("BlockDevice: registered '%s' type=%u block=%u total=%u", d->name, (unsigned)d->type, d->logical_block_size, (unsigned)(d->total_blocks));
    return d;
}

size_t BlockDevice_Count(void)
{
    return s_blkdev_list ? s_blkdev_list->count : 0;
}

BlockDevice* BlockDevice_GetAt(size_t index)
{
    if (!s_blkdev_list) return NULL;
    return (BlockDevice*)List_GetAt(s_blkdev_list, index);
}

bool BlockDevice_Read(BlockDevice* dev, uint64_t lba, uint32_t count, void* buffer)
{
    if (!dev || !dev->ops || !dev->ops->read) return false;
    return dev->ops->read(dev, lba, count, buffer);
}

bool BlockDevice_Write(BlockDevice* dev, uint64_t lba, uint32_t count, const void* buffer)
{
    if (!dev || !dev->ops || !dev->ops->write) return false;
    return dev->ops->write(dev, lba, count, buffer);
}

bool BlockDevice_Flush(BlockDevice* dev)
{
    if (!dev || !dev->ops || !dev->ops->flush) return true;
    return dev->ops->flush(dev);
}

