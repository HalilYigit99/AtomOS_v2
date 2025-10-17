#include <util/formatf.h>
#include <stdarg.h>
#include <util/VPrintf.h>
#include <util/string.h>
#include <memory/memory.h>
#include <sleep.h>

static char* formatf_buffer = NULL;
static size_t buffer_count = 0;
static size_t buffer_size = 0;
static volatile bool buffer_in_use = false;

static void formatf_putChar(char c) {
    if (buffer_size == 0 || !formatf_buffer) {
        return;
    }

    if (buffer_count + 1 >= buffer_size) {
        // Reallocate buffer
        size_t new_size = buffer_size + 256;
        char* new_buffer = realloc(formatf_buffer, new_size);
        if (!new_buffer) {
            // Allocation failed, stop writing
            return;
        }
        formatf_buffer = new_buffer;
        buffer_size = new_size;
    }

    formatf_buffer[buffer_count++] = c;
    formatf_buffer[buffer_count] = '\0'; // Null-terminate

}

char* formatf(char* format, ...) {
    va_list args;
    va_start(args, format);
    while (buffer_in_use) {
        // Wait until previous call is done
        sleep_ms(1);
    }
    buffer_in_use = true;

    formatf_buffer = malloc(256);
    if (formatf_buffer)
    {
        buffer_count = 0;
        buffer_size = 256;
        memset(formatf_buffer, 0, 256);
        
        vprintf(formatf_putChar, format, args);
    }

    buffer_in_use = false;
    va_end(args);

    // Caller is responsible for freeing the returned buffer
    return formatf_buffer;
}

