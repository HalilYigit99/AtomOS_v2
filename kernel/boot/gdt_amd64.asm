section .data

global gdt_amd64
global gdtr_amd64

gdt_amd64:
    ; Null segment (0x00)
    dq 0x0000000000000000
    
    ; Kernel code segment (0x08)
    dq 0x00AF9A000000FFFF
    
    ; Kernel data segment (0x10)
    dq 0x00AF92000000FFFF
    
    ; User data segment (0x18)
    dq 0x00AFF2000000FFFF
    
    ; User code segment (0x20)
    dq 0x00AFFA000000FFFF
    
    ; TSS segment (0x28) - will be filled at runtime
    dq 0x0000000000000000
    dq 0x0000000000000000

gdt_amd64_end:

gdtr_amd64:
    dw gdt_amd64_end - gdt_amd64 - 1
    dd gdt_amd64
    dd 0x00000000 ; Reserved