#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <list.h>

typedef struct {
    uint32_t mode_number;
    size_t width;
    size_t height;
    size_t bpp;
    size_t pitch;
    void* framebuffer;
    bool linear_framebuffer;
} ScreenVideoModeInfo;

typedef struct {
    uint8_t id;
    char* name;
    ScreenVideoModeInfo* mode;
    List* video_modes;
} ScreenInfo;

extern ScreenInfo main_screen;

extern List* screen_list;

extern void screen_changeVideoMode(ScreenInfo* screen, ScreenVideoModeInfo* mode);

#ifdef __cplusplus
}
#endif
