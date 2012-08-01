/*
 * linux/arch/arm/mach-omap2/devices.c
 *
 * OMAP2 platform device setup/initialization
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/platform_data/omap4-keypad.h>
#include <linux/pm_runtime.h>
#include <media/omap3isp.h>
#include <sound/omap-abe.h>

#include <linux/omap_ocp2scp.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/pmu.h>

#include "iomap.h"
#include <plat/board.h>
#include <plat/mmc.h>
#include <plat/dma.h>
#include <plat/gpu.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/omap4-keypad.h>
#include <plat/rpmsg_resmgr.h>
#include <plat/dvfs.h>
#include <plat/omap-pm.h>
#include <linux/mfd/omap_control.h>

#include "mux.h"
#include "control.h"
#include "devices.h"

#define L3_MODULES_MAX_LEN 12
#define L3_MODULES 3

static int __init omap_init_control(void)
{
	struct omap_hwmod		*oh;
	struct platform_device		*pdev;
	const char			*oh_name, *name;
	struct omap_control_data	*pdata;

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if ((!(cpu_is_omap44xx())) && (!cpu_is_omap54xx()))
		return -ENODEV;

	oh_name = "ctrl_module_core";
	name = "omap-control-core";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not lookup hwmod for %s\n", oh_name);
		return PTR_ERR(oh);
	}

	/* If dtb is there, the devices will be created dynamically */
	if (of_have_populated_dt()) {
		/* Probe the driver using the Device Tree model */
		pdev = omap_device_build(name, -1, oh, NULL, 0, NULL, 0, true);
	} else { /* Probe the driver using platform data */
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: No memory for scm pdata\n", __func__);
			return -EINVAL;
		}

		pdata->has_usb_phy = true;
		pdata->has_bandgap = true;
		if (cpu_is_omap44xx())
			pdata->rev = 1;
		else
			pdata->rev = 2;

		pdev = omap_device_build(name, -1, oh, pdata, sizeof(*pdata),
							NULL, 0, false);
	}

	if (IS_ERR(pdev)) {
		pr_err("Could not build omap_device for %s %s\n",
		       name, oh_name);
		return PTR_ERR(pdev);
	}

	return 0;
}
postcore_initcall(omap_init_control);

static int __init omap3_l3_init(void)
{
	int l;
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	char oh_name[L3_MODULES_MAX_LEN];

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if (!(cpu_is_omap34xx()))
		return -ENODEV;

	l = snprintf(oh_name, L3_MODULES_MAX_LEN, "l3_main");

	oh = omap_hwmod_lookup(oh_name);

	if (!oh)
		pr_err("could not look up %s\n", oh_name);

	pdev = omap_device_build("omap_l3_smx", 0, oh, NULL, 0,
							   NULL, 0, 0);

	WARN(IS_ERR(pdev), "could not build omap_device for %s\n", oh_name);

	return IS_ERR(pdev) ? PTR_ERR(pdev) : 0;
}
postcore_initcall(omap3_l3_init);

static int __init omap4_l3_init(void)
{
	int l, i;
	struct omap_hwmod *oh[3];
	struct platform_device *pdev;
	char oh_name[L3_MODULES_MAX_LEN];

	/* If dtb is there, the devices will be created dynamically */
	if (of_have_populated_dt())
		return -ENODEV;

	/*
	 * To avoid code running on other OMAPs in
	 * multi-omap builds
	 */
	if ((!(cpu_is_omap44xx())) && (!cpu_is_omap54xx()))
		return -ENODEV;

	for (i = 0; i < L3_MODULES; i++) {
		l = snprintf(oh_name, L3_MODULES_MAX_LEN, "l3_main_%d", i+1);

		oh[i] = omap_hwmod_lookup(oh_name);
		if (!(oh[i]))
			pr_err("could not look up %s\n", oh_name);
	}

	pdev = omap_device_build_ss("omap_l3_noc", 0, oh, 3, NULL,
						     0, NULL, 0, 0);

	WARN(IS_ERR(pdev), "could not build omap_device for %s\n", oh_name);

	return IS_ERR(pdev) ? PTR_ERR(pdev) : 0;
}
postcore_initcall(omap4_l3_init);

#if defined(CONFIG_VIDEO_OMAP2) || defined(CONFIG_VIDEO_OMAP2_MODULE)

static struct resource omap2cam_resources[] = {
	{
		.start		= OMAP24XX_CAMERA_BASE,
		.end		= OMAP24XX_CAMERA_BASE + 0xfff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_24XX_CAM_IRQ,
		.flags		= IORESOURCE_IRQ,
	}
};

static struct platform_device omap2cam_device = {
	.name		= "omap24xxcam",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(omap2cam_resources),
	.resource	= omap2cam_resources,
};
#endif

static struct isp_platform_data bogus_isp_pdata;

#if defined(CONFIG_IOMMU_API)

#include <plat/iommu.h>

static struct resource omap3isp_resources[] = {
	{
		.start		= OMAP3430_ISP_BASE,
		.end		= OMAP3430_ISP_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CCP2_BASE,
		.end		= OMAP3430_ISP_CCP2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CCDC_BASE,
		.end		= OMAP3430_ISP_CCDC_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_HIST_BASE,
		.end		= OMAP3430_ISP_HIST_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_H3A_BASE,
		.end		= OMAP3430_ISP_H3A_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_PREV_BASE,
		.end		= OMAP3430_ISP_PREV_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_RESZ_BASE,
		.end		= OMAP3430_ISP_RESZ_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_SBL_BASE,
		.end		= OMAP3430_ISP_SBL_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CSI2A_REGS1_BASE,
		.end		= OMAP3430_ISP_CSI2A_REGS1_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CSIPHY2_BASE,
		.end		= OMAP3430_ISP_CSIPHY2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSI2A_REGS2_BASE,
		.end		= OMAP3630_ISP_CSI2A_REGS2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSI2C_REGS1_BASE,
		.end		= OMAP3630_ISP_CSI2C_REGS1_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSIPHY1_BASE,
		.end		= OMAP3630_ISP_CSIPHY1_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3630_ISP_CSI2C_REGS2_BASE,
		.end		= OMAP3630_ISP_CSI2C_REGS2_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_34XX_CAM_IRQ,
		.flags		= IORESOURCE_IRQ,
	}
};

static struct platform_device omap3isp_device = {
	.name		= "omap3isp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(omap3isp_resources),
	.resource	= omap3isp_resources,
};

static struct omap_iommu_arch_data omap3_isp_iommu = {
	.name = "isp",
};

int omap3_init_camera(struct isp_platform_data *pdata)
{
	omap3isp_device.dev.platform_data = pdata;
	omap3isp_device.dev.archdata.iommu = &omap3_isp_iommu;

	return platform_device_register(&omap3isp_device);
}

#else /* !CONFIG_IOMMU_API */

int omap3_init_camera(struct isp_platform_data *pdata)
{
	return 0;
}

#endif

static inline void omap_init_camera(void)
{
#if defined(CONFIG_VIDEO_OMAP2) || defined(CONFIG_VIDEO_OMAP2_MODULE)
	if (cpu_is_omap24xx())
		platform_device_register(&omap2cam_device);
#endif
}

int __init omap4_keyboard_init(struct omap4_keypad_platform_data
			*sdp4430_keypad_data, struct omap_board_data *bdata)
{
	struct platform_device *pdev;
	struct omap_hwmod *oh;
	struct omap4_keypad_platform_data *keypad_data;
	unsigned int id = -1;
	char *oh_name = "kbd";
	char *name = "omap4-keypad";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not look up %s\n", oh_name);
		return -ENODEV;
	}

	keypad_data = sdp4430_keypad_data;

	pdev = omap_device_build(name, id, oh, keypad_data,
			sizeof(struct omap4_keypad_platform_data), NULL, 0, 0);

	if (IS_ERR(pdev)) {
		WARN(1, "Can't build omap_device for %s:%s.\n",
						name, oh->name);
		return PTR_ERR(pdev);
	}
	oh->mux = omap_hwmod_mux_init(bdata->pads, bdata->pads_cnt);

	return 0;
}

#if defined(CONFIG_OMAP_MBOX_FWK) || defined(CONFIG_OMAP_MBOX_FWK_MODULE)
static inline void __init omap_init_mbox(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("mailbox");
	if (!oh) {
		pr_err("%s: unable to find hwmod\n", __func__);
		return;
	}

	pdev = omap_device_build("omap-mailbox", -1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "%s: could not build device, err %ld\n",
						__func__, PTR_ERR(pdev));
}
#else
static inline void omap_init_mbox(void) { }
#endif /* CONFIG_OMAP_MBOX_FWK */

static inline void omap_init_sti(void) {}

#if defined(CONFIG_SND_SOC) || defined(CONFIG_SND_SOC_MODULE)
static struct platform_device omap_pcm = {
	.name	= "omap-pcm-audio",
	.id	= -1,
};

static void omap_init_audio(void)
{
	platform_device_register(&omap_pcm);
}

#else
static inline void omap_init_audio(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_MCASP) || \
	defined(CONFIG_SND_OMAP_SOC_MCASP_MODULE)
static struct omap_device_pm_latency omap_mcasp_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_mcasp(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("mcasp");
	if (!oh) {
		pr_err("could not look up mcasp hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-mcasp", -1, oh, NULL, 0,
				omap_mcasp_latency,
				ARRAY_SIZE(omap_mcasp_latency), 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for omap-mcasp-audio.\n");
}
#else
static inline void omap_init_mcasp(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_MCPDM) || \
		defined(CONFIG_SND_OMAP_SOC_MCPDM_MODULE)

static void __init omap_init_mcpdm(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("mcpdm");
	if (!oh) {
		printk(KERN_ERR "Could not look up mcpdm hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-mcpdm", -1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for omap-mcpdm.\n");
}
#else
static inline void omap_init_mcpdm(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_DMIC) || \
		defined(CONFIG_SND_OMAP_SOC_DMIC_MODULE)

static void __init omap_init_dmic(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("dmic");
	if (!oh) {
		printk(KERN_ERR "Could not look up mcpdm hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-dmic", -1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for omap-dmic.\n");
}
#else
static inline void omap_init_dmic(void) {}
#endif

static struct omap_device_pm_latency omap_gpu_latency[] = {
	[0] = {
		.deactivate_func	= omap_device_idle_hwmods,
		.activate_func		= omap_device_enable_hwmods,
		.flags			= OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void __init omap_init_gpu(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	struct gpu_platform_data *pdata;
	const char *oh_name = "gpu";
	const char *name = "pvrsrvkm";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("omap_init_gpu: Could not look up %s\n", oh_name);
		return;
	}

	pdata = kzalloc(sizeof(struct gpu_platform_data),
					GFP_KERNEL);
	if (!pdata) {
		pr_err("omap_init_gpu: Platform data memory allocation failed\n");
		return;
	}

	pdata->device_scale = omap_device_scale;
	pdata->device_enable = omap_device_enable;
	pdata->device_idle = omap_device_idle;
	pdata->device_shutdown = omap_device_shutdown;
	pdata->opp_get_opp_count = opp_get_opp_count;
	pdata->opp_find_freq_ceil = opp_find_freq_ceil;
	pdata->access_process_vm = access_process_vm;

	pdev = omap_device_build(name, 0, oh, pdata,
			     sizeof(struct gpu_platform_data),
			     omap_gpu_latency, ARRAY_SIZE(omap_gpu_latency), 0);
	WARN(IS_ERR(pdev), "Could not build omap_device for %s %s\n",
	     name, oh_name);

	/*
	 * Most OMAP5xxx ES1.0 silicon does not support OPP_OD, therefore the
	 * OPP table will enable only OPP_LOW and OPP_NOM. As per the 5430 and
	 * 5432 data manuals for ES1.0, OPP_LOW and OPP_NOM are 192 MHz and
	 * 384 MHz respectively and require DPLL_PER as source clock.
	 */
	if ((omap_rev() == OMAP5430_REV_ES1_0) ||
	    (omap_rev() == OMAP5432_REV_ES1_0)) {
		struct clk *cck, *hck, *pck, *orig_cck_parent;
		int ret;

		cck = clk_get(&pdev->dev, "gpu_core_clk_mux");
		if (IS_ERR_OR_NULL(cck)) {
			WARN(1, "%s: Requesting GPU core clock failed. \
				GPU may be unstable\n", __func__);
			goto exit;
		}

		hck = clk_get(&pdev->dev, "gpu_hyd_clk_mux");
		if (IS_ERR_OR_NULL(hck)) {
			WARN(1, "%s: Requesting GPU hyd clock failed. \
				GPU may be unstable\n", __func__);
			goto exit_hyd;
		}

		pck = clk_get(&pdev->dev, "dpll_core_h14x2_ck");
		if (IS_ERR_OR_NULL(pck)) {
			WARN(1, "Requesting GPU core clock failed.\n");
			goto exit_dpll;
		}
		orig_cck_parent = clk_get_parent(cck);
		if (IS_ERR(orig_cck_parent)) {
			WARN(1, "%s: Getting original GPU core clock failed. \
				GPU may be unstable\n", __func__);
			goto exit_set_parent_cck;
		}
		ret = clk_set_parent(cck, pck);
		if (IS_ERR_VALUE(ret)) {
			WARN(1, "%s: Setting GPU core parent clock failed. \
				GPU may be unstable\n", __func__);
			goto exit_set_parent_cck;
		}

		ret = clk_set_parent(hck, pck);
		if (IS_ERR_VALUE(ret)) {
			WARN(1, "%s: Setting GPU hyd parent clock failed. \
				GPU may be unstable\n", __func__);
			ret = clk_set_parent(cck, orig_cck_parent);
			if (IS_ERR_VALUE(ret))
				pr_err("%s: Cannot reset GPU core clock back \
					to original clock parent\n", __func__);
			goto exit_set_parent_cck;
		}

exit_set_parent_cck:
		clk_put(pck);
exit_dpll:
		clk_put(hck);
exit_hyd:
		clk_put(cck);
	}
exit:
	kfree(pdata);
}

#if defined(CONFIG_SND_OMAP_SOC_ABE) || \
	defined(CONFIG_SND_OMAP_SOC_ABE_MODULE)

static struct platform_device omap_abe_vxrec = {
	.name   = "omap-abe-vxrec-dai",
	.id     = -1,
};

static struct platform_device omap_abe_echo = {
	.name   = "omap-abe-echo-dai",
	.id     = -1,
};

static struct omap_device_pm_latency omap_aess_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void __init omap_init_aess(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;
	struct omap_abe_pdata *pdata;

	oh = omap_hwmod_lookup("aess");
	if (!oh) {
		pr_err("Could not look up aess hw_mod\n");
		return;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("Could not allocate aess pdata\n");
		return;
	}

	pdata->get_context_loss_count = omap_pm_get_dev_context_loss_count;
	pdata->device_scale = omap_device_scale;

	pdev = omap_device_build("aess", -1, oh,
				pdata, sizeof(*pdata),
				omap_aess_latency,
				ARRAY_SIZE(omap_aess_latency), 0);
	if (IS_ERR(pdev)) {
		WARN(1, "Can't build omap_device for omap-aess-audio.\n");
		kfree(pdata);
		return;
	}

	platform_device_register(&omap_abe_vxrec);
	platform_device_register(&omap_abe_echo);

	kfree(pdata);
}
#else
static inline void omap_init_aess(void) {}
#endif

#if defined(CONFIG_SND_OMAP_SOC_OMAP_HDMI) || \
		defined(CONFIG_SND_OMAP_SOC_OMAP_HDMI_MODULE)

static struct platform_device omap_hdmi_audio = {
	.name	= "omap-hdmi-audio",
	.id	= -1,
};

static void __init omap_init_hdmi_audio(void)
{
	struct omap_hwmod *oh;
	struct platform_device *pdev;

	oh = omap_hwmod_lookup("dss_hdmi");
	if (!oh) {
		printk(KERN_ERR "Could not look up dss_hdmi hw_mod\n");
		return;
	}

	pdev = omap_device_build("omap-hdmi-audio-dai",
		-1, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev),
	     "Can't build omap_device for omap-hdmi-audio-dai.\n");

	platform_device_register(&omap_hdmi_audio);
}
#else
static inline void omap_init_hdmi_audio(void) {}
#endif

#if defined(CONFIG_SPI_OMAP24XX) || defined(CONFIG_SPI_OMAP24XX_MODULE)

#include <plat/mcspi.h>

static int __init omap_mcspi_init(struct omap_hwmod *oh, void *unused)
{
	struct platform_device *pdev;
	char *name = "omap2_mcspi";
	struct omap2_mcspi_platform_config *pdata;
	static int spi_num;
	struct omap2_mcspi_dev_attr *mcspi_attrib = oh->dev_attr;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("Memory allocation for McSPI device failed\n");
		return -ENOMEM;
	}

	pdata->num_cs = mcspi_attrib->num_chipselect;
	switch (oh->class->rev) {
	case OMAP2_MCSPI_REV:
	case OMAP3_MCSPI_REV:
			pdata->regs_offset = 0;
			break;
	case OMAP4_MCSPI_REV:
			pdata->regs_offset = OMAP4_MCSPI_REG_OFFSET;
			break;
	default:
			pr_err("Invalid McSPI Revision value\n");
			kfree(pdata);
			return -EINVAL;
	}

	spi_num++;
	pdev = omap_device_build(name, spi_num, oh, pdata,
				sizeof(*pdata),	NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for %s:%s\n",
				name, oh->name);
	kfree(pdata);
	return 0;
}

static void omap_init_mcspi(void)
{
	omap_hwmod_for_each_by_class("mcspi", omap_mcspi_init, NULL);
}

#else
static inline void omap_init_mcspi(void) {}
#endif

static struct resource omap2_pmu_resource = {
	.start	= 3,
	.end	= 3,
	.flags	= IORESOURCE_IRQ,
};

static struct resource omap3_pmu_resource = {
	.start	= INT_34XX_BENCH_MPU_EMUL,
	.end	= INT_34XX_BENCH_MPU_EMUL,
	.flags	= IORESOURCE_IRQ,
};

static struct platform_device omap_pmu_device = {
	.name		= "arm-pmu",
	.id		= ARM_PMU_DEVICE_CPU,
	.num_resources	= 1,
};

static void omap_init_pmu(void)
{
	if (cpu_is_omap24xx())
		omap_pmu_device.resource = &omap2_pmu_resource;
	else if (cpu_is_omap34xx())
		omap_pmu_device.resource = &omap3_pmu_resource;
	else
		return;

	platform_device_register(&omap_pmu_device);
}


#if defined(CONFIG_CRYPTO_DEV_OMAP_SHAM) || defined(CONFIG_CRYPTO_DEV_OMAP_SHAM_MODULE)

#ifdef CONFIG_ARCH_OMAP2
static struct resource omap2_sham_resources[] = {
	{
		.start	= OMAP24XX_SEC_SHA1MD5_BASE,
		.end	= OMAP24XX_SEC_SHA1MD5_BASE + 0x64,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_24XX_SHA1MD5,
		.flags	= IORESOURCE_IRQ,
	}
};
static int omap2_sham_resources_sz = ARRAY_SIZE(omap2_sham_resources);
#else
#define omap2_sham_resources		NULL
#define omap2_sham_resources_sz		0
#endif

#ifdef CONFIG_ARCH_OMAP3
static struct resource omap3_sham_resources[] = {
	{
		.start	= OMAP34XX_SEC_SHA1MD5_BASE,
		.end	= OMAP34XX_SEC_SHA1MD5_BASE + 0x64,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_34XX_SHA1MD52_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= OMAP34XX_DMA_SHA1MD5_RX,
		.flags	= IORESOURCE_DMA,
	}
};
static int omap3_sham_resources_sz = ARRAY_SIZE(omap3_sham_resources);
#else
#define omap3_sham_resources		NULL
#define omap3_sham_resources_sz		0
#endif

static struct platform_device sham_device = {
	.name		= "omap-sham",
	.id		= -1,
};

static void omap_init_sham(void)
{
	if (cpu_is_omap24xx()) {
		sham_device.resource = omap2_sham_resources;
		sham_device.num_resources = omap2_sham_resources_sz;
	} else if (cpu_is_omap34xx()) {
		sham_device.resource = omap3_sham_resources;
		sham_device.num_resources = omap3_sham_resources_sz;
	} else {
		pr_err("%s: platform not supported\n", __func__);
		return;
	}
	platform_device_register(&sham_device);
}
#else
static inline void omap_init_sham(void) { }
#endif

#if defined(CONFIG_CRYPTO_DEV_OMAP_AES) || defined(CONFIG_CRYPTO_DEV_OMAP_AES_MODULE)

#ifdef CONFIG_ARCH_OMAP2
static struct resource omap2_aes_resources[] = {
	{
		.start	= OMAP24XX_SEC_AES_BASE,
		.end	= OMAP24XX_SEC_AES_BASE + 0x4C,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= OMAP24XX_DMA_AES_TX,
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= OMAP24XX_DMA_AES_RX,
		.flags	= IORESOURCE_DMA,
	}
};
static int omap2_aes_resources_sz = ARRAY_SIZE(omap2_aes_resources);
#else
#define omap2_aes_resources		NULL
#define omap2_aes_resources_sz		0
#endif

#ifdef CONFIG_ARCH_OMAP3
static struct resource omap3_aes_resources[] = {
	{
		.start	= OMAP34XX_SEC_AES_BASE,
		.end	= OMAP34XX_SEC_AES_BASE + 0x4C,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= OMAP34XX_DMA_AES2_TX,
		.flags	= IORESOURCE_DMA,
	},
	{
		.start	= OMAP34XX_DMA_AES2_RX,
		.flags	= IORESOURCE_DMA,
	}
};
static int omap3_aes_resources_sz = ARRAY_SIZE(omap3_aes_resources);
#else
#define omap3_aes_resources		NULL
#define omap3_aes_resources_sz		0
#endif

static struct platform_device aes_device = {
	.name		= "omap-aes",
	.id		= -1,
};

static void omap_init_aes(void)
{
	if (cpu_is_omap24xx()) {
		aes_device.resource = omap2_aes_resources;
		aes_device.num_resources = omap2_aes_resources_sz;
	} else if (cpu_is_omap34xx()) {
		aes_device.resource = omap3_aes_resources;
		aes_device.num_resources = omap3_aes_resources_sz;
	} else {
		pr_err("%s: platform not supported\n", __func__);
		return;
	}
	platform_device_register(&aes_device);
}

#else
static inline void omap_init_aes(void) { }
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE)

static inline void omap242x_mmc_mux(struct omap_mmc_platform_data
							*mmc_controller)
{
	if ((mmc_controller->slots[0].switch_pin > 0) && \
		(mmc_controller->slots[0].switch_pin < OMAP_MAX_GPIO_LINES))
		omap_mux_init_gpio(mmc_controller->slots[0].switch_pin,
					OMAP_PIN_INPUT_PULLUP);
	if ((mmc_controller->slots[0].gpio_wp > 0) && \
		(mmc_controller->slots[0].gpio_wp < OMAP_MAX_GPIO_LINES))
		omap_mux_init_gpio(mmc_controller->slots[0].gpio_wp,
					OMAP_PIN_INPUT_PULLUP);

	omap_mux_init_signal("sdmmc_cmd", 0);
	omap_mux_init_signal("sdmmc_clki", 0);
	omap_mux_init_signal("sdmmc_clko", 0);
	omap_mux_init_signal("sdmmc_dat0", 0);
	omap_mux_init_signal("sdmmc_dat_dir0", 0);
	omap_mux_init_signal("sdmmc_cmd_dir", 0);
	if (mmc_controller->slots[0].caps & MMC_CAP_4_BIT_DATA) {
		omap_mux_init_signal("sdmmc_dat1", 0);
		omap_mux_init_signal("sdmmc_dat2", 0);
		omap_mux_init_signal("sdmmc_dat3", 0);
		omap_mux_init_signal("sdmmc_dat_dir1", 0);
		omap_mux_init_signal("sdmmc_dat_dir2", 0);
		omap_mux_init_signal("sdmmc_dat_dir3", 0);
	}

	/*
	 * Use internal loop-back in MMC/SDIO Module Input Clock
	 * selection
	 */
	if (mmc_controller->slots[0].internal_clock) {
		u32 v = omap_ctrl_readl(OMAP2_CONTROL_DEVCONF0);
		v |= (1 << 24);
		omap_ctrl_writel(v, OMAP2_CONTROL_DEVCONF0);
	}
}

void __init omap242x_init_mmc(struct omap_mmc_platform_data **mmc_data)
{
	char *name = "mmci-omap";

	if (!mmc_data[0]) {
		pr_err("%s fails: Incomplete platform data\n", __func__);
		return;
	}

	omap242x_mmc_mux(mmc_data[0]);
	omap_mmc_add(name, 0, OMAP2_MMC1_BASE, OMAP2420_MMC_SIZE,
					INT_24XX_MMC_IRQ, mmc_data[0]);
}

#endif

#if defined(CONFIG_OMAP_OCP2SCP) || defined(CONFIG_OMAP_OCP2SCP_MODULE)
static int count_ocp2scp_devices(struct omap_ocp2scp_dev *ocp2scp_dev)
{
	int cnt	= 0;

	while (ocp2scp_dev->drv_name != NULL) {
		cnt++;
		ocp2scp_dev++;
	}

	return cnt;
}

static void __init omap_init_ocp2scp(void)
{
	struct omap_hwmod	*oh;
	struct platform_device	*pdev;
	int			bus_id = -1, dev_cnt = 0, i;
	struct omap_ocp2scp_dev	*ocp2scp_dev;
	const char		*oh_name, *name;
	struct omap_ocp2scp_platform_data *pdata;

	if (cpu_is_omap44xx())
		oh_name = "ocp2scp_usb_phy";
	else
		oh_name = "ocp2scp1";

	name	= "omap-ocp2scp";

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("%s: could not find omap_hwmod for %s\n", __func__,
								oh_name);
		return;
	}

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: No memory for ocp2scp pdata\n", __func__);
		return;
	}

	ocp2scp_dev = oh->dev_attr;
	dev_cnt = count_ocp2scp_devices(ocp2scp_dev);

	if (!dev_cnt) {
		pr_err("%s: No devices connected to ocp2scp\n", __func__);
		return;
	}

	pdata->devices = kzalloc(sizeof(struct omap_ocp2scp_dev *)
					* dev_cnt, GFP_KERNEL);
	if (!pdata->devices) {
		pr_err("%s: No memory for ocp2scp pdata devices\n", __func__);
		return;
	}

	for (i = 0; i < dev_cnt; i++, ocp2scp_dev++)
		pdata->devices[i] = ocp2scp_dev;

	pdata->dev_cnt	= dev_cnt;

	pdev = omap_device_build(name, bus_id, oh, pdata, sizeof(*pdata), NULL,
								0, false);
	if (IS_ERR(pdev)) {
		pr_err("Could not build omap_device for %s %s\n",
						name, oh_name);
		return;
	}
}
#else
static inline void omap_init_ocp2scp(void) { }
#endif

static struct omap_rprm_regulator *omap_regulators;
static u32 omap_regulators_cnt;
void __init omap_rprm_regulator_init(struct omap_rprm_regulator *regulators,
				u32 regulator_cnt)
{
	if (!regulator_cnt)
		return;

	omap_regulators = regulators;
	omap_regulators_cnt = regulator_cnt;
}

u32 omap_rprm_get_regulators(struct omap_rprm_regulator **regulators)
{
	if (omap_regulators_cnt)
		*regulators = omap_regulators;

	return omap_regulators_cnt;
}
EXPORT_SYMBOL(omap_rprm_get_regulators);

static __init void omap_init_dev(char *name,
		struct omap_device_pm_latency *pm_lats, int pm_lats_cnt)
{
	struct platform_device *pd;
	struct omap_hwmod *oh;

	oh = omap_hwmod_lookup(name);
	if (!oh) {
		pr_err("Could not look up %s hwmod\n", name);
		return;
	}

	pd = omap_device_build(name, -1, oh, NULL, 0, pm_lats, pm_lats_cnt, 0);
	if (IS_ERR(pd))
		pr_err("Can't build omap_device for %s.\n", name);
	else
		pm_runtime_enable(&pd->dev);
}

static int omap_cam_deactivate(struct omap_device *od)
{
	int i;

	for (i = 0; i < od->hwmods_cnt; i++)
		omap_hwmod_reset(od->hwmods[i]);

	return omap_device_idle_hwmods(od);
}

static struct omap_device_pm_latency omap_cam_latency[] = {
	{
		.deactivate_func = omap_cam_deactivate,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	}
};

static void __init omap_init_fdif(void)
{
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return;

	omap_init_dev("fdif", omap_cam_latency, ARRAY_SIZE(omap_cam_latency));
}

static void __init omap_init_sl2if(void)
{
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return;

	omap_init_dev("sl2if", NULL, 0);
}

static void __init omap_init_iss(void)
{
	if (!cpu_is_omap44xx() && !cpu_is_omap54xx())
		return;

	omap_init_dev("iss", omap_cam_latency, ARRAY_SIZE(omap_cam_latency));
}

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_HDQ_MASTER_OMAP) || defined(CONFIG_HDQ_MASTER_OMAP_MODULE)
#define OMAP_HDQ_BASE	0x480B2000
static struct resource omap_hdq_resources[] = {
	{
		.start		= OMAP_HDQ_BASE,
		.end		= OMAP_HDQ_BASE + 0x1C,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_24XX_HDQ_IRQ,
		.flags		= IORESOURCE_IRQ,
	},
};
static struct platform_device omap_hdq_dev = {
	.name = "omap_hdq",
	.id = 0,
	.dev = {
		.platform_data = NULL,
	},
	.num_resources	= ARRAY_SIZE(omap_hdq_resources),
	.resource	= omap_hdq_resources,
};
static inline void omap_hdq_init(void)
{
	if (cpu_is_omap2420())
		return;

	platform_device_register(&omap_hdq_dev);
}
#else
static inline void omap_hdq_init(void) {}
#endif

/*---------------------------------------------------------------------------*/

#if defined(CONFIG_VIDEO_OMAP2_VOUT) || \
	defined(CONFIG_VIDEO_OMAP2_VOUT_MODULE)
#if defined(CONFIG_FB_OMAP2) || defined(CONFIG_FB_OMAP2_MODULE)
static struct resource omap_vout_resource[3 - CONFIG_FB_OMAP2_NUM_FBS] = {
};
#else
static struct resource omap_vout_resource[2] = {
};
#endif

static struct platform_device omap_vout_device = {
	.name		= "omap_vout",
	.num_resources	= ARRAY_SIZE(omap_vout_resource),
	.resource 	= &omap_vout_resource[0],
	.id		= -1,
};
static void omap_init_vout(void)
{
	if (platform_device_register(&omap_vout_device) < 0)
		printk(KERN_ERR "Unable to register OMAP-VOUT device\n");
}
#else
static inline void omap_init_vout(void) {}
#endif

/*-------------------------------------------------------------------------*/

static int __init omap2_init_devices(void)
{
	/*
	 * please keep these calls, and their implementations above,
	 * in alphabetical order so they're easier to sort through.
	 */
	omap_init_aess();
	omap_init_audio();
	omap_init_mcasp();
	omap_init_mcpdm();
	omap_init_dmic();
	omap_init_camera();
	omap_init_gpu();
	omap_init_hdmi_audio();
	omap3_init_camera(&bogus_isp_pdata);
	omap_init_mbox();
	omap_init_mcspi();
	omap_init_pmu();
	omap_hdq_init();
	omap_init_sti();
	omap_init_sham();
	omap_init_aes();
	omap_init_vout();
	omap_init_ocp2scp();
	omap_init_fdif();
	omap_init_sl2if();
	omap_init_iss();

	return 0;
}
arch_initcall(omap2_init_devices);

#if defined(CONFIG_OMAP_WATCHDOG) || defined(CONFIG_OMAP_WATCHDOG_MODULE)
static int __init omap_init_wdt(void)
{
	int id = -1;
	struct platform_device *pdev;
	struct omap_hwmod *oh;
	char *oh_name = "wd_timer2";
	char *dev_name = "omap_wdt";

	if (!cpu_class_is_omap2())
		return 0;

	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not look up wd_timer%d hwmod\n", id);
		return -EINVAL;
	}

	pdev = omap_device_build(dev_name, id, oh, NULL, 0, NULL, 0, 0);
	WARN(IS_ERR(pdev), "Can't build omap_device for %s:%s.\n",
				dev_name, oh->name);
	return 0;
}
subsys_initcall(omap_init_wdt);
#endif
