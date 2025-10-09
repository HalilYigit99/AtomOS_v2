#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Platform algılama ve UINTN/INTN tanımlamaları */

void* efi_gop_get_framebuffer();


#ifdef __cplusplus
};
#endif
