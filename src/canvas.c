#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "canvas.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
// Following
// https://www.gabrielgambetta.com/computer-graphics-from-scratch

#define FPS 60
#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])
#define frac(x) x - floor(x)
buffer canvas;
const float viewport_h = 2.0f;
const float projection_distance = 1.0f;
float viewport_w = 3.2f;

bool load_texture(texture *tex, const char *filename)
{
	if (tex == NULL || filename == NULL)
		return false;

	*tex = (texture){ 0 };
	int width;
	int height;
	unsigned char *pixels =
		stbi_load(filename, &width, &height, NULL, STBI_rgb);
	if (pixels == NULL) {
		fprintf(stderr, "Failed to load texture '%s': %s\n", filename,
				stbi_failure_reason());
		return false;
	}

	if (width <= 0 || height <= 0 ||
		(size_t)width > SIZE_MAX / (size_t)height / (3 * sizeof(*pixels))) {
		fprintf(stderr, "Invalid texture dimensions for '%s'\n", filename);
		stbi_image_free(pixels);
		return false;
	}

	tex->width = width;
	tex->height = height;
	tex->pixels = pixels;
	return true;
}

void destroy_texture(texture *tex)
{
	if (tex == NULL)
		return;
	stbi_image_free(tex->pixels);
	*tex = (texture){ 0 };
}

bool resize_texture(texture *tex, int max_width, int max_height)
{
	if (tex == NULL || tex->pixels == NULL || tex->width <= 0 ||
		tex->height <= 0 || max_width <= 0 || max_height <= 0)
		return false;

	if (tex->width <= max_width && tex->height <= max_height)
		return true;

	int new_width;
	int new_height;
	if ((long long)max_width * tex->height <
		(long long)max_height * tex->width) {
		new_width = max_width;
		new_height =
			(int)(((long long)tex->height * max_width + tex->width / 2) /
				  tex->width);
	} else {
		new_height = max_height;
		new_width =
			(int)(((long long)tex->width * max_height + tex->height / 2) /
				  tex->height);
	}

	if (new_width < 1)
		new_width = 1;
	if (new_height < 1)
		new_height = 1;
	if ((size_t)new_width > SIZE_MAX / (size_t)new_height ||
		(size_t)new_width * (size_t)new_height >
			SIZE_MAX / (STBI_rgb * sizeof(*tex->pixels)))
		return false;

	size_t pixel_count = (size_t)new_width * (size_t)new_height;
	unsigned char *resized_pixels =
		malloc(pixel_count * STBI_rgb * sizeof(*resized_pixels));
	if (resized_pixels == NULL)
		return false;

	unsigned char *resized = stbir_resize_uint8_srgb(
		tex->pixels, tex->width, tex->height,
		(stbir_int64)tex->width * STBI_rgb, resized_pixels, new_width,
		new_height, (stbir_int64)new_width * STBI_rgb, STBIR_RGB);
	if (resized == NULL) {
		free(resized_pixels);
		return false;
	}

	stbi_image_free(tex->pixels);
	tex->width = new_width;
	tex->height = new_height;
	tex->pixels = resized_pixels;
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
	if (fb->w <= 0 || fb->h <= 0 ||
		(size_t)fb->w > SIZE_MAX / (size_t)fb->h / sizeof(*fb->depth_buffer)) {
		fprintf(stderr, "Invalid framebuffer dimensions\n");
		close(fb->fd);
		fb->fd = -1;
		return false;
	}
	if (fb->h > 0)
		viewport_w = viewport_h * (float)fb->w / (float)fb->h;
	fb->fbp = mmap(NULL, fb->screensize, PROT_READ | PROT_WRITE, MAP_SHARED,
				   fb->fd, 0);

	if (fb->fbp == MAP_FAILED) {
		perror("Failed to mmap framebuffer device");
		fb->fbp = NULL;
		close(fb->fd);
		fb->fd = -1;
		return false;
	}

	fb->back_buffer = malloc((size_t)fb->screensize);
	if (fb->back_buffer == NULL) {
		munmap(fb->fbp, fb->screensize);
		fb->fbp = NULL;
		close(fb->fd);
		fb->fd = -1;
		return false;
	}
	size_t pixel_count = (size_t)fb->w * (size_t)fb->h;
	fb->depth_buffer = malloc(pixel_count * sizeof(*fb->depth_buffer));
	if (fb->depth_buffer == NULL) {
		perror("Failed to allocate depth buffer");
		free(fb->back_buffer);
		fb->back_buffer = NULL;
		munmap(fb->fbp, fb->screensize);
		fb->fbp = NULL;
		close(fb->fd);
		fb->fd = -1;
		return false;
	}
	for (size_t i = 0; i < pixel_count; ++i)
		fb->depth_buffer[i] = INFINITY;
	memset(fb->back_buffer, 0, (size_t)fb->screensize);
	if (fb != &canvas)
		canvas = *fb;

	return true;
}

void destroy_buffer(buffer *fb)
{
	if (fb == NULL)
		return;
	if (fb->back_buffer != NULL) {
		free(fb->back_buffer);
		fb->back_buffer = NULL;
	}
	free(fb->depth_buffer);
	fb->depth_buffer = NULL;
	if (fb->fbp != NULL)
		munmap(fb->fbp, fb->screensize);
	if (fb->fd != -1)
		close(fb->fd);

	*fb = (buffer){ .fd = -1 };
	fb->fd = -1;
}

void swap_points(point *p0, point *p1)
{
	point tmp = *p1;
	*p1 = *p0;
	*p0 = tmp;
}

void put_pixel(int x, int y, colour c)
{
	int bytes_per_pixel = canvas.bpp / 8;

	int sx = canvas.w / 2 + x;
	int sy = canvas.h / 2 - y;
	if (sx < 0 || sx >= canvas.w || sy < 0 || sy >= canvas.h)
		return;

	size_t location =
		(size_t)sy * canvas.line_length + (size_t)sx * bytes_per_pixel;

	canvas.back_buffer[location] = (char)c.b;
	canvas.back_buffer[location + 1] = (char)c.g;
	canvas.back_buffer[location + 2] = (char)c.r;

	if (bytes_per_pixel == 4)
		canvas.back_buffer[location + 3] = (char)255;
}

static void put_pixel_2d(int x, int y, colour c)
{
	put_pixel(x - canvas.w / 2, canvas.h / 2 - y, c);
}

void draw2drectangle(int posX, int posY, int width, int height)
{
	if (canvas.back_buffer == NULL || canvas.w <= 0 || canvas.h <= 0 ||
		width <= 0 || height <= 0)
		return;

	int64_t left = posX;
	int64_t top = posY;
	int64_t right = left + width;
	int64_t bottom = top + height;
	int64_t x0 = left > 0 ? left : 0;
	int64_t y0 = top > 0 ? top : 0;
	int64_t x1 = right < canvas.w ? right : canvas.w;
	int64_t y1 = bottom < canvas.h ? bottom : canvas.h;
	if (x0 >= x1 || y0 >= y1)
		return;

	const colour outline = { 255, 255, 255 };
	if (top >= y0 && top < y1) {
		for (int64_t x = x0; x < x1; ++x)
			put_pixel_2d((int)x, (int)top, outline);
	}
	if (bottom - 1 != top && bottom - 1 >= y0 && bottom - 1 < y1) {
		for (int64_t x = x0; x < x1; ++x)
			put_pixel_2d((int)x, (int)(bottom - 1), outline);
	}
	if (left >= x0 && left < x1) {
		for (int64_t y = y0; y < y1; ++y)
			put_pixel_2d((int)left, (int)y, outline);
	}
	if (right - 1 != left && right - 1 >= x0 && right - 1 < x1) {
		for (int64_t y = y0; y < y1; ++y)
			put_pixel_2d((int)(right - 1), (int)y, outline);
	}
}

void clear()
{
	if (canvas.back_buffer == NULL)
		return;

	memset(canvas.back_buffer, 0, (size_t)canvas.screensize);
	if (canvas.depth_buffer != NULL) {
		size_t pixel_count = (size_t)canvas.w * (size_t)canvas.h;
		for (size_t i = 0; i < pixel_count; ++i)
			canvas.depth_buffer[i] = INFINITY;
	}
}

void present()
{
	if (canvas.fbp != NULL && canvas.back_buffer != NULL)
		memcpy(canvas.fbp, canvas.back_buffer, (size_t)canvas.screensize);
}

int *interpolate(int i0, int d0, int i1, int d1)
{
	int steps = abs(i1 - i0);
	int *values = malloc(sizeof(*values) * (size_t)(steps + 1));
	if (values == NULL)
		return NULL;

	if (steps == 0) {
		values[0] = d0;
		return values;
	}

	float step = (float)(d1 - d0) / steps;
	for (int i = 0; i <= steps; ++i) {
		values[i] = (int)lroundf(d0 + i * step);
	}

	return values;
}

void draw_line(point p0, point p1, colour c)
{
	if (abs(p1.x - p0.x) > abs(p1.y - p0.y)) {
		if (p0.x > p1.x)
			swap_points(&p0, &p1);

		int *ys = interpolate(p0.x, p0.y, p1.x, p1.y);
		if (ys == NULL)
			return;

		for (int x = p0.x; x <= p1.x; ++x)
			put_pixel(x, ys[x - p0.x], c);

		free(ys);
	} else {
		if (p0.y > p1.y)
			swap_points(&p0, &p1);

		int *xs = interpolate(p0.y, p0.x, p1.y, p1.x);
		if (xs == NULL)
			return;

		for (int y = p0.y; y <= p1.y; ++y)
			put_pixel(xs[y - p0.y], y, c);

		free(xs);
	}
}

void draw_wire_triangle(point p0, point p1, point p2, colour c)
{
	draw_line(p0, p1, c);
	draw_line(p1, p2, c);
	draw_line(p2, p0, c);
}

void draw_filled_triangle(point p0, point p1, point p2, colour c)
{
	// Sort the points so that y0 <= y1 <= y2
	if (p1.y < p0.y)
		swap_points(&p1, &p0);
	if (p2.y < p0.y)
		swap_points(&p2, &p0);
	if (p2.y < p1.y)
		swap_points(&p2, &p1);

	// Compute the x coordinates of the triangle edges
	int *x01 = interpolate(p0.y, p0.x, p1.y, p1.x);
	int *x12 = interpolate(p1.y, p1.x, p2.y, p2.x);
	int *x02 = interpolate(p0.y, p0.x, p2.y, p2.x);

	// Concatenate the short sides
	int x012_len = (p1.y - p0.y + 1) + (p2.y - p1.y + 1) - 1;
	int *x012 = malloc((size_t)x012_len * sizeof(*x012));

	int x01_len = p1.y - p0.y + 1;
	int x12_len = p2.y - p1.y + 1;
	memcpy(x012, x01, (size_t)x01_len * sizeof(*x012));
	memcpy(x012 + x01_len, x12 + 1, (size_t)(x12_len - 1) * sizeof(*x012));

	// Determine which is left and which is right
	int *x_left;
	int *x_right;

	int m = x012_len / 2;
	if (x02[m] < x012[m]) {
		x_left = x02;
		x_right = x012;
	} else {
		x_left = x012;
		x_right = x02;
	}

	// Draw the horizontal segments
	for (int y = p0.y; y <= p2.y; ++y) {
		for (int x = x_left[y - p0.y]; x <= x_right[y - p0.y]; ++x) {
			put_pixel(x, y, c);
		}
	}

	free(x01);
	free(x12);
	free(x02);
	free(x012);
}

point viewport_to_canvas(float x, float y)
{
	return (point){
		.x = (int)lroundf(x * (float)canvas.w / viewport_w),
		.y = (int)lroundf(y * (float)canvas.h / viewport_h),
	};
}

point project_vertex(vertex v)
{
	if (v.z <= 0.0f)
		return (point){ .x = 0, .y = 0 };

	float projected_x = v.x * projection_distance / v.z;
	float projected_y = v.y * projection_distance / v.z;
	return viewport_to_canvas(projected_x, projected_y);
}

static float projected_signed_area(point a, point b, point c)
{
	return (float)(b.x - a.x) * (c.y - a.y) - (float)(b.y - a.y) * (c.x - a.x);
}

static colour sample_texture(const texture *tex, float u, float v)
{
	if (u < 0.0f)
		u = 0.0f;
	else if (u > 1.0f)
		u = 1.0f;
	if (v < 0.0f)
		v = 0.0f;
	else if (v > 1.0f)
		v = 1.0f;

	int x = (int)(u * (float)tex->width);
	int y = (int)(v * (float)tex->height);
	if (x == tex->width)
		--x;
	if (y == tex->height)
		--y;

	const unsigned char *pixel =
		tex->pixels + ((size_t)y * (size_t)tex->width + (size_t)x) * 3;
	return (colour){ .r = pixel[0], .g = pixel[1], .b = pixel[2] };
}

void draw2drectangle_texture(int posX, int posY, int width, int height,
							 const texture *tex)
{
	if (tex == NULL || tex->pixels == NULL || tex->width <= 0 ||
		tex->height <= 0 || canvas.back_buffer == NULL || canvas.w <= 0 ||
		canvas.h <= 0 || width <= 0 || height <= 0)
		return;

	int64_t left = posX;
	int64_t top = posY;
	int64_t right = left + width;
	int64_t bottom = top + height;
	int64_t x0 = left > 0 ? left : 0;
	int64_t y0 = top > 0 ? top : 0;
	int64_t x1 = right < canvas.w ? right : canvas.w;
	int64_t y1 = bottom < canvas.h ? bottom : canvas.h;
	if (x0 >= x1 || y0 >= y1)
		return;

	for (int64_t y = y0; y < y1; ++y) {
		float v = ((float)(y - top) + 0.5f) / (float)height;
		for (int64_t x = x0; x < x1; ++x) {
			float u = ((float)(x - left) + 0.5f) / (float)width;
			put_pixel_2d((int)x, (int)y, sample_texture(tex, u, v));
		}
	}
}

static void draw_textured_triangle(point a, point b, point c, vertex va,
								   vertex vb, vertex vc, float uv[3][2],
								   const texture *tex)
{
	if (tex == NULL || tex->pixels == NULL || tex->width <= 0 ||
		tex->height <= 0 || canvas.back_buffer == NULL ||
		canvas.depth_buffer == NULL || canvas.w <= 0 || canvas.h <= 0 ||
		va.z <= 0.0f || vb.z <= 0.0f || vc.z <= 0.0f)
		return;

	float area = projected_signed_area(a, b, c);
	if (area <= 0.0f)
		return;

	int min_x = a.x < b.x ? a.x : b.x;
	if (c.x < min_x)
		min_x = c.x;
	int max_x = a.x > b.x ? a.x : b.x;
	if (c.x > max_x)
		max_x = c.x;
	int min_y = a.y < b.y ? a.y : b.y;
	if (c.y < min_y)
		min_y = c.y;
	int max_y = a.y > b.y ? a.y : b.y;
	if (c.y > max_y)
		max_y = c.y;

	int canvas_min_x = -(canvas.w / 2);
	int canvas_max_x = canvas.w - 1 - canvas.w / 2;
	int canvas_min_y = canvas.h / 2 - (canvas.h - 1);
	int canvas_max_y = canvas.h / 2;
	if (min_x < canvas_min_x)
		min_x = canvas_min_x;
	if (max_x > canvas_max_x)
		max_x = canvas_max_x;
	if (min_y < canvas_min_y)
		min_y = canvas_min_y;
	if (max_y > canvas_max_y)
		max_y = canvas_max_y;
	if (min_x > max_x || min_y > max_y)
		return;

	for (int y = min_y; y <= max_y; ++y) {
		for (int x = min_x; x <= max_x; ++x) {
			float w0 = ((float)(b.x - x) * (float)(c.y - y) -
						(float)(b.y - y) * (float)(c.x - x)) /
					   area;
			float w1 = ((float)(c.x - x) * (float)(a.y - y) -
						(float)(c.y - y) * (float)(a.x - x)) /
					   area;
			float w2 = 1.0f - w0 - w1;
			if (w0 < -1e-6f || w1 < -1e-6f || w2 < -1e-6f)
				continue;

			float inv_z = w0 / va.z + w1 / vb.z + w2 / vc.z;
			if (inv_z <= 0.0f)
				continue;

			float depth = 1.0f / inv_z;
			int sx = canvas.w / 2 + x;
			int sy = canvas.h / 2 - y;
			size_t depth_index = (size_t)sy * (size_t)canvas.w + (size_t)sx;
			if (depth >= canvas.depth_buffer[depth_index])
				continue;

			float u = (w0 * uv[0][0] / va.z + w1 * uv[1][0] / vb.z +
					   w2 * uv[2][0] / vc.z) /
					  inv_z;
			float v = (w0 * uv[0][1] / va.z + w1 * uv[1][1] / vb.z +
					   w2 * uv[2][1] / vc.z) /
					  inv_z;
			put_pixel(x, y, sample_texture(tex, u, v));
			canvas.depth_buffer[depth_index] = depth;
		}
	}
}

void render_triangle(triangle tri, const point *projected,
					 const vertex *vertices, const texture *tex)
{
	point a = projected[tri.v[0]];
	point b = projected[tri.v[1]];
	point c = projected[tri.v[2]];

	if (tex != NULL) {
		draw_textured_triangle(a, b, c, vertices[tri.v[0]], vertices[tri.v[1]],
							   vertices[tri.v[2]], tri.uv, tex);
	} else {
		draw_wire_triangle(a, b, c, tri.color);
	}
}

void render_object(object *obj)
{
	if (obj == NULL || obj->vertices == NULL || obj->triangles == NULL ||
		obj->vertex_count == 0)
		return;

	if (obj->projected == NULL) {
		obj->projected = malloc(obj->vertex_count * sizeof(*obj->projected));
		if (obj->projected == NULL)
			return;
	}

	for (size_t i = 0; i < obj->vertex_count; ++i)
		obj->projected[i] = project_vertex(obj->vertices[i]);

	for (size_t i = 0; i < obj->triangle_count; ++i) {
		triangle tri = obj->triangles[i];
		if (tri.v[0] < 0 || (size_t)tri.v[0] >= obj->vertex_count ||
			tri.v[1] < 0 || (size_t)tri.v[1] >= obj->vertex_count ||
			tri.v[2] < 0 || (size_t)tri.v[2] >= obj->vertex_count)
			continue;

		point a = obj->projected[tri.v[0]];
		point b = obj->projected[tri.v[1]];
		point c = obj->projected[tri.v[2]];
		if (obj->vertices[tri.v[0]].z <= 0.0f ||
			obj->vertices[tri.v[1]].z <= 0.0f ||
			obj->vertices[tri.v[2]].z <= 0.0f)
			continue;
		if (projected_signed_area(a, b, c) <= 0.0f)
			continue;

		render_triangle(tri, obj->projected, obj->vertices, obj->texture);
	}
}

static vertex get_object_center(const object *obj)
{
	vertex center = { 0 };

	if (obj == NULL || obj->vertices == NULL || obj->vertex_count == 0)
		return center;

	for (size_t i = 0; i < obj->vertex_count; ++i) {
		center.x += obj->vertices[i].x;
		center.y += obj->vertices[i].y;
		center.z += obj->vertices[i].z;
	}

	center.x /= (float)obj->vertex_count;
	center.y /= (float)obj->vertex_count;
	center.z /= (float)obj->vertex_count;
	return center;
}

void xz_rotate_object(object *obj, float angle)
{
	if (obj == NULL || obj->vertices == NULL)
		return;

	const float s = sinf(angle);
	const float c = cosf(angle);
	vertex center = get_object_center(obj);

	for (size_t i = 0; i < obj->vertex_count; ++i) {
		float x = obj->vertices[i].x - center.x;
		float z = obj->vertices[i].z - center.z;
		obj->vertices[i].x = center.x + x * c - z * s;
		obj->vertices[i].z = center.z + x * s + z * c;
	}
}

void xy_rotate_object(object *obj, float angle)
{
	if (obj == NULL || obj->vertices == NULL)
		return;

	const float s = sinf(angle);
	const float c = cosf(angle);
	vertex center = get_object_center(obj);

	for (size_t i = 0; i < obj->vertex_count; ++i) {
		float x = obj->vertices[i].x - center.x;
		float y = obj->vertices[i].y - center.y;
		obj->vertices[i].x = center.x + x * c - y * s;
		obj->vertices[i].y = center.y + x * s + y * c;
	}
}

void yz_rotate_object(object *obj, float angle)
{
	if (obj == NULL || obj->vertices == NULL)
		return;

	const float s = sinf(angle);
	const float c = cosf(angle);
	vertex center = get_object_center(obj);

	for (size_t i = 0; i < obj->vertex_count; ++i) {
		float y = obj->vertices[i].y - center.y;
		float z = obj->vertices[i].z - center.z;
		obj->vertices[i].y = center.y + y * c - z * s;
		obj->vertices[i].z = center.z + y * s + z * c;
	}
}

void rotate_object(object *obj, float x_angle, float y_angle, float z_angle)
{
	xy_rotate_object(obj, z_angle);
	yz_rotate_object(obj, x_angle);
	xz_rotate_object(obj, y_angle);
}