section .text

global ps2kbd_isr

extern ps2kbd_handler

%if __BITS__ == 64

use64
ps2kbd_isr:
    cli
    push rax
    push rbx
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

    call ps2kbd_handler

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
    pop rbx
    pop rax

    sti
    iretq


%else

use32

ps2kbd_isr:
    cli
    pushad

    call ps2kbd_handler

    popad
    sti
    iret

%endif