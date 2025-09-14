section .text

global ps2mouse_isr
extern ps2mouse_isr_handler

%if __BITS__ == 64

use64
ps2mouse_isr:
    cli
    ; Save registers that will be used
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

    ; Call the handler function
    call ps2mouse_isr_handler


    ; Restore registers
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
    ; Return from interrupt
    iretq

%else

use32
ps2mouse_isr:
    cli
    ; Save registers that will be used
    pushad

    ; Call the handler function
    call ps2mouse_isr_handler

    ; Restore registers
    popad

    sti
    ; Return from interrupt
    iret

%endif