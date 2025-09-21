#include <task/context.h>
#include <stdint.h>

void arch_task_init_stack(TaskContext* context, uintptr_t stack_top, void (*entry_trampoline)(void))
{
    if (!context || !entry_trampoline) {
        return;
    }

    stack_top &= ~((uintptr_t)0xF);

    uint32_t* stack = (uint32_t*)stack_top;

    *(--stack) = (uint32_t)entry_trampoline; // return address
    *(--stack) = 0x00000202u;               // EFLAGS with IF set
    *(--stack) = 0;                         // EAX
    *(--stack) = 0;                         // ECX
    *(--stack) = 0;                         // EDX
    *(--stack) = 0;                         // EBX
    *(--stack) = 0;                         // Original ESP (ignored on popad)
    *(--stack) = 0;                         // EBP
    *(--stack) = 0;                         // ESI
    *(--stack) = 0;                         // EDI

    context->sp = (uintptr_t)stack;
}

