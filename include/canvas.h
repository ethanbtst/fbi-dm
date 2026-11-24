#ifndef CANVAS_H
#define CANVAS_H

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
// Following
// https://www.gabrielgambetta.com/computer-graphics-from-scratch

#define ARRAY_LEN(arr) sizeof(arr) / sizeof(arr[0])
#define frac(x) x - floor(x)
typedef struct {
	int x;
	int y;
} point;

typedef struct {
	float x;
	float y;
	float z;
} vertex;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
} colour;

typedef struct {
	int width;
	int height;
	unsigned char *pixels;
} texture;

typedef struct {
	int v[3];
	float uv[3][2];
	colour color;
} triangle;

typedef struct {
	vertex *vertices;
	size_t vertex_count;
	triangle *triangles;
	size_t triangle_count;
	point *projected;
	texture *texture;
} object;

typedef struct {
	char *fbp;
	char *back_buffer;
	float *depth_buffer;
	int fd;
	int w;
	int h;
	int line_length;
	int bpp;
	int screensize;
} buffer;

extern buffer canvas;
extern const float viewport_h;
extern const float projection_distance;
extern float viewport_w;

bool load_texture(texture *tex, const char *filename);

void destroy_texture(texture *tex);

bool resize_texture(texture *tex, int max_width, int max_height);

bool create_buffer(buffer *fb, const char *device);

void destroy_buffer(buffer *fb);

void swap_points(point *p0, point *p1);

void put_pixel(int x, int y, colour c);

void draw2drectangle(int posX, int posY, int width, int height);

void clear();

void present();

int *interpolate(int i0, int d0, int i1, int d1);

void draw_line(point p0, point p1, colour c);

void draw_wire_triangle(point p0, point p1, point p2, colour c);

void draw_filled_triangle(point p0, point p1, point p2, colour c);

point viewport_to_canvas(float x, float y);

point project_vertex(vertex v);

void draw2drectangle_texture(int posX, int posY, int width, int height,
							 const texture *tex);

void render_triangle(triangle tri, const point *projected,
					 const vertex *vertices, const texture *tex);

void render_object(object *obj);

void xz_rotate_object(object *obj, float angle);

void xy_rotate_object(object *obj, float angle);

void yz_rotate_object(object *obj, float angle);

void rotate_object(object *obj, float x_angle, float y_angle, float z_angle);

#endif