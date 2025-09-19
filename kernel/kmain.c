#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>
#include <graphics/gfx.h>
#include <graphics/bmp.h>
#include <irq/IRQ.h>
#include <time/timer.h>
#include <pci/PCI.h>
#include <memory/memory.h>
#include <arch.h>
#include <gfxterm/gfxterm.h>
#include <stream/OutputStream.h>
#include <graphics/screen.h>
#include <storage/BlockDevice.h>
#include <storage/Volume.h>
#include <sleep.h>
#include <stream/DiskStream.h>
#include <util/dump.h>
#include <filesystem/VFS.h>
#include <filesystem/ramfs.h>
#include <filesystem/fat/fatfs.h>
#include <filesystem/iso9660.h>
#include <filesystem/ntfs.h>
#include <util/convert.h>
#include <util/string.h>
#include <stdint.h>

void gfx_draw_task();

extern void acpi_poweroff(); // ACPI power off function
extern void acpi_restart();  // ACPI restart function

extern unsigned char logo_128x128_bmp[];
extern unsigned int logo_128x128_bmp_len;
extern void print_memory_regions(void);
extern GFXTerminal *debug_terminal;

extern void efi_reset_to_firmware();

extern void FAT_Test_Run(void);
extern void VFS_RamFSTest_Run(void);

static void log_directory_recursive(VFSNode* dir, int depth)
{
    if (!dir) return;

    char indent[64];
    int pad = depth * 2;
    if (pad >= (int)sizeof(indent)) pad = (int)sizeof(indent) - 1;
    memset(indent, ' ', (size_t)pad);
    indent[pad] = '\0';

    VFSDirEntry entry;
    for (size_t idx = 0;; ++idx)
    {
        VFSResult res = VFS_ReadDir(dir, idx, &entry);
        if (res != VFS_RES_OK)
            break;

        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0)
            continue;

        LOG("%s%s (%s)", indent, entry.name,
            entry.type == VFS_NODE_DIRECTORY ? "dir" : "file");

        if (entry.type == VFS_NODE_DIRECTORY)
        {
            VFSNode* child = NULL;
            if (VFS_ResolveAt(dir, entry.name, &child, true) == VFS_RES_OK && child)
            {
                if (depth - 1) log_directory_recursive(child, depth - 1);
            }
        }
    }
}

void kmain()
{

    VFS_Init();
    FATFS_Register();
    NTFS_Register();
    ISO9660_Register();

    VolumeManager_Init();
    VolumeManager_Rebuild();

    char mounted_path_buf[128];
    mounted_path_buf[0] = '\0';
    const char* mounted_path = NULL;
    VFSMount* active_mount = NULL;

    size_t volume_count = VolumeManager_Count();
    for (size_t i = 0; i < volume_count; ++i)
    {
        Volume* volume = VolumeManager_GetAt(i);
        if (!volume) continue;

        const char* vol_name = Volume_Name(volume);
        if (!vol_name || !vol_name[0])
            vol_name = "volume";

        char mount_path[128];
        strcpy(mount_path, "/mnt/");
        size_t base_len = strlen(mount_path);
        size_t remaining = sizeof(mount_path) - base_len - 1;
        if (remaining > 0)
        {
            size_t copy_len = strlen(vol_name);
            if (copy_len > remaining) copy_len = remaining;
            memcpy(mount_path + base_len, vol_name, copy_len);
            mount_path[base_len + copy_len] = '\0';
        }

        for (char* p = mount_path + base_len; *p; ++p)
        {
            if (*p == '/') *p = '_';
        }

        VFSMountParams params = {
            .source = Volume_Name(volume),
            .block_device = volume->device,
            .volume = volume,
            .context = NULL,
            .flags = 0,
        };

        VFSMount* mount = VFS_MountAuto(mount_path, &params);
        if (mount)
        {
            strcpy(mounted_path_buf, mount_path);
            mounted_path = mounted_path_buf;
            active_mount = mount;
            break;
        }
    }

    if (active_mount && mounted_path)
    {
        VFSNode* root_node = VFS_GetMountRoot(active_mount);
        if (root_node)
        {
            LOG("Listing contents of %s:", mounted_path);
            log_directory_recursive(root_node, 2);
        }

        char file_path[256];
        strcpy(file_path, mounted_path);
        strcat(file_path, "/hello.txt");

        VFS_HANDLE file = VFS_Open(file_path, VFS_OPEN_READ);
        if (file)
        {
            void* buffer = calloc(4096, 1);
            int64_t read_bytes = VFS_Read(file, buffer, 4096);

            LOG("%s: ", file_path);
            size_t dump_len = (read_bytes > 0)
                              ? ((size_t)read_bytes < 4096 ? (size_t)read_bytes : 4096)
                              : 0;
            dumpHex8(buffer, 8, 8, dump_len, currentOutputStream);

            VFS_Close(file);
            free(buffer);
        }
        else
        {
            WARN("Failed to open %s", file_path);
        }
    }
    else
    {
        WARN("No FAT or NTFS volume could be mounted");
    }

}
