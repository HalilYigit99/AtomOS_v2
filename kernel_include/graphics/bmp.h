#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <graphics/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// BMP file signature (magic number)
#define BMP_SIGNATURE 0x4D42  // "BM" in little endian

// BMP compression types
#define BMP_COMPRESSION_RGB       0  // No compression
#define BMP_COMPRESSION_RLE8      1  // 8-bit RLE
#define BMP_COMPRESSION_RLE4      2  // 4-bit RLE
#define BMP_COMPRESSION_BITFIELDS 3  // Bitfield compression

// Error codes for BMP operations
typedef enum {
    BMP_SUCCESS = 0,
    BMP_ERROR_NULL_POINTER,
    BMP_ERROR_INVALID_FILE,
    BMP_ERROR_INVALID_SIGNATURE,
    BMP_ERROR_UNSUPPORTED_FORMAT,
    BMP_ERROR_MEMORY_ALLOCATION,
    BMP_ERROR_CORRUPTED_DATA,
    BMP_ERROR_FILE_TOO_SMALL
} bmp_result;

// BMP File Header (14 bytes)
typedef struct __attribute__((packed)) {
    uint16_t signature;      // File signature "BM" (0x4D42)
    uint32_t file_size;      // Size of BMP file in bytes
    uint16_t reserved1;      // Reserved field (must be 0)
    uint16_t reserved2;      // Reserved field (must be 0)
    uint32_t data_offset;    // Offset to start of pixel data
} bmp_file_header;

// BMP Info Header (40 bytes - BITMAPINFOHEADER)
typedef struct __attribute__((packed)) {
    uint32_t header_size;       // Size of info header (40 bytes)
    int32_t  width;            // Width of bitmap in pixels
    int32_t  height;           // Height of bitmap in pixels
    uint16_t planes;           // Number of color planes (must be 1)
    uint16_t bits_per_pixel;   // Bits per pixel (1, 4, 8, 16, 24, 32)
    uint32_t compression;      // Compression method
    uint32_t image_size;       // Size of raw bitmap data
    int32_t  x_pixels_per_m;   // Horizontal resolution (pixels/meter)
    int32_t  y_pixels_per_m;   // Vertical resolution (pixels/meter)
    uint32_t colors_used;      // Number of colors in palette
    uint32_t important_colors; // Number of important colors
} bmp_info_header;

// Color palette entry (for 8-bit and lower BMPs)
typedef struct __attribute__((packed)) {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved; // Usually 0
} bmp_color_entry;

// Complete BMP structure for internal use
typedef struct {
    bmp_file_header file_header;
    bmp_info_header info_header;
    bmp_color_entry* palette;     // Color palette (NULL if not used)
    uint32_t palette_size;        // Number of palette entries
    uint8_t* pixel_data;          // Raw pixel data
    uint32_t row_size;            // Size of each row in bytes (including padding)
} bmp_image;

// Main BMP loading functions
/**
 * Load BMP image from memory buffer and convert to gfx_bitmap
 * @param fileData Pointer to BMP file data in memory
 * @param fileSizeInBytes Size of the file data in bytes
 * @return Pointer to allocated gfx_bitmap or NULL on failure
 */
gfx_bitmap* bmp_load_from_memory(void* fileData, size_t fileSizeInBytes);

/**
 * Free gfx_bitmap created by bmp_load_from_memory
 * @param bitmap Pointer to gfx_bitmap to free
 */
void bmp_free(gfx_bitmap* bitmap);

/**
 * Get detailed error information from last BMP operation
 * @return Last error code
 */
bmp_result bmp_get_last_error(void);

/**
 * Get human-readable error message
 * @param error Error code
 * @return Pointer to error message string
 */
const char* bmp_get_error_string(bmp_result error);

// Helper functions for validation and debugging
/**
 * Validate BMP file structure without loading
 * @param fileData Pointer to BMP file data
 * @param fileSizeInBytes Size of the file data
 * @return BMP_SUCCESS if valid, error code otherwise
 */
bmp_result bmp_validate(void* fileData, size_t fileSizeInBytes);

/**
 * Get BMP image information without loading pixel data
 * @param fileData Pointer to BMP file data
 * @param fileSizeInBytes Size of the file data
 * @param width Output parameter for image width
 * @param height Output parameter for image height
 * @param bits_per_pixel Output parameter for bits per pixel
 * @return BMP_SUCCESS if successful, error code otherwise
 */
bmp_result bmp_get_info(void* fileData, size_t fileSizeInBytes, 
                        uint32_t* width, uint32_t* height, uint16_t* bits_per_pixel);

#ifdef __cplusplus
}
#endif