#include <filesystem/ntfs.h>
#include <memory/memory.h>
#include <util/string.h>
#include <debug/debug.h>
#include <list.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define NTFS_SIGNATURE "FILE"
#define NTFS_OEM_STRING "NTFS    "

#define NTFS_ATTR_STANDARD_INFORMATION 0x10
#define NTFS_ATTR_ATTRIBUTE_LIST       0x20
#define NTFS_ATTR_FILE_NAME            0x30
#define NTFS_ATTR_OBJECT_ID            0x40
#define NTFS_ATTR_SECURITY_DESCRIPTOR  0x50
#define NTFS_ATTR_VOLUME_NAME          0x60
#define NTFS_ATTR_VOLUME_INFORMATION   0x70
#define NTFS_ATTR_DATA                 0x80
#define NTFS_ATTR_INDEX_ROOT           0x90
#define NTFS_ATTR_INDEX_ALLOCATION     0xA0
#define NTFS_ATTR_BITMAP               0xB0
#define NTFS_ATTR_REPARSE_POINT        0xC0

#define NTFS_FILE_FLAG_IN_USE      0x0001
#define NTFS_FILE_FLAG_DIRECTORY   0x0002

#define NTFS_FILE_ATTR_READONLY    0x00000001
#define NTFS_FILE_ATTR_DIRECTORY   0x00000010
#define NTFS_FILE_ATTR_ARCHIVE     0x00000020

#define NTFS_INDEX_ENTRY_FLAG_SUBNODE  0x01
#define NTFS_INDEX_ENTRY_FLAG_LAST     0x02

typedef struct __attribute__((packed)) NTFSBootSector {
    uint8_t  jump[3];
    char     oem[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  unused1[5];
    uint8_t  media_descriptor;
    uint8_t  unused2[18];
    uint64_t total_sectors;
    uint64_t mft_lcn;
    uint64_t mftmirr_lcn;
    int8_t   clusters_per_file_record;
    uint8_t  reserved3[3];
    int8_t   clusters_per_index_buffer;
    uint8_t  reserved4[3];
    uint64_t volume_serial;
    uint32_t checksum;
    uint8_t  bootstrap[426];
    uint16_t end_marker;
} NTFSBootSector;

typedef struct __attribute__((packed)) NTFSFileRecordHeader {
    char     signature[4];
    uint16_t fixup_offset;
    uint16_t fixup_entries;
    uint64_t log_sequence_number;
    uint16_t sequence_number;
    uint16_t hard_link_count;
    uint16_t first_attribute_offset;
    uint16_t flags;
    uint32_t bytes_in_use;
    uint32_t bytes_allocated;
    uint64_t base_file_record;
    uint16_t next_attribute_id;
    uint16_t align;
    uint32_t record_number;
} NTFSFileRecordHeader;

typedef struct __attribute__((packed)) NTFSAttributeHeader {
    uint32_t type;
    uint32_t length;
    uint8_t  non_resident;
    uint8_t  name_length;
    uint16_t name_offset;
    uint16_t flags;
    uint16_t attribute_id;
    union {
        struct __attribute__((packed)) {
            uint32_t value_length;
            uint16_t value_offset;
            uint8_t  resident_flags;
            uint8_t  reserved;
        } resident;
        struct __attribute__((packed)) {
            uint64_t low_vcn;
            uint64_t high_vcn;
            uint16_t data_run_offset;
            uint16_t compression_unit;
            uint32_t padding;
            uint64_t allocated_size;
            uint64_t data_size;
            uint64_t initialized_size;
            uint64_t compressed_size;
        } non_resident;
    } body;
} NTFSAttributeHeader;

typedef struct __attribute__((packed)) NTFSIndexRootHeader {
    uint32_t attribute_type;
    uint32_t collation_rule;
    uint32_t index_record_size;
    uint8_t  clusters_per_index_record;
    uint8_t  reserved[3];
} NTFSIndexRootHeader;

typedef struct __attribute__((packed)) NTFSIndexHeader {
    uint32_t entries_offset;
    uint32_t entries_size;
    uint32_t entries_allocated;
    uint8_t  flags;
    uint8_t  reserved[3];
} NTFSIndexHeader;

typedef struct __attribute__((packed)) NTFSIndexEntryHeader {
    uint64_t file_reference;
    uint16_t entry_size;
    uint16_t stream_size;
    uint32_t flags;
    /* followed by key (typically FILE_NAME attribute) */
} NTFSIndexEntryHeader;

typedef struct __attribute__((packed)) NTFSFileNameAttribute {
    uint64_t parent_directory;
    uint64_t creation_time;
    uint64_t modification_time;
    uint64_t mft_modification_time;
    uint64_t access_time;
    uint64_t allocated_size;
    uint64_t real_size;
    uint32_t flags;
    uint32_t ea_reparse;
    uint8_t  name_length;
    uint8_t  namespace_id;
    /* followed by UTF-16LE name */
} NTFSFileNameAttribute;

typedef struct NTFSDataRun {
    uint64_t vcn;
    uint64_t length;         // in clusters
    int64_t  lcn;            // absolute logical cluster number
} NTFSDataRun;

typedef struct NTFSRunlist {
    NTFSDataRun* runs;
    size_t count;
    size_t capacity;
} NTFSRunlist;

typedef struct NTFSVolume {
    Volume* backing_volume;
    BlockDevice* device;
    uint64_t lba_offset;
    uint32_t logical_block_size;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t bytes_per_cluster;
    int32_t clusters_per_mft_record;
    uint32_t mft_record_size;
    int32_t clusters_per_index_record;
    uint32_t index_record_size;
    uint64_t mft_lcn;
    uint64_t mftmirr_lcn;
    NTFSRunlist mft_runlist;
    List* nodes;
} NTFSVolume;

typedef struct NTFSNodeInfo {
    NTFSVolume* volume;
    uint64_t file_reference; // entry number (lower 48 bits)
    uint64_t parent_reference;
    uint64_t file_size;
    bool     is_directory;
    bool     is_root;
    bool     overlay;           // true for runtime-only nodes
    uint8_t* overlay_data;
    size_t   overlay_size;
    size_t   overlay_capacity;
    List*    overlay_children;  // runtime-only children (directories)
} NTFSNodeInfo;

typedef struct NTFSHandle {
    NTFSNodeInfo* node;
    NTFSRunlist   runlist;
    bool          runlist_valid;
} NTFSHandle;

static VFSFileSystem s_ntfs_fs = {
    .name = "ntfs",
    .flags = 0,
    .ops = NULL,
    .driver_context = NULL,
};

static VFSResult ntfs_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root);
static VFSResult ntfs_unmount(VFSFileSystem* fs, VFSNode* root);
static VFSResult ntfs_node_open(VFSNode* node, uint32_t mode, void** out_handle);
static VFSResult ntfs_node_close(VFSNode* node, void* handle);
static int64_t   ntfs_node_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size);
static VFSResult ntfs_node_truncate(VFSNode* node, void* handle, uint64_t length);
static VFSResult ntfs_node_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry);
static VFSResult ntfs_node_lookup(VFSNode* node, const char* name, VFSNode** out_node);
static VFSResult ntfs_node_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node);
static VFSResult ntfs_node_remove(VFSNode* node, const char* name);
static VFSResult ntfs_node_stat(VFSNode* node, VFSNodeInfo* out_info);
static int64_t   ntfs_node_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size);
static bool      ntfs_probe(VFSFileSystem* fs, const VFSMountParams* params);

static const VFSNodeOps s_ntfs_node_ops = {
    .open     = ntfs_node_open,
    .close    = ntfs_node_close,
    .read     = ntfs_node_read,
    .write    = ntfs_node_write,
    .truncate = ntfs_node_truncate,
    .readdir  = ntfs_node_readdir,
    .lookup   = ntfs_node_lookup,
    .create   = ntfs_node_create,
    .remove   = ntfs_node_remove,
    .stat     = ntfs_node_stat,
};

static const VFSFileSystemOps s_ntfs_ops = {
    .probe   = ntfs_probe,
    .mount   = ntfs_mount,
    .unmount = ntfs_unmount,
};

static inline uint64_t ntfs_file_reference_number(uint64_t reference)
{
    return reference & 0x0000FFFFFFFFFFFFULL;
}

static inline uint16_t ntfs_file_reference_sequence(uint64_t reference)
{
    return (uint16_t)(reference >> 48);
}

/* Forward declarations for helpers */
static bool ntfs_read_boot_sector(NTFSVolume* volume, NTFSBootSector* out_boot);
static uint32_t ntfs_compute_record_size(int32_t clusters, uint32_t bytes_per_cluster);
static bool ntfs_runlist_init(NTFSRunlist* runlist);
static void ntfs_runlist_reset(NTFSRunlist* runlist);
static bool ntfs_runlist_append(NTFSRunlist* runlist, uint64_t vcn, uint64_t length, int64_t lcn);
static bool ntfs_parse_data_runs(const uint8_t* data, size_t length, NTFSRunlist* runlist);
static bool ntfs_read_bytes(NTFSVolume* volume, uint64_t offset, void* buffer, size_t size);
static bool ntfs_read_mft_record(NTFSVolume* volume, uint64_t record_index, uint8_t* buffer);
static bool ntfs_apply_fixup(uint8_t* buffer, size_t buffer_size, uint32_t bytes_per_sector);
static NTFSAttributeHeader* ntfs_first_attribute(uint8_t* record);
static NTFSAttributeHeader* ntfs_next_attribute(NTFSAttributeHeader* attr);
static bool ntfs_populate_node_info(NTFSVolume* volume, uint64_t file_ref, NTFSNodeInfo* info, char* out_name, size_t out_name_len);
static VFSNode* ntfs_alloc_node(NTFSVolume* volume, VFSNode* parent, const char* name, bool is_directory, uint64_t file_ref, uint64_t file_size, bool is_root);
static void ntfs_free_node(VFSNode* node);
static void ntfs_destroy_volume(NTFSVolume* volume);
static bool ntfs_fetch_default_data_runlist(NTFSNodeInfo* info, NTFSRunlist* out_runlist, uint64_t* out_data_size, bool* out_resident, uint8_t** out_resident_value, size_t* out_resident_length);
static int64_t ntfs_read_from_runlist(NTFSNodeInfo* info, NTFSRunlist* runlist, uint64_t offset, void* buffer, size_t size);
static bool ntfs_enumerate_directory(NTFSNodeInfo* dir, size_t target_index, VFSDirEntry* out_entry, const char* find_name, uint64_t* out_child_ref);
static uint32_t ntfs_device_block_size(const NTFSVolume* volume);
static bool ntfs_read_blocks(NTFSVolume* volume, uint64_t lba, uint32_t count, void* buffer);
static bool ntfs_overlay_reserve(NTFSNodeInfo* info, size_t required);
static VFSNode* ntfs_overlay_find_child(NTFSNodeInfo* dir, const char* name);
static size_t ntfs_overlay_child_count(NTFSNodeInfo* dir);
static bool ntfs_overlay_add_child(NTFSNodeInfo* dir, VFSNode* child);
static size_t ntfs_directory_disk_entry_count(NTFSNodeInfo* dir);

void NTFS_Register(void)
{
    if (!s_ntfs_fs.ops)
    {
        s_ntfs_fs.ops = &s_ntfs_ops;
        if (VFS_RegisterFileSystem(&s_ntfs_fs) != VFS_RES_OK)
        {
            WARN("NTFS_Register: VFS registration failed");
        }
    }
}

VFSResult NTFS_Mount(Volume* volume, const char* mount_path)
{
    if (!volume || !mount_path) return VFS_RES_INVALID;
    NTFS_Register();
    VFSFileSystem* fs = VFS_GetFileSystem("ntfs");
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

static VFSResult ntfs_mount(VFSFileSystem* fs, const VFSMountParams* params, VFSNode** out_root)
{
    (void)fs;
    if (!params || (!params->block_device && !params->volume) || !out_root)
        return VFS_RES_INVALID;

    Volume* backing_volume = params->volume;
    BlockDevice* device = params->block_device ? params->block_device : (backing_volume ? backing_volume->device : NULL);
    if (!device)
        return VFS_RES_INVALID;

    uint32_t logical_block_size = 512;
    if (backing_volume)
    {
        uint32_t bs = Volume_BlockSize(backing_volume);
        if (bs) logical_block_size = bs;
    }
    else if (device->logical_block_size)
    {
        logical_block_size = device->logical_block_size;
    }

    NTFSVolume* volume = (NTFSVolume*)malloc(sizeof(NTFSVolume));
    if (!volume)
        return VFS_RES_NO_MEMORY;
    memset(volume, 0, sizeof(NTFSVolume));
    volume->backing_volume = backing_volume;
    volume->device = device;
    volume->logical_block_size = logical_block_size;
    volume->lba_offset = 0;

    if (!ntfs_runlist_init(&volume->mft_runlist))
    {
        free(volume);
        return VFS_RES_NO_MEMORY;
    }

    NTFSBootSector boot;
    if (!ntfs_read_boot_sector(volume, &boot))
    {
        free(volume);
        return VFS_RES_UNSUPPORTED;
    }

    if (memcmp(boot.oem, NTFS_OEM_STRING, 8) != 0)
    {
        free(volume);
        return VFS_RES_UNSUPPORTED;
    }

    volume->bytes_per_sector = boot.bytes_per_sector;
    volume->sectors_per_cluster = boot.sectors_per_cluster;
    volume->bytes_per_cluster = (uint32_t)volume->bytes_per_sector * (uint32_t)volume->sectors_per_cluster;
    volume->clusters_per_mft_record = boot.clusters_per_file_record;
    volume->clusters_per_index_record = boot.clusters_per_index_buffer;
    volume->mft_record_size = ntfs_compute_record_size(volume->clusters_per_mft_record, volume->bytes_per_cluster);
    volume->index_record_size = ntfs_compute_record_size(volume->clusters_per_index_record, volume->bytes_per_cluster);
    volume->mft_lcn = boot.mft_lcn;
    volume->mftmirr_lcn = boot.mftmirr_lcn;
    volume->nodes = List_Create();
    if (!volume->nodes)
    {
        ntfs_destroy_volume(volume);
        return VFS_RES_NO_MEMORY;
    }

    if (volume->bytes_per_sector == 0 || volume->sectors_per_cluster == 0 || volume->mft_record_size == 0)
    {
        ntfs_destroy_volume(volume);
        return VFS_RES_UNSUPPORTED;
    }

    // Load MFT runlist from $MFT (record 0)
    uint8_t* record = (uint8_t*)malloc(volume->mft_record_size);
    if (!record)
    {
        ntfs_destroy_volume(volume);
        return VFS_RES_NO_MEMORY;
    }

    if (!ntfs_read_mft_record(volume, 0, record))
    {
        free(record);
        ntfs_destroy_volume(volume);
        return VFS_RES_ERROR;
    }

    NTFSAttributeHeader* attr = ntfs_first_attribute(record);
    bool mft_runs_loaded = false;
    while (attr && attr->type != 0xFFFFFFFF)
    {
        if (attr->type == NTFS_ATTR_DATA && attr->non_resident)
        {
            const uint8_t* run_data = (const uint8_t*)attr + attr->body.non_resident.data_run_offset;
            size_t run_len = attr->length - attr->body.non_resident.data_run_offset;
            ntfs_runlist_reset(&volume->mft_runlist);
            if (!ntfs_parse_data_runs(run_data, run_len, &volume->mft_runlist))
            {
                free(record);
                ntfs_destroy_volume(volume);
                return VFS_RES_ERROR;
            }
            mft_runs_loaded = true;
            break;
        }
        attr = ntfs_next_attribute(attr);
    }

    free(record);

    if (!mft_runs_loaded)
    {
        // Fall back to single run at boot.mft_lcn covering first few clusters
        if (!ntfs_runlist_append(&volume->mft_runlist, 0, 16, (int64_t)volume->mft_lcn))
        {
            ntfs_destroy_volume(volume);
            return VFS_RES_ERROR;
        }
    }

    VFSNode* root = ntfs_alloc_node(volume, NULL, "", true, 5, 0, true);
    if (!root)
    {
        ntfs_destroy_volume(volume);
        return VFS_RES_NO_MEMORY;
    }

    NTFSNodeInfo* root_info = (NTFSNodeInfo*)root->internal_data;
    char dummy[VFS_NAME_MAX + 1];
    if (!ntfs_populate_node_info(volume, root_info->file_reference, root_info, dummy, sizeof(dummy)))
    {
        ntfs_free_node(root);
        ntfs_destroy_volume(volume);
        return VFS_RES_ERROR;
    }

    *out_root = root;
    LOG("NTFS: mounted volume '%s' (bytes/sector=%u sectors/cluster=%u record=%u)",
        params->source ? params->source : "disk",
        volume->bytes_per_sector,
        volume->sectors_per_cluster,
        volume->mft_record_size);

    return VFS_RES_OK;
}

static VFSResult ntfs_unmount(VFSFileSystem* fs, VFSNode* root)
{
    (void)fs;
    if (!root) return VFS_RES_INVALID;
    NTFSNodeInfo* info = (NTFSNodeInfo*)root->internal_data;
    NTFSVolume* volume = info ? info->volume : NULL;
    ntfs_destroy_volume(volume);
    return VFS_RES_OK;
}

static bool ntfs_probe(VFSFileSystem* fs, const VFSMountParams* params)
{
    (void)fs;
    if (!params)
        return false;

    Volume* backing_volume = params->volume;
    BlockDevice* device = params->block_device ? params->block_device : (backing_volume ? backing_volume->device : NULL);
    if (!device)
        return false;

    uint32_t logical_block_size = 512;
    if (backing_volume)
    {
        uint32_t bs = Volume_BlockSize(backing_volume);
        if (bs) logical_block_size = bs;
    }
    else if (device->logical_block_size)
    {
        logical_block_size = device->logical_block_size;
    }

    NTFSVolume temp;
    memset(&temp, 0, sizeof(temp));
    temp.backing_volume = backing_volume;
    temp.device = device;
    temp.logical_block_size = logical_block_size;

    NTFSBootSector boot;
    if (!ntfs_read_boot_sector(&temp, &boot))
        return false;

    if (memcmp(boot.oem, NTFS_OEM_STRING, 8) != 0)
        return false;

    if (boot.bytes_per_sector == 0 || boot.sectors_per_cluster == 0)
        return false;

    return true;
}

static VFSResult ntfs_node_open(VFSNode* node, uint32_t mode, void** out_handle)
{
    if (!node) return VFS_RES_INVALID;

    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (!info) return VFS_RES_ERROR;

    bool wants_write = (mode & (VFS_OPEN_WRITE | VFS_OPEN_APPEND | VFS_OPEN_TRUNC)) != 0;

    if (!info->overlay && wants_write)
        return VFS_RES_ACCESS;

    if (info->overlay)
    {
        if (info->is_directory)
        {
            if (wants_write)
                return VFS_RES_ACCESS;
            if (out_handle) *out_handle = NULL;
            return VFS_RES_OK;
        }

        if (mode & VFS_OPEN_TRUNC)
        {
            info->overlay_size = 0;
            info->file_size = 0;
        }
    }

    NTFSHandle* handle = (NTFSHandle*)malloc(sizeof(NTFSHandle));
    if (!handle) return VFS_RES_NO_MEMORY;
    memset(handle, 0, sizeof(NTFSHandle));
    handle->node = info;

    if (out_handle) *out_handle = handle;
    return VFS_RES_OK;
}

static VFSResult ntfs_node_close(VFSNode* node, void* handle)
{
    (void)node;
    if (!handle) return VFS_RES_OK;
    NTFSHandle* h = (NTFSHandle*)handle;
    ntfs_runlist_reset(&h->runlist);
    free(h->runlist.runs);
    free(h);
    return VFS_RES_OK;
}

static int64_t ntfs_node_read(VFSNode* node, void* handle, uint64_t offset, void* buffer, size_t size)
{
    if (!node || !buffer || size == 0) return -1;
    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (!info || info->is_directory) return -1;

    if (info->overlay)
    {
        if (offset >= info->overlay_size)
            return 0;

        size_t available = (size_t)MIN((uint64_t)size, info->overlay_size - offset);
        if (available == 0 || !info->overlay_data)
            return 0;

        memcpy(buffer, info->overlay_data + offset, available);
        return (int64_t)available;
    }

    NTFSHandle* h = (NTFSHandle*)handle;
    NTFSRunlist temp_runlist = {0};
    NTFSRunlist* runlist = NULL;
    uint64_t data_size = 0;
    bool resident = false;
    uint8_t* resident_value = NULL;
    size_t resident_length = 0;

    if (!ntfs_fetch_default_data_runlist(info,
                                         h ? &h->runlist : &temp_runlist,
                                         &data_size,
                                         &resident,
                                         &resident_value,
                                         &resident_length))
    {
        if (!h)
        {
            ntfs_runlist_reset(&temp_runlist);
            free(temp_runlist.runs);
        }
        return -1;
    }

    if (offset >= data_size)
    {
        if (!h)
        {
            ntfs_runlist_reset(&temp_runlist);
            free(temp_runlist.runs);
        }
        return 0;
    }

    size_t available = (size_t)MIN((uint64_t)size, data_size - offset);
    int64_t result = -1;

    if (resident)
    {
        if (offset + available > resident_length)
        {
            available = (size_t)MAX((int64_t)resident_length - (int64_t)offset, 0);
        }
        if (available > 0 && resident_value)
        {
            memcpy(buffer, resident_value + offset, available);
        }
        result = (int64_t)available;
    }
    else
    {
        runlist = h ? &h->runlist : &temp_runlist;
        result = ntfs_read_from_runlist(info, runlist, offset, buffer, available);
    }

    if (resident_value)
    {
        free(resident_value);
    }

    if (!h)
    {
        ntfs_runlist_reset(&temp_runlist);
        free(temp_runlist.runs);
    }

    return result;
}

static int64_t ntfs_node_write(VFSNode* node, void* handle, uint64_t offset, const void* buffer, size_t size)
{
    (void)handle;
    if (!node || !buffer || size == 0) return -1;

    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (!info || info->is_directory) return -1;

    if (!info->overlay)
        return -1;

    uint64_t end = offset + size;
    if (end > SIZE_MAX)
        return -1;

    if (!ntfs_overlay_reserve(info, (size_t)end))
        return -1;

    memcpy(info->overlay_data + offset, buffer, size);
    if (end > info->overlay_size)
        info->overlay_size = (size_t)end;
    info->file_size = info->overlay_size;

    return (int64_t)size;
}

static VFSResult ntfs_node_truncate(VFSNode* node, void* handle, uint64_t length)
{
    (void)handle;
    if (!node) return VFS_RES_INVALID;

    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (!info || info->is_directory) return VFS_RES_INVALID;

    if (!info->overlay)
        return VFS_RES_UNSUPPORTED;

    if (length > SIZE_MAX)
        return VFS_RES_NO_SPACE;

    size_t new_size = (size_t)length;
    if (!ntfs_overlay_reserve(info, new_size))
        return VFS_RES_NO_MEMORY;

    if (new_size > info->overlay_size)
    {
        size_t delta = new_size - info->overlay_size;
        memset(info->overlay_data + info->overlay_size, 0, delta);
    }

    info->overlay_size = new_size;
    info->file_size = info->overlay_size;
    return VFS_RES_OK;
}

static VFSResult ntfs_node_readdir(VFSNode* node, void* handle, size_t index, VFSDirEntry* out_entry)
{
    (void)handle;
    if (!node || !out_entry) return VFS_RES_INVALID;
    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (!info || !info->is_directory) return VFS_RES_INVALID;

    if (index == 0)
    {
        memset(out_entry, 0, sizeof(VFSDirEntry));
        out_entry->name[0] = '.';
        out_entry->name[1] = '\0';
        out_entry->type = VFS_NODE_DIRECTORY;
        return VFS_RES_OK;
    }
    if (index == 1)
    {
        memset(out_entry, 0, sizeof(VFSDirEntry));
        out_entry->name[0] = '.';
        out_entry->name[1] = '.';
        out_entry->name[2] = '\0';
        out_entry->type = VFS_NODE_DIRECTORY;
        return VFS_RES_OK;
    }

    size_t adjusted_index = index - 2;
    size_t disk_count = 0;
    if (!info->overlay)
    {
        VFSDirEntry entry;
        if (ntfs_enumerate_directory(info, adjusted_index, &entry, NULL, NULL))
        {
            *out_entry = entry;
            return VFS_RES_OK;
        }
        disk_count = ntfs_directory_disk_entry_count(info);
    }

    size_t overlay_count = ntfs_overlay_child_count(info);
    if (adjusted_index < disk_count)
        return VFS_RES_NOT_FOUND;

    size_t overlay_index = adjusted_index - disk_count;
    if (overlay_index >= overlay_count)
        return VFS_RES_NOT_FOUND;

    if (!info->overlay_children)
        return VFS_RES_NOT_FOUND;

    VFSNode* child = (VFSNode*)List_GetAt(info->overlay_children, overlay_index);
    if (!child || !child->name)
        return VFS_RES_NOT_FOUND;

    memset(out_entry, 0, sizeof(VFSDirEntry));
    strncpy(out_entry->name, child->name, VFS_NAME_MAX);
    out_entry->name[VFS_NAME_MAX] = '\0';
    out_entry->type = child->type;
    return VFS_RES_OK;
}

static VFSResult ntfs_node_lookup(VFSNode* node, const char* name, VFSNode** out_node)
{
    if (!node || !name || !out_node) return VFS_RES_INVALID;
    NTFSNodeInfo* dir_info = (NTFSNodeInfo*)node->internal_data;
    if (!dir_info || !dir_info->is_directory) return VFS_RES_INVALID;

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

    VFSNode* overlay_child = ntfs_overlay_find_child(dir_info, name);
    if (overlay_child)
    {
        *out_node = overlay_child;
        return VFS_RES_OK;
    }

    if (dir_info->overlay)
        return VFS_RES_NOT_FOUND;

    uint64_t child_ref = 0;
    if (!ntfs_enumerate_directory(dir_info, (size_t)-1, NULL, name, &child_ref))
        return VFS_RES_NOT_FOUND;

    NTFSNodeInfo child_info;
    memset(&child_info, 0, sizeof(child_info));
    child_info.volume = dir_info->volume;
    child_info.file_reference = child_ref;

    char child_name[VFS_NAME_MAX + 1];
    if (!ntfs_populate_node_info(dir_info->volume, child_ref, &child_info, child_name, sizeof(child_name)))
        return VFS_RES_ERROR;

    VFSNode* existing = NULL;
    // Search existing cached nodes
    if (dir_info->volume->nodes)
    {
        for (ListNode* it = List_Foreach_Begin(dir_info->volume->nodes); it; it = List_Foreach_Next(it))
        {
            VFSNode* candidate = (VFSNode*)List_Foreach_Data(it);
            if (!candidate) continue;
            NTFSNodeInfo* info = (NTFSNodeInfo*)candidate->internal_data;
            if (info && info->file_reference == child_info.file_reference && info->parent_reference == dir_info->file_reference)
            {
                info->file_size = child_info.file_size;
                info->is_directory = child_info.is_directory;
                info->parent_reference = child_info.parent_reference;
                info->volume = child_info.volume;
                existing = candidate;
                break;
            }
        }
    }

    if (existing)
    {
        *out_node = existing;
        return VFS_RES_OK;
    }

    VFSNode* child_node = ntfs_alloc_node(dir_info->volume,
                                          node,
                                          child_name,
                                          child_info.is_directory,
                                          child_info.file_reference,
                                          child_info.file_size,
                                          false);
    if (!child_node)
        return VFS_RES_NO_MEMORY;

    NTFSNodeInfo* info = (NTFSNodeInfo*)child_node->internal_data;
    *info = child_info;
    info->parent_reference = dir_info->file_reference;
    *out_node = child_node;
    return VFS_RES_OK;
}

static VFSResult ntfs_node_create(VFSNode* node, const char* name, VFSNodeType type, VFSNode** out_node)
{
    if (!node || !name || !*name) return VFS_RES_INVALID;
    NTFSNodeInfo* dir_info = (NTFSNodeInfo*)node->internal_data;
    if (!dir_info || !dir_info->is_directory) return VFS_RES_INVALID;

    size_t name_len = strlen(name);
    if (name_len == 0 || name_len > VFS_NAME_MAX)
        return VFS_RES_INVALID;

    if (type != VFS_NODE_REGULAR && type != VFS_NODE_DIRECTORY)
        return VFS_RES_UNSUPPORTED;

    if (ntfs_overlay_find_child(dir_info, name))
        return VFS_RES_EXISTS;

    if (!dir_info->overlay)
    {
        if (ntfs_enumerate_directory(dir_info, (size_t)-1, NULL, name, NULL))
            return VFS_RES_EXISTS;
    }

    VFSNode* child = ntfs_alloc_node(dir_info->volume,
                                     node,
                                     name,
                                     type == VFS_NODE_DIRECTORY,
                                     0,
                                     0,
                                     false);
    if (!child)
        return VFS_RES_NO_MEMORY;

    NTFSNodeInfo* child_info = (NTFSNodeInfo*)child->internal_data;
    child_info->overlay = true;
    child_info->file_reference = 0;
    child_info->parent_reference = dir_info->file_reference;
    child_info->file_size = 0;
    child->flags = 0;

    if (type == VFS_NODE_DIRECTORY)
    {
        child_info->overlay_children = List_Create();
        if (!child_info->overlay_children)
        {
            ntfs_free_node(child);
            return VFS_RES_NO_MEMORY;
        }
    }

    if (!ntfs_overlay_add_child(dir_info, child))
    {
        ntfs_free_node(child);
        return VFS_RES_NO_MEMORY;
    }

    if (out_node)
        *out_node = child;
    return VFS_RES_OK;
}

static VFSResult ntfs_node_remove(VFSNode* node, const char* name)
{
    (void)node;
    (void)name;
    return VFS_RES_UNSUPPORTED;
}

static VFSResult ntfs_node_stat(VFSNode* node, VFSNodeInfo* out_info)
{
    if (!node || !out_info) return VFS_RES_INVALID;
    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (!info) return VFS_RES_ERROR;

    if (info->overlay)
    {
        out_info->type = info->is_directory ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR;
        out_info->flags = 0;
        out_info->size = info->overlay_size;
        out_info->inode = info->file_reference;
        out_info->atime = 0;
        out_info->mtime = 0;
        out_info->ctime = 0;
        return VFS_RES_OK;
    }

    NTFSNodeInfo temp;
    char name_buf[VFS_NAME_MAX + 1];
    if (!ntfs_populate_node_info(info->volume, info->file_reference, &temp, name_buf, sizeof(name_buf)))
        return VFS_RES_ERROR;

    out_info->type = temp.is_directory ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR;
    out_info->flags = VFS_NODE_FLAG_READONLY;
    out_info->size = temp.file_size;
    out_info->inode = temp.file_reference;
    out_info->atime = 0;
    out_info->mtime = 0;
    out_info->ctime = 0;
    return VFS_RES_OK;
}

/* Helper implementations */
static bool ntfs_read_boot_sector(NTFSVolume* volume, NTFSBootSector* out_boot)
{
    if (!volume || !out_boot) return false;
    uint32_t logical_block = volume->logical_block_size ? volume->logical_block_size : 512;
    if (logical_block == 0)
        logical_block = 512;
    uint32_t sector_count = (sizeof(NTFSBootSector) + logical_block - 1) / logical_block;
    if (sector_count == 0)
        sector_count = 1;

    size_t buf_size = (size_t)logical_block * sector_count;
    uint8_t* temp = (uint8_t*)malloc(buf_size);
    if (!temp)
        return false;

    bool ok = false;
    if (volume->backing_volume)
        ok = Volume_ReadSectors(volume->backing_volume, 0, sector_count, temp);
    else if (volume->device)
        ok = BlockDevice_Read(volume->device, volume->lba_offset, sector_count, temp);

    if (ok)
    {
        memcpy(out_boot, temp, sizeof(NTFSBootSector));
    }

    free(temp);
    return ok;
}

static uint32_t ntfs_device_block_size(const NTFSVolume* volume)
{
    if (!volume) return 512;
    if (volume->logical_block_size)
        return volume->logical_block_size;
    if (volume->backing_volume)
    {
        uint32_t bs = Volume_BlockSize(volume->backing_volume);
        if (bs) return bs;
    }
    if (volume->device && volume->device->logical_block_size)
        return volume->device->logical_block_size;
    if (volume->bytes_per_sector)
        return volume->bytes_per_sector;
    return 512;
}

static bool ntfs_read_blocks(NTFSVolume* volume, uint64_t lba, uint32_t count, void* buffer)
{
    if (!volume || !buffer || count == 0) return false;
    if (volume->backing_volume)
        return Volume_ReadSectors(volume->backing_volume, lba, count, buffer);
    if (!volume->device)
        return false;
    return BlockDevice_Read(volume->device, volume->lba_offset + lba, count, buffer);
}

static bool ntfs_overlay_reserve(NTFSNodeInfo* info, size_t required)
{
    if (!info) return false;
    if (info->overlay_capacity >= required)
        return true;

    size_t new_capacity = info->overlay_capacity ? info->overlay_capacity : 64;
    while (new_capacity < required)
    {
        if (new_capacity > SIZE_MAX / 2)
        {
            new_capacity = required;
            break;
        }
        new_capacity *= 2;
    }

    uint8_t* new_data = (uint8_t*)realloc(info->overlay_data, new_capacity);
    if (!new_data)
        return false;

    if (new_capacity > info->overlay_capacity)
    {
        size_t delta = new_capacity - info->overlay_capacity;
        memset(new_data + info->overlay_capacity, 0, delta);
    }

    info->overlay_data = new_data;
    info->overlay_capacity = new_capacity;
    return true;
}

static VFSNode* ntfs_overlay_find_child(NTFSNodeInfo* dir, const char* name)
{
    if (!dir || !name || !dir->overlay_children)
        return NULL;

    for (ListNode* it = List_Foreach_Begin(dir->overlay_children); it; it = List_Foreach_Next(it))
    {
        VFSNode* child = (VFSNode*)List_Foreach_Data(it);
        if (!child || !child->name)
            continue;
        if (strcmp(child->name, name) == 0)
            return child;
    }

    return NULL;
}

static size_t ntfs_overlay_child_count(NTFSNodeInfo* dir)
{
    return dir && dir->overlay_children ? List_Size(dir->overlay_children) : 0;
}

static bool ntfs_overlay_add_child(NTFSNodeInfo* dir, VFSNode* child)
{
    if (!dir || !child)
        return false;

    if (!dir->overlay_children)
    {
        dir->overlay_children = List_Create();
        if (!dir->overlay_children)
            return false;
    }

    List_Add(dir->overlay_children, child);
    return true;
}

static size_t ntfs_directory_disk_entry_count(NTFSNodeInfo* dir)
{
    if (!dir || dir->overlay)
        return 0;

    size_t count = 0;
    VFSDirEntry temp;
    while (ntfs_enumerate_directory(dir, count, &temp, NULL, NULL))
    {
        count++;
    }
    return count;
}

static uint32_t ntfs_compute_record_size(int32_t clusters, uint32_t bytes_per_cluster)
{
    if (clusters > 0)
    {
        return (uint32_t)clusters * bytes_per_cluster;
    }
    else if (clusters < 0)
    {
        int32_t shift = -clusters;
        if (shift >= 31) return 0;
        return (uint32_t)(1u << shift);
    }
    return 0;
}

static bool ntfs_runlist_init(NTFSRunlist* runlist)
{
    if (!runlist) return false;
    runlist->runs = NULL;
    runlist->count = 0;
    runlist->capacity = 0;
    return true;
}

static void ntfs_runlist_reset(NTFSRunlist* runlist)
{
    if (!runlist) return;
    runlist->count = 0;
}

static bool ntfs_runlist_reserve(NTFSRunlist* runlist, size_t capacity)
{
    if (runlist->capacity >= capacity)
        return true;
    size_t new_capacity = runlist->capacity ? runlist->capacity : 4;
    while (new_capacity < capacity)
    {
        if (new_capacity > SIZE_MAX / 2)
        {
            new_capacity = capacity;
            break;
        }
        new_capacity *= 2;
    }
    NTFSDataRun* new_runs = (NTFSDataRun*)realloc(runlist->runs, new_capacity * sizeof(NTFSDataRun));
    if (!new_runs)
        return false;
    runlist->runs = new_runs;
    runlist->capacity = new_capacity;
    return true;
}

static bool ntfs_runlist_append(NTFSRunlist* runlist, uint64_t vcn, uint64_t length, int64_t lcn)
{
    if (!runlist) return false;
    if (!ntfs_runlist_reserve(runlist, runlist->count + 1))
        return false;
    runlist->runs[runlist->count].vcn = vcn;
    runlist->runs[runlist->count].length = length;
    runlist->runs[runlist->count].lcn = lcn;
    runlist->count++;
    return true;
}

static bool ntfs_parse_data_runs(const uint8_t* data, size_t length, NTFSRunlist* runlist)
{
    if (!data || !runlist) return false;
    uint64_t current_vcn = 0;
    int64_t current_lcn = 0;
    size_t offset = 0;

    while (offset < length)
    {
        uint8_t header = data[offset++];
        if (header == 0)
            break;
        uint8_t len_size = header & 0x0F;
        uint8_t off_size = (header >> 4) & 0x0F;
        if (len_size == 0 || offset + len_size + off_size > length)
            return false;

        uint64_t run_length = 0;
        for (uint8_t i = 0; i < len_size; ++i)
        {
            run_length |= (uint64_t)data[offset + i] << (i * 8);
        }
        offset += len_size;

        int64_t run_offset = 0;
        if (off_size > 0)
        {
            for (uint8_t i = 0; i < off_size; ++i)
            {
                run_offset |= (int64_t)data[offset + i] << (i * 8);
            }
            // Sign extend if necessary
            int64_t sign_bit = (int64_t)1 << (off_size * 8 - 1);
            if (run_offset & sign_bit)
            {
                int64_t mask = -((int64_t)1 << (off_size * 8));
                run_offset |= mask;
            }
            offset += off_size;
            current_lcn += run_offset;
        }

        if (!ntfs_runlist_append(runlist, current_vcn, run_length, current_lcn))
            return false;

        current_vcn += run_length;
    }

    return runlist->count > 0;
}

static bool ntfs_read_bytes(NTFSVolume* volume, uint64_t offset, void* buffer, size_t size)
{
    if (!volume || !buffer || size == 0) return false;
    uint32_t block_size = ntfs_device_block_size(volume);
    if (block_size == 0) return false;

    uint64_t start_block = offset / block_size;
    uint64_t end_offset = offset + size;
    uint64_t end_block = (end_offset + block_size - 1) / block_size;
    uint64_t block_count = end_block - start_block;

    if (block_count == 0) block_count = 1;
    size_t temp_size = (size_t)(block_count * block_size);
    uint8_t* temp = (uint8_t*)malloc(temp_size);
    if (!temp) return false;

    if (!ntfs_read_blocks(volume, start_block, (uint32_t)block_count, temp))
    {
        free(temp);
        return false;
    }

    size_t block_offset = (size_t)(offset - start_block * block_size);
    memcpy(buffer, temp + block_offset, size);
    free(temp);
    return true;
}

static bool ntfs_read_mft_record(NTFSVolume* volume, uint64_t record_index, uint8_t* buffer)
{
    if (!volume || !buffer) return false;

    if (volume->mft_runlist.count == 0)
    {
        uint64_t offset = (volume->mft_lcn * volume->bytes_per_cluster) + record_index * volume->mft_record_size;
        if (!ntfs_read_bytes(volume, offset, buffer, volume->mft_record_size))
            return false;
    }
    else
    {
        uint64_t byte_offset = record_index * volume->mft_record_size;
        uint32_t to_read = volume->mft_record_size;
        uint8_t* dst = buffer;
        uint64_t remaining = to_read;
        uint64_t relative = byte_offset;

        for (size_t i = 0; i < volume->mft_runlist.count && remaining > 0; ++i)
        {
            NTFSDataRun* run = &volume->mft_runlist.runs[i];
            uint64_t run_bytes = run->length * volume->bytes_per_cluster;
            if (relative >= run_bytes)
            {
                relative -= run_bytes;
                continue;
            }

            uint64_t in_run_offset = relative;
            uint64_t in_run_remaining = run_bytes - in_run_offset;
            uint64_t chunk = MIN(in_run_remaining, remaining);
            uint64_t lcn_byte_offset = (uint64_t)run->lcn * volume->bytes_per_cluster + in_run_offset;
            if (!ntfs_read_bytes(volume, lcn_byte_offset, dst, (size_t)chunk))
                return false;
            dst += chunk;
            remaining -= chunk;
            relative = 0;
        }

        if (remaining > 0)
            return false;
    }

    if (!ntfs_apply_fixup(buffer, volume->mft_record_size, volume->bytes_per_sector))
        return false;

    NTFSFileRecordHeader* hdr = (NTFSFileRecordHeader*)buffer;
    if (memcmp(hdr->signature, NTFS_SIGNATURE, 4) != 0)
        return false;
    if (!(hdr->flags & NTFS_FILE_FLAG_IN_USE))
        return false;
    return true;
}

static bool ntfs_apply_fixup(uint8_t* buffer, size_t buffer_size, uint32_t bytes_per_sector)
{
    if (!buffer || buffer_size < bytes_per_sector) return false;
    NTFSFileRecordHeader* hdr = (NTFSFileRecordHeader*)buffer;
    if (hdr->fixup_entries == 0) return true;
    if ((size_t)hdr->fixup_offset + hdr->fixup_entries * sizeof(uint16_t) > buffer_size)
        return false;

    uint16_t* usa = (uint16_t*)(buffer + hdr->fixup_offset);
    uint16_t expected = usa[0];
    uint16_t sectors = hdr->fixup_entries - 1;

    for (uint16_t i = 0; i < sectors; ++i)
    {
        size_t sector_offset = (size_t)(i + 1) * bytes_per_sector - sizeof(uint16_t);
        if (sector_offset + sizeof(uint16_t) > buffer_size)
            return false;
        uint16_t* sector_tail = (uint16_t*)(buffer + sector_offset);
        if (*sector_tail != expected)
            return false;
        *sector_tail = usa[i + 1];
    }

    return true;
}

static NTFSAttributeHeader* ntfs_first_attribute(uint8_t* record)
{
    if (!record) return NULL;
    NTFSFileRecordHeader* hdr = (NTFSFileRecordHeader*)record;
    if (hdr->first_attribute_offset >= hdr->bytes_in_use)
        return NULL;
    return (NTFSAttributeHeader*)(record + hdr->first_attribute_offset);
}

static NTFSAttributeHeader* ntfs_next_attribute(NTFSAttributeHeader* attr)
{
    if (!attr) return NULL;
    if (attr->length == 0) return NULL;
    NTFSAttributeHeader* next = (NTFSAttributeHeader*)((uint8_t*)attr + attr->length);
    return next;
}

static void ntfs_destroy_volume(NTFSVolume* volume)
{
    if (!volume) return;
    if (volume->nodes)
    {
        for (ListNode* it = List_Foreach_Begin(volume->nodes); it; it = List_Foreach_Next(it))
        {
            VFSNode* node = (VFSNode*)List_Foreach_Data(it);
            ntfs_free_node(node);
        }
        List_Destroy(volume->nodes, false);
        volume->nodes = NULL;
    }
    free(volume->mft_runlist.runs);
    free(volume);
}

static void ntfs_free_node(VFSNode* node)
{
    if (!node) return;
    NTFSNodeInfo* info = (NTFSNodeInfo*)node->internal_data;
    if (info)
    {
        if (info->overlay_data)
            free(info->overlay_data);
        if (info->overlay_children)
            List_Destroy(info->overlay_children, false);
        free(info);
    }
    if (node->name) free(node->name);
    free(node);
}

static VFSNode* ntfs_alloc_node(NTFSVolume* volume,
                                VFSNode* parent,
                                const char* name,
                                bool is_directory,
                                uint64_t file_ref,
                                uint64_t file_size,
                                bool is_root)
{
    if (!volume) return NULL;

    VFSNode* node = (VFSNode*)malloc(sizeof(VFSNode));
    if (!node) return NULL;

    NTFSNodeInfo* info = (NTFSNodeInfo*)malloc(sizeof(NTFSNodeInfo));
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
    info->file_reference = file_ref;
    info->file_size = file_size;
    info->is_directory = is_directory;
    info->is_root = is_root;
    info->parent_reference = parent ? ((NTFSNodeInfo*)parent->internal_data)->file_reference : file_ref;
    info->overlay = false;
    info->overlay_data = NULL;
    info->overlay_size = 0;
    info->overlay_capacity = 0;
    info->overlay_children = NULL;

    node->name = node_name;
    node->type = is_directory ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR;
    node->flags = VFS_NODE_FLAG_READONLY;
    node->parent = parent;
    node->mount = parent ? parent->mount : NULL;
    node->ops = &s_ntfs_node_ops;
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
    return node;
}

static void ntfs_decode_utf16le(const uint16_t* in, size_t in_len, char* out, size_t out_len)
{
    if (!out || out_len == 0) return;
    size_t pos = 0;
    for (size_t i = 0; i < in_len && pos + 1 < out_len; ++i)
    {
        uint16_t ch = in[i];
        if (ch < 0x80)
        {
            out[pos++] = (char)ch;
        }
        else
        {
            // Basic transliteration
            out[pos++] = '?';
        }
    }
    out[pos] = '\0';
}

static bool ntfs_populate_node_info(NTFSVolume* volume,
                                    uint64_t file_ref,
                                    NTFSNodeInfo* info,
                                    char* out_name,
                                    size_t out_name_len)
{
    if (!volume || !info) return false;

    uint8_t* record = (uint8_t*)malloc(volume->mft_record_size);
    if (!record) return false;
    bool ok = false;

    if (!ntfs_read_mft_record(volume, ntfs_file_reference_number(file_ref), record))
        goto cleanup;

    NTFSFileRecordHeader* hdr = (NTFSFileRecordHeader*)record;
    info->is_directory = (hdr->flags & NTFS_FILE_FLAG_DIRECTORY) != 0;
    info->file_size = 0;

    if (out_name && out_name_len > 0)
        out_name[0] = '\0';

    NTFSAttributeHeader* attr = ntfs_first_attribute(record);
    while (attr && attr->type != 0xFFFFFFFF)
    {
        if (attr->type == NTFS_ATTR_FILE_NAME)
        {
            uint16_t* name_utf16 = NULL;
            size_t name_chars = 0;
            NTFSFileNameAttribute* fname = (NTFSFileNameAttribute*)((uint8_t*)attr + attr->body.resident.value_offset);
            name_utf16 = (uint16_t*)((uint8_t*)fname + sizeof(NTFSFileNameAttribute));
            name_chars = fname->name_length;
            info->parent_reference = ntfs_file_reference_number(fname->parent_directory);
            info->file_size = fname->real_size;
            if (out_name && out_name_len > 0)
            {
                ntfs_decode_utf16le(name_utf16, name_chars, out_name, out_name_len);
            }
        }
        else if (attr->type == NTFS_ATTR_DATA)
        {
            if (attr->non_resident)
            {
                info->file_size = attr->body.non_resident.data_size;
            }
            else
            {
                info->file_size = attr->body.resident.value_length;
            }
        }
        attr = ntfs_next_attribute(attr);
    }

    ok = true;

cleanup:
    free(record);
    return ok;
}

static bool ntfs_fetch_default_data_runlist(NTFSNodeInfo* info,
                                            NTFSRunlist* out_runlist,
                                            uint64_t* out_data_size,
                                            bool* out_resident,
                                            uint8_t** out_resident_value,
                                            size_t* out_resident_length)
{
    if (!info || !out_runlist || !out_data_size || !out_resident) return false;

    NTFSVolume* volume = info->volume;
    uint8_t* record = (uint8_t*)malloc(volume->mft_record_size);
    if (!record) return false;
    bool success = false;

    if (!ntfs_read_mft_record(volume, ntfs_file_reference_number(info->file_reference), record))
        goto cleanup;

    NTFSAttributeHeader* attr = ntfs_first_attribute(record);
    while (attr && attr->type != 0xFFFFFFFF)
    {
        if (attr->type == NTFS_ATTR_DATA && attr->name_length == 0)
        {
            if (attr->non_resident)
            {
                const uint8_t* run_data = (const uint8_t*)attr + attr->body.non_resident.data_run_offset;
                size_t run_len = attr->length - attr->body.non_resident.data_run_offset;
                if (!out_runlist->runs)
                {
                    if (!ntfs_runlist_init(out_runlist))
                        goto cleanup;
                }
                ntfs_runlist_reset(out_runlist);
                if (!ntfs_parse_data_runs(run_data, run_len, out_runlist))
                    goto cleanup;
                *out_data_size = attr->body.non_resident.data_size;
                *out_resident = false;
                if (out_resident_value) *out_resident_value = NULL;
                if (out_resident_length) *out_resident_length = 0;
                success = true;
                break;
            }
            else
            {
                const uint8_t* value = (const uint8_t*)attr + attr->body.resident.value_offset;
                size_t value_len = attr->body.resident.value_length;
                uint8_t* copy = NULL;
                if (value_len > 0)
                {
                    copy = (uint8_t*)malloc(value_len);
                    if (!copy)
                        goto cleanup;
                    memcpy(copy, value, value_len);
                }
                *out_data_size = value_len;
                *out_resident = true;
                if (out_resident_value) *out_resident_value = copy;
                if (out_resident_length) *out_resident_length = value_len;
                success = true;
                break;
            }
        }
        attr = ntfs_next_attribute(attr);
    }

cleanup:
    free(record);
    return success;
}

static int64_t ntfs_read_from_runlist(NTFSNodeInfo* info,
                                      NTFSRunlist* runlist,
                                      uint64_t offset,
                                      void* buffer,
                                      size_t size)
{
    if (!info || !info->volume || !runlist || runlist->count == 0 || !buffer) return -1;

    NTFSVolume* volume = info->volume;
    uint8_t* dst = (uint8_t*)buffer;
    uint64_t remaining = size;
    uint64_t relative = offset;

    for (size_t i = 0; i < runlist->count && remaining > 0; ++i)
    {
        const NTFSDataRun* run = &runlist->runs[i];
        uint64_t run_bytes = run->length * volume->bytes_per_cluster;
        if (relative >= run_bytes)
        {
            relative -= run_bytes;
            continue;
        }

        uint64_t in_run_offset = relative;
        uint64_t in_run_remaining = run_bytes - in_run_offset;
        uint64_t chunk = MIN(in_run_remaining, remaining);
        uint64_t lcn_byte_offset = (uint64_t)run->lcn * volume->bytes_per_cluster + in_run_offset;
        if (!ntfs_read_bytes(volume, lcn_byte_offset, dst, (size_t)chunk))
            return -1;
        dst += chunk;
        remaining -= chunk;
        relative = 0;
    }

    return (int64_t)(size - remaining);
}

static bool ntfs_enumerate_directory(NTFSNodeInfo* dir,
                                     size_t target_index,
                                     VFSDirEntry* out_entry,
                                     const char* find_name,
                                     uint64_t* out_child_ref)
{
    if (!dir || !dir->volume) return false;

    NTFSVolume* volume = dir->volume;
    uint8_t* record = (uint8_t*)malloc(volume->mft_record_size);
    if (!record) return false;
    bool success = false;

    if (!ntfs_read_mft_record(volume, ntfs_file_reference_number(dir->file_reference), record))
        goto cleanup;

    NTFSAttributeHeader* attr = ntfs_first_attribute(record);
    while (attr && attr->type != 0xFFFFFFFF)
    {
        if (attr->type == NTFS_ATTR_INDEX_ROOT)
        {
            uint8_t* value = attr->non_resident ? NULL : (uint8_t*)attr + attr->body.resident.value_offset;
            if (!value)
                break;

            NTFSIndexRootHeader* root = (NTFSIndexRootHeader*)value;
            (void)root;
            NTFSIndexHeader* hdr = (NTFSIndexHeader*)(value + sizeof(NTFSIndexRootHeader));
            uint8_t* entries = value + hdr->entries_offset;
            size_t offset = 0;
            size_t index = 0;

            while (offset < hdr->entries_size)
            {
                NTFSIndexEntryHeader* entry = (NTFSIndexEntryHeader*)(entries + offset);
                if (entry->entry_size < sizeof(NTFSIndexEntryHeader))
                    break;

                uint64_t file_ref = ntfs_file_reference_number(entry->file_reference);
                if (entry->stream_size >= sizeof(NTFSFileNameAttribute))
                {
                    NTFSFileNameAttribute* fname = (NTFSFileNameAttribute*)((uint8_t*)entry + sizeof(NTFSIndexEntryHeader));
                    const uint16_t* name_utf16 = (const uint16_t*)((const uint8_t*)fname + sizeof(NTFSFileNameAttribute));
                    size_t name_chars = fname->name_length;
                    char name_utf8[VFS_NAME_MAX + 1];
                    ntfs_decode_utf16le(name_utf16, name_chars, name_utf8, sizeof(name_utf8));

                    bool skip_entry = (name_utf8[0] == '\0')
                                      || (strcmp(name_utf8, ".") == 0)
                                      || (!find_name && strcmp(name_utf8, "..") == 0);

                    if (!skip_entry)
                    {
                        if (find_name)
                        {
                            if (strcmp(name_utf8, find_name) == 0)
                            {
                                if (out_child_ref) *out_child_ref = file_ref;
                                success = true;
                                goto cleanup;
                            }
                        }
                        else
                        {
                            if (index == target_index)
                            {
                                if (out_entry)
                                {
                                    memset(out_entry, 0, sizeof(VFSDirEntry));
                                    strncpy(out_entry->name, name_utf8, VFS_NAME_MAX);
                                    out_entry->name[VFS_NAME_MAX] = '\0';
                                    out_entry->type = (fname->flags & NTFS_FILE_ATTR_DIRECTORY) ? VFS_NODE_DIRECTORY : VFS_NODE_REGULAR;
                                }
                                success = true;
                                goto cleanup;
                            }
                            index++;
                        }
                    }
                }

                if (entry->flags & NTFS_INDEX_ENTRY_FLAG_LAST)
                    break;
                if (entry->entry_size == 0)
                    break;
                offset += entry->entry_size;
            }
        }
        attr = ntfs_next_attribute(attr);
    }

cleanup:
    free(record);
    return success;
}
