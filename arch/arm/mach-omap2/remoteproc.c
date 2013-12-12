/*
 * Remote processor machine-specific module for OMAP4+ SoCs
 *
 * Copyright (C) 2011-2013 Texas Instruments, Inc.
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
#include <linux/of.h>
#include <linux/platform_data/remoteproc-omap.h>
#include <linux/platform_data/iommu-omap.h>

#include <plat/dmtimer.h>

#include "omap_device.h"
#include "omap_hwmod.h"
#include "soc.h"
#include "control.h"

/**
 * struct omap_rproc_pdev_data - rproc platform device creation helper structure
 * @enabled: flag to dictate creation of the platform device
 * @pdev: platform device handle
 * @cma_addr: Base CMA address to use for the platform device
 * @cma_size: CMA pool size to reserve starting from cma_addr
 *
 * This structure is mainly used to decide to create a platform
 * device or not. The enabled flag for each device is conditionally
 * set based on its corresponding Kconfig symbol.
 */
struct omap_rproc_pdev_data {
	u32 enabled;
	phys_addr_t cma_addr;
	unsigned long cma_size;
	struct platform_device *pdev;
};

/* forward declarations */
static void dra7_ctrl_write_dsp1_boot_addr(u32 bootaddr);
static void dra7_ctrl_write_dsp2_boot_addr(u32 bootaddr);

/*
 * Temporarily define the CMA base address explicitly.
 *
 * This is currently done since there are some dependencies on
 * the remote processor firmware images for properly identifying
 * the vring information. The base addresses will go away as
 * soon as the vring information can be published dynamically
 * and the remote processor firmware code can parse it properly.
 *
 * XXX: Adjust these values depending on your firmware needs.
 * Placing these in Kconfig is not worth the complexity.
 */
#define DRA7_RPROC_CMA_BASE_IPU2	(0x95800000)
#define DRA7_RPROC_CMA_BASE_DSP1	(0x99000000)
#define DRA7_RPROC_CMA_BASE_IPU1	(0x9D000000)
#define DRA7_RPROC_CMA_BASE_DSP2	(0x9F000000)

#define OMAP5_RPROC_CMA_BASE_IPU	(0x95800000)
#define OMAP5_RPROC_CMA_BASE_DSP	(0x95000000)

#define OMAP4_RPROC_CMA_BASE_IPU	(0x99000000)
#define OMAP4_RPROC_CMA_BASE_DSP	(0x98800000)

#define OMAP_RPROC_CMA_SIZE_DSP		(0x800000)
#define DRA7_RPROC_CMA_SIZE_DSP1	(0x4000000)

#define OMAP4_RPROC_CMA_SIZE_IPU	(0x7000000)
#define OMAP5_RPROC_CMA_SIZE_IPU	(0x3800000)
#define DRA7_RPROC_CMA_SIZE_IPU2	(0x3800000)
#define DRA7_RPROC_CMA_SIZE_IPU1	(0x2000000)

/*
 * These data structures define the desired timers that would
 * be needed by the respective processors. The timer info is
 * defined through its hwmod name and a timer id. The id is used
 * only for non-DT boots, and the hwmod_name serves both for
 * identifying the timer as well as a matching logic to be used
 * to lookup the specific timer device node from the DT blob.
 */
static struct omap_rproc_timers_info ipu_timers[] = {
	{ .name = "timer3", .id = 3, },
};

static struct omap_rproc_timers_info dsp_timers[] = {
	{ .name = "timer5", .id = 5, },
};

static struct omap_rproc_timers_info ipu1_timers[] = {
	{ .name = "timer11", .id = 11, },
};

static struct omap_rproc_timers_info dsp2_timers[] = {
	{ .name = "timer6", .id = 6, },
};

/*
 * These data structures define platform-specific information
 * needed for each supported remote processor.
 */
static struct omap_rproc_pdata omap4_rproc_data[] = {
	{
		.name		= "dsp",
		.firmware	= "tesla-dsp.xe64T",
		.mbox_name	= "mbox-dsp",
		.oh_name	= "dsp",
		.timers		= dsp_timers,
		.timers_cnt	= ARRAY_SIZE(dsp_timers),
		.set_bootaddr	= omap_ctrl_write_dsp_boot_addr,
	},
	{
		.name		= "ipu",
		.firmware	= "ducati-m3-core0.xem3",
		.mbox_name	= "mbox-ipu",
		.oh_name	= "ipu",
		.timers		= ipu_timers,
		.timers_cnt	= ARRAY_SIZE(ipu_timers),
	},
};

static struct omap_rproc_pdata dra7_rproc_data[] = {
	{
		.name		= "dsp1",
		.firmware	= "dra7-dsp1-fw.xe66",
		.mbox_name	= "mbox-dsp1",
		.oh_name	= "dsp1",
		.timers		= dsp_timers,
		.timers_cnt	= ARRAY_SIZE(dsp_timers),
		.set_bootaddr	= dra7_ctrl_write_dsp1_boot_addr,
	},
	{
		.name		= "ipu2",
		.firmware	= "dra7-ipu2-fw.xem4",
		.mbox_name	= "mbox-ipu2",
		.oh_name	= "ipu2",
		.timers		= ipu_timers,
		.timers_cnt	= ARRAY_SIZE(ipu_timers),
	},
	{
		.name		= "dsp2",
		.firmware	= "dra7-dsp2-fw.xe66",
		.mbox_name	= "mbox-dsp2",
		.oh_name	= "dsp2",
		.timers		= dsp2_timers,
		.timers_cnt	= ARRAY_SIZE(dsp2_timers),
		.set_bootaddr	= dra7_ctrl_write_dsp2_boot_addr,
	},
	{
		.name		= "ipu1",
		.firmware	= "dra7-ipu1-fw.xem4",
		.mbox_name	= "mbox-ipu1",
		.oh_name	= "ipu1",
		.timers		= ipu1_timers,
		.timers_cnt	= ARRAY_SIZE(ipu1_timers),
	},
};

/*
 * These data structures define the necessary iommu binding information
 * for the respective processor. The listing order should match the
 * order of the platform device and data.
 */
static struct omap_iommu_arch_data omap4_rproc_iommu[] = {
	{ .name = "mmu_dsp" },
	{ .name = "mmu_ipu" },
};

static struct omap_iommu_arch_data dra7_rproc_iommu[] = {
	{ .name = "mmu0_dsp1" },
	{ .name = "mmu_ipu2" },
	{ .name = "mmu0_dsp2" },
	{ .name = "mmu_ipu1" },
};

/*
 * Define the platform devices for the possible remote processors
 * statically. This is primarily needed for tying in the CMA pools
 * at boot time. The specific id assignment helps in identifying
 * a specific processor during debugging, but otherwise serves no
 * major purpose.
 */
static struct platform_device omap4_dsp = {
	.name	= "omap-rproc",
	.id	= 0,
};

static struct platform_device omap4_ipu = {
	.name	= "omap-rproc",
	.id	= 1,
};

static struct platform_device dra7_dsp2 = {
	.name	= "omap-rproc",
	.id	= 2,
};

static struct platform_device dra7_ipu1 = {
	.name	= "omap-rproc",
	.id	= 3,
};

/*
 * These data structures contain the necessary information for
 * dictating the creation of the remote processor platform devices
 * at runtime.
 */
static struct omap_rproc_pdev_data omap4_rproc_pdev_data[] = {
	{
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
		.enabled = 1,
#endif
		.pdev = &omap4_dsp,
		.cma_addr = OMAP4_RPROC_CMA_BASE_DSP,
		.cma_size = OMAP_RPROC_CMA_SIZE_DSP,
	},
	{
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
		.enabled = 1,
#endif
		.pdev = &omap4_ipu,
		.cma_addr = OMAP4_RPROC_CMA_BASE_IPU,
		.cma_size = OMAP4_RPROC_CMA_SIZE_IPU,
	},
};

static struct omap_rproc_pdev_data omap5_rproc_pdev_data[] = {
	{
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
		.enabled = 1,
#endif
		.pdev = &omap4_dsp,
		.cma_addr = OMAP5_RPROC_CMA_BASE_DSP,
		.cma_size = OMAP_RPROC_CMA_SIZE_DSP,
	},
	{
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
		.enabled = 1,
#endif
		.pdev = &omap4_ipu,
		.cma_addr = OMAP5_RPROC_CMA_BASE_IPU,
		.cma_size = OMAP5_RPROC_CMA_SIZE_IPU,
	},
};

static struct omap_rproc_pdev_data dra7_rproc_pdev_data[] = {
	{
#ifdef CONFIG_OMAP_REMOTEPROC_DSP
		.enabled = 1,
#endif
		.pdev = &omap4_dsp,
		.cma_addr = DRA7_RPROC_CMA_BASE_DSP1,
		.cma_size = DRA7_RPROC_CMA_SIZE_DSP1,
	},
	{
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
		.enabled = 1,
#endif
		.pdev = &omap4_ipu,
		.cma_addr = DRA7_RPROC_CMA_BASE_IPU2,
		.cma_size = DRA7_RPROC_CMA_SIZE_IPU2,
	},
	{
#ifdef CONFIG_OMAP_REMOTEPROC_DSP2
		.enabled = 1,
#endif
		.pdev = &dra7_dsp2,
		.cma_addr = DRA7_RPROC_CMA_BASE_DSP2,
		.cma_size = OMAP_RPROC_CMA_SIZE_DSP,
	},
	{
#ifdef CONFIG_OMAP_REMOTEPROC_IPU1
		.enabled = 1,
#endif
		.pdev = &dra7_ipu1,
		.cma_addr = DRA7_RPROC_CMA_BASE_IPU1,
		.cma_size = DRA7_RPROC_CMA_SIZE_IPU1,
	},
};

static struct omap_device_pm_latency omap_rproc_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void dra7_ctrl_write_dsp1_boot_addr(u32 bootaddr)
{
	dra7_ctrl_write_dsp_boot_addr(bootaddr, 0);
}

static void dra7_ctrl_write_dsp2_boot_addr(u32 bootaddr)
{
	dra7_ctrl_write_dsp_boot_addr(bootaddr, 1);
}

/**
 * omap_rproc_device_enable - enable the remoteproc device
 * @pdev: the rproc platform device
 *
 * This function performs the necessary low-level functions to enable
 * a remoteproc device to start executing. This typically includes
 * releasing the reset lines, and enabling the clocks for the device.
 * We do not usually expect this function to fail.
 *
 * Return: 0 on success, or the return code from the failed function
 */
static int omap_rproc_device_enable(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;

	if (strstarts(pdata->name, "dsp")) {
		ret = omap_device_deassert_hardreset(pdev, "dsp");
		if (ret)
			goto out;
	} else if (strstarts(pdata->name, "ipu")) {
		ret = omap_device_deassert_hardreset(pdev, "cpu1");
		if (ret)
			goto out;

		ret = omap_device_deassert_hardreset(pdev, "cpu0");
		if (ret)
			goto out;
	} else {
		pr_err("unsupported remoteproc\n");
		goto out;
	}

	ret = omap_device_enable(pdev);

out:
	if (ret && pdata->name)
		pr_err("failed for proc %s\n", pdata->name);
	return ret;
}

/**
 * omap_rproc_device_shutdown - shutdown the remoteproc device
 * @pdev: the rproc platform device
 *
 * This function performs the necessary low-level functions to shutdown
 * a remoteproc device. This typically includes disabling the clocks
 * for the device and asserting the associated reset lines. We do not
 * usually expect this function to fail.
 *
 * Return: 0 on success, or the return code from the failed function
 */
static int omap_rproc_device_shutdown(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;

	ret = omap_device_shutdown(pdev);
	if (ret)
		goto out;

	if (strstarts(pdata->name, "dsp")) {
		ret = omap_device_assert_hardreset(pdev, "dsp");
	} else if (strstarts(pdata->name, "ipu")) {
		ret = omap_device_assert_hardreset(pdev, "cpu0");
		if (ret)
			goto out;

		ret = omap_device_assert_hardreset(pdev, "cpu1");
		if (ret)
			goto out;
	} else {
		pr_err("unsupported remoteproc\n");
	}

out:
	if (ret && pdata->name)
		pr_err("failed for proc %s\n", pdata->name);
	return ret;
}

/**
 * of_dev_timer_lookup - look up needed timer node from dt blob
 * @np: parent device_node of all the searchable nodes
 * @hwmod_name: hwmod name of the desired timer
 *
 * Parse the dt blob and find out needed timer by matching hwmod
 * name. The match logic only loops through one level of child
 * nodes of the @np parent device node, and uses the @hwmod_name
 * instead of the actual timer device name to minimize the need
 * for defining SoC specific timer data (the DT node names would
 * also incorporate addresses, so the same timer may have completely
 * different addresses on different SoCs).
 *
 * Return: The device node on success or NULL on failure.
 */
static struct device_node *of_dev_timer_lookup(struct device_node *np,
						const char *hwmod_name)
{
	struct device_node *np0 = NULL;
	const char *p;

	for_each_child_of_node(np, np0) {
		if (of_find_property(np0, "ti,hwmods", NULL)) {
			p = of_get_property(np0, "ti,hwmods", NULL);
			if (!strcmp(p, hwmod_name))
				return np0;
		}
	}

	return NULL;
}

/**
 * omap_rproc_enable_timers - enable the timers for a remoteproc
 * @pdev - the remoteproc platform device
 * @configure - boolean flag used to acquire and configure the timer handle
 *
 * This function is used primarily to enable the timers associated with
 * a remoteproc. The configure flag is provided to allow the remoteproc
 * driver core to either acquire and start a timer (during device
 * initialization) or to just start a timer (during a resume operation).
 */
static int omap_rproc_enable_timers(struct platform_device *pdev,
				    bool configure)
{
	int i;
	int ret = 0;
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;
	struct omap_rproc_timers_info *timers = pdata->timers;
	struct device_node *np = NULL;

	if (!timers)
		return -EINVAL;

	if (!configure)
		goto start_timers;

	for (i = 0; i < pdata->timers_cnt; i++) {
		/*
		 * The omap_dm_timer_request_specific will be made obsolete
		 * eventually, and the design needs to rely on dmtimer DT
		 * specific api. This api can only be used in non-DT boots.
		 * For DT-boot, the current logic of requesting the desired
		 * timer utilizes a somewhat crude lookup by finding the
		 * specific DT node from all the sub-nodes of the 'ocp' node
		 * and matching it with its hwmod name.
		 */
		if (!of_have_populated_dt()) {
			timers[i].odt =
				omap_dm_timer_request_specific(timers[i].id);
			goto check_timer;
		}

		np = of_dev_timer_lookup(of_find_node_by_name(NULL, "ocp"),
						timers[i].name);
		if (!np) {
			ret = -ENXIO;
			dev_err(&pdev->dev, "device node lookup for timer %s failed: %d\n",
				timers[i].name, ret);
			goto free_timers;
		}
		timers[i].odt = omap_dm_timer_request_by_node(np);
check_timer:
		if (!timers[i].odt) {
			ret = -EBUSY;
			dev_err(&pdev->dev, "request for timer %s failed: %d\n",
				timers[i].name, ret);
			goto free_timers;
		}
		omap_dm_timer_set_source(timers[i].odt, OMAP_TIMER_SRC_SYS_CLK);
	}

start_timers:
	for (i = 0; i < pdata->timers_cnt; i++)
		omap_dm_timer_start(timers[i].odt);
	return 0;

free_timers:
	while (i--) {
		omap_dm_timer_free(timers[i].odt);
		timers[i].odt = NULL;
	}

	return ret;
}

/**
 * omap_rproc_disable_timers - disable the timers for a remoteproc
 * @pdev - the remoteproc platform device
 * @configure - boolean flag used to release the timer handle
 *
 * This function is used primarily to disable the timers associated with
 * a remoteproc. The configure flag is provided to allow the remoteproc
 * driver core to either stop and release a timer (during device shutdown)
 * or to just stop a timer (during a suspend operation).
 */
static int omap_rproc_disable_timers(struct platform_device *pdev,
				     bool configure)
{
	int i;
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;
	struct omap_rproc_timers_info *timers = pdata->timers;

	for (i = 0; i < pdata->timers_cnt; i++) {
		omap_dm_timer_stop(timers[i].odt);
		if (configure) {
			omap_dm_timer_free(timers[i].odt);
			timers[i].odt = NULL;
		}
	}

	return 0;
}

/**
 * omap_rproc_reserve_cma - reserve CMA pools
 * @platform_type: SoC identifier
 *
 * This function reserves the CMA pools for each of the remoteproc
 * devices at boot time. This function needs to be called from the
 * appropriate board .reserve function in order to define the CMA
 * pools for all possible devices. The CMA pool is reserved only
 * if the corresponding remoteproc platform device is being enabled.
 * The CMA pool information is gathered from the corresponding
 * omap_rproc_pdev_data definitions.
 */
void __init omap_rproc_reserve_cma(int platform_type)
{
	struct omap_rproc_pdev_data *rproc_pdev_data = NULL;
	int rproc_size = 0;
	int i, ret;

	if (platform_type == RPROC_CMA_OMAP4) {
		rproc_pdev_data = omap4_rproc_pdev_data;
		rproc_size = ARRAY_SIZE(omap4_rproc_pdev_data);
	} else if (platform_type == RPROC_CMA_OMAP5) {
		rproc_pdev_data = omap5_rproc_pdev_data;
		rproc_size = ARRAY_SIZE(omap5_rproc_pdev_data);
	} else if (platform_type == RPROC_CMA_DRA7) {
		rproc_pdev_data = dra7_rproc_pdev_data;
		rproc_size = ARRAY_SIZE(dra7_rproc_pdev_data);
	} else {
		pr_err("incompatible machine");
		return;
	}

	for (i = 0; i < rproc_size; i++) {
		struct platform_device *pdev = rproc_pdev_data[i].pdev;

		if (!rproc_pdev_data[i].enabled)
			continue;

		ret = dma_declare_contiguous(&pdev->dev,
						rproc_pdev_data[i].cma_size,
						rproc_pdev_data[i].cma_addr, 0);
		if (ret) {
			pr_err("dma_declare_contiguous failed for rproc pdev %d, status = %d\n",
					 i, ret);
		}
	}
}

/**
 * omap_rproc_init - build remoteproc platform devices
 *
 * This function constructs the platform data for the various remoteproc
 * devices and creates the corresponding omap_devices. The function uses
 * the individual omap_device API rather than a omap_device_build_ss API
 * since it needs the platform devices at boot time for defining the CMA
 * pools and also attach each remote processor device to its specific
 * iommu device separately during init time.
 *
 * Return: 0 on success, or an appropriate error otherwise
 */
static int __init omap_rproc_init(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	int i, ret = 0, oh_count;
	struct omap_rproc_pdata *rproc_data = NULL;
	struct omap_iommu_arch_data *rproc_iommu = NULL;
	struct omap_rproc_pdev_data *rproc_pdev_data = NULL;
	int rproc_size = 0;

	if (cpu_is_omap44xx()) {
		rproc_pdev_data = omap4_rproc_pdev_data;
		rproc_size = ARRAY_SIZE(omap4_rproc_pdev_data);
		rproc_data = omap4_rproc_data;
		rproc_iommu = omap4_rproc_iommu;
	} else if (soc_is_omap54xx()) {
		rproc_pdev_data = omap5_rproc_pdev_data;
		rproc_size = ARRAY_SIZE(omap5_rproc_pdev_data);
		rproc_data = omap4_rproc_data;
		rproc_iommu = omap4_rproc_iommu;
	} else if (soc_is_dra7xx()) {
		rproc_pdev_data = dra7_rproc_pdev_data;
		rproc_size = ARRAY_SIZE(dra7_rproc_pdev_data);
		rproc_data = dra7_rproc_data;
		rproc_iommu = dra7_rproc_iommu;
	} else {
		return 0;
	}

	for (i = 0; i < rproc_size; i++) {
		const char *oh_name = rproc_data[i].oh_name;
		struct platform_device *pdev = rproc_pdev_data[i].pdev;
		oh_count = 0;

		if (!rproc_pdev_data[i].enabled) {
			pr_info("skipping platform_device creation for %s\n",
					rproc_data[i].oh_name);
			continue;
		}

		oh = omap_hwmod_lookup(oh_name);
		if (!oh) {
			pr_err("could not look up hwmod info for %s\n",
						oh_name);
			continue;
		}
		oh_count++;

		rproc_data[i].device_enable = omap_rproc_device_enable;
		rproc_data[i].device_shutdown = omap_rproc_device_shutdown;
		if (rproc_data[i].timers_cnt) {
			rproc_data[i].enable_timers = omap_rproc_enable_timers;
			rproc_data[i].disable_timers =
						omap_rproc_disable_timers;
		}

		device_initialize(&pdev->dev);
		dev_set_name(&pdev->dev, "%s.%d", pdev->name,  pdev->id);

		od = omap_device_alloc(pdev, &oh, oh_count, omap_rproc_latency,
					ARRAY_SIZE(omap_rproc_latency));
		if (!od) {
			dev_err(&pdev->dev, "omap_device_alloc failed\n");
			put_device(&pdev->dev);
			ret = PTR_ERR(od);
			continue;
		}

		ret = platform_device_add_data(pdev, &rproc_data[i],
					       sizeof(struct omap_rproc_pdata));
		if (ret) {
			dev_err(&pdev->dev, "can't add pdata\n");
			omap_device_delete(od);
			put_device(&pdev->dev);
			continue;
		}

		pdev->dev.archdata.iommu = &rproc_iommu[i];

		ret = omap_device_register(pdev);
		if (ret) {
			dev_err(&pdev->dev, "omap_device_register failed\n");
			omap_device_delete(od);
			put_device(&pdev->dev);
			continue;
		}
		dev_info(&pdev->dev, "platform_device for rproc %s created\n",
						rproc_data[i].oh_name);
	}

	return ret;
}
device_initcall(omap_rproc_init);
