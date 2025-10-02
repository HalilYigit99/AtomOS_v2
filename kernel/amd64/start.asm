section .text

global _start
extern mb2_signature
extern mb2_tagptr
extern __stack_end

extern gdtr_amd64
extern idt_init
extern idt_ptr
extern __boot_kernel_start
extern detect_erms
extern amd64_map_identity_low_4g

_start:

    cli ; Clear interrupts

    mov esp, __stack_end ; Set up the stack pointer

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

    ; Ensure low 4GiB identity-mapped with 2MiB pages (MMIO reachable)
    call amd64_map_identity_low_4g

    ; Artık paging aktif; kernel girişine devam
    call __boot_kernel_start ; Call the boot kernel start function

    sti

.halt:
    hlt ; Halt the CPU
    jmp .halt ; Loop indefinitely
