default rel
section .text

global memcpy
extern erms_supported

use64

memcpy:
    cld
    mov     rax, rdi            ; return dest

    test    rdx, rdx
    jz      .ret

    ; If ERMS supported, prefer rep movsb for all sizes
    cmp     byte [rel erms_supported], 0
    je      .no_erms
    mov     rcx, rdx
    rep     movsb
    jmp     .ret

.no_erms:
    mov     rcx, rdx
    cmp     rcx, 32
    jb      .small              ; small sizes: byte copy

    ; If src and dst share 8-byte alignment, align to 8 first
    mov     r8, rdi
    xor     r8, rsi
    test    r8, 7
    jne     .bulk

    mov     r8, rdi
    and     r8, 7
    jz      .bulk
    mov     r9, 8
    sub     r9, r8              ; bytes to reach 8-byte alignment
    cmp     r9, rdx
    cmova   r9, rdx             ; clamp to remaining length
    mov     rcx, r9
    rep     movsb
    sub     rdx, r9

.bulk:
    mov     rcx, rdx
    mov     r8, rcx
    shr     rcx, 3              ; qword count
    rep     movsq
    mov     rcx, r8
    and     rcx, 7              ; tail bytes
    rep     movsb
    jmp     .ret

.small:
    mov     rcx, rdx
    rep     movsb

.ret:
    ret
