#include <stream/FileStream.h>
#include <memory/memory.h>

static FileStream* filestream_alloc(VFS_HANDLE handle, uint32_t mode, bool take_ownership)
{
    if (!handle)
        return NULL;

    FileStream* stream = (FileStream*)malloc(sizeof(FileStream));
    if (!stream)
    {
        return NULL;
    }

    stream->handle = handle;
    stream->mode = mode ? mode : handle->mode;
    stream->owns_handle = take_ownership;
    return stream;
}

FileStream* FileStream_Create(VFS_HANDLE handle, uint32_t mode, bool take_ownership)
{
    FileStream* stream = filestream_alloc(handle, mode, take_ownership);
    if (!stream && take_ownership && handle)
    {
        VFS_Close(handle);
    }
    return stream;
}

FileStream* FileStream_Open(const char* path, uint32_t mode)
{
    VFS_HANDLE handle = VFS_Open(path, mode);
    if (!handle)
        return NULL;

    FileStream* stream = filestream_alloc(handle, mode, true);
    if (!stream)
    {
        VFS_Close(handle);
    }
    return stream;
}

void FileStream_Close(FileStream* stream)
{
    if (!stream)
        return;

    if (stream->handle && stream->owns_handle)
    {
        VFS_Close(stream->handle);
    }

    stream->handle = NULL;
    free(stream);
}

bool FileStream_IsOpen(const FileStream* stream)
{
    return stream && stream->handle;
}

bool FileStream_CanRead(const FileStream* stream)
{
    if (!stream || !stream->handle)
        return false;

    if (stream->mode & VFS_OPEN_READ)
        return true;

    if (!(stream->mode & (VFS_OPEN_READ | VFS_OPEN_WRITE)))
        return true;

    return false;
}

bool FileStream_CanWrite(const FileStream* stream)
{
    if (!stream || !stream->handle)
        return false;

    return (stream->mode & VFS_OPEN_WRITE) != 0;
}

int64_t FileStream_Read(FileStream* stream, void* buffer, size_t size)
{
    if (!FileStream_CanRead(stream))
        return -1;
    return VFS_Read(stream->handle, buffer, size);
}

int64_t FileStream_ReadAt(FileStream* stream, uint64_t offset, void* buffer, size_t size)
{
    if (!FileStream_CanRead(stream))
        return -1;
    return VFS_ReadAt(stream->handle, offset, buffer, size);
}

int64_t FileStream_Write(FileStream* stream, const void* buffer, size_t size)
{
    if (!FileStream_CanWrite(stream))
        return -1;
    return VFS_Write(stream->handle, buffer, size);
}

int64_t FileStream_WriteAt(FileStream* stream, uint64_t offset, const void* buffer, size_t size)
{
    if (!FileStream_CanWrite(stream))
        return -1;
    return VFS_WriteAt(stream->handle, offset, buffer, size);
}

VFSResult FileStream_Seek(FileStream* stream, int64_t offset, VFSSeekWhence whence, uint64_t* out_position)
{
    if (!stream || !stream->handle)
        return VFS_RES_INVALID;
    return VFS_SeekHandle(stream->handle, offset, whence, out_position);
}

VFSResult FileStream_Truncate(FileStream* stream, uint64_t length)
{
    if (!stream || !stream->handle)
        return VFS_RES_INVALID;
    return VFS_TruncateHandle(stream->handle, length);
}

uint64_t FileStream_Tell(const FileStream* stream)
{
    if (!stream || !stream->handle)
        return 0;
    return stream->handle->offset;
}

VFS_HANDLE FileStream_Handle(FileStream* stream)
{
    return stream ? stream->handle : NULL;
}
