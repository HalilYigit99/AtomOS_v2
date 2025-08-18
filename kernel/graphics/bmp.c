#include <graphics/bmp.h>
#include <memory/memory.h>
#include <util/string.h>
#include <math.h>

// Global error state
static bmp_result last_error = BMP_SUCCESS;

// __builtin_ctz() yerine manuel count trailing zeros fonksiyonu
static int manual_ctz(uint32_t value) {
    if (value == 0) return 32;
    
    int count = 0;
    while ((value & 1) == 0) {
        value >>= 1;
        count++;
    }
    return count;
}

// Helper function to set error and return NULL
static void* bmp_set_error(bmp_result error) {
    last_error = error;
    return NULL;
}

// Helper function to set error and return error code
static bmp_result bmp_return_error(bmp_result error) {
    last_error = error;
    return error;
}

// Calculate row size with 4-byte padding
static uint32_t bmp_calculate_row_size(uint32_t width, uint16_t bits_per_pixel) {
    uint32_t bytes_per_row = (width * bits_per_pixel + 7) / 8; // Round up to nearest byte
    return (bytes_per_row + 3) & ~3; // Round up to multiple of 4
}

// Validate BMP file structure
bmp_result bmp_validate(void* fileData, size_t fileSizeInBytes) {
    if (!fileData) {
        return bmp_return_error(BMP_ERROR_NULL_POINTER);
    }
    
    if (fileSizeInBytes < sizeof(bmp_file_header) + sizeof(bmp_info_header)) {
        return bmp_return_error(BMP_ERROR_FILE_TOO_SMALL);
    }
    
    bmp_file_header* file_header = (bmp_file_header*)fileData;
    
    // Check BMP signature
    if (file_header->signature != BMP_SIGNATURE) {
        return bmp_return_error(BMP_ERROR_INVALID_SIGNATURE);
    }
    
    // Check if file size matches
    if (file_header->file_size > fileSizeInBytes) {
        return bmp_return_error(BMP_ERROR_INVALID_FILE);
    }
    
    bmp_info_header* info_header = (bmp_info_header*)((uint8_t*)fileData + sizeof(bmp_file_header));
    
    // Check info header size (support BITMAPINFOHEADER (40) and BITMAPV4HEADER (108) and BITMAPV5HEADER (124))
    if (info_header->header_size != 40 && 
        info_header->header_size != 108 && 
        info_header->header_size != 124) {
        return bmp_return_error(BMP_ERROR_UNSUPPORTED_FORMAT);
    }
    
    // Check if dimensions are reasonable
    if (info_header->width <= 0 || info_header->height == 0) {
        return bmp_return_error(BMP_ERROR_INVALID_FILE);
    }
    
    // Check bits per pixel (support 8, 24, 32 bit)
    if (info_header->bits_per_pixel != 8 && 
        info_header->bits_per_pixel != 24 && 
        info_header->bits_per_pixel != 32) {
        return bmp_return_error(BMP_ERROR_UNSUPPORTED_FORMAT);
    }
    
    // Check compression (support RGB and BITFIELDS)
    if (info_header->compression != BMP_COMPRESSION_RGB && 
        info_header->compression != BMP_COMPRESSION_BITFIELDS) {
        return bmp_return_error(BMP_ERROR_UNSUPPORTED_FORMAT);
    }
    
    // Check color planes
    if (info_header->planes != 1) {
        return bmp_return_error(BMP_ERROR_INVALID_FILE);
    }
    
    return BMP_SUCCESS;
}

// Get BMP image information
bmp_result bmp_get_info(void* fileData, size_t fileSizeInBytes, 
                        uint32_t* width, uint32_t* height, uint16_t* bits_per_pixel) {
    bmp_result result = bmp_validate(fileData, fileSizeInBytes);
    if (result != BMP_SUCCESS) {
        return result;
    }
    
    bmp_info_header* info_header = (bmp_info_header*)((uint8_t*)fileData + sizeof(bmp_file_header));
    
    if (width) *width = (uint32_t)abs(info_header->width);
    if (height) *height = (uint32_t)abs(info_header->height);
    if (bits_per_pixel) *bits_per_pixel = info_header->bits_per_pixel;
    
    return BMP_SUCCESS;
}

// Convert 24-bit BGR pixel to 32-bit ARGB
static gfx_color bmp_bgr24_to_argb32(uint8_t* bgr_pixel) {
    gfx_color color;
    color.b = bgr_pixel[0];
    color.g = bgr_pixel[1];
    color.r = bgr_pixel[2];
    color.a = 0xFF; // Fully opaque
    return color;
}

// Convert 32-bit BGRA pixel to 32-bit ARGB using bit masks - DÜZELTİLMİŞ VERSİYON
static gfx_color bmp_bgra32_to_argb32_with_masks(uint8_t* pixel_data, 
                                                  uint32_t red_mask, uint32_t green_mask, 
                                                  uint32_t blue_mask, uint32_t alpha_mask) {
    gfx_color color = {0};
    
    // Read 32-bit pixel value
    uint32_t pixel_value = *((uint32_t*)pixel_data);
    
    // If no masks provided, assume standard BGRA format
    if (red_mask == 0 && green_mask == 0 && blue_mask == 0 && alpha_mask == 0) {
        color.b = pixel_data[0];
        color.g = pixel_data[1];
        color.r = pixel_data[2];
        color.a = pixel_data[3];
        return color;
    }
    
    // Extract color components using bit masks - MANUAL CTZ
    if (red_mask) {
        uint32_t red_value = pixel_value & red_mask;
        int red_shift = manual_ctz(red_mask);  // __builtin_ctz yerine manual_ctz
        color.r = (red_value >> red_shift) & 0xFF;
    }
    
    if (green_mask) {
        uint32_t green_value = pixel_value & green_mask;
        int green_shift = manual_ctz(green_mask);  // __builtin_ctz yerine manual_ctz
        color.g = (green_value >> green_shift) & 0xFF;
    }
    
    if (blue_mask) {
        uint32_t blue_value = pixel_value & blue_mask;
        int blue_shift = manual_ctz(blue_mask);  // __builtin_ctz yerine manual_ctz
        color.b = (blue_value >> blue_shift) & 0xFF;
    }
    
    if (alpha_mask) {
        uint32_t alpha_value = pixel_value & alpha_mask;
        int alpha_shift = manual_ctz(alpha_mask);  // __builtin_ctz yerine manual_ctz
        color.a = (alpha_value >> alpha_shift) & 0xFF;
    } else {
        color.a = 0xFF; // Fully opaque if no alpha mask
    }
    
    return color;
}

// Convert palette index to ARGB color
static gfx_color bmp_palette_to_argb32(uint8_t index, bmp_color_entry* palette, uint32_t palette_size) {
    gfx_color color = {0};
    
    if (index < palette_size && palette) {
        color.b = palette[index].blue;
        color.g = palette[index].green;
        color.r = palette[index].red;
        color.a = 0xFF; // Fully opaque
    }
    
    return color;
}

// Main BMP loading function
gfx_bitmap* bmp_load_from_memory(void* fileData, size_t fileSizeInBytes) {
    last_error = BMP_SUCCESS;
    
    // Validate input
    if (!fileData) {
        return (gfx_bitmap*)bmp_set_error(BMP_ERROR_NULL_POINTER);
    }
    
    // Validate BMP structure
    bmp_result validation_result = bmp_validate(fileData, fileSizeInBytes);
    if (validation_result != BMP_SUCCESS) {
        return (gfx_bitmap*)bmp_set_error(validation_result);
    }
    
    // Parse headers
    bmp_file_header* file_header = (bmp_file_header*)fileData;
    bmp_info_header* info_header = (bmp_info_header*)((uint8_t*)fileData + sizeof(bmp_file_header));
    
    uint32_t width = (uint32_t)abs(info_header->width);
    uint32_t height = (uint32_t)abs(info_header->height);
    bool is_top_down = info_header->height < 0; // Negative height means top-down
    
    // Get color masks for BITFIELDS compression
    uint32_t red_mask = 0, green_mask = 0, blue_mask = 0, alpha_mask = 0;
    
    if (info_header->compression == BMP_COMPRESSION_BITFIELDS && info_header->bits_per_pixel == 32) {
        // Color masks are located after the info header
        uint8_t* mask_data = (uint8_t*)fileData + sizeof(bmp_file_header) + 40; // After basic BITMAPINFOHEADER
        
        // Read masks in little-endian format
        red_mask = *((uint32_t*)(mask_data + 0));
        green_mask = *((uint32_t*)(mask_data + 4));
        blue_mask = *((uint32_t*)(mask_data + 8));
        
        // Alpha mask might be present in V4/V5 headers
        if (info_header->header_size >= 108) {
            alpha_mask = *((uint32_t*)(mask_data + 12));
        }
    }
    
    // Allocate gfx_bitmap structure
    gfx_bitmap* bitmap = (gfx_bitmap*)malloc(sizeof(gfx_bitmap));
    if (!bitmap) {
        return (gfx_bitmap*)bmp_set_error(BMP_ERROR_MEMORY_ALLOCATION);
    }
    
    // Set bitmap properties
    bitmap->size.width = width;
    bitmap->size.height = height;
    
    // Allocate pixel buffer (ARGB32 format)
    bitmap->pixels = (uint8_t*)malloc(width * height * sizeof(uint32_t));
    if (!bitmap->pixels) {
        free(bitmap);
        return (gfx_bitmap*)bmp_set_error(BMP_ERROR_MEMORY_ALLOCATION);
    }
    
    // Calculate source row size with padding
    uint32_t src_row_size = bmp_calculate_row_size(width, info_header->bits_per_pixel);
    
    // Get pixel data start
    uint8_t* src_data = (uint8_t*)fileData + file_header->data_offset;
    
    // Check if we have enough data
    if (file_header->data_offset + (src_row_size * height) > fileSizeInBytes) {
        free(bitmap->pixels);
        free(bitmap);
        return (gfx_bitmap*)bmp_set_error(BMP_ERROR_CORRUPTED_DATA);
    }
    
    // Handle palette for 8-bit images
    bmp_color_entry* palette = NULL;
    uint32_t palette_size = 0;
    
    if (info_header->bits_per_pixel == 8) {
        palette_size = info_header->colors_used;
        if (palette_size == 0) {
            palette_size = 256; // Default for 8-bit
        }
        
        // Palette is located after info header
        palette = (bmp_color_entry*)((uint8_t*)fileData + sizeof(bmp_file_header) + info_header->header_size);
        
        // Check if palette fits in file
        if (sizeof(bmp_file_header) + info_header->header_size + (palette_size * sizeof(bmp_color_entry)) > fileSizeInBytes) {
            free(bitmap->pixels);
            free(bitmap);
            return (gfx_bitmap*)bmp_set_error(BMP_ERROR_CORRUPTED_DATA);
        }
    }
    
    // Convert pixels
    gfx_color* dst_pixels = (gfx_color*)bitmap->pixels;
    
    for (uint32_t y = 0; y < height; y++) {
        // BMP rows are stored bottom-up by default (unless negative height)
        uint32_t src_y = is_top_down ? y : (height - 1 - y);
        uint32_t dst_y = y;
        
        uint8_t* src_row = src_data + (src_y * src_row_size);
        gfx_color* dst_row = dst_pixels + (dst_y * width);
        
        for (uint32_t x = 0; x < width; x++) {
            gfx_color pixel_color = {0};
            
            switch (info_header->bits_per_pixel) {
                case 8: {
                    uint8_t index = src_row[x];
                    pixel_color = bmp_palette_to_argb32(index, palette, palette_size);
                    break;
                }
                case 24: {
                    uint8_t* bgr_pixel = src_row + (x * 3);
                    pixel_color = bmp_bgr24_to_argb32(bgr_pixel);
                    break;
                }
                case 32: {
                    uint8_t* bgra_pixel = src_row + (x * 4);
                    if (info_header->compression == BMP_COMPRESSION_BITFIELDS) {
                        pixel_color = bmp_bgra32_to_argb32_with_masks(bgra_pixel, red_mask, green_mask, blue_mask, alpha_mask);
                    } else {
                        // Standard BGRA format
                        pixel_color.b = bgra_pixel[0];
                        pixel_color.g = bgra_pixel[1];
                        pixel_color.r = bgra_pixel[2];
                        pixel_color.a = bgra_pixel[3];
                    }
                    break;
                }
            }
            
            dst_row[x] = pixel_color;
        }
    }
    
    return bitmap;
}

// Free gfx_bitmap
void bmp_free(gfx_bitmap* bitmap) {
    if (!bitmap) return;
    
    if (bitmap->pixels) {
        free(bitmap->pixels);
    }
    
    free(bitmap);
}

// Get last error
bmp_result bmp_get_last_error(void) {
    return last_error;
}

// Get error string
const char* bmp_get_error_string(bmp_result error) {
    switch (error) {
        case BMP_SUCCESS:
            return "Success";
        case BMP_ERROR_NULL_POINTER:
            return "Null pointer provided";
        case BMP_ERROR_INVALID_FILE:
            return "Invalid BMP file";
        case BMP_ERROR_INVALID_SIGNATURE:
            return "Invalid BMP signature";
        case BMP_ERROR_UNSUPPORTED_FORMAT:
            return "Unsupported BMP format";
        case BMP_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation failed";
        case BMP_ERROR_CORRUPTED_DATA:
            return "Corrupted BMP data";
        case BMP_ERROR_FILE_TOO_SMALL:
            return "File too small to be valid BMP";
        default:
            return "Unknown error";
    }
}