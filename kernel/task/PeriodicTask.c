#include <task/PeriodicTask.h>
#include <list.h>
#include <time/timer.h>
#include <memory/memory.h>
#include <util/string.h>

List* periodicTasks = NULL;

PeriodicTask* periodic_task_create(const char* name, void(*taskFunction)(void* task, void* arg), void* arg, size_t intervalMs)
{
    if (!periodicTasks)
    {
        periodicTasks = List_Create();
    }

    PeriodicTask* task = (PeriodicTask*)malloc(sizeof(PeriodicTask));
    task->name = (char*)malloc(strlen(name) + 1);
    strcpy(task->name, name);
    task->taskFunction = taskFunction;
    task->arg = arg;
    task->intervalMs = intervalMs;
    task->lastRunMs = 0;
    task->running = false;

    List_Add(periodicTasks, task);

    return task;
}

void periodic_task_start(PeriodicTask* task)
{
    if (task)
    {
        task->running = true;
        task->lastRunMs = 0; // Reset last run time to ensure immediate execution
    }
}

void periodic_task_stop(PeriodicTask* task)
{
    if (task)
    {
        task->running = false;
    }
}

void periodic_task_destroy(PeriodicTask* task)
{
    if (task)
    {
        if (periodicTasks)
        {
            List_Remove(periodicTasks, task);
        }
        free(task->name);
        free(task);
    }
}

void periodic_task_run_all()
{
    if (!periodicTasks)
        return;

    uint64_t currentTimeMs = uptimeMs; // Assume uptimeMs is a global variable tracking system uptime in milliseconds

    for (ListNode* node = periodicTasks->head; node != NULL; node = node->next)
    {
        PeriodicTask* task = (PeriodicTask*)node->data;
        if (task->running && (currentTimeMs - task->lastRunMs >= task->intervalMs))
        {
            if (task->taskFunction) task->taskFunction((void*)task, task->arg);
            task->lastRunMs = currentTimeMs;
        }
    }

}
