#ifndef IMAGE_H
#define IMAGE_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} pixel;

typedef struct {
    int w;
    int h;
    pixel *data;
} image;

typedef struct {
    char *fbp;
    int fd;
    int w;
    int h;
    int line_length;
    int bpp;
    int screensize;
} buffer;

// Opens and maps a framebuffer device
bool create_buffer(buffer *fb, const char *device);

// Unmaps and closes a framebuffer
void destroy_buffer(buffer *fb);

// Loads an image supported by stb_image into an image struct
bool load_image(image *img, const char *filename);

// Resizes an image to fit within max_width x max_height without changing aspect ratio
bool resize_image(image *img, int max_width, int max_height);

// Rotates an image clockwise by an arbitrary angle in degrees
bool rotate_image(image *img, double degrees);

// Draws a given image onto a given framebuffer
void draw_image(buffer *buffer, const image *image, int x_offset, int y_offset);

#endif