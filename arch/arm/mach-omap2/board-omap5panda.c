/*
 * Board support file for OMAP5430 based EVM.
 *
 * Copyright (C) 2010-2011 Texas Instruments
 * Author: Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/i2c-gpio.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#ifdef CONFIG_OMAP5_SEVM_PALMAS
#include <linux/mfd/palmas.h>
#endif
#include <linux/i2c/pca953x.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/twl6040.h>
#include <linux/platform_data/omap-abe-twl6040.h>
#include <linux/omapfb.h>
#include <linux/gpio_keys.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include "common.h"
#include <asm/hardware/gic.h>
#include <plat/common.h>
#include <plat/mmc.h>
#include <plat/drm.h>
#include <plat/usb.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/remoteproc.h>
#include <plat/rpmsg_resmgr.h>
#include <plat/vram.h>
#include "hsmmc.h"
#include "common-board-devices.h"
#include "mux.h"
#include "omap5_ion.h"
#include "board-54xx-sevm.h"

#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>

#include <drm/drm_edid.h>

#include <plat/sgx_omaplfb.h>
#define OMAP5_SEVM_FB_RAM_SIZE       (SZ_16M + SZ_4M) /* 1280×800*4 * 2 */

#define GPIO_ETH_NRESET		15	/* USBB3 to SMSC LAN9730 */
#define GPIO_HUB_NRESET		80	/* USBB2 to SMSC 3530 HUB */
#define GPIO_POWER_BUTTON	83
#define GPIO_EXT_INT_PIN	99
#define GPIO_SDCARD_DETECT	152
#define HDMI_GPIO_HPD		193
#define HDMI_GPIO_CT_CP_HPD	256
#define HDMI_GPIO_LS_OE		257

static struct gpio_led panda5_gpio_leds[] = {
	{
		.name			= "blue",
		.default_trigger	= "timer",
		.gpio			= OMAP_MPUIO(19),
	},
	{
		.name			= "red",
		.default_trigger	= "timer",
		.gpio			= OMAP_MPUIO(17),
	},
	{
		.name			= "green",
		.default_trigger	= "timer",
		.gpio			= OMAP_MPUIO(18),
	},
	{
		.name			= "panda5::status1",
		.default_trigger	= "heartbeat",
		.gpio			= OMAP_MPUIO(2),
	},
	{
		.name			= "panda5::status2",
		.default_trigger	= "mmc0",
		.gpio			= OMAP_MPUIO(3),
	},
	{
		.name			= "panda5::status3",
		.default_trigger	= "none",
		.gpio			= OMAP_MPUIO(4),
	},
	{
		.name			= "panda5::status4",
		.default_trigger	= "none",
		.gpio			= OMAP_MPUIO(5),
	},
	{
		.name			= "panda5::status5",
		.default_trigger	= "none",
		.gpio			= OMAP_MPUIO(6),
	},
};

static struct gpio_led_platform_data panda5_led_data = {
	.leds	= panda5_gpio_leds,
	.num_leds = ARRAY_SIZE(panda5_gpio_leds),
};

static struct platform_device panda5_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &panda5_led_data,
	},
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP5_MUX(FREF_CLK1_OUT, OMAP_PIN_INPUT_PULLUP),
	OMAP5_MUX(USBB2_HSIC_STROBE, OMAP_PIN_INPUT | OMAP_MUX_MODE0),
	OMAP5_MUX(USBB2_HSIC_DATA, OMAP_PIN_INPUT | OMAP_MUX_MODE0),
	OMAP5_MUX(ABEDMIC_DIN3,
		OMAP_PIN_INPUT_PULLUP | OMAP_PIN_OFF_NONE | OMAP_MUX_MODE6),
	OMAP5_MUX(HSI2_ACDATA,
		OMAP_WAKEUP_EN | OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE6),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux NULL
#endif

static struct gpio_keys_button panda5_gpio_keys[] = {
	{
		.desc			= "power_button",
		.type			= EV_KEY,
		.code			= KEY_POWER,
		.gpio			= GPIO_POWER_BUTTON,
		.active_low		= 1,
		.wakeup			= 1,
		.debounce_interval	= 10,
	},
};

static struct gpio_keys_platform_data panda5_gpio_keys_data = {
	.buttons	= panda5_gpio_keys,
	.nbuttons	= ARRAY_SIZE(panda5_gpio_keys),
};

static struct platform_device panda5_gpio_keys_device = {
	.name	= "gpio-keys",
	.dev	= {
		.platform_data	= &panda5_gpio_keys_data,
	},
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 2,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA |
					MMC_CAP_1_8V_DDR,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable	= true,
		.ocr_mask	= MMC_VDD_29_30,
		.no_off_init	= true,
	},
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_UHS_SDR12 |
					MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_DDR50,
		.gpio_cd	= GPIO_SDCARD_DETECT,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_29_30,
	},
	{
		.mmc            = 3,
		.caps           = MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.pm_caps	= MMC_PM_KEEP_POWER,
		.gpio_cd        = -EINVAL,
		.gpio_wp        = -EINVAL,
		.ocr_mask       = MMC_VDD_165_195,
		.nonremovable   = true,
	},
	{}	/* Terminator */
};


#ifdef CONFIG_OMAP5_SEVM_PALMAS
#define OMAP5_GPIO_END	0

static struct palmas_gpadc_platform_data omap5_palmas_gpadc = {
	.ch3_current = 0,
	.ch0_current = 0,
	.bat_removal = 0,
	.start_polarity = 0,
};

/* Initialisation Data for Regulators */

static struct palmas_reg_init omap5_smps12_init = {
	.warm_reset = 0,
	.roof_floor = 0,
	.mode_sleep = 0,
	.tstep = 0,
};

static struct palmas_reg_init omap5_smps45_init = {
	.warm_reset = 0,
	.roof_floor = 0,
	.mode_sleep = 0,
	.tstep = 0,
};

static struct palmas_reg_init omap5_smps6_init = {
	.warm_reset = 0,
	.roof_floor = 0,
	.mode_sleep = 1,
	.tstep = 0,
};

static struct palmas_reg_init omap5_smps7_init = {
	.warm_reset = 0,
	.roof_floor = 0,
	.mode_sleep = 1,
};

static struct palmas_reg_init omap5_smps8_init = {
	.warm_reset = 0,
	.roof_floor = 0,
	.mode_sleep = 0,
	.tstep = 0,
};

static struct palmas_reg_init omap5_smps9_init = {
	.warm_reset = 0,
	.roof_floor = 0,
	.mode_sleep = 0,
	.vsel = 0xbd,
};

static struct palmas_reg_init omap5_smps10_init = {
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo1_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo2_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo3_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo4_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo5_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo6_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo7_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo8_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldo9_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
	.no_bypass = 1,
};

static struct palmas_reg_init omap5_ldoln_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init omap5_ldousb_init = {
	.warm_reset = 0,
	.mode_sleep = 0,
};

static struct palmas_reg_init *palmas_omap_reg_init[] = {
	&omap5_smps12_init,
	NULL, /* SMPS123 not used in this configuration */
	NULL, /* SMPS3 not used in this configuration */
	&omap5_smps45_init,
	NULL, /* SMPS457 not used in this configuration */
	&omap5_smps6_init,
	&omap5_smps7_init,
	&omap5_smps8_init,
	&omap5_smps9_init,
	&omap5_smps10_init,
	&omap5_ldo1_init,
	&omap5_ldo2_init,
	&omap5_ldo3_init,
	&omap5_ldo4_init,
	&omap5_ldo5_init,
	&omap5_ldo6_init,
	&omap5_ldo7_init,
	&omap5_ldo8_init,
	&omap5_ldo9_init,
	&omap5_ldoln_init,
	&omap5_ldousb_init,

};

/* Constraints for Regulators */
static struct regulator_init_data omap5_smps12 = {
	.constraints = {
		.min_uV			= 600000,
	.max_uV			= 1310000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap5_smps45 = {
	.constraints = {
		.min_uV			= 600000,
		.max_uV			= 1310000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap5_smps6 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 1200000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply omap5_vdds1v8_main_supply[] = {
	REGULATOR_SUPPLY("vio", "1-004b"),
};

static struct regulator_init_data omap5_smps7 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_vdds1v8_main_supply),
	.consumer_supplies	= omap5_vdds1v8_main_supply,
};

static struct regulator_init_data omap5_smps8 = {
	.constraints = {
		.min_uV			= 600000,
		.max_uV			= 1310000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply omap5_adac_supply[] = {
	REGULATOR_SUPPLY("v2v1", "1-004b"),
};

static struct regulator_init_data omap5_smps9 = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_adac_supply),
	.consumer_supplies	= omap5_adac_supply,

};

static struct regulator_consumer_supply omap5_vbus_supply[] = {
	REGULATOR_SUPPLY("vbus", "1-0048"),
};

static struct regulator_init_data omap5_smps10 = {
	.constraints = {
		.min_uV			= 5000000,
		.max_uV			= 5000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_vbus_supply),
	.consumer_supplies	= omap5_vbus_supply,
};

static struct regulator_init_data omap5_ldo1 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply omap5evm_lcd_panel_supply[] = {
	REGULATOR_SUPPLY("panel_supply", "omapdss_dsi.0"),
};

static struct regulator_init_data omap5_ldo2 = {
	.constraints = {
		.min_uV			= 2900000,
		.max_uV			= 2900000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.apply_uV		= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5evm_lcd_panel_supply),
	.consumer_supplies	= omap5evm_lcd_panel_supply,
};

static struct regulator_init_data omap5_ldo3 = {
	.constraints = {
		.min_uV			= 3000000,
		.max_uV			= 3000000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap5_ldo4 = {
	.constraints = {
		.min_uV			= 2200000,
		.max_uV			= 2200000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap5_ldo5 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap5_ldo6 = {
	.constraints = {
		.min_uV			= 1500000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply omap5_dss_phy_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.0"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.1"),
	REGULATOR_SUPPLY("vdds_hdmi", "omapdss_hdmi"),
};

static struct regulator_init_data omap5_ldo7 = {
	.constraints = {
		.min_uV			= 1500000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.apply_uV		= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_dss_phy_supply),
	.consumer_supplies	= omap5_dss_phy_supply,
};

static struct regulator_init_data omap5_ldo8 = {
	.constraints = {
		.min_uV			= 1500000,
		.max_uV			= 1500000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_consumer_supply omap5_mmc1_io_supply[] = {
	REGULATOR_SUPPLY("vmmc_aux", "omap_hsmmc.0"),
};
static struct regulator_init_data omap5_ldo9 = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_mmc1_io_supply),
	.consumer_supplies	= omap5_mmc1_io_supply,
};

static struct regulator_init_data omap5_ldoln = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data omap5_ldousb = {
	.constraints = {
		.min_uV			= 3250000,
		.max_uV			= 3250000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
};

static struct regulator_init_data *palmas_omap5_reg[] = {
	&omap5_smps12,
	NULL, /* SMPS123 not used in this configuration */
	NULL, /* SMPS3 not used in this configuration */
	&omap5_smps45,
	NULL, /* SMPS457 not used in this configuration */
	&omap5_smps6,
	&omap5_smps7,
	&omap5_smps8,
	&omap5_smps9,
	&omap5_smps10,

	&omap5_ldo1,
	&omap5_ldo2,
	&omap5_ldo3,
	&omap5_ldo4,
	&omap5_ldo5,
	&omap5_ldo6,
	&omap5_ldo7,
	&omap5_ldo8,
	&omap5_ldo9,
	&omap5_ldoln,
	&omap5_ldousb,
};

static struct palmas_pmic_platform_data omap5_palmas_pmic = {
	.reg_data = palmas_omap5_reg,
	.reg_init = palmas_omap_reg_init,

	.ldo6_vibrator = 0,
};

static struct palmas_resource_platform_data omap5_palmas_resource = {
	.clk32kg_mode_sleep = 0,
	.clk32kgaudio_mode_sleep = 0,
	.regen1_mode_sleep = 0,
	.regen2_mode_sleep = 0,
	.sysen1_mode_sleep = 0,
	.sysen2_mode_sleep = 0,

	.nsleep_res = 0,
	.nsleep_smps = 0,
	.nsleep_ldo1 = 0,
	.nsleep_ldo2 = 0,

	.enable1_res = 0,
	.enable1_smps = 0,
	.enable1_ldo1 = 0,
	.enable1_ldo2 = 0,

	.enable2_res = 0,
	.enable2_smps = 0,
	.enable2_ldo1 = 0,
	.enable2_ldo2 = 0,
};

static struct palmas_usb_platform_data omap5_palmas_usb = {
	.wakeup = 1,
};

static struct palmas_platform_data palmas_omap5 = {
	.gpio_base = OMAP5_GPIO_END,

	.power_ctrl = POWER_CTRL_NSLEEP_MASK | POWER_CTRL_ENABLE1_MASK |
			POWER_CTRL_ENABLE1_MASK,

	.gpadc_pdata = &omap5_palmas_gpadc,
	.pmic_pdata = &omap5_palmas_pmic,
	.usb_pdata = &omap5_palmas_usb,
	.resource_pdata = &omap5_palmas_resource,
};
#define PALMAS_NAME "twl6035"
#define PALMAS_DATA (&palmas_omap5)
#else
#define PALMAS_NAME NULL
#define PALMAS_DATA NULL
#endif  /* CONFIG_OMAP5_SEVM_PALMAS */

static struct twl6040_codec_data twl6040_codec = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step	= 0x0f,
	.hs_right_step	= 0x0f,
	.hf_left_step	= 0x1d,
	.hf_right_step	= 0x1d,
};

static struct twl6040_vibra_data twl6040_vibra = {
	.vibldrv_res = 8,
	.vibrdrv_res = 3,
	.viblmotor_res = 10,
	.vibrmotor_res = 10,
	.vddvibl_uV = 0,	/* fixed volt supply - VBAT */
	.vddvibr_uV = 0,	/* fixed volt supply - VBAT */
};

static struct twl6040_platform_data twl6040_data = {
	.codec		= &twl6040_codec,
	.vibra		= &twl6040_vibra,
	.audpwron_gpio	= 145,
};

static struct platform_device omap5evm_dmic_codec = {
	.name	= "dmic-codec",
	.id	= -1,
};

static struct platform_device omap5evm_hdmi_audio_codec = {
	.name	= "hdmi-audio-codec",
	.id	= -1,
};

static struct omap_abe_twl6040_data omap5evm_abe_audio_data = {
	/* Audio out */
	.has_hs		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* HandsFree through expasion connector */
	.has_hf		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* PandaBoard: FM TX, PandaBoardES: can be connected to audio out */
	.has_aux	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* PandaBoard: FM RX, PandaBoardES: audio in */
	.has_afm	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_hsmic	= 1,
	.has_abe	= 1,
	.has_dmic	= 1,
	/* Jack detection. */
	.jack_detection	= 1,
	/* MCLK input is 19.2MHz */
	.mclk_freq	= 19200000,
	.card_name = "OMAP5EVM",

};

static struct platform_device omap5evm_abe_audio = {
	.name		= "omap-abe-twl6040",
	.id		= -1,
	.dev = {
		.platform_data = &omap5evm_abe_audio_data,
	},
};

static struct platform_device *omap5evm_devices[] __initdata = {
	&omap5evm_dmic_codec,
	&omap5evm_hdmi_audio_codec,
	&omap5evm_abe_audio,
	&panda5_leds_gpio,
	&panda5_gpio_keys_device,
};

/*
 * Display monitor features are burnt in their EEPROM as EDID data. The EEPROM
 * is connected as I2C slave device, and can be accessed at address 0x50
 */
static struct i2c_board_info __initdata hdmi_i2c_eeprom[] = {
	{
		I2C_BOARD_INFO("eeprom", DDC_ADDR),
	},
};

static struct i2c_gpio_platform_data i2c_gpio_pdata = {
	.sda_pin                = 195,
	.sda_is_open_drain      = 0,
	.scl_pin                = 194,
	.scl_is_open_drain      = 0,
	.udelay                 = 2,            /* ~100 kHz */
};

static struct platform_device hdmi_edid_device = {
	.name                   = "i2c-gpio",
	.id                     = -1,
	.dev.platform_data      = &i2c_gpio_pdata,
};

static struct regulator_consumer_supply omap5_evm_vmmc1_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.0"),
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.1"),
};

static struct regulator_init_data omap5_evm_vmmc1 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
		.always_on	= true,
	},
	.num_consumer_supplies = ARRAY_SIZE(omap5_evm_vmmc1_supply),
	.consumer_supplies = omap5_evm_vmmc1_supply,
};

static struct fixed_voltage_config omap5_evm_sd_dummy = {
	.supply_name = "vmmc_supply",
	.microvolts = 3000000, /* 3.0V */
	.gpio = -EINVAL,
	.init_data = &omap5_evm_vmmc1,
};

static struct platform_device dummy_sd_regulator_device = {
	.name		= "reg-fixed-voltage",
	.id		= 1,
	.dev = {
		.platform_data = &omap5_evm_sd_dummy,
	}
};

static struct pca953x_platform_data omap5evm_gpio_expander_info = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= OMAP_TCA6424_IRQ_BASE,
};

static struct i2c_board_info __initdata omap5evm_i2c_5_boardinfo[] = {
	{
		I2C_BOARD_INFO("tca6424", 0x22),
		.platform_data = &omap5evm_gpio_expander_info,
		.irq = GPIO_EXT_INT_PIN,
	},
};

static int __init omap5pandai2c_init(void)
{

	omap_register_i2c_bus(1, 400, NULL, 0);
	omap_register_i2c_bus(2, 400, NULL, 0);
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, NULL, 0);
	omap_register_i2c_bus(5, 400, omap5evm_i2c_5_boardinfo,
					ARRAY_SIZE(omap5evm_i2c_5_boardinfo));

	return 0;
}

/* Display DVI */
#define PANDA_DVI_TFP410_POWER_DOWN_GPIO	0

static int omap5_panda_enable_dvi(struct omap_dss_device *dssdev)
{
	gpio_set_value(dssdev->reset_gpio, 1);
	return 0;
}

static void omap5_panda_disable_dvi(struct omap_dss_device *dssdev)
{
	gpio_set_value(dssdev->reset_gpio, 0);
}

/* Using generic display panel */
static struct panel_generic_dpi_data omap5_dvi_panel = {
	.name			= "generic_720p",
	.platform_enable	= omap5_panda_enable_dvi,
	.platform_disable	= omap5_panda_disable_dvi,
};

static struct omap_dss_device omap5_panda_dvi_device = {
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.name			= "dvi",
	.driver_name		= "generic_dpi_panel",
	.data			= &omap5_dvi_panel,
	.phy.dpi.data_lines	= 24,
	.reset_gpio		= PANDA_DVI_TFP410_POWER_DOWN_GPIO,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
};

static struct omap_dss_hdmi_data omap5panda_hdmi_data = {
        .hpd_gpio = HDMI_GPIO_HPD,
	.ct_cp_hpd_gpio = HDMI_GPIO_CT_CP_HPD,
	.ls_oe_gpio = HDMI_GPIO_LS_OE,
};

static int omap5panda_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	return 0;
}

static void omap5panda_panel_disable_hdmi(struct omap_dss_device *dssdev)
{

}

static struct omap_dss_device omap5panda_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.platform_enable = omap5panda_panel_enable_hdmi,
	.platform_disable = omap5panda_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
	.data = &omap5panda_hdmi_data,
};

static struct omap_dss_device *omap5panda_dss_devices[] = {
	&omap5panda_hdmi_device,
	&omap5_panda_dvi_device,
};

static struct omap_dss_board_info omap5evm_dss_data = {
	.num_devices	= ARRAY_SIZE(omap5panda_dss_devices),
	.devices	= omap5panda_dss_devices,
	.default_device	= &omap5panda_hdmi_device,
};

static void omap5panda_hdmi_init(void)
{
	/* Need to configure HPD as a gpio in mux */
	omap_hdmi_init(0);
}

static struct omapfb_platform_data panda_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = OMAP5_SEVM_FB_RAM_SIZE,
			},
		},
	},
};

static void __init omap5panda_display_init(void)
{
	struct sgx_omaplfb_config data = {
		.tiler2d_buffers = 0,
		.swap_chain_length = 2,
		.vram_buffers = 2,
	};

	omapfb_set_platform_data(&panda_fb_pdata);
	omap_vram_set_sdram_vram(OMAP5_SEVM_FB_RAM_SIZE, 0);
	sgx_omaplfb_set(0, &data);

	i2c_register_board_info(0, hdmi_i2c_eeprom,
			ARRAY_SIZE(hdmi_i2c_eeprom));
	platform_device_register(&hdmi_edid_device);

	omap5panda_hdmi_init();
	omap_display_init(&omap5evm_dss_data);
}

static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0] = OMAP_USBHS_PORT_MODE_UNUSED,
	.port_mode[1] = OMAP_EHCI_PORT_MODE_HSIC,
	.port_mode[2] = OMAP_EHCI_PORT_MODE_HSIC,
	.phy_reset  = true,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = GPIO_HUB_NRESET,
	.reset_gpio_port[2]  = GPIO_ETH_NRESET
};

static void __init omap_ehci_ohci_init(void)
{
	struct clk *phy_ref_clk;

       /* FREF_CLK1 provides the 19.2 MHz reference clock to the PHY */
	phy_ref_clk = clk_get(NULL, "auxclk1_ck");
	if (IS_ERR(phy_ref_clk)) {
		pr_err("Cannot request auxclk1\n");
	} else {
		clk_set_rate(phy_ref_clk, 19200000);
		clk_enable(phy_ref_clk);
	}

	omap_mux_init_gpio(GPIO_HUB_NRESET,
			OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE | OMAP_MUX_MODE6);
	omap_mux_init_gpio(GPIO_ETH_NRESET,
			OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE | OMAP_MUX_MODE6);

	usbhs_init(&usbhs_bdata);
	return;
}

static void __init omap_5_panda_init(void)
{
	omap5_mux_init(board_mux, NULL, OMAP_PACKAGE_CBL);
	omap_sdrc_init(NULL, NULL);
	omap_create_board_props();
	omap5pandai2c_init();
	omap5_pmic_init(1, PALMAS_NAME, OMAP44XX_IRQ_SYS_1N, PALMAS_DATA,
			"twl6040", OMAP44XX_IRQ_SYS_2N, &twl6040_data);

	omap_serial_init();
	platform_device_register(&dummy_sd_regulator_device);
	omap_ehci_ohci_init();

	/* TODO: Once the board identification is passed in from the
	 * bootloader pass in the HACK board ID to the conn board file
	*/
	omap5_connectivity_init(OMAP5_PANDA5_BOARD_ID);

	omap_hsmmc_init(mmc);
	usb_dwc3_init();
	platform_add_devices(omap5evm_devices, ARRAY_SIZE(omap5evm_devices));

	omap_init_dmm_tiler();
	omap5_register_ion();
	omap5panda_display_init();
}

static void __init omap_panda5_reserve(void)
{
	omap_rproc_reserve_cma(RPROC_CMA_OMAP5);

	omap5_ion_init();

	omap_reserve();
}

MACHINE_START(OMAP5_PANDA, "OMAP5 panda board")
	/* Maintainer: Dan Murphy - Texas Instruments Inc */
	.atag_offset	= 0x100,
	.reserve	= omap_panda5_reserve,
	.map_io		= omap5_map_io,
	.init_early	= omap_5430evm_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_5_panda_init,
	.timer		= &omap5_timer,
MACHINE_END
