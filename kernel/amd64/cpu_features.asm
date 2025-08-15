default rel
section .bss

; Global flag indicating ERMS support (0/1)
global erms_supported
erms_supported: resb 1

section .text

; void detect_erms(void)
; - Detects Enhanced REP MOVSB/STOSB (ERMS) via CPUID.(EAX=7,ECX=0):EBX[9]
; - Writes 0/1 to erms_supported and also returns it in AL/EAX (for convenience)
; - Preserves callee-saved registers as per SysV AMD64 (RBX is clobbered by CPUID, so we save it)
global detect_erms
detect_erms:
    push    rbx                ; preserve callee-saved
    xor     eax, eax
    mov     eax, 7             ; CPUID leaf 7
    xor     ecx, ecx           ; subleaf 0
    cpuid                      ; EBX has feature flags

    ; Check EBX bit 9 (ERMS)
    bt      ebx, 9
    setc    al                 ; AL = 1 if ERMS present, else 0
    mov     byte [rel erms_supported], al
    movzx   eax, al            ; return 0/1 in EAX as well
    pop     rbx
    ret
