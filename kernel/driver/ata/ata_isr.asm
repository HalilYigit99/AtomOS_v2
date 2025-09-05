section .text

global ata_irq14_stub
global ata_irq15_stub

extern ata_irq14
extern ata_irq15

%if __BITS__ == 64
use64
ata_irq14_stub:
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
    call ata_irq14
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

ata_irq15_stub:
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
    call ata_irq15
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
ata_irq14_stub:
    cli
    pushad
    call ata_irq14
    popad
    sti
    iret

ata_irq15_stub:
    cli
    pushad
    call ata_irq15
    popad
    sti
    iret
%endif

