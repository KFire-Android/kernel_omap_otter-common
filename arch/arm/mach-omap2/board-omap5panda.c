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
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/input.h>

#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#ifdef CONFIG_OMAP5_SEVM_PALMAS
#include <linux/mfd/palmas.h>
#endif
#include <linux/i2c/pca953x.h>
#include <linux/i2c/twl.h>
#include <linux/mfd/twl6040.h>
#include <linux/platform_data/omap-abe-twl6040.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include "common.h"
#include <asm/hardware/gic.h>
#include <plat/common.h>
#include <plat/mmc.h>
#include <plat/usb.h>
#include "hsmmc.h"
#include "common-board-devices.h"
#include "mux.h"

#include <video/omapdss.h>
#include <video/omap-panel-generic-dpi.h>


#define GPIO_ETH_NRESET		15	/* USBB3 to SMSC LAN9730 */
#define GPIO_HUB_NRESET		80	/* USBB2 to SMSC 3530 HUB */
#define GPIO_EXT_INT_PIN	99
#define HDMI_GPIO_HPD		193

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP5_MUX(FREF_CLK1_OUT, OMAP_PIN_INPUT_PULLUP),
	OMAP5_MUX(USBB2_HSIC_STROBE, OMAP_PIN_INPUT | OMAP_MUX_MODE0),
	OMAP5_MUX(USBB2_HSIC_DATA, OMAP_PIN_INPUT | OMAP_MUX_MODE0),
	OMAP5_MUX(ABEDMIC_DIN3,
		OMAP_PIN_INPUT_PULLUP | OMAP_PIN_OFF_NONE | OMAP_MUX_MODE6),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux NULL
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
		.caps		= MMC_CAP_4_BIT_DATA,
		.gpio_cd	= 67,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_29_30,
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
	&omap5_panda_dvi_device,
	&omap5panda_hdmi_device,
};

static struct omap_dss_board_info omap5evm_dss_data = {
	.num_devices	= ARRAY_SIZE(omap5panda_dss_devices),
	.devices	= omap5panda_dss_devices,
	.default_device	= &omap5_panda_dvi_device,
};

static void omap5panda_hdmi_init(void)
{
	int r;

	r = gpio_request_one(HDMI_GPIO_HPD, GPIOF_DIR_IN,
				"hdmi_gpio_hpd");
	if (r)
		pr_err("%s: Could not get HDMI\n", __func__);

	/* Need to configure HPD as a gpio in mux */
	omap_hdmi_init(0);
}

static void __init omap5panda_display_init(void)
{
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
	omap5pandai2c_init();
	omap5_pmic_init(1, PALMAS_NAME, OMAP44XX_IRQ_SYS_1N, PALMAS_DATA,
			"twl6040", OMAP44XX_IRQ_SYS_2N, &twl6040_data);

	omap_serial_init();
	omap_ehci_ohci_init();

	omap_hsmmc_init(mmc);
	usb_dwc3_init();
	platform_add_devices(omap5evm_devices, ARRAY_SIZE(omap5evm_devices));

	omap5panda_display_init();
}


MACHINE_START(OMAP5_PANDA, "OMAP5 panda board")
	/* Maintainer: Dan Murphy - Texas Instruments Inc */
	.atag_offset	= 0x100,
	.reserve	= omap_reserve,
	.map_io		= omap5_map_io,
	.init_early	= omap_5430evm_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_5_panda_init,
	.timer		= &omap5_timer,
MACHINE_END
