/*
 * arch/arm/mach-omap2/board-54xx-sevm-panel.c
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <drm/drm_edid.h>

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/omapfb.h>
#include <video/omapdss.h>

#include <plat/vram.h>
#include <plat/gpio.h>

#include <video/omapdss.h>
#include <video/omap-panel-lg4591.h>
#include <plat/android-display.h>

#include "board-54xx-sevm.h"

#define OMAP5_SEVM_FB_RAM_SIZE       (SZ_16M + SZ_4M)
#define OMAPLFB_NUM_DEV 1

#define HDMI_GPIO_HPD 193
#define HDMI_GPIO_CT_CP_HPD OMAP_MPUIO(0)
#define HDMI_GPIO_LS_OE     OMAP_MPUIO(1)

static void lg_panel_set_power(bool enable)
{
}

struct omap_sevm_panel_data {
	struct omap_dss_board_info *board_info;
	struct dsscomp_platform_data *dsscomp_data;
	struct sgx_omaplfb_platform_data *omaplfb_data;
};

static struct panel_lg4591_data dsi_panel = {
	.reset_gpio = 183,
	.set_power = lg_panel_set_power,
	.number_actions = 6,
	.cooling_actions = {
		{ .priority = 0, .percentage = 100, },
		{ .priority = 1, .percentage = 100, },
		{ .priority = 2, .percentage = 80, },
		{ .priority = 3, .percentage = 70, },
		{ .priority = 4, .percentage = 60, },
		{ .priority = 5, .percentage = 50, },
	},
};

static void omap5evm_lcd_init(void)
{
	int r;

	r = gpio_request_one(dsi_panel.reset_gpio, GPIOF_DIR_OUT,
		"lcd1_reset_gpio");
	if (r)
		pr_err("%s: Could not get lcd1_reset_gpio\n", __func__);
}

static void __init omap5evm_hdmi_init(void)
{
	/* Need to configure HPD as a gpio in mux */
	omap_hdmi_init(0);
}

static struct omap_dss_device omap5evm_lcd_device = {
	.name			= "lcd",
	.driver_name		= "lg4591",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi_panel,
	.phy.dsi		= {
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,
		.data3_lane	= 4,
		.data3_pol	= 0,
		.data4_lane	= 5,
		.data4_pol	= 0,
	},
	.clocks = {
		.dispc = {
			.channel = {
				.lck_div	= 1,	/* LCD */
				.pck_div	= 2,	/* PCD */
				.lcd_clk_src	= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
		},
		.dsi = {
			.regn		= 19,	/* DSI_PLL_REGN */
			.regm		= 238,	/* DSI_PLL_REGM */

			.regm_dispc	= 3,	/* PLL_CLK1 (M4) */
			.regm_dsi	= 3,	/* PLL_CLK2 (M5) */
			.lp_clk_div	= 8,	/* LPDIV */

			.dsi_fclk_src	= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},
	.panel = {
		.dsi_mode	= OMAP_DSS_DSI_VIDEO_MODE,
		.width_in_um	= 55400,
		.height_in_um	= 98300,
	},
	.channel		= OMAP_DSS_CHANNEL_LCD,
};

static int omap5evm_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	return 0;
}

static void omap5evm_panel_disable_hdmi(struct omap_dss_device *dssdev)
{

}

static struct omap_dss_hdmi_data sdp54xx_hdmi_data = {
	.hpd_gpio = HDMI_GPIO_HPD,
	.ct_cp_hpd_gpio = HDMI_GPIO_CT_CP_HPD,
	.ls_oe_gpio = HDMI_GPIO_LS_OE,

};

static struct omap_dss_device omap5evm_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.platform_enable = omap5evm_panel_enable_hdmi,
	.platform_disable = omap5evm_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
	.data = &sdp54xx_hdmi_data,
};

static struct omap_dss_device *omap5evm_dss_devices[] = {
	&omap5evm_lcd_device,
	&omap5evm_hdmi_device,
};

static struct omap_dss_board_info omap5evm_dss_data = {
	.num_devices	= ARRAY_SIZE(omap5evm_dss_devices),
	.devices	= omap5evm_dss_devices,
	.default_device	= &omap5evm_lcd_device,
};

static struct omap_dss_device *omap5evm_hdmi_dss_devices[] = {
	&omap5evm_hdmi_device,
};

static struct omap_dss_board_info omap5evm_dss_data_hdmi_default_display = {
	.num_devices    = ARRAY_SIZE(omap5evm_hdmi_dss_devices),
	.devices        = omap5evm_hdmi_dss_devices,
	.default_device = &omap5evm_hdmi_device
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

static struct omapfb_platform_data sevm_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = OMAP5_SEVM_FB_RAM_SIZE,
			},
		},
	},
};

static struct omap_sevm_panel_data panel_data_lg_panel = {
	.board_info = &omap5evm_dss_data,
	.dsscomp_data = NULL,
	.omaplfb_data = NULL,
};

static struct sgx_omaplfb_config omaplfb_config_hdmi_default_display[OMAPLFB_NUM_DEV] = {
	{
		.tiler2d_buffers = 0,
		.vram_buffers = 2,
		.swap_chain_length = 2,
	}
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data_hdmi_default_display = {
	.num_configs = OMAPLFB_NUM_DEV,
	.configs = omaplfb_config_hdmi_default_display,
};

static struct omap_sevm_panel_data panel_data_hdmi_default_display  = {
	.board_info = &omap5evm_dss_data_hdmi_default_display,
	.dsscomp_data = NULL,
	.omaplfb_data = &omaplfb_plat_data_hdmi_default_display,
};

static struct omap_sevm_panel_data *get_panel_data(void)
{
	if (omap_android_display_is_default(&omap5evm_hdmi_device))
		return &panel_data_hdmi_default_display;
	else
		return &panel_data_lg_panel;
}

void __init sevm_android_display_setup(void)
{
	struct omap_sevm_panel_data *panel_data = get_panel_data();
	omap_android_display_setup(panel_data->board_info,
					panel_data->dsscomp_data,
					panel_data->omaplfb_data,
					&sevm_fb_pdata);
}

int __init sevm_panel_init(void)
{

	omapfb_set_platform_data(&sevm_fb_pdata);
	i2c_register_board_info(0, hdmi_i2c_eeprom,
			ARRAY_SIZE(hdmi_i2c_eeprom));
	platform_device_register(&hdmi_edid_device);

	omap5evm_lcd_init();
	omap5evm_hdmi_init();
	omap_display_init(get_panel_data()->board_info);
	return 0;

};
