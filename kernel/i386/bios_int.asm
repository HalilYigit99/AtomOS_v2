section .bios_code

global bios_int

global bios_ax
global bios_bx
global bios_cx
global bios_dx

global bios_int_no

extern pm_registers

use32

; called from 32 bit code
bios_int:
    ret


idt_real:
    dw 0x3ff
    dd 0

bios_ax: dw 0
bios_bx: dw 0
bios_cx: dw 0
bios_dx: dw 0

bios_int_no: db 0
