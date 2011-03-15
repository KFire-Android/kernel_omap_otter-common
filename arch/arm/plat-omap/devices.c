/*
 * linux/arch/arm/plat-omap/devices.c
 *
 * Common platform device setup/initialization for OMAP1 and OMAP2
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
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/i2c/twl.h>
#include <linux/bootmem.h>

#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <asm/mach-types.h>
#include <asm/mach/map.h>

#include <plat/tc.h>
#include <plat/control.h>
#include <plat/board.h>
#include <plat/mmc.h>
#include <plat/mux.h>
#include <mach/gpio.h>
#include <plat/menelaus.h>
#include <plat/mcbsp.h>
#include <plat/mcpdm.h>
#include <plat/dsp_common.h>
#include <plat/omap44xx.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <plat/omap-pm.h>
#include <sound/omap-abe-dsp.h>

#if	defined(CONFIG_OMAP_DSP) || defined(CONFIG_OMAP_DSP_MODULE)

static struct dsp_platform_data dsp_pdata = {
	.kdev_list = LIST_HEAD_INIT(dsp_pdata.kdev_list),
};

static struct resource omap_dsp_resources[] = {
	{
		.name	= "dsp_mmu",
		.start	= -1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device omap_dsp_device = {
	.name		= "dsp",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(omap_dsp_resources),
	.resource	= omap_dsp_resources,
	.dev = {
		.platform_data = &dsp_pdata,
	},
};

static inline void omap_init_dsp(void)
{
	struct resource *res;
	int irq;

	if (cpu_is_omap15xx())
		irq = INT_1510_DSP_MMU;
	else if (cpu_is_omap16xx())
		irq = INT_1610_DSP_MMU;
	else if (cpu_is_omap24xx())
		irq = INT_24XX_DSP_MMU;

	res = platform_get_resource_byname(&omap_dsp_device,
					   IORESOURCE_IRQ, "dsp_mmu");
	res->start = irq;

	platform_device_register(&omap_dsp_device);
}

int dsp_kfunc_device_register(struct dsp_kfunc_device *kdev)
{
	static DEFINE_MUTEX(dsp_pdata_lock);

	spin_lock_init(&kdev->lock);

	mutex_lock(&dsp_pdata_lock);
	list_add_tail(&kdev->entry, &dsp_pdata.kdev_list);
	mutex_unlock(&dsp_pdata_lock);

	return 0;
}
EXPORT_SYMBOL(dsp_kfunc_device_register);

#else
static inline void omap_init_dsp(void) { }
#endif	/* CONFIG_OMAP_DSP */

#if defined(CONFIG_MPU_BRIDGE) ||  defined(CONFIG_MPU_BRIDGE_MODULE)

static unsigned long dspbridge_phys_mempool_base;

void dspbridge_reserve_sdram(void)
{
	void *va;
	unsigned long size = CONFIG_BRIDGE_MEMPOOL_SIZE;

	if (!size)
		return;

	va = __alloc_bootmem_nopanic(size, SZ_1M, 0);
	if (!va) {
		pr_err("%s: Failed to bootmem allocation(%lu bytes)\n",
		       __func__, size);
		return;
	}
	dspbridge_phys_mempool_base = virt_to_phys(va);
}

unsigned long dspbridge_get_mempool_base(void)
{
	return dspbridge_phys_mempool_base;
}
EXPORT_SYMBOL(dspbridge_get_mempool_base);
#endif
/*-------------------------------------------------------------------------*/
#if	defined(CONFIG_KEYBOARD_OMAP) || defined(CONFIG_KEYBOARD_OMAP_MODULE)

static void omap_init_kp(void)
{
	/* 2430 and 34xx keypad is on TWL4030 */
	if (cpu_is_omap2430() || cpu_is_omap34xx())
		return;

	if (machine_is_omap_h2() || machine_is_omap_h3()) {
		omap_cfg_reg(F18_1610_KBC0);
		omap_cfg_reg(D20_1610_KBC1);
		omap_cfg_reg(D19_1610_KBC2);
		omap_cfg_reg(E18_1610_KBC3);
		omap_cfg_reg(C21_1610_KBC4);

		omap_cfg_reg(G18_1610_KBR0);
		omap_cfg_reg(F19_1610_KBR1);
		omap_cfg_reg(H14_1610_KBR2);
		omap_cfg_reg(E20_1610_KBR3);
		omap_cfg_reg(E19_1610_KBR4);
		omap_cfg_reg(N19_1610_KBR5);
	} else if (machine_is_omap_perseus2() || machine_is_omap_fsample()) {
		omap_cfg_reg(E2_7XX_KBR0);
		omap_cfg_reg(J7_7XX_KBR1);
		omap_cfg_reg(E1_7XX_KBR2);
		omap_cfg_reg(F3_7XX_KBR3);
		omap_cfg_reg(D2_7XX_KBR4);

		omap_cfg_reg(C2_7XX_KBC0);
		omap_cfg_reg(D3_7XX_KBC1);
		omap_cfg_reg(E4_7XX_KBC2);
		omap_cfg_reg(F4_7XX_KBC3);
		omap_cfg_reg(E3_7XX_KBC4);
	} else if (machine_is_omap_h4()) {
		omap_cfg_reg(T19_24XX_KBR0);
		omap_cfg_reg(R19_24XX_KBR1);
		omap_cfg_reg(V18_24XX_KBR2);
		omap_cfg_reg(M21_24XX_KBR3);
		omap_cfg_reg(E5__24XX_KBR4);
		if (omap_has_menelaus()) {
			omap_cfg_reg(B3__24XX_KBR5);
			omap_cfg_reg(AA4_24XX_KBC2);
			omap_cfg_reg(B13_24XX_KBC6);
		} else {
			omap_cfg_reg(M18_24XX_KBR5);
			omap_cfg_reg(H19_24XX_KBC2);
			omap_cfg_reg(N19_24XX_KBC6);
		}
		omap_cfg_reg(R20_24XX_KBC0);
		omap_cfg_reg(M14_24XX_KBC1);
		omap_cfg_reg(V17_24XX_KBC3);
		omap_cfg_reg(P21_24XX_KBC4);
		omap_cfg_reg(L14_24XX_KBC5);
	}
}
#else
static inline void omap_init_kp(void) {}
#endif

/*-------------------------------------------------------------------------*/
#if defined(CONFIG_OMAP_MCBSP) || defined(CONFIG_OMAP_MCBSP_MODULE)

static struct platform_device **omap_mcbsp_devices;

void omap_mcbsp_register_board_cfg(struct omap_mcbsp_platform_data *config,
					int size)
{
	int i;

	omap_mcbsp_devices = kzalloc(size * sizeof(struct platform_device *),
				     GFP_KERNEL);
	if (!omap_mcbsp_devices) {
		printk(KERN_ERR "Could not register McBSP devices\n");
		return;
	}

	for (i = 0; i < size; i++) {
		struct platform_device *new_mcbsp;
		int ret;

		new_mcbsp = platform_device_alloc("omap-mcbsp", i + 1);
		if (!new_mcbsp)
			continue;
		new_mcbsp->dev.platform_data = &config[i];
		ret = platform_device_add(new_mcbsp);
		if (ret) {
			platform_device_put(new_mcbsp);
			continue;
		}
		omap_mcbsp_devices[i] = new_mcbsp;
	}
}

#else
void omap_mcbsp_register_board_cfg(struct omap_mcbsp_platform_data *config,
					int size)
{  }
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_SND_OMAP_SOC_DMIC) || \
    defined(CONFIG_SND_OMAP_SOC_DMIC_MODULE)

static struct omap_device_pm_latency omap_dmic_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_dmic(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;

	oh = omap_hwmod_lookup("dmic");
	if (!oh) {
		printk(KERN_ERR "Could not look up dmic hw_mod\n");
		return;
	}

	od = omap_device_build("omap-dmic-dai", -1, oh, NULL, 0,
				omap_dmic_latency,
				ARRAY_SIZE(omap_dmic_latency), 0);
	if (IS_ERR(od))
		printk(KERN_ERR "Could not build omap_device for omap-dmic-dai\n");
}
#else
static inline void omap_init_dmic(void) {}
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_SND_OMAP_SOC_MCPDM) || defined(CONFIG_SND_OMAP_SOC_MCPDM_MODULE)

static struct omap_device_pm_latency omap_mcpdm_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_mcpdm(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	struct omap_mcpdm_platform_data *pdata;

	oh = omap_hwmod_lookup("omap-mcpdm-dai");
	if (!oh)
		printk(KERN_ERR "Could not look up mcpdm hw_mod\n");

	pdata = kzalloc(sizeof(struct omap_mcpdm_platform_data), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "Could not allocate platform data\n");
		return;
	}
	pdata->device_enable = omap_device_enable;
	pdata->device_idle = omap_device_idle;
	pdata->device_shutdown = omap_device_shutdown;

	od = omap_device_build("omap-mcpdm-dai", -1, oh, pdata,
				sizeof(struct omap_mcpdm_platform_data),
				omap_mcpdm_latency,
				ARRAY_SIZE(omap_mcpdm_latency), 0);

	if (od <= 0)
		printk(KERN_ERR "Could not build omap_device for omap-mcpdm-dai\n");
}
#else
static inline void omap_init_mcpdm(void) {}
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_MMC_OMAP) || defined(CONFIG_MMC_OMAP_MODULE) || \
	defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)

#define OMAP_MMC_NR_RES		2

/*
 * Register MMC devices. Called from mach-omap1 and mach-omap2 device init.
 */
int __init omap_mmc_add(const char *name, int id, unsigned long base,
				unsigned long size, unsigned int irq,
				struct omap_mmc_platform_data *data)
{
	struct platform_device *pdev;
	struct resource res[OMAP_MMC_NR_RES];
	int ret;

	pdev = platform_device_alloc(name, id);
	if (!pdev)
		return -ENOMEM;

	memset(res, 0, OMAP_MMC_NR_RES * sizeof(struct resource));
	res[0].start = base;
	res[0].end = base + size - 1;
	res[0].flags = IORESOURCE_MEM;
	res[1].start = res[1].end = irq;
	res[1].flags = IORESOURCE_IRQ;

	ret = platform_device_add_resources(pdev, res, ARRAY_SIZE(res));
	if (ret == 0)
		ret = platform_device_add_data(pdev, data, sizeof(*data));
	if (ret)
		goto fail;

	ret = platform_device_add(pdev);
	if (ret)
		goto fail;

	/* return device handle to board setup code */
	data->dev = &pdev->dev;
	return 0;

fail:
	platform_device_put(pdev);
	return ret;
}

#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_SND_OMAP_SOC_ABE_DSP) || \
	defined(CONFIG_SND_OMAP_SOC_ABE_DSP_MODULE)
#if 0
	/* TODO: hwmod is not ready for AESS ABE atm so just use normal pdev */

static struct resource aess_resources[] = {
	{
		.start		= L4_ABE_44XX_PHYS,
		.end		= L4_ABE_44XX_PHYS + SZ_1M,
		.flags		= IORESOURCE_MEM,
	},
	{
		.start		= OMAP44XX_IRQ_ABE,
		.flags		= IORESOURCE_IRQ,
	},
};

static struct omap4_abe_dsp_pdata aess_pdata = {
	.device_enable = omap_device_enable,
	.device_shutdown	= omap_device_shutdown,
	.device_idle	= omap_device_idle,
};

static struct platform_device omap_aess_device = {
	.name		= "omap-aess-audio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(aess_resources),
	.resource	= aess_resources,
	.dev = {
		.platform_data = &aess_pdata,
	},
};

static void omap_init_aess(void)
{
	(void) platform_device_register(&omap_aess_device);
}

#else
static struct omap_device_pm_latency omap_aess_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static void omap_init_aess(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	struct omap4_abe_dsp_pdata *pdata;

	oh = omap_hwmod_lookup("omap-aess-audio");
	if (!oh) {
		printk (KERN_ERR "Could not look up aess hw_mod\n");
		return;
	}

	pdata = kzalloc(sizeof(struct omap4_abe_dsp_pdata), GFP_KERNEL);
	if (!pdata) {
		printk(KERN_ERR "Could not allocate platform data\n");
		return;
	}

	pdata->get_context_loss_count = omap_pm_get_dev_context_loss_count;
	pdata->enter_dpll_cascade = omap4_dpll_low_power_cascade_check_entry;
	pdata->exit_dpll_cascade = omap4_dpll_low_power_cascade_exit;

	od = omap_device_build("omap-aess-audio", -1, oh, pdata,
				sizeof(struct omap4_abe_dsp_pdata),
				omap_aess_latency,
				ARRAY_SIZE(omap_aess_latency), 0);

	kfree(pdata);

	if (IS_ERR(od))
		printk(KERN_ERR "Could not build omap_device for omap-aess-audio\n");
}
#endif
#else
static inline void omap_init_aess(void) {}
#endif

/*-------------------------------------------------------------------------*/

#if defined(CONFIG_HW_RANDOM_OMAP) || defined(CONFIG_HW_RANDOM_OMAP_MODULE)

#ifdef CONFIG_ARCH_OMAP2
#define	OMAP_RNG_BASE		0x480A0000
#else
#define	OMAP_RNG_BASE		0xfffe5000
#endif

static struct resource rng_resources[] = {
	{
		.start		= OMAP_RNG_BASE,
		.end		= OMAP_RNG_BASE + 0x4f,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device omap_rng_device = {
	.name		= "omap_rng",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rng_resources),
	.resource	= rng_resources,
};

static void omap_init_rng(void)
{
	(void) platform_device_register(&omap_rng_device);
}
#else
static inline void omap_init_rng(void) {}
#endif

/*-------------------------------------------------------------------------*/

/* Numbering for the SPI-capable controllers when used for SPI:
 * spi		= 1
 * uwire	= 2
 * mmc1..2	= 3..4
 * mcbsp1..3	= 5..7
 */

#if defined(CONFIG_SPI_OMAP_UWIRE) || defined(CONFIG_SPI_OMAP_UWIRE_MODULE)

#define	OMAP_UWIRE_BASE		0xfffb3000

static struct resource uwire_resources[] = {
	{
		.start		= OMAP_UWIRE_BASE,
		.end		= OMAP_UWIRE_BASE + 0x20,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device omap_uwire_device = {
	.name	   = "omap_uwire",
	.id	     = -1,
	.num_resources	= ARRAY_SIZE(uwire_resources),
	.resource	= uwire_resources,
};

static void omap_init_uwire(void)
{
	/* FIXME define and use a boot tag; not all boards will be hooking
	 * up devices to the microwire controller, and multi-board configs
	 * mean that CONFIG_SPI_OMAP_UWIRE may be configured anyway...
	 */

	/* board-specific code must configure chipselects (only a few
	 * are normally used) and SCLK/SDI/SDO (each has two choices).
	 */
	(void) platform_device_register(&omap_uwire_device);
}
#else
static inline void omap_init_uwire(void) {}
#endif

/*-------------------------------------------------------------------------*/

#if	defined(CONFIG_OMAP_WATCHDOG) || defined(CONFIG_OMAP_WATCHDOG_MODULE)

static struct resource wdt_resources[] = {
	{
		.start		= 0xfffeb000,
		.end		= 0xfffeb07F,
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device omap_wdt_device = {
	.name	   = "omap_wdt",
	.id	     = -1,
	.num_resources	= ARRAY_SIZE(wdt_resources),
	.resource	= wdt_resources,
};

struct omap_device_pm_latency omap_wdt_latency[] = {
	[0] = {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func   = omap_device_enable_hwmods,
		.flags		 = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	},
};

static int omap2_init_wdt(struct omap_hwmod *oh, void *user)
{
	int id = -1;
	struct omap_device *od;
	char *name = "omap_wdt";

	if (!oh)
		pr_err("Could not look up wdtimer2_hwmod\n");

	od = omap_device_build(name, id, oh, NULL, 0,
				omap_wdt_latency,
				ARRAY_SIZE(omap_wdt_latency), 0);
	WARN(IS_ERR(od), "Cant build omap_device for %s:%s.\n",
				name, oh->name);
	return 0;
}

static void omap_init_wdt(void)
{
	if (cpu_class_is_omap2())
		omap_hwmod_for_each_by_class("wd_timer", omap2_init_wdt,
						NULL);
	else if (cpu_is_omap16xx())
		(void) platform_device_register(&omap_wdt_device);
	return;
}
#else
static inline void omap_init_wdt(void) {}
#endif

/*
 * This gets called after board-specific INIT_MACHINE, and initializes most
 * on-chip peripherals accessible on this board (except for few like USB):
 *
 *  (a) Does any "standard config" pin muxing needed.  Board-specific
 *	code will have muxed GPIO pins and done "nonstandard" setup;
 *	that code could live in the boot loader.
 *  (b) Populating board-specific platform_data with the data drivers
 *	rely on to handle wiring variations.
 *  (c) Creating platform devices as meaningful on this board and
 *	with this kernel configuration.
 *
 * Claiming GPIOs, and setting their direction and initial values, is the
 * responsibility of the device drivers.  So is responding to probe().
 *
 * Board-specific knowlege like creating devices or pin setup is to be
 * kept out of drivers as much as possible.  In particular, pin setup
 * may be handled by the boot loader, and drivers should expect it will
 * normally have been done by the time they're probed.
 */
static int __init omap_init_devices(void)
{
	/* please keep these calls, and their implementations above,
	 * in alphabetical order so they're easier to sort through.
	 */
	omap_init_aess();
	omap_init_dmic();
	omap_init_dsp();
	omap_init_kp();
	omap_init_rng();
	omap_init_mcpdm();
	omap_init_uwire();
	omap_init_wdt();
	return 0;
}
arch_initcall(omap_init_devices);
