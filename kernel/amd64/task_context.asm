section .text
BITS 64

global arch_task_context_switch

arch_task_context_switch:
    pushfq
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rax, [rsp + 72]
    test rax, rax
    jz .skip_save
    mov [rax], rsp
.skip_save:
    mov rax, [rsp + 80]
    mov rsp, [rax]

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    popfq
    ret
