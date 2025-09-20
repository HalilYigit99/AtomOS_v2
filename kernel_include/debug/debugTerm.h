#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <gfxterm/gfxterm.h>

bool debugterm_is_ready(void);
bool debugterm_ensure_ready(void);
GFXTerminal* debugterm_get(void);
void debugterm_flush(void);

#ifdef __cplusplus
}
#endif
