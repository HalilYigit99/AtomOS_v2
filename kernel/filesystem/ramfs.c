#include <filesystem/ramfs.h>
#include <memory/memory.h>
#include <util/string.h>
#include <list.h>
#include <debug/debug.h>

typedef struct RamFSNode {
    List* children;
    uint8_t* data;
    size_t size;
    size_t capacity;
} RamFSNode;

typedef struct RamFS {
    VFSFileSystem base;
    char* label;
} RamFS;

static VFSResult ramfs_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root);
static VFSResult ramfs_unmount(VFSFileSystem* fs, VFSNode* root);

static VFSResult ramfs_open(VFSNode* node, uint32_t mode, void** out_handle);
static VFSResult ramfs_close(VFSNode* node, void* handle);
static int64_t   ramfs_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size);
static int64_t   ramfs_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size);
static VFSResult ramfs_truncate(VFSNode* node, void* handle, uint64_t length);
static VFSResult ramfs_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry);
static VFSResult ramfs_lookup(VFSNode* node, const char* name, VFSNode** out_node);
static VFSResult ramfs_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node);
static VFSResult ramfs_remove(VFSNode* node, const char* name);
static VFSResult ramfs_stat(VFSNode* node, VFSNodeInfo* out_info);

static const VFSNodeOps s_ramfs_node_ops = {
    .open     = ramfs_open,
    .close    = ramfs_close,
    .read     = ramfs_read,
    .write    = ramfs_write,
    .truncate = ramfs_truncate,
    .readdir  = ramfs_readdir,
    .lookup   = ramfs_lookup,
    .create   = ramfs_create,
    .remove   = ramfs_remove,
    .stat     = ramfs_stat,
};

static const VFSFileSystemOps s_ramfs_ops = {
    .mount   = ramfs_mount,
    .unmount = ramfs_unmount,
};

static RamFSNode* ramfs_payload(VFSNode* node)
{
    return node ? (RamFSNode*)node->internal_data : NULL;
}

static void ramfs_free_node(VFSNode* node)
{
    if (!node) return;
    RamFSNode* payload = ramfs_payload(node);

    if (node->type == VFS_NODE_DIRECTORY && payload && payload->children)
    {
        while (!List_IsEmpty(payload->children))
        {
            VFSNode* child = (VFSNode*)List_GetAt(payload->children, 0);
            List_RemoveAt(payload->children, 0);
            ramfs_free_node(child);
        }
        List_Destroy(payload->children, false);
    }

    if (payload)
    {
        if (payload->data) free(payload->data);
        free(payload);
    }

    if (node->name) free(node->name);
    free(node);
}

static VFSNode* ramfs_new_node(const char* name, VFSNodeType type)
{
    VFSNode* node = (VFSNode*)malloc(sizeof(VFSNode));
    if (!node) return NULL;

    RamFSNode* payload = (RamFSNode*)malloc(sizeof(RamFSNode));
    if (!payload)
    {
        free(node);
        return NULL;
    }

    char* node_name = NULL;
    if (name)
    {
        node_name = strdup(name);
        if (!node_name)
        {
            free(payload);
            free(node);
            return NULL;
        }
    }

    payload->children = (type == VFS_NODE_DIRECTORY) ? List_Create() : NULL;
    payload->data = NULL;
    payload->size = 0;
    payload->capacity = 0;

    if (type == VFS_NODE_DIRECTORY && !payload->children)
    {
        if (node_name) free(node_name);
        free(payload);
        free(node);
        return NULL;
    }

    node->name = node_name;
    node->type = type;
    node->flags = 0;
    node->parent = NULL;
    node->mount = NULL;
    node->ops = &s_ramfs_node_ops;
    node->internal_data = payload;

    return node;
}

static bool ramfs_grow_buffer(RamFSNode* node, size_t required)
{
    if (!node) return false;
    if (required <= node->capacity) return true;

    size_t new_capacity = node->capacity ? node->capacity : 64;
    while (new_capacity < required)
    {
        if (new_capacity > SIZE_MAX / 2)
        {
            new_capacity = required;
            break;
        }
        new_capacity *= 2;
    }

    uint8_t* new_data = (uint8_t*)realloc(node->data, new_capacity);
    if (!new_data)
    {
        return false;
    }

    if (new_capacity > node->capacity)
    {
        size_t diff = new_capacity - node->capacity;
        memset(new_data + node->capacity, 0, diff);
    }

    node->data = new_data;
    node->capacity = new_capacity;
    return true;
}

static VFSResult ramfs_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root)
{
    (void)params;
    if (!fs || !out_root) return VFS_RES_INVALID;

    VFSNode* root = ramfs_new_node("", VFS_NODE_DIRECTORY);
    if (!root) return VFS_RES_NO_MEMORY;

    *out_root = root;
    LOG("ramfs: mounted instance '%s'", fs->name);
    return VFS_RES_OK;
}

static VFSResult ramfs_unmount(VFSFileSystem* fs, VFSNode* root)
{
    (void)fs;
    ramfs_free_node(root);
    return VFS_RES_OK;
}

static VFSResult ramfs_open(VFSNode* node, uint32_t mode, void** out_handle)
{
    (void)out_handle;
    if (!node) return VFS_RES_INVALID;

    if (node->type == VFS_NODE_DIRECTORY && (mode & VFS_OPEN_WRITE))
    {
        return VFS_RES_ACCESS;
    }

    return VFS_RES_OK;
}

static VFSResult ramfs_close(VFSNode* node, void* handle)
{
    (void)node;
    (void)handle;
    return VFS_RES_OK;
}

static int64_t ramfs_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size)
{
    (void)handle;
    if (!node || !buffer) return -1;
    if (node->type != VFS_NODE_REGULAR) return -1;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload) return -1;

    if (offset >= payload->size) return 0;

    size_t remaining = payload->size - (size_t)offset;
    size_t to_copy = (size < remaining) ? size : remaining;
    memcpy(buffer, payload->data + offset, to_copy);
    return (int64_t)to_copy;
}

static int64_t ramfs_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size)
{
    (void)handle;
    if (!node || !buffer) return -1;
    if (node->type != VFS_NODE_REGULAR) return -1;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload) return -1;

    uint64_t end_pos = offset + size;
    if (end_pos > SIZE_MAX) return -1;

    if (!ramfs_grow_buffer(payload, (size_t)end_pos))
    {
        return -1;
    }

    memcpy(payload->data + offset, buffer, size);

    if (end_pos > payload->size)
    {
        payload->size = (size_t)end_pos;
    }

    return (int64_t)size;
}

static VFSResult ramfs_truncate(VFSNode* node, void* handle, uint64_t length)
{
    (void)handle;
    if (!node) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_REGULAR) return VFS_RES_UNSUPPORTED;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload) return VFS_RES_ERROR;

    if (length > SIZE_MAX) return VFS_RES_INVALID;

    if (!ramfs_grow_buffer(payload, (size_t)length))
        return VFS_RES_NO_MEMORY;

    if (length < payload->size)
    {
        memset(payload->data + length, 0, payload->size - (size_t)length);
    }

    payload->size = (size_t)length;
    return VFS_RES_OK;
}

static VFSResult ramfs_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry)
{
    (void)handle;
    if (!node || !out_entry) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload || !payload->children) return VFS_RES_ERROR;

    if (index >= List_Size(payload->children)) return VFS_RES_NOT_FOUND;

    VFSNode* child = (VFSNode*)List_GetAt(payload->children, index);
    if (!child) return VFS_RES_NOT_FOUND;

    size_t name_len = child->name ? strlen(child->name) : 0;
    if (name_len > VFS_NAME_MAX) name_len = VFS_NAME_MAX;
    if (name_len > 0)
    {
        memcpy(out_entry->name, child->name, name_len);
    }
    out_entry->name[name_len] = '\0';
    out_entry->type = child->type;
    return VFS_RES_OK;
}

static VFSResult ramfs_lookup(VFSNode* node, const char* name, VFSNode** out_node)
{
    if (!node || !name || !out_node) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload || !payload->children) return VFS_RES_ERROR;

    for (ListNode* it = List_Foreach_Begin(payload->children); it; it = List_Foreach_Next(it))
    {
        VFSNode* child = (VFSNode*)List_Foreach_Data(it);
        if (child && child->name && strcmp(child->name, name) == 0)
        {
            *out_node = child;
            return VFS_RES_OK;
        }
    }

    return VFS_RES_NOT_FOUND;
}

static VFSResult ramfs_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node)
{
    if (!node || !name) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload || !payload->children) return VFS_RES_ERROR;

    VFSNode* existing = NULL;
    if (ramfs_lookup(node, name, &existing) == VFS_RES_OK)
    {
        return VFS_RES_EXISTS;
    }

    VFSNode* child = ramfs_new_node(name, type);
    if (!child) return VFS_RES_NO_MEMORY;

    child->parent = node;
    child->mount = node->mount;

    List_Add(payload->children, child);
    if (out_node) *out_node = child;
    return VFS_RES_OK;
}

static VFSResult ramfs_remove(VFSNode* node, const char* name)
{
    if (!node || !name) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    RamFSNode* payload = ramfs_payload(node);
    if (!payload || !payload->children) return VFS_RES_ERROR;

    size_t index = 0;
    for (ListNode* it = List_Foreach_Begin(payload->children); it; it = List_Foreach_Next(it), ++index)
    {
        VFSNode* child = (VFSNode*)List_Foreach_Data(it);
        if (child && child->name && strcmp(child->name, name) == 0)
        {
            List_RemoveAt(payload->children, index);
            ramfs_free_node(child);
            return VFS_RES_OK;
        }
    }

    return VFS_RES_NOT_FOUND;
}

static VFSResult ramfs_stat(VFSNode* node, VFSNodeInfo* out_info)
{
    if (!node || !out_info) return VFS_RES_INVALID;

    RamFSNode* payload = ramfs_payload(node);
    out_info->type = node->type;
    out_info->flags = node->flags;
    out_info->inode = (uintptr_t)node;
    out_info->atime = 0;
    out_info->mtime = 0;
    out_info->ctime = 0;
    out_info->size = payload ? payload->size : 0;
    return VFS_RES_OK;
}

VFSFileSystem* RamFS_Create(const char* label)
{
    RamFS* fs = (RamFS*)malloc(sizeof(RamFS));
    if (!fs) return NULL;

    const char* base_name = label ? label : "ramfs";
    fs->label = strdup(base_name);
    if (!fs->label)
    {
        free(fs);
        return NULL;
    }

    fs->base.name = fs->label;
    fs->base.flags = 0;
    fs->base.ops = &s_ramfs_ops;
    fs->base.driver_context = fs;

    return &fs->base;
}

void RamFS_Destroy(VFSFileSystem* vfs_fs)
{
    if (!vfs_fs) return;
    RamFS* fs = (RamFS*)vfs_fs->driver_context;
    if (!fs) return;

    if (fs->label) free(fs->label);
    free(fs);
}

