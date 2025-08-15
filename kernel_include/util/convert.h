#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Sayı -> string dönüştürme fonksiyonları (heap yok, isteğe bağlı statik buffer)
// buffer NULL verilirse thread-safe olmayan dahili statik buffer döner.

// Taban aralığı: 2..36 aksi halde NULL döner.
char* itoa(int value, char* buffer, int base);          // signed int
char* ltoa(long value, char* buffer, int base);         // signed long
char* lltoa(long long value, char* buffer, int base);   // signed long long
char* utoa(unsigned value, char* buffer, int base);     // unsigned int
char* ultoa(unsigned long value, char* buffer, int base);
char* ulltoa(unsigned long long value, char* buffer, int base);

// Double -> string. precision: 0..18 (outsiders clamp). "nan", "inf", "-inf" desteklenir.
char* dtoa(double value, char* buffer, int precision);




#ifdef __cplusplus
}
#endif