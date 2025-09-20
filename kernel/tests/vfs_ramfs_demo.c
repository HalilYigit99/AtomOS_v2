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
static size_t s_saved_cache_capacity = 0;
static bool s_cache_capacity_saved = false;

static void log_cache_stats(const char* label)
{
    VFSCacheStats stats;
    VFS_CacheGetStats(&stats);
    LOG("cache[%s]: hits=%zu misses=%zu entries=%zu capacity=%zu",
        label ? label : "?",
        stats.hits,
        stats.misses,
        stats.entries,
        stats.capacity);
}

static void exercise_cache_demo(void)
{
    VFSCacheStats stats;
    VFS_CacheGetStats(&stats);
    if (!s_cache_capacity_saved)
    {
        s_saved_cache_capacity = stats.capacity;
        s_cache_capacity_saved = true;
    }

    VFS_CacheFlush();
    VFS_CacheResetStats();
    VFS_CacheSetCapacity(4);
    log_cache_stats("after-reset");

    VFSNode* node = NULL;
    VFSCacheStats before;
    VFS_CacheGetStats(&before);

    if (VFS_Resolve("/ramfs-demo", &node) == VFS_RES_OK && node)
    {
        LOG("cache-demo: resolved /ramfs-demo (node=%s)", VFS_NodeName(node) ? VFS_NodeName(node) : "<root>");
    }
    if (VFS_Resolve("/ramfs-demo", &node) == VFS_RES_OK && node)
    {
        LOG("cache-demo: resolved /ramfs-demo (second lookup)");
    }
    if (VFS_Resolve("/ramfs-demo/tmp", &node) == VFS_RES_OK && node)
    {
        LOG("cache-demo: resolved /ramfs-demo/tmp");
    }

    VFSCacheStats after;
    VFS_CacheGetStats(&after);
    size_t hits_delta = (after.hits >= before.hits) ? (after.hits - before.hits) : 0;
    size_t miss_delta = (after.misses >= before.misses) ? (after.misses - before.misses) : 0;
    LOG("cache lookups: +hits=%zu +misses=%zu", hits_delta, miss_delta);
    log_cache_stats("after-lookups");

    VFS_CacheFlush();
    log_cache_stats("after-flush");

    VFS_CacheSetCapacity(0);
    VFS_CacheResetStats();
    node = NULL;
    VFS_Resolve("/ramfs-demo", &node);
    VFS_Resolve("/ramfs-demo", &node);
    log_cache_stats("disabled");

    if (s_cache_capacity_saved)
    {
        VFS_CacheSetCapacity(s_saved_cache_capacity);
    }
    VFS_CacheFlush();
    VFS_CacheResetStats();
}

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

    exercise_cache_demo();

    log_result("remove /ramfs-demo/tmp/info.txt", VFS_Remove("/ramfs-demo/tmp/info.txt"));
    log_result("remove /ramfs-demo/tmp", VFS_Remove("/ramfs-demo/tmp"));

    list_directory("/ramfs-demo");
}
