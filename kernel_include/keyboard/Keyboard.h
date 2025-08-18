#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <stream/InputStream.h>

typedef enum {
    LAYOUT_US_QWERTY = 0, // US QWERTY layout ( default )
    LAYOUT_TR_QWERTY, // Turkish QWERTY layout
    LAYOUT_TR_F, // Turkish F layout
} KeyboardLayouts;

typedef enum {
    KEY_UNKNOWN = 0,

    // Letters
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    
    // Numbers
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    
    // Function keys
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8,
    KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    
    // Modifier keys
    KEY_SHIFT, KEY_CTRL, KEY_ALT, KEY_CAPS, KEY_TAB, KEY_SPACE,
    
    // Navigation keys
    KEY_ENTER, KEY_BACKSPACE, KEY_DELETE, KEY_INSERT, KEY_HOME, KEY_END,
    KEY_PAGE_UP, KEY_PAGE_DOWN,
    
    // Arrow keys
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    
    // Special characters
    KEY_ESC, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_COMMA, KEY_PERIOD,
    KEY_SLASH, KEY_BACKSLASH, KEY_MINUS, KEY_EQUALS, KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    
    // System keys
    KEY_PRINT_SCREEN, KEY_SCROLL_LOCK, KEY_PAUSE, KEY_MENU, KEY_WINDOWS
} KeyboardKeys;

typedef struct {
    char ascii;        // ASCII character
    KeyboardKeys key; // Enum value representing the key
    bool isPressed;    // Whether the key is pressed
    union {
        bool left;   // Left Shift key state
        bool upperCase;  // Uppercase state for letters
    };
} KeyboardKeyEventData;

extern InputStream keyboardInputStream;

#ifdef __cplusplus
}
#endif
