/*
 * arch/arm/mach-omap2/board-touch-cyttsp4_core.c
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 * Modified by: Cypress Semiconductor - 2011
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input/touch_platform.h>
#include <linux/input.h>
#include <linux/input/cyttsp4_params.h>


/* duplicate the defines here, should move to a header file */
#define GPIO_TOUCH_RESET 23
#define GPIO_TOUCH_IRQ   35


#define TOUCH_GPIO_RST_CYTTSP GPIO_TOUCH_RESET
#define TOUCH_GPIO_IRQ_CYTTSP GPIO_TOUCH_IRQ

static int cyttsp4_hw_reset(void)
{
	int ret = 0;
	return 0;

	gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 1);
	pr_info("%s: gpio_set_value(step%d)=%d\n", __func__, 1, 1);
	msleep(20);
	gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 0);
	pr_info("%s: gpio_set_value(step%d)=%d\n", __func__, 2, 0);
	msleep(40);
	gpio_set_value(TOUCH_GPIO_RST_CYTTSP, 1);
	msleep(20);
	pr_info("%s: gpio_set_value(step%d)=%d\n", __func__, 3, 1);

	return ret;
}

static int cyttsp4_hw_recov(int on)
{
	int retval = 0;

	pr_info("%s: on=%d\n", __func__, on);
	if (on == 0) {
		cyttsp4_hw_reset();
		retval = 0;
	} else
		retval = -EINVAL;
	return 0;
}

static int cyttsp4_irq_stat(void)
{
	return gpio_get_value(TOUCH_GPIO_IRQ_CYTTSP);
}

#define CY_ABS_MIN_X 0
#define CY_ABS_MIN_Y 0
#define CY_ABS_MIN_P 0
#define CY_ABS_MIN_W 0
#define CY_ABS_MIN_T 0 /* //1 */
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MAX_P 255
#define CY_ABS_MAX_W 255
#define CY_ABS_MAX_T 9 /* //10 */
#define CY_IGNORE_VALUE 0xFFFF


static struct touch_settings cyttsp4_sett_param_regs = {
	.data = (uint8_t *)&cyttsp4_param_regs[0],
	.size = sizeof(cyttsp4_param_regs),
	.tag = 0,
};

static struct touch_settings cyttsp4_sett_param_size = {
	.data = (uint8_t *)&cyttsp4_param_size[0],
	.size = sizeof(cyttsp4_param_size),
	.tag = 0,
};

/* Design Data Table */
static u8 cyttsp4_ddata[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24 /* test padding, 25, 26, 27, 28, 29, 30, 31 */
};

static struct touch_settings cyttsp4_sett_ddata = {
	.data = (uint8_t *)&cyttsp4_ddata[0],
	.size = sizeof(cyttsp4_ddata),
	.tag = 0,
};

/* Manufacturing Data Table */
static u8 cyttsp4_mdata[] = {
	65, 64, /* test truncation */63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48,
	47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32,
	31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
};

static struct touch_settings cyttsp4_sett_mdata = {
	.data = (uint8_t *)&cyttsp4_mdata[0],
	.size = sizeof(cyttsp4_mdata),
	.tag = 0,
};



#define CY_USE_INCLUDE_FBL
#ifdef CY_USE_INCLUDE_FBL
#include "linux/input/cyttsp4_img.h"
static struct touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = sizeof(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = sizeof(cyttsp4_ver),
};
#else
static u8 test_img[] = {0, 1, 2, 4, 8, 16, 32, 64, 128};
static u8 test_ver[] = {0, 0, 0, 0, 0x10, 0x20, 0, 0, 0};
static struct touch_firmware cyttsp4_firmware = {
	.img = test_img,
	.size = sizeof(test_img),
	.ver = test_ver,
	.vsize = sizeof(test_ver),
};
#endif

static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	ABS_MT_TOUCH_MAJOR, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
};

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *)&cyttsp4_abs[0],
	.size = sizeof(cyttsp4_abs)/sizeof(uint16_t),
	.enable_vkeys = 1,
};

struct touch_platform_data cyttsp4_i2c_touch_platform_data = {
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		&cyttsp4_sett_param_regs,
		&cyttsp4_sett_param_size,
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		&cyttsp4_sett_ddata,	/* Design Data Record */
		&cyttsp4_sett_mdata,	/* Manufacturing Data Record */
	},
	.fw = &cyttsp4_firmware,
	.frmwrk = &cyttsp4_framework,
	.addr = {CY_I2C_TCH_ADR, CY_I2C_LDR_ADR},
	.flags = /*0x01 | 0x02 | */0x20 | 0x40,
	.hw_reset = cyttsp4_hw_reset,
	.hw_recov = cyttsp4_hw_recov,
	.irq_stat = cyttsp4_irq_stat,
};

