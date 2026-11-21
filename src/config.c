#include "config.h"
#include "main_utils.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

char opt_small_pic[255];
char opt_big_pic[255];
bool opt_hostname;
bool opt_kernel;
char opt_top_text[255];
char opt_date[255];
char opt_time[255];
bool opt_battery;
bool opt_brightness;

void default_config()
{
	strcpy(opt_small_pic, "/opt/fb-dm/img/logo_linux_clut224.ppm");
	strcpy(opt_big_pic, "distro");
	strcpy(opt_top_text, "Welcome!");
	opt_hostname = true;
	opt_kernel = true;
	strcpy(opt_date, "%A %B %d, %Y");
	strcpy(opt_time, "%R");
	opt_battery = false;
	opt_brightness = false;
}

void set_distro_logo()
{
	FILE *fd = fopen("/etc/os-release", "r");
	if (fd == NULL)
		return;

	char line[255];
	char *option;
	char *value;

	while (fgets(line, sizeof(line), fd)) {
		if (!parse_line(line, &option, &value))
			continue;

		if (strcasecmp(option, "id") == 0)
			snprintf(opt_big_pic, sizeof(opt_big_pic),
					 "/opt/fb-dm/img/distro_logos/%s.ppm", value);
	}
	fclose(fd);
}

void get_config()
{
	FILE *fd = fopen("/opt/fb-dm/config.ini", "r");

	if (!fd)
		return;

	char line[255];
	char *option;
	char *value;

	while (fgets(line, sizeof(line), fd)) {
		if (!parse_line(line, &option, &value))
			continue;

		if (strcmp(option, "small_pic") == 0)
			strcpy(opt_small_pic, value);

		if (strcmp(option, "big_pic") == 0)
			strcpy(opt_big_pic, value);

		if (strcmp(option, "top_text") == 0)
			strcpy(opt_top_text, value);

		if (strcmp(option, "hostname") == 0)
			if (strcmp(value, "false") == 0)
				opt_hostname = false;

		if (strcmp(option, "kernel") == 0)
			if (strcmp(value, "false") == 0)
				opt_kernel = false;

		if (strcmp(option, "date") == 0)
			strcpy(opt_date, value);

		if (strcmp(option, "time") == 0)
			strcpy(opt_time, value);

		if (strcmp(option, "battery") == 0)
			if (strcmp(value, "true") == 0)
				opt_battery = true;

		if (strcmp(option, "brightness") == 0)
			if (strcmp(value, "true") == 0)
				opt_brightness = true;
	}

	fclose(fd);
}
