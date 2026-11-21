#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


bool load_image(image *image, const char *filename)
{
	if (image == NULL || filename == NULL)
		return false;

	image->w = 0;
	image->h = 0;
	image->data = NULL;

	int width;
	int height;
	unsigned char *stbi_data =
		stbi_load(filename, &width, &height, NULL, STBI_rgb);
	if (stbi_data == NULL) {
		fprintf(stderr, "Failed to load image '%s': %s\n", filename,
				stbi_failure_reason());
		return false;
	}

	if (width <= 0 || height <= 0 ||
		(size_t)width > SIZE_MAX / (size_t)height ||
		(size_t)width * (size_t)height > SIZE_MAX / sizeof(pixel)) {
		fprintf(stderr, "Invalid image dimensions for '%s'\n", filename);
		stbi_image_free(stbi_data);
		return false;
	}

	size_t pixel_count = (size_t)width * (size_t)height;
	pixel *data = malloc(pixel_count * sizeof(pixel));
	if (data == NULL) {
		fprintf(stderr, "Failed to allocate image data for '%s'\n", filename);
		stbi_image_free(stbi_data);
		return false;
	}

	for (size_t i = 0; i < pixel_count; i++) {
		data[i].r = stbi_data[i * STBI_rgb];
		data[i].g = stbi_data[i * STBI_rgb + 1];
		data[i].b = stbi_data[i * STBI_rgb + 2];
	}

	stbi_image_free(stbi_data);
	image->w = width;
	image->h = height;
	image->data = data;
	return true;
}

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

image *rotate_image(const image *img, float angle)
{
    if (img == NULL || img->data == NULL)
        return NULL;

    image *out = malloc(sizeof(*out));
    if (out == NULL)
        return NULL;

    out->w = img->w;
    out->h = img->h;

    size_t count = (size_t)out->w * (size_t)out->h;
    out->data = calloc(count, sizeof(*out->data));
    if (out->data == NULL) {
        free(out);
        return NULL;
    }

    float cx = (out->w - 1) / 2.0f;
    float cy = (out->h - 1) / 2.0f;

    float cosine = cosf(angle);
    float sine = sinf(angle);

    for (int y = 0; y < out->h; ++y) {
        for (int x = 0; x < out->w; ++x) {
            float dx = x - cx;
            float dy = y - cy;

            float source_x = cosine * dx + sine * dy + cx;
            float source_y = -sine * dx + cosine * dy + cy;

            int sx = (int)roundf(source_x);
            int sy = (int)roundf(source_y);

            if (sx >= 0 && sx < img->w &&
                sy >= 0 && sy < img->h) {
                out->data[y * out->w + x] =
                    img->data[sy * img->w + sx];
            }
        }
    }

    return out;
}

void draw_image(buffer *fb, const image *img, int x_offset, int y_offset)
{
	if (fb == NULL || img == NULL || fb->fbp == NULL || img->data == NULL ||
		fb->bpp < 24)
		return;

	int bytes_per_pixel = fb->bpp / 8;

	if (bytes_per_pixel == 3 || bytes_per_pixel == 4) {
		unsigned char *row =
			malloc((size_t)img->w * (size_t)bytes_per_pixel);
		if (row == NULL)
			return;

		for (int sy = 0; sy < img->h; sy++) {
			int dy = sy + y_offset;
			if (dy < 0 || dy >= fb->h)
				continue;

			int sx0 = x_offset < 0 ? -x_offset : 0;
			int sx1 = img->w;
			if (x_offset + sx1 > fb->w)
				sx1 = fb->w - x_offset;
			if (sx0 >= sx1)
				continue;

			for (int sx = sx0; sx < sx1; sx++) {
				pixel p = img->data[(size_t)sy * img->w + sx];
				size_t offset = (size_t)(sx - sx0) * bytes_per_pixel;
				row[offset] = p.b;
				row[offset + 1] = p.g;
				row[offset + 2] = p.r;
				if (bytes_per_pixel == 4)
					row[offset + 3] = 255;
			}

			size_t destination =
				(size_t)dy * fb->line_length +
				(size_t)(x_offset + sx0) * bytes_per_pixel;
			memcpy(fb->fbp + destination, row,
				   (size_t)(sx1 - sx0) * bytes_per_pixel);
		}

		free(row);
		return;
	}

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