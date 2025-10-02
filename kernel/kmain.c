// #include <debug/debug.h>
// #include <boot/multiboot2.h>
// #include <keyboard/Keyboard.h>
// #include <mouse/mouse.h>
// #include <graphics/gfx.h>
// #include <graphics/bmp.h>
// #include <irq/IRQ.h>
// #include <time/timer.h>
// #include <pci/PCI.h>
// #include <memory/memory.h>
// #include <arch.h>
// #include <gfxterm/gfxterm.h>
// #include <stream/OutputStream.h>
// #include <graphics/screen.h>
// #include <storage/BlockDevice.h>
// #include <storage/Volume.h>
// #include <sleep.h>
// #include <stream/DiskStream.h>
// #include <util/dump.h>
// #include <filesystem/VFS.h>
// #include <filesystem/ramfs.h>
// #include <filesystem/fat/fatfs.h>
// #include <filesystem/iso9660.h>
// #include <filesystem/ntfs.h>
// #include <util/convert.h>
// #include <util/string.h>
// #include <stream/FileStream.h>
// #include <task/PeriodicTask.h>
// #include <stdint.h>
// #include <efi/efi.h>

// void gfx_draw_task();

// extern PeriodicTask *gfx_task;

// extern void acpi_poweroff(); // ACPI power off function
// extern void acpi_restart();  // ACPI restart function

// extern unsigned char logo_128x128_bmp[];
// extern unsigned int logo_128x128_bmp_len;
// extern void print_memory_regions(void);
// extern GFXTerminal *debug_terminal;

// extern void efi_reset_to_firmware();

// extern void FAT_Test_Run(void);
// extern void VFS_RamFSTest_Run(void);

// static void logDirectoryContents(char *path)
// {
//     List *rootDirContents = VFS_GetDirectoryContents(path);
//     if (rootDirContents)
//     {
//         LOG("'%s' contents:", path);
//         for (ListNode *node = rootDirContents->head; node != NULL; node = node->next)
//         {
//             char *name = (char *)node->data;
//             LOG(" - %s", name);
//         }
//     }
//     else
//     {
//         LOG("Failed to get '%s' contents", path);
//     }
// }

// void kmain(void)
// {
//     LOG("AtomOS Kernel Main Function Started");

//     // List '/' directory
//     logDirectoryContents("/");

//     // List '/mnt' directory
//     logDirectoryContents("/mnt");

//     // List '/mnt/sd0' directory
//     logDirectoryContents("/mnt/cd0");

//     if (VFS_DirectoryExists("/mnt/sd0"))
//     {
//         // List '/mnt/sd0' directory
//         logDirectoryContents("/mnt/sd0");

//         VFSResult result = VFS_FileExists("/mnt/sd0/hello.txt") == true ? VFS_RES_EXISTS : VFS_RES_NOT_FOUND;

//         if (result == VFS_RES_NOT_FOUND)
//         {
//             LOG("File does not exist, creating...");

//             result = VFS_Create("/mnt/sd0/hello.txt", VFS_NODE_REGULAR);

//             LOG("VFS_Create returned : %i", (int)result);
//         }

//         if (result == VFS_RES_OK || result == VFS_RES_EXISTS)
//         {

//             LOG("File exists!");

//             FileStream *file = VFS_OpenFileStream("/mnt/sd0/hello.txt", VFS_OPEN_TRUNC);

//             FileStream_Write(file, "Hello from AtomOS!\nThis is a test file.\n", 41);

//             char content[256] = {0};

//             FileStream_Read(file, content, 256);

//             LOG("File content:\n%s", content);

//             FileStream_Close(file);
//         }
//     }

//     // List new contents of '/mnt/sd0' directory
//     logDirectoryContents("/mnt/sd0");

//     // Open test file
//     FileStream *file = VFS_OpenFileStream("/mnt/cd0/hello.txt", 0);
//     if (file)
//     {
//         LOG("Opened /mnt/cd0/hello.txt");
//     }
//     else
//     {
//         LOG("Failed to open /mnt/cd0/hello.txt");
//     }

//     // Read test file
//     if (file)
//     {
//         char buffer[128];
//         int bytesRead = FileStream_Read(file, buffer, sizeof(buffer) - 1);
//         if (bytesRead > 0)
//         {
//             buffer[bytesRead] = '\0'; // Null-terminate the string
//             LOG("Read %d bytes from /mnt/cd0/hello.txt:", bytesRead);
//             LOG("%s", buffer);
//         }
//         else
//         {
//             LOG("Failed to read from /mnt/cd0/hello.txt");
//         }
//         FileStream_Close(file);
//     }

//     LOG("AtomOS Kernel Main Function Completed");

//     gfx_draw_task();

//     periodic_task_stop(gfx_task);

//     EFI_SYSTEM_TABLE *efi_system_table = multiboot2_get_efi_system_table();

//     // EFI ile boot edilmişse (efi_system_table geçerli) önce EFI konsoluna mesaj yaz, 5sn bekle sonra yeniden başlat
//     if (efi_system_table && efi_system_table->con_out && efi_system_table->con_out->output_string)
//     {
//         // Renk: Sarı yazı, siyah arka plan
//         if (efi_system_table->con_out->set_attribute)
//         {
//             efi_system_table->con_out->set_attribute(efi_system_table->con_out, EFI_TEXT_ATTR(EFI_YELLOW, EFI_BLACK));
//         }

//         // Mesajı wide char formuna çevir
//         static uint16_t msg_w[128];
//         const char *msg = "[EFI] Sistem 5 saniye içinde ACPI restart ile yeniden başlatılacak...\r\n";
//         // Yardımcı dönüştürücü; EFI_IMPLEMENTATION tanımlı değilse basit döngü kullan
// #ifdef EFI_IMPLEMENTATION
//         efi_str_to_wstr(msg_w, msg, 128);
// #else
//         {
//             size_t i = 0;
//             for (; msg[i] && i < 127; ++i)
//                 msg_w[i] = (uint16_t)msg[i];
//             msg_w[i] = 0;
//         }
// #endif
//         efi_system_table->con_out->output_string(efi_system_table->con_out, msg_w);
//     }

//     // 5000 ms bekle
//     sleep_ms(5000);

//     acpi_restart();
// }
