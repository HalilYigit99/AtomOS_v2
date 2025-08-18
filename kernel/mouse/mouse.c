#include <mouse/mouse.h>
#include <stream/OutputStream.h>

typedef union {
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    } __attribute__((packed));
    uint32_t argb;
} gfx_color;

int cursor_X = 300; // Initialize cursor X position
int cursor_Y = 250; // Initialize cursor Y position

// Yukarıdaki gibi tek tek yazmak yerine, C'nin başlatıcı listesi özelliğini kullanarak
// görsel bir şekilde imleci tanımlayabiliriz.
// T = Şeffaf, B = Siyah, W = Beyaz
#define T (gfx_color){.argb = 0}
#define B (gfx_color){.a = 0xFF, .r = 0x00, .g = 0x00, .b = 0x00}
#define W (gfx_color){.argb = 0xFFFFFFFF}

const gfx_color full_cursor_bitmap[13*18] = {
    B,B,T,T,T,T,T,T,T,T,T,T,T,
    B,W,B,T,T,T,T,T,T,T,T,T,T,
    B,W,W,B,T,T,T,T,T,T,T,T,T,
    B,W,W,W,B,T,T,T,T,T,T,T,T,
    B,W,W,W,W,B,T,T,T,T,T,T,T,
    B,W,W,W,W,W,B,T,T,T,T,T,T,
    B,W,W,W,W,W,W,B,T,T,T,T,T,
    B,W,W,W,W,W,W,W,B,T,T,T,T,
    B,W,W,W,W,W,W,W,W,B,T,T,T,
    B,W,W,W,W,W,W,W,W,W,B,T,T,
    B,W,W,W,W,W,W,W,W,W,W,B,T,
    B,W,W,W,W,W,W,B,B,B,B,B,T,
    B,W,W,W,B,W,W,B,T,T,T,T,T,
    B,W,W,B,T,B,W,W,B,T,T,T,T,
    B,W,B,T,T,B,W,W,B,T,T,T,T,
    B,B,T,T,T,T,B,W,W,B,T,T,T,
    T,T,T,T,T,T,B,W,W,B,T,T,T,
    T,T,T,T,T,T,T,B,B,T,T,T,T,
};

void __mouse_draw() {

    // Draw the cursor bitmap at the current position
    // gfx_draw_bitmap(hardware_buffer, cursor_X, cursor_Y, (void*)full_cursor_bitmap, 13, 18);

}
