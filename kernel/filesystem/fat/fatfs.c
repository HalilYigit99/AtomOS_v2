#include <filesystem/fat/fatfs.h>
#include "fat_internal.h"
#include <list.h>
#include <memory/memory.h>
#include <util/string.h>
#include <debug/debug.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

static VFSFileSystem s_fat_fs = {
    .name = "fat",
    .flags = 0,
    .ops = NULL,
    .driver_context = NULL,
};

static VFSResult fat_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root);
static VFSResult fat_unmount(VFSFileSystem* fs, VFSNode* root);
static VFSResult fat_node_open(VFSNode* node, uint32_t mode, void** out_handle);
static VFSResult fat_node_close(VFSNode* node, void* handle);
static int64_t   fat_node_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size);
static int64_t   fat_node_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size);
static VFSResult fat_node_truncate(VFSNode* node, void* handle, uint64_t length);
static VFSResult fat_node_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry);
static VFSResult fat_node_lookup(VFSNode* node, const char* name, VFSNode** out_node);
static VFSResult fat_node_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node);
static VFSResult fat_node_remove(VFSNode* node, const char* name);
static VFSResult fat_node_stat(VFSNode* node, VFSNodeInfo* out_info);
static bool     fat_probe(VFSFileSystem* fs, const VFSMountParams* params);
static bool     fat_read_boot_sector(const VFSMountParams* params, FAT_BootSector* out_bpb, uint32_t* out_block_size);

static const VFSNodeOps s_fat_node_ops = {
    .open     = fat_node_open,
    .close    = fat_node_close,
    .read     = fat_node_read,
    .write    = fat_node_write,
    .truncate = fat_node_truncate,
    .readdir  = fat_node_readdir,
    .lookup   = fat_node_lookup,
    .create   = fat_node_create,
    .remove   = fat_node_remove,
    .stat     = fat_node_stat,
};

static const VFSFileSystemOps s_fat_ops = {
    .probe   = fat_probe,
    .mount   = fat_mount,
    .unmount = fat_unmount,
};

typedef struct FATHandle {
    FATNodeInfo* node;
} FATHandle;

static FATNodeInfo* fat_node_info(VFSNode* node)
{
    return (FATNodeInfo*)node->internal_data;
}

static void fatfs_free_node(VFSNode* node)
{
    if (!node) return;
    FATNodeInfo* info = fat_node_info(node);
    if (info) free(info);
    if (node->name) free(node->name);
    free(node);
}

static void fatfs_destroy_volume(FATVolume* volume)
{
    if (!volume) return;
    if (volume->nodes)
    {
        for (ListNode* it = List_Foreach_Begin(volume->nodes); it; it = List_Foreach_Next(it))
        {
            VFSNode* node = (VFSNode*)List_Foreach_Data(it);
            fatfs_free_node(node);
        }
        List_Destroy(volume->nodes, false);
        volume->nodes = NULL;
    }
    free(volume);
}

static VFSNode* fatfs_alloc_node(FATVolume* volume, VFSNode* parent, const char* name, VFSNodeType type, FATNodeInfo** out_info)
{
    if (!volume) return NULL;
    VFSNode* node = (VFSNode*)malloc(sizeof(VFSNode));
    if (!node) return NULL;

    FATNodeInfo* info = (FATNodeInfo*)malloc(sizeof(FATNodeInfo));
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
    info->first_cluster = 0;
    info->size = 0;
    info->attr = 0;
    info->is_root = false;

    node->name = node_name;
    node->type = type;
    node->flags = VFS_NODE_FLAG_READONLY;
    node->parent = parent;
    node->mount = parent ? parent->mount : NULL;
    node->ops = &s_fat_node_ops;
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

static bool fatfs_name_to_83(const char* name, char out[11])
{
    if (!name || !out) return false;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
    {
        memset(out, ' ', 11);
        size_t len = strlen(name);
        for (size_t i = 0; i < len && i < 2; ++i)
        {
            out[i] = (char)to_upper(name[i]);
        }
        return true;
    }

    const char* dot = strchr(name, '.');
    size_t base_len = dot ? (size_t)(dot - name) : strlen(name);
    size_t ext_len = dot ? strlen(dot + 1) : 0;

    if (base_len == 0 || base_len > 8 || ext_len > 3)
        return false;

    memset(out, ' ', 11);
    for (size_t i = 0; i < base_len; ++i)
    {
        char c = to_upper(name[i]);
        if (c == ' ')
            c = '_';
        out[i] = c;
    }
    for (size_t i = 0; i < ext_len; ++i)
    {
        char c = to_upper(dot[1 + i]);
        if (c == ' ')
            c = '_';
        out[8 + i] = c;
    }
    return true;
}

static void fatfs_83_to_name(const uint8_t in[11], char* out, size_t out_size)
{
    if (!out || out_size == 0) return;
    size_t pos = 0;
    size_t i = 0;
    for (; i < 8 && in[i] != ' '; ++i)
    {
        if (pos + 1 >= out_size) break;
        out[pos++] = (char)to_lower((char)in[i]);
    }
    uint8_t ext_non_space = 0;
    for (size_t j = 8; j < 11; ++j)
    {
        if (in[j] != ' ') { ext_non_space = 1; break; }
    }
    if (ext_non_space)
    {
        if (pos + 1 < out_size)
        {
            out[pos++] = '.';
            for (size_t j = 8; j < 11 && in[j] != ' '; ++j)
            {
                if (pos + 1 >= out_size) break;
                out[pos++] = (char)to_lower((char)in[j]);
            }
        }
    }
    out[MIN(pos, out_size - 1)] = '\0';
}

static bool fat_direntry_is_free(const FAT_DirEntry* entry)
{
    return entry->name[0] == 0x00 || entry->name[0] == 0xE5;
}

static bool fat_direntry_is_long(const FAT_DirEntry* entry)
{
    return entry->attr == FAT_ATTR_LONG_NAME;
}

static bool fat_direntry_is_directory(const FAT_DirEntry* entry)
{
    return (entry->attr & FAT_ATTR_DIRECTORY) != 0;
}

static uint32_t fat_direntry_first_cluster(const FAT_DirEntry* entry)
{
    return ((uint32_t)entry->fstClusHI << 16) | entry->fstClusLO;
}

static bool fatfs_read_dir_entry_by_index(FATNodeInfo* dir, size_t target_index, FAT_DirEntry* out_entry, char* out_name, size_t name_size)
{
    FATVolume* volume = dir->volume;
    if (!volume) return false;

    size_t buffer_size = (dir->is_root && volume->type == FAT_TYPE_16) ? volume->bytes_per_sector : volume->cluster_size_bytes;
    uint8_t* sector_buffer = (uint8_t*)malloc(buffer_size);
    if (!sector_buffer) return false;

    size_t logical_index = 0;
    bool found = false;

    if (dir->is_root && volume->type == FAT_TYPE_16)
    {
        uint32_t sectors = volume->root_dir_sectors;
        for (uint32_t i = 0; i < sectors && !found; ++i)
        {
            if (!fat_volume_read_sector(volume, volume->root_dir_sector + i, sector_buffer))
                break;
            FAT_DirEntry* entries = (FAT_DirEntry*)sector_buffer;
            size_t entries_per_sector = volume->bytes_per_sector / sizeof(FAT_DirEntry);
            for (size_t e = 0; e < entries_per_sector; ++e)
            {
                FAT_DirEntry* entry = &entries[e];
                if (entry->name[0] == 0x00)
                {
                    goto done;
                }
                if (fat_direntry_is_free(entry) || fat_direntry_is_long(entry))
                    continue;
                if (entry->attr & FAT_ATTR_VOLUME_ID)
                    continue;
                if (logical_index == target_index)
                {
                    memcpy(out_entry, entry, sizeof(FAT_DirEntry));
                    fatfs_83_to_name(entry->name, out_name, name_size);
                    found = true;
                    goto done;
                }
                logical_index++;
            }
        }
    }
    else
    {
        uint32_t cluster = dir->first_cluster;
        while (!fat_volume_is_end(volume, cluster) && !found)
        {
            if (!fat_volume_read_cluster(volume, cluster, sector_buffer))
                break;
            size_t entries_per_cluster = (volume->cluster_size_bytes) / sizeof(FAT_DirEntry);
            FAT_DirEntry* entries = (FAT_DirEntry*)sector_buffer;
            for (size_t e = 0; e < entries_per_cluster; ++e)
            {
                FAT_DirEntry* entry = &entries[e];
                if (entry->name[0] == 0x00)
                {
                    goto done;
                }
                if (fat_direntry_is_free(entry) || fat_direntry_is_long(entry))
                    continue;
                if (logical_index == target_index)
                {
                    memcpy(out_entry, entry, sizeof(FAT_DirEntry));
                    fatfs_83_to_name(entry->name, out_name, name_size);
                    found = true;
                    goto done;
                }
                logical_index++;
            }
            uint32_t next = fat_volume_get_next_cluster(volume, cluster);
            if (fat_volume_is_bad(volume, next))
            {
                break;
            }
            cluster = next;
        }
    }

done:
    free(sector_buffer);
    return found;
}

static bool fatfs_find_entry(FATNodeInfo* dir, const char* name, FAT_DirEntry* out_entry, char* out_name, size_t name_size)
{
    FATVolume* volume = dir->volume;
    if (!volume) return false;

    char short_name[11];
    bool want_short = fatfs_name_to_83(name, short_name);

    size_t buffer_size = (dir->is_root && volume->type == FAT_TYPE_16) ? volume->bytes_per_sector : volume->cluster_size_bytes;
    uint8_t* sector_buffer = (uint8_t*)malloc(buffer_size);
    if (!sector_buffer) return false;

    bool found = false;

    if (dir->is_root && volume->type == FAT_TYPE_16)
    {
        uint32_t sectors = volume->root_dir_sectors;
        for (uint32_t i = 0; i < sectors && !found; ++i)
        {
            if (!fat_volume_read_sector(volume, volume->root_dir_sector + i, sector_buffer))
                break;
            FAT_DirEntry* entries = (FAT_DirEntry*)sector_buffer;
            size_t entries_per_sector = volume->bytes_per_sector / sizeof(FAT_DirEntry);
            for (size_t e = 0; e < entries_per_sector; ++e)
            {
                FAT_DirEntry* entry = &entries[e];
                if (entry->name[0] == 0x00)
                {
                    goto done;
                }
                if (fat_direntry_is_free(entry) || fat_direntry_is_long(entry))
                    continue;
                if (entry->attr & FAT_ATTR_VOLUME_ID)
                    continue;
                char entry_name[64];
                fatfs_83_to_name(entry->name, entry_name, sizeof(entry_name));
                if ((want_short && memcmp(entry->name, short_name, 11) == 0) || strcasecmp(entry_name, name) == 0)
                {
                    memcpy(out_entry, entry, sizeof(FAT_DirEntry));
                    fatfs_83_to_name(entry->name, out_name, name_size);
                    found = true;
                    goto done;
                }
            }
        }
    }
    else
    {
        uint32_t cluster = dir->first_cluster;
        while (!fat_volume_is_end(volume, cluster) && !found)
        {
            if (!fat_volume_read_cluster(volume, cluster, sector_buffer))
                break;
            size_t entries_per_cluster = volume->cluster_size_bytes / sizeof(FAT_DirEntry);
            FAT_DirEntry* entries = (FAT_DirEntry*)sector_buffer;
            for (size_t e = 0; e < entries_per_cluster; ++e)
            {
                FAT_DirEntry* entry = &entries[e];
                if (entry->name[0] == 0x00)
                {
                    goto done;
                }
                if (fat_direntry_is_free(entry) || fat_direntry_is_long(entry))
                    continue;
                if (entry->attr & FAT_ATTR_VOLUME_ID)
                    continue;
                char entry_name[64];
                fatfs_83_to_name(entry->name, entry_name, sizeof(entry_name));
                if ((want_short && memcmp(entry->name, short_name, 11) == 0) || strcasecmp(entry_name, name) == 0)
                {
                    memcpy(out_entry, entry, sizeof(FAT_DirEntry));
                    fatfs_83_to_name(entry->name, out_name, name_size);
                    found = true;
                    goto done;
                }
            }
            uint32_t next = fat_volume_get_next_cluster(volume, cluster);
            if (fat_volume_is_bad(volume, next))
            {
                break;
            }
            cluster = next;
        }
    }

done:
    free(sector_buffer);
    return found;
}

static int64_t fatfs_read_file(FATNodeInfo* node, uint64_t offset, void* buffer, size_t size)
{
    if (!node || !buffer) return -1;
    FATVolume* volume = node->volume;
    if (!volume) return -1;

    if (offset >= node->size) return 0;

    size_t remaining = node->size - (size_t)offset;
    size_t to_read = MIN(size, remaining);
    if (to_read == 0) return 0;

    uint32_t cluster_size = volume->cluster_size_bytes;
    uint32_t cluster = node->first_cluster;
    if (cluster < 2)
        return -1;

    uint32_t skip_clusters = (uint32_t)(offset / cluster_size);
    uint32_t cluster_offset = (uint32_t)(offset % cluster_size);

    for (uint32_t i = 0; i < skip_clusters; ++i)
    {
        cluster = fat_volume_get_next_cluster(volume, cluster);
        if (fat_volume_is_end(volume, cluster))
            return 0;
    }

    uint8_t* temp = (uint8_t*)malloc(cluster_size);
    if (!temp) return -1;

    size_t total_read = 0;

    while (to_read > 0 && !fat_volume_is_end(volume, cluster))
    {
        if (!fat_volume_read_cluster(volume, cluster, temp))
            break;

        size_t start = total_read == 0 ? cluster_offset : 0;
        size_t available = cluster_size - start;
        size_t chunk = MIN(to_read, available);

        memcpy((uint8_t*)buffer + total_read, temp + start, chunk);

        total_read += chunk;
        to_read -= chunk;

        uint32_t next = fat_volume_get_next_cluster(volume, cluster);
        if (fat_volume_is_bad(volume, next))
        {
            break;
        }
        cluster = next;
        cluster_offset = 0;
    }

    free(temp);
    return (int64_t)total_read;
}

void FATFS_Register(void)
{
    if (!s_fat_fs.ops)
    {
        s_fat_fs.ops = &s_fat_ops;
        if (VFS_RegisterFileSystem(&s_fat_fs) != VFS_RES_OK)
        {
            WARN("FATFS_Register: VFS registration failed");
        }
    }
}

VFSResult FATFS_Mount(Volume* volume, const char* mount_path)
{
    if (!volume || !mount_path) return VFS_RES_INVALID;
    FATFS_Register();
    VFSFileSystem* fs = VFS_GetFileSystem("fat");
    if (!fs) return VFS_RES_ERROR;

    VFSMountParams params = {
        .source = Volume_Name(volume),
        .block_device = volume->device,
        .volume = volume,
        .context = NULL,
        .flags = 0,
    };

    VFSMount* mount = VFS_Mount(mount_path, fs, &params);
    return mount ? VFS_RES_OK : VFS_RES_ERROR;
}

static VFSResult fat_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root)
{
    (void)fs;
    if (!params || (!params->block_device && !params->volume) || !out_root)
        return VFS_RES_INVALID;

    FATVolume* volume = (FATVolume*)malloc(sizeof(FATVolume));
    if (!volume) return VFS_RES_NO_MEMORY;
    memset(volume, 0, sizeof(FATVolume));

    uint32_t logical_block_size = 0;
    FAT_BootSector bpb;
    if (!fat_read_boot_sector(params, &bpb, &logical_block_size))
    {
        free(volume);
        return VFS_RES_ERROR;
    }

    Volume* backing_volume = params->volume;
    BlockDevice* device = params->block_device ? params->block_device : (backing_volume ? backing_volume->device : NULL);
    uint64_t lba_offset = backing_volume ? 0 : 0;

    if (!fat_volume_init(volume, backing_volume, device, lba_offset, &bpb))
    {
        fatfs_destroy_volume(volume);
        return VFS_RES_UNSUPPORTED;
    }

    VFSNode* root = fatfs_alloc_node(volume, NULL, "", VFS_NODE_DIRECTORY, NULL);
    if (!root)
    {
        fatfs_destroy_volume(volume);
        return VFS_RES_NO_MEMORY;
    }

    FATNodeInfo* info = fat_node_info(root);
    info->volume = volume;
    info->is_root = true;
    if (volume->type == FAT_TYPE_16)
    {
        info->first_cluster = 0;
    }
    else
    {
        info->first_cluster = volume->root_cluster;
    }
    info->size = 0;
    info->attr = FAT_ATTR_DIRECTORY;

    root->mount = NULL;
    root->parent = NULL;

    *out_root = root;

    LOG("FAT: mounted volume '%s' (%s)",
        params->source ? params->source : "unnamed",
        fat_volume_type_name(volume));
    return VFS_RES_OK;
}

static VFSResult fat_unmount(VFSFileSystem* fs, VFSNode* root)
{
    (void)fs;
    if (!root) return VFS_RES_INVALID;
    FATNodeInfo* info = fat_node_info(root);
    FATVolume* volume = info ? info->volume : NULL;

    fatfs_destroy_volume(volume);
    return VFS_RES_OK;
}

static bool fat_probe(VFSFileSystem* fs, const VFSMountParams* params)
{
    (void)fs;
    FAT_BootSector bpb;
    uint32_t block_size = 0;
    if (!fat_read_boot_sector(params, &bpb, &block_size))
        return false;

    // Basic sanity checks that help reject non-FAT boot records (e.g., ISO9660).
    if (!(bpb.jmpBoot[0] == 0xEB || bpb.jmpBoot[0] == 0xE9))
        return false;

    if (bpb.bytesPerSector == 0 || (bpb.bytesPerSector & (bpb.bytesPerSector - 1)) != 0)
        return false;

    if (bpb.sectorsPerCluster == 0 || (bpb.sectorsPerCluster & (bpb.sectorsPerCluster - 1)) != 0)
        return false;

    if (bpb.sectorsPerCluster > 128)
        return false;

    if (bpb.numFATs == 0 || bpb.numFATs > 2)
        return false;

    if (bpb.reservedSectorCount == 0)
        return false;

    FATVolume temp;
    memset(&temp, 0, sizeof(temp));
    if (!fat_volume_probe_type(&temp, &bpb))
        return false;

    if (temp.type == FAT_TYPE_32)
    {
        if (bpb.spec.fat32.bootSignature != 0x29)
            return false;
    }
    else
    {
        if (bpb.spec.fat16.bootSignature != 0x29)
            return false;
    }

    return true;
}

static bool fat_read_boot_sector(const VFSMountParams* params, FAT_BootSector* out_bpb, uint32_t* out_block_size)
{
    if (!params || (!params->block_device && !params->volume) || !out_bpb)
        return false;

    uint32_t logical_block_size = 512;
    if (params->volume)
    {
        uint32_t bs = Volume_BlockSize(params->volume);
        if (bs) logical_block_size = bs;
    }
    else if (params->block_device && params->block_device->logical_block_size)
    {
        logical_block_size = params->block_device->logical_block_size;
    }

    if (logical_block_size == 0)
        logical_block_size = 512;

    uint8_t* sector = (uint8_t*)malloc(logical_block_size);
    if (!sector)
        return false;

    bool read_ok = false;
    if (params->volume)
        read_ok = Volume_ReadSectors(params->volume, 0, 1, sector);
    else if (params->block_device)
        read_ok = BlockDevice_Read(params->block_device, 0, 1, sector);

    if (!read_ok)
    {
        free(sector);
        return false;
    }

    bool signature_valid = (sector[510] == 0x55 && sector[511] == 0xAA);
    memcpy(out_bpb, sector, sizeof(FAT_BootSector));
    free(sector);

    if (out_block_size)
        *out_block_size = logical_block_size;

    if (!signature_valid)
        return false;

    return true;
}

static VFSResult fat_node_open(VFSNode* node, uint32_t mode, void** out_handle)
{
    (void)mode;
    if (!node) return VFS_RES_INVALID;
    FATNodeInfo* info = fat_node_info(node);
    if (!info) return VFS_RES_ERROR;

    if (node->type == VFS_NODE_DIRECTORY && (mode & VFS_OPEN_WRITE))
        return VFS_RES_ACCESS;

    FATHandle* handle = (FATHandle*)malloc(sizeof(FATHandle));
    if (!handle) return VFS_RES_NO_MEMORY;
    handle->node = info;
    if (out_handle) *out_handle = handle;
    return VFS_RES_OK;
}

static VFSResult fat_node_close(VFSNode* node, void* handle)
{
    (void)node;
    if (handle) free(handle);
    return VFS_RES_OK;
}

static int64_t fat_node_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size)
{
    (void)handle;
    if (!node || !buffer || size == 0) return -1;
    FATNodeInfo* info = fat_node_info(node);
    if (!info) return -1;

    if (node->type == VFS_NODE_DIRECTORY)
        return -1;

    return fatfs_read_file(info, offset, buffer, size);
}

static int64_t fat_node_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size)
{
    (void)node; (void)handle; (void)offset; (void)buffer; (void)size;
    return -1;
}

static VFSResult fat_node_truncate(VFSNode* node, void* handle, uint64_t length)
{
    (void)node; (void)handle; (void)length;
    return VFS_RES_UNSUPPORTED;
}

static VFSResult fat_node_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry)
{
    (void)handle;
    if (!node || !out_entry) return VFS_RES_INVALID;
    if (node->type != VFS_NODE_DIRECTORY) return VFS_RES_INVALID;

    FATNodeInfo* info = fat_node_info(node);
    if (!info) return VFS_RES_ERROR;

    FAT_DirEntry entry;
    char name[64];
    if (!fatfs_read_dir_entry_by_index(info, index, &entry, name, sizeof(name)))
        return VFS_RES_NOT_FOUND;

    memset(out_entry->name, 0, sizeof(out_entry->name));
    size_t len = strlen(name);
    if (len > VFS_NAME_MAX) len = VFS_NAME_MAX;
    memcpy(out_entry->name, name, len);
    out_entry->name[len] = '\0';
    out_entry->type = fat_direntry_is_directory(&entry) ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR;
    return VFS_RES_OK;
}

static VFSResult fat_node_lookup(VFSNode* node, const char* name, VFSNode** out_node)
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

    FATNodeInfo* dir_info = fat_node_info(node);
    if (!dir_info) return VFS_RES_ERROR;

    FAT_DirEntry entry;
    char actual_name[64];
    if (!fatfs_find_entry(dir_info, name, &entry, actual_name, sizeof(actual_name)))
        return VFS_RES_NOT_FOUND;

    FATVolume* volume = dir_info->volume;
    VFSNode* child = fatfs_alloc_node(volume, node, actual_name, fat_direntry_is_directory(&entry) ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR, NULL);
    if (!child)
        return VFS_RES_NO_MEMORY;

    FATNodeInfo* info = fat_node_info(child);
    info->volume = volume;
    info->first_cluster = fat_direntry_first_cluster(&entry);
    info->size = entry.fileSize;
    info->attr = entry.attr;
    info->is_root = false;

    *out_node = child;
    return VFS_RES_OK;
}

static VFSResult fat_node_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node)
{
    (void)node; (void)name; (void)type; (void)out_node;
    return VFS_RES_UNSUPPORTED;
}

static VFSResult fat_node_remove(VFSNode* node, const char* name)
{
    (void)node; (void)name;
    return VFS_RES_UNSUPPORTED;
}

static VFSResult fat_node_stat(VFSNode* node, VFSNodeInfo* out_info)
{
    if (!node || !out_info) return VFS_RES_INVALID;
    FATNodeInfo* info = fat_node_info(node);
    if (!info) return VFS_RES_ERROR;

    out_info->type = node->type;
    out_info->flags = node->flags;
    out_info->inode = info->first_cluster;
    out_info->size = info->size;
    out_info->atime = 0;
    out_info->mtime = 0;
    out_info->ctime = 0;
    return VFS_RES_OK;
}
