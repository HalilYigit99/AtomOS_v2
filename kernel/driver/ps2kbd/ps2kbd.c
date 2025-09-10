#include <driver/DriverBase.h>
#include <irq/IRQ.h>
#include <buffer.h>
#include <keyboard/Keyboard.h>
#include <list.h>
#include <stream/InputStream.h>
#include <arch.h>
#include <memory/memory.h>
#include <stream/OutputStream.h>
#include <debug/debug.h>
#include <sleep.h>

#define IRQ_PS2_KEYBOARD 1 // IRQ numarası, genelde 1 (IRQ1) PS/2 klavye için kullanılır

// PS/2 Klavye Komutları (bunlar klavyeye özel, controller'a değil)
#define PS2_KBD_CMD_RESET           0xFF
#define PS2_KBD_CMD_ENABLE          0xF4
#define PS2_KBD_CMD_DISABLE         0xF5
#define PS2_KBD_CMD_SET_SCANCODE    0xF0

// PS/2 Response Kodları
#define PS2_RESPONSE_ACK            0xFA
#define PS2_RESPONSE_RESEND         0xFE
#define PS2_RESPONSE_SELF_TEST_OK   0xAA

// 8042 Controller ports and status bits
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_CMD_PORT     0x64

#define PS2_STATUS_OBF   0x01 // Output buffer full
#define PS2_STATUS_IBF   0x02 // Input buffer full
#define PS2_STATUS_AUX   0x20 // 1 = AUX (mouse) data

// 8042 Controller commands
#define PS2_CMD_READ_CONFIG   0x20
#define PS2_CMD_WRITE_CONFIG  0x60
#define PS2_CMD_ENABLE_PORT1  0xAE
// Extra 8042 commands used during init sequencing
#define PS2_CMD_DISABLE_PORT1 0xAD
#define PS2_CMD_DISABLE_PORT2 0xA7
#define PS2_CMD_TEST_CTRL     0xAA
#define PS2_CMD_TEST_PORT1    0xAB

extern List* keyboardInputStreamList; // Global list to hold keyboard input streams

Buffer* ps2_event_buffer = NULL; // Buffer for PS/2 keyboard events

static bool ps2kbd_init(void);
static void ps2kbd_enable(void);
static void ps2kbd_disable(void);

static bool ps2kbd_initalized;

static int ps2kbd_stream_open();
static void ps2kbd_stream_close();
static int ps2kbd_stream_readChar(char* c);
static int ps2kbd_stream_readString(char* str, size_t maxLength);
static int ps2kbd_stream_readBuffer(void* buffer, size_t size);
static int ps2kbd_stream_available();
static char ps2kbd_stream_peek();
static void ps2kbd_stream_flush();
static bool ps2_kbd_wait_ack(uint32_t timeout_ms);

void ps2kbd_handler();

InputStream ps2kbdInputStream = {
    .Open = ps2kbd_stream_open,
    .Close = ps2kbd_stream_close,
    .readChar = ps2kbd_stream_readChar,
    .readString = ps2kbd_stream_readString,
    .readBuffer = ps2kbd_stream_readBuffer,
    .available = ps2kbd_stream_available,
    .peek = ps2kbd_stream_peek,
    .flush = ps2kbd_stream_flush
};

DriverBase ps2kbd_driver = {
    .name = "PS/2 Keyboard Driver",
    .context = NULL,
    .enabled = false,
    .version = 0,
    .init = ps2kbd_init,
    .enable = ps2kbd_enable,
    .disable = ps2kbd_disable,
    .type = DRIVER_TYPE_HID
};

extern void ps2kbd_isr(void); // PS/2 keyboard interrupt service routine
void ps2kbd_handle();

extern bool __kbd_abstraction_initialized; // Flag to check if keyboard abstraction layer is initialized

static bool scancodeSetCorrect = false;
static uint8_t scancodeSetRetryCount = 0;

// --- Local 8042 helpers (keyboard/port1 safe) ---
static bool ps2_wait_input_clear_ms(uint32_t timeout_ms)
{
    while (timeout_ms--) {
        if ((inb(PS2_STATUS_PORT) & PS2_STATUS_IBF) == 0) return true;
        sleep_ms(1);
    }
    return false;
}

static bool ps2_wait_output_full_ms(uint32_t timeout_ms)
{
    while (timeout_ms--) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) return true;
        sleep_ms(1);
    }
    return false;
}

static void ps2_flush_output(void)
{
    for (int i = 0; i < 32; ++i) {
        if (inb(PS2_STATUS_PORT) & PS2_STATUS_OBF) {
            (void)inb(PS2_DATA_PORT);
            // Give the controller a brief moment between reads
            sleep_ms(1);
        } else {
            break;
        }
    }
}

static bool ps2_write_cmd(uint8_t cmd)
{
    if (!ps2_wait_input_clear_ms(100)) return false; // ~100ms timeout
    outb(PS2_CMD_PORT, cmd);
    return true;
}

static bool ps2_write_data(uint8_t data)
{
    if (!ps2_wait_input_clear_ms(100)) return false; // ~100ms timeout
    outb(PS2_DATA_PORT, data);
    return true;
}

// Read byte that must be from keyboard (AUX=0)
static bool ps2_read_kbd(uint8_t* out, uint32_t timeout_ms)
{
    while (timeout_ms--) {
        uint8_t st = inb(PS2_STATUS_PORT);
        if ((st & (PS2_STATUS_OBF)) && ((st & PS2_STATUS_AUX) == 0)) {
            *out = inb(PS2_DATA_PORT);
            return true;
        }
        sleep_ms(1);
    }
    return false;
}

static bool __attribute__((unused)) ps2_expect_kbd(uint8_t expected, uint32_t limit)
{
    uint8_t b;
    if (!ps2_read_kbd(&b, limit)) return false;
    return b == expected;
}

// Send a byte to the keyboard and robustly wait for ACK, handling RESEND.
static bool ps2_kbd_send_expect_ack(uint8_t data)
{
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (!ps2_write_data(data)) return false;

        // Wait up to ~250ms for a response; handle RESEND
        uint8_t resp;
        if (ps2_read_kbd(&resp, 250)) {
            if (resp == PS2_RESPONSE_ACK) return true;
            if (resp == PS2_RESPONSE_RESEND) continue; // try again
            // Ignore any stray bytes and retry
        }
        // try to resend
    }
    return false;
}

static uint8_t ps2_kbd_getScanCodeSet()
{
    ps2_flush_output();

    outb(PS2_DATA_PORT, PS2_KBD_CMD_SET_SCANCODE);
    io_wait();
    outb(PS2_DATA_PORT, 0x00); // 0 = get current set
    ps2_kbd_wait_ack(100);

    sleep_ms(1);

    uint8_t current = 0xFF;
    if (ps2_read_kbd(&current, 250)) {
        return current;
    }
    LOG("PS/2 Keyboard: failed to get current scancode set ( timeout )");
    return 0xFF;
}

static void ps2_kbd_setScanCodeSet(uint8_t set)
{
    ps2_flush_output();

    outb(PS2_DATA_PORT, PS2_KBD_CMD_SET_SCANCODE);
    io_wait();
    outb(PS2_DATA_PORT, set);

    sleep_ms(1);
}

static bool ps2_kbd_wait_ack(uint32_t timeout_ms)
{
    uint8_t resp;
    if (ps2_read_kbd(&resp, timeout_ms)) {
        return resp == PS2_RESPONSE_ACK;
    }
    return false;
}

static bool ps2kbd_init(void) {

    ps2kbd_initalized = false;
    scancodeSetCorrect = false;
    scancodeSetRetryCount = 0;

    // Ensure keyboard abstraction layer is up
    if (!__kbd_abstraction_initialized) {
        if (keyboardInputStream.Open) {
            if (keyboardInputStream.Open() != 0) return false;
        } else {
            return false;
        }
    }

    // Allocate event buffer once
    if (!ps2_event_buffer) {
        ps2_event_buffer = buffer_create(sizeof(KeyboardKeyEventData));
    }
    if (!ps2_event_buffer) {
        LOG("Failed to create PS/2 keyboard event buffer.");
        return false;
    }

    LOG("Initializing PS/2 keyboard...");

    // 1) Quiesce controller: disable both ports and flush any pending data
    (void)ps2_write_cmd(PS2_CMD_DISABLE_PORT1);
    (void)ps2_write_cmd(PS2_CMD_DISABLE_PORT2);
    ps2_flush_output();

    // 2) Read and update Controller Configuration Byte (CCB)
    uint8_t ccb = 0;
    if (!ps2_write_cmd(PS2_CMD_READ_CONFIG) || !ps2_wait_output_full_ms(100)) {
        WARN("PS/2: Unable to read CCB");
        return false;
    }
    ccb = inb(PS2_DATA_PORT);

    // Disable IRQs and translation during configuration
    ccb &= ~(1 << 0); // IRQ1 off
    ccb &= ~(1 << 1); // IRQ12 off
    ccb &= ~(1 << 6); // translation off
    if (!ps2_write_cmd(PS2_CMD_WRITE_CONFIG) || !ps2_write_data(ccb)) {
        WARN("PS/2: Unable to write CCB");
        return false;
    }

    // 3) Controller self-test (non-fatal)
    if (ps2_write_cmd(PS2_CMD_TEST_CTRL)) {
        if (ps2_wait_output_full_ms(250)) {
            uint8_t st = inb(PS2_DATA_PORT);
            if (st != 0x55) {
                LOG("PS/2: Controller self-test 0x%02X", st);
            }
        }
    }

    // 4) Interface test for port1 (non-fatal)
    if (ps2_write_cmd(PS2_CMD_TEST_PORT1) && ps2_wait_output_full_ms(100)) {
        uint8_t pt = inb(PS2_DATA_PORT);
        if (pt != 0x00) {
            LOG("PS/2: Port1 interface test 0x%02X", pt);
        }
    }

    // 5) Enable port1 (keyboard)
    (void)ps2_write_cmd(PS2_CMD_ENABLE_PORT1);
    sleep_ms(1);
    ps2_flush_output();

    // 6) Reset keyboard and wait for BAT (0xAA)
    if (!ps2_kbd_send_expect_ack(PS2_KBD_CMD_RESET)) {
        WARN("PS/2 Keyboard: reset not ACKed");
        return false;
    }
    {
        uint8_t b = 0;
        bool got_bat = false;
        for (int i = 0; i < 1000; ++i) {
            if (ps2_read_kbd(&b, 1)) {
                if (b == PS2_RESPONSE_SELF_TEST_OK) { got_bat = true; break; }
            }
            sleep_ms(1);
        }
        if (!got_bat) {
            LOG("PS/2 Keyboard: no BAT after reset (continuing)");
        } else {
            // Some keyboards send an extra 0x00 after BAT; drain one byte if present
            uint8_t extra;
            if (ps2_read_kbd(&extra, 2)) {
                (void)extra; // ignore
            }
        }
    }

    // 7) Ensure scanning disabled while configuring
    (void)ps2_kbd_send_expect_ack(PS2_KBD_CMD_DISABLE);

    // 8) Set scancode set 2 and verify with small retry loop
    for (scancodeSetRetryCount = 0; scancodeSetRetryCount < 3 && !scancodeSetCorrect; ++scancodeSetRetryCount) {
        ps2_kbd_setScanCodeSet(2);
        ps2_kbd_wait_ack(100);
        uint8_t set = ps2_kbd_getScanCodeSet();
        if (set == 2) {
            scancodeSetCorrect = true;
            break;
        } else {
            LOG("PS/2 Keyboard: scancode set verify failed (got 0x%02X, expected 0x02), retrying...", set);
        }
    }
    if (!scancodeSetCorrect) {
        WARN("PS/2 Keyboard: failed to verify scancode set 2");
    }

    // 9) Enable scanning
    if (!ps2_kbd_send_expect_ack(PS2_KBD_CMD_ENABLE)) {
        WARN("PS/2 Keyboard: failed to enable scanning");
        return false;
    }

    // 10) Wire up IRQ handler but keep line disabled; enable happens in enable()
    irq_controller->register_handler(IRQ_PS2_KEYBOARD, ps2kbd_isr);
    irq_controller->disable(IRQ_PS2_KEYBOARD);

    ps2kbd_initalized = true;
    return true;
}

void ps2kbd_enable(void) {

    if (!ps2kbd_initalized) {
        return;
    }

    if (ps2_event_buffer == NULL) {
        return;
    }

    if (keyboardInputStreamList == NULL) {
        return;
    }

    if (ps2kbd_driver.enabled) {
        return;
    }

    // Ensure 8042 CCB has IRQ1 enabled now that handler is registered
    if (ps2_write_cmd(PS2_CMD_READ_CONFIG) && ps2_wait_output_full_ms(100)) {
        uint8_t cfg = inb(PS2_DATA_PORT);
        cfg |= (1 << 0); // enable first port IRQ
        // keep translation disabled
        cfg &= ~(1 << 6);
        // ensure clock enabled
        cfg &= ~(1 << 4);
        (void)ps2_write_cmd(PS2_CMD_WRITE_CONFIG);
        (void)ps2_write_data(cfg);
    }

    irq_controller->enable(IRQ_PS2_KEYBOARD); // IRQ1'i etkinleştir
    List_Add(keyboardInputStreamList, &ps2kbdInputStream);
    ps2kbd_driver.enabled = true;
}

void ps2kbd_disable(void) {
    ps2kbd_driver.enabled = false;

    if (keyboardInputStreamList != NULL) {
        List_Remove(keyboardInputStreamList, &ps2kbdInputStream);
    }

    buffer_clear(ps2_event_buffer);
}

extern KeyboardLayouts currentLayout;
extern void __ps2kbd_us_qwerty_handle(uint8_t scancode);
extern void __ps2kbd_tr_qwerty_handle(uint8_t scancode);
extern void __ps2kbd_tr_f_handle(uint8_t scancode);

void ps2kbd_handler() {

    if (!ps2_event_buffer) {
        goto _ret;
    }

    char scancode = inb(0x60); // PS/2 data port

    if (currentLayout == LAYOUT_US_QWERTY) {
        __ps2kbd_us_qwerty_handle(scancode);
    } else if (currentLayout == LAYOUT_TR_QWERTY) {
        __ps2kbd_tr_qwerty_handle(scancode);
    } else if (currentLayout == LAYOUT_TR_F) {
        __ps2kbd_tr_f_handle(scancode);
    } else {
        goto _ret;
    }

_ret:
    irq_controller->acknowledge(IRQ_PS2_KEYBOARD);
}

static int ps2kbd_stream_open() {
    return 0;
}

static void ps2kbd_stream_close() {
}

static int ps2kbd_stream_readChar(char* c) {
    if (!c || !ps2_event_buffer) {
        return -1;
    }

    if (buffer_count(ps2_event_buffer) == 0) {
        return -1;
    }

    KeyboardKeyEventData* event;

    do {
        event = (KeyboardKeyEventData*)buffer_pop(ps2_event_buffer);
        if (!event) {
            return -1;
        }
    } while (event->isPressed == false || event->key == KEY_UNKNOWN);

    *c = event->ascii;
    free(event);
    return 1;
}

static int ps2kbd_stream_readString(char* str, size_t maxLength) {
    if (!str || maxLength == 0 || !ps2_event_buffer) {
        return -1;
    }

    size_t length = 0;

    while (length < maxLength - 1) {
        char c;
        if (ps2kbd_stream_readChar(&c) != 0) {
            break;
        }
        str[length++] = c;
    }

    str[length] = '\0';
    return length;
}

static int ps2kbd_stream_readBuffer(void* buffer, size_t size) {
    if (!buffer || size == 0 || !ps2_event_buffer) {
        return -1;
    }

    char* charBuffer = (char*)buffer;
    size_t bytesRead = 0;

    while (bytesRead < size) {
        char c;
        if (ps2kbd_stream_readChar(&c) != 0) {
            break;
        }
        charBuffer[bytesRead++] = c;
    }

    return bytesRead;
}

static int ps2kbd_stream_available() {
    if (!ps2_event_buffer) {
        return 0;
    }
    return buffer_count(ps2_event_buffer);
}

static char ps2kbd_stream_peek() {
    if (!ps2_event_buffer || buffer_is_empty(ps2_event_buffer)) {
        return '\0';
    }

    KeyboardKeyEventData* event = (KeyboardKeyEventData*)buffer_peek(ps2_event_buffer);
    if (!event || !event->isPressed) {
        return '\0';
    }

    return event->ascii;
}

static void ps2kbd_stream_flush() {
    if (ps2_event_buffer) {
        buffer_clear(ps2_event_buffer);
    }
}
