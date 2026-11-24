#include "term_utils.h"
#include "pam.h"
#include "sessions.h"
#include "canvas.h"
#include "info.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>


#define FPS 30

static void delay_frame(const struct timespec *start,
						const struct timespec *stop)
{
	const long frame_ns = 1000000000L / FPS;
	long elapsed_ns =
		(stop->tv_sec - start->tv_sec) * 1000000000L +
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

int main(){

	buffer *buf = &(buffer){ .fbp = NULL };
	if (!create_buffer(buf, "/dev/fb0"))
		return 1;

	texture test = { 0 };
	if (!load_texture(&test, "img/distro_logos/proxmox.ppm")) {
		destroy_buffer(buf);
		return 1;
	}

    float angle = 0;
    float dangle = (2*M_2_PI)/FPS;

	struct timespec start, stop;

    while (1){
        clock_gettime(CLOCK_MONOTONIC, &start);

        draw2drectangle_texture(200, 200, test.width, test.height, &test);
        present();

        clock_gettime(CLOCK_MONOTONIC, &stop);

		delay_frame(&start, &stop);
    }

	free(test.data);
	destroy_buffer(buf);
    return 0;
}