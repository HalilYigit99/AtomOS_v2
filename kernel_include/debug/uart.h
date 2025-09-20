#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stream/OutputStream.h>

extern OutputStream uartOutputStream;
void uart_open(void);
void uart_close(void);
void uart_write_char(char c);
void uart_write_string(const char* str);
void uart_print(const char* str);
void uart_printf(const char* format, ...);

#ifdef __cplusplus
}
#endif
