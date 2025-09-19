#include <debug/debug.h>
#include <filesystem/VFS.h>
#include <filesystem/fat/fatfs.h>
#include <filesystem/ntfs.h>
#include <filesystem/iso9660.h>
#include <storage/Volume.h>
#include <util/string.h>
#include <util/convert.h>

static void list_directory(VFSNode* dir, const char* label, size_t max_entries)
{
    if (!dir)
    {
        WARN("list_directory: directory node is NULL");
        return;
    }

    LOG("Listing %s", label);

    VFSDirEntry entry;
    for (size_t idx = 0; idx < max_entries; ++idx)
    {
        VFSResult res = VFS_ReadDir(dir, idx, &entry);
        if (res != VFS_RES_OK)
        {
            if (idx == 0)
            {
                LOG("  (empty or unreadable, res=%d)", res);
            }
            break;
        }
        LOG("  [%zu] %s (%s)", idx, entry.name,
            entry.type == VFS_NODE_DIRECTORY ? "dir" : "file");
    }
}

static void try_create_sample_file(const char* path)
{
    VFSResult res = VFS_Create(path, VFS_NODE_REGULAR);
    if (res == VFS_RES_OK)
    {
        LOG("Created file %s", path);
    }
    else
    {
        WARN("Create %s failed (res=%d) — current FAT driver is read-only", path, res);
    }
}

void FAT_Test_Run(void)
{
    if (!VFS_IsInitialized())
    {
        VFS_Init();
    }

    FATFS_Register();
    NTFS_Register();
    ISO9660_Register();

    VolumeManager_Init();
    VolumeManager_Rebuild();

    size_t vol_count = VolumeManager_Count();
    if (vol_count == 0)
    {
        WARN("FAT test: no volumes available");
        return;
    }

    VFSResult root_res = VFS_Create("/mnt", VFS_NODE_DIRECTORY);
    if (root_res != VFS_RES_OK && root_res != VFS_RES_EXISTS)
    {
        WARN("FAT test: unable to create /mnt (res=%d)", root_res);
        return;
    }

    for (size_t i = 0; i < vol_count; ++i)
    {
        Volume* volume = VolumeManager_GetAt(i);
        if (!volume)
        {
            WARN("FAT test: VolumeManager_GetAt(%zu) returned NULL", i);
            continue;
        }

        char mount_path[32];
        strcpy(mount_path, "/mnt/fat");
        char idxbuf[16];
        utoa((unsigned)i, idxbuf, 10);
        strcat(mount_path, idxbuf);
        VFSResult mnt_res = VFS_Create(mount_path, VFS_NODE_DIRECTORY);
        if (mnt_res != VFS_RES_OK && mnt_res != VFS_RES_EXISTS)
        {
            WARN("FAT test: create mount point %s failed (res=%d)", mount_path, mnt_res);
            continue;
        }

        VFSMountParams params = {
            .source = Volume_Name(volume),
            .block_device = volume->device,
            .volume = volume,
            .context = NULL,
            .flags = 0,
        };

        VFSMount* mount = VFS_MountAuto(mount_path, &params);
        if (!mount)
        {
            LOG("FAT test: volume %s did not match any known filesystem",
                Volume_Name(volume) ? Volume_Name(volume) : "<noname>");
            continue;
        }

        LOG("FAT test: mounted %s at %s",
            Volume_Name(volume) ? Volume_Name(volume) : "<noname>",
            mount_path);

        VFSNode* root = VFS_GetMountRoot(mount);
        list_directory(root, mount_path, 16);

        char sample_path[64];
        strcpy(sample_path, mount_path);
        strcat(sample_path, "/test.txt");
        try_create_sample_file(sample_path);

        VFS_HANDLE handle = VFS_Open(sample_path, VFS_OPEN_READ);
        if (handle)
        {
            char buffer[128];
            int64_t read = VFS_Read(handle, buffer, sizeof(buffer) - 1);
            if (read > 0)
            {
                buffer[read] = '\0';
                LOG("FAT test: read %lld bytes from %s", (long long)read, sample_path);
            }
            else
            {
                WARN("FAT test: read from %s failed (rc=%lld)", sample_path, (long long)read);
            }
            VFS_Close(handle);
        }
        else
        {
            LOG("FAT test: open %s skipped (likely not created)", sample_path);
        }
    }
}
