/*
 * EMIF devices creation for OMAP4+
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Aneesh V <aneesh@ti.com>
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/platform_data/emif_plat.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include "common.h"

static struct emif_platform_data omap_emif_platform_data __initdata = {
	.hw_caps = EMIF_HW_CAPS_LL_INTERFACE
};

/**
 * omap4_emif_set_device_details() - Pass DDR device details from board file
 * @emif_nr:		The EMIF instance on which device is attached
 * @device_info:	Device info such as type, bus width, density etc.
 * @timings:		Timings information from device datasheet passed
 *			as an array of 'struct lpddr2_timings'. Can be NULL
 *			if if default timings are ok.
 * @timings_arr_size:	Size of the timings array. Depends on the number
 *			of different frequencies for which timings data
 *			is provided.
 * @min_tck:		Minimum value of some timing parameters in terms
 *			of number of cycles. Can be NULL if default values
 *			are ok.
 *
 * This function shall be used by the OMAP board files to pass DDR device
 * device information to the EMIF driver. It will in turn create EMIF
 * platform devices and pass the DDR device data and HW capability information
 * to the EMIF driver through platform data.
 */
void __init omap_emif_set_device_details(u32 emif_nr,
			struct ddr_device_info *device_info,
			struct lpddr2_timings *timings,
			u32 timings_arr_size,
			struct lpddr2_min_tck *min_tck,
			struct emif_custom_configs *custom_configs)
{
	struct platform_device	*pd;
	struct omap_hwmod	*oh;
	char			oh_name[10];

	if (emif_nr > 2 || !device_info)
		goto error;

	if (cpu_is_omap44xx()) {
		omap_emif_platform_data.ip_rev = EMIF_4D;
		omap_emif_platform_data.phy_type = EMIF_PHY_TYPE_ATTILAPHY;
	} else if (cpu_is_omap54xx()) {
		omap_emif_platform_data.ip_rev = EMIF_4D5;
		omap_emif_platform_data.phy_type = EMIF_PHY_TYPE_INTELLIPHY;
	} else {
		goto error;
	}

	sprintf(oh_name, "emif%d", emif_nr);

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("EMIF: could not find hwmod device %s\n", oh_name);
		return;
	}

	omap_emif_platform_data.device_info = device_info;
	omap_emif_platform_data.timings = timings;
	omap_emif_platform_data.min_tck = min_tck;
	omap_emif_platform_data.timings_arr_size = timings_arr_size;
	omap_emif_platform_data.custom_configs = custom_configs;

	oh = omap_hwmod_lookup(oh_name);
	if (!oh)
		goto error;

	pd = omap_device_build("emif", emif_nr, oh,
				&omap_emif_platform_data,
				sizeof(omap_emif_platform_data),
				NULL, 0, false);
	if (!pd)
		goto error;

	return;
error:
	pr_err("OMAP: EMIF device generation failed for EMIF%u\n", emif_nr);
	return;
}

/*
 * Reconfigure EMIF timing registers to known values by doing a setrate
 * at the current frequency that will in turn invoke EMIF driver APIs
 */
static int __init init_emif_timings(void)
{
	struct clk	*dpll_core_m2_clk;
	u32		rate;

	dpll_core_m2_clk = clk_get(NULL, "dpll_core_m2_ck");
	if (!dpll_core_m2_clk)
		goto error;

	rate = clk_get_rate(dpll_core_m2_clk);

	pr_info("Reprogramming LPDDR2 timings to %d Hz\n", rate);
	if (clk_set_rate(dpll_core_m2_clk, rate))
		goto error;

	return 0;
error:
	pr_err("init_emif_timings failed\n");
	return -1;
}
subsys_initcall(init_emif_timings);
