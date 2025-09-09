#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

typedef enum {
    DISKSTREAM_DEVICE_UNKNOWN = 0,
    DISKSTREAM_DEVICE_BLOCK = 1,
    // Future: DISKSTREAM_DEVICE_CHAR = 2, etc...
} DiskStreamDeviceType;

typedef struct {
    void* device; // Eg: BlockDevice*
    DiskStreamDeviceType device_type; // 0: Unknown , 1: BlockDevice , etc...
    bool isOpen;
    bool readonly;
} DiskStream;

DiskStream* DiskStream_CreateFromBlockDevice(void* blockDevice);

void* DiskStream_Open(DiskStream* stream, bool readonly);
void DiskStream_Close(DiskStream* stream);

void* DiskStream_ReadSector(DiskStream* stream, uint64_t sector);
void* DiskStream_ReadSectors(DiskStream* stream, uint64_t sector, size_t count);

uint8_t DiskStream_Read8(DiskStream* stream, uint64_t offset);
uint16_t DiskStream_Read16(DiskStream* stream, uint64_t offset);
uint32_t DiskStream_Read32(DiskStream* stream, uint64_t offset);
uint64_t DiskStream_Read64(DiskStream* stream, uint64_t offset);

void* DiskStream_Read(DiskStream* stream, uint64_t offset, size_t size);

void DiskStream_Wrtie8(DiskStream* stream, uint64_t offset, uint8_t value);
void DiskStream_Wrtie16(DiskStream* stream, uint64_t offset, uint16_t value);
void DiskStream_Wrtie32(DiskStream* stream, uint64_t offset, uint32_t value);
void DiskStream_Wrtie64(DiskStream* stream, uint64_t offset, uint64_t value);

void DiskStream_Write(DiskStream* stream, uint64_t offset, const void* data, size_t size);

void DiskStream_Destroy(DiskStream* stream);

#ifdef __cplusplus
}
#endif