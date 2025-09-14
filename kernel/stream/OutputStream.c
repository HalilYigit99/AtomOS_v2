#include <stream/OutputStream.h>
#include <list.h>
#include <util/VPrintf.h>

extern OutputStream uartOutputStream;

OutputStream* currentOutputStream = &uartOutputStream;

List* outputStreams = NULL;

void gos_addStream(OutputStream* stream)
{
    if (!stream) return;
    if (!outputStreams)
    {
        outputStreams = List_Create();
        if (!outputStreams)
        {
            return;
        }
    }
    List_Add(outputStreams, stream);
}

static void gos_Open()
{
    if (outputStreams)
    {
        for (ListNode* node = outputStreams->head; node != NULL; node = node->next)
        {
            OutputStream* os = (OutputStream*)node->data;
            if (os && os->Open)
            {
                os->Open();
            }
        }
    }
}

static void gos_Close()
{
    if (outputStreams)
    {
        for (ListNode* node = outputStreams->head; node != NULL; node = node->next)
        {
            OutputStream* os = (OutputStream*)node->data;
            if (os && os->Close)
            {
                os->Close();
            }
        }
    }
}

static void gos_WriteChar(char c)
{
    if (outputStreams)
    {
        for (ListNode* node = outputStreams->head; node != NULL; node = node->next)
        {
            OutputStream* os = (OutputStream*)node->data;

            if (os && os->WriteChar)
            {
                os->WriteChar(c);
            }
        }
    }
}

static void gos_WriteString(const char* str)
{
    if (outputStreams && str)
    {
        for (ListNode* node = outputStreams->head; node != NULL; node = node->next)
        {
            OutputStream* os = (OutputStream*)node->data;
            if (os && os->WriteString)
            {
                os->WriteString(str);
            }
        }
    }
}

static void gos_print(const char* str)
{
    gos_WriteString(str);
}

static void gos_printf(const char* format, ...)
{
    if (!format) return;

    va_list args;
    va_start(args, format);

    vprintf(gos_WriteChar, format, args);

    va_end(args);
}

OutputStream genericOutputStream = {
    .Open = gos_Open,
    .Close = gos_Close,
    .WriteChar = gos_WriteChar,
    .WriteString = gos_WriteString,
    .print = gos_print,
    .printf = gos_printf
};
