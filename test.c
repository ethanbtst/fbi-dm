#include "term_utils.h"
#include "pam.h"
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
#include <stdlib.h>
#include <pthread.h>
#include <math.h>


#define FPS 60

int main(){

	buffer *buf = &(buffer){ .fbp = NULL };
	if (!create_buffer(buf, "/dev/fb0"))
		return 1;

	image test = { 0 };
	if (!load_image(&test, "img/distro_logos/proxmox.png")) {
		destroy_buffer(buf);
		return 1;
	}

    float angle = 0;
    float dangle = (2*M_PI)/360.0;

	const long frame_ns = 1000000000L / FPS;
	struct timespec start, stop;

    while (1){
        clock_gettime(CLOCK_MONOTONIC, &start);

        image* rotated = rotate_image(&test, angle += dangle);
		if (rotated == NULL)
			break;
        draw_image(buf, rotated, 300, 300);
        free(rotated->data);
        free(rotated);

        clock_gettime(CLOCK_MONOTONIC, &stop);

		long elapsed_ns =
			(stop.tv_sec - start.tv_sec) * 1000000000L +
			stop.tv_nsec - start.tv_nsec;
		long remaining_ns = frame_ns - elapsed_ns;
		if (remaining_ns > 0) {
			struct timespec delay = {
				.tv_sec = remaining_ns / 1000000000L,
				.tv_nsec = remaining_ns % 1000000000L,
			};
			nanosleep(&delay, NULL);
		}
    }

	free(test.data);
	destroy_buffer(buf);
    return 0;
}