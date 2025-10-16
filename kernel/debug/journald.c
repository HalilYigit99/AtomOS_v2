#include <debug/debug.h>
#include <stream/OutputStream.h>
#include <util/VPrintf.h>
#include <memory/memory.h>

extern DebugStream journald_debugStream;
extern OutputStream journald_outputStream;

char* journald_buffer;
size_t journald_buffer_size;
size_t journald_buffer_index;

void journald_Open()
{
    if (journald_buffer) free(journald_buffer);

    journald_buffer = (char*)malloc(4096);
    journald_buffer_size = 4096;
    journald_buffer_index = 0;

}

void journald_Close()
{
    if (journald_buffer) free(journald_buffer);
    journald_buffer = NULL;
    journald_buffer_size = 0;
    journald_buffer_index = 0;
}

void journald_WriteChar(char c)
{
    if (!c) return;
    if (journald_buffer_index < journald_buffer_size - 1)
    {
        journald_buffer[journald_buffer_index++] = c;
        journald_buffer[journald_buffer_index] = '\0'; // Null-terminate
    }
    else
    {
        // Buffer full, expand it
        char* journald_buffer_new = realloc(journald_buffer, journald_buffer_size + 4096);
        if (journald_buffer_new)
        {
            journald_buffer = journald_buffer_new;

            journald_buffer_size += 4096;
            journald_buffer[journald_buffer_index++] = c;
            journald_buffer[journald_buffer_index] = '\0'; // Null-terminate
        }

        // If realloc fails, we silently drop the character
    }
}

void journald_WriteString(const char* str)
{
    while (*str)
    {
        journald_WriteChar(*str++);
    }
}

void journald_print(const char* str)
{
    journald_WriteString(str);
}

void journald_printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    vprintf(journald_WriteChar, format, args);

    va_end(args);
}

char* journald_getBuffer()
{
    return journald_buffer;
}

DebugStream journald_debugStream = {
    .Open = journald_Open,
    .Close = journald_Close,
    .WriteChar = journald_WriteChar,
    .WriteString = journald_WriteString,
    .print = journald_print,
    .printf = journald_printf
};

OutputStream journald_outputStream = {
    .Open = journald_Open,
    .Close = journald_Close,
    .WriteChar = journald_WriteChar,
    .WriteString = journald_WriteString,
    .print = journald_print,
    .printf = journald_printf
};
