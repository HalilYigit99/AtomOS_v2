#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <irq/IRQ.h>
#include <driver/DriverBase.h>

/* LAPIC register offsets (MMIO) */
#define LAPIC_REG_ID            0x020
#define LAPIC_REG_EOI           0x0B0
#define LAPIC_REG_SVR           0x0F0
#define LAPIC_REG_LVT_TIMER     0x320
#define LAPIC_REG_LVT_LINT0     0x350
#define LAPIC_REG_LVT_LINT1     0x360
#define LAPIC_REG_LVT_ERROR     0x370

/* LAPIC SVR bits */
#define LAPIC_SVR_APIC_ENABLE   (1u << 8)

/* IOAPIC MMIO offsets relative to IOAPIC base */
#define IOAPIC_MMIO_IOREGSEL    0x00
#define IOAPIC_MMIO_IOWIN       0x10

/* IOAPIC register indices */
#define IOAPIC_REG_ID           0x00
#define IOAPIC_REG_VER          0x01
#define IOAPIC_REG_ARB          0x02
#define IOAPIC_REG_REDIR(n)     (0x10 + (2 * (n)))

/* IOAPIC redirection entry flags (low dword) */
#define IOAPIC_REDIR_MASKED     (1u << 16)
#define IOAPIC_REDIR_LEVEL      (1u << 15)
#define IOAPIC_REDIR_ACTIVE_LOW (1u << 13)

/* High-level APIC driver API (compatible with DriverBase/IRQController) */
bool apic_init(void);
void apic_enable(void);
void apic_disable(void);

void apic_enable_irq(uint32_t irq);
void apic_disable_irq(uint32_t irq);
void apic_acknowledge_irq(uint32_t irq);
void apic_set_priority(uint32_t irq, uint8_t priority);
uint8_t apic_get_priority(uint32_t irq);
bool apic_is_enabled(uint32_t irq);
void apic_register_handler(uint32_t irq, void (*handler)(void));
void apic_unregister_handler(uint32_t irq);

extern DriverBase apic_driver;
extern IRQController apic_irq_controller;

/* Low-level helpers (LAPIC / IOAPIC) */
void lapic_set_base(uintptr_t phys);
void lapic_enable_controller(void);
void lapic_disable_controller(void);
void lapic_eoi(void);
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);
uint8_t lapic_get_id(void);

void ioapic_set_base(uintptr_t phys, uint32_t gsi_base);
uint32_t ioapic_read(uint32_t reg);
void ioapic_write(uint32_t reg, uint32_t value);
uint32_t ioapic_max_redirs(void);
void ioapic_set_redir(uint32_t gsi, uint8_t vector, uint8_t lapic_id, uint32_t flags, bool mask);
void ioapic_mask_gsi(uint32_t gsi, bool mask);
bool ioapic_is_masked(uint32_t gsi);
void ioapic_mask_all(void);

/* Debug helpers */
uint64_t ioapic_read_redir_gsi(uint32_t gsi);
void ioapic_debug_dump_gsi(uint32_t gsi, const char* tag);

#ifdef __cplusplus
}
#endif