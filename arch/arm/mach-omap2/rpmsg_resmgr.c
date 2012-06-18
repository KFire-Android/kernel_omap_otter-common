/*
 * Remote processor Resource Manager  machine-specific module for OMAP4
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <plat/rpmsg_resmgr.h>
#include <plat/omap-pm.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/dvfs.h>

static const char * const omap4_pauxclks[] = {
	"sys_clkin_ck",
	"dpll_core_m3x2_ck",
	"dpll_per_m3x2_ck",
};

static struct omap_rprm_auxclk omap4_auxclks[] = {
	{
		.name = "auxclk0_ck",
		.parents = omap4_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap4_pauxclks),
	},
	{
		.name = "auxclk1_ck",
		.parents = omap4_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap4_pauxclks),
	},
	{
		.name = "auxclk2_ck",
		.parents = omap4_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap4_pauxclks),
	},
	{
		.name = "auxclk3_ck",
		.parents = omap4_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap4_pauxclks),
	},
};

static const char * const omap5_pauxclks[] = {
	"sys_clkin",
	"dpll_core_m3x2_opt_ck",
	"dpll_per_m3x2_opt_ck",
};

static struct omap_rprm_auxclk omap5_auxclks[] = {
	{
		.name = "auxclk0_ck",
		.parents = omap5_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap5_pauxclks),
	},
	{
		.name = "auxclk1_ck",
		.parents = omap5_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap5_pauxclks),
	},
	{
		.name = "auxclk2_ck",
		.parents = omap5_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap5_pauxclks),
	},
	{
		.name = "auxclk3_ck",
		.parents = omap5_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap5_pauxclks),
	},
	{
		.name = "auxclk4_ck",
		.parents = omap5_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap5_pauxclks),
	},
	{
		.name = "auxclk5_ck",
		.parents = omap5_pauxclks,
		.parents_cnt = ARRAY_SIZE(omap5_pauxclks),
	},
};


static int omap2_rprm_set_max_dev_wakeup_lat(struct device *rdev,
		struct device *tdev, unsigned long val)
{
	return omap_pm_set_max_dev_wakeup_lat(rdev, tdev, val);
}

static int omap2_rprm_device_scale(struct device *rdev, struct device *tdev,
		unsigned long val)
{
	return omap_device_scale(rdev, tdev, val);
}

static struct omap_rprm_regulator *omap2_rprm_lookup_regulator(u32 reg_id)
{
	static struct omap_rprm_regulator *regulators;
	static u32 regulators_cnt;

	if (!regulators)
		regulators_cnt = omap_rprm_get_regulators(&regulators);

	if (--reg_id >= regulators_cnt)
		return NULL;

	return &regulators[reg_id];
}

static struct omap_rprm_auxclk *auxclks;
static u32 auxclk_cnt;

static struct omap_rprm_auxclk *omap2_rprm_lookup_auxclk(u32 id)
{
	if (id >= auxclk_cnt)
		return NULL;
	return &auxclks[id];
}

static struct omap_rprm_ops omap2_rprm_ops = {
	.set_max_dev_wakeup_lat	= omap2_rprm_set_max_dev_wakeup_lat,
	.device_scale		= omap2_rprm_device_scale,
	.lookup_regulator	= omap2_rprm_lookup_regulator,
	.lookup_auxclk		= omap2_rprm_lookup_auxclk,
};

static struct omap_rprm_pdata omap2_rprm_data = {
	.iss_opt_clk_name	= "iss_ctrlclk",
	.ops			= &omap2_rprm_ops,
};

static int __init omap2_rprm_init(void)
{
	struct platform_device *pdev;
	struct omap_rprm_pdata *pdata = &omap2_rprm_data;
	int ret;

	if (cpu_is_omap54xx()) {
		auxclks = omap5_auxclks;
		auxclk_cnt = ARRAY_SIZE(omap5_auxclks);
	} else if (cpu_is_omap44xx()) {
		auxclks = omap4_auxclks;
		auxclk_cnt = ARRAY_SIZE(omap4_auxclks);
	}

	pdev = platform_device_alloc("omap-rprm", -1);
	if (!pdev)
		return -ENOMEM;

	if (pdata->iss_opt_clk_name) {
		pdata->iss_opt_clk =
				omap_clk_get_by_name(pdata->iss_opt_clk_name);
		if (!pdata->iss_opt_clk) {
			dev_err(&pdev->dev, "error getting iss opt clk\n");
			ret = -ENOENT;
			goto err;
		}
	}

	ret = platform_device_add_data(pdev, pdata, sizeof *pdata);
	if (ret)
		goto err;

	ret =  platform_device_add(pdev);
	if (ret)
		goto err;

	return 0;

err:
	platform_device_put(pdev);
	return ret;
}
device_initcall(omap2_rprm_init);
