#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VFS_NAME_MAX 255
#define VFS_PATH_MAX 1024

struct VFSNode;
struct VFSHandle;
struct VFSFileSystem;
struct VFSMount;
struct BlockDevice;
struct Volume;
struct FileStream;
struct List;

typedef struct VFSNode VFSNode;
typedef struct VFSHandle VFSHandle;
typedef struct VFSFileSystem VFSFileSystem;
typedef struct VFSMount VFSMount;
typedef struct BlockDevice BlockDevice;
typedef struct Volume Volume;
typedef VFSHandle* VFS_HANDLE;
typedef struct List List;

typedef enum VFSNodeType {
    VFS_NODE_UNKNOWN = 0,
    VFS_NODE_DIRECTORY,
    VFS_NODE_REGULAR,
    VFS_NODE_SYMLINK,
    VFS_NODE_CHAR_DEVICE,
    VFS_NODE_BLOCK_DEVICE,
    VFS_NODE_PIPE,
    VFS_NODE_SOCKET
} VFSNodeType;

typedef enum VFSNodeFlags {
    VFS_NODE_FLAG_NONE       = 0,
    VFS_NODE_FLAG_READONLY   = 1u << 0,
    VFS_NODE_FLAG_MOUNTPOINT = 1u << 1,
    VFS_NODE_FLAG_HIDDEN     = 1u << 2
} VFSNodeFlags;

typedef enum VFSOpenMode {
    VFS_OPEN_READ    = 1u << 0,
    VFS_OPEN_WRITE   = 1u << 1,
    VFS_OPEN_APPEND  = 1u << 2,
    VFS_OPEN_CREATE  = 1u << 3,
    VFS_OPEN_TRUNC   = 1u << 4,
    VFS_OPEN_EXCL    = 1u << 5
} VFSOpenMode;

typedef enum VFSSeekWhence {
    VFS_SEEK_SET = 0,
    VFS_SEEK_CUR = 1,
    VFS_SEEK_END = 2
} VFSSeekWhence;

typedef enum VFSResult {
    VFS_RES_OK         = 0,
    VFS_RES_ERROR      = -1,
    VFS_RES_NOT_FOUND  = -2,
    VFS_RES_INVALID    = -3,
    VFS_RES_UNSUPPORTED= -4,
    VFS_RES_NO_MEMORY  = -5,
    VFS_RES_EXISTS     = -6,
    VFS_RES_BUSY       = -7,
    VFS_RES_NO_SPACE   = -8,
    VFS_RES_ACCESS     = -9
} VFSResult;

typedef struct VFSDirEntry {
    char name[VFS_NAME_MAX + 1];
    VFSNodeType type;
} VFSDirEntry;

typedef struct VFSNodeInfo {
    VFSNodeType type;
    uint32_t flags;
    uint64_t size;
    uint64_t inode;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
} VFSNodeInfo;

typedef struct VFSMountParams {
    const char* source;         // Optional source descriptor (path, label, etc.)
    BlockDevice* block_device;  // Optional backing block device
    Volume* volume;             // Optional logical volume/partition
    void* context;              // Driver-specific pointer
    uint32_t flags;             // Mount flags (driver-defined)
} VFSMountParams;

typedef struct VFSNodeOps {
    VFSResult (*open)(VFSNode* node, uint32_t mode, void** out_handle);
    VFSResult (*close)(VFSNode* node, void* handle);
    int64_t   (*read)(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size);
    int64_t   (*write)(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size);
    VFSResult (*truncate)(VFSNode* node, void* handle, uint64_t length);
    VFSResult (*readdir)(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry);
    VFSResult (*lookup)(VFSNode* node, const char* name, VFSNode** out_node);
    VFSResult (*create)(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node);
    VFSResult (*remove)(VFSNode* node, const char* name);
    VFSResult (*stat)(VFSNode* node, VFSNodeInfo* out_info);
} VFSNodeOps;

typedef struct VFSFileSystemOps {
    bool      (*probe)(VFSFileSystem* fs, const VFSMountParams* params);
    VFSResult (*mount)(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root);
    VFSResult (*unmount)(VFSFileSystem* fs, VFSNode* root);
} VFSFileSystemOps;

typedef struct VFSFileSystem {
    const char* name;
    uint32_t flags;
    const VFSFileSystemOps* ops;
    void* driver_context;
} VFSFileSystem;

struct VFSNode {
    char* name;
    VFSNodeType type;
    uint32_t flags;
    VFSNode* parent;
    VFSMount* mount;
    const VFSNodeOps* ops;
    void* internal_data; // Filesystem-private payload
};

struct VFSHandle {
    VFSNode* node;
    void* driver_handle;
    uint32_t mode;
    uint64_t offset;
};

typedef struct VFSCacheStats {
    size_t hits;
    size_t misses;
    size_t entries;
    size_t capacity;
} VFSCacheStats;

// Initialization and registration
void       VFS_Init(void);
bool       VFS_IsInitialized(void);
void       VFS_CacheFlush(void);
void       VFS_CacheSetCapacity(size_t capacity);
void       VFS_CacheResetStats(void);
void       VFS_CacheGetStats(VFSCacheStats* out_stats);
void       VFS_CacheDumpStats(void);
VFSResult  VFS_RegisterFileSystem(VFSFileSystem* fs);
VFSFileSystem* VFS_GetFileSystem(const char* name);

// Mount management
VFSMount*  VFS_Mount(const char* target, VFSFileSystem* fs, const VFSMountParams* params);
VFSMount*  VFS_MountAuto(const char* target, const VFSMountParams* params);
VFSFileSystem* VFS_DetectFileSystem(const VFSMountParams* params);
VFSResult  VFS_Unmount(const char* target);
VFSMount*  VFS_GetMount(const char* target);
VFSNode*   VFS_GetMountRoot(VFSMount* mount);

// Path helpers
VFSResult  VFS_Resolve(const char* path, VFSNode** out_node);
VFSResult  VFS_ResolveAt(VFSNode* start, const char* path, VFSNode** out_node, bool follow_last_link);

// Basic node operations
const char* VFS_NodeName(const VFSNode* node);
VFSNodeType VFS_NodeTypeOf(const VFSNode* node);
VFSNode*    VFS_NodeParent(VFSNode* node);
VFSResult   VFS_NodeStat(VFSNode* node, VFSNodeInfo* out_info);
VFSResult   VFS_ReadDir(VFSNode* directory, size_t index, VFSDirEntry* out_entry);
VFSResult   VFS_Create(const char* path, VFSNodeType type);
VFSResult   VFS_Remove(const char* path);
bool        VFS_DirectoryExists(const char* path);
bool        VFS_FileExists(const char* path);
List*       VFS_GetDirectoryContents(const char* path);
void        VFS_FreeDirectoryContents(List* contents);

// File handle operations
VFS_HANDLE  VFS_Open(const char* path, uint32_t mode);
VFSResult   VFS_Close(VFS_HANDLE handle);
int64_t     VFS_Read(VFS_HANDLE handle, void* buffer, size_t size);
int64_t     VFS_Write(VFS_HANDLE handle, const void* buffer, size_t size);
int64_t     VFS_ReadAt(VFS_HANDLE handle, uint64_t offset, void* buffer, size_t size);
int64_t     VFS_WriteAt(VFS_HANDLE handle, uint64_t offset, const void* buffer, size_t size);
VFSResult   VFS_TruncateHandle(VFS_HANDLE handle, uint64_t length);
VFSResult   VFS_SeekHandle(VFS_HANDLE handle, int64_t offset, VFSSeekWhence whence, uint64_t* out_position);
struct FileStream* VFS_OpenFileStream(const char* path, uint32_t mode);

#ifdef __cplusplus
}
#endif
