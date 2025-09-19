#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <filesystem/VFS.h>
#include <storage/Volume.h>

// Register the NTFS filesystem driver with the VFS registry.
void NTFS_Register(void);

// Helper to mount an NTFS formatted volume at the given path.
VFSResult NTFS_Mount(Volume* volume, const char* mount_path);

#ifdef __cplusplus
}
#endif
