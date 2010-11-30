/**
 * linux/arch/arm/mach-omap2/ipu_dev.c
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Paul Hunt <hunt@ti.com>
 *
 * OMAP4 Image Processing Unit
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/err.h>

#include <mach/irqs.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/omap-pm.h>
#include <linux/platform_device.h>
#include <plat/remoteproc.h>

#include <plat/ipu_dev.h>

#include "../../../drivers/dsp/syslink/ipu_pm/ipu_pm.h"

#define IPU_DRIVER_NAME "omap-ipu-pm"
#define ISS_IPU_BUS_ID 0
#define IVA_IPU_BUS_ID 1

struct omap_ipupm_mod_platform_data *ipupm_get_plat_data(void);

static char *hwmod_state_strings[] = {
	"_HWMOD_STATE_UNKNOWN",
	"_HWMOD_STATE_REGISTERED",
	"_HWMOD_STATE_CLKS_INITED",
	"_HWMOD_STATE_INITIALIZED",
	"_HWMOD_STATE_ENABLED",
	"_HWMOD_STATE_IDLE",
	"_HWMOD_STATE_DISABLED",
};
static void print_hwmod_state(struct omap_hwmod *oh, char *desc)
{
	u32 _state = (u32) oh->_state;
	pr_debug("HWMOD name = %s\n", oh->name);
	if (_state > _HWMOD_STATE_LAST)
		WARN(1, "Illegal hwmod _state = %d\n", _state);
	else
		pr_debug("%s state = %s\n", desc, hwmod_state_strings[_state]);
}

inline int ipu_pm_module_start(unsigned rsrc)
{
	int ret;
	struct omap_ipupm_mod_platform_data *pd;

	pd = ipupm_get_plat_data();

	print_hwmod_state(pd[rsrc].oh, "Previous");
	ret = omap_device_enable(pd[rsrc].pdev);
	if (ret)
		pr_err("device enable failed %s", pd[rsrc].oh_name);
	print_hwmod_state(pd[rsrc].oh, "New");
	return ret;
}
EXPORT_SYMBOL(ipu_pm_module_start);

inline int ipu_pm_module_stop(unsigned rsrc)
{
	int ret;
	struct omap_ipupm_mod_platform_data *pd;

	pd = ipupm_get_plat_data();

	print_hwmod_state(pd[rsrc].oh, "Previous");
	ret = omap_device_shutdown(pd[rsrc].pdev);
	print_hwmod_state(pd[rsrc].oh, "New");

	if (ret)
		pr_err("device disable failed %s", pd[rsrc].oh_name);

	return ret;
}
EXPORT_SYMBOL(ipu_pm_module_stop);

inline int ipu_pm_module_set_rate(unsigned rsrc,
				  unsigned target_rsrc,
				  unsigned rate)
{
	int ret;
	unsigned target;
	struct device *dp;
	struct omap_ipupm_mod_platform_data *pd;

	pd = ipupm_get_plat_data();

	if (target_rsrc == IPU_PM_MPU)
		dp = omap2_get_mpuss_device();
	else if (target_rsrc == IPU_PM_CORE)
		dp = omap2_get_l3_device();
	else {
		if (target_rsrc == IPU_PM_SELF)
			target = rsrc;
		else
			target = target_rsrc;

		if ((pd[target].caps & IPUPM_CAPS_PERF) == 0) {
			pr_err("device set rate not supported for %s",
				pd[target].oh_name);
			ret = -EINVAL;
			goto err_ret;
		} else
			dp = pd[target].dev;
	}

	ret = omap_device_set_rate(pd[rsrc].dev, dp, rate);
	if (ret)
		pr_err("device set rate failed %s", pd[target_rsrc].oh_name);
err_ret:
	return ret;
}
EXPORT_SYMBOL(ipu_pm_module_set_rate);

inline int ipu_pm_module_set_latency(unsigned rsrc,
				     unsigned target_rsrc,
				     int latency)
{
	int ret = 0;
	unsigned target;
	struct omap_ipupm_mod_platform_data *pd;

	pd = ipupm_get_plat_data();

	if (target_rsrc == IPU_PM_MPU) {
#ifdef CONFIG_OMAP_PM
		ret = omap_pm_set_max_mpu_wakeup_lat(&pd[rsrc].qos_request,
						     latency);
#endif
		if (ret)
			goto err_ret;
	} else if (target_rsrc == IPU_PM_CORE) {
#ifdef CONFIG_OMAP_PM
		ret = omap_pm_set_max_sdma_lat(&pd[rsrc].qos_request,
					       latency);
#endif
		if (ret)
			goto err_ret;
	} else {
		if (target_rsrc == IPU_PM_SELF)
			target = rsrc;
		else
			target = target_rsrc;

		if ((pd[target].caps & IPUPM_CAPS_LAT) == 0) {
			pr_err("device set latency not supported for %s",
				pd[target].oh_name);
			ret = -EINVAL;
		} else {
#ifdef CONFIG_OMAP_PM
			ret = omap_pm_set_max_dev_wakeup_lat(pd[rsrc].dev,
							     pd[target].dev,
							     latency);
#endif
		}
	}

	if (ret)
		pr_err("module set latency failed %s", pd[target].oh_name);
err_ret:
	return ret;
}
EXPORT_SYMBOL(ipu_pm_module_set_latency);

inline int ipu_pm_module_set_bandwidth(unsigned rsrc,
				       unsigned target_rsrc,
				       int bandwidth)
{
	int ret = 0;
	struct omap_ipupm_mod_platform_data *pd;

	pd = ipupm_get_plat_data();

	if ((pd[target_rsrc].caps & IPUPM_CAPS_BDW) == 0) {
		pr_err("device set bandwidth not supported for %s",
			pd[target_rsrc].oh_name);
		ret = -EINVAL;
	} else {
#ifdef CONFIG_OMAP_PM
		struct device *dp;
		dp = omap2_get_l3_device();
		ret = omap_pm_set_min_bus_tput(dp,
					       OCP_INITIATOR_AGENT,
					       bandwidth);
#endif
	}

	if (ret)
		pr_err("module set bandwidth failed %s",
						pd[target_rsrc].oh_name);
	return ret;
}
EXPORT_SYMBOL(ipu_pm_module_set_bandwidth);

/* FIXME: not in use now
 * static struct omap_ipupm_mod_ops omap_ipu_ops = {
 *	.start = NULL,
 *	.stop = NULL,
 * };
 *
 * static struct omap_ipupm_mod_ops omap_ipu0_ops = {
 *	.start = NULL,
 *	.stop = NULL,
 * };
 *
 * static struct omap_ipupm_mod_ops omap_ipu1_ops = {
 *	.start = NULL,
 *	.stop = NULL,
 * };
 */

/* ipupm generic operations */
static struct omap_ipupm_mod_ops omap_ipupm_ops = {
	.start = NULL,
	.stop = NULL,
};

static struct omap_ipupm_mod_platform_data omap_ipupm_data[] = {
	{
		.name = "omap-ipu-pm",
		.oh_name = "fdif",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_PERF | IPUPM_CAPS_LAT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "ipu",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_PERF | IPUPM_CAPS_LAT
			| IPUPM_CAPS_EXTINIT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "ipu_c0",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_EXTINIT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "ipu_c1",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_EXTINIT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "iss",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_PERF | IPUPM_CAPS_LAT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "iva",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_PERF | IPUPM_CAPS_LAT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "iva_seq0",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "iva_seq1",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "l3_main_1",
		.caps = IPUPM_CAPS_LAT | IPUPM_CAPS_BDW
			| IPUPM_CAPS_EXTINIT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "mpu",
		.caps = IPUPM_CAPS_PERF | IPUPM_CAPS_LAT
			| IPUPM_CAPS_EXTINIT,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "sl2if",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP,
		.ops = &omap_ipupm_ops,
	},
	{
		.name = "omap-ipu-pm",
		.oh_name = "dsp",
		.caps = IPUPM_CAPS_START | IPUPM_CAPS_STOP
			| IPUPM_CAPS_PERF | IPUPM_CAPS_LAT
			| IPUPM_CAPS_EXTINIT,
		.ops = &omap_ipupm_ops,
	},
};

struct omap_ipupm_mod_platform_data *ipupm_get_plat_data(void)
{
	return omap_ipupm_data;
}
EXPORT_SYMBOL(ipupm_get_plat_data);

static struct omap_device_pm_latency omap_ipupm_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int __init omap_ipussdev_init(void)
{
	int status = -ENODEV;
	int i;
	int first = 1;
	struct omap_hwmod *oh;
	struct omap_device *od;
	char *oh_name;
	char *pdev_name = IPU_DRIVER_NAME;
	struct omap_device_pm_latency *ohl = omap_ipupm_latency;
	int ohl_cnt = ARRAY_SIZE(omap_ipupm_latency);

	for (i = 0; i < ARRAY_SIZE(omap_ipupm_data); i++) {
		oh_name = omap_ipupm_data[i].oh_name;

		if (omap_ipupm_data[i].caps & IPUPM_CAPS_EXTINIT)
			continue;

		oh = omap_hwmod_lookup(oh_name);
		if (!oh) {
			pr_err("%s: could not look up %s\n", __func__, oh_name);

			continue; /* to allow init to continue with other IPs
			* we probably do not want to completely stop progress
			* due to one module anyway, but need to be able to
			* not try to use it if init fails
			* this was put in to handle disabled hwmods
			*/

			status = -ENODEV;
			goto err_init;
		}
		omap_ipupm_data[i].oh = oh;

		od = omap_device_build(pdev_name, IVA_IPU_BUS_ID+i, oh,
				    &omap_ipupm_data[i],
				    sizeof(struct omap_ipupm_mod_platform_data),
				    ohl, ohl_cnt, false);

		status = IS_ERR(od);
		WARN(status, "Could not build omap_device for %s %s\n",
							pdev_name, oh_name);
		if (!status) {
			/* Save the id of the first registered dev */
			if (first) {
				ipu_pm_first_dev = od->pdev.id;
				first = 0;
			}
			omap_ipupm_data[i].pdev = &od->pdev;
			omap_ipupm_data[i].dev = &od->pdev.dev;
		}
	}

err_init:
	return status;
}

arch_initcall(omap_ipussdev_init);
