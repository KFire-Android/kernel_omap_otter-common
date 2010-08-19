/*
 * OMAP3 resource init/change_level/validate_level functions
 *
 * Copyright (C) 2009 - 2010 Texas Instruments Incorporated.
 *	Nishanth Menon
 * Copyright (C) 2009 - 2010 Deep Root Systems, LLC.
 *	Kevin Hilman
 * Copyright (C) 2010 Nokia Corporation.
 *      Eduardo Valentin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * History:
 *
 */

#include <linux/module.h>
#include <linux/err.h>

#include <plat/opp.h>
#include <plat/cpu.h>
#include "omap3-opp.h"

static struct omap_opp_def __initdata omap34xx_opp_def_list[] = {
	/* MPU OPP1 */
	OMAP_OPP_DEF("mpu", true, 125000000, 975000),
	/* MPU OPP2 */
	OMAP_OPP_DEF("mpu", true, 250000000, 1075000),
	/* MPU OPP3 */
	OMAP_OPP_DEF("mpu", true, 500000000, 1200000),
	/* MPU OPP4 */
	OMAP_OPP_DEF("mpu", true, 550000000, 1270000),
	/* MPU OPP5 */
	OMAP_OPP_DEF("mpu", true, 600000000, 1350000),
	/*
	 * L3 OPP1 - 41.5 MHz is disabled because: The voltage for that OPP is
	 * almost the same than the one at 83MHz thus providing very little
	 * gain for the power point of view. In term of energy it will even
	 * increase the consumption due to the very negative performance
	 * impact that frequency will do to the MPU and the whole system in
	 * general.
	 */
	OMAP_OPP_DEF("l3_main", false, 41500000, 975000),
	/* L3 OPP2 */
	OMAP_OPP_DEF("l3_main", true, 83000000, 1050000),
	/* L3 OPP3 */
	OMAP_OPP_DEF("l3_main", true, 166000000, 1150000),


	/* DSP OPP1 */
	OMAP_OPP_DEF("iva", true, 90000000, 975000),
	/* DSP OPP2 */
	OMAP_OPP_DEF("iva", true, 180000000, 1075000),
	/* DSP OPP3 */
	OMAP_OPP_DEF("iva", true, 360000000, 1200000),
	/* DSP OPP4 */
	OMAP_OPP_DEF("iva", true, 400000000, 1270000),
	/* DSP OPP5 */
	OMAP_OPP_DEF("iva", true, 430000000, 1350000),
};
static u32 omap34xx_opp_def_size = ARRAY_SIZE(omap34xx_opp_def_list);

static struct omap_opp_def __initdata omap36xx_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OMAP_OPP_DEF("mpu", true,  300000000, 930000),
	/* MPU OPP2 - OPP100 */
	OMAP_OPP_DEF("mpu", true,  600000000, 1100000),
	/* MPU OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("mpu", false, 800000000, 1260000),
	/* MPU OPP4 - OPP-SB */
	OMAP_OPP_DEF("mpu", false, 1000000000, 1350000),

	/* L3 OPP1 - OPP50 */
	OMAP_OPP_DEF("l3_main", true, 100000000, 930000),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OMAP_OPP_DEF("l3_main", true, 200000000, 1137500),

	/* DSP OPP1 - OPP50 */
	OMAP_OPP_DEF("iva", true,  260000000, 930000),
	/* DSP OPP2 - OPP100 */
	OMAP_OPP_DEF("iva", true,  520000000, 1100000),
	/* DSP OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("iva", false, 660000000, 1260000),
	/* DSP OPP4 - OPP-SB */
	OMAP_OPP_DEF("iva", false, 800000000, 1350000),
};
static u32 omap36xx_opp_def_size = ARRAY_SIZE(omap36xx_opp_def_list);

/* Temp variable to allow multiple calls */
static u8 __initdata omap3_table_init;

int __init omap3_pm_init_opp_table(void)
{
	struct omap_opp_def *opp_def, *omap3_opp_def_list;
	u32 omap3_opp_def_size;
	int i, r;

	/*
	 * Allow multiple calls, but initialize only if not already initalized
	 * even if the previous call failed, coz, no reason we'd succeed again
	 */
	if (omap3_table_init)
		return 0;
	omap3_table_init = 1;

	omap3_opp_def_list = cpu_is_omap3630() ? omap36xx_opp_def_list :
			omap34xx_opp_def_list;
	omap3_opp_def_size = cpu_is_omap3630() ? omap36xx_opp_def_size :
			omap34xx_opp_def_size;

	opp_def = omap3_opp_def_list;
	for (i = 0; i < omap3_opp_def_size; i++) {
		r = opp_add(opp_def++);
		if (r)
			pr_err("unable to add OPP %ld Hz for %s\n",
				opp_def->freq, opp_def->hwmod_name);
	}
	return 0;
}
