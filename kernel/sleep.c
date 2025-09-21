#include <sleep.h>
#include <task/task.h>
#include <time/timer.h>

void sleep_ms(uint32_t milliseconds)
{
    if (milliseconds == 0) {
        return;
    }

    if (task_scheduler_is_active()) {
        task_sleep_ms(milliseconds);
        return;
    }

    uint64_t endTime = uptimeMs + milliseconds;
    while (uptimeMs < endTime) {
        asm volatile ("hlt");
    }
}

