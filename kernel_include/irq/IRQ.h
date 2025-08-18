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
} IRQController;

extern IRQController *irq_controller;

#ifdef __cplusplus
}
#endif
