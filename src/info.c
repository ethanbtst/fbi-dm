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

enum {
    DYNAMIC_DATE,
    DYNAMIC_TIME,
    DYNAMIC_BATTERY,
    DYNAMIC_INFO_COUNT
};

static char dynamic_info_cache[DYNAMIC_INFO_COUNT][255];
static bool dynamic_info_cache_valid[DYNAMIC_INFO_COUNT];

static bool update_dynamic_line(int cache_index, int cursor_index,
                                const char *line);
static bool read_battery_info(char *line, size_t line_size);

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
    update_dynamic_info(input_buffers, input_count);

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

    print_input_info(input_buffers, input_count);
    fflush(stdout);
}

void update_dynamic_info(input_buffer *input_buffers, size_t input_count)
{
    bool changed = false;
    time_t now = time(NULL);
    struct tm local_time;
    if (now == (time_t)-1) {
        perror("Failed to get current time");
    } else if (localtime_r(&now, &local_time) == NULL) {
        perror("Failed to convert current time");
    } else {
        char formatted[249];
        char line[255];

        if (opt_date[0] != '\0') {
            strftime(formatted, sizeof(formatted), opt_date, &local_time);
            snprintf(line, sizeof(line), "Date: %s", formatted);
            changed |= update_dynamic_line(DYNAMIC_DATE, CUR_DATE, line);
        }
        if (opt_time[0] != '\0') {
            strftime(formatted, sizeof(formatted), opt_time, &local_time);
            snprintf(line, sizeof(line), "Time: %s", formatted);
            changed |= update_dynamic_line(DYNAMIC_TIME, CUR_TIME, line);
        }
    }

    if (opt_battery) {
        char line[255];
        if (read_battery_info(line, sizeof(line)))
            changed |= update_dynamic_line(DYNAMIC_BATTERY, CUR_BATTERY, line);
    }

    if (changed)
        restore_input_cursor(input_buffers, input_count);
}

void print_input_info(input_buffer *input_buffers, size_t input_count)
{
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
    restore_input_cursor(input_buffers, input_count);
    fflush(stdout);
}

void restore_input_cursor(input_buffer *input_buffers, size_t input_count)
{
    if (input_buffers == NULL || input_count == 0 ||
        info_curs[CUR_INPUT].row < 0 || info_curs[CUR_INPUT].col < 0)
        return;

    size_t input_index = cur_buff < input_count ? cur_buff : input_count - 1;
    input_buffer *input = &input_buffers[input_index];
    int column = info_curs[CUR_INPUT].col + 1;
    if (input->name != NULL)
        column += (int)strlen(input->name);
    column += 2;
    if (input->show)
        column += (int)strnlen(input->buffer, sizeof(input->buffer));

    move(info_curs[CUR_INPUT].row + (int)input_index, column);
    fflush(stdout);
}

static bool update_dynamic_line(int cache_index, int cursor_index,
                                const char *line)
{
    cursor location = info_curs[cursor_index];
    if (location.row < 0 || location.col < 0)
        return false;

    char *cached = dynamic_info_cache[cache_index];
    if (dynamic_info_cache_valid[cache_index] &&
        strcmp(cached, line) == 0)
        return false;

    if (line[0] == '\0' && !dynamic_info_cache_valid[cache_index]) {
        cached[0] = '\0';
        dynamic_info_cache_valid[cache_index] = true;
        return false;
    }

    move(location.row, location.col);
    clear_line();
    if (line[0] != '\0')
        printf("%s", line);
    snprintf(cached, sizeof(dynamic_info_cache[cache_index]), "%s", line);
    dynamic_info_cache_valid[cache_index] = true;
    return true;
}

static bool read_battery_info(char *line, size_t line_size)
{
    glob_t bat_file = { 0 };
    int glob_result =
        glob("/sys/class/power_supply/*/capacity", GLOB_ERR, NULL, &bat_file);

    if (glob_result == GLOB_NOMATCH) {
        line[0] = '\0';
        globfree(&bat_file);
        return true;
    }
    if (glob_result != 0) {
        fprintf(stderr, "Failed to find battery capacity files\n");
        return false;
    }

    FILE *fd = fopen(bat_file.gl_pathv[0], "r");
    if (fd == NULL) {
        perror(bat_file.gl_pathv[0]);
        globfree(&bat_file);
        return false;
    }

    int charge;
    if (fscanf(fd, "%d", &charge) != 1) {
        fprintf(stderr, "Failed to read battery capacity from %s\n",
                bat_file.gl_pathv[0]);
        fclose(fd);
        globfree(&bat_file);
        return false;
    }
    fclose(fd);
    globfree(&bat_file);

    int length = snprintf(line, line_size, "Bat: %d%%", charge);
    if (length < 0 || (size_t)length >= line_size) {
        fprintf(stderr, "Battery status text is too long\n");
        return false;
    }
    return true;
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
    char line[255];
    if (read_battery_info(line, sizeof(line)) && line[0] != '\0')
        printf("%s", line);
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