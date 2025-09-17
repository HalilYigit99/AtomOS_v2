#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stream/OutputStream.h>

void dumpHex8(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream);
void dumpHex16(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream);
void dumpHex32(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream);
void dumpHex64(void* data, size_t raw, size_t row, size_t sizeInBytes, OutputStream* stream);

#ifdef __cplusplus
};
#endif
