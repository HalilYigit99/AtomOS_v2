#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Mouse cursor position variables
extern int cursor_X;
extern int cursor_Y;

extern bool mouse_enabled;

// Mouse initialization and drawing functions
void mouse_init(void);
void __mouse_draw(void);

// Mouse movement functions
void mouse_set_position(int x, int y);
void mouse_move_relative(int dx, int dy);
void mouse_get_position(int* x, int* y);

// Mouse cursor management
void mouse_show_cursor(void);
void mouse_hide_cursor(void);
int mouse_is_cursor_visible(void);

#ifdef __cplusplus
}
#endif