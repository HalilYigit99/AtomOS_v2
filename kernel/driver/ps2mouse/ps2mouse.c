#include <arch.h>
#include <irq/IRQ.h>
#include <driver/DriverBase.h>
#include <debug/debug.h>
#include <driver/ps2mouse/ps2mouse.h>

#define IRQ_PS2_MOUSE 12 // IRQ numarası, genelde 12 (IRQ12) PS/2 mouse için kullanılır

static bool enabled = false;
static uint8_t packet_buffer[3];
static size_t packet_index = 0;

extern int cursor_X;
extern int cursor_Y;
extern void ps2mouse_isr(void);

// PS/2 Controller helper'ları (klavyeyi etkilemeden güvenli erişim)
#define PS2_STATUS_OBF 0x01   // Output Buffer Full
#define PS2_STATUS_IBF 0x02   // Input Buffer Full
#define PS2_STATUS_AUX 0x20   // 1=mouse (AUX) verisi

static bool ps2_wait_input_clear(uint32_t limit)
{
    while (limit--) {
        if ((inb(0x64) & PS2_STATUS_IBF) == 0)
            return true;
        io_wait();
    }
    return false;
}

static bool ps2_wait_output_full(uint32_t limit)
{
    while (limit--) {
        if (inb(0x64) & PS2_STATUS_OBF)
            return true;
        io_wait();
    }
    return false;
}

static void ps2_flush_output(void)
{
    // Mevcut tüm baytları boşalt (kaynak fark etmeksizin)
    for (int i = 0; i < 32; ++i) {
        if (inb(0x64) & PS2_STATUS_OBF) {
            (void)inb(0x60);
            io_wait();
        } else {
            break;
        }
    }
}

static bool ps2_write_cmd(uint8_t cmd)
{
    if (!ps2_wait_input_clear(100000)) return false;
    outb(0x64, cmd);
    return true;
}

static bool ps2_write_data(uint8_t data)
{
    if (!ps2_wait_input_clear(100000)) return false;
    outb(0x60, data);
    return true;
}

static bool ps2_write_aux(uint8_t data)
{
    // İkinci port (AUX) için: önce 0xD4 komut portuna, sonra veri portuna komut
    if (!ps2_write_cmd(0xD4)) return false;
    return ps2_write_data(data);
}

static bool ps2_read_aux(uint8_t* out, uint32_t limit)
{
    while (limit--) {
        uint8_t st = inb(0x64);
        if ((st & (PS2_STATUS_OBF | PS2_STATUS_AUX)) == (PS2_STATUS_OBF | PS2_STATUS_AUX)) {
            *out = inb(0x60);
            return true;
        }
        io_wait();
    }
    return false;
}

static bool ps2_expect_aux(uint8_t expected, uint32_t limit)
{
    uint8_t b;
    if (!ps2_read_aux(&b, limit)) return false;
    return b == expected;
}

bool ps2mouse_init(void) {
    LOG("PS/2 mouse initializing...");

    // Çıkış tamponunu temizle (karma veri kalmasın)
    ps2_flush_output();

    // İkinci PS/2 portunu test et (0xA9)
    if (!ps2_write_cmd(0xA9)) {
        WARN("PS/2 Mouse: Controller cmd (test port 2) failed");
        return false;
    }
    if (!ps2_wait_output_full(100000)) {
        WARN("PS/2 Mouse: No response to port 2 test");
        return false;
    }
    uint8_t response = inb(0x60);
    if (response != 0x00) {
        WARN("PS/2 Mouse: Second PS/2 port test failed (code=%02x)", response);
        return false;
    }

    // Enable the second PS/2 port (mouse)
    if (!ps2_write_cmd(0xA8)) {
        WARN("PS/2 Mouse: Failed to enable second port");
        return false;
    }

    // Get PS/2 controller configuration byte
    if (!ps2_write_cmd(0x20)) {
        WARN("PS/2 Mouse: Failed to read config byte");
        return false;
    }
    if (!ps2_wait_output_full(100000)) {
        WARN("PS/2 Mouse: No config byte available");
        return false;
    }
    uint8_t config = inb(0x60);

    // Enable mouse interrupt (IRQ12) and clear disable mouse clock bit
    config |= (1 << 1); // Enable IRQ12
    config &= ~(1 << 5); // Enable Mouse Clock
    
    // Write the new configuration byte
    if (!ps2_write_cmd(0x60)) {
        WARN("PS/2 Mouse: Failed to write config index");
        return false;
    }
    if (!ps2_write_data(config)) {
        WARN("PS/2 Mouse: Failed to write config byte");
        return false;
    }

    // Reset the mouse
    if (!ps2_write_aux(0xFF)) {
        WARN("PS/2 Mouse: Failed to send reset to AUX");
        return false;
    }
    // ACK (0xFA)
    if (!ps2_expect_aux(0xFA, 200000)) {
        WARN("PS/2 Mouse: No ACK for reset");
        return false;
    }
    // Self-test (0xAA) ve opsiyonel device ID (0x00)
    uint8_t b = 0x00;
    if (!ps2_read_aux(&b, 200000) || b != 0xAA) {
        WARN("PS/2 Mouse: Self-test failed (got %02x)", b);
        return false;
    }
    // Bazı fareler 0x00 aygıt ID'si yollar; varsa oku ama zorunlu değil
    if (ps2_wait_output_full(1000)) {
        uint8_t st = inb(0x64);
        if ((st & (PS2_STATUS_OBF | PS2_STATUS_AUX)) == (PS2_STATUS_OBF | PS2_STATUS_AUX)) {
            (void)inb(0x60); // device id (genellikle 0x00)
        }
    }

    // Varsayılan ayarlar (F6)
    if (!ps2_write_aux(0xF6) || !ps2_expect_aux(0xFA, 200000)) {
        WARN("PS/2 Mouse: Failed to set defaults");
        return false;
    }

    // Register the interrupt handler
    irq_controller->disable(IRQ_PS2_MOUSE); // Initially disable IRQ12
    irq_controller->register_handler(IRQ_PS2_MOUSE, ps2mouse_isr);

    LOG("PS/2 Mouse: Initialization successful.");

    return true;
}

void ps2mouse_enable(void) {
    if (!enabled) {
        // Veri raporlamayı aç ve ACK'yi kesme açmadan önce tüket
        for (int tries = 0; tries < 3; ++tries) {
            if (!ps2_write_aux(0xF4)) continue; // Enable data reporting
            uint8_t ack;
            if (ps2_read_aux(&ack, 200000) && ack == 0xFA) {
                break;
            }
        }

        packet_index = 0;
        enabled = true;

        // IRQ12'yi en son aç (ACK'ler işlendi)
        irq_controller->enable(IRQ_PS2_MOUSE);
    }
}

void ps2mouse_disable(void) {
    if (enabled) {

        // Disable data reporting
        for (int tries = 0; tries < 3; ++tries) {
            if (!ps2_write_aux(0xF5)) continue; // Disable data reporting
            uint8_t ack;
            if (ps2_read_aux(&ack, 200000) && ack == 0xFA) {
                break;
            }
        }

        // Disable irq
        irq_controller->disable(IRQ_PS2_MOUSE);

        enabled = false;
    }
}

bool ps2mouse_enabled(void) {
    return enabled;
}

void ps2mouse_isr_handler(void) {
    
    // Status register'ı oku
    uint8_t status = inb(PS2_STATUS_PORT);
    
    // Veri var mı kontrol et
    if (!(status & PS2_STATUS_OUTPUT_FULL)) {
        goto _ret;
    }
    
    // Veriyi oku
    uint8_t data = inb(PS2_DATA_PORT);

    // Mouse verisi mi kontrol et
    if (!(status & PS2_STATUS_AUX_DATA)) {
        goto _ret;
    }
    
    // Packet topla
    packet_buffer[packet_index++] = data;
    
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
                
            }
        }
        
        packet_index = 0;
    }

_ret:
    irq_controller->acknowledge(IRQ_PS2_MOUSE);
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