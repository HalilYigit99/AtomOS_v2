#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>


typedef struct {
    char* name;
    void(*taskFunction)(void* task, void* arg);
    void* arg;
    size_t intervalMs;
    uint64_t lastRunMs;
    bool running;
} PeriodicTask;

PeriodicTask* periodic_task_create(const char* name, void(*taskFunction)(void* task, void* arg), void* arg, size_t intervalMs);
void periodic_task_start(PeriodicTask* task);
void periodic_task_stop(PeriodicTask* task);

void periodic_task_destroy(PeriodicTask* task);

void periodic_task_run_all();

#ifdef __cplusplus
}
#endif
