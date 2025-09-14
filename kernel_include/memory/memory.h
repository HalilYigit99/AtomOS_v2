#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern void* malloc(size_t size);
extern void free(void* ptr);
extern void* realloc(void* ptr, size_t size);
extern void* calloc(size_t count, size_t size);
extern void* malloc_aligned(size_t alignment, size_t size);

extern void memcpy(void* dest, const void* src, size_t n);

extern void memset(void* ptr, char value, size_t num);

extern void memmove(void* dest, const void* src, size_t n);

extern int memcmp(const void* ptr1, const void* ptr2, size_t num);

#ifdef __cplusplus
}
#endif