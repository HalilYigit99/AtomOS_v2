#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char* name;
    void* data;
    bool active;

    void (*writeByte)(uint8_t byte);
    void (*write)(const void* data, size_t size);
    void (*writeString)(const char* str);
    void (*writeLine)(const char* str);
    void (*writeF)(const char* format, ...);
    void (*flush)(void);

    uint8_t (*readByte)(void);
    size_t (*read)(void* buffer, size_t size);
    size_t (*readLine)(char* buffer, size_t maxSize);
    size_t (*readUntil)(char* buffer, size_t maxSize, char delimiter);

} IOStream;



#ifdef __cplusplus
}
#endif
