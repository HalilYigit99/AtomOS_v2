#include <event/event.h>
#include <memory/memory.h>

Event* event_create(const char* name) {
    Event* event = (Event*)malloc(sizeof(Event));
    if (!event) {
        return NULL;
    }
    event->name = strdup(name);
    event->invoke_count = 0;
    event->enabled = true;
    event->last_invoke_time = 0;
    event->callbacks = List_Create();
    return event;
}

void event_destroy(Event* event) {
    if (!event) {
        return;
    }
    // Free all callbacks
    List_Destroy(event->callbacks, true);
    free(event->name);
    free(event);
}

void event_register_callback(Event* event, EventCallback callback) {
    if (!event || !callback) {
        return;
    }
    List_Add(&event->callbacks, callback);
}

void event_unregister_callback(Event* event, EventCallback callback) {
    if (!event || !callback) {
        return;
    }
    List_Remove(&event->callbacks, callback);
}

void event_invoke(Event* event, void* context) {
    if (!event || !event->enabled) {
        return;
    }
    event->invoke_count++;
    // Here we would normally get the current time
    event->last_invoke_time = 0; // Placeholder for current time

    ListNode* current = event->callbacks->head;
    while (current) {
        EventCallback callback = (EventCallback)current->data;
        if (callback) {
            callback(context);
        }
        current = current->next;
    }
}
