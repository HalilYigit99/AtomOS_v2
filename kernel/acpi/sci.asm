section .text

global sci_isr
extern sci_isr_handler

%if __BITS__ == 64
use64

sci_isr:
    cli

    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call sci_isr_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax

    sti
    iretq

%else
use32
sci_isr:
    cli

    pushad

    call sci_isr_handler

    popad

    sti

    iret
    
%endif
