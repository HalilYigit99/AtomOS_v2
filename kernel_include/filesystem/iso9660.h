#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <filesystem/VFS.h>
#include <storage/BlockDevice.h>

// Register the ISO9660 filesystem driver.
void ISO9660_Register(void);

// Mount helper that probes the given block device and mounts it at the
// provided path. Returns VFS_RES_OK on success.
VFSResult ISO9660_Mount(BlockDevice* device, const char* mount_path);

#ifdef __cplusplus
}
#endif

