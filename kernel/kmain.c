#include <debug/debug.h>
#include <keyboard/Keyboard.h>
#include <mouse/mouse.h>
#include <graphics/gfx.h>
#include <irq/IRQ.h>

void gfx_draw_task();

void __attribute__((optimize("O0"))) kmain() {

    LOG("cursor_X: %d, cursor_Y: %d", cursor_X, cursor_Y);

    int last_cursor_X = cursor_X;
    int last_cursor_Y = cursor_Y;

    while (1) {
        if (last_cursor_X != cursor_X || last_cursor_Y != cursor_Y) {
            LOG("Cursor moved: X=%d, Y=%d", cursor_X, cursor_Y);
            last_cursor_X = cursor_X;
            last_cursor_Y = cursor_Y;
            gfx_draw_task(); // Call the graphics draw task to update the screen
        }else asm volatile ("hlt"); // If no movement, halt the CPU to save power
    }

}

