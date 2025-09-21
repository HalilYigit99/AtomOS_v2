#include <task/task.h>
#include <task/context.h>
#include <list.h>
#include <memory/memory.h>
#include <debug/debug.h>
#include <time/timer.h>
#include <util/string.h>

struct TaskProcess {
    uint64_t pid;
    TaskProcessType type;
    char name[TASK_NAME_MAX_LENGTH];
    List* threads;
};

struct TaskThread {
    uint64_t tid;
    TaskThreadType type;
    TaskThreadState state;
    char name[TASK_NAME_MAX_LENGTH];
    TaskProcess* process;
    TaskContext context;
    void (*entry)(void*);
    void* entry_arg;
    uint8_t* kernel_stack;
    size_t kernel_stack_size;
    uintptr_t kernel_stack_top;
    void* user_stack;
    size_t user_stack_size;
    uintptr_t wake_deadline;
    int exit_status;
    bool managed_allocation;
    bool is_idle;
    bool is_bootstrap;
};

static TaskProcess* g_kernel_process = NULL;
static TaskThread* g_idle_thread = NULL;
static TaskThread* g_current_thread = NULL;
static TaskThread g_bootstrap_thread = {0};
static List* g_processes = NULL;
static List* g_all_threads = NULL;
static List* g_ready_queue = NULL;
static List* g_sleep_queue = NULL;
static List* g_zombie_queue = NULL;
static uint64_t g_next_pid = 1;
static uint64_t g_next_tid = 1;
static bool g_scheduler_active = false;

static uint32_t g_scheduler_lock_depth = 0;
static bool g_scheduler_saved_if = false;

static inline void scheduler_lock(void)
{
    unsigned long flags;
    __asm__ __volatile__("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    if (g_scheduler_lock_depth++ == 0) {
        g_scheduler_saved_if = (flags & (1 << 9)) != 0;
    }
}

static inline void scheduler_unlock(void)
{
    if (g_scheduler_lock_depth == 0) {
        return;
    }

    g_scheduler_lock_depth--;
    if (g_scheduler_lock_depth == 0 && g_scheduler_saved_if) {
        __asm__ __volatile__("sti" ::: "memory");
    }
}

static void enqueue_ready_locked(TaskThread* thread)
{
    if (!thread || thread->is_idle) {
        return;
    }

    if (!g_ready_queue) {
        g_ready_queue = List_Create();
    }

    if (List_IndexOf(g_ready_queue, thread) >= 0) {
        return;
    }

    List_Add(g_ready_queue, thread);
    thread->state = TASK_THREAD_STATE_READY;
}

static TaskThread* dequeue_ready_locked(void)
{
    if (!g_ready_queue || List_IsEmpty(g_ready_queue)) {
        return NULL;
    }

    TaskThread* thread = (TaskThread*)List_GetAt(g_ready_queue, 0);
    List_RemoveAt(g_ready_queue, 0);
    return thread;
}

static void reap_zombies_locked(void)
{
    if (!g_zombie_queue) {
        return;
    }

    while (!List_IsEmpty(g_zombie_queue)) {
        TaskThread* thread = (TaskThread*)List_GetAt(g_zombie_queue, 0);
        List_RemoveAt(g_zombie_queue, 0);
        if (!thread) {
            continue;
        }

        if (thread->process && thread->process->threads) {
            List_Remove(thread->process->threads, thread);
        }

        if (g_all_threads) {
            List_Remove(g_all_threads, thread);
        }

        if (thread->kernel_stack) {
            free(thread->kernel_stack);
            thread->kernel_stack = NULL;
        }

        if (thread->user_stack) {
            free(thread->user_stack);
            thread->user_stack = NULL;
        }

        if (thread->managed_allocation) {
            free(thread);
        }
    }
}

static void wake_sleepers_locked(void)
{
    if (!g_sleep_queue || List_IsEmpty(g_sleep_queue)) {
        return;
    }

    ListNode* node = g_sleep_queue->head;
    while (node) {
        TaskThread* thread = (TaskThread*)node->data;
        ListNode* next = node->next;
        if (thread && thread->state == TASK_THREAD_STATE_SLEEPING && thread->wake_deadline <= uptimeMs) {
            List_Remove(g_sleep_queue, thread);
            enqueue_ready_locked(thread);
        }
        node = next;
    }
}

static TaskThread* pick_next_thread_locked(void)
{
    wake_sleepers_locked();
    reap_zombies_locked();

    TaskThread* next = dequeue_ready_locked();
    if (!next) {
        return g_idle_thread;
    }
    return next;
}

static void context_switch_locked(TaskThread* next, bool requeue_current)
{
    if (!next) {
        return;
    }

    TaskThread* previous = g_current_thread;

    if (previous == next) {
        if (previous) {
            previous->state = TASK_THREAD_STATE_RUNNING;
        }
        return;
    }

    if (previous && requeue_current && !previous->is_idle && previous->state == TASK_THREAD_STATE_RUNNING) {
        enqueue_ready_locked(previous);
    }

    if (previous && previous->state == TASK_THREAD_STATE_RUNNING && !requeue_current) {
        previous->state = TASK_THREAD_STATE_BLOCKED;
    }

    g_current_thread = next;
    next->state = TASK_THREAD_STATE_RUNNING;

    if (next->kernel_stack_top != 0) {
        // no-op placeholder for future TSS integration
    }

    TaskContext* prev_context = previous ? &previous->context : NULL;
    TaskContext* next_context = &next->context;

    if (!prev_context) {
        TaskContext dummy = {0};
        arch_task_context_switch(&dummy, next_context);
        return;
    }

    arch_task_context_switch(prev_context, next_context);
}

static void schedule_locked(bool requeue_current)
{
    TaskThread* next = pick_next_thread_locked();
    context_switch_locked(next, requeue_current);
}

static void kernel_thread_trampoline(void)
{
    TaskThread* self = g_current_thread;
    if (!self || !self->entry) {
        ERROR("task: invalid thread trampoline context");
        task_exit(-1);
    }

    self->entry(self->entry_arg);
    task_exit(0);
}

static void idle_thread_entry(void* arg)
{
    (void)arg;
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

static TaskProcess* process_allocate(const char* name, TaskProcessType type)
{
    TaskProcess* process = (TaskProcess*)malloc(sizeof(TaskProcess));
    if (!process) {
        return NULL;
    }

    memset(process, 0, sizeof(TaskProcess));
    process->pid = g_next_pid++;
    process->type = type;
    process->threads = List_Create();
    if (!process->threads) {
        free(process);
        return NULL;
    }

    if (name) {
        strncpy(process->name, name, TASK_NAME_MAX_LENGTH - 1);
        process->name[TASK_NAME_MAX_LENGTH - 1] = '\0';
    } else {
        strcpy(process->name, "process");
    }

    if (!g_processes) {
        g_processes = List_Create();
    }

    if (g_processes) {
        List_Add(g_processes, process);
    }

    return process;
}

static TaskThread* thread_allocate(TaskProcess* process,
                                   const char* name,
                                   TaskThreadType type,
                                   void (*entry)(void*),
                                   void* arg,
                                   size_t stack_size,
                                   bool managed)
{
    TaskThread* thread = (TaskThread*)malloc(sizeof(TaskThread));
    if (!thread) {
        return NULL;
    }

    memset(thread, 0, sizeof(TaskThread));
    thread->tid = g_next_tid++;
    thread->type = type;
    thread->state = TASK_THREAD_STATE_INIT;
    thread->process = process;
    thread->entry = entry;
    thread->entry_arg = arg;
    thread->managed_allocation = managed;

    if (name) {
        strncpy(thread->name, name, TASK_NAME_MAX_LENGTH - 1);
        thread->name[TASK_NAME_MAX_LENGTH - 1] = '\0';
    } else {
        strcpy(thread->name, "thread");
    }

    thread->kernel_stack_size = stack_size ? stack_size : TASK_DEFAULT_KERNEL_STACK;
    thread->kernel_stack = (uint8_t*)malloc(thread->kernel_stack_size);
    if (!thread->kernel_stack) {
        free(thread);
        return NULL;
    }

    uintptr_t stack_top = (uintptr_t)thread->kernel_stack + thread->kernel_stack_size;
    arch_task_init_stack(&thread->context, stack_top, kernel_thread_trampoline);
    thread->kernel_stack_top = stack_top;

    if (!g_all_threads) {
        g_all_threads = List_Create();
    }

    if (g_all_threads) {
        List_Add(g_all_threads, thread);
    }

    if (process && process->threads) {
        List_Add(process->threads, thread);
    }

    return thread;
}

void tasking_system_init(void)
{
    if (g_scheduler_active) {
        return;
    }

    g_ready_queue = List_Create();
    g_sleep_queue = List_Create();
    g_zombie_queue = List_Create();

    g_kernel_process = process_allocate("kernel", TASK_PROCESS_KERNEL);
    if (g_kernel_process) {
        g_kernel_process->pid = 0;
    }

    memset(&g_bootstrap_thread, 0, sizeof(g_bootstrap_thread));
    g_bootstrap_thread.tid = g_next_tid++;
    g_bootstrap_thread.type = TASK_THREAD_KERNEL;
    g_bootstrap_thread.state = TASK_THREAD_STATE_RUNNING;
    g_bootstrap_thread.process = g_kernel_process;
    g_bootstrap_thread.is_bootstrap = true;
    strcpy(g_bootstrap_thread.name, "bootstrap");
    g_bootstrap_thread.context.sp = arch_read_stack_pointer();

    g_current_thread = &g_bootstrap_thread;

    if (g_all_threads == NULL) {
        g_all_threads = List_Create();
    }
    if (g_all_threads) {
        List_Add(g_all_threads, &g_bootstrap_thread);
    }

    TaskThread* idle = thread_allocate(g_kernel_process, "idle", TASK_THREAD_KERNEL, idle_thread_entry, NULL, TASK_DEFAULT_KERNEL_STACK, true);
    if (!idle) {
        ERROR("task: failed to create idle thread");
        return;
    }
    idle->is_idle = true;
    idle->state = TASK_THREAD_STATE_READY;
    g_idle_thread = idle;

    g_scheduler_active = true;
}

TaskProcess* task_process_kernel(void)
{
    return g_kernel_process;
}

TaskProcess* task_process_create(const char* name, TaskProcessType type)
{
    TaskProcess* process = process_allocate(name, type);
    if (!process) {
        ERROR("task: failed to create process '%s'", name ? name : "<unnamed>");
    }
    return process;
}

TaskThread* task_thread_create_kernel(TaskProcess* process,
                                      const char* name,
                                      void (*entry)(void*),
                                      void* arg,
                                      size_t stack_size)
{
    if (!g_scheduler_active) {
        ERROR("task: scheduler not initialised");
        return NULL;
    }

    if (!process) {
        process = g_kernel_process;
    }

    TaskThread* thread = thread_allocate(process, name, TASK_THREAD_KERNEL, entry, arg, stack_size, true);
    if (!thread) {
        ERROR("task: failed to allocate kernel thread '%s'", name ? name : "<unnamed>");
        return NULL;
    }

    enqueue_ready_locked(thread);
    return thread;
}

TaskThread* task_thread_create_user(TaskProcess* process,
                                    const char* name,
                                    void (*entry)(void*),
                                    void* arg,
                                    size_t stack_size)
{
    WARN("task: user thread '%s' runs with kernel privileges (user mode pending)", name ? name : "<unnamed>");
    return task_thread_create_kernel(process, name, entry, arg, stack_size);
}

void task_yield(void)
{
    if (!g_scheduler_active) {
        return;
    }

    scheduler_lock();
    schedule_locked(true);
    scheduler_unlock();
}

void task_sleep_ms(uint64_t milliseconds)
{
    if (!g_scheduler_active || !g_current_thread) {
        return;
    }

    scheduler_lock();
    TaskThread* self = g_current_thread;
    if (self->is_idle) {
        scheduler_unlock();
        return;
    }

    self->wake_deadline = uptimeMs + milliseconds;
    self->state = TASK_THREAD_STATE_SLEEPING;
    if (!g_sleep_queue) {
        g_sleep_queue = List_Create();
    }
    if (g_sleep_queue) {
        List_Add(g_sleep_queue, self);
    }

    schedule_locked(false);
    scheduler_unlock();
}

void task_exit(int status)
{
    if (!g_scheduler_active || !g_current_thread) {
        return;
    }

    scheduler_lock();
    TaskThread* self = g_current_thread;
    self->exit_status = status;
    self->state = TASK_THREAD_STATE_ZOMBIE;

    if (!g_zombie_queue) {
        g_zombie_queue = List_Create();
    }
    if (g_zombie_queue) {
        List_Add(g_zombie_queue, self);
    }

    schedule_locked(false);
    scheduler_unlock();

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

TaskThread* task_current_thread(void)
{
    return g_current_thread;
}

TaskThreadState task_current_state(void)
{
    return g_current_thread ? g_current_thread->state : TASK_THREAD_STATE_INIT;
}

TaskThreadType task_current_type(void)
{
    return g_current_thread ? g_current_thread->type : TASK_THREAD_KERNEL;
}

bool task_scheduler_is_active(void)
{
    return g_scheduler_active;
}

