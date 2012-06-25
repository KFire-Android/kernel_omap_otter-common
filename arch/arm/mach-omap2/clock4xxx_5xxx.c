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

#include <plat/hardware.h>
#include <plat/clkdev_omap.h>

#include "clock.h"
#include "clock44xx.h"
#include "clock54xx.h"

#define OMAP4_L3_OPP50_RATE 100000000
#define OMAP5_L3_OPP50_RATE 133000000

struct virt_l3_clk_data {
	unsigned long opp50_rate;
	unsigned long opp100_rate;
	char *clk_name;
	struct clk *clk_p;
};

static struct virt_l3_clk_data *omap_virt_l3_clk_data;
static struct clk *main_l3_clk;
static unsigned long l3_opp50_rate;


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

static struct virt_l3_clk_data omap5_virt_l3_clk_data[] = {
	{	.opp50_rate = 266000000,
		.opp100_rate = 532000000,
		.clk_name	=  "dpll_core_m2_ck"
	},
	{	.opp50_rate = 266000000,
		.opp100_rate = 425600000,
		.clk_name	=  "dpll_core_m3x2_ck"
	},

	{	.opp50_rate = 212800000,
		.opp100_rate = 425600000,
		.clk_name	=  "dpll_core_h14x2_ck"
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
 * omap4xxx_custom_clk_init() - clock initialization for OMAP4
 */
int omap4xxx_custom_clk_init(void)
{
	int i;

	if (!cpu_is_omap44xx()) {
		pr_info("Nothing to be done %s\n", __func__);
		return 0;
	}

	omap_virt_l3_clk_data = omap4_virt_l3_clk_data;
	main_l3_clk = clk_get(NULL, "dpll_core_m5x2_ck");
	if (!main_l3_clk) {
		pr_err("%s: Unable to get clock dpll_core_m5x2_ck\n", __func__);
		return -EINVAL;
	}
	l3_opp50_rate = OMAP4_L3_OPP50_RATE;

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

	return 0;
}
