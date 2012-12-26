/*
 * OMAP4/OMAP5 custom common clock function
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Vishwanath BS (vishwanath.bs@ti.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/clk.h>
#include <linux/err.h>

#include <plat/hardware.h>
#include <plat/clkdev_omap.h>

#include "clock.h"
#include "clock44xx.h"
#include "clock54xx.h"

#define OMAP4_L3_OPP50_RATE	100000000
#define OMAP4470_L3_OPP50_RATE	116666666
#define OMAP4470_LP_OPP_SET_CORE_DPLL_FREQ	1600000000
#define OMAP5_L3_OPP50_RATE	133000000

#define CLK_SCALE_DOWN					0
#define CLK_SCALE_NONE					1
#define CLK_SCALE_UP					2
#define DPLL_RELOCK_NOTNEED				0
#define DPLL_RELOCK_NEEDED				1
#define NO_OF_MM_OPPS 3
#define MM_OPP_LOW_INDEX 0
#define MM_OPP_NOM_INDEX 1
#define MM_OPP_OD_INDEX 2

/* OMAP4 IVA/DSP Clock rates */
#define DPLL_IVA_M4_OPP50_RATE		232800000
#define DPLL_IVA_M4_OPP100_RATE		465500000
#define DPLL_IVA_M4_OPPTURBO_RATE	496000000
#define DPLL_IVA_M4_OPPNITRO_RATE	430000000
#define DPLL_IVA_M4_OPPNITROSB_RATE	500000000

#define DPLL_IVA_M5_OPP50_RATE		133100000
#define DPLL_IVA_M5_OPP100_RATE		266000000
#define DPLL_IVA_M5_OPPTURBO_RATE	331000000
#define DPLL_IVA_M5_OPPNITRO_RATE	430000000
#define DPLL_IVA_M5_OPPNITROSB_RATE	500000000

#define DPLL_IVA_OPP50_RATE			1862400000
#define DPLL_IVA_OPP100_RATE		1862400000
#define DPLL_IVA_OPPTURBO_RATE		992000000
#define DPLL_IVA_OPPNITRO_RATE		1290000000
#define DPLL_IVA_OPPNITROSB_RATE	1500000000

/* OMAP5 IVA/DSP Clock rates */
#ifdef CONFIG_ARCH_OMAP5_ES1
#define DPLL_IVA_H11_OPPLOW_RATE	233000000
#define DPLL_IVA_H11_OPPNOM_RATE	466000000
#define DPLL_IVA_H11_OPPOD_RATE		532000000
#define DPLL_IVA_H12_OPPLOW_RATE	194167741
#define DPLL_IVA_H12_OPPNOM_RATE	388335483
#define DPLL_IVA_H12_OPPOD_RATE		582503225
#define DPLL_IVA_OPPNOM_RATE		2329600000LL
#define DPLL_IVA_OPPOD_RATE			1064000000
#else
#define DPLL_IVA_H11_OPPLOW_RATE	233000000
#define DPLL_IVA_H11_OPPNOM_RATE	465900000
#define DPLL_IVA_H11_OPPOD_RATE		531200000
#define DPLL_IVA_H12_OPPLOW_RATE	194100000
#define DPLL_IVA_H12_OPPNOM_RATE	388300000
#define DPLL_IVA_H12_OPPOD_RATE		531200000
#define DPLL_IVA_OPPNOM_RATE		2329600000LL
#define DPLL_IVA_OPPOD_RATE		1062400000
#endif

struct virt_iva_ck_deps {
	unsigned long iva_ck_rate;
	unsigned long dsp_ck_rate;
	unsigned long iva_dpll_rate;
};

static struct virt_iva_ck_deps omap4_virt_iva_clk_deps[] = {
	{ /* OPP 50 */
		.iva_ck_rate = DPLL_IVA_M5_OPP50_RATE,
		.dsp_ck_rate = DPLL_IVA_M4_OPP50_RATE,
		.iva_dpll_rate = DPLL_IVA_OPP50_RATE,
	},
	{ /* OPP 100 */
		.iva_ck_rate = DPLL_IVA_M5_OPP100_RATE,
		.dsp_ck_rate = DPLL_IVA_M4_OPP100_RATE,
		.iva_dpll_rate = DPLL_IVA_OPP100_RATE,
	},
	{ /* OPP TURBO */
		.iva_ck_rate = DPLL_IVA_M5_OPPTURBO_RATE,
		.dsp_ck_rate = DPLL_IVA_M4_OPPTURBO_RATE,
		.iva_dpll_rate = DPLL_IVA_OPPTURBO_RATE,
	},
	{ /* OPP NITRO */
		.iva_ck_rate = DPLL_IVA_M5_OPPNITRO_RATE,
		.dsp_ck_rate = DPLL_IVA_M4_OPPNITRO_RATE,
		.iva_dpll_rate = DPLL_IVA_OPPNITRO_RATE,
	},
	{ /* OPP NITROSB */
		.iva_ck_rate = DPLL_IVA_M5_OPPNITROSB_RATE,
		.dsp_ck_rate = DPLL_IVA_M4_OPPNITROSB_RATE,
		.iva_dpll_rate = DPLL_IVA_OPPNITROSB_RATE,
	},
};

static struct virt_iva_ck_deps omap5_virt_iva_clk_deps[] = {
	{ /* OPP LOW */
		.iva_ck_rate = DPLL_IVA_H12_OPPLOW_RATE,
		.dsp_ck_rate = DPLL_IVA_H11_OPPLOW_RATE,
		.iva_dpll_rate = DPLL_IVA_OPPNOM_RATE,
	},
	{ /* OPP NOM */
		.iva_ck_rate = DPLL_IVA_H12_OPPNOM_RATE,
		.dsp_ck_rate = DPLL_IVA_H11_OPPNOM_RATE,
		.iva_dpll_rate = DPLL_IVA_OPPNOM_RATE,
	},
	{ /* OPP OD */
		.iva_ck_rate = DPLL_IVA_H12_OPPOD_RATE,
		.dsp_ck_rate = DPLL_IVA_H11_OPPOD_RATE,
		.iva_dpll_rate = DPLL_IVA_OPPOD_RATE,
	},
};

struct virt_l3_clk_data {
	unsigned long opp50_rate;
	unsigned long opp100_rate;
	char *clk_name;
	struct clk *clk_p;
};

static struct virt_l3_clk_data *omap_virt_l3_clk_data;
static struct virt_iva_ck_deps *omap_virt_iva_clk_deps;
static int omap_virt_iva_clk_deps_size;
static struct clk *main_l3_clk;
static unsigned long l3_opp50_rate;
static struct clk *iva_ck, *dsp_ck, *dpll_iva_ck;

static struct virt_l3_clk_data omap4_virt_l3_clk_data[] = {
	{	.opp50_rate = 200000000,
		.opp100_rate = 320000000,
		.clk_name	=  "dpll_core_m3x2_ck"
	},
	{	.opp50_rate = 200000000,
		.opp100_rate = 266600000,
		.clk_name	=  "dpll_core_m6x2_ck"
	},

	{	.opp50_rate = 133333333,
		.opp100_rate = 266666666,
		.clk_name	=  "dpll_core_m7x2_ck"
	},
	{	.opp50_rate = 192000000,
		.opp100_rate = 256000000,
		.clk_name	=  "dpll_per_m3x2_ck"
	},
	{	.opp50_rate = 192000000,
		.opp100_rate = 384000000,
		.clk_name	=  "dpll_per_m6x2_ck"
	},
	{	.opp50_rate = 400000000,
		.opp100_rate = 800000000,
		.clk_name	=  "dpll_core_m2_ck"
	},
	{	.opp50_rate = 0,
		.opp100_rate = 0,
		.clk_name	=  NULL
	},
};

static struct virt_l3_clk_data omap4470_low_virt_l3_clk_data[] = {
	{	.opp50_rate = 200000000,
		.opp100_rate = 320000000,
		.clk_name	=  "dpll_core_m3x2_ck"
	},
	{	.opp50_rate = 200000000,
		.opp100_rate = 266600000,
		.clk_name	=  "dpll_core_m6x2_ck"
	},
	{	.opp50_rate = 192000000,
		.opp100_rate = 256000000,
		.clk_name	=  "dpll_per_m3x2_ck"
	},
	{	.opp50_rate = 400000000,
		.opp100_rate = 800000000,
		.clk_name	=  "dpll_core_m2_ck"
	},
	{	.opp50_rate = 0,
		.opp100_rate = 0,
		.clk_name	=  NULL
	},
};

static struct virt_l3_clk_data omap4470_high_virt_l3_clk_data[] = {
	{	.opp50_rate = 200000000,
		.opp100_rate = 320000000,
		.clk_name	=  "dpll_core_m3x2_ck"
	},
	{	.opp50_rate = 200000000,
		.opp100_rate = 266600000,
		.clk_name	=  "dpll_core_m6x2_ck"
	},
	{	.opp50_rate = 192000000,
		.opp100_rate = 256000000,
		.clk_name	=  "dpll_per_m3x2_ck"
	},
	{	.opp50_rate = 466000000,
		.opp100_rate = 932000000,
		.clk_name	=  "dpll_core_m2_ck"
	},
	{	.opp50_rate = 0,
		.opp100_rate = 0,
		.clk_name	=  NULL
	},
};

static struct virt_l3_clk_data omap5_virt_l3_clk_data[] = {
#ifdef CONFIG_ARCH_OMAP5_ES1
	{	.opp50_rate = 266000000,
		.opp100_rate = 532000000,
		.clk_name	=  "dpll_core_m2_ck"
	},
	{	.opp50_rate = 266000000,
		.opp100_rate = 425600000,
		.clk_name	=  "dpll_core_m3x2_ck"
	},
#else
	{	.opp50_rate = 265800000,
		.opp100_rate = 531800000,
		.clk_name	=  "dpll_core_m2_ck"
	},
	{	.opp50_rate = 265900000,
		.opp100_rate = 425500000,
		.clk_name	=  "dpll_core_m3x2_ck"
	},
#endif
	{	.opp50_rate = 192000000,
		.opp100_rate = 384000000,
		.clk_name	=  "dpll_per_h14x2_ck"
	},
	{	.opp50_rate = 192000000,
		.opp100_rate = 256000000,
		.clk_name	=  "dpll_per_m3x2_ck"
	},
	{	.opp50_rate = 0,
		.opp100_rate = 0,
		.clk_name	=  NULL
	},
};

/**
 * omap_virt_l3_round_rate() - virtual l3 clk round rate
 * @clk:	clk pointer
 * @rate:	frequency to round
 */
long omap_virt_l3_round_rate(struct clk *clk, unsigned long rate)
{
	long parent_rate;

	if (!clk || !clk->parent)
		return 0;

	if (clk->parent->round_rate) {
		parent_rate = clk->parent->round_rate(clk->parent, rate * 2);
		if (parent_rate)
			return parent_rate / 2;
	}
	return 0;
}

/**
 * omap_virt_l3_recalc() - virtual l3 clk recalc
 * @clk: clk pointer to recalculate for
 */
unsigned long omap_virt_l3_recalc(struct clk *clk)
{
	if (!clk || !clk->parent)
		return 0;

	return clk->parent->rate / 2;
}

static int omap_clksel_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = -EINVAL;

	if (!clk->set_rate || !clk->round_rate)
		return ret;

	rate = clk->round_rate(clk, rate);
	if (rate) {
		ret = clk->set_rate(clk, rate);
		if (!ret)
			propagate_rate(clk);
	}
	return ret;
}


/**
 * omap_virt_l3_set_rate() - set rate virtual handler
 * @clk:	clk pointer
 * @rate:	frequency to set
 *
 * Setsup the clocks for the Virtual L3 clock
 */
int omap_virt_l3_set_rate(struct clk *clk, unsigned long rate)
{
	int i;
	struct clk *clk_p;
	unsigned long set_rate;

	if (main_l3_clk)
		omap_clksel_set_rate(main_l3_clk, rate * 2);

	for (i = 0; omap_virt_l3_clk_data[i].clk_name != NULL; i++) {
		clk_p = omap_virt_l3_clk_data[i].clk_p;
		if (clk_p) {
			if (rate <= l3_opp50_rate)
				set_rate = omap_virt_l3_clk_data[i].opp50_rate;
			else
				set_rate = omap_virt_l3_clk_data[i].opp100_rate;
			omap_clksel_set_rate(clk_p, set_rate);
		}
	}

	clk->rate = rate;
	return 0;
}

/**
 * omap_virt_iva_round_rate() - Virtual iva clk round rate
 * @clk:	clk pointer
 * @rate:	frequency to round up to
 */
long omap_virt_iva_round_rate(struct clk *clk, unsigned long rate)
{
	struct virt_iva_ck_deps *iva_deps = NULL;
	long last_diff = LONG_MAX;
	int i;

	if (!clk)
		return 0;

	if (!omap_virt_iva_clk_deps_size)
		return 0;

	for (i = 0; i < omap_virt_iva_clk_deps_size; i++) {
		long diff;
		iva_deps = &omap_virt_iva_clk_deps[i];
		diff = abs(rate - iva_deps->iva_ck_rate);
		if (diff >= last_diff) {
			iva_deps = &omap_virt_iva_clk_deps[i - 1];
			break;
		}
		last_diff = diff;
	}

	if (!iva_deps)
		return 0;

	return iva_deps->iva_ck_rate;
}

/**
 * omap_virt_iva_set_rate() -  set rate for virtual IVA clk
 * @clk:	clk pointer
 * @rate:	frequency to set
 */
int omap_virt_iva_set_rate(struct clk *clk, unsigned long rate)
{
	struct virt_iva_ck_deps *iva_deps = NULL;
	long next_iva_dpll_rate;
	int i, ret = 0;

	if (!clk)
		return -EINVAL;

	if (!omap_virt_iva_clk_deps_size || !dpll_iva_ck || !iva_ck || !dsp_ck)
		return -EINVAL;

	for (i = 0; i < omap_virt_iva_clk_deps_size; i++)
		if (rate == omap_virt_iva_clk_deps[i].iva_ck_rate)
			break;

	if (i < omap_virt_iva_clk_deps_size)
		iva_deps = &omap_virt_iva_clk_deps[i];
	else
		return -EINVAL;

	next_iva_dpll_rate = dpll_iva_ck->round_rate(dpll_iva_ck,
			iva_deps->iva_dpll_rate / 2);

	if (next_iva_dpll_rate == dpll_iva_ck->rate)
		goto set_clock_rates;
	else if (next_iva_dpll_rate < dpll_iva_ck->rate)
		goto set_dpll_rate;

	if (iva_deps->iva_ck_rate < iva_ck->rate) {
		ret = omap_clksel_set_rate(iva_ck, iva_deps->iva_ck_rate);
		if (ret)
			goto out;
	}

	if (iva_deps->dsp_ck_rate < dsp_ck->rate) {
		ret = omap_clksel_set_rate(dsp_ck, iva_deps->dsp_ck_rate);
		if (ret)
			goto out;
	}

set_dpll_rate:
	ret = dpll_iva_ck->set_rate(dpll_iva_ck, next_iva_dpll_rate);
	if (ret)
		goto out;

	propagate_rate(dpll_iva_ck);

set_clock_rates:
	ret = omap_clksel_set_rate(iva_ck, iva_deps->iva_ck_rate);
	if (ret)
		goto out;

	ret = omap_clksel_set_rate(dsp_ck, iva_deps->dsp_ck_rate);
	if (ret)
		goto out;

	clk->rate = iva_deps->iva_ck_rate;
	return 0;

out:
	return ret;
}

/**
 * omap_virt_dsp_round_rate() - DSP virtual clk round rate function
 * @clk:	clk pointer
 * @rate:	frequency to round up
 */
long omap_virt_dsp_round_rate(struct clk *clk, unsigned long rate)
{
	/*
	 * DUMMY Function:
	 * In the current code, DSP clocks are configured as part of IVA Clock
	 * handling. Hence DSP_set and round rate are dummy. When you enable
	 * DSP and try to set DSP rate (using omap_device_scale), it would
	 * still work since DSP OPP Change will trigger IVA OPP change and
	 * as part of IVA OPP change, DSP clocks are handled. As DSP and IVA
	 * clocks are sourced via the same DPLL (with different dividers),
	 * these 2 clocks are tightly coupled and hence it is implemented
	 * this way.
	 */
	return rate;
}

/**
 * omap_virt_dsp_set_rate() - DSP virtual clk set rate function
 * @clk:	clk pointer
 * @rate:	frequency to set
 */
int omap_virt_dsp_set_rate(struct clk *clk, unsigned long rate)
{
	/*
	 * DUMMY Function:
	 * In the current code, DSP clocks are configured as part of IVA Clock
	 * handling. Hence DSP_set and round rate are dummy. When you enable
	 * DSP and try to set DSP rate (using omap_device_scale), it would
	 * still work since DSP OPP Change will trigger IVA OPP change and
	 * as part of IVA OPP change, DSP clocks are handled. As DSP and IVA
	 * clocks are sourced via the same DPLL (with different dividers),
	 * these 2 clocks are tightly coupled and hence it is implemented
	 * this way.
	 */
	clk->rate = rate;
	return 0;
}

/**
 * omap4xxx_custom_clk_init() - clock initialization for OMAP4
 */
int omap4xxx_custom_clk_init(void)
{
	int i;

	if (!cpu_is_omap44xx()) {
		pr_info("Nothing to be done %s\n", __func__);
		return 0;
	}

	main_l3_clk = clk_get(NULL, "dpll_core_m5x2_ck");
	if (!main_l3_clk) {
		pr_err("%s: Unable to get clock dpll_core_m5x2_ck\n", __func__);
		return -EINVAL;
	}

	if (cpu_is_omap447x()) {
		struct clk *dpll_core_ck;
		unsigned long rate = 0;

		dpll_core_ck = clk_get(NULL, "dpll_core_ck");
		if (dpll_core_ck) {
			rate = clk_get_rate(dpll_core_ck);
			clk_put(dpll_core_ck);
		}

		if (rate > OMAP4470_LP_OPP_SET_CORE_DPLL_FREQ / 2)
			omap_virt_l3_clk_data = omap4470_high_virt_l3_clk_data;
		else
			omap_virt_l3_clk_data = omap4470_low_virt_l3_clk_data;

		l3_opp50_rate = main_l3_clk->round_rate(main_l3_clk,
							OMAP4470_L3_OPP50_RATE);
	} else {
		omap_virt_l3_clk_data = omap4_virt_l3_clk_data;
		l3_opp50_rate = main_l3_clk->round_rate(main_l3_clk,
							OMAP4_L3_OPP50_RATE);
	}

	for (i = 0; ; i++) {
		char *clk_name = omap_virt_l3_clk_data[i].clk_name;

		if (!clk_name)
			break;

		omap_virt_l3_clk_data[i].clk_p = clk_get(NULL, clk_name);
		if (!omap_virt_l3_clk_data[i].clk_p) {
			pr_err("%s: Unable to get clock %s\n",
			       __func__, clk_name);
			return -EINVAL;
		}
	}

	iva_ck = clk_get(NULL, "dpll_iva_m5x2_ck");
	if (IS_ERR(iva_ck)) {
		pr_warning("%s:dpll_iva_h12x2_ck clk_get failed.\n", __func__);
		return -EINVAL;
	};

	dsp_ck = clk_get(NULL, "dpll_iva_m4x2_ck");
	if (IS_ERR(dsp_ck)) {
		pr_warning("%s:dpll_iva_h11x2_ck clk_get failed.\n", __func__);
		return -EINVAL;
	}

	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	if (IS_ERR(dpll_iva_ck)) {
		pr_warning("%s:dpll_iva_ck clk_get failed.\n", __func__);
		return -EINVAL;
	}

	omap_virt_iva_clk_deps = omap4_virt_iva_clk_deps;
	omap_virt_iva_clk_deps_size = ARRAY_SIZE(omap4_virt_iva_clk_deps);

	return 0;
}

/**
 * omap5xxx_custom_clk_init() - clock initialization for OMAP5
 */
int omap5xxx_custom_clk_init(void)
{
	int i;

	if (!cpu_is_omap54xx()) {
		pr_info("Nothing to be done %s\n", __func__);
		return 0;
	}

	omap_virt_l3_clk_data = omap5_virt_l3_clk_data;
	main_l3_clk = clk_get(NULL, "dpll_core_h12x2_ck");
	if (!main_l3_clk) {
		pr_err("%s: Unable to get clock dpll_core_h12x2_ck\n",
		       __func__);
		return -EINVAL;
	}

	l3_opp50_rate = OMAP5_L3_OPP50_RATE;

	for (i = 0; ; i++) {
		char *clk_name = omap_virt_l3_clk_data[i].clk_name;

		if (!clk_name)
			break;

		omap_virt_l3_clk_data[i].clk_p = clk_get(NULL, clk_name);
		if (!omap_virt_l3_clk_data[i].clk_p) {
			pr_err("%s: Unable to get clock %s\n",
			       __func__, clk_name);
			return -EINVAL;
		}
	}

	iva_ck = clk_get(NULL, "dpll_iva_h12x2_ck");
	if (IS_ERR(iva_ck)) {
		pr_warning("%s:dpll_iva_h12x2_ck clk_get failed.\n", __func__);
		return -EINVAL;
	};

	dsp_ck = clk_get(NULL, "dpll_iva_h11x2_ck");
	if (IS_ERR(dsp_ck)) {
		pr_warning("%s:dpll_iva_h11x2_ck clk_get failed.\n", __func__);
		return -EINVAL;
	}

	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	if (IS_ERR(dpll_iva_ck)) {
		pr_warning("%s:dpll_iva_ck clk_get failed.\n", __func__);
		return -EINVAL;
	}

	omap_virt_iva_clk_deps = omap5_virt_iva_clk_deps;
	omap_virt_iva_clk_deps_size = ARRAY_SIZE(omap5_virt_iva_clk_deps);

	return 0;
}
