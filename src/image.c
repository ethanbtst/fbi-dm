#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

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

bool resize_image(image *img, int max_width, int max_height)
{
    if (img == NULL || img->data == NULL || img->w <= 0 || img->h <= 0 ||
        max_width <= 0 || max_height <= 0)
        return false;

    if (img->w <= max_width && img->h <= max_height)
        return true;

    int new_width;
    int new_height;
    if ((long long)max_width * img->h < (long long)max_height * img->w) {
        new_width = max_width;
        new_height =
            (int)(((long long)img->h * max_width + img->w / 2) / img->w);
    } else {
        new_height = max_height;
        new_width =
            (int)(((long long)img->w * max_height + img->h / 2) / img->h);
    }

    if (new_width < 1)
        new_width = 1;
    if (new_height < 1)
        new_height = 1;
    if ((size_t)new_width > SIZE_MAX / (size_t)new_height ||
        (size_t)new_width * (size_t)new_height > SIZE_MAX / sizeof(pixel))
        return false;

    size_t pixel_count = (size_t)new_width * (size_t)new_height;
    pixel *new_data = malloc(pixel_count * sizeof(pixel));
    if (new_data == NULL) {
        fprintf(stderr, "Failed to allocate resized image data\n");
        return false;
    }

    unsigned char *resized = stbir_resize_uint8_srgb(
        (const unsigned char *)img->data, img->w, img->h,
        (stbir_int64)img->w * STBI_rgb, (unsigned char *)new_data, new_width,
        new_height, (stbir_int64)new_width * STBI_rgb, STBIR_RGB);
    if (resized == NULL) {
        fprintf(stderr, "Failed to resize image\n");
        free(new_data);
        return false;
    }

    free(img->data);
    img->w = new_width;
    img->h = new_height;
    img->data = new_data;
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