#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Configure an identity-mapped MMIO range with architecture-appropriate cache attributes. */
bool mmio_configure_region(uintptr_t phys_start, size_t length);

#ifdef __cplusplus
}
#endif
