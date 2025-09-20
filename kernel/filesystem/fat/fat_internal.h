#pragma once

#include <filesystem/VFS.h>
#include <storage/BlockDevice.h>
#include <storage/Volume.h>
#include <list.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN    0x02
#define FAT_ATTR_SYSTEM    0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE   0x20
#define FAT_ATTR_LONG_NAME 0x0F

#define FAT_LONG_ENTRY_SEQ_MASK 0x1F

#define FAT_LONG_ENTRY_LAST 0x40

#pragma pack(push, 1)
typedef struct FAT_BootSector {
    uint8_t  jmpBoot[3];
    uint8_t  OEMName[8];
    uint16_t bytesPerSector;
    uint8_t  sectorsPerCluster;
    uint16_t reservedSectorCount;
    uint8_t  numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t  media;
    uint16_t FATSize16;
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;

    union {
        struct {
            uint8_t  driveNumber;
            uint8_t  reserved1;
            uint8_t  bootSignature;
            uint32_t volumeID;
            uint8_t  volumeLabel[11];
            uint8_t  fileSystemType[8];
            uint8_t  bootCode[448];
            uint16_t bootSectorSig;
        } fat16;
        struct {
            uint32_t FATSize32;
            uint16_t extFlags;
            uint16_t FSVersion;
            uint32_t rootCluster;
            uint16_t FSInfo;
            uint16_t backupBootSector;
            uint8_t  reserved[12];
            uint8_t  driveNumber;
            uint8_t  reserved1;
            uint8_t  bootSignature;
            uint32_t volumeID;
            uint8_t  volumeLabel[11];
            uint8_t  fileSystemType[8];
            uint8_t  bootCode[420];
            uint16_t bootSectorSig;
        } fat32;
    } spec;
} FAT_BootSector;

typedef struct FAT_DirEntry {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  ntRes;
    uint8_t  crtTimeTenth;
    uint16_t crtTime;
    uint16_t crtDate;
    uint16_t lstAccDate;
    uint16_t fstClusHI;
    uint16_t wrtTime;
    uint16_t wrtDate;
    uint16_t fstClusLO;
    uint32_t fileSize;
} FAT_DirEntry;

#pragma pack(pop)

typedef enum FATType {
    FAT_TYPE_INVALID = 0,
    FAT_TYPE_16,
    FAT_TYPE_32
} FATType;

struct FATVolume;
typedef struct FATVolume FATVolume;

typedef struct FATNodeInfo {
    FATVolume* volume;
    uint32_t first_cluster;
    uint32_t size;
    uint8_t  attr;
    bool     is_root;
    bool     overlay;
    uint8_t* overlay_data;
    size_t   overlay_size;
    size_t   overlay_capacity;
    List*    overlay_children;
} FATNodeInfo;

typedef struct FATVolume {
    BlockDevice* device;
    Volume* backing_volume;
    uint64_t lba_offset;
    FATType type;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t cluster_size_bytes;
    uint32_t reserved_sectors;
    uint32_t fat_count;
    uint32_t sectors_per_fat;
    uint32_t root_dir_entries;
    uint32_t root_dir_sectors;
    uint32_t fat_start_sector;
    uint32_t root_dir_sector;    // For FAT16 fixed root
    uint32_t first_data_sector;
    uint32_t cluster_count;
    uint32_t root_cluster;       // For FAT32
    uint64_t total_sectors;
    uint8_t  fat_bits;
    List*    nodes;              // All allocated VFS nodes for cleanup
} FATVolume;

// Common helpers
bool fat_volume_init(FATVolume* volume,
                     Volume* backing_volume,
                     BlockDevice* device,
                     uint64_t lba_offset,
                     const FAT_BootSector* bpb);
bool fat_volume_probe_type(FATVolume* volume, const FAT_BootSector* bpb);
bool fat_volume_read_sector(FATVolume* volume, uint32_t sector, void* buffer);
bool fat_volume_read_cluster(FATVolume* volume, uint32_t cluster, void* buffer);
bool fat_volume_is_end(FATVolume* volume, uint32_t value);
bool fat_volume_is_bad(FATVolume* volume, uint32_t value);
uint32_t fat_volume_get_next_cluster(FATVolume* volume, uint32_t cluster);
const char* fat_volume_type_name(FATVolume* volume);

// Type specific initialisation
bool fat16_configure(FATVolume* volume, const FAT_BootSector* bpb);
bool fat32_configure(FATVolume* volume, const FAT_BootSector* bpb);
