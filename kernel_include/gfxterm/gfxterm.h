#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <event/event.h>
#include <stream/OutputStream.h>
#include <graphics/types.h>

typedef struct {
    char* name;

    char* buffer;
    size_t bufferLength;
    size_t bufferCapacity;

    gfx_size terminalSize;
    gfx_buffer* framebuffer;

    gfx_point cursorPos;
    size_t drawLineIndex;

    gfx_color fgColor;
    gfx_color bgColor;

    gfx_font* font;

    bool visible;
    bool dirty; // When true, framebuffer needs full redraw from buffer grid

    // Per-cell attributes (optional, default to global colors)
    gfx_color* cellFg; // size = width*height
    gfx_color* cellBg; // size = width*height

    // Cursor
    bool cursor_enabled;
    bool cursor_visible;
    size_t cursor_blink_ticks; // toggle interval in draw_task ticks
    size_t cursor_tick;
    size_t cursor_blink_next;  // absolute next toggle tick (frame counter)
    gfx_color cursor_color;

    // Scrollback (history of lines that scrolled off-screen)
    size_t scrollback_max_lines;    // configurable max line count
    size_t scrollback_count;        // current count of stored lines
    size_t scrollback_start;        // ring start index (oldest line)
    void*  sb_blocks;               // internal block list (opaque to callers)
    size_t sb_blocks_count;         // number of allocated blocks
    size_t sb_blocks_capacity;      // capacity of block pointer array

    Event* onTerminalResize;

} GFXTerminal;

GFXTerminal* gfxterm_create(const char* name);
void gfxterm_destroy(GFXTerminal* term);

void gfxterm_putChar(GFXTerminal* term, char c);
void gfxterm_write(GFXTerminal* term, const char* str);
void gfxterm_printf(GFXTerminal* term, const char* format, ...);

void gfxterm_setBGColor(GFXTerminal* term, gfx_color color);
void gfxterm_setFGColor(GFXTerminal* term, gfx_color color);

void gfxterm_clear(GFXTerminal* term);

void gfxterm_redraw(GFXTerminal* term);

void gfxterm_resize(GFXTerminal* term, gfx_size newSizeInChars);

void gfxterm_setCursorPos(GFXTerminal* term, gfx_point pos);

void gfxterm_visible(GFXTerminal* term, bool visible);

/*

    @name gfxterm_scroll
    @brief Viewport scroll using scrollback history.
           Positive lines move the view down toward the live tail.
           Negative lines move the view up into history (older lines).
           Does not modify the underlying text buffer; just changes drawLineIndex
           and triggers a redraw.
    @param lines Positive: view down; Negative: view up.

*/
void gfxterm_scroll(GFXTerminal* term, int lines);

// Cursor control
void gfxterm_enable_cursor(GFXTerminal* term, bool enable);

// Scrollback control
void gfxterm_set_scrollback_max(GFXTerminal* term, size_t max_lines);

// Bind this terminal as the default output stream (currentOutputStream)
void gfxterm_bind_output_stream(GFXTerminal* term);

#ifdef __cplusplus
}
#endif
