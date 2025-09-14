// 64-bit IDT implementation (mirrors i386 style but uses 64-bit gate layout)
#include <arch.h>

// 64-bit IDT gate descriptor
typedef struct {
	uint16_t offset_low;     // bits 0..15
	uint16_t selector;       // code segment selector in GDT
	uint8_t  ist;            // bits 0..2 IST, rest zero
	uint8_t  type_attr;      // type and attributes
	uint16_t offset_mid;     // bits 16..31
	uint32_t offset_high;    // bits 32..63
	uint32_t zero;           // reserved
} __attribute__((packed)) idt_entry64_t;

typedef struct {
	uint16_t limit;          // size - 1
	uint64_t base;           // base address
} __attribute__((packed)) idt_ptr64_t;

idt_entry64_t idt[256];
idt_ptr64_t   idt_ptr;

extern void idt_default_isr_amd64();

void idt_init(void)
{
	idt_ptr.limit = sizeof(idt) - 1;
	idt_ptr.base  = (uint64_t)idt;

	for (int i = 0; i < 256; ++i) {
		idt_set_gate(i, (size_t)(uintptr_t)idt_default_isr_amd64);
	}

}

void idt_set_gate(uint8_t vector, size_t offset)
{
	idt[vector].offset_low  = offset & 0xFFFF;
	idt[vector].offset_mid  = (offset >> 16) & 0xFFFF;
	idt[vector].offset_high = (uint32_t)((offset >> 32) & 0xFFFFFFFF);
	idt[vector].selector    = 0x08;    // kernel code segment
	idt[vector].ist         = 0;       // IST index 0
	idt[vector].type_attr   = 0x8E;    // present, DPL=0, interrupt gate (0xE)
	idt[vector].zero        = 0;
}

size_t idt_get_gate(uint8_t vector) {

	size_t offset = ((size_t)idt[vector].offset_high << 32) |
	                ((size_t)idt[vector].offset_mid << 16) |
	                idt[vector].offset_low;

	return offset;
}

void idt_reset_gate(uint8_t vector)
{
	idt_set_gate(vector, (size_t)(uintptr_t)idt_default_isr_amd64);
}

