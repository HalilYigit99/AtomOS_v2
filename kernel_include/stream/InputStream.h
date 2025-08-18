#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {

    int(*Open)();
    void(*Close)();

    int(*readChar)(char* c);
    int(*readString)(char* str, size_t maxLength);
    int(*readBuffer)(void* buffer, size_t size);

    int(*available)();
    char(*peek)();
    void(*flush)();

} InputStream;





#ifdef __cplusplus
}
#endif
