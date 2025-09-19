#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct Volume;
typedef struct Volume Volume;

#include <storage/BlockDevice.h>

typedef enum VolumeType {
    VOLUME_TYPE_WHOLE_DEVICE = 0,
    VOLUME_TYPE_MBR_PARTITION,
    VOLUME_TYPE_GPT_PARTITION,
} VolumeType;

struct Volume {
    char* name;                 // descriptive name (e.g., "disk0p1")
    VolumeType type;            // partitioning scheme
    BlockDevice* device;        // underlying block device
    uint64_t start_lba;         // starting LBA within the device
    uint64_t block_count;       // number of logical blocks
    uint32_t block_size;        // logical block size in bytes
    uint8_t  mbr_type;          // legacy MBR partition type (if applicable)
    uint64_t attributes;        // GPT attributes or driver-defined flags
    uint8_t  type_guid[16];     // GPT type GUID (zeroed if not GPT)
    uint8_t  unique_guid[16];   // GPT unique partition GUID
};

void VolumeManager_Init(void);
void VolumeManager_Rebuild(void);
size_t VolumeManager_Count(void);
Volume* VolumeManager_GetAt(size_t index);
const char* Volume_Name(const Volume* volume);
uint32_t Volume_BlockSize(const Volume* volume);
uint64_t Volume_Length(const Volume* volume);
uint64_t Volume_StartLBA(const Volume* volume);

bool Volume_ReadSectors(Volume* volume, uint64_t lba, uint32_t count, void* buffer);
bool Volume_WriteSectors(Volume* volume, uint64_t lba, uint32_t count, const void* buffer);

#ifdef __cplusplus
}
#endif

