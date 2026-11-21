#ifndef TERM_H
#define TERM_H
#include <termios.h>
#include <stddef.h>
#include <stdbool.h>
#include "image.h"

typedef struct {
	char *name;
	char buffer[255];
	bool show;
} input_buffer;

typedef struct {
	int col;
	int row;
} cursor;

void setup_term();

// Moves the cursor to a given location
void move_cur(cursor *cur, int row, int col);

// Reads a line from the terminal into a buffer
void readline(cursor *cur, char *buff);

// Reads one terminal key, including navigation and function-key escape sequences
int readkey(void);

// Queries the terminal for the current cursor position
cursor get_cur(void);
int get_row(void);
int get_col(void);

void cleanup(image *images[], int image_count, struct termios *term,
			 buffer *buffer);

void key_loop(input_buffer *input_buffers, size_t input_count);

void toggle_echo(bool enabled);

#define KEY_BACKSPACE 127
#define KEY_LEFT 0x101
#define KEY_RIGHT 0x102
#define KEY_F1 0x103
#define KEY_F2 0x104
#define KEY_F3 0x105
#define KEY_F4 0x106
#define clear_line() printf("\x1b[K")
#define move(row, col) move_cur(cur, row, col);
#define echo()             \
	{                      \
		toggle_echo(true); \
	}
#define no_echo()           \
	{                       \
		toggle_echo(false); \
	}

extern cursor *cur;
extern struct termios current_term;
extern struct termios saved_term;
#endif