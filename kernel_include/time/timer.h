#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char* name; // Name of the timer
    size_t frequency; // Frequency of the timer in Hz
    void* context; // Pointer to the timer context
    void (*init)(); // Function to initialize the timer
    int (*start)(); // Function to start the timer
    int (*stop)(); // Function to stop the timer
    int (*setPrescaler)(uint32_t prescaler); // Function to set the timer prescaler
    int (*setFrequency)(uint32_t frequency); // Function to set the timer frequency
    void (*add_callback)(void (*callback)()); // Function to add a callback
    void (*remove_callback)(void (*callback)()); // Function to remove a callback
} HardwareTimer;

typedef struct {
    char* name; // Name of the timer
    bool active; // Indicates if the timer is active
    size_t intervalMs; // Interval in milliseconds
    size_t lastTick; // Last tick time in milliseconds
    void (*onTick)(); // Callback function to be called when the timer expires
} Timer;

Timer* create_timer(const char* name, size_t intervalMs, void (*onTick)());
void start_timer(Timer* timer);
void stop_timer(Timer* timer);
void delete_timer(Timer* timer);

extern HardwareTimer* pit_timer; // Pointer to the PIT timer
extern HardwareTimer* rtc_timer; // Pointer to the RTC timer
extern HardwareTimer* acpi_timer; // Pointer to the ACPI timer
extern HardwareTimer* hpet_timer; // Pointer to the HPET timer

extern HardwareTimer* apic_timers[]; // Array of APIC timers
extern size_t apic_timer_count; // Count of APIC timers

extern uint64_t uptimeMs; // System uptime in milliseconds

#ifdef __cplusplus
}
#endif
