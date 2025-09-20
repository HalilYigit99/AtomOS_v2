#include <gfxterm/gfxterm.h>
#include <debug/debug.h>
#include <debug/debugTerm.h>
#include <util/VPrintf.h>

GFXTerminal* debug_terminal = NULL;

static void dbgterm_Open();

bool debugterm_is_ready(void)
{
    return debug_terminal != NULL;
}

bool debugterm_ensure_ready(void)
{
    if (!debug_terminal)
    {
        dbgterm_Open();
    }
    return debug_terminal != NULL;
}

GFXTerminal* debugterm_get(void)
{
    return debug_terminal;
}

extern void gfx_draw_task();

void debugterm_flush(void)
{
    if (debug_terminal)
    {
        gfx_draw_task();
    }
}

static void dbgterm_Open()
{
    if (!debug_terminal)
    {
        debug_terminal = gfxterm_create("ttyDBG");
        if (debug_terminal)
        {
            gfxterm_visible(debug_terminal, true);
            gfxterm_enable_cursor(debug_terminal, false);
        }
    }
}

static void dbgterm_Close()
{
    if (debug_terminal)
    {
        gfxterm_visible(debug_terminal, false);
        gfxterm_destroy(debug_terminal);
        debug_terminal = NULL;
    }
}

static void dbgterm_writeChar(char c)
{
    if (debug_terminal)
    {
        gfxterm_putChar(debug_terminal, c);
    }
}

static void dbgterm_writeString(const char* str)
{
    if (debug_terminal && str)
    {
        while (*str)
        {
            gfxterm_putChar(debug_terminal, *str++);
        }
    }
}

static void dbgterm_print(const char* str)
{
    if (debug_terminal && str)
    {
        while (*str)
        {
            gfxterm_putChar(debug_terminal, *str++);
        }
    }
}

static void dbgterm_printf(const char* format, ...)
{
    if (!debug_terminal || !format)
        return;

    va_list args;
    va_start(args, format);

    vprintf(dbgterm_writeChar, format, args);

    va_end(args);
}

DebugStream dbgGFXTerm = {
    .Open = dbgterm_Open,
    .Close = dbgterm_Close,
    .WriteChar = dbgterm_writeChar,
    .WriteString = dbgterm_writeString,
    .print = dbgterm_print,
    .printf = dbgterm_printf
};

OutputStream dbgGFXTermStream = {
    .Open = dbgterm_Open,
    .Close = dbgterm_Close,
    .WriteChar = dbgterm_writeChar,
    .WriteString = dbgterm_writeString,
    .print = dbgterm_print,
    .printf = dbgterm_printf
};
