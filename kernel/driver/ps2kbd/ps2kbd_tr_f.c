#include <keyboard/Keyboard.h>
#include <list.h>
#include <buffer.h>

extern List* keyboardInputStreamList; // Global list to hold keyboard input streams

extern Buffer* ps2_event_buffer; // Buffer for PS/2 keyboard events

static KeyboardKeyEventData data;
static bool isExtended = false; // Flag for extended keys
static bool isPressed = false;

void __ps2kbd_tr_f_handle(uint8_t scancode) {

    if (scancode == 0xE0) {
        isExtended = true; // Next scancode is an extended key
        return; // Wait for the next scancode
    }

    if (scancode == 0xF0) {
        isPressed = false; // Key is released
        return; // Wait for the next scancode
    }

    data.isPressed = isPressed; // Set the pressed state

    if (isExtended) {

        switch (scancode)
        {
        case 0x1F: // Windows (left)
            data.ascii = 0; // No ASCII character for this key
            data.key = KEY_WINDOWS; // Set the key to Windows
            data.isPressed = isPressed; // Set the pressed state
            data.left = true;
            break;

        case 0x27: // Windows (right)
            data.ascii = 0;
            data.key = KEY_WINDOWS;
            data.isPressed = isPressed;
            data.left = false;
            break;

        case 0x2F: // Menus
            data.ascii = 0;
            data.key = KEY_MENU;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x14: // Ctrl (right)
            data.ascii = 0;
            data.key = KEY_CTRL;
            data.isPressed = isPressed;
            data.left = false;
            break;

        case 0x11: // ALT (right)
            data.ascii = 0; // No ASCII character for this key
            data.key = KEY_ALT;
            data.isPressed = isPressed; // Set the pressed state
            data.left = false; // Right ALT key
            break;

        // Navigation keys
        case 0x70: // Insert
            data.ascii = 0;
            data.key = KEY_INSERT;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x6C: // Home
            data.ascii = 0;
            data.key = KEY_HOME;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7D: // Page Up
            data.ascii = 0;
            data.key = KEY_PAGE_UP;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x71: // Delete
            data.ascii = 0x7F; // ASCII for DEL
            data.key = KEY_DELETE;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x69: // End
            data.ascii = 0;
            data.key = KEY_END;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7A: // Page Down
            data.ascii = 0;
            data.key = KEY_PAGE_DOWN;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Arrow keys
        case 0x75: // Up Arrow
            data.ascii = 0;
            data.key = KEY_UP;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x6B: // Left Arrow
            data.ascii = 0;
            data.key = KEY_LEFT;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x72: // Down Arrow
            data.ascii = 0;
            data.key = KEY_DOWN;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x74: // Right Arrow
            data.ascii = 0;
            data.key = KEY_RIGHT;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Numeric keypad extended keys
        case 0x4A: // / (keypad)
            data.ascii = '/';
            data.key = KEY_SLASH;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x5A: // Enter (keypad)
            data.ascii = '\n';
            data.key = KEY_ENTER;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        default:
            break;
        }

        isExtended = false; // Reset extended flag after processing

    }else {

        switch (scancode)
        {
        case 0x76: // ESC
            data.ascii = 0x1B; // ASCII for ESC
            data.key = KEY_ESC;
            data.upperCase = false; // Not uppercase
            break;

        // Function keys
        case 0x05: // F1
            data.ascii = 0;
            data.key = KEY_F1;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x06: // F2
            data.ascii = 0;
            data.key = KEY_F2;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x04: // F3
            data.ascii = 0;
            data.key = KEY_F3;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x0C: // F4
            data.ascii = 0;
            data.key = KEY_F4;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x03: // F5
            data.ascii = 0;
            data.key = KEY_F5;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x0B: // F6
            data.ascii = 0;
            data.key = KEY_F6;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x83: // F7
            data.ascii = 0;
            data.key = KEY_F7;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x0A: // F8
            data.ascii = 0;
            data.key = KEY_F8;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x01: // F9
            data.ascii = 0;
            data.key = KEY_F9;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x09: // F10
            data.ascii = 0;
            data.key = KEY_F10;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x78: // F11
            data.ascii = 0;
            data.key = KEY_F11;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x07: // F12
            data.ascii = 0;
            data.key = KEY_F12;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7E: // Scroll Lock
            data.ascii = 0;
            data.key = KEY_SCROLL_LOCK;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Numbers row (TR F layout)
        case 0x0E: // +
            data.ascii = '+';
            data.key = KEY_UNKNOWN; // No direct enum for plus
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x16: // 1
            data.ascii = '1';
            data.key = KEY_1;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x1E: // 2
            data.ascii = '2';
            data.key = KEY_2;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x26: // 3
            data.ascii = '3';
            data.key = KEY_3;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x25: // 4
            data.ascii = '4';
            data.key = KEY_4;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x2E: // 5
            data.ascii = '5';
            data.key = KEY_5;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x36: // 6
            data.ascii = '6';
            data.key = KEY_6;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x3D: // 7
            data.ascii = '7';
            data.key = KEY_7;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x3E: // 8
            data.ascii = '8';
            data.key = KEY_8;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x46: // 9
            data.ascii = '9';
            data.key = KEY_9;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x45: // 0
            data.ascii = '0';
            data.key = KEY_0;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x4E: // / (slash)
            data.ascii = '/';
            data.key = KEY_SLASH;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x55: // - (minus)
            data.ascii = '-';
            data.key = KEY_MINUS;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x66: // Backspace
            data.ascii = '\b';
            data.key = KEY_BACKSPACE;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Top letter row (TR F layout)
        case 0x0D: // Tab
            data.ascii = '\t';
            data.key = KEY_TAB;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x15: // F
            data.ascii = 'f';
            data.key = KEY_F;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x1D: // G (Turkish ğ -> g)
            data.ascii = 'g';
            data.key = KEY_G;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x24: // G (regular)
            data.ascii = 'g';
            data.key = KEY_G;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x2D: // I (Turkish ı -> i)
            data.ascii = 'i';
            data.key = KEY_I;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x2C: // O (Turkish ö -> o)
            data.ascii = 'o';
            data.key = KEY_O;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x35: // D
            data.ascii = 'd';
            data.key = KEY_D;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x3C: // R
            data.ascii = 'r';
            data.key = KEY_R;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x43: // N
            data.ascii = 'n';
            data.key = KEY_N;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x44: // H
            data.ascii = 'h';
            data.key = KEY_H;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x4D: // P
            data.ascii = 'p';
            data.key = KEY_P;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x54: // Q
            data.ascii = 'q';
            data.key = KEY_Q;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x5B: // W
            data.ascii = 'w';
            data.key = KEY_W;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x5D: // X
            data.ascii = 'x';
            data.key = KEY_X;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Middle letter row (TR F layout)
        case 0x58: // Caps Lock
            data.ascii = 0;
            data.key = KEY_CAPS;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x1C: // U (Turkish ü -> u)
            data.ascii = 'u';
            data.key = KEY_U;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x1B: // I (regular)
            data.ascii = 'i';
            data.key = KEY_I;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x23: // E
            data.ascii = 'e';
            data.key = KEY_E;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x2B: // A
            data.ascii = 'a';
            data.key = KEY_A;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x34: // U (regular)
            data.ascii = 'u';
            data.key = KEY_U;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x33: // T
            data.ascii = 't';
            data.key = KEY_T;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x3B: // K
            data.ascii = 'k';
            data.key = KEY_K;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x42: // M
            data.ascii = 'm';
            data.key = KEY_M;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x4B: // L
            data.ascii = 'l';
            data.key = KEY_L;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x4C: // Y
            data.ascii = 'y';
            data.key = KEY_Y;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x52: // S (Turkish ş -> s)
            data.ascii = 's';
            data.key = KEY_S;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x5A: // Enter
            data.ascii = '\n';
            data.key = KEY_ENTER;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Bottom letter row (TR F layout)
        case 0x12: // Shift (Left)
            data.ascii = 0;
            data.key = KEY_SHIFT;
            data.isPressed = isPressed;
            data.left = true;
            break;

        case 0x61: // < (less than)
            data.ascii = '<';
            data.key = KEY_UNKNOWN; // No direct enum for less than
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x1A: // J
            data.ascii = 'j';
            data.key = KEY_J;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x22: // O (Turkish ö -> o)
            data.ascii = 'o';
            data.key = KEY_O;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x21: // V
            data.ascii = 'v';
            data.key = KEY_V;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x2A: // C (Turkish ç -> c)
            data.ascii = 'c';
            data.key = KEY_C;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x32: // C (regular)
            data.ascii = 'c';
            data.key = KEY_C;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x31: // Z
            data.ascii = 'z';
            data.key = KEY_Z;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x3A: // S (regular)
            data.ascii = 's';
            data.key = KEY_S;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x41: // B
            data.ascii = 'b';
            data.key = KEY_B;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x49: // . (period)
            data.ascii = '.';
            data.key = KEY_PERIOD;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x4A: // , (comma)
            data.ascii = ',';
            data.key = KEY_COMMA;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x59: // Shift (Right)
            data.ascii = 0;
            data.key = KEY_SHIFT;
            data.isPressed = isPressed;
            data.left = false;
            break;

        // Bottom row
        case 0x14: // Ctrl (left)
            data.ascii = 0;
            data.key = KEY_CTRL;
            data.isPressed = isPressed;
            data.left = true;
            break;

        case 0x11: // Alt (left)
            data.ascii = 0;
            data.key = KEY_ALT;
            data.isPressed = isPressed;
            data.left = true;
            break;

        case 0x29: // Spacebar
            data.ascii = ' ';
            data.key = KEY_SPACE;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        // Numeric keypad (non-extended)
        case 0x77: // Num Lock
            data.ascii = 0;
            data.key = KEY_UNKNOWN; // NUM_LOCK key would need to be added to enum
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7C: // * (keypad)
            data.ascii = '*';
            data.key = KEY_UNKNOWN; // KEYPAD_MULTIPLY would need to be added to enum
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7B: // - (keypad)
            data.ascii = '-';
            data.key = KEY_UNKNOWN; // KEYPAD_MINUS would need to be added to enum
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x6C: // 7 (keypad)
            data.ascii = '7';
            data.key = KEY_7;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x75: // 8 (keypad)
            data.ascii = '8';
            data.key = KEY_8;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7D: // 9 (keypad)
            data.ascii = '9';
            data.key = KEY_9;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x79: // + (keypad)
            data.ascii = '+';
            data.key = KEY_UNKNOWN; // KEYPAD_PLUS would need to be added to enum
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x6B: // 4 (keypad)
            data.ascii = '4';
            data.key = KEY_4;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x73: // 5 (keypad)
            data.ascii = '5';
            data.key = KEY_5;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x74: // 6 (keypad)
            data.ascii = '6';
            data.key = KEY_6;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x69: // 1 (keypad)
            data.ascii = '1';
            data.key = KEY_1;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x72: // 2 (keypad)
            data.ascii = '2';
            data.key = KEY_2;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x7A: // 3 (keypad)
            data.ascii = '3';
            data.key = KEY_3;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x70: // 0 (keypad)
            data.ascii = '0';
            data.key = KEY_0;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        case 0x71: // . (keypad)
            data.ascii = '.';
            data.key = KEY_PERIOD;
            data.isPressed = isPressed;
            data.upperCase = false;
            break;

        default:
            data.ascii = '\0'; // No ASCII character for this key
            data.key = KEY_UNKNOWN; // Unknown key
            data.upperCase = false; // Not uppercase
            break;
        }

    }

    isPressed = true; // Reset pressed flag after processing (default to pressed for next key)
    isExtended = false; // Reset extended flag after processing

    // Add the event to the PS/2 event buffer
    if (ps2_event_buffer) {
        buffer_push(ps2_event_buffer, &data);
    }

}