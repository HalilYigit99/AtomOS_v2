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
#include <stream/FileStream.h>
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

static void logDirectoryContents(char* path)
{
    List* rootDirContents = VFS_GetDirectoryContents(path);
    if (rootDirContents)
    {
        LOG("'%s' contents:", path);
        for (ListNode* node = rootDirContents->head; node != NULL; node = node->next)
        {
            char* name = (char*)node->data;
            LOG(" - %s", name);
        }
    }
    else
    {
        LOG("Failed to get '%s' contents", path);
    }
}

void kmain()
{
    LOG("AtomOS Kernel Main Function Started");

    // List '/' directory
    logDirectoryContents("/");

    // List '/mnt' directory
    logDirectoryContents("/mnt");

    // List '/mnt/sd0' directory
    logDirectoryContents("/mnt/cd0");

    if (VFS_DirectoryExists("/mnt/sd0"))
    {
        // List '/mnt/sd0' directory
        logDirectoryContents("/mnt/sd0");

        VFSResult result = VFS_FileExists("/mnt/sd0/hello.txt") == true ? VFS_RES_EXISTS : VFS_RES_NOT_FOUND;

        if (result == VFS_RES_NOT_FOUND)
        {
            LOG("File does not exist, creating...");

            result = VFS_Create("/mnt/sd0/hello.txt", VFS_NODE_REGULAR);

            LOG("VFS_Create returned : %i", (int)result);
        }

        if (result == VFS_RES_OK || result == VFS_RES_EXISTS)
        {

            LOG("File exists!");

            FileStream* file = VFS_OpenFileStream("/mnt/sd0/hello.txt", VFS_OPEN_TRUNC);

            FileStream_Write(file, "Hello from AtomOS!\nThis is a test file.\n", 41);

            char content[256] = {0};

            FileStream_Read(file, content, 256);

            LOG("File content:\n%s", content);

            FileStream_Close(file);

        }

    }

    // List new contents of '/mnt/sd0' directory
    logDirectoryContents("/mnt/sd0");

    // Open test file
    FileStream* file = VFS_OpenFileStream("/mnt/cd0/hello.txt", 0);
    if (file)
    {
        LOG("Opened /mnt/cd0/hello.txt");
    }else 
    {
        LOG("Failed to open /mnt/cd0/hello.txt");
    }

    // Read test file
    if (file)
    {
        char buffer[128];
        int bytesRead = FileStream_Read(file, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0)
        {
            buffer[bytesRead] = '\0'; // Null-terminate the string
            LOG("Read %d bytes from /mnt/cd0/hello.txt:", bytesRead);
            LOG("%s", buffer);
        }
        else
        {
            LOG("Failed to read from /mnt/cd0/hello.txt");
        }
    }

    LOG("BIOS interrupt test:");
    arch_processor_regs_t regs_in = {0};
    arch_processor_regs_t regs_out = {0};
    arch_bios_int(0x10, &regs_in, &regs_out);

    LOG("AtomOS Kernel Main Function Completed");

}