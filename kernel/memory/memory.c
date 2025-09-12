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

int memcmp(const void *s1, const void *s2, size_t n)
{
    if (n == 0 || s1 == s2) return 0;

    if ((n % sizeof(size_t)) == 0 &&
        (((size_t)s1 % sizeof(size_t)) == 0) &&
        (((size_t)s2 % sizeof(size_t)) == 0))
    {
        const size_t *w1 = (const size_t *)s1;
        const size_t *w2 = (const size_t *)s2;
        size_t words = n / sizeof(size_t);

        for (size_t i = 0; i < words; ++i) {
            if (w1[i] != w2[i]) {
                const uint8_t *b1 = (const uint8_t *)&w1[i];
                const uint8_t *b2 = (const uint8_t *)&w2[i];
                for (size_t j = 0; j < sizeof(size_t); ++j) {
                    if (b1[j] != b2[j]) {
                        return (int)b1[j] - (int)b2[j];
                    }
                }
            }
        }
        return 0;
    }

    const uint8_t *a = (const uint8_t *)s1;
    const uint8_t *b = (const uint8_t *)s2;
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return (int)a[i] - (int)b[i];
        }
    }
    return 0;
}
