#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <filesystem/VFS.h>
#include <storage/Volume.h>

// Register the FAT filesystem driver with the global VFS registry.
void FATFS_Register(void);

// Convenience helper to mount a FAT-formatted volume at the given path.
// Returns VFS_RES_OK on success.
VFSResult FATFS_Mount(Volume* volume, const char* mount_path);

#ifdef __cplusplus
}
#endif
