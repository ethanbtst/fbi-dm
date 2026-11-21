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

// Loads a PPM image into an image struct with pixels and dimensions
bool load_ppm(image *img, const char *filename);

// Draws a given image onto a given framebuffer
void draw_image(buffer *buffer, const image *image, int x_offset, int y_offset);

#endif