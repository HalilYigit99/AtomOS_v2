section .text

; (C code function)
; void i386_processor_exceptions_handle(uint8_t exceptionNumber)
extern i386_processor_exceptions_handle

global i386_exception_0_isr
global i386_exception_1_isr
global i386_exception_2_isr
global i386_exception_3_isr
global i386_exception_4_isr
global i386_exception_5_isr
global i386_exception_6_isr
global i386_exception_7_isr
global i386_exception_8_isr
global i386_exception_9_isr
global i386_exception_10_isr
global i386_exception_11_isr
global i386_exception_12_isr
global i386_exception_13_isr
global i386_exception_14_isr
global i386_exception_15_isr
global i386_exception_16_isr
global i386_exception_17_isr
global i386_exception_18_isr
global i386_exception_19_isr
global i386_exception_20_isr
global i386_exception_21_isr
global i386_exception_22_isr
global i386_exception_23_isr
global i386_exception_24_isr
global i386_exception_25_isr
global i386_exception_26_isr
global i386_exception_27_isr
global i386_exception_28_isr
global i386_exception_29_isr
global i386_exception_30_isr
global i386_exception_31_isr

%if __BITS__ == 64
use64

; 64-bit ISR şablonları
; Hata kodu OLMAYAN exceptionlar için
%macro EXC64_NOERR 1
i386_exception_%1_isr:
    cli

    ; Save caller-saved + rbx for safety
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    ; Arg0 = exception number (uint8_t)
    xor     edi, edi
    mov     dil, %1
    call    i386_processor_exceptions_handle

    ; Restore registers
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    sti
    iretq
%endmacro

; Hata kodu OLAN exceptionlar için (CPU stack'e error code pushlar -> iretq'tan önce temizle)
%macro EXC64_ERR 1
i386_exception_%1_isr:
    cli

    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    xor     edi, edi
    mov     dil, %1
    call    i386_processor_exceptions_handle

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    sti
    add rsp, 8        ; CPU'nun pushladığı error code'u at
    iretq
%endmacro

; 0..31 için üretim (Intel SDM'ye göre error code olanlar: 8,10,11,12,13,14,17,21,30)
EXC64_NOERR 0
EXC64_NOERR 1
EXC64_NOERR 2
EXC64_NOERR 3
EXC64_NOERR 4
EXC64_NOERR 5
EXC64_NOERR 6
EXC64_NOERR 7
EXC64_ERR   8
EXC64_NOERR 9
EXC64_ERR   10
EXC64_ERR   11
EXC64_ERR   12
EXC64_ERR   13
EXC64_ERR   14
EXC64_NOERR 15
EXC64_NOERR 16
EXC64_ERR   17
EXC64_NOERR 18
EXC64_NOERR 19
EXC64_NOERR 20
EXC64_ERR   21
EXC64_NOERR 22
EXC64_NOERR 23
EXC64_NOERR 24
EXC64_NOERR 25
EXC64_NOERR 26
EXC64_NOERR 27
EXC64_NOERR 28
EXC64_NOERR 29
EXC64_ERR   30
EXC64_NOERR 31

%else
use32

; 32-bit ISR şablonları
%macro EXC32_NOERR 1
i386_exception_%1_isr:
    cli

    pushad
    push dword %1            ; arg0 (cdecl, caller cleans)
    call i386_processor_exceptions_handle
    add esp, 4               ; arg temizliği
    popad

    sti
    iret
%endmacro

%macro EXC32_ERR 1
i386_exception_%1_isr:
    cli

    pushad
    push dword %1
    call i386_processor_exceptions_handle
    add esp, 4               ; arg temizliği
    popad

    add esp, 4               ; CPU'nun pushladığı error code'u at
    sti
    iret
%endmacro

; 0..31 için üretim (error code olanlar: 8,10,11,12,13,14,17,21,30)
EXC32_NOERR 0
EXC32_NOERR 1
EXC32_NOERR 2
EXC32_NOERR 3
EXC32_NOERR 4
EXC32_NOERR 5
EXC32_NOERR 6
EXC32_NOERR 7
EXC32_ERR   8
EXC32_NOERR 9
EXC32_ERR   10
EXC32_ERR   11
EXC32_ERR   12
EXC32_ERR   13
EXC32_ERR   14
EXC32_NOERR 15
EXC32_NOERR 16
EXC32_ERR   17
EXC32_NOERR 18
EXC32_NOERR 19
EXC32_NOERR 20
EXC32_ERR   21
EXC32_NOERR 22
EXC32_NOERR 23
EXC32_NOERR 24
EXC32_NOERR 25
EXC32_NOERR 26
EXC32_NOERR 27
EXC32_NOERR 28
EXC32_NOERR 29
EXC32_ERR   30
EXC32_NOERR 31

%endif
