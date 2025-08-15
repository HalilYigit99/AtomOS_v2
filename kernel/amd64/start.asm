section .text

global _start
extern mb2_signature
extern mb2_tagptr

extern gdtr_amd64
extern idt_init
extern idt_ptr
extern __boot_kernel_start
extern detect_erms

_start:

    cli ; Clear interrupts

    mov [mb2_signature], eax
    mov [mb2_tagptr], ebx

    ; Initialize GDT, IDT and other necessary structures here
    lgdt [gdtr_amd64] ; Load Global Descriptor Table
    ; Far jump to reload CS for 64-bit mode
    push 0x08
    push .afterLoadCS
    retfq
.afterLoadCS:
    mov ax, 0x10
    
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Initialize IDT (64-bit)
    call idt_init

    lidt [idt_ptr] ; Load IDT pointer

    ; Detect CPU features (ERMS) after segmentation set up
    call detect_erms

    ; Artık paging aktif; kernel girişine devam
    call __boot_kernel_start ; Call the boot kernel start function

    sti

.halt:
    hlt ; Halt the CPU
    jmp .halt ; Loop indefinitely
