/*
 *  arch/arm/plat-omap/include/plat/board-backlight.h
 *
 */

#ifndef _BOARD_BACKLIGHT_H
#define _BOARD_BACKLIGHT_H

struct bowser_backlight_platform_data {
	int gpio_en_o2m;
	int gpio_vol_o2m;     /* o2m voltage gpio, could be negative which means no need to do it */
	int gpio_en_lcd;
	int gpio_cabc_en;
	int timer;            /* GPTimer for backlight */
	unsigned int sysclk;  /* input frequency to the timer, 38.4M */
	unsigned int pwmfreq; /* output freqeuncy from timer, 10k */
	int totalsteps;       /* how many backlight steps for the user, also the max brightness */
	int initialstep;      /* initial brightness */
};


#endif
