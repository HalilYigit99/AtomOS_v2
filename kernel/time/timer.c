#include <time/timer.h>

HardwareTimer* rtc_timer; // Pointer to the RTC timer
HardwareTimer* acpi_timer; // Pointer to the ACPI timer
HardwareTimer* hpet_timer; // Pointer to the HPET timer

HardwareTimer* apic_timers[] = {}; // Array of APIC timers
size_t apic_timer_count; // Count of APIC timers

uint64_t uptimeMs; // System uptime in milliseconds
