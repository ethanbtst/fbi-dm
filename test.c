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

    image test;
    load_image(&test, "img/distro_logos/proxmox.png");


    float angle = 0;
    float dangle = (2*M_PI)/360.0;


    struct timespec start, stop;

    while (1){
        clock_gettime(CLOCK_REALTIME, &start);


        image* rotated = rotate_image(&test, angle += dangle);
        draw_image(buf, rotated, 300, 300);
        free(rotated->data);
        free(rotated);

        clock_gettime(CLOCK_REALTIME, &stop);

        
        nanosleep();
    }
    return 0;
}