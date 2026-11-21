#include <string.h>
#include <ctype.h>
#include "main_utils.h"

char *trim(char *text)
{
	while (isspace((unsigned char)*text))
		text++;

	char *end = text + strlen(text);
	while (end > text && isspace((unsigned char)end[-1]))
		*--end = '\0';

	return text;
}

bool parse_line(char *line, char **option, char **value)
{
	char *separator = strchr(line, '=');

	if (separator == NULL)
		return false;

	*separator = '\0';
	*option = trim(line);
	*value = trim(separator + 1);

	if (**option == '\0' || **option == '#')
		return false;

	if ((*value)[0] == '\0')
		return false;

	return true;
}