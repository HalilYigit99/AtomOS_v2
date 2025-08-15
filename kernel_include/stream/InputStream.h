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
    char (*ReadChar)();
    void (*ReadString)(char* buffer, size_t size);
    int (*Available)();
} InputStream;




#ifdef __cplusplus
}
#endif