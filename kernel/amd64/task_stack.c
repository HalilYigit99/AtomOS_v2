#include <task/context.h>
#include <stdint.h>

void arch_task_init_stack(TaskContext* context, uintptr_t stack_top, void (*entry_trampoline)(void))
{
    if (!context || !entry_trampoline) {
        return;
    }

    stack_top &= ~((uintptr_t)0xF);

    uint64_t* stack = (uint64_t*)stack_top;

    *(--stack) = (uint64_t)entry_trampoline;   // return address for ret
    *(--stack) = 0x00000202ull;                // RFLAGS with IF set
    *(--stack) = 0;                            // RAX
    *(--stack) = 0;                            // RBX
    *(--stack) = 0;                            // RCX
    *(--stack) = 0;                            // RDX
    *(--stack) = 0;                            // RSI
    *(--stack) = 0;                            // RDI
    *(--stack) = 0;                            // RBP
    *(--stack) = 0;                            // R8
    *(--stack) = 0;                            // R9
    *(--stack) = 0;                            // R10
    *(--stack) = 0;                            // R11
    *(--stack) = 0;                            // R12
    *(--stack) = 0;                            // R13
    *(--stack) = 0;                            // R14
    *(--stack) = 0;                            // R15

    context->sp = (uintptr_t)stack;
}

