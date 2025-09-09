#include <debug/debug.h>
#include <util/VPrintf.h>
#include <list.h>

extern DebugStream uartDebugStream;

List* debugStreams = NULL;
DebugStream* debugStream = &uartDebugStream;

void gds_addStream(DebugStream* stream)
{
    if (!stream) return;
    if (!debugStreams)
    {
        debugStreams = List_Create();
        if (!debugStreams)
        {
            WARN("Failed to create debug streams list");
            return;
        }
    }
    List_Add(debugStreams, stream);
}

static void gds_Open()
{
    if (debugStreams)
    {
        for (ListNode* node = debugStreams->head; node != NULL; node = node->next)
        {
            DebugStream* ds = (DebugStream*)node->data;
            if (ds && ds->Open)
            {
                ds->Open();
            }
        }
    }
}

static void gds_Close()
{
    if (debugStreams)
    {
        for (ListNode* node = debugStreams->head; node != NULL; node = node->next)
        {
            DebugStream* ds = (DebugStream*)node->data;
            if (ds && ds->Close)
            {
                ds->Close();
            }
        }
    }
}

static void gds_WriteChar(char c)
{
    if (debugStreams)
    {
        for (ListNode* node = debugStreams->head; node != NULL; node = node->next)
        {
            DebugStream* ds = (DebugStream*)node->data;
            if (ds && ds->WriteChar)
            {
                ds->WriteChar(c);
            }
        }
    }
}

static void gds_WriteString(const char* str)
{
    if (!str) return;
    while (*str) {
        gds_WriteChar(*str++);
    }
}

static void gds_print(const char* str)
{
    gds_WriteString(str);
}

static void gds_printf(const char* format, ...)
{
    if (!format) return;
    va_list args;
    va_start(args, format);
    vprintf(gds_WriteChar, format, args);
    va_end(args);
}

DebugStream genericDebugStream = {
    .Open = gds_Open,
    .Close = gds_Close,
    .WriteChar = gds_WriteChar,
    .WriteString = gds_WriteString,
    .print = gds_print,
    .printf = gds_printf
};