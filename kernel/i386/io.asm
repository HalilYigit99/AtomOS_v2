section .text

; uint8_t inb(uint16_t port)
global inb
inb:
	mov dx, [esp+4]   ; port
	in al, dx
	movzx eax, al
	ret

; void outb(uint16_t port, uint8_t value)
global outb
outb:
	mov dx, [esp+4]   ; port
	mov al, [esp+8]   ; value
	out dx, al
	ret

; uint16_t inw(uint16_t port)
global inw
inw:
	mov dx, [esp+4]
	in ax, dx
	movzx eax, ax
	ret

; void outw(uint16_t port, uint16_t value)
global outw
outw:
	mov dx, [esp+4]
	mov ax, [esp+8]
	out dx, ax
	ret

; uint32_t inl(uint16_t port)
global inl
inl:
	mov dx, [esp+4]
	in eax, dx
	ret

; void outl(uint16_t port, uint32_t value)
global outl
outl:
	mov dx, [esp+4]
	mov eax, [esp+8]
	out dx, eax
	ret

; void io_wait(void) -- klasik 0x80 portuna dummy yazma
global io_wait
io_wait:
	mov dx, 0x80
	mov al, 0
	out dx, al
	ret


