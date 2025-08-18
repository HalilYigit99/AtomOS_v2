#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint32_t width;
    uint32_t height;
} gfx_size;

typedef struct {
    int x;
    int y;
} gfx_point;

typedef union {
    uint32_t argb;
    struct {
        uint8_t b; // Blue
        uint8_t g; // Green
        uint8_t r; // Red
        uint8_t a; // Alpha
    };
} gfx_color;

typedef struct {
    gfx_size size;
    gfx_point position;
    gfx_color color;
} gfx_rect;

typedef struct {
    size_t radius;
    gfx_point position;
    gfx_color color;
} gfx_circle;

typedef struct {
    gfx_point start;
    gfx_point end;
    gfx_color color;
    size_t thickness; // Thickness of the line
} gfx_line;

typedef struct {
    gfx_size size;
    void* buffer; // Pointer to the pixel buffer
    uint32_t bpp; // Bits per pixel
    uint8_t drawBeginLineIndex; // Index of the first line to draw
} gfx_buffer;

typedef enum {
    GFX_FONT_BITMAP, // Bitmap font
    GFX_FONT_VECTOR, // Vector font
    GFX_FONT_PSF,    // PostScript font
    GFX_FONT_TTF,    // TrueType font
    GFX_FONT_OTF     // OpenType font
} gfx_font_type;

typedef struct {
    char* name; // Font name
    gfx_size size; // Font size
    uint32_t* glyphs; // Pointer to glyph data (bitmap or vector)
    gfx_font_type type; // Type of the font
} gfx_font;

typedef struct {
    gfx_size size; // Size of the bitmap
    uint8_t* pixels; // Pointer to pixel data
} gfx_bitmap;

// Global değişkenler
extern gfx_buffer* screen_buffer; // Global screen buffer
extern gfx_font* default_font; // Global default font

extern gfx_font gfx_font8x8; // 8x8 bitmap font
extern gfx_font gfx_font8x16; // 8x16 bitmap font



#ifdef __cplusplus
}
#endif