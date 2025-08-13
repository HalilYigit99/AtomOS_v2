section .data

global gdt_i386
global gdtr_i386

gdt_i386:
    ; Null Descriptor
    dd 0x00000000
    dd 0x00000000
    
    ; Kernel Code Segment (0x08)
    dw 0xFFFF       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0x9A         ; Access byte (Present, Ring 0, Code, Execute/Read)
    db 0xCF         ; Flags + Limit (16-19)
    db 0x00         ; Base (24-31)
    
    ; Kernel Data Segment (0x10)
    dw 0xFFFF       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0x92         ; Access byte (Present, Ring 0, Data, Read/Write)
    db 0xCF         ; Flags + Limit (16-19)
    db 0x00         ; Base (24-31)
    
    ; User Code Segment (0x18)
    dw 0xFFFF       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0xFA         ; Access byte (Present, Ring 3, Code, Execute/Read)
    db 0xCF         ; Flags + Limit (16-19)
    db 0x00         ; Base (24-31)
    
    ; User Data Segment (0x20)
    dw 0xFFFF       ; Limit (0-15)
    dw 0x0000       ; Base (0-15)
    db 0x00         ; Base (16-23)
    db 0xF2         ; Access byte (Present, Ring 3, Data, Read/Write)
    db 0xCF         ; Flags + Limit (16-19)
    db 0x00         ; Base (24-31)
    
    ; TSS Segment (0x28)
    dw 0x0067       ; Limit (104 bytes - 1)
    dw 0x0000       ; Base (0-15) - will be set at runtime
    db 0x00         ; Base (16-23) - will be set at runtime
    db 0x89         ; Access byte (Present, Ring 0, TSS Available)
    db 0x00         ; Flags + Limit (16-19)
    db 0x00         ; Base (24-31) - will be set at runtime

gdtr_i386:
    dw $ - gdt_i386 - 1  ; GDT size
    dd gdt_i386          ; GDT address