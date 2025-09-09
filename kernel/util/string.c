#include <util/string.h>

// String uzunluğu hesaplama
size_t strlen(const char *str) {
    if (!str) return 0;
    
    size_t len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}

// Maksimum uzunlukla sınırlı string uzunluğu
size_t strnlen(const char *str, size_t maxlen) {
    if (!str) return 0;
    
    size_t len = 0;
    while (len < maxlen && str[len]) {
        len++;
    }
    return len;
}

// String kopyalama
char *strcpy(char *dest, const char *src) {
    if (!dest || !src) return dest;
    
    char *orig_dest = dest;
    while ((*dest++ = *src++));
    return orig_dest;
}

// Sınırlı string kopyalama
char *strncpy(char *dest, const char *src, size_t n) {
    if (!dest || !src) return dest;
    
    char *orig_dest = dest;
    size_t i;
    
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    
    // Kalan alanı null ile doldur
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return orig_dest;
}

// String birleştirme
char *strcat(char *dest, const char *src) {
    if (!dest || !src) return dest;
    
    char *orig_dest = dest;
    
    // dest'in sonuna git
    while (*dest) {
        dest++;
    }
    
    // src'yi kopyala
    while ((*dest++ = *src++));
    
    return orig_dest;
}

// Sınırlı string birleştirme
char *strncat(char *dest, const char *src, size_t n) {
    if (!dest || !src) return dest;
    
    char *orig_dest = dest;
    
    // dest'in sonuna git
    while (*dest) {
        dest++;
    }
    
    // n kadar karakter kopyala
    size_t i = 0;
    while (i < n && src[i]) {
        dest[i] = src[i];
        i++;
    }
    
    dest[i] = '\0';
    return orig_dest;
}

// String karşılaştırma
int strcmp(const char *str1, const char *str2) {
    if (!str1 || !str2) {
        if (str1 == str2) return 0;
        return str1 ? 1 : -1;
    }
    
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    
    return (unsigned char)*str1 - (unsigned char)*str2;
}

// Sınırlı string karşılaştırma
int strncmp(const char *str1, const char *str2, size_t n) {
    if (!str1 || !str2 || n == 0) {
        if (n == 0) return 0;
        if (str1 == str2) return 0;
        return str1 ? 1 : -1;
    }
    
    size_t i = 0;
    while (i < n && str1[i] && (str1[i] == str2[i])) {
        i++;
    }
    
    if (i == n) return 0;
    return (unsigned char)str1[i] - (unsigned char)str2[i];
}

// Büyük/küçük harf duyarsız karşılaştırma
int strcasecmp(const char *str1, const char *str2) {
    if (!str1 || !str2) {
        if (str1 == str2) return 0;
        return str1 ? 1 : -1;
    }
    
    while (*str1 && (to_lower(*str1) == to_lower(*str2))) {
        str1++;
        str2++;
    }
    
    return to_lower(*str1) - to_lower(*str2);
}

// Sınırlı büyük/küçük harf duyarsız karşılaştırma
int strncasecmp(const char *str1, const char *str2, size_t n) {
    if (!str1 || !str2 || n == 0) {
        if (n == 0) return 0;
        if (str1 == str2) return 0;
        return str1 ? 1 : -1;
    }
    
    size_t i = 0;
    while (i < n && str1[i] && (to_lower(str1[i]) == to_lower(str2[i]))) {
        i++;
    }
    
    if (i == n) return 0;
    return to_lower(str1[i]) - to_lower(str2[i]);
}

// Karakter arama (ilk bulunana kadar)
char *strchr(const char *str, int c) {
    if (!str) return NULL;
    
    while (*str) {
        if (*str == (char)c) {
            return (char*)str;
        }
        str++;
    }
    
    if (c == '\0') {
        return (char*)str;
    }
    
    return NULL;
}

// Karakter arama (son bulunandan itibaren)
char *strrchr(const char *str, int c) {
    if (!str) return NULL;
    
    const char *last = NULL;
    
    do {
        if (*str == (char)c) {
            last = str;
        }
    } while (*str++);
    
    return (char*)last;
}

// Alt string arama
char *strstr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)haystack;
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (!*n) {
            return (char*)haystack;
        }
        
        haystack++;
    }
    
    return NULL;
}

// Sınırlı alt string arama
char *strnstr(const char *haystack, const char *needle, size_t len) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)haystack;
    
    size_t needle_len = strlen(needle);
    if (needle_len > len) return NULL;
    
    for (size_t i = 0; i <= len - needle_len; i++) {
        if (strncmp(haystack + i, needle, needle_len) == 0) {
            return (char*)(haystack + i);
        }
    }
    
    return NULL;
}

// String duplicate (heap gerektirir - basit implementasyon)
char *strdup(const char *str) {
    size_t len = strlen(str);

    char* newBuffer = malloc(len + 1);
    memcpy(newBuffer, str, len + 1);

    return newBuffer;
}

// String tokenize (basit implementasyon)
char *strtok(char *str, const char *delim) {
    static char *saved_str = NULL;
    
    if (str != NULL) {
        saved_str = str;
    } else if (saved_str == NULL) {
        return NULL;
    }
    
    // Başlangıçtaki delimiterleri atla
    while (*saved_str && strchr(delim, *saved_str)) {
        saved_str++;
    }
    
    if (*saved_str == '\0') {
        saved_str = NULL;
        return NULL;
    }
    
    char *token_start = saved_str;
    
    // Token'ın sonunu bul
    while (*saved_str && !strchr(delim, *saved_str)) {
        saved_str++;
    }
    
    if (*saved_str) {
        *saved_str = '\0';
        saved_str++;
    } else {
        saved_str = NULL;
    }
    
    return token_start;
}

// Kabul edilen karakterlerin uzunluğu
size_t strspn(const char *str1, const char *str2) {
    if (!str1 || !str2) return 0;
    
    size_t count = 0;
    
    while (str1[count] && strchr(str2, str1[count])) {
        count++;
    }
    
    return count;
}

// Reddedilen karakterlerin uzunluğu
size_t strcspn(const char *str1, const char *str2) {
    if (!str1 || !str2) return strlen(str1);
    
    size_t count = 0;
    
    while (str1[count] && !strchr(str2, str1[count])) {
        count++;
    }
    
    return count;
}

// İlk eşleşen karakteri bul
char *strpbrk(const char *str1, const char *str2) {
    if (!str1 || !str2) return NULL;
    
    while (*str1) {
        if (strchr(str2, *str1)) {
            return (char*)str1;
        }
        str1++;
    }
    
    return NULL;
}

// Yardımcı fonksiyonlar
char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

char to_upper(char c) {
    if (c >= 'a' && c <= 'z') {
        return c - ('a' - 'A');
    }
    return c;
}

int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int is_digit(char c) {
    return c >= '0' && c <= '9';
}

int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
