#ifndef STRING_H
#define STRING_H

#include <stddef.h>

// String uzunluğu fonksiyonları
size_t strlen(const char *str);
size_t strnlen(const char *str, size_t maxlen);

// String kopyalama fonksiyonları
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);

// String birleştirme fonksiyonları
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);

// String karşılaştırma fonksiyonları
int strcmp(const char *str1, const char *str2);
int strncmp(const char *str1, const char *str2, size_t n);
int strcasecmp(const char *str1, const char *str2);
int strncasecmp(const char *str1, const char *str2, size_t n);

// String arama fonksiyonları
char *strchr(const char *str, int c);
char *strrchr(const char *str, int c);
char *strstr(const char *haystack, const char *needle);
char *strnstr(const char *haystack, const char *needle, size_t len);

// Memory fonksiyonları
void *memset(void *ptr, int value, size_t num);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *ptr1, const void *ptr2, size_t num);
void *memchr(const void *ptr, int value, size_t num);

// String yardımcı fonksiyonları
char *strdup(const char *str);
char *strtok(char *str, const char *delim);
size_t strspn(const char *str1, const char *str2);
size_t strcspn(const char *str1, const char *str2);
char *strpbrk(const char *str1, const char *str2);

// Karakter dönüştürme yardımcı fonksiyonları
char to_lower(char c);
char to_upper(char c);
int is_alpha(char c);
int is_digit(char c);
int is_space(char c);

#endif // STRING_H