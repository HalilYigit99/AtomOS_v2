section .text

; (C code function)
; void i386_processor_exceptions_handle(uint8_t vector, const void* gp_regs, const void* cpu_frame, bool has_error_code)
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

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    mov     edi, %1            ; arg0: vector
    mov     rsi, rsp           ; arg1: gp register dump base
    lea     rdx, [rsp + 15*8]  ; arg2: cpu frame start (RIP,...)
    xor     ecx, ecx           ; arg3: has_error_code = false
    call    i386_processor_exceptions_handle

    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    sti
    iretq
%endmacro

; Hata kodu OLAN exceptionlar için (CPU stack'e error code pushlar -> iretq'tan önce temizle)
%macro EXC64_ERR 1
i386_exception_%1_isr:
    cli

    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rdi
    push rsi
    push rbp
    push rbx
    push rdx
    push rcx
    push rax

    mov     edi, %1            ; arg0: vector
    mov     rsi, rsp           ; arg1: gp register dump base
    lea     rdx, [rsp + 15*8]  ; arg2: cpu frame start (error code + RIP,...)
    mov     ecx, 1             ; arg3: has_error_code = true
    call    i386_processor_exceptions_handle

    pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    sti
    add     rsp, 8             ; CPU'nun pushladığı error code'u at
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

    mov     eax, esp         ; gp register dump base (EDI..EAX order)
    lea     edx, [esp + 32]  ; cpu frame start (EIP, CS, ...)

    push    dword 0          ; has_error_code = false
    push    edx              ; cpu frame
    push    eax              ; gp regs
    push    dword %1         ; vector
    call    i386_processor_exceptions_handle
    add     esp, 16          ; clean up args
    popad

    sti
    iret
%endmacro

%macro EXC32_ERR 1
i386_exception_%1_isr:
    cli

    pushad

    mov     eax, esp
    lea     edx, [esp + 32]

    push    dword 1          ; has_error_code = true
    push    edx              ; cpu frame (error code + ...)
    push    eax              ; gp regs
    push    dword %1
    call    i386_processor_exceptions_handle
    add     esp, 16
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
