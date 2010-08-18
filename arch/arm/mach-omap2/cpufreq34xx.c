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
#include <plat/clock.h>

#include "cm-regbits-34xx.h"
#include "prm.h"
#include "omap3-opp.h"

static int omap3_mpu_set_rate(struct device *dev, unsigned long rate);
static int omap3_iva_set_rate(struct device *dev, unsigned long rate);
static int omap3_l3_set_rate(struct device *dev, unsigned long rate);

static unsigned long omap3_mpu_get_rate(struct device *dev);
static unsigned long omap3_iva_get_rate(struct device *dev);
static unsigned long omap3_l3_get_rate(struct device *dev);

struct clk *dpll1_clk, *dpll2_clk, *dpll3_clk;

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


static int omap3_mpu_set_rate(struct device *dev, unsigned long rate)
{
	unsigned long cur_rate = omap3_mpu_get_rate(dev);
	int ret;

#ifdef CONFIG_CPU_FREQ
	struct cpufreq_freqs freqs_notify;

	freqs_notify.old = cur_rate / 1000;
	freqs_notify.new = rate / 1000;
	freqs_notify.cpu = 0;
	/* Send pre notification to CPUFreq */
	cpufreq_notify_transition(&freqs_notify, CPUFREQ_PRECHANGE);
#endif
	ret = clk_set_rate(dpll1_clk, rate);
	if (ret) {
		dev_warn(dev, "%s: Unable to set rate to %ld\n",
			__func__, rate);
		return ret;
	}

#ifdef CONFIG_CPU_FREQ
	/* Send a post notification to CPUFreq */
	cpufreq_notify_transition(&freqs_notify, CPUFREQ_POSTCHANGE);
#endif

#ifndef CONFIG_CPU_FREQ
	/*Update loops_per_jiffy if processor speed is being changed*/
	loops_per_jiffy = compute_lpj(loops_per_jiffy,
			cur_rate / 1000, rate / 1000);
#endif
	return 0;
}

static unsigned long omap3_mpu_get_rate(struct device *dev)
{
	return dpll1_clk->rate;
}

static int omap3_iva_set_rate(struct device *dev, unsigned long rate)
{
	return clk_set_rate(dpll2_clk, rate);
}

static unsigned long omap3_iva_get_rate(struct device *dev)
{
	return dpll2_clk->rate;
}

static int omap3_l3_set_rate(struct device *dev, unsigned long rate)
{
	int l3_div;

	l3_div = cm_read_mod_reg(CORE_MOD, CM_CLKSEL) &
			OMAP3430_CLKSEL_L3_MASK;

	return clk_set_rate(dpll3_clk, rate * l3_div);
}

static unsigned long omap3_l3_get_rate(struct device *dev)
{
	int l3_div;

	l3_div = cm_read_mod_reg(CORE_MOD, CM_CLKSEL) &
			OMAP3430_CLKSEL_L3_MASK;
	return dpll3_clk->rate / l3_div;
}

/* Temp variable to allow multiple calls */
static u8 __initdata omap3_table_init;

int __init omap3_pm_init_opp_table(void)
{
	struct omap_opp_def *opp_def, *omap3_opp_def_list;
	struct device *dev;
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

	dpll1_clk = clk_get(NULL, "dpll1_ck");
	dpll2_clk = clk_get(NULL, "dpll2_ck");
	dpll3_clk = clk_get(NULL, "dpll3_m2_ck");

	/* Populate the set rate and get rate for mpu, iva and l3 device */
	dev = omap2_get_mpuss_device();
	if (dev)
		opp_populate_rate_fns(dev, omap3_mpu_set_rate,
				omap3_mpu_get_rate);

	dev = omap2_get_iva_device();
	if (dev)
		opp_populate_rate_fns(dev, omap3_iva_set_rate,
				omap3_iva_get_rate);

	dev = omap2_get_l3_device();
	if (dev)
		opp_populate_rate_fns(dev, omap3_l3_set_rate,
				omap3_l3_get_rate);

	return 0;
}
