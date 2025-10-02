#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    void (*Open)();
    void (*Close)();
    void (*WriteChar)(char c);
    void (*WriteString)(const char* str);
    void (*print)(const char* str);
    void (*printf)(const char* format, ...);
} OutputStream;

extern OutputStream* currentOutputStream;

void gos_addStream(OutputStream* stream);

extern OutputStream nullOutputStream;

#ifdef __cplusplus
}
#endif