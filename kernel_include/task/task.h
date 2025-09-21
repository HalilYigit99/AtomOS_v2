#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define TASK_NAME_MAX_LENGTH 32
#define TASK_DEFAULT_KERNEL_STACK (16 * 1024)

typedef struct TaskProcess TaskProcess;
typedef struct TaskThread TaskThread;

typedef enum {
    TASK_PROCESS_KERNEL = 0,
    TASK_PROCESS_USER   = 1
} TaskProcessType;

typedef enum {
    TASK_THREAD_KERNEL = 0,
    TASK_THREAD_USER   = 1
} TaskThreadType;

typedef enum {
    TASK_THREAD_STATE_INIT = 0,
    TASK_THREAD_STATE_READY,
    TASK_THREAD_STATE_RUNNING,
    TASK_THREAD_STATE_SLEEPING,
    TASK_THREAD_STATE_BLOCKED,
    TASK_THREAD_STATE_ZOMBIE
} TaskThreadState;

void tasking_system_init(void);

TaskProcess* task_process_kernel(void);
TaskProcess* task_process_create(const char* name, TaskProcessType type);

TaskThread* task_thread_create_kernel(TaskProcess* process,
                                      const char* name,
                                      void (*entry)(void*),
                                      void* arg,
                                      size_t stack_size);

TaskThread* task_thread_create_user(TaskProcess* process,
                                    const char* name,
                                    void (*entry)(void*),
                                    void* arg,
                                    size_t stack_size);

void task_yield(void);
void task_sleep_ms(uint64_t milliseconds);
void task_exit(int status);

TaskThread* task_current_thread(void);
TaskThreadState task_current_state(void);
TaskThreadType task_current_type(void);

bool task_scheduler_is_active(void);

#ifdef __cplusplus
}
#endif

