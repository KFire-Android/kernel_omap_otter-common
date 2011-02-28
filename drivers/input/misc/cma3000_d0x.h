/*
 * cma3000_d0x.h
 * VTI CMA3000_D0x Accelerometer driver
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Hemanth V <hemanthv@ti.com>
 * Contributor: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef INPUT_CMA3000_H
#define INPUT_CMA3000_H

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>

#include <linux/i2c/cma3000.h>

struct cma3000_accl_data {
#ifdef CONFIG_INPUT_CMA3000_I2C
	struct i2c_client *client;
#endif
	struct input_dev *input_dev;
	struct cma3000_platform_data pdata;

	struct delayed_work input_work;

	/* mutex for sysfs operations */
	struct mutex mutex;
	int bit_to_mg;
	int req_poll_rate;
};

int cma3000_set(struct cma3000_accl_data *, u8, u8, char *);
int cma3000_read(struct cma3000_accl_data *, u8, char *);
int cma3000_init(struct cma3000_accl_data *);
int cma3000_exit(struct cma3000_accl_data *);
int cma3000_poweron(struct cma3000_accl_data *);
int cma3000_poweroff(struct cma3000_accl_data *);

#endif
