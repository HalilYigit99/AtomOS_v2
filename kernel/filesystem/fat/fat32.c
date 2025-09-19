#include "fat_internal.h"

bool fat32_configure(FATVolume* volume, const FAT_BootSector* bpb)
{
    if (!volume || !bpb) return false;
    uint32_t fat_size = bpb->spec.fat32.FATSize32;
    if (fat_size == 0) return false;

    volume->fat_bits = 32;
    volume->sectors_per_fat = fat_size;
    volume->root_dir_entries = 0;
    volume->root_dir_sectors = 0;
    volume->fat_start_sector = volume->reserved_sectors;
    volume->first_data_sector = volume->fat_start_sector + (volume->fat_count * volume->sectors_per_fat);
    volume->root_dir_sector = 0;
    volume->root_cluster = bpb->spec.fat32.rootCluster;
    if (volume->root_cluster < 2)
        volume->root_cluster = 2;
    volume->cluster_size_bytes = volume->bytes_per_sector * volume->sectors_per_cluster;
    return true;
}

