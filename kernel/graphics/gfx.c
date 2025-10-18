#include <graphics/gfx.h>
#include <list.h>
#include <memory/memory.h>
#include <boot/multiboot2.h>
#include <stream/OutputStream.h>
#include <math.h>
#include <mouse/mouse.h>
#include <debug/debug.h>
#include <graphics/screen.h>

List *gfx_buffers;
static bool gfx_buffers_busy;

gfx_buffer *hardware_buffer;
gfx_buffer *screen_buffer;

void gfx_draw_task();

bool gfx_screen_has_buffer(gfx_buffer *buffer)
{
    if (!gfx_buffers)
        return false;

    return List_IndexOf(gfx_buffers, buffer) != -1;
}

void gfx_screen_register_buffer(gfx_buffer *buffer)
{
    gfx_buffers_busy = true;
    if (!gfx_screen_has_buffer(buffer)) List_InsertAt(gfx_buffers, 0, buffer);
    gfx_buffers_busy = false;
}

void gfx_screen_unregister_buffer(gfx_buffer *buffer)
{
    gfx_buffers_busy = true;
    if (gfx_screen_has_buffer(buffer)) List_Remove(gfx_buffers, buffer);
    gfx_buffers_busy = false;
}

void gfx_init()
{

    // Initialize hardware buffer
    hardware_buffer = (gfx_buffer *)malloc(sizeof(gfx_buffer));

    if (!hardware_buffer)
    {
        LOG("Failed to allocate hardware buffer");
        asm volatile("cli; hlt"); // Halt the system
    }

    hardware_buffer->size.width = main_screen.mode->width;
    hardware_buffer->size.height = main_screen.mode->height;

    size_t fb_addr = (size_t)main_screen.mode->framebuffer;

    hardware_buffer->buffer = (void *)fb_addr;
    hardware_buffer->bpp = main_screen.mode->bpp;
    hardware_buffer->drawBeginLineIndex = 0;
    hardware_buffer->isDirty = true; // Always true
    hardware_buffer->suppress_draw = false; // default: allow drawing
    hardware_buffer->position = (gfx_point){0, 0};

    gfx_buffers = List_Create();

    if (!gfx_buffers)
    {
        LOG("Failed to create graphics buffer list");
        asm volatile("cli; hlt"); // Halt the system
    }

    // Initialize screen buffer
    LOG("Main screen width: %d , hegiht: %d", (size_t)main_screen.mode->width, (size_t)main_screen.mode->height);
    screen_buffer = gfx_create_buffer(main_screen.mode->width, main_screen.mode->height);
    gfx_clear_buffer(screen_buffer, (gfx_color){.argb = 0xFF000000}); // Clear screen to black
    gfx_screen_register_buffer(screen_buffer);
}

gfx_buffer* gfx_create_buffer(size_t width, size_t height)
{
    if (!gfx_buffers)
    {
        LOG("[ERROR] Graphics buffers list is not initialized");
        asm volatile("cli; hlt"); // Halt the system
    }

    gfx_buffer *buffer = (gfx_buffer *)malloc(sizeof(gfx_buffer));
    if (!buffer)
    {
        LOG("Failed to allocate graphics buffer");
        asm volatile("cli; hlt"); // Halt the system
    }

    buffer->size.width = width;
    buffer->size.height = height;
    buffer->buffer = malloc(width * height * sizeof(uint32_t)); // Assuming 32 bits per pixel

    if (!buffer->buffer)
    {
        LOG("buffer->size.width : %d", (size_t)width);
        LOG("buffer->size.height : %d", (size_t)height);
        LOG("Failed to allocate buffer memory");
        asm volatile("cli; hlt"); // Halt the system
    }

    buffer->bpp = 32; // Assuming 32 bits per pixel
    buffer->drawBeginLineIndex = 0;
    buffer->suppress_draw = false; // default: allow drawing
    buffer->position = (gfx_point){0, 0};

    return buffer;
}

void gfx_destroy_buffer(gfx_buffer *buffer)
{
    if (!buffer)
        return;

    free(buffer->buffer);

    // Remove from the list
    List_Remove(gfx_buffers, buffer);

    free(buffer);
}

void gfx_draw_pixel(gfx_buffer *buffer, int x, int y, gfx_color color)
{

    if (color.a == 0)
        return;

    if (!buffer || x >= (int32_t)buffer->size.width || y >= (int32_t)buffer->size.height)
        return;
    if (x < 0 || y < 0)
        return;

    volatile gfx_color *pixel = (volatile gfx_color *)((size_t)buffer->buffer + (y * buffer->size.width + x) * (buffer->bpp / 8));
    if (buffer->bpp == 32) pixel->argb = color.argb;
    else 
    if (buffer->bpp == 24) {
        // 24 bpp için sadece RGB bileşenlerini ayarla, alfa bileşeni yok
        ((uint8_t*)pixel)[0] = color.b; // Blue
        ((uint8_t*)pixel)[1] = color.g; // Green
        ((uint8_t*)pixel)[2] = color.r; // Red
    }else {
        WARN("Unsupported buffer bpp: %u", buffer->bpp);
    }
    buffer->isDirty = true;
}

void gfx_draw_line(gfx_buffer *buffer, int x1, int y1, int x2, int y2, gfx_color color)
{
    if (!buffer)
        return;

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true)
    {
        gfx_draw_pixel(buffer, x1, y1, color);
        if (x1 == x2 && y1 == y2)
            break;
        int err2 = err * 2;
        if (err2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (err2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }
}

void gfx_draw_rectangle(gfx_buffer *buffer, int x, int y, int width, int height, gfx_color color)
{
    if (!buffer)
        return;

    gfx_draw_line(buffer, x, y, x + width - 1, y, color);
    gfx_draw_line(buffer, x + width - 1, y, x + width - 1, y + height - 1, color);
    gfx_draw_line(buffer, x + width - 1, y + height - 1, x, y + height - 1, color);
    gfx_draw_line(buffer, x, y + height - 1, x, y, color);
}

void gfx_fill_rectangle(gfx_buffer *buffer, int x, int y, int width, int height, gfx_color color)
{
    if (!buffer)
        return;

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            gfx_draw_pixel(buffer, x + j, y + i, color);
        }
    }
}

void gfx_draw_circle(gfx_buffer *buffer, int x, int y, int radius, gfx_color color)
{
    if (!buffer || radius <= 0)
        return;

    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int x1 = 0;
    int y1 = radius;

    gfx_draw_pixel(buffer, x, y + radius, color);
    gfx_draw_pixel(buffer, x, y - radius, color);
    gfx_draw_pixel(buffer, x + radius, y, color);
    gfx_draw_pixel(buffer, x - radius, y, color);

    while (x1 < y1)
    {
        if (f >= 0)
        {
            y1--;
            ddF_y += 2;
            f += ddF_y;
        }
        x1++;
        ddF_x += 2;
        f += ddF_x;

        gfx_draw_pixel(buffer, x + x1, y + y1, color);
        gfx_draw_pixel(buffer, x - x1, y + y1, color);
        gfx_draw_pixel(buffer, x + x1, y - y1, color);
        gfx_draw_pixel(buffer, x - x1, y - y1, color);
        gfx_draw_pixel(buffer, x + y1, y + x1, color);
        gfx_draw_pixel(buffer, x - y1, y + x1, color);
        gfx_draw_pixel(buffer, x + y1, y - x1, color);
        gfx_draw_pixel(buffer, x - y1, y - x1, color);
    }
}

void gfx_fill_circle(gfx_buffer *buffer, int x, int y, int radius, gfx_color color)
{
    if (!buffer || radius <= 0)
        return;

    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int x1 = 0;
    int y1 = radius;

    for (int i = -radius; i <= radius; i++)
    {
        gfx_draw_pixel(buffer, x + i, y + radius, color);
        gfx_draw_pixel(buffer, x + i, y - radius, color);
    }

    while (x1 < y1)
    {
        if (f >= 0)
        {
            y1--;
            ddF_y += 2;
            f += ddF_y;
        }
        x1++;
        ddF_x += 2;
        f += ddF_x;

        for (int i = -x1; i <= x1; i++)
        {
            gfx_draw_pixel(buffer, x + i, y + y1, color);
            gfx_draw_pixel(buffer, x + i, y - y1, color);
        }
        for (int i = -y1; i <= y1; i++)
        {
            gfx_draw_pixel(buffer, x + x1, y + i, color);
            gfx_draw_pixel(buffer, x - x1, y + i, color);
        }
    }
}

void gfx_draw_triangle(gfx_buffer *buffer, int x1, int y1, int x2, int y2, int x3, int y3, gfx_color color)
{
    if (!buffer)
        return;

    gfx_draw_line(buffer, x1, y1, x2, y2, color);
    gfx_draw_line(buffer, x2, y2, x3, y3, color);
    gfx_draw_line(buffer, x3, y3, x1, y1, color);
}

void gfx_fill_triangle(gfx_buffer *buffer, int x1, int y1, int x2, int y2, int x3, int y3, gfx_color color)
{
    if (!buffer)
        return;

    // Sort vertices by y-coordinate
    if (y1 > y2)
    {
        int tmp = y1;
        y1 = y2;
        y2 = tmp;
        tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    if (y1 > y3)
    {
        int tmp = y1;
        y1 = y3;
        y3 = tmp;
        tmp = x1;
        x1 = x3;
        x3 = tmp;
    }
    if (y2 > y3)
    {
        int tmp = y2;
        y2 = y3;
        y3 = tmp;
        tmp = x2;
        x2 = x3;
        x3 = tmp;
    }

    // Calculate the area of the triangle
    float area = 0.5f * (-x2 * y3 + x1 * (-y2 + y3) + x2 * (y1 - y3) + x3 * (y2 - y1));

    for (int i = 0; i <= area; i++)
    {
        float alpha = (float)i / area;
        float beta = (float)(area - i) / area;

        int px = (int)(x1 * alpha + x2 * beta);
        int py = (int)(y1 * alpha + y2 * beta);

        gfx_draw_pixel(buffer, px, py, color);
    }
}

void gfx_draw_char(gfx_buffer *buffer, int x, int y, char c, gfx_color color, gfx_font *font)
{
    if (!buffer || !font || x < 0 || y < 0)
        return;

    // Buffer sınırlarını kontrol et
    if (x + font->size.width > buffer->size.width || y + font->size.height > buffer->size.height)
    {
        return;
    }

    // Karakter indeksini hesapla (ASCII değeri olarak)
    unsigned char char_index = (unsigned char)c;
    if (char_index > 127)
        return; // Şimdilik sadece ASCII karakterleri destekleniyor

    switch (font->type)
    {
    case GFX_FONT_BITMAP:
    {
        // Font verisini unsigned char* olarak cast et
        unsigned char (*bitmap_font)[font->size.height] = (unsigned char (*)[font->size.height])font->glyphs;

        // Her satır için
        for (int row = 0; row < (int32_t)font->size.height; row++)
        {
            unsigned char bitmap_row = bitmap_font[char_index][row];

            // Her bit için (MSB solda, LSB sağda)
            for (int col = 0; col < (int32_t)font->size.width; col++)
            {
                // MSB'den başlayarak bit kontrol et (7, 6, 5, 4, 3, 2, 1, 0)
                if (bitmap_row & (1 << (font->size.width - 1 - col)))
                {
                    gfx_draw_pixel(buffer, x + col, y + row, color);
                }
            }
        }
        break;
    }

    case GFX_FONT_VECTOR:
        // TODO: Vector font desteği eklenecek
        break;

    case GFX_FONT_PSF:
        // TODO: PostScript font desteği eklenecek
        break;

    case GFX_FONT_TTF:
        // TODO: TrueType font desteği eklenecek
        break;

    case GFX_FONT_OTF:
        // TODO: OpenType font desteği eklenecek
        break;

    default:
        // Bilinmeyen font tipi, hiçbir şey yapma
        break;
    }
}

void gfx_draw_text(gfx_buffer *buffer, int x, int y, char *text, gfx_color color, gfx_font *font)
{
    if (!buffer || !text || !font || x < 0 || y < 0)
        return;

    for (int i = 0; text[i] != '\0'; i++)
    {
        gfx_draw_char(buffer, x + i * font->size.width, y, text[i], color, font);
    }
}

void gfx_draw_bitmap(gfx_buffer *buffer, int x, int y, void *bitmap, size_t width, size_t height)
{
    if (!buffer || !bitmap || x < 0 || y < 0 || x + width > buffer->size.width || y + height > buffer->size.height)
        return;

    gfx_color *src = (gfx_color *)bitmap;
    for (size_t j = 0; j < height; j++)
    {
        for (size_t i = 0; i < width; i++)
        {
            gfx_color pixel_color = src[j * width + i];
            gfx_draw_pixel(buffer, x + i, y + j, pixel_color);
        }
    }
}

void gfx_clear_buffer(gfx_buffer *buffer, gfx_color color)
{
    if (!buffer)
        return;

    for (size_t y = 0; y < buffer->size.height; y++)
    {
        for (size_t x = 0; x < buffer->size.width; x++)
        {
            gfx_draw_pixel(buffer, x, y, color);
        }
    }
}

extern void __mouse_draw();

static void gfx_draw_bpp32()
{
    gfx_buffer *buffer = (gfx_buffer *)List_GetAt(gfx_buffers, 0);
    if (!buffer)
        return; // No buffer to draw

    if (buffer->size.width == 0 || buffer->size.height == 0)
    {
        ERROR("Buffer size is zero, cannot draw");
        return;
    }

    size_t copy_width = (buffer->size.width < hardware_buffer->size.width) ? buffer->size.width : hardware_buffer->size.width;
    size_t copy_height = (buffer->size.height < hardware_buffer->size.height) ? buffer->size.height : hardware_buffer->size.height;

    // Fast path: exact match and no vertical offset
    if (buffer->bpp == hardware_buffer->bpp &&
        buffer->size.width == hardware_buffer->size.width &&
        buffer->size.height == hardware_buffer->size.height &&
        buffer->drawBeginLineIndex == 0)
    {
        memcpy((void *)hardware_buffer->buffer, (void *)buffer->buffer,
               buffer->size.width * buffer->size.height * sizeof(uint32_t));
        return;
    }

    // General path: copy line by line honoring drawBeginLineIndex (with wrap)
    const size_t bytes_per_pixel = sizeof(uint32_t);
    const size_t dst_pitch = hardware_buffer->size.width * bytes_per_pixel;
    const size_t src_pitch = buffer->size.width * bytes_per_pixel;

    for (size_t y = 0; y < copy_height; y++)
    {
        size_t src_y = buffer->drawBeginLineIndex + y;
        if (src_y >= buffer->size.height) src_y %= buffer->size.height; // wrap safely

        void* dst = (void*)((size_t)hardware_buffer->buffer + y * dst_pitch);
        void* src = (void*)((size_t)buffer->buffer + src_y * src_pitch);
        memcpy(dst, src, copy_width * bytes_per_pixel);
    }
}

static void gfx_draw_bpp24()
{
    gfx_buffer *buffer = (gfx_buffer *)List_GetAt(gfx_buffers, 0);

    if (!buffer)
        return; // No buffer to draw

    if (buffer->size.width == 0 || buffer->size.height == 0)
    {
        ERROR("Buffer size is zero, cannot draw");
        return;
    }

    size_t copy_width = (buffer->size.width < hardware_buffer->size.width) ? buffer->size.width : hardware_buffer->size.width;
    size_t copy_height = (buffer->size.height < hardware_buffer->size.height) ? buffer->size.height : hardware_buffer->size.height;

    for (size_t y = 0; y < copy_height; y++)
    {
        size_t src_y = buffer->drawBeginLineIndex + y;
        if (src_y >= buffer->size.height) src_y %= buffer->size.height; // wrap safely

        for (size_t x = 0; x < copy_width; x++)
        {
            gfx_color pixel = ((gfx_color *)buffer->buffer)[src_y * buffer->size.width + x];

            size_t fb_index = (y * hardware_buffer->size.width + x) * 3; // 3 bytes per pixel for 24bpp
            ((uint8_t *)hardware_buffer->buffer)[fb_index + 0] = pixel.b; // Blue
            ((uint8_t *)hardware_buffer->buffer)[fb_index + 1] = pixel.g; // Green
            ((uint8_t *)hardware_buffer->buffer)[fb_index + 2] = pixel.r; // Red
        }
    }
}

void gfx_draw_task()
{

    if (gfx_buffers_busy)
        return;

    if (!gfx_buffers)
    {
        LOG("Graphics buffers list is not initialized");
        return;
    }

    if (List_IsEmpty(gfx_buffers))
    {
        LOG("No graphics buffers available for drawing");
    }
    else
    {
        // If top buffer requests suppression, skip this draw cycle entirely
        gfx_buffer *buffer = (gfx_buffer *)List_GetAt(gfx_buffers, 0);
        if (buffer && buffer->suppress_draw) {
            return;
        }
        if (main_screen.mode->bpp == 32)
        {
            gfx_draw_bpp32();
        }
        else if (main_screen.mode->bpp == 24)
        {
            gfx_draw_bpp24();
        }
        else
        {
            ERROR("Unsupported framebuffer bpp: %u", main_screen.mode->bpp);
        }
    }

    // Draw mouse cursor

    __mouse_draw();
}

bool gfx_resize_buffer(gfx_buffer* buffer, size_t newWidth, size_t newHeight)
{
    if (!buffer) return false;
    if (newWidth == 0 || newHeight == 0) return false;

    void* newBuffer = malloc(newWidth * newHeight * (buffer->bpp / 8));
    if (!newBuffer) return false;

    // Copy old content to new buffer
    size_t copyWidth = (newWidth < buffer->size.width) ? newWidth : buffer->size.width;
    size_t copyHeight = (newHeight < buffer->size.height) ? newHeight : buffer->size.height;

    for (size_t y = 0; y < copyHeight; y++) {
        memcpy((uint8_t*)newBuffer + y * newWidth * (buffer->bpp / 8),
               (uint8_t*)buffer->buffer + y * buffer->size.width * (buffer->bpp / 8),
               copyWidth * (buffer->bpp / 8));
    }

    free(buffer->buffer);
    buffer->buffer = newBuffer;
    buffer->size.width = newWidth;
    buffer->size.height = newHeight;
    buffer->isDirty = true;

    return true;
}
