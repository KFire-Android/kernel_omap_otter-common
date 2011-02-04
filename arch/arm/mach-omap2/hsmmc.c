/*
 * linux/arch/arm/mach-omap2/hsmmc.c
 *
 * Copyright (C) 2007-2008 Texas Instruments
 * Copyright (C) 2008 Nokia Corporation
 * Author: Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <plat/control.h>
#include <plat/mmc.h>
#include <plat/omap-pm.h>
#include <plat/omap_device.h>

#ifdef CONFIG_TIWLAN_SDIO
#include <linux/mmc/sdio_ids.h>
#include <linux/mmc/sdio_func.h>
#endif

#include "hsmmc.h"

#if defined(CONFIG_MMC_OMAP_HS) || defined(CONFIG_MMC_OMAP_HS_MODULE)

static u16 control_pbias_offset;
static u16 control_devconf1_offset;
static u16 control_mmc1;

#define HSMMC_NAME_LEN	9

static struct hsmmc_controller {
	char				name[HSMMC_NAME_LEN + 1];
} hsmmc[OMAP44XX_NR_MMC];

#if defined(CONFIG_ARCH_OMAP3) || defined(CONFIG_ARCH_OMAP4) && defined(CONFIG_PM)

static int hsmmc_get_context_loss(struct device *dev)
{
	return omap_pm_get_dev_context_loss_count(dev);
}

#else
#define hsmmc_get_context_loss NULL
#endif

static void omap_hsmmc1_before_set_reg(struct device *dev, int slot,
				  int power_on, int vdd)
{
	u32 reg, prog_io;
	struct omap_mmc_platform_data *mmc = dev->platform_data;

	if (mmc->slots[0].remux)
		mmc->slots[0].remux(dev, slot, power_on);

	/*
	 * Assume we power both OMAP VMMC1 (for CMD, CLK, DAT0..3) and the
	 * card with Vcc regulator (from twl4030 or whatever).  OMAP has both
	 * 1.8V and 3.0V modes, controlled by the PBIAS register.
	 *
	 * In 8-bit modes, OMAP VMMC1A (for DAT4..7) needs a supply, which
	 * is most naturally TWL VSIM; those pins also use PBIAS.
	 *
	 * FIXME handle VMMC1A as needed ...
	 */
	if (power_on) {
		if (cpu_is_omap2430()) {
			reg = omap_ctrl_readl(OMAP243X_CONTROL_DEVCONF1);
			if ((1 << vdd) >= MMC_VDD_30_31)
				reg |= OMAP243X_MMC1_ACTIVE_OVERWRITE;
			else
				reg &= ~OMAP243X_MMC1_ACTIVE_OVERWRITE;
			omap_ctrl_writel(reg, OMAP243X_CONTROL_DEVCONF1);
		}

		if (mmc->slots[0].internal_clock) {
			reg = omap_ctrl_readl(OMAP2_CONTROL_DEVCONF0);
			reg |= OMAP2_MMCSDIO1ADPCLKISEL;
			omap_ctrl_writel(reg, OMAP2_CONTROL_DEVCONF0);
		}

		reg = omap_ctrl_readl(control_pbias_offset);
		if (cpu_is_omap3630()) {
			/* Set MMC I/O to 52Mhz */
			prog_io = omap_ctrl_readl(OMAP343X_CONTROL_PROG_IO1);
			prog_io |= OMAP3630_PRG_SDMMC1_SPEEDCTRL;
			omap_ctrl_writel(prog_io, OMAP343X_CONTROL_PROG_IO1);
		} else {
			reg |= OMAP2_PBIASSPEEDCTRL0;
		}
		reg &= ~OMAP2_PBIASLITEPWRDNZ0;
		omap_ctrl_writel(reg, control_pbias_offset);
	} else {
		reg = omap_ctrl_readl(control_pbias_offset);
		reg &= ~OMAP2_PBIASLITEPWRDNZ0;
		omap_ctrl_writel(reg, control_pbias_offset);
	}
}

static void omap_hsmmc1_after_set_reg(struct device *dev, int slot,
				 int power_on, int vdd)
{
	u32 reg;

	/* 100ms delay required for PBIAS configuration */
	msleep(100);

	if (power_on) {
		reg = omap_ctrl_readl(control_pbias_offset);
		reg |= (OMAP2_PBIASLITEPWRDNZ0 | OMAP2_PBIASSPEEDCTRL0);
		if ((1 << vdd) <= MMC_VDD_165_195)
			reg &= ~OMAP2_PBIASLITEVMODE0;
		else
			reg |= OMAP2_PBIASLITEVMODE0;
		omap_ctrl_writel(reg, control_pbias_offset);
	} else {
		reg = omap_ctrl_readl(control_pbias_offset);
		reg |= (OMAP2_PBIASSPEEDCTRL0 | OMAP2_PBIASLITEPWRDNZ0 |
			OMAP2_PBIASLITEVMODE0);
		omap_ctrl_writel(reg, control_pbias_offset);
	}
}

static void omap4_hsmmc1_before_set_reg(struct device *dev, int slot,
				  int power_on, int vdd)
{
	u32 reg;

	/*
	 * Assume we power both OMAP VMMC1 (for CMD, CLK, DAT0..3) and the
	 * card with Vcc regulator (from twl4030 or whatever).  OMAP has both
	 * 1.8V and 3.0V modes, controlled by the PBIAS register.
	 *
	 * In 8-bit modes, OMAP VMMC1A (for DAT4..7) needs a supply, which
	 * is most naturally TWL VSIM; those pins also use PBIAS.
	 *
	 * FIXME handle VMMC1A as needed ...
	 */
	reg = omap4_ctrl_pad_readl(control_pbias_offset);
	reg &= ~(OMAP4_MMC1_PBIASLITE_PWRDNZ_MASK |
		OMAP4_MMC1_PWRDNZ_MASK |
		OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
	omap4_ctrl_pad_writel(reg, control_pbias_offset);
}

static void omap4_hsmmc1_after_set_reg(struct device *dev, int slot,
				 int power_on, int vdd)
{
	u32 reg;

	if (power_on) {
		reg = omap4_ctrl_pad_readl(control_pbias_offset);
		reg |= OMAP4_MMC1_PBIASLITE_PWRDNZ_MASK;
		if ((1 << vdd) <= MMC_VDD_165_195)
			reg &= ~OMAP4_MMC1_PBIASLITE_VMODE_MASK;
		else
			reg |= OMAP4_MMC1_PBIASLITE_VMODE_MASK;
		reg |= (OMAP4_MMC1_PBIASLITE_PWRDNZ_MASK |
			OMAP4_MMC1_PWRDNZ_MASK |
			OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
		omap4_ctrl_pad_writel(reg, control_pbias_offset);
		/* 4 microsec delay for comparator to generate an error*/
		udelay(4);
		reg = omap4_ctrl_pad_readl(control_pbias_offset);
		if (reg & OMAP4_MMC1_PBIASLITE_VMODE_ERROR_MASK) {
			pr_err("Pbias Voltage is not same as LDO\n");
			/* Caution : On VMODE_ERROR Power Down MMC IO */
			reg &= ~(OMAP4_MMC1_PWRDNZ_MASK |
				OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
			omap4_ctrl_pad_writel(reg, control_pbias_offset);
		}
	} else {
		reg = omap4_ctrl_pad_readl(control_pbias_offset);
		reg |= (OMAP4_MMC1_PBIASLITE_PWRDNZ_MASK |
			OMAP4_MMC1_PWRDNZ_MASK |
			OMAP4_MMC1_PBIASLITE_VMODE_MASK |
			OMAP4_USBC1_ICUSB_PWRDNZ_MASK);
		omap4_ctrl_pad_writel(reg, control_pbias_offset);
	}
}

static void hsmmc23_before_set_reg(struct device *dev, int slot,
				   int power_on, int vdd)
{
	struct omap_mmc_platform_data *mmc = dev->platform_data;

	if (mmc->slots[0].remux)
		mmc->slots[0].remux(dev, slot, power_on);

	if (power_on) {
		/* Only MMC2 supports a CLKIN */
		if (mmc->slots[0].internal_clock) {
			u32 reg;

			reg = omap_ctrl_readl(control_devconf1_offset);
			reg |= OMAP2_MMCSDIO2ADPCLKISEL;
			omap_ctrl_writel(reg, control_devconf1_offset);
		}
	}
}

static int nop_mmc_set_power(struct device *dev, int slot, int power_on,
							int vdd)
{
	return 0;
}

static struct omap_mmc_platform_data *hsmmc_data[OMAP44XX_NR_MMC] __initdata;

#ifdef CONFIG_TIWLAN_SDIO
static struct sdio_embedded_func wifi_func_array[] = {
	{
		.f_class        = SDIO_CLASS_NONE,
		.f_maxblksize   = 512,
	},
	{
		.f_class        = SDIO_CLASS_WLAN,
		.f_maxblksize   = 512,
	},
};

static struct embedded_sdio_data omap_wifi_emb_data = {
	.cis    = {
		.vendor         = SDIO_VENDOR_ID_TI,
		.device         = SDIO_DEVICE_ID_TI_WL12xx,
		.blksize        = 512,
		.max_dtr        = 48000000,
	},
	.cccr   = {
		.multi_block	= 1,
		.low_speed	= 0,
		.wide_bus	= 1,
		.high_power	= 0,
		.high_speed	= 1,
		.disable_cd	= 1,
	},
	.funcs  = wifi_func_array,
	.quirks = MMC_QUIRK_VDD_165_195 | MMC_QUIRK_LENIENT_FUNC0,
};
#endif

void __init omap2_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;
	int nr_hsmmc = ARRAY_SIZE(hsmmc_data);
	int i;
	u32 reg;
	int controller_cnt = 0;

	if (!cpu_is_omap44xx()) {
		if (cpu_is_omap2430()) {
			control_pbias_offset = OMAP243X_CONTROL_PBIAS_LITE;
			control_devconf1_offset = OMAP243X_CONTROL_DEVCONF1;
		} else {
			control_pbias_offset = OMAP343X_CONTROL_PBIAS_LITE;
			control_devconf1_offset = OMAP343X_CONTROL_DEVCONF1;
		}
	} else {
		control_pbias_offset =
			OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE;
		control_mmc1 = OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_MMC1;
		reg = omap4_ctrl_pad_readl(control_mmc1);
		reg |= (OMAP4_SDMMC1_PUSTRENGTH_GRP0_MASK |
			OMAP4_SDMMC1_PUSTRENGTH_GRP1_MASK);
		reg &= ~(OMAP4_SDMMC1_PUSTRENGTH_GRP2_MASK |
			OMAP4_SDMMC1_PUSTRENGTH_GRP3_MASK);
		reg |= (OMAP4_USBC1_DR0_SPEEDCTRL_MASK|
			OMAP4_SDMMC1_DR1_SPEEDCTRL_MASK |
			OMAP4_SDMMC1_DR2_SPEEDCTRL_MASK);
		omap4_ctrl_pad_writel(reg, control_mmc1);
	}

	for (c = controllers; c->mmc; c++) {
		struct hsmmc_controller *hc = hsmmc + controller_cnt;
		struct omap_mmc_platform_data *mmc = hsmmc_data[controller_cnt];
		if (!c->mmc || c->mmc > nr_hsmmc) {
			pr_debug("MMC%d: no such controller\n", c->mmc);
			continue;
		}
		if (mmc) {
			pr_debug("MMC%d: already configured\n", c->mmc);
			continue;
		}

		mmc = kzalloc(sizeof(struct omap_mmc_platform_data),
			      GFP_KERNEL);
		if (!mmc) {
			pr_err("Cannot allocate memory for mmc device!\n");
			goto done;
		}

		if (c->name)
			strncpy(hc->name, c->name, HSMMC_NAME_LEN);
		else
			snprintf(hc->name, ARRAY_SIZE(hc->name),
				"mmc%islot%i", c->mmc, 1);

#ifdef CONFIG_TIWLAN_SDIO
		if (c->mmc == CONFIG_TIWLAN_MMC_CONTROLLER) {
			mmc->slots[0].embedded_sdio = &omap_wifi_emb_data;
			mmc->slots[0].register_status_notify =
				&omap_wifi_status_register;
			mmc->slots[0].card_detect = &omap_wifi_status;
		}
#endif

		mmc->slots[0].name = hc->name;
		mmc->nr_slots = 1;
		mmc->slots[0].caps = c->caps;
		mmc->slots[0].internal_clock = !c->ext_clock;
		mmc->dma_mask = 0xffffffff;

		/* Register offset Mapping */
		if (cpu_is_omap44xx())
			mmc->regs_map = (u16 *) omap4_mmc_reg_map;
		else
			mmc->regs_map = (u16 *) omap3_mmc_reg_map;

		if (!cpu_is_omap44xx())
			mmc->get_context_loss_count = hsmmc_get_context_loss;

		mmc->slots[0].switch_pin = c->gpio_cd;
		mmc->slots[0].gpio_wp = c->gpio_wp;

		mmc->slots[0].remux = c->remux;

		if (c->cover_only)
			mmc->slots[0].cover = 1;

		if (c->nonremovable)
			mmc->slots[0].nonremovable = 1;

		if (c->power_saving)
			mmc->slots[0].power_saving = 1;

		if (c->no_off)
			mmc->slots[0].no_off = 1;

		if (c->vcc_aux_disable_is_sleep)
			mmc->slots[0].vcc_aux_disable_is_sleep = 1;

		/* NOTE:  MMC slots should have a Vcc regulator set up.
		 * This may be from a TWL4030-family chip, another
		 * controllable regulator, or a fixed supply.
		 *
		 * temporary HACK: ocr_mask instead of fixed supply
		 */
		mmc->slots[0].ocr_mask = c->ocr_mask;

		if (cpu_is_omap3517() || cpu_is_omap3505())
			mmc->slots[0].set_power = nop_mmc_set_power;
		else
			mmc->slots[0].features |= HSMMC_HAS_PBIAS;

		if (cpu_is_omap44xx()) {
			if (omap_rev() > OMAP4430_REV_ES1_0)
				mmc->slots[0].features |= HSMMC_HAS_UPDATED_RESET;

			mmc->slots[0].features |= HSMMC_DVFS_24MHZ_CONST;

			if (c->mmc >= 3 && c->mmc <= 5) {
				mmc->slots[0].features |= HSMMC_HAS_48MHZ_MASTER_CLK;
				mmc->get_context_loss_count =
						hsmmc_get_context_loss;
			}
		}
		switch (c->mmc) {
		case 1:
			if (mmc->slots[0].features & HSMMC_HAS_PBIAS) {
				/* on-chip level shifting via PBIAS0/PBIAS1 */
				if (cpu_is_omap44xx()) {
					mmc->slots[0].before_set_reg =
						omap4_hsmmc1_before_set_reg;
					mmc->slots[0].after_set_reg =
						omap4_hsmmc1_after_set_reg;
				} else {
					mmc->slots[0].before_set_reg =
						omap_hsmmc1_before_set_reg;
					mmc->slots[0].after_set_reg =
						omap_hsmmc1_after_set_reg;
				}
			}

			/* Omap3630 HSMMC1 supports only 4-bit */
			if (cpu_is_omap3630() &&
					(c->caps & MMC_CAP_8_BIT_DATA)) {
				c->caps &= ~MMC_CAP_8_BIT_DATA;
				c->caps |= MMC_CAP_4_BIT_DATA;
				mmc->slots[0].caps = c->caps;
			}
			break;
		case 2:
			if (c->ext_clock)
				c->transceiver = 1;
			if (c->transceiver && (c->caps & MMC_CAP_8_BIT_DATA)) {
				c->caps &= ~MMC_CAP_8_BIT_DATA;
				c->caps |= MMC_CAP_4_BIT_DATA;
			}
			/* FALLTHROUGH */
		case 3:
			if (mmc->slots[0].features & HSMMC_HAS_PBIAS) {
				/* off-chip level shifting, or none */
				mmc->slots[0].before_set_reg =
						hsmmc23_before_set_reg;
				mmc->slots[0].after_set_reg = NULL;
			}
#ifdef CONFIG_TIWLAN_SDIO
			mmc->slots[0].ocr_mask  = MMC_VDD_165_195;
#endif
			break;
		case 4:
		case 5:
			/* TODO Update required */
			mmc->slots[0].before_set_reg = NULL;
			mmc->slots[0].after_set_reg = NULL;
#ifdef CONFIG_TIWLAN_SDIO
			mmc->slots[0].ocr_mask  = MMC_VDD_165_195;
#endif
			break;
		default:
			pr_err("MMC%d configuration not supported!\n", c->mmc);
			kfree(mmc);
			continue;
		}
		hsmmc_data[controller_cnt] = mmc;
		omap2_init_mmc(hsmmc_data[controller_cnt], c->mmc);
		controller_cnt++;
	}


	/* pass the device nodes back to board setup code */
	controller_cnt = 0;
	for (c = controllers; c->mmc; c++) {
		struct omap_mmc_platform_data *mmc = hsmmc_data[controller_cnt];

		if (!c->mmc || c->mmc > nr_hsmmc)
			continue;
		c->dev = mmc->dev;
		controller_cnt++;
	}

done:
	for (i = 0; i < controller_cnt; i++)
		kfree(hsmmc_data[i]);
}

#endif
