// i386 (32-bit) IDT tanımı ve kurulum kodu
#include <arch.h>

// 32-bit IDT gate descriptor (Intel SDM cilt 3A, Bölüm 6)
typedef struct {
    uint16_t offset_low;   // ISR adresinin 0..15 bitleri
    uint16_t selector;     // Kod segment seçicisi (GDT)
    uint8_t  zero;         // Her zaman 0
    uint8_t  type_attr;    // Tür + DPL + P bitleri
    uint16_t offset_high;  // ISR adresinin 16..31 bitleri
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;        // Tablo boyutu - 1
    uint32_t base;         // Taban adresi
} __attribute__((packed)) idt_ptr_t;

idt_entry_t idt[256];
idt_ptr_t   idt_ptr;

extern void idt_default_isr();

void idt_set_gate(uint8_t vector, size_t offset)
{
    idt[vector].offset_low  = offset & 0xFFFF;
    idt[vector].offset_high = (offset >> 16) & 0xFFFF;
    idt[vector].selector    = 0x08;    // Çekirdek kod segmenti (GDT'de varsayılan)
    idt[vector].zero        = 0;
    idt[vector].type_attr   = 0x8E;    // Present | DPL=0 | 32-bit interrupt gate (0xE)
}

void idt_reset_gate(uint8_t vector)
{
    idt_set_gate(vector, (size_t)(uintptr_t)idt_default_isr);
}

void idt_init(void)
{
    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base  = (uint32_t)(uintptr_t)idt;

    // Tüm vektörlere varsayılan ISR ata
    for (int i = 0; i < 256; ++i) {
        idt_set_gate((uint8_t)i, (size_t)(uintptr_t)idt_default_isr);
    }

}
