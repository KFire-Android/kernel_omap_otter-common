/*
 * Samsung lg4591 panel support
 *
 * Copyright 2011 Texas Instruments, Inc.
 * Author: Archit Taneja <archit@ti.com>
 *
 * based on d2l panel driver by Jerry Alexander <x0135174@ti.com>
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

#ifndef __LINUX_PLATFORM_DATA_PANEL_LG4591_H
#define __LINUX_PLATFORM_DATA_PANEL_LF4591_H

struct lg4591_cooling_actions {
	int priority;
	int percentage;
};

struct panel_lg4591_data {
	int	reset_gpio;
	void	(*set_power)(bool enable);
	int	number_actions;
	struct	lg4591_cooling_actions cooling_actions[];
};

#endif
