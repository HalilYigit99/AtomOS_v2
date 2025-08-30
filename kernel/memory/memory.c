#include <memory/memory.h>
#include <memory/heap.h>

void *malloc(size_t size)
{
    return heap_alloc(size);
}
void free(void *ptr)
{
    heap_free(ptr);
}
void *realloc(void *ptr, size_t size)
{
    return heap_realloc(ptr, size);
}
void *calloc(size_t count, size_t size)
{
    return heap_calloc(count, size);
}
void *malloc_aligned(size_t alignment, size_t size)
{
    return heap_aligned_alloc(alignment, size);
}

void memset(void *ptr, char value, size_t num)
{
    for (size_t i = 0; i < num; i++)
    {
        ((char *)ptr)[i] = value;
    }
}

void memmove(void *dest, const void *src, size_t n)
{
    if (!dest || !src || n == 0) return;

    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        for (size_t i = 0; i < n; ++i) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = n; i != 0; --i) {
            d[i - 1] = s[i - 1];
        }
    }
}
