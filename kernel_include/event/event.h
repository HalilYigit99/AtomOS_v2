#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <list.h>

typedef void (*EventCallback)(void* context);

typedef struct {
    char* name;
    size_t invoke_count;
    bool enabled;
    uint64_t last_invoke_time;
    List* callbacks;
} Event;

Event* event_create(const char* name);
void event_destroy(Event* event);

void event_register_callback(Event* event, EventCallback callback);
void event_unregister_callback(Event* event, EventCallback callback);

void event_invoke(Event* event, void* args);

void event_enable(Event* event);
void event_disable(Event* event);

#ifdef __cplusplus
}
#endif
