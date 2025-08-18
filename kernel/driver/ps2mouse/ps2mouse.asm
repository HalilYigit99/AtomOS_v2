section .text
use32

global ps2mouse_isr
extern ps2mouse_isr_handler

%if __BITS__ == 32

ps2mouse_isr:
    cli
    ; Save registers that will be used
    pushad

    ; Call the handler function
    call ps2mouse_isr_handler

    ; Send End of Interrupt (EOI) signal to the PIC
    mov al, 0x20
    out 0xA0, al    ; Slave PIC EOI
    out 0x20, al    ; Master PIC EOI


    ; Restore registers
    popad

    sti
    ; Return from interrupt
    iret

%else

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

    ; Send End of Interrupt (EOI) signal to the PIC
    mov al, 0x20
    out 0xA0, al    ; Slave PIC EOI
    out 0x20, al    ; Master PIC EOI

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

%endif