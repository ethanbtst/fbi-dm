// Helpers to for image processing (PPM only right now)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image_resize.h"
#include "stb_image.h"


bool create_buffer(buffer *fb, const char *device)
{
	if (fb == NULL || device == NULL)
		return false;

	*fb = (buffer){ .fd = -1 };
	fb->fd = open(device, O_RDWR);
	if (fb->fd == -1) {
		perror("Failed to open framebuffer device");
		return false;
	}

	struct fb_var_screeninfo vinfo;
	if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
		perror("Failed to get variable screen info");
		close(fb->fd);
		fb->fd = -1;
		return false;
	}

	struct fb_fix_screeninfo finfo;
	if (ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
		perror("Failed to get fixed screen info");
		close(fb->fd);
		fb->fd = -1;
		return false;
	}

	fb->w = (int)vinfo.xres;
	fb->h = (int)vinfo.yres;
	fb->bpp = (int)vinfo.bits_per_pixel;
	fb->line_length = (int)finfo.line_length;
	fb->screensize = fb->line_length * fb->h;
	fb->fbp = mmap(NULL, fb->screensize, PROT_READ | PROT_WRITE, MAP_SHARED,
				   fb->fd, 0);

	if (fb->fbp == MAP_FAILED) {
		perror("Failed to mmap framebuffer device");
		fb->fbp = NULL;
		close(fb->fd);
		fb->fd = -1;
		return false;
	}

	return true;
}

void destroy_buffer(buffer *fb)
{
	if (fb == NULL)
		return;
	if (fb->fbp != NULL)
		munmap(fb->fbp, fb->screensize);
	if (fb->fd != -1)
		close(fb->fd);

	*fb = (buffer){ .fd = -1 };
	fb->fd = -1;
}

bool load_ppm(image *image, const char *filename)
{
	if (image == NULL || filename == NULL)
		return false;

	FILE *fd = fopen(filename, "rb");

	if (!fd) {
		perror("Failed to open PPM");
		return false;
	}

	char type[5];
	if (fscanf(fd, "%4s", type) != 1 ||
		fscanf(fd, "%d %d %*d", &image->w, &image->h) != 2 || image->w <= 0 ||
		image->h <= 0 || (size_t)image->w > SIZE_MAX / (size_t)image->h ||
		(size_t)image->w * (size_t)image->h > SIZE_MAX / sizeof(pixel)) {
		fclose(fd);
		return false;
	}
	pixel *data = malloc(image->w * image->h * sizeof(pixel));
	if (data == NULL) {
		fclose(fd);
		perror("Failed to allocate PPM data");
		return false;
	}

	if (strcmp(type, "P6") == 0) {
		fgetc(fd);
		if (fread(data, sizeof(pixel), image->w * image->h, fd) !=
			(size_t)image->w * (size_t)image->h) {
			fclose(fd);
			free(data);
			perror("Bad PPM File: Pixels do not fit dimensions");
			return false;
		}
	} else if (strcmp(type, "P3") == 0) {
		for (int y = 0; y < image->h; y++) {
			for (int x = 0; x < image->w; x++) {
				int r, g, b;

				if (fscanf(fd, "%d %d %d", &r, &g, &b) != 3) {
					fclose(fd);
					free(data);
					perror("Bad PPM File: Pixels do not fit dimensions");
					return false;
				}

				data[y * image->w + x].r = (uint8_t)r;
				data[y * image->w + x].g = (uint8_t)g;
				data[y * image->w + x].b = (uint8_t)b;
			}
		}
	} else {
		perror("Bad PPM file: Invalid PPM type");
		fclose(fd);
		free(data);
		return false;
	}

	image->data = data;
	fclose(fd);
	return true;
}




void draw_image(buffer *fb, const image *img, int x_offset, int y_offset)
{
	if (fb == NULL || img == NULL || fb->fbp == NULL || img->data == NULL ||
		fb->bpp < 24)
		return;

	int bytes_per_pixel = fb->bpp / 8;
	for (int sy = 0; sy < img->h; sy++) {
		int dy = sy + y_offset;
		if (dy < 0 || dy >= fb->h)
			continue;

		for (int sx = 0; sx < img->w; sx++) {
			int dx = sx + x_offset;
			if (dx < 0 || dx >= fb->w)
				continue;

			size_t location =
				(size_t)dy * fb->line_length + (size_t)dx * bytes_per_pixel;

			pixel pixel = img->data[sy * img->w + sx];

			fb->fbp[location] = (char)pixel.b;
			fb->fbp[location + 1] = (char)pixel.g;
			fb->fbp[location + 2] = (char)pixel.r;

			if (bytes_per_pixel == 4)
				fb->fbp[location + 3] = (char)255;
		}
	}
}