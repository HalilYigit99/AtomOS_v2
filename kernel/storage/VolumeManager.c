#include <storage/Volume.h>
#include <memory/memory.h>
#include <util/string.h>
#include <util/convert.h>
#include <debug/debug.h>
#include <list.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#pragma pack(push, 1)
typedef struct MBRPartitionEntry {
    uint8_t status;
    uint8_t chs_first[3];
    uint8_t type;
    uint8_t chs_last[3];
    uint32_t first_lba;
    uint32_t sector_count;
} MBRPartitionEntry;

typedef struct GPTHeader {
    char     signature[8];
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t  disk_guid[16];
    uint64_t partition_entry_lba;
    uint32_t partition_entry_count;
    uint32_t partition_entry_size;
    uint32_t partition_entry_crc32;
} GPTHeader;

typedef struct GPTPartitionEntry {
    uint8_t  type_guid[16];
    uint8_t  unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    uint16_t name[36];
} GPTPartitionEntry;
#pragma pack(pop)

static bool s_initialized = false;
static List* s_volumes = NULL;

static void volume_manager_ensure_init(void)
{
    if (s_initialized)
        return;
    BlockDevice_InitRegistry();
    s_volumes = List_Create();
    if (!s_volumes)
    {
        ERROR("VolumeManager: failed to allocate volume list");
        return;
    }
    s_initialized = true;
}

static Volume* volume_allocate(BlockDevice* device,
                               VolumeType type,
                               const char* name,
                               uint64_t start_lba,
                               uint64_t blocks,
                               uint32_t block_size)
{
    Volume* volume = (Volume*)malloc(sizeof(Volume));
    if (!volume)
        return NULL;
    memset(volume, 0, sizeof(Volume));
    volume->device = device;
    volume->type = type;
    volume->start_lba = start_lba;
    volume->block_count = blocks;
    volume->block_size = block_size;
    if (name)
    {
        volume->name = strdup(name);
        if (!volume->name)
        {
            free(volume);
            return NULL;
        }
    }
    return volume;
}

static void volume_free(Volume* volume)
{
    if (!volume) return;
    if (volume->name) free(volume->name);
    free(volume);
}

static void volume_manager_clear(void)
{
    if (!s_volumes) return;
    while (!List_IsEmpty(s_volumes))
    {
        Volume* vol = (Volume*)List_GetAt(s_volumes, 0);
        List_RemoveAt(s_volumes, 0);
        volume_free(vol);
    }
}

static const char* volume_base_name(BlockDevice* device, char* buffer, size_t buffer_len, size_t index)
{
    if (!buffer || buffer_len == 0)
        return NULL;
    buffer[0] = '\0';
    const char* dev_name = device && device->name ? device->name : "block";
    strncpy(buffer, dev_name, buffer_len - 1);
    buffer[buffer_len - 1] = '\0';
    if (index != (size_t)-1)
    {
        strcat(buffer, "p");
        char idx_buf[16];
        utoa((unsigned)index, idx_buf, 10);
        strcat(buffer, idx_buf);
    }
    return buffer;
}

static void volume_manager_add(Volume* volume)
{
    if (!volume || !s_volumes) return;
    List_Add(s_volumes, volume);
    LOG("VolumeManager: registered volume '%s' (start=%llu, blocks=%llu)",
        volume->name ? volume->name : "<noname>",
        (unsigned long long)volume->start_lba,
        (unsigned long long)volume->block_count);
}

static bool volume_read_device(BlockDevice* device, uint64_t lba, uint32_t count, void* buffer)
{
    if (!device) return false;
    return BlockDevice_Read(device, lba, count, buffer);
}

static void gpt_decode_name(const uint16_t* name_utf16, size_t length, char* out, size_t out_len)
{
    if (!out || out_len == 0) return;
    size_t pos = 0;
    for (size_t i = 0; i < length && name_utf16[i] != 0 && pos + 1 < out_len; ++i)
    {
        uint16_t ch = name_utf16[i];
        if (ch < 0x80)
            out[pos++] = (char)ch;
        else
            out[pos++] = '?';
    }
    out[pos] = '\0';
}

static void volume_scan_gpt(BlockDevice* device, uint32_t block_size)
{
    uint8_t* header_block = (uint8_t*)malloc(block_size);
    if (!header_block)
        return;
    if (!volume_read_device(device, 1, 1, header_block))
    {
        free(header_block);
        return;
    }

    GPTHeader* header = (GPTHeader*)header_block;
    if (memcmp(header->signature, "EFI PART", 8) != 0)
    {
        free(header_block);
        return;
    }

    uint32_t entry_size = header->partition_entry_size;
    uint32_t entry_count = header->partition_entry_count;
    uint64_t entry_lba = header->partition_entry_lba;

    if (entry_size < sizeof(GPTPartitionEntry) || entry_count == 0)
    {
        free(header_block);
        return;
    }

    uint64_t table_size_bytes = (uint64_t)entry_size * entry_count;
    uint64_t blocks_to_read = (table_size_bytes + block_size - 1) / block_size;
    uint8_t* entries = (uint8_t*)malloc((size_t)(blocks_to_read * block_size));
    if (!entries)
    {
        free(header_block);
        return;
    }

    if (!volume_read_device(device, entry_lba, (uint32_t)blocks_to_read, entries))
    {
        free(entries);
        free(header_block);
        return;
    }

    char name_buf[64];
    size_t partition_index = 1;
    for (uint32_t i = 0; i < entry_count; ++i)
    {
        GPTPartitionEntry* entry = (GPTPartitionEntry*)(entries + (size_t)i * entry_size);
        bool empty = true;
        for (size_t b = 0; b < sizeof(entry->type_guid); ++b)
        {
            if (entry->type_guid[b] != 0)
            {
                empty = false;
                break;
            }
        }
        if (empty)
            continue;

        if (entry->last_lba < entry->first_lba)
            continue;

        uint64_t blocks = entry->last_lba - entry->first_lba + 1;
        if (blocks == 0)
            continue;

        Volume* volume = volume_allocate(device,
                                         VOLUME_TYPE_GPT_PARTITION,
                                         NULL,
                                         entry->first_lba,
                                         blocks,
                                         block_size);
        if (!volume)
            continue;

        memcpy(volume->type_guid, entry->type_guid, sizeof(entry->type_guid));
        memcpy(volume->unique_guid, entry->unique_guid, sizeof(entry->unique_guid));
        volume->attributes = entry->attributes;
        volume->mbr_type = 0xEE;

        char base_buf[64];
        volume_base_name(device, base_buf, sizeof(base_buf), partition_index);
        volume->name = strdup(base_buf);
        if (!volume->name)
        {
            volume_free(volume);
            continue;
        }

        if (entry->name[0] != 0)
        {
            gpt_decode_name(entry->name, sizeof(entry->name) / sizeof(uint16_t), name_buf, sizeof(name_buf));
            LOG("VolumeManager: GPT part %s label '%s'", volume->name, name_buf);
        }

        volume_manager_add(volume);
        partition_index++;
    }

    free(entries);
    free(header_block);
}

static void volume_scan_mbr(BlockDevice* device, uint32_t block_size)
{
    uint8_t* sector = (uint8_t*)malloc(block_size);
    if (!sector)
        return;
    if (!volume_read_device(device, 0, 1, sector))
    {
        free(sector);
        return;
    }

    bool valid_sig = (sector[510] == 0x55) && (sector[511] == 0xAA);
    if (!valid_sig)
    {
        free(sector);
        return;
    }

    MBRPartitionEntry* entries = (MBRPartitionEntry*)(sector + 446);
    bool gpt_protective = false;
    for (int i = 0; i < 4; ++i)
    {
        if (entries[i].type == 0xEE)
        {
            gpt_protective = true;
            break;
        }
    }

    if (gpt_protective)
    {
        free(sector);
        volume_scan_gpt(device, block_size);
        return;
    }

    for (int i = 0; i < 4; ++i)
    {
        uint8_t part_type = entries[i].type;
        uint32_t first_lba = entries[i].first_lba;
        uint32_t sector_count = entries[i].sector_count;
        if (part_type == 0 || sector_count == 0)
            continue;

        Volume* volume = volume_allocate(device,
                                         VOLUME_TYPE_MBR_PARTITION,
                                         NULL,
                                         first_lba,
                                         sector_count,
                                         block_size);
        if (!volume)
            continue;
        volume->mbr_type = part_type;
        char base_buf[64];
        volume_base_name(device, base_buf, sizeof(base_buf), (size_t)(i + 1));
        volume->name = strdup(base_buf);
        if (!volume->name)
        {
            volume_free(volume);
            continue;
        }
        volume_manager_add(volume);
    }

    free(sector);
}

void VolumeManager_Init(void)
{
    volume_manager_ensure_init();
}

void VolumeManager_Rebuild(void)
{
    volume_manager_ensure_init();
    if (!s_volumes) return;

    volume_manager_clear();

    size_t device_count = BlockDevice_Count();
    for (size_t i = 0; i < device_count; ++i)
    {
        BlockDevice* device = BlockDevice_GetAt(i);
        if (!device)
            continue;

        uint32_t block_size = device->logical_block_size ? device->logical_block_size : 512;
        if (block_size == 0)
            block_size = 512;

        const char* base_name = device->name ? device->name : "disk";
        Volume* whole = volume_allocate(device,
                                        VOLUME_TYPE_WHOLE_DEVICE,
                                        base_name,
                                        0,
                                        device->total_blocks,
                                        block_size);
        if (whole)
        {
            whole->mbr_type = 0;
            volume_manager_add(whole);
        }

        if (device->type != BLKDEV_TYPE_CDROM)
        {
            volume_scan_mbr(device, block_size);
        }
    }
}

size_t VolumeManager_Count(void)
{
    if (!s_volumes) return 0;
    return s_volumes->count;
}

Volume* VolumeManager_GetAt(size_t index)
{
    if (!s_volumes) return NULL;
    return (Volume*)List_GetAt(s_volumes, index);
}

const char* Volume_Name(const Volume* volume)
{
    if (!volume) return NULL;
    return volume->name;
}

uint32_t Volume_BlockSize(const Volume* volume)
{
    if (!volume) return 0;
    return volume->block_size;
}

uint64_t Volume_Length(const Volume* volume)
{
    if (!volume) return 0;
    return volume->block_count;
}

uint64_t Volume_StartLBA(const Volume* volume)
{
    if (!volume) return 0;
    return volume->start_lba;
}

bool Volume_ReadSectors(Volume* volume, uint64_t lba, uint32_t count, void* buffer)
{
    if (!volume || !buffer || count == 0) return false;
    uint64_t absolute_lba = volume->start_lba + lba;
    if (absolute_lba + count > volume->start_lba + volume->block_count)
        return false;
    return BlockDevice_Read(volume->device, absolute_lba, count, buffer);
}

bool Volume_WriteSectors(Volume* volume, uint64_t lba, uint32_t count, const void* buffer)
{
    if (!volume || !buffer || count == 0) return false;
    uint64_t absolute_lba = volume->start_lba + lba;
    if (absolute_lba + count > volume->start_lba + volume->block_count)
        return false;
    return BlockDevice_Write(volume->device, absolute_lba, count, buffer);
}
