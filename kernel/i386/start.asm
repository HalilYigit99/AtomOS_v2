section .text

global _start
extern mb2_signature
extern mb2_tagptr
extern idt_init
extern idt_ptr
extern __boot_kernel_start
extern gdtr_i386
extern detect_erms
extern kmain

extern page_directory
extern paging_init

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

    call idt_init ; Initialize IDT (32-bit)

    lidt [idt_ptr] ; Load IDT pointer

    ; Detect CPU features (ERMS)
    call detect_erms
    
    ; Enable disable by clearing the PG bit in CR0
    mov eax, cr0 ; Read CR0
    and eax, 0x7FFFFFFF ; Clear the PG bit (bit 31) to disable paging
    mov cr0, eax ; Write back to CR0

    call __boot_kernel_start ; Call the boot kernel start function

    sti

    call kmain ; Call the main kernel function

.halt:
    hlt ; Halt the CPU
    jmp .halt ; Loop indefinitely

