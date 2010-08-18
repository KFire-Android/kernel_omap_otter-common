/*
 * OMAP4 OPP table definitions.
 *
 * Copyright (C) 2009 - 2010 Texas Instruments Incorporated.
 *	Nishanth Menon
 * Copyright (C) 2009 - 2010 Deep Root Systems, LLC.
 *	Kevin Hilman
 * Copyright (C) 2010 Nokia Corporation.
 *      Eduardo Valentin
 * Copyright (C) 2010 Texas Instruments Incorporated.
 *	Thara Gopinath
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * History:
 */

#include <linux/module.h>
#include <linux/err.h>

#include <plat/opp.h>
#include <plat/cpu.h>

#include "opp44xx.h"

static struct omap_opp_def __initdata omap44xx_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OMAP_OPP_DEF("mpu", true, 300000000, 930000),
	/* MPU OPP2 - OPP100 */
	OMAP_OPP_DEF("mpu", true, 600000000, 1100000),
	/* MPU OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("mpu", true, 800000000, 1260000),
	/* MPU OPP4 - OPP-SB */
	OMAP_OPP_DEF("mpu", true, 1008000000, 1350000),
	/* IVA OPP1 - OPP50 */
	OMAP_OPP_DEF("iva", true,  133000000, 930000),
	/* IVA OPP2 - OPP100 */
	OMAP_OPP_DEF("iva", true,  266000000, 1100000),
	/* IVA OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("iva", false, 332000000, 1260000),
	/* L3 OPP1 - OPP50 */
	OMAP_OPP_DEF("l3_main_1", true, 100000000, 930000),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OMAP_OPP_DEF("l3_main_1", true, 200000000, 1100000),
};

static u32 omap44xx_opp_def_size = ARRAY_SIZE(omap44xx_opp_def_list);

/* Temp variable to allow multiple calls */
static u8 __initdata omap4_table_init;


int __init omap4_pm_init_opp_table(void)
{
	struct omap_opp_def *opp_def;
	int i, r;

	/*
	 * Allow multiple calls, but initialize only if not already initalized
	 * even if the previous call failed, coz, no reason we'd succeed again
	 */
	if (omap4_table_init)
		return 0;
	omap4_table_init = 1;

	opp_def = omap44xx_opp_def_list;

	for (i = 0; i < omap44xx_opp_def_size; i++) {
		r = opp_add(opp_def++);
		if (r)
			pr_err("unable to add OPP %ld Hz for %s\n",
				opp_def->freq, opp_def->hwmod_name);
	}
	return 0;
}
