#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <util/assert.h>

typedef struct {
    void (*Open)();
    void (*Close)();
    void (*WriteChar)(char c);
    void (*WriteString)(const char* str);
    void (*print)(const char* str);
    void (*printf)(const char* format, ...);
} DebugStream;

extern DebugStream* debugStream;

// Variadic logging macros supporting printf-style formatting.
// Usage examples:
//   LOG("Kernel started");
//   LOG("Value=%d", val);
//   WARN("Buffer %s size=%u", name, size);
//   ERROR("Init failed: code=%d", err);
// NOTE: We add an empty variadic arg (##__VA_ARGS__) so that calls without
// extra arguments compile cleanly in GCC/Clang.

extern uint64_t uptimeMs;

#define LOG(fmt, ...) \
    do { \
        debugStream->printf("[%llu.%llu] [%s:%d] [LOG] " fmt "\n", (uint64_t)(uptimeMs / 1000), (uint64_t)(uptimeMs % 1000), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define WARN(fmt, ...) \
    do { \
        debugStream->printf("[%llu.%llu] [%s:%d] [WARN] " fmt "\n", (uint64_t)(uptimeMs / 1000), (uint64_t)(uptimeMs % 1000), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

#define ERROR(fmt, ...) \
    do { \
        debugStream->printf("[%llu.%llu] [%s:%d] [ERROR] " fmt "\n", (uint64_t)(uptimeMs / 1000), (uint64_t)(uptimeMs % 1000), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

void gds_addStream(DebugStream* stream);

extern DebugStream nullDebugStream;

#ifdef __cplusplus
}
#endif