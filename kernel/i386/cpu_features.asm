section .bss

; Global flag indicating ERMS support (0/1)
global erms_supported
erms_supported: resb 1

section .text
use32

; void detect_erms(void)
; Detect ERMS via CPUID.(EAX=7,ECX=0):EBX[9]
; Writes 0/1 into erms_supported and returns it in EAX
global detect_erms
detect_erms:
    push    ebx                ; preserve callee-saved
    xor     eax, eax
    mov     eax, 7             ; leaf 7
    xor     ecx, ecx           ; subleaf 0
    cpuid
    ; Check EBX bit 9
    bt      ebx, 9
    setc    al
    mov     byte [erms_supported], al
    movzx   eax, al
    pop     ebx
    ret
