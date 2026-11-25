#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <signal.h>

#include "pam.h"
#include "config.h"
#include "sessions.h"
#include "info.h"
#define CANVAS_IMPLEMENTATION
#include "fb_canvas.h"

#define FPS 60

pthread_t keyThread, displayThread;
static volatile sig_atomic_t display_running;
texture small_img = { 0 };
texture large_img = { 0 };
static int pane_region_x;
static int pane_region_y;
static int pane_region_width;
static int pane_region_height;

vertex pane_vertices[8] = {
    { 1.0f, -1.0f, 0.0f },      { -1.0f, -1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f },      { 1.0f, 1.0f, 0.0f },
    { 1.0f, -1.0f, 0.000001f }, { -1.0f, -1.0f, 0.000001f },
    { -1.0f, 1.0f, 0.000001f }, { 1.0f, 1.0f, 0.000001f },
};

triangle pane_triangles[] = {
    { .v = { 0, 1, 2 },
      .uv = { { 1, 0 }, { 0, 0 }, { 0, 1 } },
      .c = { 255, 0, 0 } },
    { .v = { 0, 2, 3 },
      .uv = { { 1, 0 }, { 0, 1 }, { 1, 1 } },
      .c = { 255, 0, 0 } },
    { .v = { 4, 0, 3 },
      .uv = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
      .c = { 0, 0, 255 } },
    { .v = { 4, 3, 7 },
      .uv = { { 0, 0 }, { 1, 1 }, { 0, 1 } },
      .c = { 0, 0, 255 } },
    { .v = { 5, 4, 7 },
      .uv = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
      .c = { 0, 255, 0 } },
    { .v = { 5, 7, 6 },
      .uv = { { 0, 0 }, { 1, 1 }, { 0, 1 } },
      .c = { 0, 255, 0 } },
    { .v = { 1, 5, 6 },
      .uv = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
      .c = { 255, 0, 0 } },
    { .v = { 1, 6, 2 },
      .uv = { { 0, 0 }, { 1, 1 }, { 0, 1 } },
      .c = { 255, 0, 0 } },
    { .v = { 4, 5, 1 },
      .uv = { { 1, 0 }, { 0, 0 }, { 0, 1 } },
      .c = { 0, 255, 0 } },
    { .v = { 4, 1, 0 },
      .uv = { { 1, 0 }, { 0, 1 }, { 1, 1 } },
      .c = { 0, 255, 0 } },
    { .v = { 2, 6, 7 },
      .uv = { { 0, 0 }, { 0, 1 }, { 1, 1 } },
      .c = { 0, 0, 255 } },
    { .v = { 2, 7, 3 },
      .uv = { { 0, 0 }, { 1, 1 }, { 1, 0 } },
      .c = { 0, 0, 255 } },
};

object pane = {
    .vertices = pane_vertices,
    .vertex_count = ARRAY_LEN(pane_vertices),
    .triangles = pane_triangles,
    .triangle_count = ARRAY_LEN(pane_triangles),
};

static void delay_frame(const struct timespec *start,
                        const struct timespec *stop);

static void *display_loop(void *input_buffers)
{
    pane.texture = &large_img;
    float d_angle = (2.0 * M_PI) / (opt_rot_period * FPS);
    bool first_frame = true;
    time_t last_info_update = 0;

    while (display_running) {
        struct timespec start, stop;
        clock_gettime(CLOCK_MONOTONIC, &start);

        if (first_frame) {
            clear();
            for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); ++i)
                draw2drectangle_texture(4 + i * small_img.w, 0, small_img.w,
                                        small_img.h, &small_img);
            present();

            if (opt_spin)
                rotate_object(&pane, 0.0f, d_angle, 0.0f);
            render_object(&pane);
            present_region(pane_region_x, pane_region_y, pane_region_width,
                           pane_region_height);
            print_all_info(input_buffers, 2);
            last_info_update = time(NULL);
            first_frame = false;
        } else if (opt_spin) {
            clear_region(pane_region_x, pane_region_y, pane_region_width,
                         pane_region_height);
            rotate_object(&pane, 0.0f, d_angle, 0.0f);
            render_object(&pane);
            present_region(pane_region_x, pane_region_y, pane_region_width,
                           pane_region_height);
        }

        time_t now = time(NULL);
        if (now != (time_t)-1 && now != last_info_update) {
            update_dynamic_info();
            last_info_update = now;
        }

        clock_gettime(CLOCK_MONOTONIC, &stop);
        delay_frame(&start, &stop);
    }
    return NULL;
}

static void *keyboard_loop(void *buffers)
{
    key_loop(buffers, 2);
    display_running = 0;
    return NULL;
}

static void delay_frame(const struct timespec *start,
                        const struct timespec *stop)
{
    const long frame_ns = 1000000000L / FPS;
    long elapsed_ns = (stop->tv_sec - start->tv_sec) * 1000000000L +
                      stop->tv_nsec - start->tv_nsec;
    long remaining_ns = frame_ns - elapsed_ns;

    if (remaining_ns > 0) {
        struct timespec delay = {
            .tv_sec = remaining_ns / 1000000000L,
            .tv_nsec = remaining_ns % 1000000000L,
        };
        nanosleep(&delay, NULL);
    }
}

int main()
{
    // Configs
    default_config();
    setup_term();
    get_config();
    setlocale(LC_ALL, "");

    if (strcmp(opt_big_pic, "distro") == 0)
        set_distro_logo();

    if (!create_buffer(&canvas, "/dev/fb0")) {
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
        return 1;
    }

    // Image stuff
    if (!load_texture(&small_img, opt_small_pic))
        load_texture(&small_img, "/opt/fbi-dm/img/linux_logo.png");
    if (!load_texture(&large_img, opt_big_pic))
        load_texture(&large_img, "/opt/fbi-dm/img/fbi.png");

    if (!resize_texture(&small_img, opt_small_max_w, opt_small_max_h) ||
        !resize_texture(&large_img, opt_big_max_w, opt_big_max_h)) {
        fprintf(stderr,
                "Failed to fit loaded textures within configured limits\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }

    // Image and text dimensions
    struct winsize w = { 0 };
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &w) == -1) {
        perror("Failed to get terminal dimensions");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }
    if (w.ws_row == 0 || w.ws_col == 0) {
        fprintf(stderr, "Terminal reported invalid dimensions\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }

    int font_height = canvas.h / w.ws_row;
    int font_width = canvas.w / w.ws_col;
    if (font_height == 0 || font_width == 0) {
        fprintf(stderr,
                "Framebuffer is too small for the terminal dimensions\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }

    float image_aspect = (float)large_img.w / (float)large_img.h;
    for (size_t i = 0; i < ARRAY_LEN(pane_vertices); ++i)
        pane_vertices[i].x *= image_aspect;

    int info_rows = info_rows_before_controls();
    float min_image_height = (float)info_rows * (float)font_height;
    float img_height = fmaxf((float)large_img.h, min_image_height);
    float pane_height = img_height;
    float pane_width = pane_height * image_aspect;
    if (pane_width > (float)canvas.w) {
        pane_width = (float)canvas.w;
        pane_height = pane_width / image_aspect;
    }
    const float pane_top = (float)small_img.h + 5.0f;
    if (!fit_object_to_area(&pane, 0.0f, pane_top, pane_width, pane_height,
                            5.0f)) {
        fprintf(stderr, "Failed to fit 3D object into image area\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }
    pane_region_x = 0;
    pane_region_y = (int)floorf(pane_top);
    pane_region_width = (int)ceilf(pane_width);
    pane_region_height = (int)ceilf(img_height);

    int info_start_row = (small_img.h / font_height) + 2;
    int controls_row = (int)((pane_top + img_height) / font_height) + 2;
    int min_controls_row = info_start_row + info_rows;
    if (controls_row < min_controls_row)
        controls_row = min_controls_row;
    int right = (int)ceilf(pane_width / (float)font_width) + 2;

    sessions = get_sessions(&session_count);
    if ((session_cur = get_session_index(session_count)) == -1)
        session_cur = (int)session_count;

    set_cursors_info(info_start_row, right, controls_row);

    // Text input
    input_buffer in[2];
    in[0] = (input_buffer){ "login", { 0 }, true };
    in[1] = (input_buffer){ "password", { 0 }, false };

    // Main drawing/keyboard threads
    display_running = 1;
    if (pthread_create(&displayThread, NULL, display_loop, &in) != 0) {
        perror("Failed to create display thread");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }
    if (pthread_create(&keyThread, NULL, keyboard_loop, &in) != 0) {
        perror("Failed to create keyboard thread");
        display_running = 0;
        pthread_join(displayThread, NULL);
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }

    pthread_join(displayThread, NULL);
    display_running = 0;
    pthread_join(keyThread, NULL);

    // Auth
    pam_handle_t *pamh = NULL;
    if (check_user(in[0].buffer, in[1].buffer, &pamh)) {
        struct passwd *pw = getpwnam(in[0].buffer);

        if (!pw) {
            perror("getpwnam");
            cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                    &canvas);
            pam_end(pamh, PAM_SYSTEM_ERR);
            return 1;
        }

        session selected_session = { 0 };
        if (sessions != NULL && session_cur >= 0 &&
            session_cur <= (int)session_count)
            selected_session = copy_session(&sessions[session_cur]);

        if (selected_session.type == NULL ||
            !setup_session_environment(&selected_session)) {
            perror("session environment setup failed");
            free_session(&selected_session);
            pam_setcred(pamh, PAM_DELETE_CRED);
            pam_end(pamh, PAM_SYSTEM_ERR);
            cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                    &canvas);
            return 1;
        }

        if (pam_open_session(pamh, 0) != PAM_SUCCESS) {
            perror("pam_open_session failed");
            pam_setcred(pamh, PAM_DELETE_CRED);
            pam_end(pamh, PAM_SESSION_ERR);
            free_session(&selected_session);
            cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                    &canvas);
            return 1;
        }

        set_session_index(session_cur);

        if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
            setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
            perror("privilege drop failed");
            pam_close_session(pamh, PAM_SILENT);
            pam_setcred(pamh, PAM_DELETE_CRED);
            pam_end(pamh, PAM_SYSTEM_ERR);
            free_session(&selected_session);
            cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                    &canvas);
            return 1;
        }

        if (!setup_env(pamh, pw)) {
            perror("environment setup failed");
            pam_close_session(pamh, PAM_SILENT);
            pam_setcred(pamh, PAM_DELETE_CRED);
            pam_end(pamh, PAM_SYSTEM_ERR);
            free_session(&selected_session);
            cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                    &canvas);
            return 1;
        }

        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        start_session(pw, &selected_session);
        free_session(&selected_session);

        pam_close_session(pamh, PAM_SILENT);
        pam_setcred(pamh, PAM_DELETE_CRED);
        pam_end(pamh, PAM_SUCCESS);
    } else {
        printf("Login Failed\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
    }
    return 0;
}
