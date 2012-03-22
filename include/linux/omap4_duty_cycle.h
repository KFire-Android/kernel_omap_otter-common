/*
 * Thermal Framework Driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Eugen Mandrenko <ievgen.mandrenko@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
*/

#ifndef __OMAP4_DUTY_CYCLE_H__
#define __OMAP4_DUTY_CYCLE_H__

#include <linux/thermal_framework.h>

struct duty_cycle_dev {
	int (*cool_device) (struct thermal_dev *, int temp);
};

int duty_cooling_dev_register(struct duty_cycle_dev *duty_cycle_dev);
void duty_cooling_dev_unregister(void);

#endif /*__OMAP4_DUTY_CYCLE_H__*/

