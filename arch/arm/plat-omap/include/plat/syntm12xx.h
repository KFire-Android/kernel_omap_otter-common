/*
 * Platform configuration for Synaptic TM12XX touchscreen driver.
 */

#ifndef __TM12XX_TS_H
#define __TM12XX_TS_H

#define SYNTM12XX_SLEEP_ON_SUSPEND	0
#define SYNTM12XX_ON_ON_SUSPEND	1

struct tm12xx_ts_platform_data {
	int      gpio_intr;

	char     **idev_name;  /* Input Device name. */
	u8       *button_map;  /* Button to keycode  */
	unsigned num_buttons;  /* Registered buttons */
	u8       repeat;       /* Input dev Repeat enable */
	u8       swap_xy;      /* ControllerX==InputDevY...*/
	u8	controller_num; /* Number of the controller */
	int	suspend_state; /* Indicate IC state during suspend */
};

#endif
