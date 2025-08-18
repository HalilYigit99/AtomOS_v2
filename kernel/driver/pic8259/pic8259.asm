section .text

global pic8259_irq2
global pic8259_slave_default_isr
global pic8259_irq2_handler
extern pic8259_irq2_isr_addr

%if __BITS__ == 64
use64
pic8259_irq2:
    cli

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

    call pic8259_irq2_isr_addr

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

    push qword [pic8259_irq2_isr_addr]
    ret

pic8259_slave_default_isr:
    cli

    push rax

    mov al, 0x20
    out 0xA0, al
    out 0x20, al

    pop rax

    iretq

%else

use32
pic8259_irq2:
    cli

    pushad
    call pic8259_irq2_isr_addr
    popad

    push dword [pic8259_irq2_isr_addr]

    ret

pic8259_slave_default_isr:
    cli

    push eax

    mov al, 0x20
    out 0xA0, al
    out 0x20, al

    pop eax

    iret

%endif
