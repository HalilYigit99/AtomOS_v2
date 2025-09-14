section .text

global idt_default_isr

use32
idt_default_isr:
    iret
