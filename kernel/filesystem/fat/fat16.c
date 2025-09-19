#include "fat_internal.h"

bool fat16_configure(FATVolume* volume, const FAT_BootSector* bpb)
{
    if (!volume || !bpb) return false;
    if (bpb->FATSize16 == 0) return false;

    volume->fat_bits = 16;
    volume->sectors_per_fat = bpb->FATSize16;
    volume->root_dir_entries = bpb->rootEntryCount;
    volume->root_dir_sectors = ((volume->root_dir_entries * 32) + (volume->bytes_per_sector - 1)) / volume->bytes_per_sector;
    volume->fat_start_sector = volume->reserved_sectors;
    volume->root_dir_sector = volume->fat_start_sector + (volume->fat_count * volume->sectors_per_fat);
    volume->first_data_sector = volume->root_dir_sector + volume->root_dir_sectors;
    volume->root_cluster = 0;
    volume->cluster_size_bytes = volume->bytes_per_sector * volume->sectors_per_cluster;
    return true;
}

