#include <stdint.h>
#include <stddef.h>
#include <debug/debug.h>
#include <boot/multiboot2.h>
#include <arch.h>
#include <acpi/acpi.h>
#include <driver/DriverBase.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>
#include <driver/apic/apic.h>
#include <memory/pmm.h>
#include <time/timer.h>
#include <stream/OutputStream.h>
#include <task/PeriodicTask.h>
#include <graphics/gfx.h>
#include <list.h>
#include <graphics/screen.h>
#include <gfxterm/gfxterm.h>
#include <sleep.h>
#include <filesystem/VFS.h>
#include <filesystem/ramfs.h>
#include <filesystem/fat/fatfs.h>
#include <filesystem/ntfs.h>
#include <filesystem/iso9660.h>
#include <storage/BlockDevice.h>
#include <storage/Volume.h>
#include <util/string.h>
#include <util/convert.h>

extern DriverBase pic8259_driver;
extern DriverBase ps2kbd_driver;
extern DriverBase ps2mouse_driver;
extern DriverBase pit_driver;
extern DriverBase apic_driver;
extern DriverBase ahci_driver;
extern DriverBase ata_driver;
extern DriverBase hpet_driver;
extern bool hpet_supported();

extern uint32_t mb2_signature;
extern uint32_t mb2_tagptr;

extern DebugStream genericDebugStream;
extern DebugStream dbgGFXTerm;
extern DebugStream uartDebugStream;

extern OutputStream dbgGFXTermStream;
extern OutputStream genericOutputStream;
extern OutputStream uartOutputStream;

extern GFXTerminal *debug_terminal;

extern void multiboot2_parse();
extern void pmm_init();

extern void efi_init();
extern void bios_init();

extern void heap_init();
extern void gfx_init();
extern void acpi_sci_init();
extern void efi_exit_boot_services();

extern bool apic_supported();

extern void i386_processor_exceptions_init();
extern void i386_tss_install(void);
extern void print_memory_regions();

extern void gfx_draw_task();

extern void screen_init();

PeriodicTask* gfx_task = NULL;

static bool ensure_directory(const char* path)
{
    VFSResult res = VFS_Create(path, VFS_NODE_DIRECTORY);
    if (res == VFS_RES_OK || res == VFS_RES_EXISTS)
        return true;

    WARN("boot: ensure_directory('%s') failed (res=%d)", path, res);
    return false;
}

static void mount_block_devices(void)
{
    ensure_directory("/dev");

    size_t blk_count = BlockDevice_Count();
    for (size_t i = 0; i < blk_count; ++i)
    {
        BlockDevice* device = BlockDevice_GetAt(i);
        if (!device)
            continue;

        char mount_path[32];
        strcpy(mount_path, "/dev/blk");
        char idx_buf[16];
        utoa((unsigned)i, idx_buf, 10);
        strcat(mount_path, idx_buf);

        if (!ensure_directory(mount_path))
            continue;

        VFSMountParams params = {
            .source = device->name,
            .block_device = device,
            .volume = NULL,
            .context = NULL,
            .flags = 0,
        };

        VFSMount* mount = VFS_MountAuto(mount_path, &params);
        if (mount)
        {
            LOG("boot: mounted block device %s at %s",
                device->name ? device->name : "<noname>",
                mount_path);
        }
        else
        {
            WARN("boot: no filesystem detected on block device %s (mount %s)",
                 device->name ? device->name : "<noname>",
                 mount_path);
        }
    }
}

static void mount_volumes(void)
{
    size_t disk_index = 0;
    size_t cd_index = 0;

    size_t volume_count = VolumeManager_Count();
    for (size_t i = 0; i < volume_count; ++i)
    {
        Volume* volume = VolumeManager_GetAt(i);
        if (!volume)
            continue;

        const bool is_cd = volume->device && volume->device->type == BLKDEV_TYPE_CDROM;
        char mount_path[32];
        if (is_cd)
        {
            strcpy(mount_path, "/mnt/cd");
            char idx_buf[16];
            utoa((unsigned)cd_index++, idx_buf, 10);
            strcat(mount_path, idx_buf);
        }
        else
        {
            strcpy(mount_path, "/mnt/sd");
            char idx_buf[16];
            utoa((unsigned)disk_index++, idx_buf, 10);
            strcat(mount_path, idx_buf);
        }

        if (!ensure_directory(mount_path))
            continue;

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
            LOG("boot: mounted volume %s at %s",
                Volume_Name(volume) ? Volume_Name(volume) : "<unnamed>",
                mount_path);
        }
        else
        {
            WARN("boot: no filesystem matched volume %s (mount %s)",
                 Volume_Name(volume) ? Volume_Name(volume) : "<unnamed>",
                 mount_path);
        }
    }
}

void uptime_counter_task()
{
    uptimeMs++;
    periodic_task_run_all();
}

void __boot_kernel_start(void)
{

    debugStream->Open();

    i386_processor_exceptions_init();

    LOG("Booting AtomOS Kernel");

    LOG("Multiboot2 Signature: 0x%08X", mb2_signature);
    LOG("Multiboot2 Tag Pointer: 0x%08X", mb2_tagptr);

    multiboot2_parse();

    if (mb2_framebuffer)
    {
        // Fill framebuffer with white
        memset((void*)(uintptr_t)mb2_framebuffer->framebuffer_addr, 0xFF, mb2_framebuffer->framebuffer_pitch * mb2_framebuffer->framebuffer_height);
    }

    heap_init(); // Initialize local heap

    if (mb2_is_efi_boot)
    {
        efi_init();
    }
    else
    {
        bios_init();
    }

    pmm_init(); // Initialize physical memory manager

    screen_init();

    /* ACPI tablolarını multiboot üzerinden başlat */
    acpi_init();

    // Don't exit boot services yet; some modules may need it ( ChangeVideoMode function, AHCI driver, etc.)
    // if (mb2_is_efi_boot)
    // {
    //     LOG("Exiting boot services");
    //     efi_exit_boot_services();
    // }

    void *table = pmm_alloc(1); // Kernel için ilk sayfa tablosu

    if (table)
        LOG("Initial page table allocated at %p", table);
    else
        ERROR("Failed to allocate initial page table");

    print_memory_regions();

    // APIC varsa onu kullan, yoksa PIC'e düş
    if (apic_supported())
    {
        LOG("Using APIC interrupt controller");
        system_driver_register(&apic_driver);
        system_driver_enable(&apic_driver);
    }
    else
    {
        LOG("Using PIC8259 interrupt controller");
        system_driver_register(&pic8259_driver);
        system_driver_enable(&pic8259_driver);
    }

    if (hpet_supported()) {
        LOG("HPET supported – using HPET for system tick");
        system_driver_register(&hpet_driver);
        system_driver_enable(&hpet_driver);
    }

    LOG("HPET not available – falling back to PIT");
    system_driver_register(&pit_driver);
    system_driver_enable(&pit_driver);
    irq_controller->acknowledge(0); // Acknowledge IRQ0 (PIT)

    asm volatile ("sti"); // Enable interrupts

    // Hook uptime tick to the active hardware timer
    pit_timer->setFrequency(1000);
    pit_timer->add_callback(uptime_counter_task);

    gfx_init();

    gfx_task = periodic_task_create("GFX Task", gfx_draw_task, NULL, 16); // Yaklaşık 60 FPS
    if (!gfx_task)
    {
        ERROR("Failed to create GFX task");
    }
    periodic_task_start(gfx_task);

    currentOutputStream = &genericOutputStream;

    gos_addStream(&uartOutputStream);
    gos_addStream(&dbgGFXTermStream);

    currentOutputStream->Open();

    debugStream = &genericDebugStream;

    gds_addStream(&dbgGFXTerm);
    gds_addStream(&uartDebugStream);

    debugStream->Open();

    // Enable HID drivers
    LOG("Loading HID drivers...");

    gfx_draw_task(); // İlk çizimi yap

    system_driver_register(&ps2kbd_driver);
    system_driver_register(&ps2mouse_driver);

    gfx_draw_task(); // İlk çizimi yap

    system_driver_enable(&ps2kbd_driver);
    system_driver_enable(&ps2mouse_driver);

    gfx_draw_task(); // İlk çizimi yap

    mouse_enabled = true;

    // Storage drivers (AHCI first, then legacy ATA/PATA)
    LOG("Loading storage drivers...");
    system_driver_register(&ahci_driver);
    system_driver_enable(&ahci_driver);

    system_driver_register(&ata_driver);
    system_driver_enable(&ata_driver);

    ScreenVideoModeInfo* best = List_GetAt(main_screen.video_modes, 0);
    LOG("Current video mode: %ux%u, %u bpp", main_screen.mode->width, main_screen.mode->height, main_screen.mode->bpp);

    for (ListNode* node = main_screen.video_modes->head; node != NULL; node = node->next)
    {
        ScreenVideoModeInfo* mode = (ScreenVideoModeInfo*)node->data;
        if (mode->width >= 1920 || mode->height >= 1080)
            continue; // Skip modes larger than 1080p
        if (mode->width >= best->width || mode->height >= best->height)
        {
            best = mode;
        }
        if (mode->width > best->width && mode->height > best->height)
        {
            best = mode;
        }
    }

    LOG("Changing video mode...");
    LOG("Best mode found: %ux%u, %u bpp", best->width, best->height, best->bpp);

    screen_changeVideoMode(&main_screen, best);
    gfxterm_resize(debug_terminal, (gfx_size){best->width / debug_terminal->font->size.width, best->height / debug_terminal->font->size.height});
    gfxterm_clear(debug_terminal);

    LOG("Selected video mode: %ux%u, %u bpp", best->width, best->height, best->bpp);

    VFS_Init();

    VFSFileSystem* rootfs = VFS_GetFileSystem("rootfs");
    if (!rootfs)
    {
        VFSFileSystem* new_rootfs = RamFS_Create("rootfs");
        if (!new_rootfs)
        {
            ERROR("boot: failed to allocate root RAMFS filesystem");
        }
        else
        {
            VFSResult reg_res = VFS_RegisterFileSystem(new_rootfs);
            if (reg_res == VFS_RES_OK)
            {
                rootfs = new_rootfs;
            }
            else if (reg_res == VFS_RES_EXISTS)
            {
                rootfs = VFS_GetFileSystem("rootfs");
                RamFS_Destroy(new_rootfs);
            }
            else
            {
                ERROR("boot: failed to register RAMFS (res=%d)", reg_res);
                RamFS_Destroy(new_rootfs);
            }
        }
    }

    if (rootfs && !VFS_GetMount("/"))
    {
        VFSMount* root_mount = VFS_Mount("/", rootfs, NULL);
        if (!root_mount)
        {
            ERROR("boot: failed to mount RAMFS at /");
        }
        else
        {
            LOG("boot: root filesystem mounted on RAMFS");
        }
    }

    bool root_ready = VFS_GetMount("/") != NULL;
    if (!root_ready)
    {
        WARN("boot: root filesystem not ready; skipping device mounts");
    }

    FATFS_Register();
    NTFS_Register();
    ISO9660_Register();

    VolumeManager_Init();
    VolumeManager_Rebuild();

    if (root_ready && ensure_directory("/mnt"))
    {
        mount_block_devices();
        mount_volumes();
    }

}
