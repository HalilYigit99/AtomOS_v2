#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stream/OutputStream.h>

typedef enum UARTBusType {
    UART_BUS_IO_PORT = 0,
    UART_BUS_MMIO    = 1
} UARTBusType;

typedef struct uart_device_info {
    UARTBusType bus;
    bool        present;
    bool        is_console;
    bool        preferred;
    uint16_t    io_port;
    uintptr_t   mmio_phys;
    uintptr_t   mmio_virt;
    size_t      span;
    uint8_t     reg_shift;
    uint8_t     interface_type;
    uint32_t    clock_hz;
    uint32_t    default_baud;
    uint8_t     priority;
    const char* source;
    const char* name;
} uart_device_info;

extern OutputStream uartOutputStream;
void uart_open(void);
void uart_close(void);
void uart_write_char(char c);
void uart_write_string(const char* str);
void uart_print(const char* str);
void uart_printf(const char* format, ...);
bool uart_supported(void);

size_t uart_get_devices(const uart_device_info** out_devices);
const uart_device_info* uart_get_active_device(void);
bool uart_select_device(size_t index);
void uart_refresh_devices(void);

#ifdef __cplusplus
}
#endif
