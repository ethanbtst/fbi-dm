#ifndef INFO_H
#define INFO_H

#include "sessions.h"

void file_info(char *filename, char *in_fmt, char *out_fmt);

void time_info(char *name, char *fmt);

void session_info(session *sessions, int session_cur, int count);

void battery_info();

void set_brightness_values();

void update_brightness(int change);

#endif