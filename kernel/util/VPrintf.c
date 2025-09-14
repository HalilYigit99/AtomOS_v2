#include <util/VPrintf.h>
#include <util/convert.h>
#include <stdint.h>

// Printf format flags
typedef struct {
    bool leftAlign;      // '-' flag
    bool showSign;       // '+' flag  
    bool spaceSign;      // ' ' flag
    bool alternate;      // '#' flag
    bool zeroPad;        // '0' flag
    int width;           // Field width
    int precision;       // Precision
    bool hasPrecision;   // Precision specified
} FormatFlags;

static int strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static int parseNumber(const char** format) {
    int num = 0;
    while (**format >= '0' && **format <= '9') {
        num = num * 10 + (**format - '0');
        (*format)++;
    }
    return num;
}

static FormatFlags parseFlags(const char** format) {
    FormatFlags flags = {0};
    flags.precision = 6; // Default precision for floating point
    
    // Parse flags
    bool parsing = true;
    while (parsing) {
        switch (**format) {
            case '-': flags.leftAlign = true; (*format)++; break;
            case '+': flags.showSign = true; (*format)++; break;
            case ' ': flags.spaceSign = true; (*format)++; break;
            case '#': flags.alternate = true; (*format)++; break;
            case '0': 
                if (!flags.leftAlign) { // '0' flag is ignored if '-' flag is present
                    flags.zeroPad = true; 
                }
                (*format)++; 
                break;
            default: parsing = false; break;
        }
    }
    
    // Parse width (including leading zeros)
    if (**format >= '0' && **format <= '9') {
        // If we see a '0' after flags, this could be width with zero padding
        if (**format == '0' && !flags.zeroPad) {
            flags.zeroPad = true;
            (*format)++;
        }
        flags.width = parseNumber(format);
    } else if (**format == '*') {
        flags.width = -1; // Will be read from va_list
        (*format)++;
    }
    
    // Parse precision
    if (**format == '.') {
        (*format)++;
        flags.hasPrecision = true;
        if (**format >= '0' && **format <= '9') {
            flags.precision = parseNumber(format);
        } else if (**format == '*') {
            flags.precision = -1; // Will be read from va_list
            (*format)++;
        } else {
            flags.precision = 0;
        }
    }
    
    return flags;
}

static int printPadding(void(*putChar)(char), int count, char padChar) {
    int printed = 0;
    for (int i = 0; i < count; i++) {
        putChar(padChar);
        printed++;
    }
    return printed;
}

static int printString(void(*putChar)(char), const char* str, FormatFlags flags) {
    int printed = 0;
    if (!str) str = "(null)";
    
    int len = strlen(str);
    if (flags.hasPrecision && flags.precision < len) {
        len = flags.precision;
    }
    
    int padding = flags.width - len;
    
    if (!flags.leftAlign && padding > 0) {
        printed += printPadding(putChar, padding, ' ');
    }
    
    for (int i = 0; i < len; i++) {
        putChar(str[i]);
        printed++;
    }
    
    if (flags.leftAlign && padding > 0) {
        printed += printPadding(putChar, padding, ' ');
    }
    
    return printed;
}

static int printNumber(void(*putChar)(char), long long value, int base, bool uppercase, FormatFlags flags) {
    int printed = 0;
    char buffer[66]; // Enough for 64-bit binary + sign + null
    
    // Manual number conversion to ensure it works
    char* ptr = buffer;
    bool isNegative = false;
    
    if (value < 0 && base == 10) {
        isNegative = true;
        value = -value;
    }
    
    // Convert to string manually
    int i = 0;
    if (value == 0) {
        ptr[i++] = '0';
    } else {
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        
        // Build string in reverse using proper 64-bit arithmetic
        long long temp = value;
        while (temp > 0) {
            long long remainder = temp % base;
            ptr[i++] = digits[remainder];
            temp = temp / base;
        }
        
        // Add negative sign
        if (isNegative) {
            ptr[i++] = '-';
        }
        
        // Reverse the string
        for (int j = 0; j < i / 2; j++) {
            char temp_char = ptr[j];
            ptr[j] = ptr[i - 1 - j];
            ptr[i - 1 - j] = temp_char;
        }
    }
    
    ptr[i] = '\0';
    
    int len = i;
    bool hasSign = (isNegative || ptr[0] == '-');
    
    // Calculate prefix length
    int prefixLen = 0;
    if (hasSign) prefixLen = 1;
    else if (flags.showSign) prefixLen = 1;
    else if (flags.spaceSign) prefixLen = 1;
    
    if (flags.alternate) {
        if (base == 8 && buffer[0] != '0') prefixLen += 1;
        else if (base == 16 && value != 0) prefixLen += 2;
    }
    
    int totalLen = len + (hasSign ? 0 : prefixLen);
    int padding = flags.width - totalLen;
    
    // Print left padding (spaces)
    if (!flags.leftAlign && !flags.zeroPad && padding > 0) {
        printed += printPadding(putChar, padding, ' ');
    }
    
    // Print prefix
    if (hasSign) {
        putChar(ptr[0]); // This is the '-' sign
        printed++;
    } else if (flags.showSign) {
        putChar('+');
        printed++;
    } else if (flags.spaceSign) {
        putChar(' ');
        printed++;
    }
    
    if (flags.alternate) {
        if (base == 8 && buffer[0] != '0') {
            putChar('0');
            printed++;
        } else if (base == 16 && value != 0) {
            putChar('0');
            putChar(uppercase ? 'X' : 'x');
            printed += 2;
        }
    }
    
    // Print zero padding
    if (!flags.leftAlign && flags.zeroPad && padding > 0) {
        printed += printPadding(putChar, padding, '0');
    }
    
    // Print number (skip sign if already printed)
    const char* numStart = hasSign ? buffer + 1 : buffer;
    while (*numStart) {
        putChar(*numStart++);
        printed++;
    }
    
    // Print right padding
    if (flags.leftAlign && padding > 0) {
        printed += printPadding(putChar, padding, ' ');
    }
    
    return printed;
}

static int printUnsignedNumber(void(*putChar)(char), unsigned long long value, int base, bool uppercase, FormatFlags flags) {
    int printed = 0;
    char buffer[65]; // Enough for 64-bit binary + null
    
    char* ptr = buffer;
    int i = 0;
    
    if (value == 0) {
        ptr[i++] = '0';
    } else {
        const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
        
        // Working version - bypass gcc.asm issues
        if (value <= 0xFFFFFFFFUL) {
            // 32-bit path - safe to use normal division
            unsigned int temp32 = (unsigned int)value;
            while (temp32 > 0) {
                ptr[i++] = digits[temp32 % base];
                temp32 = temp32 / base;
            }
        } else {
            // 64-bit path - use bit operations for hex, manual for decimal
            if (base == 16) {
                // Hex: extract 4-bit nibbles
                unsigned long long temp = value;
                while (temp > 0) {
                    ptr[i++] = digits[temp & 0xF];
                    temp >>= 4;
                }
            } else if (base == 10) {
                // Decimal: powers of 10 method
                unsigned long long temp = value;
                unsigned long long powers[20] = {
                    1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL,
                    100000ULL, 1000000ULL, 10000000ULL, 100000000ULL, 1000000000ULL,
                    10000000000ULL, 100000000000ULL, 1000000000000ULL, 10000000000000ULL,
                    100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL,
                    100000000000000000ULL, 1000000000000000000ULL, 10000000000000000000ULL
                };
                
                // Find highest power
                int powerIndex = 19;
                while (powerIndex >= 0 && temp < powers[powerIndex]) powerIndex--;
                
                // Extract digits
                for (int p = powerIndex; p >= 0; p--) {
                    int digit = 0;
                    while (temp >= powers[p]) {
                        temp -= powers[p];
                        digit++;
                    }
                    if (digit > 0 || i > 0) {
                        ptr[i++] = digits[digit];
                    }
                }
                
                if (i == 0) ptr[i++] = '0';
                goto skip_reverse; // Already in correct order
            } else {
                // Other bases: fallback to hex
                while (value > 0) {
                    ptr[i++] = digits[value & 0xF];
                    value >>= 4;
                }
            }
        }
        
        // Reverse the string (except decimal which is already correct)
        for (int j = 0; j < i / 2; j++) {
            char temp_char = ptr[j];
            ptr[j] = ptr[i - 1 - j];
            ptr[i - 1 - j] = temp_char;
        }
        
        skip_reverse:;
    }
    
    ptr[i] = '\0';
    
    int len = i;
    int prefixLen = 0;
    
    if (flags.alternate) {
        if (base == 8 && buffer[0] != '0') prefixLen = 1;
        else if (base == 16 && value != 0) prefixLen = 2;
    }
    
    int totalLen = len + prefixLen;
    int padding = flags.width - totalLen;
    
    // Print left padding
    if (!flags.leftAlign && !flags.zeroPad && padding > 0) {
        printed += printPadding(putChar, padding, ' ');
    }
    
    // Print prefix
    if (flags.alternate) {
        if (base == 8 && buffer[0] != '0') {
            putChar('0');
            printed++;
        } else if (base == 16 && value != 0) {
            putChar('0');
            putChar(uppercase ? 'X' : 'x');
            printed += 2;
        }
    }
    
    // Print zero padding
    if (!flags.leftAlign && flags.zeroPad && padding > 0) {
        printed += printPadding(putChar, padding, '0');
    }
    
    // Print number
    const char* numPtr = buffer;
    while (*numPtr) {
        putChar(*numPtr++);
        printed++;
    }
    
    // Print right padding
    if (flags.leftAlign && padding > 0) {
        printed += printPadding(putChar, padding, ' ');
    }
    
    return printed;
}

static int printFloat(void(*putChar)(char), double value, FormatFlags flags) {
    char buffer[128];
    int precision = flags.hasPrecision ? flags.precision : 6;
    
    // Convert to string
    dtoa(value, buffer, precision);
    
    // Print as string with formatting
    return printString(putChar, buffer, flags);
}

int vprintf(void(*putChar)(char), const char* format, va_list list) {
    int printed = 0;
    
    while (*format) {
        if (*format != '%') {
            putChar(*format++);
            printed++;
            continue;
        }
        
        format++; // Skip '%'
        
        // Check for %%
        if (*format == '%') {
            putChar('%');
            printed++;
            format++;
            continue;
        }
        
        // Parse flags
        FormatFlags flags = parseFlags(&format);
        
        // Get width from va_list if needed
        if (flags.width == -1) {
            flags.width = va_arg(list, int);
            if (flags.width < 0) {
                flags.leftAlign = true;
                flags.width = -flags.width;
            }
        }
        
        // Get precision from va_list if needed
        if (flags.hasPrecision && flags.precision == -1) {
            flags.precision = va_arg(list, int);
            if (flags.precision < 0) {
                flags.hasPrecision = false;
            }
        }
        
        // Parse length modifiers
        int length = 0; // 0=int, 1=long, 2=long long
        bool isShort = false;
        bool isChar = false;
        
        if (*format == 'h') {
            format++;
            if (*format == 'h') {
                isChar = true;
                format++;
            } else {
                isShort = true;
            }
        } else if (*format == 'l') {
            format++;
            if (*format == 'l') {
                length = 2;
                format++;
            } else {
                length = 1;
            }
        } else if (*format == 'z' || *format == 't') {
            length = sizeof(size_t) == sizeof(long) ? 1 : 2;
            format++;
        }
        
        // Handle format specifier
        switch (*format) {
            case 'd':
            case 'i': {
                long long value;
                if (isChar) value = (char)va_arg(list, int);
                else if (isShort) value = (short)va_arg(list, int);
                else if (length == 0) value = va_arg(list, int);
                else if (length == 1) value = va_arg(list, long);
                else value = va_arg(list, long long);
                
                printed += printNumber(putChar, value, 10, false, flags);
                break;
            }
            
            case 'u': {
                unsigned long long value;
                if (isChar) value = (unsigned char)va_arg(list, unsigned int);
                else if (isShort) value = (unsigned short)va_arg(list, unsigned int);
                else if (length == 0) value = va_arg(list, unsigned int);
                else if (length == 1) value = va_arg(list, unsigned long);
                else value = va_arg(list, unsigned long long);
                
                printed += printUnsignedNumber(putChar, value, 10, false, flags);
                break;
            }
            
            case 'o': {
                unsigned long long value;
                if (isChar) value = (unsigned char)va_arg(list, unsigned int);
                else if (isShort) value = (unsigned short)va_arg(list, unsigned int);
                else if (length == 0) value = va_arg(list, unsigned int);
                else if (length == 1) value = va_arg(list, unsigned long);
                else value = va_arg(list, unsigned long long);
                
                printed += printUnsignedNumber(putChar, value, 8, false, flags);
                break;
            }
            
            case 'x':
            case 'X': {
                unsigned long long value;
                if (isChar) value = (unsigned char)va_arg(list, unsigned int);
                else if (isShort) value = (unsigned short)va_arg(list, unsigned int);
                else if (length == 0) value = va_arg(list, unsigned int);
                else if (length == 1) value = va_arg(list, unsigned long);
                else value = va_arg(list, unsigned long long);
                
                printed += printUnsignedNumber(putChar, value, 16, *format == 'X', flags);
                break;
            }
            
            case 'c': {
                char c = (char)va_arg(list, int);
                if (!flags.leftAlign && flags.width > 1) {
                    printed += printPadding(putChar, flags.width - 1, ' ');
                }
                putChar(c);
                printed++;
                if (flags.leftAlign && flags.width > 1) {
                    printed += printPadding(putChar, flags.width - 1, ' ');
                }
                break;
            }
            
            case 's': {
                const char* str = va_arg(list, const char*);
                printed += printString(putChar, str, flags);
                break;
            }
            
            case 'p': {
                void* ptr = va_arg(list, void*);
                
                // Manual pointer formatting
                uintptr_t value = (uintptr_t)ptr;
                char buffer[20];
                
                // Add "0x" prefix
                buffer[0] = '0';
                buffer[1] = 'x';
                
                // Convert to hex manually
                char* ptr_buf = buffer + 2;
                int i = 0;
                
                if (value == 0) {
                    ptr_buf[i++] = '0';
                } else {
                    const char* digits = "0123456789abcdef";
                    // Build in reverse
                    uintptr_t temp = value;
                    while (temp > 0) {
                        ptr_buf[i++] = digits[temp & 0xF];
                        temp >>= 4;
                    }
                    
                    // Reverse the hex part
                    for (int j = 0; j < i / 2; j++) {
                        char temp_char = ptr_buf[j];
                        ptr_buf[j] = ptr_buf[i - 1 - j];
                        ptr_buf[i - 1 - j] = temp_char;
                    }
                }
                
                ptr_buf[i] = '\0';
                printed += printString(putChar, buffer, flags);
                break;
            }
            
            case 'f':
            case 'F': {
                double value = va_arg(list, double);
                printed += printFloat(putChar, value, flags);
                break;
            }
            
            case 'e':
            case 'E':
            case 'g':
            case 'G': {
                // For now, treat as 'f'
                double value = va_arg(list, double);
                printed += printFloat(putChar, value, flags);
                break;
            }
            
            case 'n': {
                // Number of characters written so far
                int* ptr = va_arg(list, int*);
                if (ptr) *ptr = printed;
                break;
            }
            
            default:
                // Unknown format, just print it
                putChar('%');
                putChar(*format);
                printed += 2;
                break;
        }
        
        format++;
    }

    return printed;
}