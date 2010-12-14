/*
 * lv8093.h
 *
 * Copyright (C) 2008-2009 Texas Instruments.
 * Copyright (C) 2009 Hewlett-Packard.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#ifndef LV8093_H
#define LV8093_H

/* i2c slave address = 0xE4 */
#define LV8093_AF_I2C_ADDR			0x72
#define LV8093_NAME 				"lv8093"

/**
 * struct lv8093_platform_data - platform data values and access functions
 * @power_set: Power state access function, zero is off, non-zero is on.
 * @priv_data_set: device private data (pointer) access function
 */
struct lv8093_platform_data {
	int (*power_set)(enum v4l2_power power);
	int (*priv_data_set)(void *);
};

#endif /* End of of LV8093_H */
