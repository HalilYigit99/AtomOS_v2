section .text

global memcpy
extern erms_supported

use32

; void* memcpy(void* dest [ESP+4], const void* src [ESP+8], size_t n [ESP+12])
memcpy:
	push    edi
	push    esi
	push    ebx

	mov     edi, [esp+16]     ; dest
	mov     esi, [esp+20]     ; src
	mov     ecx, [esp+24]     ; n
	mov     eax, edi          ; return dest in EAX

	test    ecx, ecx
	jz      .ret

	; If ERMS supported, prefer rep movsb for all sizes
	cmp     byte [erms_supported], 0
	je      .no_erms
	rep     movsb
	jmp     .ret

.no_erms:
	cmp     ecx, 32
	jb      .small

	; If src and dst share 4-byte alignment, align to 4 first
	mov     ebx, edi
	xor     ebx, esi
	test    ebx, 3
	jne     .bulk

	; Align dest to 4 bytes
	mov     ebx, edi
	and     ebx, 3
	jz      .bulk
	mov     edx, 4
	sub     edx, ebx           ; bytes to reach 4-byte alignment
	cmp     edx, ecx
	cmova   edx, ecx
	mov     ecx, edx
	rep     movsb
	mov     ecx, [esp+24]
	sub     ecx, edx

.bulk:
	mov     edx, ecx
	shr     ecx, 2             ; dword count
	rep     movsd
	mov     ecx, edx
	and     ecx, 3             ; tail bytes
	rep     movsb
	jmp     .ret

.small:
	rep     movsb

.ret:
	pop     ebx
	pop     esi
	pop     edi
	ret
