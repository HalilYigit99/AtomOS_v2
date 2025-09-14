default rel
section .text

; System V AMD64 calling convention
; uint8_t inb(uint16_t port)
global inb
inb:
	mov dx, di        ; port in RDI (lower 16 bits)
	in al, dx
	movzx eax, al
	ret

; void outb(uint16_t port, uint8_t value)
global outb
outb:
	mov dx, di        ; port (RDI)
	mov al, sil       ; value in RSI (lower 8 bits)
	out dx, al
	ret

; uint16_t inw(uint16_t port)
global inw
inw:
	mov dx, di
	in ax, dx
	movzx eax, ax
	ret

; void outw(uint16_t port, uint16_t value)
global outw
outw:
	mov dx, di
	mov ax, si
	out dx, ax
	ret

; uint32_t inl(uint16_t port)
global inl
inl:
	mov dx, di
	in eax, dx
	ret

; void outl(uint16_t port, uint32_t value)
global outl
outl:
	mov dx, di
	mov eax, esi
	out dx, eax
	ret

; void io_wait(void)
global io_wait
io_wait:
	mov dx, 0x80
	mov al, 0
	out dx, al
	ret


