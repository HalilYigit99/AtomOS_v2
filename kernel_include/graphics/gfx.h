#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <graphics/types.h>

extern size_t screen_width;
extern size_t screen_height;
extern size_t gfx_videoModeCount;
extern uint8_t screen_bpp;

gfx_buffer* gfx_create_buffer(size_t width, size_t height);
void gfx_destroy_buffer(gfx_buffer* buffer);

void gfx_screen_register_buffer(gfx_buffer* buffer);
void gfx_screen_unregister_buffer(gfx_buffer* buffer);

void gfx_clear_buffer(gfx_buffer* buffer, gfx_color color);

void gfx_draw_pixel(gfx_buffer* buffer, int x, int y, gfx_color color);
void gfx_draw_line(gfx_buffer* buffer, int x1, int y1, int x2, int y2, gfx_color color);
void gfx_draw_rectangle(gfx_buffer* buffer, int x, int y, int width, int height, gfx_color color);
void gfx_fill_rectangle(gfx_buffer* buffer, int x, int y, int width, int height, gfx_color color);
void gfx_draw_circle(gfx_buffer* buffer, int x, int y, int radius, gfx_color color);
void gfx_fill_circle(gfx_buffer* buffer, int x, int y, int radius, gfx_color color);
void gfx_draw_triangle(gfx_buffer* buffer, int x1, int y1, int x2, int y2, int x3, int y3, gfx_color color);
void gfx_fill_triangle(gfx_buffer* buffer, int x1, int y1, int x2, int y2, int x3, int y3, gfx_color color);

// Font rendering functions - Dinamik font desteği
// Şu anda sadece GFX_FONT_BITMAP destekleniyor
// Gelecekte eklenecek: GFX_FONT_VECTOR, GFX_FONT_PSF, GFX_FONT_TTF, GFX_FONT_OTF
void gfx_draw_char(gfx_buffer* buffer, int x, int y, char c, gfx_color color, gfx_font* font);
void gfx_draw_text(gfx_buffer* buffer, int x, int y, char* text, gfx_color color, gfx_font* font);

void gfx_draw_bitmap(gfx_buffer* buffer, int x, int y, void* bitmap, size_t width, size_t height);

bool gfx_resize_buffer(gfx_buffer* buffer, size_t newWidth, size_t newHeight);

gfx_videomode* get_video_mode(size_t index);

#ifdef __cplusplus
}
#endif