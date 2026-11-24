#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

char *trim(char *text);
bool parse_line(char *line, char **option, char **value);

#endif