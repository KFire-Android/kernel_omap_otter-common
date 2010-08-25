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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/pmu.h>

#include <plat/control.h>
#include <plat/tc.h>
#include <plat/board.h>
#include <plat/mux.h>
#include <mach/gpio.h>
#include <plat/mmc.h>
#include <plat/display.h>
#include <plat/dma.h>
#include <plat/gpu.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/omap4-keypad.h>
#include <plat/mcbsp.h>
#include <sound/omap-abe-dsp.h>

#include "mux.h"

static struct resource omap_32k_resources[] = {
	{
		.start          = -EINVAL,      /* gets changed later */
		.end            = -EINVAL,      /* gets changed later */
		.flags          = IORESOURCE_MEM,
	},
};

static struct platform_device omap_32k_device = {
	.name                   = "omap-32k-sync-timer",
	.id                     = -1,
	.num_resources          = ARRAY_SIZE(omap_32k_resources),
	.resource               = omap_32k_resources,
};

static void omap_init_32k(void)
{
	if (cpu_is_omap2420()) {
		omap_32k_resources[0].start = OMAP2420_32KSYNCT_BASE;
		omap_32k_resources[0].end = OMAP2420_32KSYNCT_BASE + SZ_4K;
	} else if (cpu_is_omap2430()) {
		omap_32k_resources[0].start = OMAP2430_32KSYNCT_BASE;
		omap_32k_resources[0].end = OMAP2430_32KSYNCT_BASE + SZ_4K;
	} else if (cpu_is_omap34xx()) {
		omap_32k_resources[0].start = OMAP3430_32KSYNCT_BASE;
		omap_32k_resources[0].end = OMAP3430_32KSYNCT_BASE + SZ_4K;
	} else if (cpu_is_omap44xx()) {
		omap_32k_resources[0].start = OMAP4430_32KSYNCT_BASE;
		omap_32k_resources[0].end = OMAP4430_32KSYNCT_BASE + SZ_4K;
	} else {        /* not supported */
		return;
	}


	(void) platform_device_register(&omap_32k_device);
};

#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>

#if defined(CONFIG_VIDEO_OMAP2) || defined(CONFIG_VIDEO_OMAP2_MODULE)

static struct resource cam_resources[] = {
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

static struct platform_device omap_cam_device = {
	.name		= "omap24xxcam",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(cam_resources),
	.resource	= cam_resources,
};

static inline void omap_init_camera(void)
{
	platform_device_register(&omap_cam_device);
}

#elif defined(CONFIG_VIDEO_OMAP3) || defined(CONFIG_VIDEO_OMAP3_MODULE)

static struct resource omap3isp_resources[] = {
	{
		.start		= OMAP3430_ISP_BASE,
		.end		= OMAP3430_ISP_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CBUFF_BASE,
		.end		= OMAP3430_ISP_CBUFF_END,
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
		.start		= OMAP3430_ISP_CSI2A_BASE,
		.end		= OMAP3430_ISP_CSI2A_END,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3430_ISP_CSI2PHY_BASE,
		.end		= OMAP3430_ISP_CSI2PHY_END,
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

static inline void omap_init_camera(void)
{
	platform_device_register(&omap3isp_device);
}
#else
static inline void omap_init_camera(void)
{
}
#endif

#ifdef CONFIG_ARCH_OMAP4 /* KEYBOARD */

struct omap_device_pm_latency omap_keyboard_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int omap4_init_keypad(struct omap_hwmod *oh, void *user)
{
	struct omap_device *od;
	struct omap4_keypad_platform_data *sdp4430_keypad_data;
	unsigned int id = -1;
	char *name = "omap4-keypad";

	sdp4430_keypad_data = user;

	od = omap_device_build(name, id, oh, sdp4430_keypad_data,
			sizeof(struct omap4_keypad_platform_data),
			omap_keyboard_latency,
			ARRAY_SIZE(omap_keyboard_latency), 0);
	WARN(IS_ERR(od), "Could not build omap_device for %s %s\n",
		name, oh->name);

	return 0;
}

int omap4_keypad_initialization(struct omap4_keypad_platform_data
						*sdp4430_keypad_data)
{
	if (!cpu_is_omap44xx())
		return -ENODEV;

	return omap_hwmod_for_each_by_class("kbd", omap4_init_keypad,
			sdp4430_keypad_data);
}

#endif /* KEYBOARD CONFIG_ARCH_OMAP4 */

#if defined(CONFIG_OMAP_MBOX_FWK) || defined(CONFIG_OMAP_MBOX_FWK_MODULE)

#define MBOX_REG_SIZE   0x120

#ifdef CONFIG_ARCH_OMAP2
static struct resource omap2_mbox_resources[] = {
	{
		.start		= OMAP24XX_MAILBOX_BASE,
		.end		= OMAP24XX_MAILBOX_BASE + MBOX_REG_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_24XX_MAIL_U0_MPU,
		.flags		= IORESOURCE_IRQ,
	},
	{
		.start		= INT_24XX_MAIL_U3_MPU,
		.flags		= IORESOURCE_IRQ,
	},
};
static int omap2_mbox_resources_sz = ARRAY_SIZE(omap2_mbox_resources);
#else
#define omap2_mbox_resources		NULL
#define omap2_mbox_resources_sz		0
#endif

#ifdef CONFIG_ARCH_OMAP3
static struct resource omap3_mbox_resources[] = {
	{
		.start		= OMAP34XX_MAILBOX_BASE,
		.end		= OMAP34XX_MAILBOX_BASE + MBOX_REG_SIZE - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= INT_24XX_MAIL_U0_MPU,
		.flags		= IORESOURCE_IRQ,
	},
};
static int omap3_mbox_resources_sz = ARRAY_SIZE(omap3_mbox_resources);
#else
#define omap3_mbox_resources		NULL
#define omap3_mbox_resources_sz		0
#endif

#ifdef CONFIG_ARCH_OMAP4

#define OMAP4_MBOX_REG_SIZE	0x130
static struct resource omap4_mbox_resources[] = {
	{
		.start          = OMAP44XX_MAILBOX_BASE,
		.end            = OMAP44XX_MAILBOX_BASE +
					OMAP4_MBOX_REG_SIZE - 1,
		.flags          = IORESOURCE_MEM,
	},
	{
		.start          = OMAP44XX_IRQ_MAIL_U0,
		.flags          = IORESOURCE_IRQ,
	},
};
static int omap4_mbox_resources_sz = ARRAY_SIZE(omap4_mbox_resources);
#else
#define omap4_mbox_resources		NULL
#define omap4_mbox_resources_sz		0
#endif

static struct platform_device mbox_device = {
	.name		= "omap2-mailbox",
	.id		= -1,
};

static inline void omap_init_mbox(void)
{
	if (cpu_is_omap24xx()) {
		mbox_device.resource = omap2_mbox_resources;
		mbox_device.num_resources = omap2_mbox_resources_sz;
	} else if (cpu_is_omap34xx()) {
		mbox_device.resource = omap3_mbox_resources;
		mbox_device.num_resources = omap3_mbox_resources_sz;
	} else if (cpu_is_omap44xx()) {
		mbox_device.resource = omap4_mbox_resources;
		mbox_device.num_resources = omap4_mbox_resources_sz;
	} else {
		pr_err("%s: platform not supported\n", __func__);
		return;
	}
	platform_device_register(&mbox_device);
}
#else
static inline void omap_init_mbox(void) { }
#endif /* CONFIG_OMAP_MBOX_FWK */

#if defined CONFIG_ARCH_OMAP4

static struct omap4_abe_dsp_pdata omap4_abe_dsp_config = {
	/* FIXME: dsp */
//	int (*device_enable) (struct platform_device *pdev);
	//int (*device_shutdown) (struct platform_device *pdev);
//	int (*device_idle) (struct platform_device *pdev);
};

static struct platform_device codec_dmic0 = {
	.name	= "dmic-codec",
	.id	= 0,
};

static struct platform_device codec_dmic1 = {
	.name	= "dmic-codec",
	.id	= 1,
};

static struct platform_device codec_dmic2 = {
	.name	= "dmic-codec",
	.id	= 2,
};

static struct platform_device omap_abe_dai = {
	.name	= "omap-abe-dai",
	.id	= -1,
};

static struct platform_device omap_dsp_audio = {
	.name	= "omap-dsp-audio",
	.id	= -1,
	.dev		= {
		.platform_data = &omap4_abe_dsp_config,
	},
};

static struct platform_device omap_dmic_dai0 = {
	.name	= "omap-dmic-dai",
	.id	= 0,
};

static struct platform_device omap_dmic_dai1 = {
	.name	= "omap-dmic-dai",
	.id	= 1,
};

static struct platform_device omap_dmic_dai2 = {
	.name	= "omap-dmic-dai",
	.id	= 2,
};

static inline void omap_init_abe(void)
{
	platform_device_register(&codec_dmic0);
	platform_device_register(&codec_dmic1);
	platform_device_register(&codec_dmic2);
	platform_device_register(&omap_abe_dai);
	platform_device_register(&omap_dsp_audio);
	platform_device_register(&omap_dmic_dai0);
	platform_device_register(&omap_dmic_dai1);
	platform_device_register(&omap_dmic_dai2);
}
#else
static inline void omap_init_abe(void) {}
#endif

#if defined(CONFIG_OMAP_STI)

#if defined(CONFIG_ARCH_OMAP2)

#define OMAP2_STI_BASE		0x48068000
#define OMAP2_STI_CHANNEL_BASE	0x54000000
#define OMAP2_STI_IRQ		4

static struct resource sti_resources[] = {
	{
		.start		= OMAP2_STI_BASE,
		.end		= OMAP2_STI_BASE + 0x7ff,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP2_STI_CHANNEL_BASE,
		.end		= OMAP2_STI_CHANNEL_BASE + SZ_64K - 1,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP2_STI_IRQ,
		.flags		= IORESOURCE_IRQ,
	}
};
#elif defined(CONFIG_ARCH_OMAP3)

#define OMAP3_SDTI_BASE		0x54500000
#define OMAP3_SDTI_CHANNEL_BASE	0x54600000

static struct resource sti_resources[] = {
	{
		.start		= OMAP3_SDTI_BASE,
		.end		= OMAP3_SDTI_BASE + 0xFFF,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP3_SDTI_CHANNEL_BASE,
		.end		= OMAP3_SDTI_CHANNEL_BASE + SZ_1M - 1,
		.flags		= IORESOURCE_MEM,
	}
};

#endif

static struct platform_device sti_device = {
	.name		= "sti",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sti_resources),
	.resource	= sti_resources,
};

static inline void omap_init_sti(void)
{
	platform_device_register(&sti_device);
}
#else
static inline void omap_init_sti(void) {}
#endif

#if defined(CONFIG_SND_SOC) || defined(CONFIG_SND_SOC_MODULE)

static struct platform_device omap_pcm = {
	.name	= "omap-pcm-audio",
	.id	= -1,
};

/*
 * OMAP2420 has 2 McBSP ports
 * OMAP2430 has 5 McBSP ports
 * OMAP3 has 5 McBSP ports
 * OMAP4 has 4 McBSP ports
 */
OMAP_MCBSP_PLATFORM_DEVICE(1);
OMAP_MCBSP_PLATFORM_DEVICE(2);
OMAP_MCBSP_PLATFORM_DEVICE(3);
OMAP_MCBSP_PLATFORM_DEVICE(4);
OMAP_MCBSP_PLATFORM_DEVICE(5);

static void omap_init_audio(void)
{
	platform_device_register(&omap_mcbsp1);
	platform_device_register(&omap_mcbsp2);
	if (cpu_is_omap243x() || cpu_is_omap34xx() || cpu_is_omap44xx()) {
		platform_device_register(&omap_mcbsp3);
		platform_device_register(&omap_mcbsp4);
	}
	if (cpu_is_omap243x() || cpu_is_omap34xx())
		platform_device_register(&omap_mcbsp5);

	platform_device_register(&omap_pcm);
}

#else
static inline void omap_init_audio(void) {}
#endif

#if defined(CONFIG_SPI_OMAP24XX) || defined(CONFIG_SPI_OMAP24XX_MODULE)

#include <plat/mcspi.h>

struct omap_device_pm_latency omap_mcspi_latency[] = {
	[0] = {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int omap_mcspi_init(struct omap_hwmod *oh, void *user)
{
	struct omap_device *od;
	char *name = "omap2_mcspi";
	struct omap2_mcspi_platform_config *pdata;
	static int spi_num;

	pdata = kzalloc(sizeof(struct omap2_mcspi_platform_config),
				GFP_KERNEL);
	if (!pdata) {
		pr_err("\nMemory allocation for Mcspi device failed\n");
		return -ENOMEM;
	}

	switch (spi_num) {
	case 0:
		pdata->num_cs = 4;
		pdata->force_cs_mode = 1;
		break;
	case 1:
		pdata->num_cs = 2;
		pdata->mode = OMAP2_MCSPI_MASTER;
		pdata->dma_mode = 1;
		pdata->force_cs_mode = 0;
		pdata->fifo_depth = 0;
		break;
	case 2:
		pdata->num_cs = 2;
		break;
	case 3:
		pdata->num_cs = 1;
		break;
	}

	if (cpu_is_omap44xx())
		pdata->regs_data = (u16 *)omap4_reg_map;
	else
		pdata->regs_data = (u16 *)omap2_reg_map;

	od = omap_device_build(name, spi_num + 1, oh, pdata,
				sizeof(*pdata),	omap_mcspi_latency,
				ARRAY_SIZE(omap_mcspi_latency), 0);
	WARN(IS_ERR(od), "\nCant build omap_device for %s:%s\n",
				name, oh->name);
	spi_num++;
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

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE) || \
	defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)

static struct omap_device_pm_latency omap_hsmmc_latency[] = {
	[0] = {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
	/*
	 * XXX There should also be an entry here to power off/on the
	 * MMC regulators/PBIAS cells, etc.
	 */
};

static inline void omap2_mmc_mux(struct omap_mmc_platform_data *mmc_controller,
			int controller_nr)
{
	if ((mmc_controller->slots[0].switch_pin > 0) && \
		(mmc_controller->slots[0].switch_pin < OMAP_MAX_GPIO_LINES))
		omap_mux_init_gpio(mmc_controller->slots[0].switch_pin,
					OMAP_PIN_INPUT_PULLUP);
	if ((mmc_controller->slots[0].gpio_wp > 0) && \
		(mmc_controller->slots[0].gpio_wp < OMAP_MAX_GPIO_LINES))
		omap_mux_init_gpio(mmc_controller->slots[0].gpio_wp,
					OMAP_PIN_INPUT_PULLUP);

	if (cpu_is_omap2420() && controller_nr == 0) {
		omap_cfg_reg(H18_24XX_MMC_CMD);
		omap_cfg_reg(H15_24XX_MMC_CLKI);
		omap_cfg_reg(G19_24XX_MMC_CLKO);
		omap_cfg_reg(F20_24XX_MMC_DAT0);
		omap_cfg_reg(F19_24XX_MMC_DAT_DIR0);
		omap_cfg_reg(G18_24XX_MMC_CMD_DIR);
		if (mmc_controller->slots[0].wires == 4) {
			omap_cfg_reg(H14_24XX_MMC_DAT1);
			omap_cfg_reg(E19_24XX_MMC_DAT2);
			omap_cfg_reg(D19_24XX_MMC_DAT3);
			omap_cfg_reg(E20_24XX_MMC_DAT_DIR1);
			omap_cfg_reg(F18_24XX_MMC_DAT_DIR2);
			omap_cfg_reg(E18_24XX_MMC_DAT_DIR3);
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

	if (cpu_is_omap34xx()) {
		if (controller_nr == 0) {
			omap_mux_init_signal("sdmmc1_clk",
				OMAP_PIN_INPUT_PULLUP);
			omap_mux_init_signal("sdmmc1_cmd",
				OMAP_PIN_INPUT_PULLUP);
			omap_mux_init_signal("sdmmc1_dat0",
				OMAP_PIN_INPUT_PULLUP);
			if (mmc_controller->slots[0].wires == 4 ||
				mmc_controller->slots[0].wires == 8) {
				omap_mux_init_signal("sdmmc1_dat1",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc1_dat2",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc1_dat3",
					OMAP_PIN_INPUT_PULLUP);
			}
			if (mmc_controller->slots[0].wires == 8) {
				omap_mux_init_signal("sdmmc1_dat4",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc1_dat5",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc1_dat6",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc1_dat7",
					OMAP_PIN_INPUT_PULLUP);
			}
		}
		if (controller_nr == 1) {
			/* MMC2 */
			omap_mux_init_signal("sdmmc2_clk",
				OMAP_PIN_INPUT_PULLUP);
			omap_mux_init_signal("sdmmc2_cmd",
				OMAP_PIN_INPUT_PULLUP);
			omap_mux_init_signal("sdmmc2_dat0",
				OMAP_PIN_INPUT_PULLUP);

			/*
			 * For 8 wire configurations, Lines DAT4, 5, 6 and 7 need to be muxed
			 * in the board-*.c files
			 */
			if (mmc_controller->slots[0].wires == 4 ||
				mmc_controller->slots[0].wires == 8) {
				omap_mux_init_signal("sdmmc2_dat1",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc2_dat2",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc2_dat3",
					OMAP_PIN_INPUT_PULLUP);
			}
			if (mmc_controller->slots[0].wires == 8) {
				omap_mux_init_signal("sdmmc2_dat4.sdmmc2_dat4",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc2_dat5.sdmmc2_dat5",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc2_dat6.sdmmc2_dat6",
					OMAP_PIN_INPUT_PULLUP);
				omap_mux_init_signal("sdmmc2_dat7.sdmmc2_dat7",
					OMAP_PIN_INPUT_PULLUP);
			}
		}

		/*
		 * For MMC3 the pins need to be muxed in the board-*.c files
		 */
	}
}

#define MAX_OMAP_MMC_HWMOD_NAME_LEN		16

void __init omap2_init_mmc(struct omap_mmc_platform_data *mmc_data, int ctrl_nr)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	struct omap_device_pm_latency *ohl;
	char oh_name[MAX_OMAP_MMC_HWMOD_NAME_LEN];
	char *name;
	int l;
	int ohl_cnt = 0;

	if (cpu_is_omap2420()) {
		name = "mmci-omap";
	} else {
		name = "mmci-omap-hs";
		ohl = omap_hsmmc_latency;
		ohl_cnt = ARRAY_SIZE(omap_hsmmc_latency);
	}

	l = snprintf(oh_name, MAX_OMAP_MMC_HWMOD_NAME_LEN,
		     "mmc%d", ctrl_nr);
	WARN(l >= MAX_OMAP_MMC_HWMOD_NAME_LEN,
	     "String buffer overflow in MMC%d device setup\n", ctrl_nr);
	oh = omap_hwmod_lookup(oh_name);
	if (!oh) {
		pr_err("Could not look up %s\n", oh_name);
		return;
	}

	mmc_data->dev_attr = oh->dev_attr;

	omap2_mmc_mux(mmc_data, ctrl_nr - 1);
	od = omap_device_build(name, ctrl_nr - 1, oh, mmc_data,
			       sizeof(struct omap_mmc_platform_data),
			       ohl, ohl_cnt, false);
	WARN(IS_ERR(od), "Could not build omap_device for %s %s\n",
	     name, oh_name);

	/*
	 * return device handle to board setup code
	 * required to populate for regulator framework structure
	 */
	mmc_data->dev = &od->pdev.dev;
}

#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_HDQ_MASTER_OMAP) || defined(CONFIG_HDQ_MASTER_OMAP_MODULE)
#if defined(CONFIG_ARCH_OMAP2430) || defined(CONFIG_ARCH_OMAP3430)
#define OMAP_HDQ_BASE	0x480B2000
#endif
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
	(void) platform_device_register(&omap_hdq_dev);
}
#else
static inline void omap_hdq_init(void) {}
#endif

/*---------------------------------------------------------------------------*/
#ifdef CONFIG_OMAP2_DSS

#define MAX_OMAP_DSS_HWMOD_NAME_LEN 16
static const char dssname[] = "dss";
struct omap_device *od;

static struct omap_device_pm_latency omap_dss_latency[] = {
	[0] = {
		.deactivate_func	= omap_device_idle_hwmods,
		.activate_func		= omap_device_enable_hwmods,
		.flags			= OMAP_DEVICE_LATENCY_AUTO_ADJUST,
		},
};

static struct platform_device omap_display_device = {
	.name		= "omapdss",
	.id		= -1,
	.dev		= {
		.platform_data = NULL,
	},
};
void __init omap_display_init(struct omap_dss_board_info *board_data)
{
	struct omap_hwmod *oh;
	char oh_name[7][MAX_OMAP_DSS_HWMOD_NAME_LEN] = {"dss", "dss_dispc", "dss_dsi1", "dss_dsi2", "dss_hdmi", "dss_rfbi", "dss_venc"};
	int l, idx, i;
	struct omap_display_platform_data pdata;
	idx = 1;

	for (i = 0; i < 7; i++)	{
		l = snprintf(oh_name[i], MAX_OMAP_DSS_HWMOD_NAME_LEN, oh_name[i]);
		WARN(l >= MAX_OMAP_DSS_HWMOD_NAME_LEN,
		"String buffer overflow in DSS device setup\n");

		oh = omap_hwmod_lookup(oh_name[i]);

		if (!oh) {
			pr_err("Could not look up %s\n", oh_name[i]);
			return;
		}

		strcpy(pdata.name, oh_name[i]);
		pdata.hwmod_count	= 2;
		pdata.board_data	= board_data;
		pdata.board_data->get_last_off_on_transaction_id = NULL;

		pdata.device_enable	= omap_device_enable;
		pdata.device_idle	= omap_device_idle;
		pdata.device_shutdown	= omap_device_shutdown;
		od = omap_device_build(oh_name[i], -1, oh, &pdata,
			sizeof(struct omap_display_platform_data),
			omap_dss_latency,
			ARRAY_SIZE(omap_dss_latency), 0);
		WARN((IS_ERR(od)), "Could not build omap_device for %s %s\n",
			dssname, oh_name[i]);
	}
	omap_display_device.dev.platform_data = board_data;
	if (platform_device_register(&omap_display_device) < 0)
		printk(KERN_ERR "Unable to register OMAP-Display device\n");

	return;
}
#else
void __init omap_display_init(struct omap_dss_board_info *board_data)
{
}
#endif

#if defined(CONFIG_VIDEO_OMAP2_VOUT) || \
	defined(CONFIG_VIDEO_OMAP2_VOUT_MODULE)
#if defined(CONFIG_FB_OMAP2) || defined(CONFIG_FB_OMAP2_MODULE)
#ifdef CONFIG_ARCH_OMAP4
static struct resource omap_vout_resource[4 - CONFIG_FB_OMAP2_NUM_FBS] = {
#else
static struct resource omap_vout_resource[3 - CONFIG_FB_OMAP2_NUM_FBS] = {
#endif
};
#else
#ifdef CONFIG_ARCH_OMAP4
static struct resource omap_vout_resource[3] = {
#else
static struct resource omap_vout_resource[2] = {
#endif
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

static struct resource sdp4430_wb_resource[1] = {
};

static struct platform_device sdp4430_wb_device = {
	.name		= "omap_wb",
	.num_resources	= ARRAY_SIZE(sdp4430_wb_resource),
	.resource	= &sdp4430_wb_resource[0],
	.id		= -1,
};

static void omap_init_wb(void)
{
		(void) platform_device_register(&sdp4430_wb_device);
}

#else
static inline void omap_init_vout(void) {}
static void omap_init_wb(void) {}
#endif

static struct omap_device_pm_latency omap_gpu_latency[] = {
	[0] = {
		.deactivate_func	= omap_device_idle_hwmods,
		.activate_func		= omap_device_enable_hwmods,
		.flags			= OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_gpu(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	int max_omap_gpu_hwmod_name_len = 16;
	char oh_name[max_omap_gpu_hwmod_name_len];
	int l;
	struct gpu_platform_data *pdata;
	char *name = "omap_gpu";

	l = snprintf(oh_name, max_omap_gpu_hwmod_name_len,
		     "gpu");
	WARN(l >= max_omap_gpu_hwmod_name_len,
		"String buffer overflow in GPU device setup\n");

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

	pdata->device_enable = omap_device_enable;
	pdata->device_idle = omap_device_idle;
	pdata->device_shutdown = omap_device_shutdown;

	od = omap_device_build(name, 0, oh, pdata,
			     sizeof(struct gpu_platform_data),
			     omap_gpu_latency, ARRAY_SIZE(omap_gpu_latency), 0);
	WARN(IS_ERR(od), "Could not build omap_device for %s %s\n",
	     name, oh_name);

	kfree(pdata);
}

/*-------------------------------------------------------------------------*/

static int __init omap2_init_devices(void)
{
	/* please keep these calls, and their implementations above,
	 * in alphabetical order so they're easier to sort through.
	 */
	omap_init_32k();
	omap_init_abe();
	omap_init_audio();
	omap_init_camera();
	omap_init_mbox();
	omap_init_mcspi();
	omap_init_pmu();
	omap_hdq_init();
	omap_init_sti();
	omap_init_sham();
	omap_init_vout();
	if (cpu_is_omap44xx())
		omap_init_wb();
	omap_init_gpu();
	return 0;
}
arch_initcall(omap2_init_devices);
