#include <driver/DriverBase.h>
#include <list.h>
#include <debug/debug.h>

List* system_driver_list = NULL;

bool system_driver_is_available(DriverBase* driver) {
    if (system_driver_list == NULL || driver == NULL) {
        return false;
    }

    ListNode* current = system_driver_list->head;
    while (current) {
        if (current->data == driver) {
            return true;
        }
        current = current->next;
    }
    return false;
}

void system_driver_register(DriverBase* driver) {
    if (system_driver_list == NULL) {
        system_driver_list = List_Create();
    }

    if (
        !driver ||
        !driver->name ||
        !driver->init ||
        !driver->enable ||
        !driver->disable
    ) {
        ERROR("Invalid driver registration attempt.");
        return;
    }

    List_Add(system_driver_list, driver);

    if (driver->init) {
        if (!driver->init()) {
            ERROR("Failed to initialize driver: '%s'", driver->name);
            return;
        }
    } else {
        WARN("Driver '%s' does not have an init function.", driver->name);
    }

}

void system_driver_unregister(DriverBase* driver) {
    if (system_driver_list == NULL || driver == NULL) {
        ERROR("No drivers registered or invalid driver for unregistration.");
        return;
    }

    if (driver->disable)
    {
        driver->disable(); // Ensure the driver is disabled before removal
    }

    // Assuming List_Remove is a function that removes an item from the list
    if (!List_Remove(system_driver_list, driver)) {
        ERROR("Failed to unregister driver: '%s'", driver->name);
    }
}

void system_driver_enable(DriverBase* driver)
{

    if (driver->name == NULL) {
        ERROR("Driver name is NULL, cannot enable.");
        return;
    }

    if (driver->enable) {
        driver->enable();
        LOG("Driver '%s' enabled.", driver->name);
    }else {
        WARN("Driver '%s' does not have an enable function.", driver->name);
    }
}

void system_driver_disable(DriverBase* driver)
{
    if (driver->name == NULL) {
        ERROR("Driver name is NULL, cannot disable.");
        return;
    }

    if (driver->disable) {
        driver->disable();
        LOG("Driver '%s' disabled.", driver->name);
    } else {
        WARN("Driver '%s' does not have a disable function.", driver->name);
    }
}
