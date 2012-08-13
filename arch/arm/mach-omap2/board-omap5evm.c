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

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <linux/gpio.h>
#include <linux/hwspinlock.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c/pca953x.h>
#include <linux/i2c/tmp102.h>
#include <linux/i2c/twl.h>
#include <linux/input/matrix_keypad.h>
#include <linux/platform_data/omap-abe-twl6040.h>
#include <linux/platform_data/omap4-keypad.h>
#include <linux/mfd/palmas.h>
#include <linux/mfd/twl6040.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <mach/hardware.h>

#include <plat/common.h>
#include <plat/i2c.h>
#include <plat/gpio.h>
#include <plat/omap_hsi.h>
#include <plat/omap4-keypad.h>
#include <plat/mmc.h>
#include <plat/omap4-keypad.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/drm.h>
#include <plat/remoteproc.h>
#include <plat/rpmsg_resmgr.h>
#include <plat/usb.h>

#include "board-54xx-sevm.h"
#include "common.h"
#include "common-board-devices.h"
#include "hsmmc.h"
#include "mux.h"
#include "omap5_ion.h"
#include "omap_ram_console.h"

/* USBB3 to SMSC LAN9730 */
#define GPIO_ETH_NRESET	172

/* USBB2 to SMSC 4640 HUB */
#define GPIO_HUB_NRESET	173

/* MSECURE GPIO */
#define GPIO_MSECURE 234

static const uint32_t evm5430_keymap[] = {
	KEY(2, 2, KEY_VOLUMEUP),
	KEY(2, 3, KEY_VOLUMEDOWN),
	KEY(2, 4, KEY_BACK),
	KEY(2, 5, KEY_HOME),
	KEY(2, 6, KEY_MENU),
	KEY(2, 7, KEY_SEARCH),
};

static struct matrix_keymap_data evm5430_keymap_data = {
	.keymap                 = evm5430_keymap,
	.keymap_size            = ARRAY_SIZE(evm5430_keymap),
};

static struct omap4_keypad_platform_data evm5430_keypad_data = {
	.keymap_data            = &evm5430_keymap_data,
	.rows                   = 8,
	.cols                   = 8,
	.no_autorepeat		= true,
};

static struct omap_board_data keypad_data = {
	.id                     = 1,
};

static struct gpio_led sevm_gpio_leds[] = {
	{
		.name	= "blue",
		.default_trigger = "timer",
		.gpio	= OMAP_MPUIO(19),
	},
	{
		.name	= "red",
		.default_trigger = "timer",
		.gpio	= OMAP_MPUIO(17),
	},
	{
		.name	= "green",
		.default_trigger = "timer",
		.gpio	= OMAP_MPUIO(18),
	},

};

static struct gpio_led_platform_data sevm_led_data = {
	.leds	= sevm_gpio_leds,
	.num_leds = ARRAY_SIZE(sevm_gpio_leds),
};

static struct platform_device sevm_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &sevm_led_data,
	},
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux NULL
#endif

#if defined(CONFIG_TI_EMIF) || defined(CONFIG_TI_EMIF_MODULE)
#ifndef CONFIG_MACH_OMAP_5430ZEBU
static struct __devinitdata emif_custom_configs custom_configs = {
	.mask	= EMIF_CUSTOM_CONFIG_LPMODE,
	.lpmode	= EMIF_LP_MODE_DISABLE
};
#endif
#endif

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
		.gpio_cd	= 67,
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
		.built_in	= 1,
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

static struct regulator_consumer_supply omap5_evm_cam2_supply[] = {
	REGULATOR_SUPPLY("cam2pwr", NULL),
};

/* VAUX3 for Camera */
static struct regulator_init_data omap5_ldo1 = {
	.constraints = {
		.min_uV			= 2800000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_evm_cam2_supply),
	.consumer_supplies	= omap5_evm_cam2_supply,
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

static struct regulator_consumer_supply omap5_evm_phy3_supply[] = {
	REGULATOR_SUPPLY("cam2csi", NULL),
};

/* CSI for Camera */
static struct regulator_init_data omap5_ldo8 = {
	.constraints = {
		.min_uV			= 1500000,
		.max_uV			= 1500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap5_evm_phy3_supply),
	.consumer_supplies	= omap5_evm_phy3_supply,
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

	.sysen2_mode_active = 1,

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
	.amic_bias_settle_ms = 0xeb, /* 235 ms */
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

#ifdef CONFIG_OMAP5_SEVM_PALMAS
static struct omap_rprm_regulator omap5evm_rprm_regulators[] = {
	{
		.name = "cam2pwr",
		.fixed = true,
	},
	{
		.name = "cam2csi",
		.fixed = true,
	},
};
#else
static struct omap_rprm_regulator omap5evm_rprm_regulators[] = { };
#endif  /* CONFIG_OMAP5_SEVM_PALMAS */

static struct platform_device omap5evm_dmic_codec = {
	.name	= "dmic-codec",
	.id	= -1,
};

static struct platform_device omap5evm_spdif_dit_codec = {
	.name           = "spdif-dit",
	.id             = -1,
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
	/* Earpiece */
	.has_ep		= 1,
	/* PandaBoard: FM TX, PandaBoardES: can be connected to audio out */
	.has_aux	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	/* PandaBoard: FM RX, PandaBoardES: audio in */
	.has_afm	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_abe	= 1,
	.has_dmic	= 1,
	.has_hsmic	= 1,
	.has_mainmic	= 1,
	.has_submic	= 1,
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
	&omap5evm_spdif_dit_codec,
	&omap5evm_hdmi_audio_codec,
	&omap5evm_abe_audio,
	&sevm_leds_gpio,
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

/*
 * I2C GPIO Expander - TCA6424
 */

/* Mounted on Base-Board */
static struct pca953x_platform_data omap5evm_gpio_expander_info = {
	.gpio_base	= OMAP_MAX_GPIO_LINES,
	.irq_base	= OMAP_TCA6424_IRQ_BASE,
};

static struct i2c_board_info __initdata omap5evm_i2c_5_boardinfo[] = {
	{
		I2C_BOARD_INFO("tca6424", 0x22),
		.platform_data = &omap5evm_gpio_expander_info,
		.irq = 32,
	},
};

static struct i2c_board_info __initdata omap5evm_i2c_1_boardinfo[] = {
	{
		I2C_BOARD_INFO("bq27530", 0x55),
	},
};

/* TMP102 PCB Temperature sensor */
static struct tmp102_platform_data tmp102_slope_offset_info = {
	.slope = 470,
	.slope_cpu = 378,
	.offset = -1272,
	.offset_cpu = -154,
};

static struct i2c_board_info __initdata omap5evm_i2c_4_boardinfo[] = {
	{
		I2C_BOARD_INFO("tmp102_temp_sensor", 0x48),
		.platform_data = &tmp102_slope_offset_info,
	},
	{
		I2C_BOARD_INFO("tmp006_temp_sensor", 0x40),
	},
};

static struct omap_i2c_bus_board_data __initdata omap5_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap5_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap5_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap5_i2c_4_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap5_i2c_5_bus_pdata;

static void __init omap_i2c_hwspinlock_init(int bus_id, int spinlock_id,
				struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request(USE_MUTEX_LOCK);
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id,
							USE_MUTEX_LOCK);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else
		pr_err("I2C hwspinlock request failed for bus %d\n", bus_id);
}

static int __init omap_5430evm_i2c_init(void)
{

	omap_i2c_hwspinlock_init(1, 0, &omap5_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &omap5_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &omap5_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &omap5_i2c_4_bus_pdata);
	omap_i2c_hwspinlock_init(5, 4, &omap5_i2c_5_bus_pdata);

	omap_register_i2c_bus_board_data(1, &omap5_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &omap5_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &omap5_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &omap5_i2c_4_bus_pdata);
	omap_register_i2c_bus_board_data(5, &omap5_i2c_5_bus_pdata);

	/*
	 * WA for OMAP5430-1.0BUG01477: I2C1 and I2C_SR weak and strong
	 * pull-up are both activated by default. It affects I2C1 and
	 * hence disable weak pull for it. This is for ES1.0 and fixed
	 * in ES2.0
	 */
	if (omap_rev() == OMAP5430_REV_ES1_0)
		omap5_i2c_weak_pullup(1, false);

	/* Enable internal pull-ups for SCL, SDA lines. OMAP5 sEVM platform
	 * does not have external pull-ups for any of the I2C buses hence
	 * internal pull-ups are enabled
	 */
	omap5_i2c_pullup(1, OMAP5_I2C_PULLUP_EN, OMAP5_I2C_GLITCH_FREE_DIS);
	omap5_i2c_pullup(2, OMAP5_I2C_PULLUP_EN, OMAP5_I2C_GLITCH_FREE_DIS);
	omap5_i2c_pullup(3, OMAP5_I2C_PULLUP_EN, OMAP5_I2C_GLITCH_FREE_DIS);
	omap5_i2c_pullup(4, OMAP5_I2C_PULLUP_EN, OMAP5_I2C_GLITCH_FREE_DIS);
	omap5_i2c_pullup(5, OMAP5_I2C_PULLUP_EN, OMAP5_I2C_GLITCH_FREE_DIS);

	omap_register_i2c_bus(1, 400, omap5evm_i2c_1_boardinfo,
					ARRAY_SIZE(omap5evm_i2c_1_boardinfo));
	omap_register_i2c_bus(2, 400, NULL, 0);
	omap_register_i2c_bus(3, 400, NULL, 0);
	omap_register_i2c_bus(4, 400, omap5evm_i2c_4_boardinfo,
					ARRAY_SIZE(omap5evm_i2c_4_boardinfo));
	omap_register_i2c_bus(5, 400, omap5evm_i2c_5_boardinfo,
					ARRAY_SIZE(omap5evm_i2c_5_boardinfo));

	return 0;
}

/*
 * HSI usage is declared using bootargs variable:
 * board-omap5evm.modem_ipc=hsi
 * Variable modem_ipc is used to catch bootargs parameter value.
 */
static char *modem_ipc = "n/a";
module_param(modem_ipc, charp, 0);
MODULE_PARM_DESC(modem_ipc, "Modem IPC setting");

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
	omap_mux_init_gpio(172, OMAP_PIN_OUTPUT | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_gpio(173, OMAP_PIN_OUTPUT | OMAP_PIN_INPUT_PULLUP);

	usbhs_init(&usbhs_bdata);
	return;
}

static void __init omap_msecure_init(void)
{
	int err;
	/* setup msecure line as GPIO for RTC accesses */
	omap_mux_init_gpio(GPIO_MSECURE, OMAP_PIN_OUTPUT);
	err = gpio_request_one(GPIO_MSECURE, GPIOF_OUT_INIT_HIGH, "msecure");
	if (err < 0)
		pr_err("Failed to request GPIO %d, error %d\n",
			GPIO_MSECURE, err);
}

static void __init omap_5430evm_init(void)
{
	int status;
#if defined(CONFIG_TI_EMIF) || defined(CONFIG_TI_EMIF_MODULE)
#ifndef CONFIG_MACH_OMAP_5430ZEBU
	omap_emif_set_device_details(1, &lpddr2_elpida_4G_S4_x2_info,
			lpddr2_elpida_4G_S4_timings,
			ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
			&lpddr2_elpida_S4_min_tck,
			&custom_configs);

	omap_emif_set_device_details(2, &lpddr2_elpida_4G_S4_x2_info,
			lpddr2_elpida_4G_S4_timings,
			ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
			&lpddr2_elpida_S4_min_tck,
			&custom_configs);
#endif
#endif
	omap5_mux_init(board_mux, NULL, OMAP_PACKAGE_CBL);
	omap_sdrc_init(NULL, NULL);
	omap_create_board_props();
	omap_5430evm_i2c_init();
	omap_msecure_init();
	omap5_pmic_init(1, PALMAS_NAME, OMAP44XX_IRQ_SYS_1N, PALMAS_DATA,
			"twl6040", OMAP44XX_IRQ_SYS_2N, &twl6040_data);

	omap5_board_serial_init();
	platform_device_register(&dummy_sd_regulator_device);
	sevm_dock_init();
	sevm_touch_init();
	sevm_sensor_init();

	/* omap5evm_modem_init shall be called before omap_ehci_ohci_init */
	if (!strcmp(modem_ipc, "hsi"))
		omap5evm_modem_init(true);
	else
		omap5evm_modem_init(false);

	status = omap4_keyboard_init(&evm5430_keypad_data, &keypad_data);
	if (status)
		pr_err("Keypad initialization failed: %d\n", status);

	omap_ehci_ohci_init();

	/* TODO: Once the board identification is passed in from the
	 * bootloader pass in the HACK board ID to the conn board file
	*/
	omap5_connectivity_init(OMAP5_SEVM_BOARD_ID);
	omap_hsmmc_init(mmc);
	usb_dwc3_init();
	platform_add_devices(omap5evm_devices, ARRAY_SIZE(omap5evm_devices));

	omap_init_dmm_tiler();
	omap5_register_ion();
	sevm_panel_init();
	omap_rprm_regulator_init(omap5evm_rprm_regulators,
					ARRAY_SIZE(omap5evm_rprm_regulators));

}

static void __init omap_5430evm_reserve(void)
{
	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT,
			OMAP_RAM_CONSOLE_SIZE_DEFAULT);

	omap_rproc_reserve_cma(RPROC_CMA_OMAP5);

	omap5_ion_init();

	omap_reserve();
}

MACHINE_START(OMAP5_SEVM, "OMAP5 sevm board")
	/* Maintainer: Santosh Shilimkar - Texas Instruments Inc */
	.atag_offset	= 0x100,
	.reserve	= omap_5430evm_reserve,
	.map_io		= omap5_map_io,
	.init_early	= omap_5430evm_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_5430evm_init,
	.timer		= &omap5_timer,
MACHINE_END
