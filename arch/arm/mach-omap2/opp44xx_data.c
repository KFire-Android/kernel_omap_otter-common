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
#include <linux/delay.h>

#include <plat/opp.h>
#include <plat/clock.h>
#include <plat/omap_device.h>

#include "cm-regbits-34xx.h"
#include "prm.h"
#include "opp44xx.h"

static struct clk *dpll_mpu_clk, *iva_clk, *dsp_clk, *l3_clk, *core_m2_clk;
static struct clk *core_m3_clk, *core_m6_clk, *core_m7_clk;
static struct clk *per_m3_clk, *per_m6_clk;
static struct clk *abe_clk, *sgx_clk, *fdif_clk, *hsi_clk;

/*
 * Separate OPP table is needed for pre ES2.1 chips as emif cannot be scaled.
 * This table needs to be maintained only temporarily till everybody
 * migrates to ES2.1
 */
static struct omap_opp_def __initdata omap44xx_pre_es2_1_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OMAP_OPP_DEF("mpu", true, 300000000, 930000),
	/* MPU OPP2 - OPP100 */
	OMAP_OPP_DEF("mpu", true, 600000000, 1100000),
	/* MPU OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("mpu", true, 800000000, 1260000),
	/* MPU OPP4 - OPP-SB */
	OMAP_OPP_DEF("mpu", true, 1008000000, 1350000),
	/* IVA OPP1 - OPP50_98 */
	OMAP_OPP_DEF("iva", true,  133000000, 928000),
	/* IVA OPP1 - OPP50 */
	OMAP_OPP_DEF("iva", true,  133000000, 930000),
	/* IVA OPP2 - OPP100 */
	OMAP_OPP_DEF("iva", true,  266000000, 1100000),
	/* IVA OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("iva", false, 332000000, 1260000),
	/* DSP OPP1 - OPP50_98 */
	OMAP_OPP_DEF("dsp", true, 232800000, 928000),
	/* DSP OPP1 - OPP50 */
	OMAP_OPP_DEF("dsp", true, 232800000, 930000),
	/* DSP OPP2 - OPP100 */
	OMAP_OPP_DEF("dsp", true, 465600000, 1100000),
	/* DSP OPP3 - OPPTB */
	OMAP_OPP_DEF("dsp", false, 498000000, 1260000),
	/* ABE OPP - OPP50_98 */
	OMAP_OPP_DEF("omap-aess-audio", true, 49152000, 928000),
	/* ABE OPP1 - OPP50 */
	OMAP_OPP_DEF("omap-aess-audio", true, 98304000, 930000),
	/* ABE OPP2 - OPP100 */
	OMAP_OPP_DEF("omap-aess-audio", true, 196608000, 1100000),
	/* ABE OPP3 - OPPTB */
	OMAP_OPP_DEF("omap-aess-audio", false, 196608000, 1260000),
	/* L3 OPP1 - OPP50 */
	OMAP_OPP_DEF("l3_main_1", true, 100000000, 930000),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OMAP_OPP_DEF("l3_main_1", true, 200000000, 1100000),
	/* CAM FDIF OPP1 - OPP50 */
	OMAP_OPP_DEF("fdif", true, 64000000, 930000),
	/* CAM FDIF OPP2 - OPP100 */
	OMAP_OPP_DEF("fdif", true, 128000000, 1100000),
	/* SGX OPP1 - OPP50 */
	OMAP_OPP_DEF("gpu", true, 153600000, 930000),
	/* SGX OPP2 - OPP100 */
	OMAP_OPP_DEF("gpu", true, 307200000, 1100000),
	/* HSI OPP1 - OPP50 */
	OMAP_OPP_DEF("hsi", true, 96000000, 930000),
	/* HSI OPP2 - OPP100 */
	OMAP_OPP_DEF("hsi", true, 96000000, 1100000),
};

static struct omap_opp_def __initdata omap44xx_opp_def_list[] = {
	/* MPU OPPLP - DPLL cascading */
	OMAP_OPP_DEF("mpu", false, 196608000, 1005000),
	/* MPU OPP1 - OPP50 */
	OMAP_OPP_DEF("mpu", true, 300000000, 1025000),
	/* MPU OPP2 - OPP100 */
	OMAP_OPP_DEF("mpu", true, 600000000, 1200000),
	/* MPU OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("mpu", true, 800000000, 1313000),
	/* MPU OPP4 - OPP-SB */
	OMAP_OPP_DEF("mpu", true, 1008000000, 1375000),

	/* IVA OPPLP - DPLL cascading */
	OMAP_OPP_DEF("iva", true,  98304000, 1011000),
	/* IVA OPP1 - OPP50 */
	OMAP_OPP_DEF("iva", true,  133000000, 1013000),
	/* IVA OPP2 - OPP100 */
	OMAP_OPP_DEF("iva", true,  266000000, 1188000),
	/* IVA OPP3 - OPP-Turbo */
	OMAP_OPP_DEF("iva", false, 332000000, 1300000),

	/* DSP OPPLP - DPLL cascading */
	OMAP_OPP_DEF("dsp", false, false, 1011000),
	/* DSP OPP1 - OPP50 */
	OMAP_OPP_DEF("dsp", true, 232800000, 1013000),
	/* DSP OPP2 - OPP100 */
	OMAP_OPP_DEF("dsp", true, 465600000, 1188000),
	/* DSP OPP3 - OPPTB */
	OMAP_OPP_DEF("dsp", false, 498000000, 1300000),

	/* ABE OPP - OPP50_98 */
	OMAP_OPP_DEF("omap-aess-audio", false, 98304000, 1011000),
	/* ABE OPP1 - OPP50 */
	OMAP_OPP_DEF("omap-aess-audio", true, 98304000, 1013000),
	/* ABE OPP2 - OPP100 */
	OMAP_OPP_DEF("omap-aess-audio", true, 196608000, 1188000),
	/* ABE OPP3 - OPPTB */
	OMAP_OPP_DEF("omap-aess-audio", false, 196608000, 1300000),

	/* L3 OPPLP - DPLL cascading */
	OMAP_OPP_DEF("l3_main_1", false, 98304000, 1005000),
	/* L3 OPP1 - OPP50 */
	OMAP_OPP_DEF("l3_main_1", true, 100000000, 1025000),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OMAP_OPP_DEF("l3_main_1", true, 200000000, 1200000),

	/* EMIF1 OPPLP - DPLL cascading */
	OMAP_OPP_DEF("emif1", false, 196608000, 1005000),
	/*EMIF1 OPP1 - OPP50 */
	OMAP_OPP_DEF("emif1", true, 400000000, 1025000),
	/*EMIF1 OPP2 - OPP100 */
	OMAP_OPP_DEF("emif1", true, 800000000, 1200000),

	/* EMIF2 OPPLP - DPLL cascading */
	OMAP_OPP_DEF("emif2", false, 196608000, 1005000),
	/*EMIF2 OPP1 - OPP50 */
	OMAP_OPP_DEF("emif2", true, 400000000, 1025000),
	/*EMIF2 OPP2 - OPP100 */
	OMAP_OPP_DEF("emif2", true, 800000000, 1200000),

	/* CAM FDIF OPPLP - DPLL cascading */
	OMAP_OPP_DEF("fdif", false, 49152000, 1005000),
	/* CAM FDIF OPP1 - OPP50 */
	OMAP_OPP_DEF("fdif", true, 64000000, 1025000),
	/* CAM FDIF OPP2 - OPP100 */
	OMAP_OPP_DEF("fdif", true, 128000000, 1200000),

	/* SGX OPPLP - DPLL cascading */
	OMAP_OPP_DEF("gpu", false, 196608000, 1005000),
	/* SGX OPP1 - OPP50 */
	OMAP_OPP_DEF("gpu", true, 153600000, 1025000),
	/* SGX OPP2 - OPP100 */
	OMAP_OPP_DEF("gpu", true, 307200000, 1200000),

	/* HSI OPPLP - DPLL cascading */
	OMAP_OPP_DEF("hsi", false, 98304000, 1005000),
	/* HSI OPP1 - OPP50 */
	OMAP_OPP_DEF("hsi", true, 96000000, 1025000),
	/* HSI OPP2 - OPP100 */
	OMAP_OPP_DEF("hsi", true, 96000000, 1200000),
};

#define	L3_OPP50_RATE			100000000
#define DPLL_CORE_M3_OPP50_RATE		200000000
#define DPLL_CORE_M3_OPP100_RATE	320000000
#define DPLL_CORE_M6_OPP50_RATE		200000000
#define DPLL_CORE_M6_OPP100_RATE	266600000
#define DPLL_CORE_M7_OPP50_RATE		133333333
#define DPLL_CORE_M7_OPP100_RATE	266666666
#define DPLL_PER_M3_OPP50_RATE		192000000
#define DPLL_PER_M3_OPP100_RATE		256000000
#define DPLL_PER_M6_OPP50_RATE		192000000
#define DPLL_PER_M6_OPP100_RATE		384000000

static u32 omap44xx_opp_def_size = ARRAY_SIZE(omap44xx_opp_def_list);

static unsigned long omap4_mpu_get_rate(struct device *dev);

#ifndef CONFIG_CPU_FREQ
static unsigned long compute_lpj(unsigned long ref, u_int div, u_int mult)
{
	unsigned long new_jiffy_l, new_jiffy_h;

	/*
	 * Recalculate loops_per_jiffy.  We do it this way to
	 * avoid math overflow on 32-bit machines.  Maybe we
	 * should make this architecture dependent?  If you have
	 * a better way of doing this, please replace!
	 *
	 *    new = old * mult / div
	 */
	new_jiffy_h = ref / div;
	new_jiffy_l = (ref % div) / 100;
	new_jiffy_h *= mult;
	new_jiffy_l = new_jiffy_l * mult / div;

	return new_jiffy_h + new_jiffy_l * 100;
}
#endif

static int omap4_mpu_set_rate(struct device *dev, unsigned long rate)
{
	int ret;

	ret = clk_set_rate(dpll_mpu_clk, rate);
	if (ret) {
		dev_warn(dev, "%s: Unable to set rate to %ld\n",
			__func__, rate);
		return ret;
	}

	return 0;
}

static unsigned long omap4_mpu_get_rate(struct device *dev)
{
	return dpll_mpu_clk->rate;
}

static int omap4_iva_set_rate(struct device *dev, unsigned long rate)
{
	if (dev == omap2_get_iva_device()) {
		unsigned long round_rate;
		/*
		 * Round rate is required as the actual iva clock rate is
		 * weird for OMAP4
		 */
		round_rate = clk_round_rate(iva_clk, rate);
		return clk_set_rate(iva_clk, round_rate);
	} else if (dev == omap4_get_dsp_device()) {
		return clk_set_rate(dsp_clk, rate);
	} else {
		dev_warn(dev, "%s: Wrong device pointer\n", __func__);
		return -EINVAL;
	}

}

static unsigned long omap4_iva_get_rate(struct device *dev)
{
	if (dev == omap2_get_iva_device()) {
		return iva_clk->rate ;
	} else if (dev == omap4_get_dsp_device()) {
		return dsp_clk->rate ;
	} else {
		dev_warn(dev, "%s: Wrong device pointer\n", __func__);
		return -EINVAL;
	}
}

static int omap4_l3_set_rate(struct device *dev, unsigned long rate)
{
	u32 d_core_m3_rate, d_core_m6_rate, d_core_m7_rate;
	u32 d_per_m3_rate, d_per_m6_rate;

	if (rate <= L3_OPP50_RATE) {
		d_core_m3_rate = DPLL_CORE_M3_OPP50_RATE;
		d_core_m6_rate = DPLL_CORE_M6_OPP50_RATE;
		d_core_m7_rate = DPLL_CORE_M7_OPP50_RATE;
		d_per_m3_rate = DPLL_PER_M3_OPP50_RATE;
		d_per_m6_rate = DPLL_PER_M6_OPP50_RATE;
	} else {
		d_core_m3_rate = DPLL_CORE_M3_OPP100_RATE;
		d_core_m6_rate = DPLL_CORE_M6_OPP100_RATE;
		d_core_m7_rate = DPLL_CORE_M7_OPP100_RATE;
		d_per_m3_rate = DPLL_PER_M3_OPP100_RATE;
		d_per_m6_rate = DPLL_PER_M6_OPP100_RATE;
	}

	clk_set_rate(core_m3_clk, d_core_m3_rate);
	d_core_m6_rate = clk_round_rate(core_m6_clk, d_core_m6_rate);
	clk_set_rate(core_m6_clk, d_core_m6_rate);
	clk_set_rate(core_m7_clk, d_core_m7_rate);
	clk_set_rate(per_m3_clk, d_per_m3_rate);
	clk_set_rate(per_m6_clk, d_per_m6_rate);
	return clk_set_rate(l3_clk, rate * 2);
}

static unsigned long omap4_l3_get_rate(struct device *dev)
{
	return l3_clk->rate / 2;
}

static int omap4_emif_set_rate(struct device *dev, unsigned long rate)
{
	return clk_set_rate(core_m2_clk, rate);
}

static unsigned long omap4_emif_get_rate(struct device *dev)
{
	return core_m2_clk->rate;
}

static int omap4_abe_set_rate(struct device *dev, unsigned long rate)
{
	unsigned long round_rate;

	round_rate = clk_round_rate(abe_clk, rate);

	return clk_set_rate(abe_clk, round_rate);
}

static unsigned long omap4_abe_get_rate(struct device *dev)
{
	return abe_clk->rate ;
}

static int omap4_sgx_set_rate(struct device *dev, unsigned long rate)
{
	return clk_set_rate(sgx_clk, rate);
}

static unsigned long omap4_sgx_get_rate(struct device *dev)
{
	return sgx_clk->rate ;
}

static int omap4_fdif_set_rate(struct device *dev, unsigned long rate)
{
	return clk_set_rate(fdif_clk, rate);
}

static unsigned long omap4_fdif_get_rate(struct device *dev)
{
	return fdif_clk->rate ;
}

static int omap4_hsi_set_rate(struct device *dev, unsigned long rate)
{
	return clk_set_rate(hsi_clk, rate);
}

static unsigned long omap4_hsi_get_rate(struct device *dev)
{
	return hsi_clk->rate ;
}

struct device *find_dev_ptr(char *name)
{

	struct omap_hwmod *oh;
	struct device *dev = NULL;

	oh = omap_hwmod_lookup(name);
	if (oh && oh->od)
		dev = &oh->od->pdev.dev;

	return dev;
}

/* Temp variable to allow multiple calls */
static u8 __initdata omap4_table_init;


int __init omap4_pm_init_opp_table(void)
{
	struct omap_opp_def *opp_def;
	struct device *dev;
	struct clk *gpu_fclk;
	int i, r;

	/*
	 * Allow multiple calls, but initialize only if not already initalized
	 * even if the previous call failed, coz, no reason we'd succeed again
	 */
	if (omap4_table_init)
		return 0;
	omap4_table_init = 1;

	if (omap_rev() <= OMAP4430_REV_ES2_0)
		opp_def = omap44xx_pre_es2_1_opp_def_list;
	else
		opp_def = omap44xx_opp_def_list;

	for (i = 0; i < omap44xx_opp_def_size; i++) {
		r = opp_add(opp_def);
		if (r)
			pr_err("unable to add OPP %ld Hz for %s\n",
				opp_def->freq, opp_def->hwmod_name);
		opp_def++;
	}

	dpll_mpu_clk = clk_get(NULL, "dpll_mpu_ck");
	iva_clk = clk_get(NULL, "dpll_iva_m5x2_ck");
	dsp_clk = clk_get(NULL, "dpll_iva_m4x2_ck");
	l3_clk = clk_get(NULL, "dpll_core_m5x2_ck");
	core_m2_clk = clk_get(NULL, "dpll_core_m2_ck");
	core_m3_clk = clk_get(NULL, "dpll_core_m3x2_ck");
	core_m6_clk = clk_get(NULL, "dpll_core_m6x2_ck");
	core_m7_clk = clk_get(NULL, "dpll_core_m7x2_ck");
	sgx_clk = clk_get(NULL, "dpll_per_m7x2_ck");
	gpu_fclk = clk_get(NULL, "gpu_fck");
	per_m3_clk = clk_get(NULL, "dpll_per_m3x2_ck");
	per_m6_clk = clk_get(NULL, "dpll_per_m6x2_ck");
	abe_clk = clk_get(NULL, "abe_clk");
	fdif_clk = clk_get(NULL, "fdif_fck");
	hsi_clk = clk_get(NULL, "hsi_fck");

	/* Set SGX parent to PER DPLL */
	clk_set_parent(gpu_fclk, sgx_clk);
	clk_put(gpu_fclk);

	/* Populate the set rate and get rate for mpu, iva, dsp and l3 device */
	dev = omap2_get_mpuss_device();
	if (dev)
		opp_populate_rate_fns(dev, omap4_mpu_set_rate,
				omap4_mpu_get_rate);

	dev = omap2_get_iva_device();
	if (dev)
		opp_populate_rate_fns(dev, omap4_iva_set_rate,
				omap4_iva_get_rate);

	dev = omap4_get_dsp_device();
	if (dev)
		opp_populate_rate_fns(dev, omap4_iva_set_rate,
				omap4_iva_get_rate);

	dev = omap2_get_l3_device();
	if (dev)
		opp_populate_rate_fns(dev, omap4_l3_set_rate,
				omap4_l3_get_rate);

	/*
	 * This is a temporary hack since emif clocks cannot be scaled
	 * on ES1.0 and ES2.0. Once everybody has migrated to ES2.1 this
	 * check can be remove.
	 */
	if (omap_rev() > OMAP4430_REV_ES2_0) {
		dev = find_dev_ptr("emif1");
		if (dev)
			opp_populate_rate_fns(dev, omap4_emif_set_rate,
					omap4_emif_get_rate);

		dev = find_dev_ptr("emif2");
		if (dev)
			opp_populate_rate_fns(dev, omap4_emif_set_rate,
					omap4_emif_get_rate);
	}

	dev = find_dev_ptr("omap-aess-audio");
	if (dev)
		opp_populate_rate_fns(dev, omap4_abe_set_rate,
				omap4_abe_get_rate);

	dev = find_dev_ptr("gpu");
	if (dev)
		opp_populate_rate_fns(dev, omap4_sgx_set_rate,
				omap4_sgx_get_rate);

	dev = find_dev_ptr("fdif");
	if (dev)
		opp_populate_rate_fns(dev, omap4_fdif_set_rate,
				omap4_fdif_get_rate);

	dev = find_dev_ptr("hsi");
	if (dev)
		opp_populate_rate_fns(dev, omap4_hsi_set_rate,
				omap4_hsi_get_rate);

	return 0;
}
