#include <filesystem/VFS.h>
#include <filesystem/ramfs.h>
#include <debug/debug.h>
#include <util/string.h>

static void log_result(const char *label, VFSResult res)
{
    if (res == VFS_RES_OK)
    {
        LOG("%s: OK", label);
    }
    else
    {
        WARN("%s: err=%d", label, res);
    }
}

static void list_directory(const char *path)
{
    VFSNode *dir = NULL;
    VFSResult res = VFS_Resolve(path, &dir);
    if (res != VFS_RES_OK)
    {
        WARN("list_directory('%s'): resolve failed (%d)", path, res);
        return;
    }

    LOG("Directory listing for %s", path);
    size_t index = 0;
    VFSDirEntry entry;
    while (1)
    {
        res = VFS_ReadDir(dir, index, &entry);
        if (res != VFS_RES_OK)
            break;
        const char *type = "?";
        switch (entry.type)
        {
        case VFS_NODE_DIRECTORY:
            type = "dir";
            break;
        case VFS_NODE_REGULAR:
            type = "file";
            break;
        default:
            break;
        }
        LOG("  [%zu] %s (%s)", index, entry.name, type);
        index++;
    }
}

static bool initialized = false;

void VFS_RamFSTest_Run(void)
{
    if (!initialized)
    {
        if (!VFS_IsInitialized())
        {
            VFS_Init();
        }

        VFSFileSystem *ramfs = RamFS_Create("ramfs-demo");
        if (!ramfs)
        {
            ERROR("ramfs demo: create failed");
            return;
        }

        VFSResult res = VFS_RegisterFileSystem(ramfs);
        if (res != VFS_RES_OK && res != VFS_RES_EXISTS)
        {
            ERROR("ramfs demo: register failed (%d)", res);
            RamFS_Destroy(ramfs);
            return;
        }

        VFSMount *mount = VFS_Mount("/ramfs-demo", ramfs, NULL);
        if (!mount)
        {
            ERROR("ramfs demo: mount failed");
            return;
        }
        initialized = true;
    }

    log_result("mkdir /ramfs-demo/tmp", VFS_Create("/ramfs-demo/tmp", VFS_NODE_DIRECTORY));
    log_result("touch /ramfs-demo/tmp/info.txt", VFS_Create("/ramfs-demo/tmp/info.txt", VFS_NODE_REGULAR));

    const char *text = "RamFS VFS example\n";
    VFS_HANDLE handle = VFS_Open("/ramfs-demo/tmp/info.txt", VFS_OPEN_READ | VFS_OPEN_WRITE | VFS_OPEN_TRUNC);
    if (handle)
    {
        VFS_Write(handle, text, strlen(text));
        VFS_SeekHandle(handle, 0, VFS_SEEK_SET, NULL);

        char buffer[64];
        int64_t read = VFS_Read(handle, buffer, sizeof(buffer) - 1);
        if (read > 0)
        {
            buffer[read] = '\0';
            LOG("file contents: %s", buffer);
        }
        VFS_Close(handle);
    }
    else
    {
        WARN("ramfs demo: open failed");
    }

    list_directory("/ramfs-demo");
    list_directory("/ramfs-demo/tmp");

    log_result("remove /ramfs-demo/tmp/info.txt", VFS_Remove("/ramfs-demo/tmp/info.txt"));
    log_result("remove /ramfs-demo/tmp", VFS_Remove("/ramfs-demo/tmp"));

    list_directory("/ramfs-demo");
}
