section .text

global _start
extern mb2_signature
extern mb2_tagptr

extern gdtr_i386

_start:

    cli ; Clear interrupts

    mov [mb2_signature], eax
    mov [mb2_tagptr], ebx

    ; Initialize GDT, IDT and other necessary structures here
    lgdt [gdtr_i386] ; Load Global Descriptor Table
    jmp 0x08:.afterLoadCS
.afterLoadCS:
    mov ax, 0x10
    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    hlt ; Halt the CPU

