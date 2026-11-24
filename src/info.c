#include <stdio.h>
#include <time.h>
#include <glob.h>
#include <string.h>

#include "info.h"
#include "sessions.h"
#include "config.h"

char info_buff[255];
int brightness_delta;
int max_brightness;
int brightness;
cursor info_curs[CUR_NUM_MAX];

int info_rows_before_controls(void)
{
    int rows = 2;
    if (strlen(opt_top_text) != 0)
        rows += 2;
    if (opt_hostname)
        rows += 2;
    if (opt_kernel)
        rows += 2;
    if (strlen(opt_date) != 0)
        rows += 2;
    if (strlen(opt_time) != 0)
        rows += 2;
    if (opt_battery)
        rows += 2;
    return rows;
}

void set_cursors_info(int start_row, int start_col, int line_count_down)
{
    move(start_row, start_col);

    if (strlen(opt_top_text) != 0) {
        info_curs[CUR_TOP_TEXT] = get_cur();
        move(cur->row + 2, start_col);
    } else
        info_curs[CUR_TOP_TEXT] = (cursor){ -1, -1 };
    if (opt_hostname) {
        info_curs[CUR_HOSTNAME] = get_cur();
        move(cur->row + 2, start_col);
    } else
        info_curs[CUR_HOSTNAME] = (cursor){ -1, -1 };
    if (opt_kernel) {
        info_curs[CUR_KERNEL] = get_cur();
        move(cur->row + 2, start_col);
    } else
        info_curs[CUR_KERNEL] = (cursor){ -1, -1 };
    if (strlen(opt_date) != 0) {
        info_curs[CUR_DATE] = get_cur();
        move(cur->row + 2, start_col);
    } else
        info_curs[CUR_DATE] = (cursor){ -1, -1 };
    if (strlen(opt_time) != 0) {
        info_curs[CUR_TIME] = get_cur();
        move(cur->row + 2, start_col);
    } else
        info_curs[CUR_TIME] = (cursor){ -1, -1 };
    if (opt_battery) {
        info_curs[CUR_BATTERY] = get_cur();
        move(cur->row + 2, start_col);
    } else
        info_curs[CUR_BATTERY] = (cursor){ -1, -1 };

    info_curs[CUR_SESSION] = get_cur();
    info_curs[CUR_CONTROLS] = (cursor){ .row = line_count_down, .col = 0 };
    info_curs[CUR_INPUT] = (cursor){ .row = line_count_down + 2, .col = 0 };
}

void print_all_info(input_buffer *input_buffers, size_t input_count)
{
    if (move(info_curs[CUR_TOP_TEXT].row, info_curs[CUR_TOP_TEXT].col))
        printf("%s", opt_top_text);
    if (move(info_curs[CUR_HOSTNAME].row, info_curs[CUR_HOSTNAME].col))
        file_info("/etc/hostname", "%254s", "[%s]");
    if (move(info_curs[CUR_KERNEL].row, info_curs[CUR_KERNEL].col))
        file_info("/proc/version", "%*s %*s %254s", "Kernel: %s");
    if (move(info_curs[CUR_DATE].row, info_curs[CUR_DATE].col))
        time_info("Date", opt_date);
    if (move(info_curs[CUR_TIME].row, info_curs[CUR_TIME].col))
        time_info("Time", opt_time);
    if (move(info_curs[CUR_BATTERY].row, info_curs[CUR_BATTERY].col))
        battery_info();

    move(info_curs[CUR_SESSION].row, info_curs[CUR_SESSION].col);
    session_info(sessions, session_cur, session_count);

    move(info_curs[CUR_CONTROLS].row, info_curs[CUR_CONTROLS].col);
    printf("[F1] Shutdown [F2] Reboot ");
    if (opt_brightness) {
        set_brightness_values();
        printf("[F3] Increase brightness [F4] Decrease brightness\n\n");
    } else {
        printf("\n\n");
    }

    size_t visible_count = cur_buff < input_count ? cur_buff + 1 : input_count;
    for (size_t i = 0; i < visible_count; ++i) {
        if (!move(info_curs[CUR_INPUT].row + (int)i, info_curs[CUR_INPUT].col))
            continue;
        clear_line();
        if (input_buffers[i].show)
            printf("%s: %s", input_buffers[i].name, input_buffers[i].buffer);
        else
            printf("%s: ", input_buffers[i].name);
    }
    fflush(stdout);
}

void file_info(char *filename, char *in_fmt, char *out_fmt)
{
    FILE *fd = fopen(filename, "r");
    if (fd == NULL)
        return;
    fscanf(fd, in_fmt, info_buff);
    fclose(fd);
    printf(out_fmt, info_buff);
}

void time_info(char *name, char *fmt)
{
    time_t t;
    struct tm *tmp;
    time(&t);
    tmp = localtime(&t);

    strftime(info_buff, sizeof(info_buff), fmt, tmp);
    printf("%s: %s", name, info_buff);
}

void battery_info()
{
    glob_t bat_file = { 0 };
    int glob_result =
        glob("/sys/class/power_supply/*/capacity", GLOB_ERR, NULL, &bat_file);

    if (glob_result == GLOB_NOMATCH)
        return;

    int charge = 0;
    FILE *fd = fopen(bat_file.gl_pathv[0], "r");
    fscanf(fd, "%d", &charge);
    fclose(fd);
    globfree(&bat_file);
    printf("Bat: %d%%", charge);
}

void session_info(session *sessions, int session_cur, int count)
{
    if (sessions == NULL || session_cur < 0 || session_cur > count)
        return;

    printf("%s: < %s >", sessions[session_cur].type,
           sessions[session_cur].name);
}

void set_brightness_values()
{
    glob_t backlight_file = { 0 };

    int glob_result = glob("/sys/class/backlight/*/max_brightness", GLOB_ERR,
                           NULL, &backlight_file);
    if (glob_result == GLOB_NOMATCH)
        return;

    glob_result = glob("/sys/class/backlight/*/brightness",
                       GLOB_ERR | GLOB_APPEND, NULL, &backlight_file);
    if (glob_result == GLOB_NOMATCH)
        return;

    FILE *fd = fopen(backlight_file.gl_pathv[0], "r");
    fscanf(fd, "%d", &max_brightness);
    fclose(fd);

    fd = fopen(backlight_file.gl_pathv[1], "r");
    fscanf(fd, "%d", &brightness);
    fclose(fd);

    globfree(&backlight_file);

    brightness_delta = ((5 * max_brightness) / 100);
}

void update_brightness(int change)
{
    glob_t backlight_file = { 0 };

    int glob_result = glob("/sys/class/backlight/*/brightness", GLOB_ERR, NULL,
                           &backlight_file);
    if (glob_result == GLOB_NOMATCH) {
        return;
    }

    brightness += (change * brightness_delta);
    if (brightness < 0)
        brightness = 0;
    else if (brightness > max_brightness)
        brightness = max_brightness;

    FILE *fd = fopen(backlight_file.gl_pathv[0], "w");
    if (fd == NULL) {
        globfree(&backlight_file);
        return;
    }
    fprintf(fd, "%d", brightness);
    fclose(fd);

    globfree(&backlight_file);
}