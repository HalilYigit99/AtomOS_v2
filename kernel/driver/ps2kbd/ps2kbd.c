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

extern List* keyboardInputStreamList; // Global list to hold keyboard input streams

Buffer* ps2_event_buffer = NULL; // Buffer for PS/2 keyboard events

static bool ps2kbd_init(void);
static void ps2kbd_enable(void);
static void ps2kbd_disable(void);

static int ps2kbd_stream_open();
static void ps2kbd_stream_close();
static int ps2kbd_stream_readChar(char* c);
static int ps2kbd_stream_readString(char* str, size_t maxLength);
static int ps2kbd_stream_readBuffer(void* buffer, size_t size);
static int ps2kbd_stream_available();
static char ps2kbd_stream_peek();
static void ps2kbd_stream_flush();

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

static bool ps2kbd_init(void) {

    // Check for keyboard abstraction layer initialized
    if (!__kbd_abstraction_initialized) {
        if (keyboardInputStream.Open) keyboardInputStream.Open();
        else return false;
    }

    // Initialize the PS/2 keyboard driver
    ps2_event_buffer = buffer_create(sizeof(KeyboardKeyEventData));

    if (!ps2_event_buffer) {
        LOG("Failed to create PS/2 keyboard event buffer.");
        return false;
    }

    LOG("Initializing PS/2 keyboard...");

    bool ps2Mouse_Clk_Enabled = false;
    bool ps2Mouse_Irq_Enabled = false;

    bool ps2Mouse_Port_Enabled = false;

    // PS/2 Controller ports configuration read
    uint8_t status = inb(0x64);
    if (status & 0x20) {
        ps2Mouse_Port_Enabled = true;
    }

    // Read controller configuration byte
    io_wait();
    outb(0x64, 0x20); // Command to read configuration byte
    io_wait();
    uint8_t config = inb(0x60);

    ps2Mouse_Clk_Enabled = (config & 0x20) == 0;
    ps2Mouse_Irq_Enabled = (config & 0x02) != 0;

    // Disable PS/2 mouse clock and IRQ if enabled
    if (ps2Mouse_Clk_Enabled || ps2Mouse_Irq_Enabled) {
        config &= ~0x20; // Disable mouse clock
        config &= ~0x02; // Disable mouse IRQ
        io_wait();
        outb(0x64, 0x60); // Command to write configuration byte
        io_wait();
        outb(0x60, config);
    }

    // Enable PS/2 keyboard CLK and IRQ
    config |= 0x20; // Enable keyboard clock
    config |= 0x02; // Enable keyboard IRQ

    // Disable translation
    config &= ~0x40; // Clear translation bit

    // Write back modified configuration byte
    outb(0x64, 0x60); // Command to write configuration byte
    io_wait();
    outb(0x60, config);
    io_wait();

    // Disable PS/2 mouse port
    io_wait();
    outb(0x64, 0xA7); 
    io_wait();
    
    // Wait until the controller is ready to accept data
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if ((status & 0x02) == 0) { // Check if input buffer is clear
            break;
        }
    }

    // Send Disable scanning command
    io_wait();
    outb(0x60, PS2_KBD_CMD_DISABLE);
    io_wait();

    // Wait for ACK
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, PS2_KBD_CMD_DISABLE);
            }
        }
    }

    // Reset the keyboard
    io_wait();
    outb(0x60, PS2_KBD_CMD_RESET);
    io_wait();

    // Wait for ACK
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, PS2_KBD_CMD_RESET);
            }
        }
    }

    // Wait for self-test result
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_SELF_TEST_OK) {
                break; // Self-test passed
            } else {
                LOG("PS/2 Keyboard self-test failed or unexpected response: 0x%02X", response);
                return false;
                break; // Self-test failed or unexpected response
            }
        }
    }

set_scancode:
    // Set scancode set to 2 (most common)
    io_wait();
    outb(0x60, PS2_KBD_CMD_SET_SCANCODE);
    
    // Wait for ACK

    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, PS2_KBD_CMD_SET_SCANCODE);
            }
        }
    }

    outb(0x60, 0x02); // Set to scancode set 2
    io_wait();

    // Wait for ACK

    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, 0x02);
            }
        }
    }

    // Test scancode 2 is setted?
    io_wait();
    outb(0x60, PS2_KBD_CMD_SET_SCANCODE); // Set current scancode set

    // Wait for ACK
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, PS2_KBD_CMD_SET_SCANCODE);
            }
        }
    }

    outb(0x60, 0x00); // Request current scancode set
    io_wait();

    // Wait for ACK
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, 0xF0);
            }
        }
    }

    // Wait for scancode set response

    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == 0x02) {
                scancodeSetCorrect = true;
                break; // Scancode set 2 confirmed
            } else {
                LOG("PS/2 Keyboard scancode set verification failed or unexpected response: 0x%02X", response);
                return false;
                break; // Scancode set verification failed or unexpected response
            }
        }
    }

    if ((scancodeSetRetryCount < 10) && (scancodeSetCorrect == false)) {
        scancodeSetRetryCount++;
        WARN("PS/2 Keyboard: Scancode set verification failed, retrying... ( %d/10 )", scancodeSetRetryCount);
        // Retry setting scancode set to 2,
        goto set_scancode;
    }

    // Enable scanning
    io_wait();
    outb(0x60, PS2_KBD_CMD_ENABLE);
    io_wait();

    // Wait for ACK
    for (int i = 0; i < 1000; i++) {
        io_wait();
        uint8_t status = inb(0x64);
        if (status & 0x01) { // Check if output buffer is full
            uint8_t response = inb(0x60);
            if (response == PS2_RESPONSE_ACK) {
                break; // ACK received
            } else if (response == PS2_RESPONSE_RESEND) {
                // Resend the command
                io_wait();
                outb(0x60, PS2_KBD_CMD_ENABLE);
            }
        }
    }

    // Register the interrupt handler
    irq_controller->register_handler(IRQ_PS2_KEYBOARD, ps2kbd_isr);
    irq_controller->disable(IRQ_PS2_KEYBOARD); // Disable IRQ until enabled explicitly

    // Restore PS/2 mouse clock and IRQ if they were originally enabled
    if (ps2Mouse_Clk_Enabled || ps2Mouse_Irq_Enabled)
    {
        if (ps2Mouse_Clk_Enabled) config |= 0x20; // Enable mouse clock
        if (ps2Mouse_Irq_Enabled) config |= 0x02; // Enable mouse IRQ
        io_wait();
        outb(0x64, 0x60); // Command to write configuration byte
        io_wait();
        outb(0x60, config);
    }

    // Enable PS/2 mouse port
    if (ps2Mouse_Port_Enabled) {
        io_wait();
        outb(0x64, 0xA8); 
        io_wait();
    }

    if (scancodeSetCorrect == false) {
        WARN("PS/2 Keyboard: Scancode set verification failed.");
    }

    return true;
}

void ps2kbd_enable(void) {
    if (ps2_event_buffer == NULL) {
        return;
    }

    if (keyboardInputStreamList == NULL) {
        return;
    }

    if (ps2kbd_driver.enabled) {
        return;
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