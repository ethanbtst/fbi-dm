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

#define FPS 24

pthread_t keyThread, displayThread;
static volatile sig_atomic_t display_running;
texture small_img = { 0 };
texture large_img = { 0 };

vertex pane_vertices[8] = {
    { 1.0f, -1.0f, 0.0f },      { -1.0f, -1.0f, 0.0f },
    { -1.0f, 1.0f, 0.0f },      { 1.0f, 1.0f, 0.0f },
    { 1.0f, -1.0f, 0.000001f }, { -1.0f, -1.0f, 0.000001f },
    { -1.0f, 1.0f, 0.000001f }, { 1.0f, 1.0f, 0.000001f },
};

triangle pane_triangles[] = {
    { .v = { 0, 1, 2 },
      .uv = { { 1, 0 }, { 0, 0 }, { 0, 1 } },
      .color = { 255, 0, 0 } },
    { .v = { 0, 2, 3 },
      .uv = { { 1, 0 }, { 0, 1 }, { 1, 1 } },
      .color = { 255, 0, 0 } },
    { .v = { 4, 0, 3 },
      .uv = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
      .color = { 0, 0, 255 } },
    { .v = { 4, 3, 7 },
      .uv = { { 0, 0 }, { 1, 1 }, { 0, 1 } },
      .color = { 0, 0, 255 } },
    { .v = { 5, 4, 7 },
      .uv = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
      .color = { 0, 255, 0 } },
    { .v = { 5, 7, 6 },
      .uv = { { 0, 0 }, { 1, 1 }, { 0, 1 } },
      .color = { 0, 255, 0 } },
    { .v = { 1, 5, 6 },
      .uv = { { 0, 0 }, { 1, 0 }, { 1, 1 } },
      .color = { 255, 0, 0 } },
    { .v = { 1, 6, 2 },
      .uv = { { 0, 0 }, { 1, 1 }, { 0, 1 } },
      .color = { 255, 0, 0 } },
    { .v = { 4, 5, 1 },
      .uv = { { 1, 0 }, { 0, 0 }, { 0, 1 } },
      .color = { 0, 255, 0 } },
    { .v = { 4, 1, 0 },
      .uv = { { 1, 0 }, { 0, 1 }, { 1, 1 } },
      .color = { 0, 255, 0 } },
    { .v = { 2, 6, 7 },
      .uv = { { 0, 0 }, { 0, 1 }, { 1, 1 } },
      .color = { 0, 0, 255 } },
    { .v = { 2, 7, 3 },
      .uv = { { 0, 0 }, { 1, 1 }, { 1, 0 } },
      .color = { 0, 0, 255 } },
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
    float d_angle = (2.0 * M_PI)  / (opt_rot_period * FPS);
    while (display_running) {
        struct timespec start, stop;
        clock_gettime(CLOCK_MONOTONIC, &start);
        clear();

        if (opt_spin)
            rotate_object(&pane, 0.0f, d_angle, 0.0f);
            
        render_object(&pane);
        for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); ++i)
            draw2drectangle_texture(4 + i * small_img.width, 0, small_img.width,
                                    small_img.height, &small_img);
        present();
        print_all_info(input_buffers, 2);
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
        fprintf(stderr, "Failed to fit loaded textures within configured limits\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }

    if (!fit_object_to_area(&pane, 0.0f, (float)small_img.height,
                            (float)large_img.width, (float)large_img.height,
                            5.0f)) {
        fprintf(stderr, "Failed to fit 3D object into image area\n");
        cleanup((texture *[]){ &small_img, &large_img }, 2, &saved_term,
                &canvas);
        return 1;
    }

    present();

    // Text positioning
    struct winsize w;
    ioctl(STDERR_FILENO, TIOCGWINSZ, &w);
    int font_height = canvas.h / w.ws_row;
    int font_width = canvas.w / w.ws_col;
    int right = large_img.width / font_width + 2;

    sessions = get_sessions(&session_count);
    if ((session_cur = get_session_index(session_count)) == -1)
        session_cur = (int)session_count;

    set_cursors_info((small_img.height / font_height) + 2, right,
                     ((small_img.height + large_img.height) / font_height) + 2);

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
