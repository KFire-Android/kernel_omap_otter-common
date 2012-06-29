/*
 * Header file for:
 * Cypress TrueTouch(TM) Standard Product (TTSP) touchscreen drivers.
 * For use with Cypress Gen4 and Solo parts.
 * Supported parts include:
 * CY8CTMA398
 * CY8CTMA884
 *
 * Copyright (C) 2009-2011 Cypress Semiconductor, Inc.
 * Copyright (C) 2011 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <kev@cypress.com>
 *
 */

#ifndef __CYTTSP4_CORE_H__
#define __CYTTSP4_CORE_H__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/err.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* #define CONFIG_TOUCHSCREEN_DEBUG */

#define CY_NUM_RETRY                10 /* max retries for rd/wr ops */

#define CY_I2C_NAME                 "cyttsp4-i2c"
#define CY_DRIVER_VERSION           "Rev4-2M-20"
#define CY_DRIVER_DATE              "2011-08-17"

#define CY_TCH_I2C_ADDR			0x67
#define CY_LDR_I2C_ADDR			0x69

#ifdef CONFIG_TOUCHSCREEN_DEBUG
/* use the following defines for dynamic debug printing */
/*
 * Level 0: Default Level
 * All debug (cyttsp_dbg) prints turned off
 */
#define CY_DBG_LVL_0                    0
/*
 * Level 1:  Used to verify driver and IC are working
 *    Input from IC, output to event queue
 */
#define CY_DBG_LVL_1                    1
/*
 * Level 2:  Used to further verify/debug the IC
 *    Output to IC
 */
#define CY_DBG_LVL_2                    2
/*
 * Level 3:  Used to further verify/debug the driver
 *    Driver internals
 */
#define CY_DBG_LVL_3                    3

#endif

#ifdef CONFIG_TOUCHSCREEN_DEBUG
#define cyttsp4_dbg(ts, l, f, a...) {\
	if (ts->bus_ops->tsdebug >= (l))\
		pr_info(f, ## a);\
}
#else
#define cyttsp4_dbg(ts, l, f, a...)
#endif

struct cyttsp4_bus_ops {
	s32 (*write)(void *handle, u16 subaddr, u8 length, const void *values,
		int i2c_addr, bool use_subaddr);
	s32 (*read)(void *handle, u16 subaddr, u8 length, void *values,
		int i2c_addr, bool use_subaddr);
	struct device *dev;
#ifdef CONFIG_TOUCHSCREEN_DEBUG
	u8 tsdebug;
#endif
};

void *cyttsp4_core_init(struct cyttsp4_bus_ops *bus_ops,
	struct device *dev, int irq, char *name);

void cyttsp4_core_release(void *handle);
#if defined(CONFIG_PM) || defined(CONFIG_HAS_EARLY_SUSPEND)
int cyttsp4_resume(void *handle);
int cyttsp4_suspend(void *handle);
#endif

#endif /* __CYTTSP4_CORE_H__ */
