/*
 * OMAP4430 SDP display LED Driver
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

#ifndef __OMAP4430_DISPLAY_LED__
#define __OMAP4430_DISPLAY_LED__

#define LEDS_CTRL_AS_ONE_DISPLAY 	(1 << 0)
#define LEDS_CTRL_AS_TWO_DISPLAYS 	(1 << 1)

struct omap4430_sdp_disp_led_platform_data {
	int flags;
	void (*display_led_init)(void);
	void (*primary_display_set)(u8 value);
	void (*secondary_display_set)(u8 value);
};

#endif  /*__OMAP4430_DISPLAY_LED__*/
