/*
 * Kindle Fire Power Button LED (based OMAP4430 SDP display LED Driver)
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Author: Dan Murphy <DMurphy@ti.com>
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

#ifndef __OTTER_BUTTON_LED__
#define __OTTER_BUTTON_LED__

struct otter_button_led_platform_data {
	char *name;
	void (*led_init)(void);
	void (*led_set_brightness)(u8 value);
};

#endif  /*__OTTER_BUTTON_LED__*/
