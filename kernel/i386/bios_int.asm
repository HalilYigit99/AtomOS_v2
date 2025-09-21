section .bios_code

global bios_int

global bios_ax
global bios_bx
global bios_cx
global bios_dx
global bios_si
global bios_di
global bios_bp
global bios_sp
global bios_cs
global bios_es
global bios_ds
global bios_fs
global bios_gs
global bios_ss
global bios_flags

global bios_int_no

extern idt_ptr

use32

bios_int:
    cli
    pushfd
    pushad
    push ds
    push es
    push fs
    push gs

    mov [saved_pm_esp], esp
    mov ax, ss
    mov [saved_pm_ss], ax

    mov eax, cr0
    mov [saved_cr0], eax

    mov edx, eax
    and edx, 0x7fffffff          ; clear PG bit
    mov cr0, edx

    and edx, 0xfffffffe          ; clear PE bit
    mov cr0, edx

    jmp 0x700:rm_entry

use16
rm_entry:
    cli
    mov ax, cs
    mov ds, ax
    mov ss, ax
    mov sp, rm_stack_top

    lidt [cs:idt_real]

    movzx bx, byte [cs:bios_int_no]
    shl bx, 2
    xor ax, ax
    mov ds, ax
    mov dx, [bx + 2]
    mov cx, [bx]
    mov [cs:int_vector_segment], dx
    mov [cs:int_vector_offset], cx

    mov ax, cs
    mov ds, ax

    mov ax, [cs:bios_ax]
    mov bx, [cs:bios_bx]
    mov cx, [cs:bios_cx]
    mov dx, [cs:bios_dx]
    mov si, [cs:bios_si]
    mov di, [cs:bios_di]
    mov bp, [cs:bios_bp]

    mov ax, [cs:bios_ds]
    mov ds, ax
    mov ax, [cs:bios_es]
    mov es, ax
    mov ax, [cs:bios_fs]
    mov fs, ax
    mov ax, [cs:bios_gs]
    mov gs, ax

    mov ax, [cs:bios_flags]
    and ax, 0xfcff               ; clear IF/TF like real INT
    push ax
    push cs
    push word bios_after
    push word [cs:int_vector_segment]
    push word [cs:int_vector_offset]
    retf

bios_after:
    mov [cs:bios_ax], ax
    mov [cs:bios_bx], bx
    mov [cs:bios_cx], cx
    mov [cs:bios_dx], dx
    mov [cs:bios_si], si
    mov [cs:bios_di], di
    mov [cs:bios_bp], bp

    mov ax, ds
    mov [cs:bios_ds], ax
    mov ax, es
    mov [cs:bios_es], ax
    mov ax, fs
    mov [cs:bios_fs], ax
    mov ax, gs
    mov [cs:bios_gs], ax
    mov ax, ss
    mov [cs:bios_ss], ax
    mov ax, sp
    mov [cs:bios_sp], ax

    pushf
    pop ax
    mov [cs:bios_flags], ax

    mov ax, cs
    mov [cs:bios_cs], ax

    cli
    mov ax, cs
    mov ds, ax
    mov ss, ax
    mov sp, rm_stack_top

    o32 mov eax, [cs:saved_cr0]
    o32 mov cr0, eax
    o32 jmp far [cs:pm_return_ptr]

use32
pm_return:
    mov ax, [cs:saved_pm_ss]
    mov ss, ax
    mov esp, [cs:saved_pm_esp]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    lidt [idt_ptr]

    pop gs
    pop fs
    pop es
    pop ds
    popad
    popfd
    ret

align 4
saved_cr0:           dd 0
pm_return_ptr:       dd pm_return
                     dw 0x08
saved_pm_esp:        dd 0
saved_pm_ss:         dw 0
                     dw 0
int_vector_offset:   dw 0
int_vector_segment:  dw 0

rm_stack:            times 512 db 0
rm_stack_top:

idt_real:
    dw 0x03ff
    dd 0x00000000

bios_ax:     dw 0
bios_bx:     dw 0
bios_cx:     dw 0
bios_dx:     dw 0
bios_si:     dw 0
bios_di:     dw 0
bios_bp:     dw 0
bios_sp:     dw 0
bios_es:     dw 0
bios_ds:     dw 0
bios_fs:     dw 0
bios_gs:     dw 0
bios_ss:     dw 0
bios_flags:  dw 0
bios_cs:     dw 0

bios_int_no: db 0
