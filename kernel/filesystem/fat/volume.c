#include "fat_internal.h"
#include <memory/memory.h>
#include <util/string.h>

static uint32_t fat_read_fat_entry(FATVolume* volume, uint32_t cluster)
{
    uint32_t entry_size = (volume->fat_bits == 32) ? 4u : 2u;
    uint32_t fat_offset = cluster * entry_size;
    uint32_t sector = volume->fat_start_sector + (fat_offset / volume->bytes_per_sector);
    uint32_t offset = fat_offset % volume->bytes_per_sector;

    uint8_t* buffer = (uint8_t*)malloc(volume->bytes_per_sector);
    if (!buffer)
        return 0xFFFFFFFFu;

    if (!fat_volume_read_sector(volume, sector, buffer))
    {
        free(buffer);
        return 0xFFFFFFFFu;
    }

    uint32_t value;
    if (volume->fat_bits == 32)
    {
        value = *((uint32_t*)(buffer + offset)) & 0x0FFFFFFFu;
    }
    else
    {
        value = *((uint16_t*)(buffer + offset)) & 0xFFFFu;
    }

    free(buffer);
    return value;
}

bool fat_volume_read_sector(FATVolume* volume, uint32_t sector, void* buffer)
{
    if (!volume || !buffer) return false;
    if (volume->backing_volume)
    {
        return Volume_ReadSectors(volume->backing_volume, sector, 1, buffer);
    }
    if (!volume->device) return false;
    return BlockDevice_Read(volume->device, volume->lba_offset + sector, 1, buffer);
}

bool fat_volume_read_cluster(FATVolume* volume, uint32_t cluster, void* buffer)
{
    if (!volume || !buffer) return false;
    if (cluster < 2) return false;

    uint32_t first_sector = volume->first_data_sector + (cluster - 2) * volume->sectors_per_cluster;
    uint32_t sectors = volume->sectors_per_cluster;
    if (volume->backing_volume)
    {
        return Volume_ReadSectors(volume->backing_volume, first_sector, sectors, buffer);
    }
    return BlockDevice_Read(volume->device, volume->lba_offset + first_sector, sectors, buffer);
}

uint32_t fat_volume_get_next_cluster(FATVolume* volume, uint32_t cluster)
{
    if (!volume) return 0xFFFFFFFFu;
    uint32_t value = fat_read_fat_entry(volume, cluster);
    return value;
}

bool fat_volume_is_end(FATVolume* volume, uint32_t value)
{
    if (!volume) return true;
    if (value < 2) return true;
    if (value == 0xFFFFFFFFu) return true;
    if (volume->fat_bits == 32)
    {
        return value >= 0x0FFFFFF8u;
    }
    return value >= 0xFFF8u;
}

bool fat_volume_is_bad(FATVolume* volume, uint32_t value)
{
    if (!volume) return true;
    if (volume->fat_bits == 32)
    {
        return value == 0x0FFFFFF7u;
    }
    return value == 0xFFF7u;
}

const char* fat_volume_type_name(FATVolume* volume)
{
    if (!volume) return "unknown";
    switch (volume->type)
    {
        case FAT_TYPE_16: return "FAT16";
        case FAT_TYPE_32: return "FAT32";
        default: return "unsupported";
    }
}

bool fat_volume_probe_type(FATVolume* volume, const FAT_BootSector* bpb)
{
    if (!volume || !bpb) return false;
    uint32_t bytes_per_sector = bpb->bytesPerSector;
    uint32_t sectors_per_cluster = bpb->sectorsPerCluster;
    uint32_t reserved = bpb->reservedSectorCount;
    uint32_t fats = bpb->numFATs;

    if (bytes_per_sector == 0 || sectors_per_cluster == 0 || fats == 0)
        return false;

    uint32_t total_sectors = bpb->totalSectors16 ? bpb->totalSectors16 : bpb->totalSectors32;
    if (total_sectors == 0) return false;

    uint32_t fat_size = bpb->FATSize16;
    if (fat_size == 0)
        fat_size = bpb->spec.fat32.FATSize32;
    if (fat_size == 0) return false;

    uint32_t root_dir_entries = bpb->rootEntryCount;
    uint32_t root_dir_sectors = ((root_dir_entries * 32) + (bytes_per_sector - 1)) / bytes_per_sector;
    uint32_t data_sectors = total_sectors - (reserved + (fats * fat_size) + root_dir_sectors);
    uint32_t cluster_count = data_sectors / sectors_per_cluster;

    volume->total_sectors = total_sectors;
    volume->cluster_count = cluster_count;
    volume->root_dir_sectors = root_dir_sectors;

    if (cluster_count < 4085)
    {
        return false; // FAT12 unsupported
    }
    else if (cluster_count < 65525)
    {
        volume->type = FAT_TYPE_16;
    }
    else
    {
        volume->type = FAT_TYPE_32;
    }

    return true;
}

bool fat_volume_init(FATVolume* volume,
                     Volume* backing_volume,
                     BlockDevice* device,
                     uint64_t lba_offset,
                     const FAT_BootSector* bpb)
{
    if (!volume || !bpb) return false;

    volume->device = device;
    volume->backing_volume = backing_volume;
    volume->lba_offset = lba_offset;
    volume->bytes_per_sector = bpb->bytesPerSector;
    volume->sectors_per_cluster = bpb->sectorsPerCluster;
    volume->reserved_sectors = bpb->reservedSectorCount;
    volume->fat_count = bpb->numFATs;
    volume->root_dir_entries = bpb->rootEntryCount;
    volume->cluster_size_bytes = volume->bytes_per_sector * volume->sectors_per_cluster;
    volume->nodes = NULL;

    if (!fat_volume_probe_type(volume, bpb))
        return false;

    volume->fat_start_sector = volume->reserved_sectors;

    switch (volume->type)
    {
        case FAT_TYPE_16:
            if (!fat16_configure(volume, bpb))
                return false;
            break;
        case FAT_TYPE_32:
            if (!fat32_configure(volume, bpb))
                return false;
            break;
        default:
            return false;
    }

    return true;
}
