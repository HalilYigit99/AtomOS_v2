#include <filesystem/VFS.h>
#include <list.h>
#include <memory/memory.h>
#include <util/string.h>
#include <debug/debug.h>

#define VFS_MAX_SEGMENTS (VFS_PATH_MAX / 2)

struct VFSMount {
    char* path;
    VFSFileSystem* fs;
    VFSNode* root;
    uint32_t flags;
};

static bool s_vfs_initialized = false;
static List* s_filesystems = NULL;
static List* s_mounts = NULL;
static VFSMount* s_root_mount = NULL;

static VFSResult vfs_normalize_path(const char* path, char* out_path, size_t out_size);
static VFSMount* vfs_select_mount(const char* normalized_path);
static VFSResult vfs_walk(VFSNode* start, const char* relative_path, VFSNode** out_node, bool follow_last_link);

static void vfs_attach_mount_to_tree(VFSMount* mount)
{
    if (!mount || !mount->root) return;
    mount->root->mount = mount;
    mount->root->flags |= VFS_NODE_FLAG_MOUNTPOINT;
    mount->root->parent = NULL;
}

void VFS_Init(void)
{
    if (s_vfs_initialized) return;

    s_filesystems = List_Create();
    s_mounts = List_Create();

    if (!s_filesystems || !s_mounts)
    {
        ERROR("VFS_Init: failed to allocate lists");
        return;
    }

    s_vfs_initialized = true;
}

bool VFS_IsInitialized(void)
{
    return s_vfs_initialized;
}

VFSResult VFS_RegisterFileSystem(VFSFileSystem* fs)
{
    if (!s_vfs_initialized || !fs || !fs->name || !fs->ops || !fs->ops->mount || !fs->ops->probe)
        return VFS_RES_INVALID;

    if (!s_filesystems) return VFS_RES_ERROR;

    for (ListNode* it = List_Foreach_Begin(s_filesystems); it; it = List_Foreach_Next(it))
    {
        VFSFileSystem* existing = (VFSFileSystem*)List_Foreach_Data(it);
        if (existing && strcmp(existing->name, fs->name) == 0)
        {
            return VFS_RES_EXISTS;
        }
    }

    List_Add(s_filesystems, fs);
    LOG("VFS: registered filesystem '%s'", fs->name);
    return VFS_RES_OK;
}

VFSFileSystem* VFS_GetFileSystem(const char* name)
{
    if (!s_vfs_initialized || !name || !s_filesystems) return NULL;
    for (ListNode* it = List_Foreach_Begin(s_filesystems); it; it = List_Foreach_Next(it))
    {
        VFSFileSystem* fs = (VFSFileSystem*)List_Foreach_Data(it);
        if (fs && strcmp(fs->name, name) == 0) return fs;
    }
    return NULL;
}

VFSMount* VFS_Mount(const char* target, VFSFileSystem* fs, const VFSMountParams* params)
{
    if (!s_vfs_initialized || !target || !fs || !fs->ops || !fs->ops->mount)
        return NULL;

    char normalized[VFS_PATH_MAX];
    VFSResult norm_res = vfs_normalize_path(target, normalized, sizeof(normalized));
    if (norm_res != VFS_RES_OK)
    {
        WARN("VFS_Mount: invalid mount path '%s'", target);
        return NULL;
    }

    if (List_Size(s_mounts) > 0)
    {
        for (ListNode* it = List_Foreach_Begin(s_mounts); it; it = List_Foreach_Next(it))
        {
            VFSMount* m = (VFSMount*)List_Foreach_Data(it);
            if (m && strcmp(m->path, normalized) == 0)
            {
                WARN("VFS_Mount: path '%s' already mounted", normalized);
                return NULL;
            }
        }
    }

    VFSNode* root_node = NULL;
    VFSResult mount_res = fs->ops->mount(fs, params, &root_node);
    if (mount_res != VFS_RES_OK || !root_node)
    {
        WARN("VFS_Mount: filesystem '%s' mount handler failed (%d)", fs->name, mount_res);
        return NULL;
    }

    VFSMount* mount = (VFSMount*)malloc(sizeof(VFSMount));
    if (!mount)
    {
        ERROR("VFS_Mount: out of memory");
        if (fs->ops->unmount)
        {
            fs->ops->unmount(fs, root_node);
        }
        return NULL;
    }

    mount->path = strdup(normalized);
    if (!mount->path)
    {
        ERROR("VFS_Mount: strdup failed");
        if (fs->ops->unmount)
        {
            fs->ops->unmount(fs, root_node);
        }
        free(mount);
        return NULL;
    }

    mount->fs = fs;
    mount->root = root_node;
    mount->flags = params ? params->flags : 0;

    vfs_attach_mount_to_tree(mount);

    List_Add(s_mounts, mount);

    if (strcmp(mount->path, "/") == 0)
    {
        s_root_mount = mount;
    }

    LOG("VFS: mounted '%s' at '%s'", fs->name, mount->path);
    return mount;
}

VFSFileSystem* VFS_DetectFileSystem(const VFSMountParams* params)
{
    if (!s_vfs_initialized || !params || !s_filesystems)
        return NULL;

    for (ListNode* it = List_Foreach_Begin(s_filesystems); it; it = List_Foreach_Next(it))
    {
        VFSFileSystem* fs = (VFSFileSystem*)List_Foreach_Data(it);
        if (!fs || !fs->ops || !fs->ops->probe)
            continue;
        bool matches = fs->ops->probe(fs, params);
        if (matches)
            return fs;
    }

    return NULL;
}

VFSMount* VFS_MountAuto(const char* target, const VFSMountParams* params)
{
    if (!s_vfs_initialized || !target || !params)
        return NULL;

    if (!s_filesystems || List_IsEmpty(s_filesystems))
        return NULL;

    VFSMount* mount = NULL;
    for (ListNode* it = List_Foreach_Begin(s_filesystems); it; it = List_Foreach_Next(it))
    {
        VFSFileSystem* fs = (VFSFileSystem*)List_Foreach_Data(it);
        if (!fs || !fs->ops || !fs->ops->probe)
            continue;

        if (!fs->ops->probe(fs, params))
            continue;

        mount = VFS_Mount(target, fs, params);
        if (mount)
            break;
    }

    return mount;
}

VFSResult VFS_Unmount(const char* target)
{
    if (!s_vfs_initialized || !target) return VFS_RES_INVALID;

    char normalized[VFS_PATH_MAX];
    VFSResult norm_res = vfs_normalize_path(target, normalized, sizeof(normalized));
    if (norm_res != VFS_RES_OK) return norm_res;

    if (!s_mounts || List_IsEmpty(s_mounts)) return VFS_RES_NOT_FOUND;

    size_t index = 0;
    for (ListNode* it = List_Foreach_Begin(s_mounts); it; it = List_Foreach_Next(it), ++index)
    {
        VFSMount* mount = (VFSMount*)List_Foreach_Data(it);
        if (!mount) continue;
        if (strcmp(mount->path, normalized) == 0)
        {
            if (strcmp(mount->path, "/") == 0)
            {
                return VFS_RES_BUSY;
            }

            if (mount->fs && mount->fs->ops && mount->fs->ops->unmount)
            {
                mount->fs->ops->unmount(mount->fs, mount->root);
            }

            free(mount->path);
            free(mount);
            List_RemoveAt(s_mounts, index);
            return VFS_RES_OK;
        }
    }

    return VFS_RES_NOT_FOUND;
}

VFSMount* VFS_GetMount(const char* target)
{
    if (!s_vfs_initialized || !target || !s_mounts) return NULL;

    char normalized[VFS_PATH_MAX];
    if (vfs_normalize_path(target, normalized, sizeof(normalized)) != VFS_RES_OK)
        return NULL;

    for (ListNode* it = List_Foreach_Begin(s_mounts); it; it = List_Foreach_Next(it))
    {
        VFSMount* mount = (VFSMount*)List_Foreach_Data(it);
        if (mount && strcmp(mount->path, normalized) == 0)
            return mount;
    }
    return NULL;
}

VFSNode* VFS_GetMountRoot(VFSMount* mount)
{
    if (!mount) return NULL;
    return mount->root;
}

VFSResult VFS_Resolve(const char* path, VFSNode** out_node)
{
    if (!s_vfs_initialized || !path || !out_node) return VFS_RES_INVALID;
    if (!s_root_mount) return VFS_RES_NOT_FOUND;

    char normalized[VFS_PATH_MAX];
    VFSResult norm_res = vfs_normalize_path(path, normalized, sizeof(normalized));
    if (norm_res != VFS_RES_OK) return norm_res;

    VFSMount* mount = vfs_select_mount(normalized);
    if (!mount) return VFS_RES_NOT_FOUND;

    const char* rel = normalized;
    size_t mount_len = strlen(mount->path);

    if (strcmp(mount->path, "/") == 0)
    {
        if (rel[0] == '/') rel++;
    }
    else
    {
        rel += mount_len;
        if (*rel == '/') rel++;
    }

    if (!*rel)
    {
        *out_node = mount->root;
        return VFS_RES_OK;
    }

    return vfs_walk(mount->root, rel, out_node, true);
}

VFSResult VFS_ResolveAt(VFSNode* start, const char* path, VFSNode** out_node, bool follow_last_link)
{
    if (!path || !out_node) return VFS_RES_INVALID;

    if (path[0] == '/')
    {
        return VFS_Resolve(path, out_node);
    }

    VFSNode* base = start ? start : (s_root_mount ? s_root_mount->root : NULL);
    if (!base) return VFS_RES_NOT_FOUND;

    if (!path[0])
    {
        *out_node = base;
        return VFS_RES_OK;
    }

    return vfs_walk(base, path, out_node, follow_last_link);
}

const char* VFS_NodeName(const VFSNode* node)
{
    if (!node) return NULL;
    return node->name;
}

VFSNodeType VFS_NodeTypeOf(const VFSNode* node)
{
    if (!node) return VFS_NODE_UNKNOWN;
    return node->type;
}

VFSNode* VFS_NodeParent(VFSNode* node)
{
    if (!node) return NULL;
    return node->parent;
}

VFSResult VFS_NodeStat(VFSNode* node, VFSNodeInfo* out_info)
{
    if (!node || !out_info) return VFS_RES_INVALID;
    if (node->ops && node->ops->stat)
    {
        return node->ops->stat(node, out_info);
    }

    out_info->type = node->type;
    out_info->flags = node->flags;
    out_info->size = 0;
    out_info->inode = 0;
    out_info->atime = 0;
    out_info->mtime = 0;
    out_info->ctime = 0;
    return VFS_RES_OK;
}

VFSResult VFS_ReadDir(VFSNode* directory, size_t index, VFSDirEntry* out_entry)
{
    if (!directory || !out_entry) return VFS_RES_INVALID;
    if (directory->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;
    if (!directory->ops || !directory->ops->readdir) return VFS_RES_UNSUPPORTED;
    return directory->ops->readdir(directory, NULL, index, out_entry);
}

VFS_HANDLE VFS_Open(const char* path, uint32_t mode)
{
    if (!path) return NULL;
    VFSNode* node = NULL;
    if (VFS_Resolve(path, &node) != VFS_RES_OK)
    {
        return NULL;
    }

    VFSHandle* handle = (VFSHandle*)malloc(sizeof(VFSHandle));
    if (!handle) return NULL;

    handle->node = node;
    handle->driver_handle = NULL;
    handle->mode = mode;
    handle->offset = 0;

    if (node->ops && node->ops->open)
    {
        VFSResult res = node->ops->open(node, mode, &handle->driver_handle);
        if (res != VFS_RES_OK)
        {
            free(handle);
            return NULL;
        }
    }

    return handle;
}

VFSResult VFS_Close(VFS_HANDLE handle)
{
    if (!handle) return VFS_RES_INVALID;
    VFSResult res = VFS_RES_OK;
    if (handle->node && handle->node->ops && handle->node->ops->close)
    {
        res = handle->node->ops->close(handle->node, handle->driver_handle);
    }
    free(handle);
    return res;
}

static bool vfs_handle_can_read(VFS_HANDLE handle)
{
    if (!handle) return false;
    if (handle->mode & VFS_OPEN_READ) return true;
    if (!(handle->mode & (VFS_OPEN_READ | VFS_OPEN_WRITE)))
    {
        return true; // default read-only if no explicit flags were provided
    }
    return false;
}

int64_t VFS_Read(VFS_HANDLE handle, void* buffer, size_t size)
{
    if (!handle || !buffer || size == 0) return -1;
    if (!vfs_handle_can_read(handle)) return -1;
    if (!handle->node || !handle->node->ops || !handle->node->ops->read)
        return -1;

    int64_t read_bytes = handle->node->ops->read(handle->node,
                                                  handle->driver_handle,
                                                  handle->offset,
                                                  buffer,
                                                  size);
    if (read_bytes > 0)
    {
        handle->offset += (uint64_t)read_bytes;
    }
    return read_bytes;
}

int64_t VFS_ReadAt(VFS_HANDLE handle, uint64_t offset, void* buffer, size_t size)
{
    if (!handle || !buffer || size == 0) return -1;
    if (!vfs_handle_can_read(handle)) return -1;
    if (!handle->node || !handle->node->ops || !handle->node->ops->read)
        return -1;
    return handle->node->ops->read(handle->node, handle->driver_handle, offset, buffer, size);
}

static bool vfs_handle_can_write(VFS_HANDLE handle)
{
    if (!handle) return false;
    return (handle->mode & VFS_OPEN_WRITE) != 0;
}

int64_t VFS_Write(VFS_HANDLE handle, const void* buffer, size_t size)
{
    if (!handle || !buffer || size == 0) return -1;
    if (!vfs_handle_can_write(handle)) return -1;
    if (!handle->node || !handle->node->ops || !handle->node->ops->write)
        return -1;

    int64_t written = handle->node->ops->write(handle->node,
                                               handle->driver_handle,
                                               handle->offset,
                                               buffer,
                                               size);
    if (written > 0)
    {
        handle->offset += (uint64_t)written;
    }
    return written;
}

int64_t VFS_WriteAt(VFS_HANDLE handle, uint64_t offset, const void* buffer, size_t size)
{
    if (!handle || !buffer || size == 0) return -1;
    if (!vfs_handle_can_write(handle)) return -1;
    if (!handle->node || !handle->node->ops || !handle->node->ops->write)
        return -1;
    return handle->node->ops->write(handle->node, handle->driver_handle, offset, buffer, size);
}

VFSResult VFS_TruncateHandle(VFS_HANDLE handle, uint64_t length)
{
    if (!handle) return VFS_RES_INVALID;
    if (!vfs_handle_can_write(handle)) return VFS_RES_ACCESS;
    if (!handle->node || !handle->node->ops || !handle->node->ops->truncate)
        return VFS_RES_UNSUPPORTED;
    return handle->node->ops->truncate(handle->node, handle->driver_handle, length);
}

VFSResult VFS_SeekHandle(VFS_HANDLE handle, int64_t offset, VFSSeekWhence whence, uint64_t* out_position)
{
    if (!handle) return VFS_RES_INVALID;

    uint64_t new_pos = handle->offset;
    if (whence == VFS_SEEK_SET)
    {
        if (offset < 0) return VFS_RES_INVALID;
        new_pos = (uint64_t)offset;
    }
    else if (whence == VFS_SEEK_CUR)
    {
        if (offset < 0 && (uint64_t)(-offset) > handle->offset) return VFS_RES_INVALID;
        new_pos = handle->offset + offset;
    }
    else if (whence == VFS_SEEK_END)
    {
        VFSNodeInfo info;
        if (VFS_NodeStat(handle->node, &info) != VFS_RES_OK)
            return VFS_RES_ERROR;
        if (offset < 0 && (uint64_t)(-offset) > info.size) return VFS_RES_INVALID;
        new_pos = info.size + offset;
    }
    else
    {
        return VFS_RES_INVALID;
    }

    handle->offset = new_pos;
    if (out_position) *out_position = handle->offset;
    return VFS_RES_OK;
}

static VFSResult vfs_normalize_path(const char* path, char* out_path, size_t out_size)
{
    if (!path || !out_path || out_size == 0) return VFS_RES_INVALID;

    if (*path == '\0')
    {
        if (out_size < 2) return VFS_RES_NO_SPACE;
        out_path[0] = '/';
        out_path[1] = '\0';
        return VFS_RES_OK;
    }

    if (*path != '/') return VFS_RES_INVALID;

    size_t len = 0;
    size_t depth = 0;
    size_t offsets[VFS_MAX_SEGMENTS];

    out_path[len++] = '/';
    out_path[len] = '\0';

    const char* p = path;
    while (*p == '/') p++;

    while (*p)
    {
        const char* segment_start = p;
        size_t seg_len = 0;
        while (*p && *p != '/')
        {
            ++p;
            ++seg_len;
        }

        if (seg_len == 0)
        {
            while (*p == '/') ++p;
            continue;
        }

        if (seg_len > VFS_NAME_MAX) return VFS_RES_INVALID;

        if (seg_len == 1 && segment_start[0] == '.')
        {
            while (*p == '/') ++p;
            continue;
        }

        if (seg_len == 2 && segment_start[0] == '.' && segment_start[1] == '.')
        {
            if (depth > 0)
            {
                depth--;
                len = offsets[depth];
                out_path[len] = '\0';
            }
            while (*p == '/') ++p;
            continue;
        }

        if (len > 1)
        {
            if (len + 1 >= out_size) return VFS_RES_NO_SPACE;
            out_path[len++] = '/';
        }

        if (len + seg_len >= out_size) return VFS_RES_NO_SPACE;
        for (size_t i = 0; i < seg_len; ++i)
        {
            out_path[len++] = segment_start[i];
        }
        out_path[len] = '\0';

        if (depth < VFS_MAX_SEGMENTS)
        {
            offsets[depth++] = len;
        }

        while (*p == '/') ++p;
    }

    if (len == 1 && out_path[0] == '/' && out_path[1] == '\0')
    {
        return VFS_RES_OK;
    }

    return VFS_RES_OK;
}

static VFSMount* vfs_select_mount(const char* normalized_path)
{
    if (!normalized_path || !s_mounts) return NULL;

    VFSMount* best = NULL;
    size_t best_len = 0;

    for (ListNode* it = List_Foreach_Begin(s_mounts); it; it = List_Foreach_Next(it))
    {
        VFSMount* mount = (VFSMount*)List_Foreach_Data(it);
        if (!mount || !mount->path) continue;
        size_t mount_len = strlen(mount->path);
        if (mount_len > strlen(normalized_path)) continue;
        if (strncmp(normalized_path, mount->path, mount_len) != 0) continue;
        if (mount_len != 1 && normalized_path[mount_len] != '\0' && normalized_path[mount_len] != '/')
            continue;
        if (!best || mount_len > best_len)
        {
            best = mount;
            best_len = mount_len;
        }
    }

    return best;
}

static VFSResult vfs_walk(VFSNode* start, const char* relative_path, VFSNode** out_node, bool follow_last_link)
{
    (void)follow_last_link;

    if (!start || !relative_path || !out_node) return VFS_RES_INVALID;

    VFSNode* current = start;
    const char* p = relative_path;

    while (*p)
    {
        while (*p == '/') ++p;
        if (!*p) break;

        char segment[VFS_NAME_MAX + 1];
        size_t seg_len = 0;
        while (p[seg_len] && p[seg_len] != '/')
        {
            if (seg_len >= VFS_NAME_MAX)
                return VFS_RES_INVALID;
            segment[seg_len] = p[seg_len];
            ++seg_len;
        }
        segment[seg_len] = '\0';
        p += seg_len;

        if (strcmp(segment, ".") == 0)
        {
            continue;
        }
        if (strcmp(segment, "..") == 0)
        {
            if (current->parent)
            {
                current = current->parent;
            }
            continue;
        }

        if (!current->ops || !current->ops->lookup)
        {
            return VFS_RES_UNSUPPORTED;
        }

        VFSNode* next = NULL;
        VFSResult res = current->ops->lookup(current, segment, &next);
        if (res != VFS_RES_OK || !next)
        {
            return VFS_RES_NOT_FOUND;
        }

        current = next;
    }

    *out_node = current;
    return VFS_RES_OK;
}

VFSResult VFS_Create(const char* path, VFSNodeType type)
{
    if (!path || type == VFS_NODE_UNKNOWN) return VFS_RES_INVALID;
    if (!s_vfs_initialized) return VFS_RES_ERROR;

    char normalized[VFS_PATH_MAX];
    VFSResult res = vfs_normalize_path(path, normalized, sizeof(normalized));
    if (res != VFS_RES_OK) return res;

    if (strcmp(normalized, "/") == 0) return VFS_RES_EXISTS;

    char* last_sep = strrchr(normalized, '/');
    if (!last_sep || !last_sep[1]) return VFS_RES_INVALID;

    char name[VFS_NAME_MAX + 1];
    size_t name_len = strlen(last_sep + 1);
    if (name_len > VFS_NAME_MAX) return VFS_RES_INVALID;
    memcpy(name, last_sep + 1, name_len + 1);

    char parent_path[VFS_PATH_MAX];
    size_t parent_len = (size_t)(last_sep - normalized);
    if (parent_len == 0)
    {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    else
    {
        if (parent_len >= sizeof(parent_path)) return VFS_RES_NO_SPACE;
        memcpy(parent_path, normalized, parent_len);
        parent_path[parent_len] = '\0';
    }

    VFSNode* parent = NULL;
    res = VFS_Resolve(parent_path, &parent);
    if (res != VFS_RES_OK) return res;

    if (!parent->ops || !parent->ops->create)
        return VFS_RES_UNSUPPORTED;

    return parent->ops->create(parent, name, type, NULL);
}

VFSResult VFS_Remove(const char* path)
{
    if (!path) return VFS_RES_INVALID;
    if (!s_vfs_initialized) return VFS_RES_ERROR;

    char normalized[VFS_PATH_MAX];
    VFSResult res = vfs_normalize_path(path, normalized, sizeof(normalized));
    if (res != VFS_RES_OK) return res;

    if (strcmp(normalized, "/") == 0) return VFS_RES_BUSY;

    char* last_sep = strrchr(normalized, '/');
    if (!last_sep || !last_sep[1]) return VFS_RES_INVALID;

    char name[VFS_NAME_MAX + 1];
    size_t name_len = strlen(last_sep + 1);
    if (name_len > VFS_NAME_MAX) return VFS_RES_INVALID;
    memcpy(name, last_sep + 1, name_len + 1);

    char parent_path[VFS_PATH_MAX];
    size_t parent_len = (size_t)(last_sep - normalized);
    if (parent_len == 0)
    {
        parent_path[0] = '/';
        parent_path[1] = '\0';
    }
    else
    {
        if (parent_len >= sizeof(parent_path)) return VFS_RES_NO_SPACE;
        memcpy(parent_path, normalized, parent_len);
        parent_path[parent_len] = '\0';
    }

    VFSNode* parent = NULL;
    res = VFS_Resolve(parent_path, &parent);
    if (res != VFS_RES_OK) return res;

    if (!parent->ops || !parent->ops->remove)
        return VFS_RES_UNSUPPORTED;

    return parent->ops->remove(parent, name);
}
