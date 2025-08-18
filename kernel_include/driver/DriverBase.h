#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    DRIVER_TYPE_ANY = 0,
    DRIVER_TYPE_HID = 1,        // Human Interface Device (e.g., keyboard, mouse)
    DRIVER_TYPE_STORAGE = 2,    // Storage devices (e.g., ATA, SATA)
    DRIVER_TYPE_NETWORK = 3,    // Network devices (e.g., Ethernet, Wi-Fi)
    DRIVER_TYPE_GRAPHICS = 4,   // Graphics devices (e.g., GPUs)
    DRIVER_TYPE_FILESYSTEM = 5, // Filesystem drivers
    DRIVER_TYPE_AUDIO = 6,      // Audio devices
} DriverType;

typedef struct {
    char* name;          // Name of the driver
    bool enabled;       // Whether the driver is enabled
    uint32_t version;    // Version of the driver
    void* context;       // Pointer to driver-specific context data
    bool (*init)();  // Function pointer for initialization
    void (*enable)(); // Function pointer to enable the driver
    void (*disable)(); // Function pointer to disable the driver
    DriverType type;    // Type of the driver
} DriverBase;

void system_driver_register(DriverBase* driver);
void system_driver_unregister(DriverBase* driver);

void system_driver_enable(DriverBase* driver);
void system_driver_disable(DriverBase* driver);

#ifdef __cplusplus
}
#endif