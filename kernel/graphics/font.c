#include <graphics/types.h>

extern unsigned char __font8x8[][8];
extern unsigned char __font8x16[][16];

// DÜZELTME: Font glyphs'i doğru tip olarak cast et
gfx_font gfx_font8x8 = {
    .name = "8x8 Bitmap Font",
    .size = {8, 8},
    .glyphs = (uint32_t*)__font8x8,  // Bu casting hala geçerli ama VDrawChar'da unsigned char* olarak kullanılacak
    .type = GFX_FONT_BITMAP
};

gfx_font gfx_font8x16 = {
    .name = "8x16 Bitmap Font",
    .size = {8, 16},
    .glyphs = (uint32_t*)__font8x16,  // Bu casting hala geçerli ama VDrawChar'da unsigned char* olarak kullanılacak
    .type = GFX_FONT_BITMAP
};

gfx_font* default_font = &gfx_font8x16; // Varsayılan font olarak 8x8 bitmap font kullanılıyor
