/*
 * TI's dspbridge platform device registration
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 * Copyright (C) 2009 Nokia Corporation
 *
 * Written by Hiroshi DOYU <Hiroshi.DOYU@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include "prm.h"
#include "cm.h"
#ifdef CONFIG_BRIDGE_DVFS
#include <plat/omap-pm.h>
/*
 * The DSP load balancer works on the following logic:
 * Opp frequencies:
 * 0 <---------> Freq 1 <------------> Freq 2 <---------> Freq 3
 * DSP Thresholds for the frequencies:
 * 0M<-------X-> Freq 1 <-------M--X-> Freq 2 <----M--X-> Freq 3
 * Where, M is the minimal threshold and X is maximum threshold
 *
 * if from Freq x to Freq y; where x > y, transition happens on M
 * if from Freq x to Freq y; where x < y, transition happens on X
 */
#define BRIDGE_THRESH_HIGH_PERCENT	95
#define BRIDGE_THRESH_LOW_PERCENT	88
#endif

#include <dspbridge/host_os.h>

static struct platform_device *dspbridge_pdev;

static struct dspbridge_platform_data dspbridge_pdata __initdata = {
#ifdef CONFIG_BRIDGE_DVFS
	.dsp_set_min_opp = omap_pm_dsp_set_min_opp,
	.dsp_get_opp	 = omap_pm_dsp_get_opp,
	.cpu_set_freq	 = omap_pm_cpu_set_freq,
	.cpu_get_freq	 = omap_pm_cpu_get_freq,
	.dsp_get_rate_table = omap_get_dsp_rate_table,
#endif
	.dsp_prm_read	= prm_read_mod_reg,
	.dsp_prm_write	= prm_write_mod_reg,
	.dsp_prm_rmw_bits = prm_rmw_mod_reg_bits,
	.dsp_cm_read	= cm_read_mod_reg,
	.dsp_cm_write	= cm_write_mod_reg,
	.dsp_cm_rmw_bits = cm_rmw_mod_reg_bits,
};

/**
 * get_opp_table() - populate the pdata with opp info
 * @pdata: pointer to pdata
 *
 * OPP table implementation is a variant b/w platforms.
 * the platform file now incorporates this into the build
 * itself and uses the interface to talk to platform specific
 * functions
 */
static int get_opp_table(struct dspbridge_platform_data *pdata)
{
	int ret = 0;
#ifdef CONFIG_BRIDGE_DVFS
	int opp_count, i, j;
	unsigned long old_rate;
	struct omap_opp *dsp_rate, *dsp_sort, temp;

	opp_count = omap_pm_get_max_vdd1_opp();
	dsp_rate = (*pdata->dsp_get_rate_table)();

	dsp_sort = kzalloc(sizeof(struct omap_opp) * (opp_count + 1),
			   GFP_KERNEL);
	if (!dsp_sort) {
		pr_err("dspbridge mpu sorted table allocation failed\n");
		ret = -ENOMEM;
		goto out;
	}

	memcpy(dsp_sort, dsp_rate, sizeof(struct omap_opp) * (opp_count + 1));

	/* Sort the frequencies to get a linear table */
	for (i = 1; i <= opp_count; i++) {
		temp = dsp_sort[i];
		for (j = i - 1; j >= 0 && dsp_sort[j].rate > temp.rate; j--)
			dsp_sort[j + 1] = dsp_sort[j];
		dsp_sort[j + 1] = temp;
	}

	pdata->mpu_min_speed = dsp_sort[1].rate;
	pdata->mpu_max_speed = dsp_sort[opp_count].rate;

	/* need an initial terminator */
	pdata->dsp_freq_table = kzalloc(
			sizeof(struct dsp_shm_freq_table) *
			(opp_count + 1), GFP_KERNEL);
	if (!pdata->dsp_freq_table) {
		pr_err("dspbridge frequency table allocation failed\n");
		ret = -ENOMEM;
		goto err;
	}

	old_rate = 0;
	/*
	 * the freq table is in terms of khz.. so we need to
	 * divide by 1000
	 */
	for (i = 1; i <= opp_count; i++) {
		/* dsp frequencies are in khz */
		u32 rate = dsp_sort[i].rate / 1000;
		/*
		 * On certain 34xx silicons, certain OPPs are duplicated
		 * for DSP - handle those by copying previous opp value
		 */
		if (rate == old_rate) {
			memcpy(&pdata->dsp_freq_table[i],
				&pdata->dsp_freq_table[i-1],
				sizeof(struct dsp_shm_freq_table));
		} else {
			pdata->dsp_freq_table[i].dsp_freq = rate;
			pdata->dsp_freq_table[i].u_volts = dsp_sort[i].vsel;
			/*
			 * min threshold:
			 * NOTE: index 1 needs a min of 0! else no
			 * scaling happens at DSP!
			 */
			pdata->dsp_freq_table[i].thresh_min_freq =
				((old_rate * BRIDGE_THRESH_LOW_PERCENT) / 100);

			/* max threshold */
			pdata->dsp_freq_table[i].thresh_max_freq =
				((rate * BRIDGE_THRESH_HIGH_PERCENT) / 100);
		}
		old_rate = rate;
	}
	/* the last entry should map with maximum rate */
	pdata->dsp_freq_table[i - 1].thresh_max_freq = old_rate;
	pdata->dsp_num_speeds = opp_count;

err:
	kfree(dsp_sort);

out:
#endif
	return ret;
}

static int __init dspbridge_init(void)
{
	struct platform_device *pdev;
	int err = -ENOMEM;
	struct dspbridge_platform_data *pdata = &dspbridge_pdata;

	pdata->phys_mempool_base = dspbridge_get_mempool_base();

	if (pdata->phys_mempool_base) {
		pdata->phys_mempool_size = CONFIG_BRIDGE_MEMPOOL_SIZE;
		pr_info("%s: %x bytes @ %x\n", __func__,
			pdata->phys_mempool_size, pdata->phys_mempool_base);
	}

	pdev = platform_device_alloc("C6410", -1);
	if (!pdev)
		goto err_out;

	err = get_opp_table(pdata);
	if (err)
		goto err_out;

	err = platform_device_add_data(pdev, pdata, sizeof(*pdata));
	if (err)
		goto err_out;

	err = platform_device_add(pdev);
	if (err)
		goto err_out;

	dspbridge_pdev = pdev;
	return 0;

err_out:
	kfree(pdata->dsp_freq_table);
	pdata->dsp_freq_table = NULL;
	platform_device_put(pdev);
	return err;
}
module_init(dspbridge_init);

static void __exit dspbridge_exit(void)
{
	struct dspbridge_platform_data *pdata = &dspbridge_pdata;
	kfree(pdata->dsp_freq_table);
	pdata->dsp_freq_table = NULL;
	platform_device_unregister(dspbridge_pdev);
}
module_exit(dspbridge_exit);

MODULE_AUTHOR("Hiroshi DOYU");
MODULE_DESCRIPTION("TI's dspbridge platform device registration");
MODULE_LICENSE("GPL v2");
