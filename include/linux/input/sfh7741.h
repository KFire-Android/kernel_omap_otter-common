/*
 * sfh7741.h
 * SFH7741 Proximity sensor driver
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Shubhrajyoti D <shubhrajyoti@ti.com>
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

#ifndef __SFH7741_H
#define __SFH7741_H

#define SFH7741_NAME	"sfh7741"

#define SFH7741_WAKEABLE_INT  (1 << 0)
#define SFH7741_DEFAULT_ON    (1 << 1)

/*
 * struct sfh7741_platform_data - SFH7741 Platform data
 * @irq: IRQ assigned
 * @prox_enable: State of the sensor
 * @activate_func: function called to activate/deactivate the sensor
 * @read_prox: function to read the sensor output
 */
struct sfh7741_platform_data {
	int flags;
	int irq;
	int irq_flags;
	int prox_enable;
	void (*activate_func)(int state);
	int (*read_prox)(void);
};

#endif

