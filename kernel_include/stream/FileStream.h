#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <filesystem/VFS.h>

typedef struct FileStream FileStream;

struct FileStream {
    VFS_HANDLE handle;
    uint32_t mode;
    bool owns_handle;
};

FileStream* FileStream_Create(VFS_HANDLE handle, uint32_t mode, bool take_ownership);
FileStream* FileStream_Open(const char* path, uint32_t mode);
void        FileStream_Close(FileStream* stream);
bool        FileStream_IsOpen(const FileStream* stream);
bool        FileStream_CanRead(const FileStream* stream);
bool        FileStream_CanWrite(const FileStream* stream);
int64_t     FileStream_Read(FileStream* stream, void* buffer, size_t size);
int64_t     FileStream_ReadAt(FileStream* stream, uint64_t offset, void* buffer, size_t size);
int64_t     FileStream_Write(FileStream* stream, const void* buffer, size_t size);
int64_t     FileStream_WriteAt(FileStream* stream, uint64_t offset, const void* buffer, size_t size);
VFSResult   FileStream_Seek(FileStream* stream, int64_t offset, VFSSeekWhence whence, uint64_t* out_position);
VFSResult   FileStream_Truncate(FileStream* stream, uint64_t length);
uint64_t    FileStream_Tell(const FileStream* stream);
VFS_HANDLE  FileStream_Handle(FileStream* stream);

#ifdef __cplusplus
}
#endif

