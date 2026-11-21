#ifndef MAIN_UTILS_H
#define MAIN_UTILS_H

#include <stdbool.h>

char *trim(char *text);
bool parse_line(char *line, char **option, char **value);

#endif