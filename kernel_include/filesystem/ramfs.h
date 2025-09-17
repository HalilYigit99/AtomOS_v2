#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <filesystem/VFS.h>

VFSFileSystem* RamFS_Create(const char* label);
void           RamFS_Destroy(VFSFileSystem* fs);

#ifdef __cplusplus
}
#endif

