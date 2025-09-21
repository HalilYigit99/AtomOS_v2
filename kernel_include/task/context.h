#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct {
    uintptr_t sp;
} TaskContext;

void arch_task_context_switch(TaskContext* previous, TaskContext* next);
void arch_task_init_stack(TaskContext* context,
                          uintptr_t stack_top,
                          void (*entry_trampoline)(void));

static inline uintptr_t arch_read_stack_pointer(void)
{
#if defined(__x86_64__)
    uintptr_t value;
    __asm__ __volatile__("mov %%rsp, %0" : "=r"(value));
    return value;
#else
    uintptr_t value;
    __asm__ __volatile__("mov %%esp, %0" : "=r"(value));
    return value;
#endif
}

#ifdef __cplusplus
}
#endif

