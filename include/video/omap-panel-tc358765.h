/*
 * Header for DSI-to-LVDS bridge driver
 *
 * Copyright (C) 2012 Texas Instruments Inc
 * Author: Sergii Kibrik <sergiikibrik@ti.com>
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

#ifndef __VIDEO_TC358765_BOARD_DATA_H__
#define __VIDEO_TC358765_BOARD_DATA_H__

/**
 * struct tc358765_board_data - represent DSI-to-LVDS bridge configuration
 * @lp_time: Timing Generation Counter
 * @clrsipo: CLRSIPO counter (one value for all lanes)
 * @lv_is: charge pump control pin
 * @lv_nd: Feed Back Divider Ratio
 * @pclkdiv: PCLK Divide Option
 * @pclksel: PCLK Selection: HSRCK/HbyteHSClkx2/ByteHsClk
 * @vsdelay: VSYNC Delay
 * @lvdlink: is single or dual link
 * @vtgen: drive video timing signals by the on-chip Video Timing Gen module
 * @msf: enable/disable Magic Square
 * @evtmode: event/pulse mode of video timing information transmission
*/
struct tc358765_board_data {
	u16	lp_time;
	u8	clrsipo;
	u8	lv_is;
	u8	lv_nd;
	u8	pclkdiv;
	u8	pclksel;
	u16	vsdelay;
	bool	lvdlink;
	bool	vtgen;
	bool	msf;
	bool	evtmode;
};

#endif
