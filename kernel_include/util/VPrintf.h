#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Main vprintf function
int vprintf(void(*putChar)(char), const char* format, va_list list);

#ifdef __cplusplus
}
#endif