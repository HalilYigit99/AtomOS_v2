#include <filesystem/iso9660.h>
#include <memory/memory.h>
#include <util/string.h>
#include <debug/debug.h>
#include <list.h>
#include <storage/Volume.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define ISO9660_VOLUME_DESCRIPTOR_PRIMARY   1
#define ISO9660_VOLUME_DESCRIPTOR_TERMINATOR 255
#define ISO9660_STANDARD_ID "CD001"
#define ISO9660_FILE_FLAG_HIDDEN      0x01
#define ISO9660_FILE_FLAG_DIRECTORY   0x02

#pragma pack(push, 1)
typedef struct ISO9660PrimaryVolumeDescriptor {
    uint8_t type;
    char    identifier[5];
    uint8_t version;
    uint8_t unused1;
    char    system_identifier[32];
    char    volume_identifier[32];
    uint8_t unused2[8];
    uint32_t volume_space_size_lsb;
    uint32_t volume_space_size_msb;
    uint8_t unused3[32];
    uint16_t volume_set_size_lsb;
    uint16_t volume_set_size_msb;
    uint16_t volume_sequence_number_lsb;
    uint16_t volume_sequence_number_msb;
    uint16_t logical_block_size_lsb;
    uint16_t logical_block_size_msb;
    uint32_t path_table_size_lsb;
    uint32_t path_table_size_msb;
    uint32_t type_l_path_table_lba;
    uint32_t opt_type_l_path_table_lba;
    uint32_t type_m_path_table_lba;
    uint32_t opt_type_m_path_table_lba;
    uint8_t  root_directory_record[34];
} ISO9660PrimaryVolumeDescriptor;

typedef struct ISO9660DirectoryRecordHeader {
    uint8_t  length;
    uint8_t  extended_attribute_length;
    uint32_t extent_lba_lsb;
    uint32_t extent_lba_msb;
    uint32_t data_length_lsb;
    uint32_t data_length_msb;
    uint8_t  recording_time[7];
    uint8_t  file_flags;
    uint8_t  file_unit_size;
    uint8_t  interleave_gap_size;
    uint16_t volume_sequence_number_lsb;
    uint16_t volume_sequence_number_msb;
    uint8_t  file_identifier_length;
    /* followed by file identifier bytes and optional padding */
} ISO9660DirectoryRecordHeader;
#pragma pack(pop)

typedef struct ISO9660Volume ISO9660Volume;

typedef struct ISO9660NodeInfo {
    ISO9660Volume* volume;
    uint32_t extent_lba;
    uint32_t data_length;
    uint8_t  flags;
    bool     is_root;
} ISO9660NodeInfo;

typedef struct ISO9660Volume {
    BlockDevice* device;
    uint32_t logical_block_size;
    List* nodes; // track allocated VFS nodes for cleanup
} ISO9660Volume;

typedef struct ISO9660Handle {
    ISO9660NodeInfo* node;
} ISO9660Handle;

typedef struct ISO9660ParsedDirRecord {
    uint32_t extent_lba;
    uint32_t data_length;
    uint8_t flags;
    char name[VFS_NAME_MAX + 1];
} ISO9660ParsedDirRecord;

static VFSFileSystem s_iso_fs = {
    .name = "iso9660",
    .flags = 0,
    .ops = NULL,
    .driver_context = NULL,
};

static VFSResult iso9660_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root);
static VFSResult iso9660_unmount(VFSFileSystem* fs, VFSNode* root);
static VFSResult iso9660_node_open(VFSNode* node, uint32_t mode, void** out_handle);
static VFSResult iso9660_node_close(VFSNode* node, void* handle);
static int64_t   iso9660_node_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size);
static int64_t   iso9660_node_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size);
static VFSResult iso9660_node_truncate(VFSNode* node, void* handle, uint64_t length);
static VFSResult iso9660_node_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry);
static VFSResult iso9660_node_lookup(VFSNode* node, const char* name, VFSNode** out_node);
static VFSResult iso9660_node_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node);
static VFSResult iso9660_node_remove(VFSNode* node, const char* name);
static VFSResult iso9660_node_stat(VFSNode* node, VFSNodeInfo* out_info);
static bool      iso9660_probe(VFSFileSystem* fs, const VFSMountParams* params);
static bool      iso9660_read_sector(const VFSMountParams* params, uint32_t block_size, uint32_t lba, void* buffer);

static const VFSNodeOps s_iso_node_ops = {
    .open     = iso9660_node_open,
    .close    = iso9660_node_close,
    .read     = iso9660_node_read,
    .write    = iso9660_node_write,
    .truncate = iso9660_node_truncate,
    .readdir  = iso9660_node_readdir,
    .lookup   = iso9660_node_lookup,
    .create   = iso9660_node_create,
    .remove   = iso9660_node_remove,
    .stat     = iso9660_node_stat,
};

static const VFSFileSystemOps s_iso_ops = {
    .probe   = iso9660_probe,
    .mount   = iso9660_mount,
    .unmount = iso9660_unmount,
};

static inline uint16_t iso9660_read_lsb16(const void* ptr)
{
    const uint8_t* b = (const uint8_t*)ptr;
    return (uint16_t)(b[0] | ((uint16_t)b[1] << 8));
}

static inline uint32_t iso9660_read_lsb32(const void* ptr)
{
    const uint8_t* b = (const uint8_t*)ptr;
    return (uint32_t)(b[0]
                      | ((uint32_t)b[1] << 8)
                      | ((uint32_t)b[2] << 16)
                      | ((uint32_t)b[3] << 24));
}

static ISO9660NodeInfo* iso9660_node_info(VFSNode* node)
{
    return node ? (ISO9660NodeInfo*)node->internal_data : NULL;
}

static void iso9660_free_node(VFSNode* node)
{
    if (!node) return;
    ISO9660NodeInfo* info = iso9660_node_info(node);
    if (info) free(info);
    if (node->name) free(node->name);
    free(node);
}

static void iso9660_destroy_volume(ISO9660Volume* volume)
{
    if (!volume) return;
    if (volume->nodes)
    {
        for (ListNode* it = List_Foreach_Begin(volume->nodes); it; it = List_Foreach_Next(it))
        {
            VFSNode* node = (VFSNode*)List_Foreach_Data(it);
            iso9660_free_node(node);
        }
        List_Destroy(volume->nodes, false);
        volume->nodes = NULL;
    }
    free(volume);
}

static VFSNode* iso9660_alloc_node(ISO9660Volume* volume,
                                   VFSNode* parent,
                                   const char* name,
                                   VFSNodeType type,
                                   ISO9660NodeInfo** out_info)
{
    if (!volume) return NULL;

    VFSNode* node = (VFSNode*)malloc(sizeof(VFSNode));
    if (!node) return NULL;

    ISO9660NodeInfo* info = (ISO9660NodeInfo*)malloc(sizeof(ISO9660NodeInfo));
    if (!info)
    {
        free(node);
        return NULL;
    }

    char* node_name = NULL;
    if (name && name[0])
    {
        node_name = strdup(name);
        if (!node_name)
        {
            free(info);
            free(node);
            return NULL;
        }
    }

    info->volume = volume;
    info->extent_lba = 0;
    info->data_length = 0;
    info->flags = 0;
    info->is_root = false;

    node->name = node_name;
    node->type = type;
    node->flags = VFS_NODE_FLAG_READONLY;
    node->parent = parent;
    node->mount = parent ? parent->mount : NULL;
    node->ops = &s_iso_node_ops;
    node->internal_data = info;

    if (!volume->nodes)
    {
        volume->nodes = List_Create();
        if (!volume->nodes)
        {
            if (node_name) free(node_name);
            free(info);
            free(node);
            return NULL;
        }
    }

    List_Add(volume->nodes, node);

    if (out_info) *out_info = info;
    return node;
}

static size_t iso9660_normalize_name(const uint8_t* raw, uint8_t raw_len, char* out, size_t out_size)
{
    if (!out || out_size == 0) return 0;
    size_t pos = 0;
    for (size_t i = 0; i < raw_len; ++i)
    {
        char c = (char)raw[i];
        if (c == ';' || c == '\0')
            break;
        if (pos + 1 >= out_size)
            break;
        if (c >= 'A' && c <= 'Z')
            c = to_lower(c);
        out[pos++] = c;
    }

    // Trim trailing spaces (ISO 9660 may pad)
    while (pos > 0 && out[pos - 1] == ' ')
        --pos;

    if (pos >= out_size)
        pos = out_size - 1;

    out[pos] = '\0';
    return pos;
}

typedef bool (*iso9660_dir_iter_cb)(const ISO9660ParsedDirRecord* record, void* context);

static bool iso9660_iterate_directory(ISO9660NodeInfo* dir,
                                      iso9660_dir_iter_cb callback,
                                      void* context)
{
    if (!dir || !callback) return false;
    ISO9660Volume* volume = dir->volume;
    if (!volume || !volume->device) return false;

    uint32_t block_size = volume->logical_block_size;
    if (block_size == 0)
        block_size = 2048;

    if (dir->data_length == 0)
        return true; // empty directory

    uint8_t* block = (uint8_t*)malloc(block_size);
    if (!block)
        return false;

    uint32_t total_blocks = (dir->data_length + block_size - 1) / block_size;

    for (uint32_t block_index = 0; block_index < total_blocks; ++block_index)
    {
        if (!BlockDevice_Read(volume->device, dir->extent_lba + block_index, 1, block))
        {
            free(block);
            return false;
        }

        size_t pos = 0;
        while (pos < block_size)
        {
            size_t absolute_offset = (size_t)block_index * block_size + pos;
            if (absolute_offset >= dir->data_length)
            {
                break;
            }

            ISO9660DirectoryRecordHeader* header = (ISO9660DirectoryRecordHeader*)(block + pos);
            uint8_t length = header->length;
            if (length == 0)
            {
                // Remaining bytes in this block are padding
                break;
            }

            if (absolute_offset + length > dir->data_length)
            {
                // Malformed entry that overflows directory size
                free(block);
                return false;
            }

            const uint8_t* identifier = (const uint8_t*)(block + pos + sizeof(ISO9660DirectoryRecordHeader));
            uint8_t identifier_len = header->file_identifier_length;

            bool is_special = (identifier_len == 1) && (identifier[0] == 0 || identifier[0] == 1);
            if (!is_special)
            {
                ISO9660ParsedDirRecord parsed;
                parsed.extent_lba = iso9660_read_lsb32(&header->extent_lba_lsb);
                parsed.data_length = iso9660_read_lsb32(&header->data_length_lsb);
                parsed.flags = header->file_flags;
                parsed.name[0] = '\0';

                iso9660_normalize_name(identifier, identifier_len, parsed.name, sizeof(parsed.name));

                if (parsed.name[0] != '\0')
                {
                    bool keep = callback(&parsed, context);
                    if (!keep)
                    {
                        free(block);
                        return true;
                    }
                }
            }

            pos += length;
        }
    }

    free(block);
    return true;
}

void ISO9660_Register(void)
{
    if (!s_iso_fs.ops)
    {
        s_iso_fs.ops = &s_iso_ops;
        if (VFS_RegisterFileSystem(&s_iso_fs) != VFS_RES_OK)
        {
            WARN("ISO9660_Register: VFS registration failed");
        }
    }
}

VFSResult ISO9660_Mount(BlockDevice* device, const char* mount_path)
{
    if (!device || !mount_path) return VFS_RES_INVALID;
    ISO9660_Register();
    VFSFileSystem* fs = VFS_GetFileSystem("iso9660");
    if (!fs) return VFS_RES_ERROR;

    VFSMountParams params = {
        .source = device->name,
        .block_device = device,
        .volume = NULL,
        .context = NULL,
        .flags = 0,
    };

    VFSMount* mount = VFS_Mount(mount_path, fs, &params);
    return mount ? VFS_RES_OK : VFS_RES_ERROR;
}

static VFSResult iso9660_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root)
{
    (void)fs;
    if (!params || (!params->block_device && !params->volume) || !out_root)
        return VFS_RES_INVALID;

    Volume* backing_volume = params->volume;
    BlockDevice* device = params->block_device ? params->block_device : (backing_volume ? backing_volume->device : NULL);
    if (!device && !backing_volume)
        return VFS_RES_INVALID;

    uint32_t block_size = 0;
    if (backing_volume)
        block_size = Volume_BlockSize(backing_volume);
    else if (device && device->logical_block_size)
        block_size = device->logical_block_size;
    if (block_size == 0)
        block_size = 2048;

    uint8_t* sector = (uint8_t*)malloc(block_size);
    if (!sector)
        return VFS_RES_NO_MEMORY;

    bool found_primary = false;
    ISO9660PrimaryVolumeDescriptor primary;

    for (uint32_t lba = 16; lba < 16 + 64; ++lba)
    {
        if (!iso9660_read_sector(params, block_size, lba, sector))
        {
            free(sector);
            return VFS_RES_ERROR;
        }

        uint8_t type = sector[0];
        if (memcmp(sector + 1, ISO9660_STANDARD_ID, 5) != 0)
        {
            if (type == ISO9660_VOLUME_DESCRIPTOR_TERMINATOR)
                break;
            else
                continue;
        }

        if (type == ISO9660_VOLUME_DESCRIPTOR_PRIMARY)
        {
            memcpy(&primary, sector, sizeof(ISO9660PrimaryVolumeDescriptor));
            found_primary = true;
            break;
        }
        else if (type == ISO9660_VOLUME_DESCRIPTOR_TERMINATOR)
        {
            break;
        }
    }

    free(sector);

    if (!found_primary)
    {
        return VFS_RES_UNSUPPORTED;
    }

    uint32_t descriptor_block_size = iso9660_read_lsb16(&primary.logical_block_size_lsb);
    if (descriptor_block_size == 0)
    {
        descriptor_block_size = block_size;
    }

    if (descriptor_block_size != block_size)
    {
        LOG("ISO9660: logical block size mismatch (descriptor=%u device=%u) using device size",
            descriptor_block_size, block_size);
    }

    ISO9660Volume* volume = (ISO9660Volume*)malloc(sizeof(ISO9660Volume));
    if (!volume)
        return VFS_RES_NO_MEMORY;
    memset(volume, 0, sizeof(ISO9660Volume));

    volume->device = device;
    volume->logical_block_size = block_size;
    volume->nodes = List_Create();
    if (!volume->nodes)
    {
        free(volume);
        return VFS_RES_NO_MEMORY;
    }

    VFSNode* root = iso9660_alloc_node(volume, NULL, "", VFS_NODE_DIRECTORY, NULL);
    if (!root)
    {
        iso9660_destroy_volume(volume);
        return VFS_RES_NO_MEMORY;
    }

    ISO9660NodeInfo* info = iso9660_node_info(root);
    info->volume = volume;
    info->is_root = true;

    ISO9660DirectoryRecordHeader* root_header = (ISO9660DirectoryRecordHeader*)primary.root_directory_record;
    info->extent_lba = iso9660_read_lsb32(&root_header->extent_lba_lsb);
    info->data_length = iso9660_read_lsb32(&root_header->data_length_lsb);
    info->flags = ISO9660_FILE_FLAG_DIRECTORY;

    root->parent = NULL;
    root->mount = NULL;

    *out_root = root;

    LOG("ISO9660: mounted volume '%s' (extent=%u size=%u)",
        params->source ? params->source : "cdrom",
        info->extent_lba,
        info->data_length);

    return VFS_RES_OK;
}

static VFSResult iso9660_unmount(VFSFileSystem* fs, VFSNode* root)
{
    (void)fs;
    if (!root) return VFS_RES_INVALID;
    ISO9660NodeInfo* info = iso9660_node_info(root);
    ISO9660Volume* volume = info ? info->volume : NULL;
    iso9660_destroy_volume(volume);
    return VFS_RES_OK;
}

static bool iso9660_probe(VFSFileSystem* fs, const VFSMountParams* params)
{
    (void)fs;
    if (!params)
        return false;

    uint32_t block_size = 0;
    if (params->volume)
        block_size = Volume_BlockSize(params->volume);
    else if (params->block_device && params->block_device->logical_block_size)
        block_size = params->block_device->logical_block_size;
    if (block_size == 0)
        block_size = 2048;

    uint8_t* sector = (uint8_t*)malloc(block_size);
    if (!sector)
        return false;

    bool ok = iso9660_read_sector(params, block_size, 16, sector);
    if (!ok)
    {
        free(sector);
        return false;
    }

    bool match = false;
    if (memcmp(sector + 1, ISO9660_STANDARD_ID, 5) == 0)
    {
        uint8_t type = sector[0];
        if (type == ISO9660_VOLUME_DESCRIPTOR_PRIMARY || type == 0)
            match = true;
    }

    free(sector);
    return match;
}

static bool iso9660_read_sector(const VFSMountParams* params, uint32_t block_size, uint32_t lba, void* buffer)
{
    (void)block_size;
    if (!params || !buffer)
        return false;
    if (params->volume)
        return Volume_ReadSectors(params->volume, lba, 1, buffer);
    if (params->block_device)
        return BlockDevice_Read(params->block_device, lba, 1, buffer);
    return false;
}

static VFSResult iso9660_node_open(VFSNode* node, uint32_t mode, void** out_handle)
{
    if (!node) return VFS_RES_INVALID;
    if ((mode & VFS_OPEN_WRITE) || (mode & VFS_OPEN_APPEND))
        return VFS_RES_ACCESS;

    ISO9660NodeInfo* info = iso9660_node_info(node);
    if (!info) return VFS_RES_ERROR;

    ISO9660Handle* handle = (ISO9660Handle*)malloc(sizeof(ISO9660Handle));
    if (!handle) return VFS_RES_NO_MEMORY;
    handle->node = info;
    if (out_handle) *out_handle = handle;
    return VFS_RES_OK;
}

static VFSResult iso9660_node_close(VFSNode* node, void* handle)
{
    (void)node;
    if (handle) free(handle);
    return VFS_RES_OK;
}

static int64_t iso9660_node_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size)
{
    (void)handle;
    if (!node || !buffer || size == 0) return -1;
    if (node->type == VFS_NODE_DIRECTORY) return -1;

    ISO9660NodeInfo* info = iso9660_node_info(node);
    if (!info || !info->volume || !info->volume->device)
        return -1;

    if (offset >= info->data_length)
        return 0;

    size_t remaining = size;
    if (offset + remaining > info->data_length)
    {
        if (info->data_length <= offset)
            remaining = 0;
        else
            remaining = (size_t)(info->data_length - offset);
    }

    if (remaining == 0)
        return 0;

    ISO9660Volume* volume = info->volume;
    uint32_t block_size = volume->logical_block_size;
    if (block_size == 0)
        block_size = 2048;

    uint8_t* temp = (uint8_t*)malloc(block_size);
    if (!temp)
        return -1;

    uint8_t* out = (uint8_t*)buffer;
    size_t total_read = 0;

    while (total_read < remaining)
    {
        uint64_t abs_offset = offset + total_read;
        uint32_t lba = info->extent_lba + (uint32_t)(abs_offset / block_size);
        size_t intra = (size_t)(abs_offset % block_size);
        size_t chunk = MIN(remaining - total_read, block_size - intra);

        if (intra == 0 && chunk == block_size && (remaining - total_read) >= block_size)
        {
            size_t max_blocks = (remaining - total_read) / block_size;
            if (max_blocks > UINT32_MAX)
                max_blocks = UINT32_MAX;
            if (!BlockDevice_Read(volume->device, lba, (uint32_t)max_blocks, out + total_read))
            {
                WARN("ISO9660: bulk read failed at LBA=%u count=%zu", lba, max_blocks);
                break;
            }
            size_t bytes = max_blocks * block_size;
            total_read += bytes;
            continue;
        }

        if (!BlockDevice_Read(volume->device, lba, 1, temp))
        {
            WARN("ISO9660: read failed at LBA=%u", lba);
            break;
        }

        memcpy(out + total_read, temp + intra, chunk);
        total_read += chunk;
    }

    free(temp);
    return (int64_t)total_read;
}

static int64_t iso9660_node_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size)
{
    (void)node; (void)handle; (void)offset; (void)buffer; (void)size;
    return -1;
}

static VFSResult iso9660_node_truncate(VFSNode* node, void* handle, uint64_t length)
{
    (void)node; (void)handle; (void)length;
    return VFS_RES_UNSUPPORTED;
}

static bool iso9660_readdir_cb(const ISO9660ParsedDirRecord* record, void* context)
{
    struct {
        size_t target;
        size_t current;
        ISO9660ParsedDirRecord found;
        bool matched;
    }* ctx = context;

    if (!ctx->matched && ctx->current == ctx->target)
    {
        ctx->found = *record;
        ctx->matched = true;
        return false;
    }

    ctx->current++;
    return true;
}

static VFSResult iso9660_node_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry)
{
    (void)handle;
    if (!node || !out_entry) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    ISO9660NodeInfo* info = iso9660_node_info(node);
    if (!info) return VFS_RES_ERROR;

    struct {
        size_t target;
        size_t current;
        ISO9660ParsedDirRecord found;
        bool matched;
    } ctx = { .target = index, .current = 0, .matched = false };

    if (!iso9660_iterate_directory(info, iso9660_readdir_cb, &ctx))
        return VFS_RES_ERROR;

    if (!ctx.matched)
        return VFS_RES_NOT_FOUND;

    memset(out_entry->name, 0, sizeof(out_entry->name));
    size_t len = strlen(ctx.found.name);
    if (len > VFS_NAME_MAX) len = VFS_NAME_MAX;
    memcpy(out_entry->name, ctx.found.name, len);
    out_entry->name[len] = '\0';
    out_entry->type = (ctx.found.flags & ISO9660_FILE_FLAG_DIRECTORY) ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR;
    return VFS_RES_OK;
}

static bool iso9660_lookup_cb(const ISO9660ParsedDirRecord* record, void* context)
{
    struct {
        const char* name;
        ISO9660ParsedDirRecord found;
        bool matched;
    }* ctx = context;

    if (!ctx->matched && strcasecmp(record->name, ctx->name) == 0)
    {
        ctx->found = *record;
        ctx->matched = true;
        return false;
    }
    return true;
}

static VFSResult iso9660_node_lookup(VFSNode* node, const char* name, VFSNode** out_node)
{
    if (!node || !name || !out_node) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    if (strcmp(name, ".") == 0)
    {
        *out_node = node;
        return VFS_RES_OK;
    }
    if (strcmp(name, "..") == 0)
    {
        *out_node = node->parent ? node->parent : node;
        return VFS_RES_OK;
    }

    ISO9660NodeInfo* info = iso9660_node_info(node);
    if (!info) return VFS_RES_ERROR;

    struct {
        const char* name;
        ISO9660ParsedDirRecord found;
        bool matched;
    } ctx = { .name = name, .matched = false };

    if (!iso9660_iterate_directory(info, iso9660_lookup_cb, &ctx))
        return VFS_RES_ERROR;

    if (!ctx.matched)
        return VFS_RES_NOT_FOUND;

    ISO9660NodeInfo* child_info = NULL;
    VFSNode* child = iso9660_alloc_node(info->volume,
                                        node,
                                        ctx.found.name,
                                        (ctx.found.flags & ISO9660_FILE_FLAG_DIRECTORY) ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR,
                                        &child_info);
    if (!child)
        return VFS_RES_NO_MEMORY;

    child_info->extent_lba = ctx.found.extent_lba;
    child_info->data_length = ctx.found.data_length;
    child_info->flags = ctx.found.flags;
    child_info->is_root = false;

    *out_node = child;
    return VFS_RES_OK;
}

static VFSResult iso9660_node_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node)
{
    (void)node; (void)name; (void)type; (void)out_node;
    return VFS_RES_UNSUPPORTED;
}

static VFSResult iso9660_node_remove(VFSNode* node, const char* name)
{
    (void)node; (void)name;
    return VFS_RES_UNSUPPORTED;
}

static VFSResult iso9660_node_stat(VFSNode* node, VFSNodeInfo* out_info)
{
    if (!node || !out_info) return VFS_RES_INVALID;
    ISO9660NodeInfo* info = iso9660_node_info(node);
    if (!info) return VFS_RES_ERROR;

    out_info->type = node->type;
    out_info->flags = node->flags;
    out_info->inode = info->extent_lba;
    out_info->size = info->data_length;
    out_info->atime = 0;
    out_info->mtime = 0;
    out_info->ctime = 0;
    return VFS_RES_OK;
}
