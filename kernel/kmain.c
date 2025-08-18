#include <debug/debug.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>
#include <irq/IRQ.h>

void kmain() {

    LOG("cursor_X: %d, cursor_Y: %d", cursor_X, cursor_Y);

    int last_cursor_X = cursor_X;
    int last_cursor_Y = cursor_Y;

    while (1) {
        if (last_cursor_X != cursor_X || last_cursor_Y != cursor_Y) {
            LOG("Cursor moved: X=%d, Y=%d", cursor_X, cursor_Y);
            last_cursor_X = cursor_X;
            last_cursor_Y = cursor_Y;
        }else {
            for (size_t i = 0; i < 0xFFFFFFF; i++) {
                // Busy wait to avoid flooding the log
                asm volatile ("nop");
            }
            LOG("Cursor not moved!");
        }
    }

}

