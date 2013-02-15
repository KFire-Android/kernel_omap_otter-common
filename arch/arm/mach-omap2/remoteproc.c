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
#include <linux/platform_data/remoteproc-omap.h>
#include <linux/platform_data/iommu-omap.h>

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
#define OMAP_RPROC_CMA_BASE_IPU		(0x99000000)
#define OMAP_RPROC_CMA_BASE_DSP		(0x98800000)

#define OMAP_RPROC_CMA_SIZE_DSP		(0x800000)
#define OMAP_RPROC_CMA_SIZE_IPU		(0x7000000)

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
		.set_bootaddr	= omap_ctrl_write_dsp_boot_addr,
	},
	{
		.name		= "ipu",
		.firmware	= "ducati-m3-core0.xem3",
		.mbox_name	= "mbox-ipu",
		.oh_name	= "ipu",
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
		.cma_addr = OMAP_RPROC_CMA_BASE_DSP,
		.cma_size = OMAP_RPROC_CMA_SIZE_DSP,
	},
	{
#ifdef CONFIG_OMAP_REMOTEPROC_IPU
		.enabled = 1,
#endif
		.pdev = &omap4_ipu,
		.cma_addr = OMAP_RPROC_CMA_BASE_IPU,
		.cma_size = OMAP_RPROC_CMA_SIZE_IPU,
	},
};

static struct omap_device_pm_latency omap_rproc_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

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
 * omap_rproc_reserve_cma - reserve CMA pools
 *
 * This function reserves the CMA pools for each of the remoteproc
 * devices at boot time. This function needs to be called from the
 * appropriate board .reserve function in order to define the CMA
 * pools for all possible devices. The CMA pool is reserved only
 * if the corresponding remoteproc platform device is being enabled.
 * The CMA pool information is gathered from the corresponding
 * omap_rproc_pdev_data definitions.
 */
void __init omap_rproc_reserve_cma(void)
{
	struct omap_rproc_pdev_data *rproc_pdev_data = omap4_rproc_pdev_data;
	int rproc_size = ARRAY_SIZE(omap4_rproc_pdev_data);
	int i, ret;

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

	if (!cpu_is_omap44xx())
		return 0;

	for (i = 0; i < ARRAY_SIZE(omap4_rproc_pdev_data); i++) {
		const char *oh_name = omap4_rproc_data[i].oh_name;
		struct platform_device *pdev = omap4_rproc_pdev_data[i].pdev;
		oh_count = 0;

		if (!omap4_rproc_pdev_data[i].enabled) {
			pr_info("skipping platform_device creation for %s\n",
					omap4_rproc_data[i].oh_name);
			continue;
		}

		oh = omap_hwmod_lookup(oh_name);
		if (!oh) {
			pr_err("could not look up hwmod info for %s\n",
						oh_name);
			continue;
		}
		oh_count++;

		omap4_rproc_data[i].device_enable = omap_rproc_device_enable;
		omap4_rproc_data[i].device_shutdown =
						omap_rproc_device_shutdown;

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

		ret = platform_device_add_data(pdev, &omap4_rproc_data[i],
					       sizeof(struct omap_rproc_pdata));
		if (ret) {
			dev_err(&pdev->dev, "can't add pdata\n");
			omap_device_delete(od);
			put_device(&pdev->dev);
			continue;
		}

		pdev->dev.archdata.iommu = &omap4_rproc_iommu[i];

		ret = omap_device_register(pdev);
		if (ret) {
			dev_err(&pdev->dev, "omap_device_register failed\n");
			omap_device_delete(od);
			put_device(&pdev->dev);
			continue;
		}
		dev_info(&pdev->dev, "platform_device for rproc %s created\n",
						omap4_rproc_data[i].oh_name);
	}

	return ret;
}
device_initcall(omap_rproc_init);
