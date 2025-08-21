#include <graphics/gfx.h>
#include <list.h>
#include <memory/memory.h>
#include <boot/multiboot2.h>
#include <stream/OutputStream.h>
#include <math.h>
#include <mouse/mouse.h>
#include <debug/debug.h>

static volatile size_t screen_width;
static volatile size_t screen_height;

List *gfx_buffers;
static bool gfx_buffers_busy;

gfx_buffer *hardware_buffer;
gfx_buffer *screen_buffer;

void gfx_draw_task();

void gfx_screen_register_buffer(gfx_buffer *buffer)
{
    gfx_buffers_busy = true;
    List_Add(gfx_buffers, buffer);
    gfx_buffers_busy = false;
}

void gfx_screen_unregister_buffer(gfx_buffer *buffer)
{
    gfx_buffers_busy = true;
    List_Remove(gfx_buffers, buffer);
    gfx_buffers_busy = false;
}

void gfx_init()
{

    screen_height = mb2_framebuffer->framebuffer_height;
    screen_width = mb2_framebuffer->framebuffer_width;

    // Initialize hardware buffer
    hardware_buffer = (gfx_buffer *)malloc(sizeof(gfx_buffer));

    if (!hardware_buffer)
    {
        LOG("Failed to allocate hardware buffer");
        asm volatile("cli; hlt"); // Halt the system
    }

    hardware_buffer->size.width = screen_width;
    hardware_buffer->size.height = screen_height;

    size_t fb_addr = (size_t)mb2_framebuffer->framebuffer_addr;

    hardware_buffer->buffer = (void *)fb_addr;
    hardware_buffer->bpp = mb2_framebuffer->framebuffer_bpp;
    hardware_buffer->drawBeginLineIndex = 0;
    hardware_buffer->isDirty = true; // Always true
    hardware_buffer->position = (gfx_point){0, 0};

    gfx_buffers = List_Create();

    if (!gfx_buffers)
    {
        LOG("Failed to create graphics buffer list");
        asm volatile("cli; hlt"); // Halt the system
    }

    // Initialize screen buffer
    screen_buffer = gfx_create_buffer(screen_width, screen_height);
    gfx_clear_buffer(screen_buffer, (gfx_color){.argb = 0xFF000000}); // Clear screen to black
    gfx_screen_register_buffer(screen_buffer);
}

gfx_buffer *gfx_create_buffer(size_t width, size_t height)
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
        LOG("Failed to allocate buffer memory");
        asm volatile("cli; hlt"); // Halt the system
    }

    buffer->bpp = 32; // Assuming 32 bits per pixel
    buffer->drawBeginLineIndex = 0;

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

    volatile gfx_color *pixel = (volatile gfx_color *)((size_t)buffer->buffer + (y * buffer->size.width + x) * sizeof(uint32_t));
    pixel->argb = color.argb;
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

static int cursor_last_X;
static int cursor_last_Y;

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
        gfx_buffer *buffer = (gfx_buffer *)List_GetAt(gfx_buffers, List_Size(gfx_buffers) - 1);
        if (!buffer)
            return; // No buffer to draw

        if (buffer->size.width == 0 || buffer->size.height == 0)
        {
            ERROR("Buffer size is zero, cannot draw");
            return;
        }

        if (buffer->size.width == hardware_buffer->size.width &&
            buffer->size.height == hardware_buffer->size.height &&
            buffer->bpp == hardware_buffer->bpp)
        {
            // If the buffer size and bpp match, we can copy directly
            memcpy((void *)hardware_buffer->buffer, (void *)buffer->buffer, buffer->size.width * buffer->size.height * sizeof(uint32_t));
        }
        else
        {
            size_t width = buffer->size.width < screen_width ? buffer->size.width : screen_width;
            size_t height = buffer->size.height < screen_height ? buffer->size.height : screen_height;

            for (size_t y = 0; y < height; y++)
            {
                memcpy(
                    (void *)((size_t)hardware_buffer->buffer + (y * hardware_buffer->size.width * sizeof(uint32_t))),
                    (void *)((size_t)buffer->buffer + (y * buffer->size.width * sizeof(uint32_t))),
                    width * sizeof(uint32_t));
            }
        }
    }

    // Draw mouse cursor

    if (cursor_last_X != cursor_X || cursor_last_Y != cursor_Y) __mouse_draw();

    cursor_last_X = cursor_X;
    cursor_last_Y = cursor_Y;
}
