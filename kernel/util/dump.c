#include <util/dump.h>
#include <util/VPrintf.h>

#include <stdarg.h>

// Resolve the stream to write into; fall back to currentOutputStream.
static OutputStream* resolveStream(OutputStream* stream)
{
    return stream ? stream : currentOutputStream;
}

static void stream_putc(OutputStream* stream, char c)
{
    if (!stream) return;
    if (stream->WriteChar)
    {
        stream->WriteChar(c);
        return;
    }

    if (stream->WriteString)
    {
        char buf[2] = { c, '\0' };
        stream->WriteString(buf);
        return;
    }

    if (stream->print)
    {
        char buf[2] = { c, '\0' };
        stream->print(buf);
    }
}

static void stream_puts(OutputStream* stream, const char* str)
{
    if (!stream || !str) return;
    if (stream->print)
    {
        stream->print(str);
        return;
    }

    if (stream->WriteString)
    {
        stream->WriteString(str);
        return;
    }

    if (stream->WriteChar)
    {
        while (*str)
        {
            stream->WriteChar(*str++);
        }
    }
}

static void stream_printf(OutputStream* stream, const char* fmt, ...)
{
    if (!stream || !fmt) return;
    if (!stream->WriteChar) return;

    va_list args;
    va_start(args, fmt);
    vprintf(stream->WriteChar, fmt, args);
    va_end(args);
}

static size_t defaultUnitsPerRow(size_t unitSize)
{
    const size_t bytesPerRow = 16;
    if (unitSize == 0) return 1;

    size_t units = bytesPerRow / unitSize;
    if (units == 0) units = 1;
    return units;
}

static void dumpHexGeneric(const void* data,
                            size_t raw,
                            size_t row,
                            size_t sizeInBytes,
                            size_t unitSize,
                            bool withAscii,
                            OutputStream* stream)
{
    if (!data || sizeInBytes == 0 || unitSize == 0)
    {
        return;
    }

    OutputStream* out = resolveStream(stream);
    if (!out || !out->WriteChar)
    {
        return;
    }

    size_t unitsPerRow = raw ? raw : defaultUnitsPerRow(unitSize);
    if (unitsPerRow == 0) unitsPerRow = 1;

    if (unitsPerRow > SIZE_MAX / unitSize)
    {
        unitsPerRow = SIZE_MAX / unitSize;
    }

    size_t rowBytes = unitsPerRow * unitSize;
    if (rowBytes == 0)
    {
        return;
    }

    size_t totalRows = (sizeInBytes + rowBytes - 1) / rowBytes;
    if (row != 0 && row < totalRows)
    {
        totalRows = row;
    }

    const uint8_t* bytes = (const uint8_t*)data;
    const size_t offsetWidth = sizeof(size_t) * 2;

    for (size_t r = 0; r < totalRows; ++r)
    {
        size_t offset = r * rowBytes;
        if (offset >= sizeInBytes)
        {
            break;
        }

        size_t bytesThisRow = sizeInBytes - offset;
        if (bytesThisRow > rowBytes)
        {
            bytesThisRow = rowBytes;
        }

        stream_printf(out, "%0*zx  ", (int)offsetWidth, offset);

        for (size_t i = 0; i < unitsPerRow; ++i)
        {
            size_t index = offset + i * unitSize;
            size_t remaining = 0;
            if (index < sizeInBytes)
            {
                remaining = sizeInBytes - index;
                if (remaining > unitSize)
                {
                    remaining = unitSize;
                }
            }

            if (remaining == unitSize)
            {
                unsigned long long value = 0;
                for (size_t b = 0; b < unitSize; ++b)
                {
                    value |= ((unsigned long long)bytes[index + b]) << (8 * b);
                }

                switch (unitSize)
                {
                    case 1:
                        stream_printf(out, "%02X ", (unsigned int)value & 0xFF);
                        break;
                    case 2:
                        stream_printf(out, "%04X ", (unsigned int)value & 0xFFFF);
                        break;
                    case 4:
                        stream_printf(out, "%08X ", (unsigned int)value & 0xFFFFFFFFU);
                        break;
                    case 8:
                        stream_printf(out, "%016llX ", (unsigned long long)value);
                        break;
                    default:
                        stream_printf(out, "%0*llX ", (int)(unitSize * 2), (unsigned long long)value);
                        break;
                }
            }
            else if (remaining > 0)
            {
                for (size_t b = 0; b < remaining; ++b)
                {
                    stream_printf(out, "%02X", bytes[index + b]);
                }
                for (size_t b = remaining; b < unitSize; ++b)
                {
                    stream_puts(out, "  ");
                }
                stream_putc(out, ' ');
            }
            else
            {
                size_t spaces = unitSize * 2 + 1;
                for (size_t s = 0; s < spaces; ++s)
                {
                    stream_putc(out, ' ');
                }
            }
        }

        if (withAscii)
        {
            stream_puts(out, " |");
            size_t asciiCount = unitsPerRow * unitSize;
            if (asciiCount > bytesThisRow)
            {
                asciiCount = bytesThisRow;
            }

            for (size_t i = 0; i < asciiCount; ++i)
            {
                uint8_t c = bytes[offset + i];
                char printable = (c >= 32 && c <= 126) ? (char)c : '.';
                stream_putc(out, printable);
            }
            for (size_t i = asciiCount; i < unitsPerRow * unitSize; ++i)
            {
                stream_putc(out, ' ');
            }
            stream_puts(out, "|");
        }

        stream_putc(out, '\n');
    }
}

void dumpHex8(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream)
{
    dumpHexGeneric(data, raw, row, sizeInBytes, 1, true, stream);
}

void dumpHex16(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream)
{
    dumpHexGeneric(data, raw, row, sizeInBytes, 2, false, stream);
}

void dumpHex32(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream)
{
    dumpHexGeneric(data, raw, row, sizeInBytes, 4, false, stream);
}

void dumpHex64(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream)
{
    dumpHexGeneric(data, raw, row, sizeInBytes, 8, false, stream);
}

