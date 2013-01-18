/*
 * arch/arm/mach-omap2/board-44xx-blaze.h
 *
 * Copyright (C) 2012 Texas Instruments
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

#ifndef _MACH_OMAP_BOARD_44XX_SDP_H
#define _MACH_OMAP_BOARD_44XX_SDP_H

#include "board-omap4plus-common.h"

int __init blaze_touch_init(void);
int __init blaze_sensor_init(void);
int __init blaze_keypad_init(void);

#define BLAZE_MDM_PWR_EN_GPIO	157

#endif
