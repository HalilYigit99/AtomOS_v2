#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

extern const char* machine_firmware_version;
extern const char* machine_firmware_vendor;
extern const char* machine_firmware_revision;

extern const char* machine_processor_architecture;
extern const char* machine_processor_name;
extern const char* machine_processor_vendor;

extern size_t machine_ramSizeInKB;


#ifdef __cplusplus
}
#endif
