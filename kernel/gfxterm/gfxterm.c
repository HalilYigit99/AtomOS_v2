#include <gfxterm/gfxterm.h>
#include <graphics/gfx.h>
#include <stream/OutputStream.h>
#include <memory/memory.h>
#include <util/string.h>
#include <debug/debug.h>
#include <math.h>
#include <util/VPrintf.h>

static bool ascii_printable(char c) {
    return c >= 32 && c <= 126;
}

GFXTerminal* gfxterm_create(const char* name) {

    GFXTerminal* term = (GFXTerminal*)malloc(sizeof(GFXTerminal));
    if (!term) {
        return NULL;
    }

    term->name = strdup(name);

    if (!term->name) {
        free(term);
        return NULL;
    }

    term->font = &gfx_font8x16; // Default font

    term->framebuffer = gfx_create_buffer(screen_width, screen_height);

    term->terminalSize.width = screen_width / gfx_font8x16.size.width;
    term->terminalSize.height = screen_height / gfx_font8x16.size.height;

    term->buffer = (char*)malloc(term->terminalSize.width * term->terminalSize.height);

    if (!term->buffer) {
        free(term->name);
        free(term);
        WARN("Failed to allocate terminal buffer");
        return NULL;
    }

    term->bufferLength = 0;
    term->bufferCapacity = term->terminalSize.width * term->terminalSize.height;

    term->cursorPos.x = 0;
    term->cursorPos.y = 0;

    term->fgColor = (gfx_color){.argb = 0xFFFFFFFF}; // White
    term->bgColor = (gfx_color){.argb = 0};       // Black
    
    gfxterm_visible(term, true);

    return term;
}

void gfxterm_visible(GFXTerminal* term, bool visible) {
    if (!term) return;
    term->visible = visible;
    if (visible) {
        gfx_screen_register_buffer(term->framebuffer);
    } else {
        gfx_screen_unregister_buffer(term->framebuffer);
    }
}

void gfxterm_setChar(GFXTerminal* term, size_t x, size_t y, char c) {
    if (x >= term->terminalSize.width || y >= term->terminalSize.height) {
        return; // Out of bounds
    }

    if (!ascii_printable(c) && c != '\n' && c != '\r' && c != '\t') {
        c = ' '; // Replace non-printable characters with space
    }

    size_t index = y * term->terminalSize.width + x;
    if (index >= term->bufferCapacity) {
        return; // Out of bounds
    }

    term->buffer[index] = c;

    size_t fb_x = x * term->font->size.width;
    size_t fb_y = y * term->font->size.height;

    // Fill background
    {
        size_t fb_x_end = fb_x + term->font->size.width;
        size_t fb_y_end = fb_y + term->font->size.height;

        gfx_fill_rectangle(term->framebuffer, fb_x, fb_y, fb_x_end, fb_y_end, term->bgColor);
    }

    gfx_draw_char(term->framebuffer, fb_x, fb_y, c, term->fgColor, term->font);

}

void gfxterm_putChar(GFXTerminal* term, char c) {
    if (!term) return;

    if (c == '\n') {
        term->cursorPos.x = 0;
        term->cursorPos.y++;
    } else if (c == '\r') {
        term->cursorPos.x = 0;
    } else if (c == '\t') {
        term->cursorPos.x += 4 - (term->cursorPos.x % 4);
    } else if (c == '\b') {
        if (term->cursorPos.x) term->cursorPos.x--;
        else {
            term->cursorPos.y--;
            term->cursorPos.x = term->terminalSize.width - 1;
        }

        gfxterm_setChar(term, term->cursorPos.x, term->cursorPos.y, ' ');
    }else {
        gfxterm_setChar(term, term->cursorPos.x, term->cursorPos.y, c);
        term->cursorPos.x++;
    }

    if ((size_t)term->cursorPos.x >= term->terminalSize.width) {
        term->cursorPos.x = 0;
        term->cursorPos.y++;
    }

    if ((size_t)term->cursorPos.y >= term->terminalSize.height) {
        gfxterm_scroll(term, -(term->font->size.height));
    }

    // Add to buffer
    {
        // Check buffer overflow
        if (term->bufferLength >= term->bufferCapacity){
            // Expand buffer
            void* newBuffer = realloc(term->buffer, term->bufferCapacity * 2);
            if (!newBuffer)
            {
                WARN("Failed to expand GFXTerminal buffer( %p )", term);
                return;
            }

            term->buffer = newBuffer;
            term->bufferCapacity *= 2;
        }

        term->buffer[term->bufferLength] = c;
        term->bufferLength++;

    }
}

void gfxterm_scroll(GFXTerminal* term, int lines) {

    if (!term) return;
    if (!lines) return;

    if (lines < 0) {
        // Scroll down

        lines = abs(lines);

        gfx_buffer* buffer = term->framebuffer;
        size_t bufferLineCount = buffer->size.height / buffer->size.width;
        size_t currentBufferSizeInBytes = buffer->size.width * buffer->size.height * (buffer->bpp / 8);

        if (bufferLineCount < (buffer->drawBeginLineIndex + (size_t)lines))
        {
            // Expand the framebuffer

            void* newArea = realloc(buffer->buffer, currentBufferSizeInBytes * 2);

            if (!newArea){
                WARN("Failed to expand gfx_buffer at GFXTerminal ( %p )", term);
            }

            buffer->size.height = buffer->size.height * 2;
            buffer->buffer = newArea;

        }

        buffer->drawBeginLineIndex += lines;

    }else 
    {
        // Scroll up

        for (int i = 0; i < lines; i++)
        {
            if (term->framebuffer->drawBeginLineIndex) term->framebuffer->drawBeginLineIndex--;
        }
    }

}

void gfxterm_write(GFXTerminal* term, const char* str) {
    if (!term || !str) return;

    while (*str) {
        gfxterm_putChar(term, *str++);
    }
}

static GFXTerminal* gfxterm_printf_terminal = NULL;

static void gfxterm_printf_putChar(char c) {
    if (gfxterm_printf_terminal) {
        gfxterm_putChar(gfxterm_printf_terminal, c);
    }
}

void gfxterm_printf(GFXTerminal* term, const char* format, ...) {
    if (!term || !format) return;

    while (gfxterm_printf_terminal)
    {
        // Wait until the previous printf is done
    }

    va_list args;
    va_start(args, format);

    gfxterm_printf_terminal = term;

    vprintf(gfxterm_printf_putChar, format, args);

    va_end(args);
    gfxterm_printf_terminal = NULL;

}

void gfxterm_setBGColor(GFXTerminal* term, gfx_color color) {
    if (!term) return;
    term->bgColor = color;
}

void gfxterm_setFGColor(GFXTerminal* term, gfx_color color) {
    if (!term) return;
    term->fgColor = color;
}

void gfxterm_clear(GFXTerminal* term) {
    if (!term) return;

    // Clear buffer
    memset(term->buffer, ' ', term->bufferCapacity);
    term->bufferLength = 0;

    // Clear framebuffer
    gfx_fill_rectangle(term->framebuffer, 0, 0, term->framebuffer->size.width, term->framebuffer->size.height, term->bgColor);

    // Reset cursor position
    term->cursorPos.x = 0;
    term->cursorPos.y = 0;

    term->framebuffer->drawBeginLineIndex = 0;
}

void gfxterm_resize(GFXTerminal* term, gfx_size newSize) {
    if (!term) return;

    size_t newWidthInChars = newSize.width / term->font->size.width;
    size_t newHeightInChars = newSize.height / term->font->size.height;

    if (newWidthInChars == term->terminalSize.width && newHeightInChars == term->terminalSize.height) {
        return; // No change
    }

    term->terminalSize.width = newWidthInChars;
    term->terminalSize.height = newHeightInChars;

    // Resize framebuffer
    gfx_resize_buffer(term->framebuffer, newSize.width, newSize.height);

    // Reset cursor position if out of bounds
    term->cursorPos.x = 0;
    term->cursorPos.y = 0;

    // Re-render entire terminal
    gfxterm_clear(term);
    gfxterm_write(term, term->buffer);

}

void gfxterm_setCursorPos(GFXTerminal* term, gfx_point pos) {
    if (!term) return;
    if (pos.x < 0 || pos.y < 0) {
        pos.x = 0;
        pos.y = 0;
    }

    if ((size_t)pos.x >= term->terminalSize.width || (size_t)pos.y >= term->terminalSize.height) {
        return; // Out of bounds
    }

    term->cursorPos.x = pos.x;
    term->cursorPos.y = pos.y;
}

void gfxterm_destroy(GFXTerminal* term) {
    if (!term) return;

    if (term->visible) {
        gfx_screen_unregister_buffer(term->framebuffer);
    }

    if (term->framebuffer) {
        gfx_destroy_buffer(term->framebuffer);
    }

    if (term->name) {
        free(term->name);
    }

    if (term->buffer) {
        free(term->buffer);
    }

    free(term);
}
