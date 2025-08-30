#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char* name;
    void* specific_data;

    void (*init)(void);
    void (*enable)(uint32_t irq);
    void (*disable)(uint32_t irq);
    void (*acknowledge)(uint32_t irq);
    void (*set_priority)(uint32_t irq, uint8_t priority);
    uint8_t (*get_priority)(uint32_t irq);
    bool (*is_enabled)(uint32_t irq);
    void (*register_handler)(uint32_t irq, void (*handler)(void));
    void (*unregister_handler)(uint32_t irq);

    // GSI-based operations (for APIC/IOAPIC). PIC maps GSI to IRQ (GSI==IRQ).
    void (*enable_gsi)(uint32_t gsi);
    void (*disable_gsi)(uint32_t gsi);
    void (*acknowledge_gsi)(uint32_t gsi);
    void (*set_priority_gsi)(uint32_t gsi, uint8_t priority);
    uint8_t (*get_priority_gsi)(uint32_t gsi);
    bool (*is_enabled_gsi)(uint32_t gsi);
    void (*register_handler_gsi)(uint32_t gsi, void (*handler)(void));
    void (*unregister_handler_gsi)(uint32_t gsi);
} IRQController;

extern IRQController *irq_controller;

#ifdef __cplusplus
}
#endif
