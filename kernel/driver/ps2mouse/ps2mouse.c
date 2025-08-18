#include <arch.h>
#include <irq/IRQ.h>
#include <driver/DriverBase.h>
#include <debug/debug.h>
#include <driver/ps2mouse/ps2mouse.h>
#include <driver/ps2controller/ps2controller.h>

static bool enabled = false;
static uint8_t packet_buffer[3];
static size_t packet_index = 0;

extern int cursor_X;
extern int cursor_Y;
extern void ps2mouse_isr(void);

// Mouse'a komut gönder (controller'a değil)
static bool ps2mouse_send_command_to_device(uint8_t cmd) {
    ps2_controller_write_command(PS2_CMD_WRITE_TO_AUX);
    ps2_controller_write_data(cmd);
    
    int timeout = 1000;
    while (timeout-- > 0) {
        if (ps2_controller_wait_read()) {
            uint8_t response = ps2_controller_read_data();
            if (response == PS2_MOUSE_ACK) {
                return true;
            } else if (response == PS2_MOUSE_RESEND) {
                return false;
            }
        }
    }
    
    LOG("PS/2 Mouse: Command 0x%02X timeout\n", cmd);
    return false;
}

bool ps2mouse_init(void) {
    LOG("PS/2 mouse initializing...\n");
    
    // Controller başlatıldığından emin ol
    if (!ps2_controller_initialized) {
        if (!ps2_controller_init()) {
            LOG("PS/2 Mouse: Controller init failed!\n");
            return false;
        }
    }
    
    // Mouse port'u test et
    LOG("PS/2 Mouse: Testing mouse port...\n");
    ps2_controller_write_command(0xA9); // Test mouse port
    uint8_t test_result = ps2_controller_read_data();
    if (test_result != 0x00) {
        LOG("PS/2 Mouse: Port test failed: 0x%02X\n", test_result);
    }
    
    // Mouse port'unu etkinleştir
    ps2_controller_write_command(PS2_CMD_ENABLE_PORT2);
    
    // Controller config'i oku
    uint8_t config = ps2_controller_get_config();
    LOG("PS/2 Mouse: Initial config: 0x%02X\n", config);
    
    // ÖNEMLİ: Klavye ayarlarını KORU, sadece mouse bitlerini değiştir
    config |= 0x02;   // Bit 1 = Enable mouse interrupt
    config &= ~0x20;  // Bit 5 = Enable mouse clock (clear = enabled)
    // Klavye bitlerini koru
    config |= 0x01;   // Bit 0 = Keep keyboard interrupt enabled
    config &= ~0x10;  // Bit 4 = Keep keyboard clock enabled
    config &= ~0x40;  // Bit 6 = Keep translation disabled (raw scancode)
    
    ps2_controller_set_config(config);
    
    // Tekrar oku ve doğrula
    config = ps2_controller_get_config();
    LOG("PS/2 Mouse: After config update: 0x%02X\n", config);
    
    // Buffer'ı temizle
    ps2_controller_flush_buffer();
    
    // ÖNEMLİ: Mouse'u reset etmeden önce bir süre bekle
    for (int i = 0; i < 10000; i++) io_wait();
    
    // Mouse'u reset et
    LOG("PS/2 Mouse: Resetting mouse device...\n");
    ps2_controller_write_command(PS2_CMD_WRITE_TO_AUX);
    ps2_controller_write_data(PS2_MOUSE_CMD_RESET);
    
    // Reset yanıtlarını bekle (timeout'lu)
    int timeout = 100000;
    while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) {
        io_wait();
    }
    
    if (timeout > 0) {
        uint8_t ack = inb(PS2_DATA_PORT);
        LOG("PS/2 Mouse: Reset ACK: 0x%02X\n", ack);
        
        if (ack == PS2_MOUSE_ACK) {
            // Self-test result bekle
            timeout = 100000;
            while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) {
                io_wait();
            }
            if (timeout > 0) {
                uint8_t test = inb(PS2_DATA_PORT);
                LOG("PS/2 Mouse: Self-test result: 0x%02X\n", test);
                
                // Device ID bekle
                timeout = 100000;
                while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) {
                    io_wait();
                }
                if (timeout > 0) {
                    uint8_t id = inb(PS2_DATA_PORT);
                    LOG("PS/2 Mouse: Device ID: 0x%02X\n", id);
                }
            }
        }
    }
    
    // Defaults ayarla
    LOG("PS/2 Mouse: Setting defaults...\n");
    ps2_controller_write_command(PS2_CMD_WRITE_TO_AUX);
    ps2_controller_write_data(PS2_MOUSE_CMD_SET_DEFAULTS);
    timeout = 10000;
    while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) io_wait();
    if (timeout > 0) inb(PS2_DATA_PORT); // ACK'i oku
    
    // Sample rate ayarla
    ps2_controller_write_command(PS2_CMD_WRITE_TO_AUX);
    ps2_controller_write_data(0xF3); // Set sample rate
    timeout = 10000;
    while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) io_wait();
    if (timeout > 0) inb(PS2_DATA_PORT); // ACK
    
    ps2_controller_write_command(PS2_CMD_WRITE_TO_AUX);
    ps2_controller_write_data(60); // 60 samples/sec (daha düşük değer dene)
    timeout = 10000;
    while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) io_wait();
    if (timeout > 0) inb(PS2_DATA_PORT); // ACK
    
    // IDT'ye interrupt handler ekle
    LOG("PS/2 Mouse: Setting up IRQ12 handler...\n");
    irq_controller->register_handler(12, ps2mouse_isr);
    
    // PIC'de IRQ12'yi mask et
    irq_controller->disable(12);
    
    // Son config kontrolü
    config = ps2_controller_get_config();
    LOG("PS/2 Mouse: Config before data reporting: 0x%02X\n", config);
    
    // Data reporting'i aç
    LOG("PS/2 Mouse: Enabling data reporting...\n");
    ps2_controller_write_command(PS2_CMD_WRITE_TO_AUX);
    ps2_controller_write_data(PS2_MOUSE_CMD_ENABLE_REPORTING);
    timeout = 10000;
    while (timeout-- > 0 && !(inb(PS2_STATUS_PORT) & 0x01)) io_wait();
    if (timeout > 0) {
        uint8_t response = inb(PS2_DATA_PORT);
        LOG("PS/2 Mouse: Enable reporting response: 0x%02X\n", response);
    }
    
    // Buffer'ı son kez temizle
    while (inb(PS2_STATUS_PORT) & 0x01) {
        inb(PS2_DATA_PORT);
    }
    
    // Packet index'i sıfırla
    packet_index = 0;
    
    // Son config durumu
    config = ps2_controller_get_config();
    LOG("PS/2 Mouse: Final config: 0x%02X (should be 0x03 or similar)\n", config);

    return true;
}

void ps2mouse_enable(void) {
    if (!enabled) {
        
        // Data reporting'i tekrar aç (emin olmak için)
        ps2mouse_send_command_to_device(PS2_MOUSE_CMD_ENABLE_REPORTING);
        
        // IRQ12'nin açık olduğundan emin ol
        irq_controller->enable(12);
        
        // Controller config'i kontrol et
        uint8_t config = ps2_controller_get_config();
        config |= 0x02;  // Mouse interrupt enable
        config &= ~0x20; // Mouse clock enable
        ps2_controller_set_config(config);
        
        enabled = true;
        LOG("PS/2 Mouse: Enabled (config: 0x%02X)\n", ps2_controller_get_config());
    }
}

void ps2mouse_disable(void) {
    if (enabled) {
        ps2mouse_send_command_to_device(PS2_MOUSE_CMD_DISABLE_REPORTING);
        irq_controller->enable(12);
        enabled = false;
        LOG("PS/2 Mouse: Disabled\n");
    }
}

bool ps2mouse_enabled(void) {
    return enabled;
}

void ps2mouse_isr_handler(void) {
    LOG("MOUSE INTERRUPT!\n"); // İlk önce buraya geldiğini görelim
    
    // Status register'ı oku
    uint8_t status = inb(PS2_STATUS_PORT);
    LOG("Mouse ISR: Status = 0x%02X\n", status);
    
    // Veri var mı kontrol et
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
        LOG("Mouse ISR: No data available\n");
        return;
    }
    
    // Veriyi oku
    uint8_t data = inb(PS2_DATA_PORT);
    LOG("Mouse ISR: Data = 0x%02X, AUX bit = %d\n", 
            data, (status & PS2_STATUS_AUX_DATA) ? 1 : 0);
    
    // Mouse verisi mi kontrol et
    if (!(status & PS2_STATUS_AUX_DATA)) {
        LOG("Mouse ISR: Data is not from mouse\n");
        return;
    }
    
    // Packet topla
    packet_buffer[packet_index++] = data;
    LOG("Mouse ISR: Packet byte %d = 0x%02X\n", packet_index - 1, data);
    
    if (packet_index >= 3) {
        uint8_t flags = packet_buffer[0];
        
        if (flags & PS2_MOUSE_ALWAYS_1) {
            int8_t delta_x = packet_buffer[1];
            int8_t delta_y = packet_buffer[2];
            
            if (!(flags & (PS2_MOUSE_X_OVERFLOW | PS2_MOUSE_Y_OVERFLOW))) {
                if (flags & PS2_MOUSE_X_SIGN) {
                    delta_x |= 0xFFFFFF00;
                }
                if (flags & PS2_MOUSE_Y_SIGN) {
                    delta_y |= 0xFFFFFF00;
                }
                
                delta_y = -delta_y;
                
                cursor_X += delta_x;
                cursor_Y += delta_y;
                
                LOG("Mouse moved: X=%d Y=%d (dx=%d dy=%d)\n", 
                        cursor_X, cursor_Y, delta_x, delta_y);
            }
        }
        
        packet_index = 0;
    }
}

DriverBase ps2mouse_driver = {
    .name = "PS/2 Mouse Driver",
    .version = 1,
    .context = NULL,
    .init = ps2mouse_init,
    .enable = ps2mouse_enable,
    .disable = ps2mouse_disable,
    .enabled = false,
    .type = DRIVER_TYPE_HID
};