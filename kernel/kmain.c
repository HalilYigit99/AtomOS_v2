#include <debug/debug.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>


void kmain() {

    LOG("cursor_X: %d, cursor_Y: %d", cursor_X, cursor_Y);

    int last_cursor_X = cursor_X;
    int last_cursor_Y = cursor_Y;

    while (1) {
        if (last_cursor_X != cursor_X || last_cursor_Y != cursor_Y) {
            LOG("Cursor moved: X=%d, Y=%d", cursor_X, cursor_Y);
            last_cursor_X = cursor_X;
            last_cursor_Y = cursor_Y;
        }
    }

}

