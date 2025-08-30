; 64-bit yardımcı (libgcc benzeri) fonksiyonlar - i386 NASM
; Amaç: 32-bit derlenen çekirdekte GCC'nin üretebileceği 64-bit işlemleri (diğer
; mimari desteği yokken) karşılamak. (add, sub, shift, div, mod, cmp)
; Sözleşme:
;  - 64-bit değerler: düşük 32-bit önce, sonra yüksek 32-bit (cdecl stack düzeni)
;  - 64-bit dönüş: EDX = yüksek, EAX = düşük
;  - Karşılaştırma fonksiyonları: EAX = -1 / 0 / 1
;  - Shift fonksiyonları: Arg1 = 64-bit değer, Arg2 = shift miktarı (unsigned int)
;  - İmzalı bölme/mod için işaret işlenir, çekirdek algoritma unsigned'dır.
; Not: Basitlik adına kritik olmayan yerlerde optimize edilmemiş uzun bölme
; uygulanmıştır. Gerektiğinde daha hızlı algoritmalarla değiştirilebilir.

BITS 32
SECTION .text

; ---------------------------------------------------------------------------
; EAX:LOW, EDX:HIGH (dönüş)
; uint64_t __adddi3(uint64_t a, uint64_t b)
; ---------------------------------------------------------------------------
global __adddi3
__adddi3:
	; a_low  [esp+4]
	; a_high [esp+8]
	; b_low  [esp+12]
	; b_high [esp+16]
	mov eax, [esp+4]
	mov edx, [esp+8]
	add eax, [esp+12]
	adc edx, [esp+16]
	ret

; ---------------------------------------------------------------------------
; uint64_t __subdi3(uint64_t a, uint64_t b)  (a - b)
; ---------------------------------------------------------------------------
global __subdi3
__subdi3:
	mov eax, [esp+4]
	mov edx, [esp+8]
	sub eax, [esp+12]
	sbb edx, [esp+16]
	ret

; ---------------------------------------------------------------------------
; int __cmpdi2(int64_t a, int64_t b)
; return 0 if equal, 1 if a > b, -1 if a < b (signed)
; ---------------------------------------------------------------------------
global __cmpdi2
__cmpdi2:
	mov eax, [esp+8]      ; a_high
	mov edx, [esp+16]     ; b_high
	cmp eax, edx
	jl .less_signed
	jg .greater_signed
	; high equal -> compare low as unsigned
	mov eax, [esp+4]
	mov edx, [esp+12]
	cmp eax, edx
	jb .less_signed       ; low smaller
	ja .greater_signed
	xor eax, eax          ; equal
	ret
.greater_signed:
	mov eax, 1
	ret
.less_signed:
	mov eax, -1
	ret

; ---------------------------------------------------------------------------
; int __ucmpdi2(uint64_t a, uint64_t b)  (unsigned compare)
; ---------------------------------------------------------------------------
global __ucmpdi2
__ucmpdi2:
	mov eax, [esp+8]      ; a_high
	mov edx, [esp+16]     ; b_high
	cmp eax, edx
	jb .less
	ja .greater
	mov eax, [esp+4]
	mov edx, [esp+12]
	cmp eax, edx
	jb .less
	ja .greater
	xor eax, eax
	ret
.greater:
	mov eax, 1
	ret
.less:
	mov eax, -1
	ret
; uint64_t __ashldi3(uint64_t value, unsigned int shift)  (arithmetic/logical left)
; ---------------------------------------------------------------------------
global __ashldi3
__ashldi3:
	; Stack: ret, val_lo, val_hi, shift
	mov eax, [esp+4]
	mov edx, [esp+8]
	mov ecx, [esp+12]
	push ebx               ; preserve callee-saved
	cmp ecx, 64
	jae .zero
	cmp ecx, 32
	jae .ge32
	shld edx, eax, cl
	shl eax, cl
	pop ebx
	ret
.ge32:
	; shift >=32 && <64
	sub ecx, 32            ; ecx = shift-32
	mov edx, eax            ; new high = old low << (shift-32)
	xor eax, eax            ; new low cleared (will remain 0 or bits if shift<64)
	shl edx, cl
	pop ebx
	ret
.zero:
	xor eax, eax
	xor edx, edx
	pop ebx
	ret

; ---------------------------------------------------------------------------
; uint64_t __lshrdi3(uint64_t value, unsigned int shift) (logical right)
; ---------------------------------------------------------------------------
global __lshrdi3
__lshrdi3:
	; Stack: ret, val_lo, val_hi, shift
	mov eax, [esp+4]
	mov edx, [esp+8]
	mov ecx, [esp+12]
	push ebx
	cmp ecx, 64
	jae .zr
	cmp ecx, 32
	jae .ge32lr
	shrd eax, edx, cl
	shr edx, cl
	pop ebx
	ret
.ge32lr:
	; shift >=32 && <64
	sub ecx, 32            ; ecx = shift-32
	mov eax, edx            ; new low = old high >> (shift-32)
	xor edx, edx            ; new high = 0
	shr eax, cl
	pop ebx
	ret
.zr:
	xor eax, eax
	xor edx, edx
	pop ebx
	ret

; ---------------------------------------------------------------------------
; int64_t __ashrdi3(int64_t value, unsigned int shift) (arithmetic right)
; ---------------------------------------------------------------------------
global __ashrdi3
__ashrdi3:
	; Stack: ret, val_lo, val_hi, shift
	mov eax, [esp+4]
	mov edx, [esp+8]
	mov ecx, [esp+12]
	push ebx
	cmp ecx, 64
	jae .signfull
	cmp ecx, 32
	jae .ge32ar
	shrd eax, edx, cl
	sar edx, cl
	pop ebx
	ret
.ge32ar:
	; shift >=32 && <64
	sub ecx, 32            ; ecx = shift-32
	mov eax, edx            ; new low = old high >> (shift-32) arith
	sar eax, cl
	sar edx, 31            ; new high = sign bits
	pop ebx
	ret
.signfull:
	mov edx, [esp+8]
	sar edx, 31
	mov eax, edx
	pop ebx
	ret

; ---------------------------------------------------------------------------
; (Önceki iskelet uzun bölme kaldırıldı; artık __udivdi3 / __umoddi3 içinde tam
; 64-bit bit-kaydırmalı algoritma uygulanıyor.)

; ---------------------------------------------------------------------------
; unsigned __udivdi3(uint64_t n, uint64_t d)
; (Tam 64/64 destekli. d_hi==0 durumunda hızlı yol, aksi halde bit-tabanlı div.)
; ---------------------------------------------------------------------------
global __udivdi3
__udivdi3:
	mov eax, [esp+16]       ; d_hi
	test eax, eax
	jnz .udiv_64_64
	; 64/32 yolu
	mov ecx, [esp+12]       ; d_lo
	test ecx, ecx
	jz .udiv_divzero
	mov edx, [esp+8]        ; n_hi
	mov eax, [esp+4]        ; n_lo
	cmp edx, ecx
	jb .udiv_fast_single
	; n_hi >= d_lo -> iki aşama (bölüm 64-bit olabilir)
	push ebx
	mov ebx, ecx
	mov eax, edx
	xor edx, edx
	div ebx                 ; eax=q_high, edx=rem
	push eax                ; save q_high on stack
	mov eax, [esp+8]        ; n_lo (after two pushes? only ebx + q_high) -> correct
	div ebx                 ; eax = q_low (divides edx:eax)
	pop edx                 ; retrieve q_high
	pop ebx
	ret
.udiv_fast_single:
	div ecx                 ; edx:eax / ecx -> eax=quot, edx=rem
	xor edx, edx
	ret

.udiv_64_64:
	; Yavaş fakat basit bit-kaydırmalı 64/64 unsigned bölme (d_hi != 0).
	; Stack: ret, n_lo, n_hi, d_lo, d_hi
	push ebp
	push ebx
	push esi
	push edi
	sub esp, 16             ; locals: [0]=d_lo [4]=d_hi [8]=num_lo [12]=num_hi
	mov ebx, [esp+16+4]     ; d_lo (ret + pushes + locals offset)
	mov [esp+0], ebx
	mov ebx, [esp+20+4]     ; d_hi
	mov [esp+4], ebx
	mov ebx, [esp+8+4]      ; n_lo
	mov [esp+8], ebx
	mov ebx, [esp+12+4]     ; n_hi
	mov [esp+12], ebx
	xor eax, eax            ; rem_low
	xor edx, edx            ; rem_high
	xor edi, edi            ; quotient (32-bit)
	mov ecx, 64
.u64_loop:
	shl edi,1               ; quotient <<=1
	shl eax,1               ; remainder <<=1
	rcl edx,1
	; numerator <<=1, CF=bit çıkıyor
	mov ebx, [esp+8]
	shl ebx,1
	mov [esp+8], ebx
	mov ebx, [esp+12]
	rcl ebx,1
	mov [esp+12], ebx       ; şimdi CF = eski bit63
	adc eax,0               ; CF ekle
	; remainder >= divisor ?
	mov ebx, edx
	cmp ebx, [esp+4]
	jb .no_sub
	ja .do_sub
	mov ebx, eax
	cmp ebx, [esp+0]
	jb .no_sub
.do_sub:
	sub eax, [esp+0]
	sbb edx, [esp+4]
	inc edi
.no_sub:
	dec ecx
	jnz .u64_loop
	; sonucu döndür
	mov eax, edi
	xor edx, edx
	add esp,16
	pop edi
	pop esi
	pop ebx
	pop ebp
	ret

.udiv_divzero:
	cli
	hlt
	jmp .udiv_divzero

; ---------------------------------------------------------------------------
; unsigned __umoddi3(uint64_t n, uint64_t d)  (remainder)
; ---------------------------------------------------------------------------
global __umoddi3
__umoddi3:
	mov eax, [esp+16]       ; d_hi
	test eax, eax
	jnz .umod_64_64
	mov ecx, [esp+12]       ; d_lo
	test ecx, ecx
	jz .umod_divzero
	mov edx, [esp+8]        ; n_hi
	mov eax, [esp+4]        ; n_lo
	cmp edx, ecx
	jb .umod_fast_single
	push ebx
	mov ebx, ecx
	mov eax, edx
	xor edx, edx
	div ebx                 ; rem1
	mov eax, [esp+4+4]
	div ebx                 ; final rem -> edx
	mov eax, edx            ; remainder 32-bit
	xor edx, edx
	pop ebx
	ret
.umod_fast_single:
	div ecx
	mov eax, edx
	xor edx, edx
	ret
.umod_64_64:
	; Bit-kaydırmalı (aynı loop) -> sadece remainder döndürülür
	push ebp
	push ebx
	push esi
	push edi
	sub esp,16              ; locals: d_lo,d_hi,n_lo,n_hi
	mov ebx, [esp+16+4]     ; d_lo
	mov [esp+0], ebx
	mov ebx, [esp+20+4]     ; d_hi
	mov [esp+4], ebx
	mov ebx, [esp+8+4]      ; n_lo
	mov [esp+8], ebx
	mov ebx, [esp+12+4]     ; n_hi
	mov [esp+12], ebx
	xor eax, eax            ; rem_low
	xor edx, edx            ; rem_high
	xor edi, edi            ; quotient (lazım değil ama set 0)
	mov ecx, 64
.u64m_loop:
	shl edi,1
	shl eax,1
	rcl edx,1
	mov ebx, [esp+8]
	shl ebx,1
	mov [esp+8], ebx
	mov ebx, [esp+12]
	rcl ebx,1
	mov [esp+12], ebx
	adc eax,0
	mov ebx, edx
	cmp ebx, [esp+4]
	jb .no_sub_m
	ja .do_sub_m
	mov ebx, eax
	cmp ebx, [esp+0]
	jb .no_sub_m
.do_sub_m:
	sub eax, [esp+0]
	sbb edx, [esp+4]
	inc edi
.no_sub_m:
	dec ecx
	jnz .u64m_loop
	add esp,16
	pop edi
	pop esi
	pop ebx
	pop ebp
	ret
.umod_divzero:
	cli
	hlt
	jmp .umod_divzero

; ---------------------------------------------------------------------------
; signed __divdi3(int64_t n, int64_t d)
; ---------------------------------------------------------------------------
global __divdi3
__divdi3:
	push ebx
	push esi
	push edi
	push ebp
	; Load operands
	mov ebx, [esp+4+16]      ; n_lo
	mov edi, [esp+8+16]      ; n_hi
	mov esi, [esp+12+16]     ; d_lo
	mov ebp, [esp+16+16]     ; d_hi
	; Determine signs
	mov eax, edi
	sar eax, 31              ; sign_n in eax (0 or -1)
	mov edx, ebp
	sar edx, 31              ; sign_d in edx
	xor ecx, ecx
	mov ecx, eax             ; save sign_n (unused for quotient)
	xor eax, edx             ; eax = sign_q (0 or -1)
	push eax                 ; save sign_q
	; abs(n)
	test edi, edi
	jns .d3_n_abs_ok
	neg ebx
	adc edi, 0
	neg edi
.d3_n_abs_ok:
	; abs(d)
	test ebp, ebp
	jns .d3_d_abs_ok
	neg esi
	adc ebp, 0
	neg ebp
.d3_d_abs_ok:
	; Call __udivdi3(|n|,|d|)
	push ebp
	push esi
	push edi
	push ebx
	call __udivdi3
	add esp,16
	pop ecx                  ; ecx = sign_q
	test ecx, ecx
	jz .d3_no_neg
	neg eax
	adc edx,0
	neg edx
.d3_no_neg:
	pop ebp
	pop edi
	pop esi
	pop ebx
	ret

; ---------------------------------------------------------------------------
; signed __moddi3(int64_t n, int64_t d)
; ---------------------------------------------------------------------------
global __moddi3
__moddi3:
	push ebx
	push esi
	push edi
	push ebp
	; Load operands
	mov ebx, [esp+4+16]      ; n_lo
	mov edi, [esp+8+16]      ; n_hi
	mov esi, [esp+12+16]     ; d_lo
	mov ebp, [esp+16+16]     ; d_hi
	; Determine sign of n
	mov eax, edi
	sar eax, 31              ; sign_n (0 veya -1)
	push eax                 ; save sign_n
	; abs(n)
	test edi, edi
	jns .md3_n_abs_ok
	neg ebx
	adc edi, 0
	neg edi
.md3_n_abs_ok:
	; abs(d)
	test ebp, ebp
	jns .md3_d_abs_ok
	neg esi
	adc ebp, 0
	neg ebp
.md3_d_abs_ok:
	; Call __umoddi3(|n|,|d|)
	push ebp
	push esi
	push edi
	push ebx
	call __umoddi3
	add esp,16
	pop ecx                  ; ecx = sign_n
	test ecx, ecx
	jz .md3_no_neg
	neg eax
	adc edx,0
	neg edx
.md3_no_neg:
	pop ebp
	pop edi
	pop esi
	pop ebx
	ret

; ---------------------------------------------------------------------------
; uint64_t __muldi3(uint64_t a, uint64_t b)
; Karşılık: (a_low + 2^32 a_high) * (b_low + 2^32 b_high)
; Sonuç 128-bit; alt 64-bit döner.
; ---------------------------------------------------------------------------
global __muldi3
__muldi3:
	; Stack: ret, a_lo, a_hi, b_lo, b_hi
	mov eax, [esp+4]      ; a0
	mov ecx, [esp+12]     ; b0
	mul ecx               ; p0 = a0*b0  -> EDX:EAX
	mov esi, eax          ; p0_low32
	mov edi, edx          ; p0_high32
	; p1 = a0*b1
	mov eax, [esp+4]      ; a0
	mov ecx, [esp+16]     ; b1
	mul ecx               ; EDX:EAX = p1
	mov ebx, eax          ; p1_low32
	mov ebp, edx          ; p1_high32
	; p2 = a1*b0
	mov eax, [esp+8]      ; a1
	mov ecx, [esp+12]     ; b0
	mul ecx               ; EDX:EAX = p2
	add ebx, eax          ; cross_low32 = p1_low32 + p2_low32
	adc ebp, edx          ; cross_high32 = p1_high32 + p2_high32 + carry
	; result_low = p0_low32
	; result_high = p0_high32 + cross_low32 (mod 2^32)
	add edi, ebx
	; ignore overflow beyond 64 bits
	mov eax, esi
	mov edx, edi
	ret

; ---------------------------------------------------------------------------
; int64_t __negdi2(int64_t a)  => -a
; ---------------------------------------------------------------------------
global __negdi2
__negdi2:
	mov eax, [esp+4]
	mov edx, [esp+8]
	neg eax
	adc edx, 0
	neg edx
	ret

; ---------------------------------------------------------------------------
; Kombine bölüm+kalan: (signed) __divmoddi4(int64_t a, int64_t b, int64_t *rem)
; GCC bazen çağırabilir. Dönüş: quotient, *rem = remainder.
; Arg layout (cdecl): a_lo,a_hi,b_lo,b_hi, rem_ptr
; ---------------------------------------------------------------------------
global __divmoddi4
__divmoddi4:
	; Stack on entry: ret, a_lo, a_hi, b_lo, b_hi, rem_ptr
	push ebx
	push esi
	push edi
	; rem_ptr = [esp + 16 + 4] after pushes (ret + 3 pushes + original args)
	mov edi, [esp+16+4]
	; Prepare and call __divdi3(a,b)
	push dword [esp+12+4]    ; b_hi
	push dword [esp+12+4]    ; b_lo
	push dword [esp+12+4]    ; a_hi
	push dword [esp+12+4]    ; a_lo
	call __divdi3
	add esp,16
	; Save quotient in (esi:ebx)
	mov esi, eax
	mov ebx, edx
	; Prepare and call __moddi3(a,b)
	push dword [esp+12+4]    ; b_hi
	push dword [esp+12+4]    ; b_lo
	push dword [esp+12+4]    ; a_hi
	push dword [esp+12+4]    ; a_lo
	call __moddi3
	add esp,16
	; Store remainder
	mov [edi], eax
	mov [edi+4], edx
	; Restore quotient
	mov eax, esi
	mov edx, ebx
	pop edi
	pop esi
	pop ebx
	ret

; ---------------------------------------------------------------------------
; NOT: Bölme / mod yavaş (bit döngülü) fakat doğruluk önceliklidir. Gerekirse
; ileride normalizasyonlu (Knuth) daha hızlı algoritma eklenebilir.

