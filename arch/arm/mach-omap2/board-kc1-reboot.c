/*
 * Board support file for OMAP4430 SDP.
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Author: Deepak Bhople <deepakb@ti.com>
 *
 * Based on mach-omap2/board-3430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>

#include <plat/io.h>
#include <plat/prcm.h>
#include <plat/control.h>

#include "prm-regbits-44xx.h"
#include "prcm-common.h"
#include "clock.h"


// This code is yanked from arch/arm/mach-omap2/prcm.c
void machine_emergency_restart(void)
{
	u32 v = 0;
	// delay here to allow eMMC to finish any internal housekeeping before reset
	// even if mdelay fails to work correctly, 8 second button press should work
	// this used to be an msleep but scheduler is gone here and calling msleep
	// will cause a panic
	mdelay(1600);
	/* reboot: non-supported mode */
	v |= OMAP4430_RST_GLOBAL_COLD_SW_MASK;
	/* clear previous reboot status */
	prm_write_mod_reg(0xfff,
			OMAP4430_PRM_DEVICE_MOD,
			OMAP4_RM_RSTST);
	prm_write_mod_reg(v,
			OMAP4430_PRM_DEVICE_MOD,
			OMAP4_RM_RSTCTRL);
	/* OCP barrier */
	v = prm_read_mod_reg(WKUP_MOD, OMAP4_RM_RSTCTRL);
}
