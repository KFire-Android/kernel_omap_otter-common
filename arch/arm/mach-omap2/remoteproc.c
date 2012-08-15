/*
 * Remote processor machine-specific module for OMAP4
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
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
#include <linux/remoteproc.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>

#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/remoteproc.h>
#include <plat/iommu.h>
#include <plat/omap-pm.h>

#include "cm1_44xx.h"
#include "cm2_44xx.h"
#include "cm-regbits-44xx.h"

/*
 * CLKCTRL register will be used as idle register, due to the bit 18 is
 * STBYST bit:
 * Read 0x0: Module is functional (not in standby)
 * Read 0x1: Module is in standby
 *
 * standby state is reached every time the remoteproc executes WFI instruction.
 */
#define OMAP4430_CM_M3_M3_CLKCTRL (OMAP4430_CM2_BASE + OMAP4430_CM2_CORE_INST \
		+ OMAP4_CM_DUCATI_DUCATI_CLKCTRL_OFFSET)

#define OMAP4430_CM_DSP_DSP_CLKCTRL (OMAP4430_CM1_BASE \
		+ OMAP4430_CM1_TESLA_INST + OMAP4_CM_TESLA_TESLA_CLKCTRL_OFFSET)


/* CONTROL_DSP_BOOTADDR
 * Description: DSP boot loader physical address. It strores the boot address,
 * from which DSP will start executing code after taken out of reset.
 * This register belongs to the SYSCTRL_GENERAL_CORE set of registers
 * of the OMAP4's Control Module.
 */
#define OMAP4430_CONTROL_DSP_BOOTADDR (0x4A002304)

/*
 * Temporarily define the CMA base address explicitly.
 *
 * This will go away as soon as we have the IOMMU-based generic
 * DMA API in place.
 */

#define OMAP5_RPROC_CMA_BASE_IPU	(0x95800000)
#define OMAP5_RPROC_CMA_BASE_DSP	(0x95000000)

#define OMAP4_RPROC_CMA_BASE_IPU	(0x99000000)
#define OMAP4_RPROC_CMA_BASE_DSP	(0x98800000)


#ifdef CONFIG_OMAP_REMOTEPROC_DSP
static struct omap_rproc_timers_info dsp_timers[] = {
	{ .id = 5 },
#ifdef CONFIG_OMAP_REMOTEPROC_WATCHDOG
	{ .id = 6, .is_wdt = 1 },
#endif
};
#endif

#ifdef CONFIG_OMAP_REMOTEPROC_IPU
static struct omap_rproc_timers_info ipu_timers[] = {
	{ .id = 3 },
#ifdef CONFIG_OMAP_REMOTEPROC_WATCHDOG
	{ .id = 9, .is_wdt = 1 },
	{ .id = 11, .is_wdt = 1 },
#endif
};

static int
_ducati_set_frequency(struct device *dev, struct rproc *rproc, long val)
{
	return 0;
}

static struct rproc_ops ducati_ops = {
	.set_frequency	= _ducati_set_frequency,
};
#endif

/*
 * These data structures define platform-specific information
 * needed for each supported remote processor.
 *
 * At this point we only support the remote dual M3 "Ducati" imaging
 * subsystem (aka "ipu"), but later on we'll also add support for the
 * DSP ("Tesla").
 */
static struct omap_rproc_pdata omap4_rproc_data[] = {
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
	{
		.name		= "dsp_c0",
		.firmware	= "tesla-dsp.xe64T",
		.mbox_name	= "mailbox-2",
		.oh_name	= "dsp_c0",
		.boot_reg	= OMAP4430_CONTROL_DSP_BOOTADDR,
		.timers		= dsp_timers,
		.timers_cnt	= ARRAY_SIZE(dsp_timers),
		.idle_addr	= OMAP4430_CM_DSP_DSP_CLKCTRL,
		.idle_mask	= OMAP4430_STBYST_MASK,
	},
#endif
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
	{
		.name		= "ipu_c0",
		.firmware	= "ducati-m3-core0.xem3",
		.mbox_name	= "mailbox-1",
		.oh_name	= "ipu_c0",
		.oh_name_opt	= "ipu_c1",
		.timers		= ipu_timers,
		.timers_cnt	= ARRAY_SIZE(ipu_timers),
		.ops		= &ducati_ops,
		.idle_addr	= OMAP4430_CM_M3_M3_CLKCTRL,
		.idle_mask	= OMAP4430_STBYST_MASK,
	},
#endif
};

static struct omap_iommu_arch_data omap4_rproc_iommu[] = {
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
	{ .name = "dsp" },
#endif
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
	{ .name = "ipu" },
#endif
};

static struct omap_device_pm_latency omap_rproc_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
static struct platform_device omap4_tesla = {
	.name	= "omap-rproc",
	.id	= 0,
};
#endif
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
static struct platform_device omap4_ducati = {
	.name	= "omap-rproc",
	.id	= 1,
};
#endif

static struct platform_device *omap4_rproc_devs[] __initdata = {
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
	&omap4_tesla,
#endif
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
	&omap4_ducati,
#endif
};

void __init omap_rproc_reserve_cma(int platform_type)
{
	int ret;
	phys_addr_t cma_addr = 0;
	unsigned long cma_size = 0;

#ifdef CONFIG_OMAP_REMOTEPROC_DSP

	cma_size = CONFIG_OMAP_DSP_CMA_SIZE;
	if (platform_type == RPROC_CMA_OMAP4)
		cma_addr = OMAP4_RPROC_CMA_BASE_DSP;
	else if (platform_type == RPROC_CMA_OMAP5)
		cma_addr = OMAP5_RPROC_CMA_BASE_DSP;

	/* reserve CMA memory for OMAP4's dsp "tesla" remote processor */
	ret = dma_declare_contiguous(&omap4_tesla.dev,
					cma_size, cma_addr, 0);
	if (ret)
		pr_err("dma_declare_contiguous failed for dsp %d\n", ret);
#endif
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
	if (platform_type == RPROC_CMA_OMAP4) {
		cma_size = CONFIG_OMAP4_IPU_CMA_SIZE;
		cma_addr = OMAP4_RPROC_CMA_BASE_IPU;
	} else if (platform_type == RPROC_CMA_OMAP5) {
		cma_size = CONFIG_OMAP5_IPU_CMA_SIZE;
		cma_addr = OMAP5_RPROC_CMA_BASE_IPU;
	}

	/* reserve CMA memory for OMAP4's M3 "ducati" remote processor */
	ret = dma_declare_contiguous(&omap4_ducati.dev,
					cma_size, cma_addr, 0);
	if (ret)
		pr_err("dma_declare_contiguous failed for ipu %d\n", ret);
#endif
}

static int __init omap_rproc_init(void)
{
	struct omap_hwmod *oh[2];
	struct omap_device *od;
	int i, ret = 0, oh_count;

	/* names like ipu_cx/dsp_cx might show up on other OMAPs, too */
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return 0;

	/* build the remote proc devices */
	for (i = 0; i < ARRAY_SIZE(omap4_rproc_data); i++) {
		const char *oh_name = omap4_rproc_data[i].oh_name;
		const char *oh_name_opt = omap4_rproc_data[i].oh_name_opt;
		struct platform_device *pdev = omap4_rproc_devs[i];
		oh_count = 0;

		oh[0] = omap_hwmod_lookup(oh_name);
		if (!oh[0]) {
			pr_err("could not look up %s\n", oh_name);
			continue;
		}
		oh_count++;

		/*
		 * ipu might have a secondary hwmod entry (for configurations
		 * where we want both M3 cores to be represented by a single
		 * device).
		 */
		if (oh_name_opt) {
			oh[1] = omap_hwmod_lookup(oh_name_opt);
			if (!oh[1]) {
				pr_err("could not look up %s\n", oh_name_opt);
				continue;
			}
			oh_count++;
		}

		omap4_rproc_data[i].device_enable = omap_device_enable;
		omap4_rproc_data[i].device_shutdown = omap_device_shutdown;

		device_initialize(&pdev->dev);

		/* Set dev_name early to allow dev_xxx in omap_device_alloc */
		dev_set_name(&pdev->dev, "%s.%d", pdev->name,  pdev->id);

		od = omap_device_alloc(pdev, oh, oh_count,
					omap_rproc_latency,
					ARRAY_SIZE(omap_rproc_latency));
		if (!od) {
			dev_err(&pdev->dev, "omap_device_alloc failed\n");
			put_device(&pdev->dev);
			ret = PTR_ERR(od);
			continue;
		}

		ret = platform_device_add_data(pdev,
					&omap4_rproc_data[i],
					sizeof(struct omap_rproc_pdata));
		if (ret) {
			dev_err(&pdev->dev, "can't add pdata\n");
			omap_device_delete(od);
			put_device(&pdev->dev);
			continue;
		}

		/* attach the remote processor to its iommu device */
		pdev->dev.archdata.iommu = &omap4_rproc_iommu[i];

		ret = omap_device_register(pdev);
		if (ret) {
			dev_err(&pdev->dev, "omap_device_register failed\n");
			omap_device_delete(od);
			put_device(&pdev->dev);
			continue;
		}
	}

	return ret;
}
device_initcall(omap_rproc_init);
