#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>

#include "config.h"
#include "parser.h"

char opt_small_pic[255];
char opt_big_pic[255];
int opt_small_max_h;
int opt_small_max_w;
int opt_big_max_h;
int opt_big_max_w;
bool opt_spin;
float opt_rot_period;

bool opt_hostname;
bool opt_kernel;
char opt_top_text[255];
char opt_date[255];
char opt_time[255];
bool opt_battery;
bool opt_brightness;

void default_config()
{
    strcpy(opt_small_pic, "/opt/fbi-dm/img/linux_logo.png");
    strcpy(opt_big_pic, "distro");
    opt_small_max_h = 80;
    opt_small_max_w = 80;
    opt_big_max_h = 260;
    opt_big_max_w = 260;
    opt_spin = false;
    opt_rot_period = 4.0f;

    strcpy(opt_top_text, "Welcome!");
    opt_hostname = true;
    opt_kernel = true;
    strcpy(opt_date, "%A %B %d, %Y");
    strcpy(opt_time, "%R:%S");
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
                     "/opt/fbi-dm/img/distro_logos/%s.png", value);
    }
    fclose(fd);
}

static void set_rotation_period(const char *value)
{
    char *end;
    errno = 0;
    float period = strtof(value, &end);
    if (errno == ERANGE || end == value || *end != '\0' ||
        !isfinite(period) || period <= 0.0f) {
        fprintf(stderr,
                "Invalid rotation_period '%s'; expected positive seconds\n",
                value);
        return;
    }

    opt_rot_period = period;
}

void get_config()
{
    FILE *fd = fopen("/opt/fbi-dm/config.ini", "r");

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

        if (strcmp(option, "small_max_h") == 0)
            opt_small_max_h = atoi(value);

        if (strcmp(option, "small_max_w") == 0)
            opt_small_max_w = atoi(value);

        if (strcmp(option, "big_max_h") == 0)
            opt_big_max_h = atoi(value);

        if (strcmp(option, "big_max_w") == 0)
            opt_big_max_w = atoi(value);

        if (strcmp(option, "spin") == 0)
            if (strcasecmp(value, "true") == 0)
                opt_spin = true;

        if (strcmp(option, "rotation_period") == 0)
            set_rotation_period(value);

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
