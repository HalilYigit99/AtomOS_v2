section .text

global idt_default_isr_amd64

; Simple default ISR that just returns with iretq

use64
idt_default_isr_amd64:
    iretq
