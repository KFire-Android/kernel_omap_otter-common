/*
 *  arch/arm/plat-omap/include/mach/led.h
 *
 *  Copyright (C) 2006 Samsung Electronics
 *  Kyungmin Park <kyungmin.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ASMARM_ARCH_LED_H
#define ASMARM_ARCH_LED_H

struct omap_led_config {
	struct led_classdev	cdev;
	s16			gpio;
};

struct omap_led_platform_data {
	s16			nr_leds;
	struct omap_led_config	*leds;
};

extern void omap4430_green_led_set(struct led_classdev *led_cdev,
        enum led_brightness value);
extern void omap4430_orange_led_set(struct led_classdev *led_cdev,
        enum led_brightness value);
extern int omap4430_green_led_set_blink(struct led_classdev *led_cdev, 
        unsigned long *delay_on, unsigned long *delay_off);
extern int omap4430_orange_led_set_blink(struct led_classdev *led_cdev, 
        unsigned long *delay_on, unsigned long *delay_off);

#endif
