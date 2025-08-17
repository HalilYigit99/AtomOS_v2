#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    char* name;          // Name of the driver
    uint32_t version;    // Version of the driver
    void* context;       // Pointer to driver-specific context data
    bool (*init)();  // Function pointer for initialization
    void (*enable)(); // Function pointer to enable the driver
    void (*disable)(); // Function pointer to disable the driver

} DriverBase;


#ifdef __cplusplus
}
#endif