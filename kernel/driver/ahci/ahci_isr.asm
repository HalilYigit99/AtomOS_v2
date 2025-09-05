section .text

global ahci_isr_stub

extern ahci_irq_isr

%if __BITS__ == 64
use64
ahci_isr_stub:
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

    call ahci_irq_isr

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
ahci_isr_stub:
    cli
    pushad
    call ahci_irq_isr
    popad
    sti
    iret
%endif

