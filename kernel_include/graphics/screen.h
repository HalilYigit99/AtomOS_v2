#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <list.h>
#include <event/event.h>

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

/*
    Event invoked when the screen video mode changes.
    The event data is a pointer to the ScreenInfo whose mode has changed.
*/
extern Event* screen_modeChangeEvent;

extern void screen_changeVideoMode(ScreenInfo* screen, ScreenVideoModeInfo* mode);

#ifdef __cplusplus
}
#endif
