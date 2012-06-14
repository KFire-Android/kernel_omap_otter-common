/*
 * arch/arm/mach-omap2/board-54xx-sevm-dock.c
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

#include <linux/input.h>
#include <linux/i2c/smsc.h>
#include <linux/input/matrix_keypad.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/sevm_dock_station.h>

#include <plat/i2c.h>
#include <plat/gpio.h>

#include "board-54xx-sevm.h"

#define OMAP5_SEVM_KEYBOARD_IRQ	151
#define DOCK_STATION_GPIO	OMAP_MPUIO(20)

static const uint32_t sevm_dock_keymap[] = {
	KEY(0, 0,  KEY_RESERVED)     , KEY(0, 1,  KEY_RESERVED),
	KEY(0, 2,  KEY_F7)           , KEY(0, 3,  KEY_ESC),
	KEY(0, 4,  KEY_F4)           , KEY(0, 5,  KEY_G),
	KEY(0, 6,  KEY_RESERVED)     , KEY(0, 7,  KEY_H),
	KEY(0, 8,  KEY_RESERVED)     , KEY(0, 9,  KEY_CYCLEWINDOWS),
	KEY(0, 10, KEY_RESERVED)     , KEY(0, 11, KEY_RESERVED),
	KEY(0, 12, KEY_BACKSPACE)    , KEY(0, 13, KEY_F11),
	KEY(0, 14, KEY_FORWARD)      , KEY(0, 15, KEY_INSERT),

	KEY(1, 0,  KEY_RIGHTSHIFT)   , KEY(1, 1,  KEY_RESERVED),
	KEY(1, 2,  KEY_W)            , KEY(1, 3,  KEY_Q),
	KEY(1, 4,  KEY_E)            , KEY(1, 5,  KEY_R),
	KEY(1, 6,  KEY_RESERVED)     , KEY(1, 7,  KEY_U),
	KEY(1, 8,  KEY_I)	     , KEY(1, 9,  KEY_RESERVED),
	KEY(1, 10, KEY_RESERVED)     , KEY(1, 11, KEY_RESERVED),
	KEY(1, 12, KEY_UP)           , KEY(1, 13, KEY_O),
	KEY(1, 14, KEY_P)            , KEY(1, 15, KEY_LEFT),

	KEY(2, 0,  KEY_RESERVED)     , KEY(2, 1,  KEY_RESERVED),
	KEY(2, 2,  KEY_2)            , KEY(2, 3,  KEY_1),
	KEY(2, 4,  KEY_3)            , KEY(2, 5,  KEY_4),
	KEY(2, 6,  KEY_RESERVED)     , KEY(2, 7,  KEY_7),
	KEY(2, 8,  KEY_8)            , KEY(2, 9,  KEY_RESERVED),
	KEY(2, 10, KEY_RESERVED)     , KEY(2, 11, KEY_RIGHTALT),
	KEY(2, 12, KEY_DOWN)         , KEY(2, 13, KEY_9),
	KEY(2, 14, KEY_0)            , KEY(2, 15, KEY_RIGHT),

	KEY(3, 0,  KEY_RESERVED)     , KEY(3, 1,  KEY_RIGHTCTRL),
	KEY(3, 2,  KEY_S)            , KEY(3, 3,  KEY_A),
	KEY(3, 4,  KEY_D)            , KEY(3, 5,  KEY_F),
	KEY(3, 6,  KEY_RESERVED)     , KEY(3, 7,  KEY_J),
	KEY(3, 8,  KEY_K)            , KEY(3, 9,  KEY_RESERVED),
	KEY(3, 10, KEY_RESERVED)     , KEY(3, 11, KEY_RESERVED),
	KEY(3, 12, KEY_ENTER)        , KEY(3, 13, KEY_L),
	KEY(3, 14, KEY_SEMICOLON)    , KEY(3, 15, KEY_RESERVED),
	KEY(4, 0,  KEY_LEFTSHIFT)    , KEY(4, 1,  KEY_RESERVED),
	KEY(4, 2,  KEY_X)            , KEY(4, 3,  KEY_Z),
	KEY(4, 4,  KEY_C)            , KEY(4, 5,  KEY_V),
	KEY(4, 6,  KEY_RESERVED)     , KEY(4, 7,  KEY_M),
	KEY(4, 8,  KEY_COMMA)        , KEY(4, 9,  KEY_RESERVED),
	KEY(4, 10, KEY_RESERVED)     , KEY(4, 11, KEY_RESERVED),
	KEY(4, 12, KEY_SPACE)        , KEY(4, 13, KEY_DOT),
	KEY(4, 14, KEY_SLASH)        , KEY(4, 15, KEY_END),

	KEY(5, 0,  KEY_RESERVED)     , KEY(5, 1,  KEY_LEFTCTRL),
	KEY(5, 2,  KEY_F6)           , KEY(5, 3,  KEY_TAB),
	KEY(5, 4,  KEY_F3)           , KEY(5, 5,  KEY_T),
	KEY(5, 6,  KEY_RESERVED)     , KEY(5, 7,  KEY_Y),
	KEY(5, 8,  KEY_LEFTBRACE)    , KEY(5, 9,  KEY_RESERVED),
	KEY(5, 10, KEY_RESERVED)     , KEY(5, 11, KEY_RESERVED),
	KEY(5, 12, KEY_RESERVED)     , KEY(5, 13, KEY_F10),
	KEY(5, 14, KEY_RIGHTBRACE)   , KEY(5, 15, KEY_HOME),
	KEY(6, 0,  KEY_RESERVED)     , KEY(6, 1,  KEY_RESERVED),
	KEY(6, 2,  KEY_F5)           , KEY(6, 3,  KEY_RESERVED),
	KEY(6, 4,  KEY_F2)           , KEY(6, 5,  KEY_5),
	KEY(6, 6,  KEY_FN)           , KEY(6, 7,  KEY_6),
	KEY(6, 8,  KEY_RESERVED)     , KEY(6, 9,  KEY_RESERVED),
	KEY(6, 10, KEY_MENU)         , KEY(6, 11, KEY_RESERVED),
	KEY(6, 12, KEY_BACKSLASH)    , KEY(6, 13, KEY_F9),
	KEY(6, 14, KEY_RESERVED)     , KEY(6, 15, KEY_RESERVED),

	KEY(7, 0,  KEY_RESERVED)     , KEY(7, 1,  KEY_RESERVED),
	KEY(7, 2,  KEY_F8)           , KEY(7, 3,  KEY_CAPSLOCK),
	KEY(7, 4,  KEY_F1)           , KEY(7, 5,  KEY_B),
	KEY(7, 6,  KEY_RESERVED)     , KEY(7, 7,  KEY_N),
	KEY(7, 8,  KEY_RESERVED)     , KEY(7, 9,  KEY_RESERVED),
	KEY(7, 10, KEY_RESERVED)     , KEY(7, 11, KEY_LEFTALT),
	KEY(7, 12, KEY_RESERVED)     , KEY(7, 13, KEY_F12),
	KEY(7, 14, KEY_RESERVED)     , KEY(7, 15, KEY_DELETE),
};

static struct matrix_keymap_data sevm_dock_keypad_data = {
	.keymap                 = sevm_dock_keymap,
	.keymap_size            = ARRAY_SIZE(sevm_dock_keymap),
};

static struct smsc_keypad_data sevm_dock_kp_data = {
	.keymap_data    = &sevm_dock_keypad_data,
	.rows           = 8,
	.cols           = 16,
	.rep            = 0,
};

static struct i2c_board_info __initdata sevm_i2c_5_dockinfo[] = {
	{
		I2C_BOARD_INFO("smsc", 0x38),
		.platform_data = &sevm_dock_kp_data,
		.irq = OMAP5_SEVM_KEYBOARD_IRQ,
	},
};

static struct sevm_dockstation_platform_data dockstation_data = {
	.gpio		= DOCK_STATION_GPIO,
	.irqflags	= (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING),
};

static struct platform_device sevm_dockstation = {
	.name = DOCK_STATION_NAME,
	.dev = {
		.platform_data = &dockstation_data,
	},
};

int __init sevm_dock_init(void)
{
	i2c_register_board_info(5, sevm_i2c_5_dockinfo,
		ARRAY_SIZE(sevm_i2c_5_dockinfo));

	platform_device_register(&sevm_dockstation);

	return 0;
}

