#include <gfxterm/gfxterm.h>
#include <graphics/gfx.h>
#include <stream/OutputStream.h>
#include <memory/memory.h>
#include <util/string.h>
#include <debug/debug.h>
#include <math.h>
#include <util/VPrintf.h>
#include <time/timer.h>
#include <list.h>
#include <task/PeriodicTask.h>

// Scrollback ring buffer implementation (block-based, linear growth)
// - Stores lines in fixed-size blocks to reduce realloc/memmove and fragmentation.
// - Grows linearly by blocks up to a configurable max (not exponential).
// - On resize (width change), scrollback is cleared for simplicity and speed.

static inline size_t __min_size_t(size_t a, size_t b) { return a < b ? a : b; }

#define SB_BLOCK_LINES 128u
#define SB_BLOCK_PTR_GROW 8u

typedef struct {
    char* chars;        // SB_BLOCK_LINES * width
    gfx_color* fg;      // per-cell fg
    gfx_color* bg;      // per-cell bg
} sb_block_t;

static inline size_t __sb_capacity_lines(GFXTerminal* term)
{
    return term->sb_blocks_count * SB_BLOCK_LINES;
}

static void __scrollback_free(GFXTerminal* term)
{
    if (!term) return;
    if (term->sb_blocks) {
        sb_block_t* blocks = (sb_block_t*)term->sb_blocks;
        for (size_t i = 0; i < term->sb_blocks_count; ++i) {
            if (blocks[i].chars) free(blocks[i].chars);
            if (blocks[i].fg) free(blocks[i].fg);
            if (blocks[i].bg) free(blocks[i].bg);
        }
        free(blocks);
    }
    term->sb_blocks = NULL;
    term->sb_blocks_count = 0;
    term->sb_blocks_capacity = 0;
    term->scrollback_count = 0;
    term->scrollback_start = 0;
}

// Helper to free an external scrollback block array captured during resize
static void __scrollback_free_external(void* blocks_ptr, size_t blocks_count)
{
    if (!blocks_ptr || blocks_count == 0) return;
    sb_block_t* blocks = (sb_block_t*)blocks_ptr;
    for (size_t i = 0; i < blocks_count; ++i) {
        if (blocks[i].chars) free(blocks[i].chars);
        if (blocks[i].fg) free(blocks[i].fg);
        if (blocks[i].bg) free(blocks[i].bg);
    }
    free(blocks);
}

static void __scrollback_clear(GFXTerminal* term)
{
    if (!term) return;
    term->scrollback_count = 0;
    term->scrollback_start = 0;
}

static bool __sb_grow_blocks_array(GFXTerminal* term)
{
    size_t new_cap = term->sb_blocks_capacity ? (term->sb_blocks_capacity + SB_BLOCK_PTR_GROW) : SB_BLOCK_PTR_GROW;
    sb_block_t* new_arr = (sb_block_t*)malloc(new_cap * sizeof(sb_block_t));
    if (!new_arr) return false;
    // copy old
    if (term->sb_blocks && term->sb_blocks_capacity) {
        memcpy(new_arr, term->sb_blocks, term->sb_blocks_capacity * sizeof(sb_block_t));
        free(term->sb_blocks);
    }
    // zero init new tail
    for (size_t i = term->sb_blocks_capacity; i < new_cap; ++i) {
        new_arr[i].chars = NULL; new_arr[i].fg = NULL; new_arr[i].bg = NULL;
    }
    term->sb_blocks = new_arr;
    term->sb_blocks_capacity = new_cap;
    return true;
}

static bool __sb_alloc_block(GFXTerminal* term)
{
    if (term->sb_blocks_count == term->sb_blocks_capacity) {
        if (!__sb_grow_blocks_array(term)) return false;
    }
    sb_block_t* blocks = (sb_block_t*)term->sb_blocks;
    size_t w = term->terminalSize.width;
    size_t cells = SB_BLOCK_LINES * w;
    sb_block_t* b = &blocks[term->sb_blocks_count];
    b->chars = (char*)malloc(cells);
    b->fg = (gfx_color*)malloc(cells * sizeof(gfx_color));
    b->bg = (gfx_color*)malloc(cells * sizeof(gfx_color));
    if (!b->chars || !b->fg || !b->bg) {
        if (b->chars) free(b->chars);
        if (b->fg) free(b->fg);
        if (b->bg) free(b->bg);
        b->chars = NULL; b->fg = NULL; b->bg = NULL;
        return false;
    }
    term->sb_blocks_count++;
    return true;
}

static void __scrollback_init(GFXTerminal* term, size_t max_lines)
{
    term->scrollback_max_lines = max_lines;
    term->scrollback_count = 0;
    term->scrollback_start = 0;
    term->sb_blocks = NULL;
    term->sb_blocks_count = 0;
    term->sb_blocks_capacity = 0;
}

// Get pointers for a given ring line index (0..capacity_lines-1)
static inline void __sb_get_line_ptrs(GFXTerminal* term, size_t ring_index,
                                      char** out_chars, gfx_color** out_fg, gfx_color** out_bg)
{
    sb_block_t* blocks = (sb_block_t*)term->sb_blocks;
    size_t w = term->terminalSize.width;
    size_t bi = ring_index / SB_BLOCK_LINES;
    size_t li = ring_index % SB_BLOCK_LINES;
    sb_block_t* b = &blocks[bi];
    *out_chars = b->chars + li * w;
    *out_fg = b->fg + li * w;
    *out_bg = b->bg + li * w;
}

// Ensure capacity for at least 'needed' lines (in units of lines), up to max_lines
static bool __attribute__((unused)) __sb_ensure_capacity_for(GFXTerminal* term, size_t needed)
{
    size_t cap_lines = __sb_capacity_lines(term);
    size_t max_lines = term->scrollback_max_lines ? term->scrollback_max_lines : needed;
    if (needed > max_lines) needed = max_lines;
    while (cap_lines < needed) {
        if (!__sb_alloc_block(term)) return false;
        cap_lines = __sb_capacity_lines(term);
    }
    return true;
}

// Push one line into scrollback from src chars/fg/bg
static void __sb_push_line(GFXTerminal* term, const char* chars, const gfx_color* fg, const gfx_color* bg)
{
    if (!term) return;
    size_t w = term->terminalSize.width;
    if (w == 0) return;

    // Ensure we have at least one block
    size_t cap_lines = __sb_capacity_lines(term);
    if (cap_lines == 0) {
        if (!__sb_alloc_block(term)) return;
        cap_lines = __sb_capacity_lines(term);
    }

    // If at max lines, drop oldest (advance start)
    if (term->scrollback_max_lines && term->scrollback_count >= term->scrollback_max_lines) {
        // overwrite oldest, no count change
        size_t ring_index = term->scrollback_start; // position to overwrite
        char* dst_c; gfx_color* dst_fg; gfx_color* dst_bg;
        __sb_get_line_ptrs(term, ring_index, &dst_c, &dst_fg, &dst_bg);
        memcpy(dst_c, chars, w);
        if (fg) memcpy(dst_fg, fg, w * sizeof(gfx_color)); else { for (size_t i = 0; i < w; ++i) dst_fg[i] = term->fgColor; }
        if (bg) memcpy(dst_bg, bg, w * sizeof(gfx_color)); else { for (size_t i = 0; i < w; ++i) dst_bg[i] = term->bgColor; }
        term->scrollback_start = (term->scrollback_start + 1) % cap_lines;
        return;
    }

    // Need to append new line; ensure capacity lines >= start+count+1
    if (term->scrollback_count >= cap_lines) {
        // grow by one block linearly
        if (!__sb_alloc_block(term)) {
            // on failure, fallback to overwrite oldest if exists
            if (cap_lines) {
                size_t ring_index = term->scrollback_start;
                char* dst_c; gfx_color* dst_fg; gfx_color* dst_bg;
                __sb_get_line_ptrs(term, ring_index, &dst_c, &dst_fg, &dst_bg);
                memcpy(dst_c, chars, w);
                if (fg) memcpy(dst_fg, fg, w * sizeof(gfx_color)); else { for (size_t i = 0; i < w; ++i) dst_fg[i] = term->fgColor; }
                if (bg) memcpy(dst_bg, bg, w * sizeof(gfx_color)); else { for (size_t i = 0; i < w; ++i) dst_bg[i] = term->bgColor; }
                term->scrollback_start = (term->scrollback_start + 1) % cap_lines;
            }
            return;
        }
        cap_lines = __sb_capacity_lines(term);
    }

    size_t write_index = (term->scrollback_start + term->scrollback_count) % cap_lines;
    char* dst_c; gfx_color* dst_fg; gfx_color* dst_bg;
    __sb_get_line_ptrs(term, write_index, &dst_c, &dst_fg, &dst_bg);
    memcpy(dst_c, chars, w);
    if (fg) memcpy(dst_fg, fg, w * sizeof(gfx_color)); else { for (size_t i = 0; i < w; ++i) dst_fg[i] = term->fgColor; }
    if (bg) memcpy(dst_bg, bg, w * sizeof(gfx_color)); else { for (size_t i = 0; i < w; ++i) dst_bg[i] = term->bgColor; }
    term->scrollback_count++;
}

// Push N top rows (that are about to be scrolled off) into scrollback
static void __scrollback_push_top_rows(GFXTerminal* term, size_t rows)
{
    if (!term || rows == 0) return;
    size_t w = term->terminalSize.width;
    size_t h = term->terminalSize.height;
    if (w == 0 || h == 0) return;

    size_t max_rows = __min_size_t(rows, h);
    for (size_t i = 0; i < max_rows; ++i) {
        const char* src_c = term->buffer + i * w;
        const gfx_color* src_fg = term->cellFg ? (term->cellFg + i * w) : NULL;
        const gfx_color* src_bg = term->cellBg ? (term->cellBg + i * w) : NULL;
        // If no per-cell attrs allocated, pass NULL to fill with global colors
        __sb_push_line(term, src_c, src_fg, src_bg);
    }
}

static char *strLineStart(char *str, size_t width, size_t line) __attribute__((unused));

List* terminals;
bool gfxRedrawTaskActive = false;

static void __gfxterm_draw_cursor(GFXTerminal* term, bool show);
static void __gfxterm_scroll_content(GFXTerminal* term, size_t up);
static size_t __gfxterm_frame_tick = 0; // global frame tick for blink scheduling

PeriodicTask* gfxterm_task = NULL;

void gfxterm_draw_task()
{
    if (List_IsEmpty(terminals)) return;

    __gfxterm_frame_tick++;
    for (ListNode* node = terminals->head; node != NULL; node = node->next) {
        GFXTerminal* term = (GFXTerminal*)node->data;
        if (!term) continue;
        if (!term->visible) continue;
        // If terminal requests suppression (active write), skip drawing this cycle
        if (term->framebuffer && term->framebuffer->suppress_draw) {
            continue;
        }
        bool redrawn = false;
        if (term->dirty) {
            gfxterm_redraw(term);
            redrawn = true;
        }
        // Handle cursor blink overlay
        if (term->cursor_enabled) {
            // If viewing history, hide cursor overlay to avoid mismatch
            if (term->drawLineIndex > 0) {
                if (term->cursor_visible) {
                    __gfxterm_draw_cursor(term, false);
                    term->cursor_visible = false;
                }
                continue;
            }
            // If we redrew the framebuffer, reapply current overlay state immediately
            if (redrawn && term->cursor_visible) {
                __gfxterm_draw_cursor(term, true);
            }
            // Absolute scheduling to avoid external resets interfering
            if (term->cursor_blink_next == 0) {
                term->cursor_blink_next = __gfxterm_frame_tick + term->cursor_blink_ticks;
            }
            if (__gfxterm_frame_tick >= term->cursor_blink_next) {
                term->cursor_blink_next = __gfxterm_frame_tick + term->cursor_blink_ticks;
                term->cursor_visible = !term->cursor_visible;
                __gfxterm_draw_cursor(term, term->cursor_visible);
            }
        }
    }
}

// PeriodicTask adapter: call draw task on scheduler tick
static void __gfxterm_periodic(void* task, void* arg) { (void)task; (void)arg; gfxterm_draw_task(); }

bool ascii_printable(char c)
{
    return c >= 32 && c <= 126;
}

// Draw or erase cursor overlay at current cursor position
static void __gfxterm_draw_cursor(GFXTerminal* term, bool show)
{
    if (!term || !term->framebuffer) return;
    if (term->drawLineIndex > 0) return; // do not draw cursor when viewing history
    size_t w = term->terminalSize.width;
    size_t h = term->terminalSize.height;
    if ((size_t)term->cursorPos.x >= w || (size_t)term->cursorPos.y >= h) return;

    size_t index = term->cursorPos.y * w + term->cursorPos.x;
    int px = term->cursorPos.x * (int)term->font->size.width;
    int py = term->cursorPos.y * (int)term->font->size.height;
    int cw = (int)term->font->size.width;
    int ch = (int)term->font->size.height;

    if (show) {
        // underline 2px cursor
        int barH = ch >= 2 ? 2 : 1;
        gfx_fill_rectangle(term->framebuffer, px, py + ch - barH, cw, barH, term->cursor_color);
    } else {
        // restore cell content fully
        gfx_color bg = term->cellBg ? term->cellBg[index] : term->bgColor;
        gfx_color fg = term->cellFg ? term->cellFg[index] : term->fgColor;
        gfx_fill_rectangle(term->framebuffer, px, py, cw, ch, bg);
        char c = term->buffer[index];
        if (c != ' ') {
            gfx_draw_char(term->framebuffer, px, py, c, fg, term->font);
        }
    }
    term->framebuffer->isDirty = true;
}

GFXTerminal *gfxterm_create(const char *name)
{
    // Removed unnecessary large allocation test

    if (!terminals)
    {
        terminals = List_Create();
        if (!terminals)
        {
            WARN("Failed to create terminals list");
            return NULL;
        }
    }

    GFXTerminal *term = (GFXTerminal *)malloc(sizeof(GFXTerminal));
    if (!term)
    {
        return NULL;
    }

    term->name = strdup(name);

    if (!term->name)
    {
        free(term);
        return NULL;
    }

    term->font = &gfx_font8x16; // Default font

    // Initialize fields to safe defaults before resize allocates resources
    term->buffer = NULL;
    term->bufferLength = 0;
    term->bufferCapacity = 0;
    term->terminalSize = (gfx_size){0, 0};
    term->framebuffer = NULL;
    term->cursorPos = (gfx_point){0, 0};
    term->drawLineIndex = 0;
    term->visible = false;
    term->dirty = true;
    term->cellFg = NULL;
    term->cellBg = NULL;
    term->cursor_enabled = true;
    term->cursor_visible = false;
    term->cursor_blink_ticks = 30; // default; adjust below if PIT known
    term->cursor_tick = 0;
    term->cursor_blink_next = 0;
    term->cursor_color = (gfx_color){ .argb = 0xFFFFFFFF };

    // Initialize scrollback config (dynamic, linear growth), default max lines
    __scrollback_init(term, 4096);

    gfx_size screenSizeInChars;
    screenSizeInChars.width = screen_width / term->font->size.width;
    screenSizeInChars.height = screen_height / term->font->size.height;

    gfxterm_resize(term, screenSizeInChars);

    // Adjust cursor blink to PIT frequency (~2 blinks/sec)
    if (pit_timer && pit_timer->frequency) {
        size_t bt = pit_timer->frequency / 2;
        if (bt == 0) bt = 1;
        term->cursor_blink_ticks = bt;
        term->cursor_blink_next = __gfxterm_frame_tick + term->cursor_blink_ticks;
    }

    term->bufferLength = 0;
    term->bufferCapacity = term->terminalSize.width * term->terminalSize.height;

    term->cursorPos.x = 0;
    term->cursorPos.y = 0;

    term->drawLineIndex = 0;

    term->fgColor = (gfx_color){.argb = 0xFFFFFFFF}; // White (opaque)
    term->bgColor = (gfx_color){.argb = 0xFF000000}; // Black (opaque)
    term->dirty = true;

    gfxterm_visible(term, true);

    List_Add(terminals, term);

    if (gfxRedrawTaskActive == false)
    {
        gfxterm_task = periodic_task_create("GFXTerm Task", __gfxterm_periodic, NULL, 100); // ~10 FPS
        if (!gfxterm_task)
        {
            ERROR("Failed to create GFXTerm task");
            // Cleanup
            gfxterm_visible(term, false);
            List_Remove(terminals, term);
            gfxterm_destroy(term);
            return NULL;
        }
        periodic_task_start(gfxterm_task);
        gfxRedrawTaskActive = true;
    }

    return term;
}

void gfxterm_visible(GFXTerminal *term, bool visible)
{
    if (!term)
        return;
    term->visible = visible;
    if (visible)
    {
        gfx_screen_register_buffer(term->framebuffer);
        term->cursor_tick = 0;
        term->cursor_visible = false;
    }
    else
    {
        gfx_screen_unregister_buffer(term->framebuffer);
    }
}

void gfxterm_setChar(GFXTerminal *term, size_t x, size_t y, char c)
{
    if (x >= term->terminalSize.width || y >= term->terminalSize.height)
    {
        return; // Out of bounds
    }

    if (!ascii_printable(c) && c != '\n' && c != '\r' && c != '\t')
    {
        c = ' '; // Replace non-printable characters with space
    }

    size_t index = y * term->terminalSize.width + x;
    if (index >= term->bufferCapacity)
    {
        return; // Out of bounds
    }

    term->buffer[index] = c;
    if (term->cellFg) term->cellFg[index] = term->fgColor;
    if (term->cellBg) term->cellBg[index] = term->bgColor;

    // If viewing history, defer drawing to full redraw to keep viewport consistent
    if (term->drawLineIndex > 0) {
        term->dirty = true;
        return;
    }

    size_t fb_x = x * term->font->size.width;
    size_t fb_y = y * term->font->size.height;

    // Fill background for this cell (use width/height, not x2/y2)
    gfx_color bg = term->cellBg ? term->cellBg[index] : term->bgColor;
    gfx_fill_rectangle(term->framebuffer, fb_x, fb_y,
                       term->font->size.width, term->font->size.height,
                       bg);

    gfx_color fg = term->cellFg ? term->cellFg[index] : term->fgColor;
    gfx_draw_char(term->framebuffer, fb_x, fb_y, c, fg, term->font);
    if (term->framebuffer) term->framebuffer->isDirty = true;
}

void gfxterm_putChar(GFXTerminal *term, char c)
{
    if (!term)
        return;

    // Suppress global flush while updating terminal framebuffer
    bool __prev_suppress = false;
    if (term->framebuffer) {
        __prev_suppress = term->framebuffer->suppress_draw;
        term->framebuffer->suppress_draw = true;
    }

    // If cursor overlay is visible at old position, erase it before modifying content/position
    if (term->cursor_enabled && term->cursor_visible) {
        __gfxterm_draw_cursor(term, false);
        term->cursor_visible = false;
        term->cursor_tick = 0;
        term->cursor_blink_next = __gfxterm_frame_tick + term->cursor_blink_ticks;
    }

    // Work on a local cursor copy, commit at the end
    int curx = term->cursorPos.x;
    int cury = term->cursorPos.y;

    if (c == '\n') {
        curx = 0;
        cury++;
    } else if (c == '\r') {
        curx = 0;
    } else if (c == '\t') {
        if (curx < 0) curx = 0;
        int tab = 4 - (curx % 4);
        if (tab <= 0) tab = 4;
        // Write spaces to next tab stop
        for (int i = 0; i < tab; ++i) {
            gfxterm_setChar(term, (size_t)curx, (size_t)cury, ' ');
            curx++;
        }
    } else if (c == '\b') {
        if (curx > 0) {
            curx--;
        } else if (cury > 0) {
            cury--;
            curx = (int)term->terminalSize.width - 1;
        }
        if (curx < 0) curx = 0;
        if (cury < 0) cury = 0;
        gfxterm_setChar(term, (size_t)curx, (size_t)cury, ' ');
    } else {
        // Printable or others handled by setChar
        gfxterm_setChar(term, (size_t)curx, (size_t)cury, c);
        curx++;
    }

    // Wrap to next line on width overflow
    if ((size_t)curx >= term->terminalSize.width) {
        curx = 0;
        cury++;
    }

    // Scroll when reaching bottom
    if ((size_t)cury >= term->terminalSize.height) {
        __gfxterm_scroll_content(term, 1);
        cury = (int)term->terminalSize.height - 1;
    }

    // Commit new cursor position and reset blink state
    term->cursor_tick = 0;
    term->cursor_visible = false;
    term->cursor_blink_next = __gfxterm_frame_tick + term->cursor_blink_ticks;
    __gfxterm_draw_cursor(term, false); // ensure no overlay at new position
    
    term->cursorPos.x = curx;
    term->cursorPos.y = cury;

    // Restore suppression state
    if (term->framebuffer) {
        term->framebuffer->suppress_draw = __prev_suppress;
    }
}

// Internal content scroll used by writer when reaching bottom
static void __gfxterm_scroll_content(GFXTerminal* term, size_t up)
{
    if (!term || up == 0) return;
    size_t w = term->terminalSize.width;
    size_t h = term->terminalSize.height;
    if (!term->buffer || w == 0 || h == 0) return;

    // Erase cursor overlay before we move pixels around
    if (term->cursor_enabled && term->cursor_visible) {
        __gfxterm_draw_cursor(term, false);
        term->cursor_visible = false;
        term->cursor_tick = 0;
    }

    if (up > h) up = h;
    // Push the top rows into scrollback before overwriting
    __scrollback_push_top_rows(term, up);
    size_t remain = h - up;
    if (remain > 0) {
        memmove(term->buffer + 0 * w,
                term->buffer + up * w,
                remain * w);
    }
    memset(term->buffer + remain * w, ' ', up * w);

    // Attributes scroll (foreground/background)
    if (term->cellFg && term->cellBg) {
        memmove(term->cellFg + 0 * w,
                term->cellFg + up * w,
                remain * w * sizeof(gfx_color));
        memmove(term->cellBg + 0 * w,
                term->cellBg + up * w,
                remain * w * sizeof(gfx_color));
        for (size_t i = 0; i < up * w; ++i) {
            term->cellFg[remain * w + i] = term->fgColor;
            term->cellBg[remain * w + i] = term->bgColor;
        }
    }

    // Also scroll the framebuffer pixels up by up*charHeight (only if at live tail)
    if (term->framebuffer && term->drawLineIndex == 0) {
        size_t ch = term->font->size.height;
        size_t scroll_px = up * ch;
        if (scroll_px > term->framebuffer->size.height)
            scroll_px = term->framebuffer->size.height;

        size_t bpp = term->framebuffer->bpp / 8;
        size_t pitch = term->framebuffer->size.width * bpp;
        size_t copy_rows = term->framebuffer->size.height - scroll_px;

        if (copy_rows > 0) {
            memmove((uint8_t*)term->framebuffer->buffer + 0 * pitch,
                    (uint8_t*)term->framebuffer->buffer + scroll_px * pitch,
                    copy_rows * pitch);
        }
        // Clear bottom region
        gfx_fill_rectangle(term->framebuffer,
                           0,
                           (int)(term->framebuffer->size.height - scroll_px),
                           (int)term->framebuffer->size.width,
                           (int)scroll_px,
                           term->bgColor);
        term->framebuffer->isDirty = true;
    } else {
        term->dirty = true;
    }
}

void gfxterm_scroll(GFXTerminal *term, int lines)
{
    if (!term || lines == 0) return;
    // Viewport scroll using history via drawLineIndex
    size_t w = term->terminalSize.width;
    size_t h = term->terminalSize.height;
    if (!term->buffer || w == 0 || h == 0) return;

    // Hide cursor when changing viewport
    if (term->cursor_visible) {
        __gfxterm_draw_cursor(term, false);
        term->cursor_visible = false;
    }

    size_t max_offset = term->scrollback_count; // number of lines we can go up
    if (lines < 0) {
        // Scroll up into history
        size_t delta = (size_t)(-lines);
        size_t new_index = term->drawLineIndex + delta;
        if (new_index > max_offset) new_index = max_offset;
        term->drawLineIndex = new_index;
        term->dirty = true;
    } else if (lines > 0) {
        // Scroll down toward live tail
        size_t delta = (size_t)lines;
        if (term->drawLineIndex > delta) term->drawLineIndex -= delta;
        else term->drawLineIndex = 0;
        term->dirty = true;
    }
}

void gfxterm_write(GFXTerminal *term, const char *str)
{
    if (!term || !str)
        return;
    // Suppress global flush for the duration of this write
    bool __prev_suppress = false;
    if (term->framebuffer) {
        __prev_suppress = term->framebuffer->suppress_draw;
        term->framebuffer->suppress_draw = true;
    }
    while (*str)
    {
        gfxterm_putChar(term, *str++);
    }
    // Restore suppression state
    if (term->framebuffer) {
        term->framebuffer->suppress_draw = __prev_suppress;
    }
}

static GFXTerminal *gfxterm_printf_terminal = NULL;

static void gfxterm_printf_putChar(char c)
{
    if (gfxterm_printf_terminal)
    {
        gfxterm_putChar(gfxterm_printf_terminal, c);
    }
}

void gfxterm_printf(GFXTerminal *term, const char *format, ...)
{
    if (!term || !format)
        return;

    while (gfxterm_printf_terminal)
    {
        // Wait until the previous printf is done
    }

    va_list args;
    va_start(args, format);

    // Suppress global flush during formatted print
    bool __prev_suppress = false;
    if (term->framebuffer) {
        __prev_suppress = term->framebuffer->suppress_draw;
        term->framebuffer->suppress_draw = true;
    }

    gfxterm_printf_terminal = term;

    vprintf(gfxterm_printf_putChar, format, args);

    va_end(args);
    gfxterm_printf_terminal = NULL;

    // Restore suppression state
    if (term->framebuffer) {
        term->framebuffer->suppress_draw = __prev_suppress;
    }
}

void gfxterm_enable_cursor(GFXTerminal *term, bool enable)
{
    if (!term) return;
    term->cursor_enabled = enable;
    term->cursor_tick = 0;
    if (!enable && term->cursor_visible) {
        // Hide cursor by restoring cell
        __gfxterm_draw_cursor(term, false);
        term->cursor_visible = false;
    }
}

void gfxterm_set_scrollback_max(GFXTerminal* term, size_t max_lines)
{
    if (!term) return;
    term->scrollback_max_lines = max_lines;
    if (max_lines == 0) return; // unlimited, keep current
    if (term->scrollback_count > max_lines) {
        size_t cap_lines = __sb_capacity_lines(term);
        if (cap_lines) {
            size_t drop = term->scrollback_count - max_lines;
            term->scrollback_start = (term->scrollback_start + drop) % cap_lines;
            term->scrollback_count = max_lines;
        } else {
            term->scrollback_count = 0;
            term->scrollback_start = 0;
        }
    }
}

// OutputStream binding
static GFXTerminal* __os_term = NULL;
static void __os_open() {}
static void __os_close() {}
static void __os_putc(char c) { if (__os_term) gfxterm_putChar(__os_term, c); }
static void __os_puts(const char* s) { if (__os_term) gfxterm_write(__os_term, s); }
static void __os_print(const char* s) { __os_puts(s); }
static void __os_printf(const char* fmt, ...) {
    if (!__os_term) return;
    va_list args; va_start(args, fmt);
    // Suppress global flush across the whole formatted print
    bool __prev_suppress = false;
    if (__os_term->framebuffer) {
        __prev_suppress = __os_term->framebuffer->suppress_draw;
        __os_term->framebuffer->suppress_draw = true;
    }
    // Reuse printf path
    GFXTerminal* prev = gfxterm_printf_terminal;
    while (gfxterm_printf_terminal) {}
    gfxterm_printf_terminal = __os_term;
    vprintf(gfxterm_printf_putChar, fmt, args);
    va_end(args);
    gfxterm_printf_terminal = prev;
    if (__os_term->framebuffer) {
        __os_term->framebuffer->suppress_draw = __prev_suppress;
    }
}

static OutputStream __gfxterm_stream = {
    .Open = __os_open,
    .Close = __os_close,
    .WriteChar = __os_putc,
    .WriteString = __os_puts,
    .print = __os_print,
    .printf = __os_printf,
};

void gfxterm_bind_output_stream(GFXTerminal* term)
{
    __os_term = term;
    currentOutputStream = &__gfxterm_stream;
}

void gfxterm_setBGColor(GFXTerminal *term, gfx_color color)
{
    if (!term)
        return;
    // Ensure background is opaque to allow clearing
    if (color.a == 0) color.a = 0xFF;
    term->bgColor = color;
    term->dirty = true;
}

void gfxterm_setFGColor(GFXTerminal *term, gfx_color color)
{
    if (!term)
        return;
    if (color.a == 0) color.a = 0xFF;
    term->fgColor = color;
    term->dirty = true;
}

void gfxterm_clear(GFXTerminal *term)
{
    if (!term)
        return;

    // Suppress global flush while clearing terminal
    bool __prev_suppress = false;
    if (term->framebuffer) {
        __prev_suppress = term->framebuffer->suppress_draw;
        term->framebuffer->suppress_draw = true;
    }

    // Clear text grid
    if (term->buffer && term->bufferCapacity) {
        memset(term->buffer, ' ', term->bufferCapacity);
    }
    if (term->cellFg && term->cellBg) {
        for (size_t i = 0; i < term->bufferCapacity; ++i) {
            term->cellFg[i] = term->fgColor;
            term->cellBg[i] = term->bgColor;
        }
    }
    term->bufferLength = 0;

    // Clear framebuffer
    if (term->framebuffer) {
        gfx_fill_rectangle(term->framebuffer, 0, 0,
                           term->framebuffer->size.width,
                           term->framebuffer->size.height,
                           term->bgColor);
        term->framebuffer->isDirty = true;
    }

    // Reset cursor position
    term->cursorPos.x = 0;
    term->cursorPos.y = 0;

    // Clear scrollback and viewport offset
    __scrollback_clear(term);
    term->drawLineIndex = 0;

    term->dirty = true;

    // Restore suppression state
    if (term->framebuffer) {
        term->framebuffer->suppress_draw = __prev_suppress;
    }
}

void gfxterm_resize(GFXTerminal *term, gfx_size newSizeInChars)
{
    if (!term)
        return;

    // Suppress global flush during resize to avoid mid-frame artifacts
    bool __prev_suppress = false;
    if (term->framebuffer) {
        __prev_suppress = term->framebuffer->suppress_draw;
        term->framebuffer->suppress_draw = true;
    }

    if (!term->font)
    {
        WARN("Font is not given, using default font is 8x16 ASCII");
        term->font = &gfx_font8x16;
    }

    size_t screenMaxWidthInChars = screen_width / term->font->size.width;
    size_t screenMaxHeightInChars = screen_height / term->font->size.height;

    gfx_size sizeInPixels = {
        .width = newSizeInChars.width * term->font->size.width,
        .height = newSizeInChars.height * term->font->size.height};

    if (sizeInPixels.width > screen_width || sizeInPixels.height > screen_height)
    {
        WARN("Requested terminal size is too large, resizing to fit the screen");
        sizeInPixels.width = screen_width;
        sizeInPixels.height = screen_height;

        newSizeInChars.width = screenMaxWidthInChars;
        newSizeInChars.height = screenMaxHeightInChars;
    }

    // Preserve old grid and scrollback
    size_t oldW = term->terminalSize.width;
    size_t oldH = term->terminalSize.height;
    char* oldBuf = term->buffer;
    gfx_color* __attribute__((unused)) oldFgGrid = term->cellFg; // may be NULL
    gfx_color* __attribute__((unused)) oldBgGrid = term->cellBg; // may be NULL
    // Capture old scrollback pointers to migrate later
    void* old_blocks = term->sb_blocks;
    size_t old_blocks_count = term->sb_blocks_count;
    size_t old_scrollback_count = term->scrollback_count;
    size_t old_scrollback_start = term->scrollback_start;
    size_t prev_max_lines = term->scrollback_max_lines;

    term->terminalSize = newSizeInChars;

    if (term->framebuffer)
    {
        if (term->visible)
        {
            // Clear old framebuffer to avoid artifacts, flush once
            gfx_fill_rectangle(term->framebuffer, 0, 0,
                               term->framebuffer->size.width,
                               term->framebuffer->size.height,
                               term->bgColor);
            term->framebuffer->isDirty = true;
            gfxterm_draw_task();

            gfx_screen_unregister_buffer(term->framebuffer);
        }
        gfx_destroy_buffer(term->framebuffer);
    }

    // Create framebuffer with correct pixel dimensions
    term->framebuffer = gfx_create_buffer(sizeInPixels.width, sizeInPixels.height);

    if (!term->framebuffer)
    {
        WARN("Failed to create framebuffer for GFXTerminal");
        term->terminalSize = (gfx_size){0, 0};
        // Restore suppression state before returning
        if (term->framebuffer) {
            term->framebuffer->suppress_draw = __prev_suppress;
        }
        return;
    }

    // If terminal was visible, re-register new framebuffer to screen stack
    if (term->visible)
    {
        gfx_screen_register_buffer(term->framebuffer);
    }

    // Recreate text grid
    term->bufferCapacity = term->terminalSize.width * term->terminalSize.height;
    term->buffer = (char*)malloc(term->bufferCapacity);
    if (!term->buffer)
    {
        WARN("Failed to allocate terminal text buffer");
        term->bufferCapacity = 0;
        // Restore suppression state before returning
        if (term->framebuffer) {
            term->framebuffer->suppress_draw = __prev_suppress;
        }
        return;
    }

    memset(term->buffer, ' ', term->bufferCapacity);

    // Recreate attribute grids (free old ones)
    if (term->cellFg) free(term->cellFg); // note: do not use oldFgGrid after this
    if (term->cellBg) free(term->cellBg); // note: do not use oldBgGrid after this
    term->cellFg = (gfx_color*)malloc(term->bufferCapacity * sizeof(gfx_color));
    term->cellBg = (gfx_color*)malloc(term->bufferCapacity * sizeof(gfx_color));
    if (!term->cellFg || !term->cellBg)
    {
        WARN("Failed to allocate terminal attribute buffers");
        // best-effort: free partially and continue without per-cell attrs
        if (term->cellFg) { free(term->cellFg); term->cellFg = NULL; }
        if (term->cellBg) { free(term->cellBg); term->cellBg = NULL; }
    } else {
        for (size_t i = 0; i < term->bufferCapacity; ++i) {
            term->cellFg[i] = term->fgColor;
            term->cellBg[i] = term->bgColor;
        }
    }

    // Start fresh scrollback storage for new width; we'll migrate old content
    term->sb_blocks = NULL;
    term->sb_blocks_count = 0;
    term->sb_blocks_capacity = 0;
    term->scrollback_count = 0;
    term->scrollback_start = 0;
    term->scrollback_max_lines = prev_max_lines ? prev_max_lines : 4096;

    // Migrate: reflow old scrollback lines into new scrollback
    if (old_blocks && old_blocks_count && oldW) {
        sb_block_t* blocks = (sb_block_t*)old_blocks;
        size_t cap_lines = old_blocks_count * SB_BLOCK_LINES;
        size_t newW = term->terminalSize.width;
        if (newW == 0) newW = 1;
        for (size_t i = 0; i < old_scrollback_count; ++i) {
            size_t ring_index = (old_scrollback_start + i) % cap_lines;
            size_t bi = ring_index / SB_BLOCK_LINES;
            size_t li = ring_index % SB_BLOCK_LINES;
            char* src_c = blocks[bi].chars + li * oldW;
            gfx_color* src_fg = blocks[bi].fg + li * oldW;
            gfx_color* src_bg = blocks[bi].bg + li * oldW;

            // Split and push into new width lines, padding spaces at end
            size_t off = 0;
            while (off < oldW) {
                size_t chunk = newW;
                char* out_c = (char*)malloc(newW);
                gfx_color* out_fg = (gfx_color*)malloc(newW * sizeof(gfx_color));
                gfx_color* out_bg = (gfx_color*)malloc(newW * sizeof(gfx_color));
                // preset
                for (size_t x = 0; x < newW; ++x) {
                    out_c[x] = ' ';
                    out_fg[x] = term->fgColor;
                    out_bg[x] = term->bgColor;
                }
                size_t remain = oldW - off;
                if (chunk > remain) chunk = remain;
                memcpy(out_c, src_c + off, chunk);
                memcpy(out_fg, src_fg + off, chunk * sizeof(gfx_color));
                memcpy(out_bg, src_bg + off, chunk * sizeof(gfx_color));
                __sb_push_line(term, out_c, out_fg, out_bg);
                off += chunk;
            }
        }
    }

    // Migrate: append old visible buffer lines to the new scrollback (treat them as newest)
    if (oldBuf && oldW && oldH) {
        size_t newW = term->terminalSize.width;
        if (newW == 0) newW = 1;
        for (size_t y = 0; y < oldH; ++y) {
            char* src_c = oldBuf + y * oldW;
            size_t off = 0;
            while (off < oldW) {
                size_t chunk = newW;
                char* out_c = (char*)malloc(newW);
                gfx_color* out_fg = (gfx_color*)malloc(newW * sizeof(gfx_color));
                gfx_color* out_bg = (gfx_color*)malloc(newW * sizeof(gfx_color));
                for (size_t x = 0; x < newW; ++x) {
                    out_c[x] = ' ';
                    out_fg[x] = term->fgColor;
                    out_bg[x] = term->bgColor;
                }
                size_t remain = oldW - off;
                if (chunk > remain) chunk = remain;
                memcpy(out_c, src_c + off, chunk);
                // We don't preserve per-cell attrs from the live grid (simplify); keep global fg/bg
                __sb_push_line(term, out_c, out_fg, out_bg);
                off += chunk;
            }
        }
        // oldBuf no longer needed
        free(oldBuf);
    }

    // Now fill the new visible buffer with the newest h lines from scrollback
    {
        size_t w = term->terminalSize.width;
        size_t h = term->terminalSize.height;
        size_t cap_lines = __sb_capacity_lines(term);
        size_t take = (term->scrollback_count > h) ? h : term->scrollback_count;
        size_t start_from = (cap_lines && take) ? ((term->scrollback_start + term->scrollback_count - take) % cap_lines) : 0;
        // Clear buffer first
        memset(term->buffer, ' ', term->bufferCapacity);
        if (term->cellFg && term->cellBg) {
            for (size_t i = 0; i < term->bufferCapacity; ++i) {
                term->cellFg[i] = term->fgColor;
                term->cellBg[i] = term->bgColor;
            }
        }
        for (size_t i = 0; i < take; ++i) {
            size_t ring_index = (start_from + i) % cap_lines;
            char* cptr; gfx_color* fptr; gfx_color* bptr;
            __sb_get_line_ptrs(term, ring_index, &cptr, &fptr, &bptr);
            size_t dst_line = h - take + i;
            memcpy(term->buffer + dst_line * w, cptr, w);
            if (term->cellFg && term->cellBg) {
                memcpy(term->cellFg + dst_line * w, fptr, w * sizeof(gfx_color));
                memcpy(term->cellBg + dst_line * w, bptr, w * sizeof(gfx_color));
            }
        }
        // Remove the lines we just displayed from scrollback tail
        if (take > 0) {
            term->scrollback_count -= take;
        }
        term->drawLineIndex = 0;
    }

    // Free old scrollback blocks captured earlier
    __scrollback_free_external(old_blocks, old_blocks_count);

    term->dirty = true;

    // Restore suppression state
    if (term->framebuffer) {
        term->framebuffer->suppress_draw = __prev_suppress;
    }
}

void gfxterm_redraw(GFXTerminal *term)
{
    if (!term || !term->framebuffer || !term->buffer) return;

    // Clear framebuffer once
    gfx_fill_rectangle(term->framebuffer, 0, 0,
                       term->framebuffer->size.width,
                       term->framebuffer->size.height,
                       term->bgColor);

    size_t w = term->terminalSize.width;
    size_t h = term->terminalSize.height;

    // Determine viewport start considering scrollback and drawLineIndex
    size_t total_lines = term->scrollback_count + h; // current screen buffer assumed full h
    size_t max_offset = (total_lines > h) ? (total_lines - h) : 0;
    if (term->drawLineIndex > max_offset) term->drawLineIndex = max_offset;
    size_t start_line = (total_lines > h) ? (total_lines - h - term->drawLineIndex) : 0;

    size_t cap_lines = __sb_capacity_lines(term);

    for (size_t y = 0; y < h; y++)
    {
        size_t logical_line = start_line + y;
        const char* line_c;
        const gfx_color* line_fg;
        const gfx_color* line_bg;

        if (logical_line < term->scrollback_count && cap_lines) {
            // From scrollback (oldest + logical_line)
            size_t ring_index = (term->scrollback_start + logical_line) % cap_lines;
            char* cptr; gfx_color* fptr; gfx_color* bptr;
            __sb_get_line_ptrs(term, ring_index, &cptr, &fptr, &bptr);
            line_c = cptr; line_fg = fptr; line_bg = bptr;
        } else {
            // From current screen buffer
            size_t buf_line = logical_line - term->scrollback_count;
            line_c = term->buffer + buf_line * w;
            line_fg = term->cellFg ? (term->cellFg + buf_line * w) : NULL;
            line_bg = term->cellBg ? (term->cellBg + buf_line * w) : NULL;
        }

        for (size_t x = 0; x < w; x++)
        {
            char c = line_c[x];
            if (c == ' ') continue; // skip spaces for speed
            size_t px = x * term->font->size.width;
            size_t py = y * term->font->size.height;
            gfx_color fg = line_fg ? line_fg[x] : term->fgColor;
            gfx_color bg = line_bg ? line_bg[x] : term->bgColor;
            // fill background only if per-cell differs from global arrays exist
            if (line_bg) {
                gfx_fill_rectangle(term->framebuffer, px, py,
                                   term->font->size.width, term->font->size.height,
                                   bg);
            }
            gfx_draw_char(term->framebuffer, px, py, c, fg, term->font);
        }
    }

    term->framebuffer->isDirty = true;
    term->dirty = false;
}

void gfxterm_setCursorPos(GFXTerminal *term, gfx_point pos)
{
    if (!term)
        return;
    if (pos.x < 0 || pos.y < 0)
    {
        pos.x = 0;
        pos.y = 0;
    }

    if ((size_t)pos.x >= term->terminalSize.width || (size_t)pos.y >= term->terminalSize.height)
    {
        return; // Out of bounds
    }

    // If a cursor overlay is visible at the old location, erase it before moving
    if (term->cursor_enabled && term->cursor_visible) {
        __gfxterm_draw_cursor(term, false);
        term->cursor_visible = false;
    }

    term->cursorPos.x = pos.x;
    term->cursorPos.y = pos.y;
    term->cursor_tick = 0;
    term->cursor_visible = false; // force redraw of cursor on next tick
    term->cursor_blink_next = __gfxterm_frame_tick + term->cursor_blink_ticks;
}

void gfxterm_destroy(GFXTerminal *term)
{
    if (!term)
        return;

    if (term->visible)
    {
        gfx_screen_unregister_buffer(term->framebuffer);
    }

    if (term->framebuffer)
    {
        gfx_destroy_buffer(term->framebuffer);
    }

    if (term->name)
    {
        free(term->name);
    }

    if (term->buffer)
    {
        free(term->buffer);
    }
    if (term->cellFg) free(term->cellFg);
    if (term->cellBg) free(term->cellBg);
    __scrollback_free(term);

    if (terminals)
    {
        List_Remove(terminals, term);
    }

    free(term);
}

static char *strLineStart(char *str, size_t width, size_t line)
{

    if (line == 0)
        return str;

    size_t currentOffset = 0;
    size_t currentLineId = 0;

    while (*str)
    {

        switch (*str)
        {
        case '\n':
            currentLineId++;
            currentOffset = 0;
            break;

        case '\r':
            currentOffset = 0;
            break;

        case '\b':
            if (currentOffset)
                currentOffset--;
            else
            {
                currentLineId--;
                currentOffset = 0;
            }
            break;

        case '\t':
            currentOffset += 4;
            break;

        default:
            currentOffset++;
            break;
        }

        if (currentOffset >= width)
        {
            currentLineId++;
            currentOffset = 0;
        }

        str++;

        if (currentLineId >= line)
        {
            return str;
        }
    }

    // str has X lines, we wanted X+Y lines...
    return str;
}
