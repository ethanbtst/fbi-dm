#include "term_utils.h"
#include "pam.h"
#include "config.h"
#include "sessions.h"
#include "image.h"
#include "info.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <grp.h>
#include <pwd.h>
#include <sys/ioctl.h>
#include <pthread.h>

int main()
{
	default_config();

	setup_term();
	get_config();
	setlocale(LC_ALL, "");

	if (strcmp(opt_big_pic, "distro") == 0)
		set_distro_logo();

	buffer *buf = &(buffer){ .fbp = NULL };
	if (!create_buffer(buf, "/dev/fb0")) {
		tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
		return 1;
	}

	image small_img = { 0 };
	image large_img = { 0 };
	if (!load_ppm(&small_img, opt_small_pic) ||
		!load_ppm(&large_img, opt_big_pic)) {
		cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
		return 1;
	}

	// Positioning
	struct winsize w;
	ioctl(STDERR_FILENO, TIOCGWINSZ, &w);
	int font_height = buf->h / w.ws_row;
	int font_width = buf->w / w.ws_col;

	int right = large_img.w / font_width + 2;
	move((small_img.h / font_height) + 2, right);

	// Display images
	for (int i = 0; i < sysconf(_SC_NPROCESSORS_ONLN); ++i)
		draw_image(buf, &small_img, i * small_img.w, 0);
	draw_image(buf, &large_img, 0, small_img.h);

	// Info printing
	if (strlen(opt_top_text) != 0){
		printf("%s", opt_top_text);
		move(cur->row + 2, right);
	}
	if (opt_hostname)
		file_info("/etc/hostname", "%254s", "[%s]");
	if (opt_kernel)
		file_info("/proc/version", "%*s %*s %254s", "Kernel: %s");
	if (strlen(opt_date) != 0)
		time_info("Date", opt_date);
	if (strlen(opt_time) != 0)
		time_info("Time", opt_time);
	if (opt_battery)
		battery_info();

	// Session Picking

	session_row = cur->row;
	session_col = cur->col;
	sessions = get_sessions(&session_count);
	if ((session_cur = get_session_index(session_count)) == -1)
		session_cur = (int)session_count;
	session_info(sessions, session_cur, session_count);

	move(((small_img.h + large_img.h) / font_height) + 1, 0);
	printf("[F1] Shutdown [F2] Reboot ");

	if (opt_brightness) {
		set_brightness_values();
		printf("[F3] Increase brightness [F4] Decrease brightness\n\n");
	} else
		printf("\n\n");

	// Text input
	input_buffer in[2];
	in[0] = (input_buffer){ "login", { 0 }, true };
	in[1] = (input_buffer){ "password", { 0 }, false };

	key_loop(in, 2);

	// Auth
	pam_handle_t *pamh = NULL;
	if (check_user(in[0].buffer, in[1].buffer, &pamh)) {
		struct passwd *pw = getpwnam(in[0].buffer);

		if (!pw) {
			perror("getpwnam");
			cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
			pam_end(pamh, PAM_SYSTEM_ERR);
			return 1;
		}

		if (pam_open_session(pamh, 0) != PAM_SUCCESS) {
			fprintf(stderr, "pam_open_session failed\n");
			pam_setcred(pamh, PAM_DELETE_CRED);
			pam_end(pamh, PAM_SESSION_ERR);
			cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
			return 1;
		}

		set_session_index(session_cur);
		
		if (initgroups(pw->pw_name, pw->pw_gid) != 0 ||
			setgid(pw->pw_gid) != 0 || setuid(pw->pw_uid) != 0) {
			perror("privilege drop failed");
			pam_close_session(pamh, PAM_SILENT);
			pam_setcred(pamh, PAM_DELETE_CRED);
			pam_end(pamh, PAM_SYSTEM_ERR);
			cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
			return 1;
		}

		if (!setup_env(pamh, pw)) {
			perror("environment setup failed");
			pam_close_session(pamh, PAM_SILENT);
			pam_setcred(pamh, PAM_DELETE_CRED);
			pam_end(pamh, PAM_SYSTEM_ERR);
			cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
			return 1;
		}

		session selected_session = (session){ 0 };
		if (sessions != NULL && session_cur >= 0 &&
			session_cur <= (int)session_count)
			selected_session = copy_session(&sessions[session_cur]);
		cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
		start_session(pw, &selected_session);
		free_session(&selected_session);

		pam_close_session(pamh, PAM_SILENT);
		pam_setcred(pamh, PAM_DELETE_CRED);
		pam_end(pamh, PAM_SUCCESS);
	} else {
		printf("Login Failed\n");
		cleanup((image *[]){ &small_img, &large_img }, 2, &saved_term, buf);
	}
	return 0;
}
