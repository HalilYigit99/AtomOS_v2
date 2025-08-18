#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__x86_64__)
#define ARCH_AMD
#elif defined(__i386__)
#define ARCH_INTEL
#else
#error "Unsupported architecture"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

void idt_set_gate(uint8_t vector, size_t offset);
void idt_reset_gate(uint8_t vector);

size_t idt_get_gate(uint8_t vector);

// Common port I/O interface (architecture-specific implementation)
extern uint8_t  inb(uint16_t port);
extern void     outb(uint16_t port, uint8_t value);
extern uint16_t inw(uint16_t port);
extern void     outw(uint16_t port, uint16_t value);
extern uint32_t inl(uint16_t port);
extern void     outl(uint16_t port, uint32_t value);
extern void     io_wait(void);

#ifdef __cplusplus
}
#endif