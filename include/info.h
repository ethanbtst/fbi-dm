#ifndef INFO_H
#define INFO_H

#include "sessions.h"
#include "term.h"

enum {
    CUR_TOP_TEXT,
    CUR_HOSTNAME,
    CUR_KERNEL,
    CUR_DATE,
    CUR_TIME,
    CUR_BATTERY,
    CUR_SESSION,
    CUR_CONTROLS,
    CUR_INPUT,
    CUR_NUM_MAX
};

extern cursor info_curs[CUR_NUM_MAX];

// Stores where information is placed into `cursor info_curs[]`
void set_cursors_info(int start_row, int start_col, int line_count_down);

int info_rows_before_controls(void);

// Prints all info to the terminal
void print_all_info(input_buffer *input_buffers, size_t input_count);

// Refreshes date, time, and battery rows only when their displayed text changes
void update_dynamic_info(input_buffer *input_buffers, size_t input_count);

void print_input_info(input_buffer *input_buffers, size_t input_count);

void restore_input_cursor(input_buffer *input_buffers, size_t input_count);

void file_info(char *filename, char *in_fmt, char *out_fmt);

void time_info(char *name, char *fmt);

void session_info(session *sessions, int session_cur, int count);

void battery_info();

void set_brightness_values();

void update_brightness(int change);

#endif