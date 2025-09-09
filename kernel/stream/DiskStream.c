#include <stream/DiskStream.h>
#include <storage/BlockDevice.h>
#include <memory/memory.h>
#include <debug/debug.h>

DiskStream* DiskStream_CreateFromBlockDevice(void* blockDevice)
{
    if (!blockDevice) return NULL;
    DiskStream* stream = (DiskStream*)malloc(sizeof(DiskStream));
    if (!stream) return NULL;
    stream->device = blockDevice;
    stream->device_type = DISKSTREAM_DEVICE_BLOCK; // BlockDevice
    stream->isOpen = false;
    stream->readonly = true; // Default to readonly
    return stream;
}

void* DiskStream_Open(DiskStream* stream, bool readonly)
{
    if (!stream) return NULL;
    if (stream->isOpen) return stream->device; // Already open
    stream->readonly = readonly;
    stream->isOpen = true;
    return stream->device;
}

void DiskStream_Close(DiskStream* stream)
{
    if (!stream) return;
    if (!stream->isOpen) return; // Already closed
    stream->isOpen = false;
}

void* DiskStream_ReadSector(DiskStream* stream, uint64_t sector)
{
    if (!stream)
    {
        WARN("DiskStream_ReadSector: stream is NULL");
        return NULL;
    }

    if (!stream->isOpen)
    {
        WARN("DiskStream_ReadSector: stream is not open");
        return NULL;
    }

    if (stream->device_type != DISKSTREAM_DEVICE_BLOCK)
    {
        WARN("DiskStream_ReadSector: unsupported device type %d", stream->device_type);
        return NULL;
    }

    BlockDevice* dev = (BlockDevice*)stream->device;
    size_t block_size = dev->logical_block_size;
    
    if (dev->total_blocks && sector >= dev->total_blocks)
    {
        WARN("DiskStream_ReadSector: sector out of range (%llu >= %llu)",
             (unsigned long long)sector, (unsigned long long)dev->total_blocks);
        return NULL;
    }

    void* buffer = malloc(block_size);
    if (!buffer)
    {
        ERROR("DiskStream_ReadSector: malloc(%zu) failed", block_size);
        return NULL;
    }

    if (!BlockDevice_Read(dev, sector, 1, buffer))
    {
        WARN("DiskStream_ReadSector: read failed (lba=%llu)", (unsigned long long)sector);
        free(buffer);
        return NULL;
    }

    return buffer;

}

void* DiskStream_ReadSectors(DiskStream* stream, uint64_t sector, size_t count)
{
    if (!stream)
    {
        WARN("DiskStream_ReadSectors: stream is NULL");
        return NULL;
    }

    if (!stream->isOpen)
    {
        WARN("DiskStream_ReadSectors: stream is not open");
        return NULL;
    }

    if (stream->device_type != DISKSTREAM_DEVICE_BLOCK)
    {
        WARN("DiskStream_ReadSectors: unsupported device type %d", stream->device_type);
        return NULL;
    }

    if (count == 0)
    {
        return NULL;
    }

    BlockDevice* dev = (BlockDevice*)stream->device;
    size_t block_size = dev->logical_block_size;

    if (dev->total_blocks && (sector >= dev->total_blocks || (uint64_t)count > (dev->total_blocks - sector)))
    {
        WARN("DiskStream_ReadSectors: range out of bounds (lba=%llu count=%zu total=%llu)",
             (unsigned long long)sector, count, (unsigned long long)dev->total_blocks);
        return NULL;
    }

    if (count > (SIZE_MAX / block_size))
    {
        ERROR("DiskStream_ReadSectors: size overflow (count=%zu block=%zu)", count, block_size);
        return NULL;
    }

    if (count > UINT32_MAX)
    {
        WARN("DiskStream_ReadSectors: count too large for device API (%zu)", count);
        return NULL;
    }

    size_t total_bytes = count * block_size;
    void* buffer = malloc(total_bytes);
    if (!buffer)
    {
        ERROR("DiskStream_ReadSectors: malloc(%zu) failed", total_bytes);
        return NULL;
    }

    if (!BlockDevice_Read(dev, sector, (uint32_t)count, buffer))
    {
        WARN("DiskStream_ReadSectors: read failed (lba=%llu count=%zu)", (unsigned long long)sector, count);
        free(buffer);
        return NULL;
    }

    return buffer;
}

void* DiskStream_Read(DiskStream* stream, uint64_t offset, size_t size)
{
    if (!stream)
    {
        WARN("DiskStream_Read: stream is NULL");
        return NULL;
    }

    if (!stream->isOpen)
    {
        WARN("DiskStream_Read: stream is not open");
        return NULL;
    }

    if (stream->device_type != DISKSTREAM_DEVICE_BLOCK)
    {
        WARN("DiskStream_Read: unsupported device type %d", stream->device_type);
        return NULL;
    }

    if (size == 0)
    {
        return NULL;
    }

    BlockDevice* dev = (BlockDevice*)stream->device;
    size_t block_size = dev->logical_block_size;

    uint64_t start_lba = offset / block_size;
    if (offset > UINT64_MAX - (size - 1))
    {
        WARN("DiskStream_Read: offset+size overflow");
        return NULL;
    }
    uint64_t end_byte = offset + size - 1;
    uint64_t end_lba = end_byte / block_size;

    if (dev->total_blocks)
    {
        if (start_lba >= dev->total_blocks || end_lba >= dev->total_blocks)
        {
            WARN("DiskStream_Read: range out of bounds (lba=%llu..%llu total=%llu)",
                 (unsigned long long)start_lba, (unsigned long long)end_lba, (unsigned long long)dev->total_blocks);
            return NULL;
        }
    }

    if (size > SIZE_MAX)
    {
        ERROR("DiskStream_Read: invalid size");
        return NULL;
    }

    void* out = malloc(size);
    if (!out)
    {
        ERROR("DiskStream_Read: malloc(%zu) failed", size);
        return NULL;
    }

    // Read block-by-block and copy the relevant byte ranges
    void* block_buf = malloc(block_size);
    if (!block_buf)
    {
        ERROR("DiskStream_Read: malloc(%zu) for block buffer failed", block_size);
        free(out);
        return NULL;
    }

    size_t copied = 0;
    uint64_t cur_offset = offset;
    while (copied < size)
    {
        uint64_t lba = cur_offset / block_size;
        size_t intra = (size_t)(cur_offset % block_size);
        size_t remain = size - copied;
        size_t chunk = block_size - intra;
        if (chunk > remain) chunk = remain;

        if (!BlockDevice_Read(dev, lba, 1, block_buf))
        {
            WARN("DiskStream_Read: read failed at lba=%llu", (unsigned long long)lba);
            free(block_buf);
            free(out);
            return NULL;
        }

        memcpy((uint8_t*)out + copied, (uint8_t*)block_buf + intra, chunk);
        copied += chunk;
        cur_offset += chunk;
    }

    free(block_buf);
    return out;
}

static inline bool diskstream_can_write(DiskStream* stream)
{
    if (!stream)
        return false;
    if (!stream->isOpen)
        return false;
    if (stream->readonly)
        return false;
    if (stream->device_type != DISKSTREAM_DEVICE_BLOCK)
        return false;
    BlockDevice* dev = (BlockDevice*)stream->device;
    return dev && dev->ops && dev->ops->write != NULL;
}

void DiskStream_Wrtie8(DiskStream* stream, uint64_t offset, uint8_t value)
{
    DiskStream_Write(stream, offset, &value, 1);
}

void DiskStream_Wrtie16(DiskStream* stream, uint64_t offset, uint16_t value)
{
    DiskStream_Write(stream, offset, &value, 2);
}

void DiskStream_Wrtie32(DiskStream* stream, uint64_t offset, uint32_t value)
{
    DiskStream_Write(stream, offset, &value, 4);
}

void DiskStream_Wrtie64(DiskStream* stream, uint64_t offset, uint64_t value)
{
    DiskStream_Write(stream, offset, &value, 8);
}

void DiskStream_Write(DiskStream* stream, uint64_t offset, const void* data, size_t size)
{
    if (!stream)
    {
        WARN("DiskStream_Write: stream is NULL");
        return;
    }

    if (!diskstream_can_write(stream))
    {
        WARN("DiskStream_Write: stream is not writable (open=%d readonly=%d type=%d)",
             stream->isOpen ? 1 : 0, stream->readonly ? 1 : 0, stream->device_type);
        return;
    }

    if (!data || size == 0)
        return;

    BlockDevice* dev = (BlockDevice*)stream->device;
    size_t block_size = dev->logical_block_size;

    uint64_t start_lba = offset / block_size;
    if (offset > UINT64_MAX - (size - 1))
    {
        WARN("DiskStream_Write: offset+size overflow");
        return;
    }
    uint64_t end_byte = offset + size - 1;
    uint64_t end_lba = end_byte / block_size;

    if (dev->total_blocks)
    {
        if (start_lba >= dev->total_blocks || end_lba >= dev->total_blocks)
        {
            WARN("DiskStream_Write: range out of bounds (lba=%llu..%llu total=%llu)",
                 (unsigned long long)start_lba, (unsigned long long)end_lba, (unsigned long long)dev->total_blocks);
            return;
        }
    }

    // Temporary buffer for block read-modify-write
    void* block_buf = malloc(block_size);
    if (!block_buf)
    {
        ERROR("DiskStream_Write: malloc(%zu) for block buffer failed", block_size);
        return;
    }

    size_t written = 0;
    uint64_t cur_offset = offset;
    const uint8_t* src = (const uint8_t*)data;

    while (written < size)
    {
        uint64_t lba = cur_offset / block_size;
        size_t intra = (size_t)(cur_offset % block_size);
        size_t remain = size - written;
        size_t chunk = block_size - intra;
        if (chunk > remain) chunk = remain;

        // If we're writing a full aligned block, we can write directly
        if (intra == 0 && chunk == block_size)
        {
            if (!BlockDevice_Write(dev, lba, 1, src + written))
            {
                WARN("DiskStream_Write: write failed at lba=%llu", (unsigned long long)lba);
                break;
            }
        }
        else
        {
            // Read-modify-write for partial block
            if (!BlockDevice_Read(dev, lba, 1, block_buf))
            {
                WARN("DiskStream_Write: read for RMW failed at lba=%llu", (unsigned long long)lba);
                break;
            }
            memcpy((uint8_t*)block_buf + intra, src + written, chunk);
            if (!BlockDevice_Write(dev, lba, 1, block_buf))
            {
                WARN("DiskStream_Write: write failed at lba=%llu", (unsigned long long)lba);
                break;
            }
        }

        written += chunk;
        cur_offset += chunk;
    }

    free(block_buf);

    if (written != size)
    {
        WARN("DiskStream_Write: partial write (%zu of %zu)", written, size);
    }

    // Best-effort flush if the device supports it
    if (!BlockDevice_Flush(dev))
    {
        WARN("DiskStream_Write: flush reported failure");
    }
}

uint8_t DiskStream_Read8(DiskStream* stream, uint64_t offset)
{
    uint8_t val = 0;
    void* buf = DiskStream_Read(stream, offset, 1);
    if (buf)
    {
        val = *(uint8_t*)buf;
        free(buf);
    }
    return val;
}

uint16_t DiskStream_Read16(DiskStream* stream, uint64_t offset)
{
    uint16_t val = 0;
    void* buf = DiskStream_Read(stream, offset, 2);
    if (buf)
    {
        memcpy(&val, buf, 2);
        free(buf);
    }
    return val;
}

uint32_t DiskStream_Read32(DiskStream* stream, uint64_t offset)
{
    uint32_t val = 0;
    void* buf = DiskStream_Read(stream, offset, 4);
    if (buf)
    {
        memcpy(&val, buf, 4);
        free(buf);
    }
    return val;
}

uint64_t DiskStream_Read64(DiskStream* stream, uint64_t offset)
{
    uint64_t val = 0;
    void* buf = DiskStream_Read(stream, offset, 8);
    if (buf)
    {
        memcpy(&val, buf, 8);
        free(buf);
    }
    return val;
}

void DiskStream_Destroy(DiskStream* stream)
{
    if (!stream) return;
    if (stream->isOpen)
    {
        // No underlying close for BlockDevice registry; just mark closed.
        stream->isOpen = false;
    }
    free(stream);
}
