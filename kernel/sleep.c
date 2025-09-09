#include <sleep.h>
#include <time/timer.h>

void sleep_ms(uint32_t milliseconds)
{
    if (milliseconds == 0) return;

    uint64_t endTime = uptimeMs + milliseconds;
    while (uptimeMs < endTime) {
        asm volatile ("hlt");
    }
}
