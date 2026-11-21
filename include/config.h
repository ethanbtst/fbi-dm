#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

extern char opt_small_pic[255];
extern char opt_big_pic[255];
extern bool opt_hostname;
extern bool opt_kernel;
extern char opt_top_text[255];
extern char opt_date[255];
extern char opt_time[255];
extern bool opt_battery;
extern bool opt_brightness;

void default_config(void);
void set_distro_logo(void);
void get_config(void);

#endif
