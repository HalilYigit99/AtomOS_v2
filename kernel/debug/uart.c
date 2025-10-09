#include <debug/uart.h>
#include <util/VPrintf.h>
#include <arch.h>
#include <debug/debug.h>

void uart_open() {
    // Implementation for opening UART

    #define COM1_PORT 0x3F8
    static int uart_initialized = 0;
    if (uart_initialized) return;

    /* Disable interrupts */
    outb(COM1_PORT + 1, 0x00);
    /* Enable DLAB to set baud rate */
    outb(COM1_PORT + 3, 0x80);
    /* Set baud rate divisor (115200 baud => divisor 1) */
    outb(COM1_PORT + 0, 0x01);
    outb(COM1_PORT + 1, 0x00);
    /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 3, 0x03);
    /* Enable FIFO, clear them, 14-byte threshold */
    outb(COM1_PORT + 2, 0xC7);
    /* IRQs enabled, RTS/DSR set */
    outb(COM1_PORT + 4, 0x0B);

    /* Mark initialized */
    uart_initialized = 1;

}

void uart_close() {
    // Implementation for closing UART

    #define COM1_PORT 0x3F8
    /* Disable all UART interrupts */
    outb(COM1_PORT + 1, 0x00);
    /* Disable FIFO */
    outb(COM1_PORT + 2, 0x00);
    /* Reset line control (DLAB cleared, 8N1 not needed when closed) */
    outb(COM1_PORT + 3, 0x00);
    /* Drop RTS/DTR and disable OUT pins */
    outb(COM1_PORT + 4, 0x00);
    /* Read line status and data registers to clear pending states */
    (void)inb(COM1_PORT + 5);
    (void)inb(COM1_PORT + 0);

}

void uart_write_char(char c) {
    // Implementation for writing a character to UART

    #define COM1_PORT 0x3F8
    /* Wait for the transmit buffer to be empty */
    while ((inb(COM1_PORT + 5) & 0x20) == 0) {
        asm volatile ("pause");
    }

    if (c == '\n') {
        /* Convert newline to carriage return + newline */
        outb(COM1_PORT + 0, '\r');
        while ((inb(COM1_PORT + 5) & 0x20) == 0) {
            asm volatile ("pause");
        }
    }

    /* Write the character to the data register */
    outb(COM1_PORT + 0, c);

}

void uart_write_string(const char* str) {
    // Implementation for writing a string to UART

    char* str_ = (char*)str;

    while (*str_) {
        uart_write_char(*str_);
        str_++;
    }

}

void uart_print(const char* str) {
    // Implementation for printing a string to UART

    char* str_ = (char*)str;

    while (*str_) {
        uart_write_char(*str_);
        str_++;
    }

}

void uart_printf(const char* format, ...) {
    // Implementation for formatted printing to UART

    va_list args;

    va_start(args, format);

    vprintf(uart_write_char, format, args);

    va_end(args);

}

OutputStream uartOutputStream = {
    .Open = uart_open,
    .Close = uart_close,
    .WriteChar = uart_write_char,
    .WriteString = uart_write_string,
    .print = uart_print,
    .printf = uart_printf
};

DebugStream uartDebugStream = {
    .Open = uart_open,
    .Close = uart_close,
    .WriteChar = uart_write_char,
    .WriteString = uart_write_string,
    .print = uart_print,
    .printf = uart_printf
};

bool uart_supported() {
    // Check for UART support by probing COM1 port
    #define COM1_PORT 0x3F8
    /* Try to read the Line Status Register */
    uint8_t lsr = inb(COM1_PORT + 5);
    /* If we can read a valid value, assume UART is present */
    return (lsr != 0xFF);
}
