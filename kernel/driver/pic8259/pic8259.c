#include <driver/DriverBase.h>
#include <irq/IRQ.h>
#include <arch.h>
#include <debug/debug.h>

size_t pic8259_irq2_isr_addr;

extern IRQController pic8259_irq_controller;
extern DriverBase pic8259_driver;

extern void pic8259_master_default_isr();
extern void pic8259_slave_default_isr();

static inline uint8_t get_active_slave_irq() {
    // Slave PIC Command Port = 0xA0
    // OCW3: Read ISR (0x0B)
    outb(0xA0, 0x0B);
    uint8_t isr = inb(0xA0);

    // isr'nin hangi biti 1 ise onun indexi + 8 = gerçek IRQ numarası
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (isr & (1 << bit)) {
            return 8 + bit;
        }
    }

    // Hiçbiri aktif değilse 0xFF gibi bir şey dönebiliriz
    return 0xFF;
}

bool pic8259_init() {
    // PIC8259'ları başlatma işlemleri
    // Master PIC Command Port = 0x20
    // Slave PIC Command Port = 0xA0

    // 1. PIC'leri resetle
    outb(0x20, 0x11); // Master PIC
    outb(0xA0, 0x11); // Slave PIC

    // 2. IRQ tablosunu ayarla
    outb(0x20, 0x20); // Master PIC IRQ başlangıç vektörü
    outb(0xA0, 0x28); // Slave PIC IRQ başlangıç vektörü

    // 3. Slave PIC'i Master'a bağla
    outb(0x21, 0x04);  // Master PIC: Slave on IRQ2
    outb(0xa1, 0x02);  // Slave PIC: Cascade identity

    // 4. Modu ayarla (8086/88 mod)
    outb(0x20, 0x01); // Master PIC
    outb(0xA0, 0x01); // Slave PIC

    // 5. Maskeleri doldur (tüm IRQ'lar pasif)
    outb(0x21, 0xFF); // Master PIC maskesi
    outb(0xA1, 0xFF); // Slave PIC maskesi

    for (uint8_t i = 0; i < 8; i++) {
        idt_set_gate(32 + i, (uintptr_t)pic8259_master_default_isr); // Master PIC için varsayılan ISR
    }

    for (uint8_t i = 0; i < 8; i++) {
        idt_set_gate(40 + i, (uintptr_t)pic8259_slave_default_isr); // Slave PIC için varsayılan ISR
    }

    // Send EOI command to both PICs
    outb(0x20, 0x20); // Master PIC EOI
    outb(0xA0, 0x20); // Slave PIC EOI

    irq_controller = &pic8259_irq_controller;
    pic8259_driver.enabled = true;

    return true;

}

static uint8_t master_mask = 0xFF; // Master PIC maskesi
static uint8_t slave_mask = 0xFF; // Slave PIC maskesi

void pic8259_enable() {

    // IRQ durum yeniden ayarlanacak
    outb(0x21, master_mask); // Master PIC maskesi
    outb(0xA1, slave_mask); // Slave PIC maskesi

    pic8259_driver.enabled = true;
}

void pic8259_disable() {
    // Tüm IRQ'ları kaydet

    master_mask = inb(0x21);
    slave_mask = inb(0xA1);

    // Tüm IRQ'ları pasifleştir

    outb(0x21, 0xFF); // Master PIC maskesi
    outb(0xA1, 0xFF); // Slave PIC maskesi

    pic8259_driver.enabled = false;
}

void pic8259_enable_irq(uint32_t irq) {
    if (irq > 15) {
        return; // Geçersiz IRQ numarası
    }
    if (irq < 8) {
        // Master PIC
        uint8_t mask = inb(0x21); // Mevcut maskeyi al
        outb(0x21, mask & ~(1 << irq)); // IRQ'yu aktif et
    } else {
        // Slave PIC
        uint8_t mask = inb(0xA1); // Mevcut maskeyi al
        outb(0xA1, mask & ~(1 << (irq - 8))); // IRQ'yu aktif et
    }
}

void pic8259_disable_irq(uint32_t irq) {
    if (irq > 15) {
        return; // Geçersiz IRQ numarası
    }
    if (irq < 8) {
        // Master PIC
        uint8_t mask = inb(0x21); // Mevcut maskeyi al
        outb(0x21, mask | (1 << irq)); // IRQ'yu pasif et
    } else {
        // Slave PIC
        uint8_t mask = inb(0xA1); // Mevcut maskeyi al
        outb(0xA1, mask | (1 << (irq - 8))); // IRQ'yu pasif et
    }
}

void pic8259_acknowledge_irq(uint32_t irq) {
    if (irq > 15) {
        return; // Geçersiz IRQ numarası
    }

    if (irq >= 8) outb(0xA0, 0x20); // Acknowledge Slave PIC
    outb(0x20, 0x20); // Acknowledge Master PIC
}

void pic8259_set_priority(uint32_t irq, uint8_t priority) {
    // PIC8259'da öncelik ayarlama işlemi genellikle desteklenmez
    // Bu fonksiyon boş bırakılabilir veya hata verebilir
}

uint8_t pic8259_get_priority(uint32_t irq) {
    // PIC8259'da öncelik alma işlemi genellikle desteklenmez
    // Bu fonksiyon boş bırakılabilir veya hata verebilir
    return 0; // Varsayılan olarak 0 döndür
}

bool pic8259_is_enabled(uint32_t irq) {
    if (irq > 15) {
        return false; // Geçersiz IRQ numarası
    }
    if (irq < 8) {
        // Master PIC
        uint8_t mask = inb(0x21); // Mevcut maskeyi al
        return !(mask & (1 << irq)); // Eğer bit set değilse aktif
    } else {
        // Slave PIC
        uint8_t mask = inb(0xA1); // Mevcut maskeyi al
        return !(mask & (1 << (irq - 8))); // Eğer bit set değilse aktif
    }
}

void pic8259_register_handler(uint32_t irq, void (*handler)(void)) {
    // PIC8259'da IRQ handler kaydetme işlemi genellikle desteklenmez
    // Bu fonksiyon boş bırakılabilir veya hata verebilir
    // Donanım IRQ'ları, PIC remap sırasında 0x20 (master) ve 0x28 (slave)
    // taban vektörlerine taşınır. Doğru IDT girişini programlamak için
    // gerçek CPU vektörünü hesaplayın.

    if (irq > 16)
    {
        ERROR("Invalid IRQ number for registration: %u", irq);
        return; // Geçersiz IRQ numarası
    }

    idt_set_gate(32 + irq, (uintptr_t)handler); // 0x8E = Present, DPL=0, Type=Interrupt Gate
}

void pic8259_unregister_handler(uint32_t irq) {
    // PIC8259'da IRQ handler kaldırma işlemi genellikle desteklenmez
    // Bu fonksiyon boş bırakılabilir veya hata verebilir
    if (irq < 8) idt_set_gate(32 + irq, (uintptr_t)pic8259_master_default_isr); // Varsayılan ISR'yi ayarla
    else
    if (irq < 16) idt_set_gate(32 + irq, (uintptr_t)pic8259_slave_default_isr); // Varsayılan ISR'yi ayarla
    else {
        ERROR("Invalid IRQ number for unregistration: %u", irq);
        return; // Geçersiz IRQ numarası
    }
}

// IRQ controller wrapper fonksiyonu
static inline void pic8259_init_irq_controller() {
    // IRQ kontrolcüsünü başlatma işlemleri
    pic8259_init();
}

void pic8259_irq2_isr_handler() {
    // IRQ2 için özel ISR
    // Slave PIC'den gelen IRQ'ları işleme
    uint8_t active_irq = get_active_slave_irq();
    
    pic8259_irq2_isr_addr = idt_get_gate(32 + active_irq);

}

DriverBase pic8259_driver = {
    .name = "PIC8259",
    .version = 1,
    .context = NULL,
    .enabled = false,
    .init = pic8259_init, // PIC8259 init fonksiyonu burada tanımlanacak
    .enable = pic8259_enable, // PIC8259 enable fonksiyonu burada tanımlanacak
    .disable = pic8259_disable, // PIC8259 disable fonksiyonu burada tanımlanacak
    .type = DRIVER_TYPE_ANY
};

IRQController pic8259_irq_controller = {

    .name = "PIC8259 IRQ Controller",
    .specific_data = NULL,

    .init = pic8259_init_irq_controller,
    .enable = pic8259_enable_irq,
    .disable = pic8259_disable_irq,
    .acknowledge = pic8259_acknowledge_irq, // Acknowledge fonksiyonu henüz tanımlanmadı
    .set_priority = pic8259_set_priority, // Priority ayarlama fonksiyonu henüz tanımlanmadı
    .get_priority = pic8259_get_priority, // Priority alma fonksiyonu henüz tanımlanmadı
    .is_enabled = pic8259_is_enabled, // IRQ'nun aktif olup olmadığını kontrol etme fonksiyonu henüz tanımlanmadı
    .register_handler = pic8259_register_handler, // IRQ handler kaydetme fonksiyonu henüz tanımlanmadı
    .unregister_handler = pic8259_unregister_handler // IRQ handler kaldırma fonksiyonu henüz tanımlanmadı
};
