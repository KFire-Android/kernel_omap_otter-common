/*
 * arch/arm/mach-omap2/board-44xx-tablet-panel.c
 *
 * Copyright (C) 2011 Texas Instruments
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

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/leds-omap4430sdp-display.h>
#include <linux/platform_device.h>
#include <linux/omapfb.h>
#include <video/omapdss.h>
#include <video/omap-panel-tc358765.h>

#include <linux/i2c/twl.h>

#include <plat/android-display.h>
#include <plat/vram.h>
#include <plat/omap_apps_brd_id.h>

#include "board-44xx-tablet.h"
#include "control.h"
#include "mux.h"

#define DP_4430_GPIO_59         59
#define LED_DISP_EN		102
#define DSI2_GPIO_59		59

#define HDMI_GPIO_CT_CP_HPD             60
#define HDMI_GPIO_HPD                   63  /* Hot plug pin for HDMI */

#define HDMI_GPIO_LS_OE 41 /* Level shifter for HDMI */
#define LCD_BL_GPIO		59 /*27*/	/* LCD Backlight GPIO */
/* PWM2 and TOGGLE3 register offsets */
#define LED_PWM2ON		0x03
#define LED_PWM2OFF		0x04
#define TWL6030_TOGGLE3		0x92

#define OMAP_HDMI_HPD_ADDR      0x4A100098
#define OMAP_HDMI_PULLTYPE_MASK 0x00000010

static enum omap_44xx_tablet_panel_type {
	LCD_DEFAULT,
	TC35876x_SHARP_LQ101K1LYxx_WXGA,/* TC35876x + Sharp LCD */
	TC35876x_SAMSUNG_HYDIS_WUXGA,	/* TC35876x + Samsung or HYDIS */
} tablet_panel_type = LCD_DEFAULT;

struct omap_tablet_panel_data {
	struct omap_dss_board_info *board_info;
	struct dsscomp_platform_data *dsscomp_data;
	struct sgx_omaplfb_platform_data *omaplfb_data;
};

static void omap4_tablet_init_display_led(void)
{
	twl_i2c_write_u8(TWL_MODULE_PWM, 0xFF, LED_PWM2ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x7F, LED_PWM2OFF);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x30, TWL6030_TOGGLE3);
}

static void omap4_tablet_set_primary_brightness(u8 brightness)
{
	if (brightness > 1) {
		if (brightness == 255)
			brightness = 0x7f;
		else
			brightness = (~(brightness/2)) & 0x7f;

		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x30, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL_MODULE_PWM, brightness, LED_PWM2ON);
		gpio_set_value(LED_DISP_EN, 1);
	} else if (brightness <= 1) {
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x08, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x38, TWL6030_TOGGLE3);
		gpio_set_value(LED_DISP_EN, 0);
	}
}

static void omap4_tablet_set_secondary_brightness(u8 brightness)
{
	if (brightness > 0)
		brightness = 1;

	gpio_set_value(LED_DISP_EN, brightness);
}

static struct omap4430_sdp_disp_led_platform_data sdp4430_disp_led_data = {
	.flags = LEDS_CTRL_AS_ONE_DISPLAY,
	.display_led_init = omap4_tablet_init_display_led,
	.primary_display_set = omap4_tablet_set_primary_brightness,
	.secondary_display_set = omap4_tablet_set_secondary_brightness,
};

static struct platform_device omap4_tablet_disp_led = {
	.name	=	"display_led",
	.id	=	-1,
	.dev	= {
		.platform_data = &sdp4430_disp_led_data,
	},
};

static struct gpio tablet_hdmi_gpios[] = {
	{HDMI_GPIO_CT_CP_HPD,  GPIOF_OUT_INIT_HIGH,    "hdmi_gpio_hpd"   },
	{HDMI_GPIO_LS_OE,      GPIOF_OUT_INIT_HIGH,    "hdmi_gpio_ls_oe" },
};

static void tablet_hdmi_mux_init(void)
{
	u32 r;
	int status;
	/* PAD0_HDMI_HPD_PAD1_HDMI_CEC */
	omap_mux_init_signal("hdmi_hpd.hdmi_hpd",
				OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("gpmc_wait2.gpio_100",
			OMAP_PIN_INPUT_PULLDOWN);
	omap_mux_init_signal("hdmi_cec.hdmi_cec",
			OMAP_PIN_INPUT_PULLUP);
	/* PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA */
	omap_mux_init_signal("hdmi_ddc_scl.hdmi_ddc_scl",
			OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("hdmi_ddc_sda.hdmi_ddc_sda",
			OMAP_PIN_INPUT_PULLUP);

	/* strong pullup on DDC lines using unpublished register */
	r = ((1 << 24) | (1 << 28)) ;
	omap4_ctrl_pad_writel(r, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_I2C_1);

	gpio_request(HDMI_GPIO_HPD, NULL);
	omap_mux_init_gpio(HDMI_GPIO_HPD, OMAP_PIN_INPUT | OMAP_PULL_ENA);
	gpio_direction_input(HDMI_GPIO_HPD);

	status = gpio_request_array(tablet_hdmi_gpios,
			ARRAY_SIZE(tablet_hdmi_gpios));
	if (status)
		pr_err("%s: Cannot request HDMI GPIOs %x\n", __func__, status);
}

static struct tc358765_board_data dsi_data_tc35876x_sharp_lq101k1lyxx = {
	.lp_time	= 0x4,
	.clrsipo	= 0x3,
	.lv_is		= 0x1,
	.lv_nd		= 0x6,
	.vtgen		= 0x0,
	.vsdelay	= 0xf02,
	.pclkdiv	= 0x0,
	.pclksel	= 0x0,
	.lvdlink	= 0x0,
	.msf		= 0x0,
	.evtmode	= 0x1,
};

static struct tc358765_board_data dsi_data_tc35876x_samhyd_wuxga = {
	.lp_time	= 0x7,
	.clrsipo	= 0x7,
	.lv_is		= 0x2,
	.lv_nd		= 0x6,
	.vtgen		= 0x0,
	.vsdelay	= 0x0,
	.pclkdiv	= 0x1,
	.pclksel	= 0x0,
	.lvdlink	= 0x1,
	.msf		= 0x1,
	.evtmode	= 0x1,
};

static struct omap_dsi_timings dsi_timings_tc35876x_samhyd_wuxga = {
	.hbp		= 0,
	.hfp		= 24,
	.hsa		= 0,
	.vbp		= 9,
	.vfp		= 10,
	.vsa		= 1,
	.vact		= 1200,
	.tl		= 1107,
	.hsa_hs_int	= 0,
	.hfp_hs_int	= 0,
	.hbp_hs_int	= 0,
	.hsa_lp_int	= 130,
	.hfp_lp_int	= 223,
	.hbp_lp_int	= 59,
	.bl_lp_int	= 0,
	.bl_hs_int	= 1038,
	.exit_lat	= 20,
	.enter_lat	= 23,
};

static struct omap_video_timings dispc_timings_tc35876x_samhyd_wuxga = {
	.x_res		= 1920,
	.y_res		= 1200,
	.hfp		= 4,
	.hsw		= 5,
	.hbp		= 39,
	.vfp		= 9,
	.vsw		= 1,
	.vbp		= 10,
};

static struct omap_dss_device lcd_tc35876x_sharp_lq101k1lyxx = {
	.name                   = "lcd",
	.driver_name            = "tc358765",
	.type                   = OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi_data_tc35876x_sharp_lq101k1lyxx,
	.phy.dsi                = {
		.clk_lane       = 1,
		.clk_pol        = 0,
		.data1_lane     = 2,
		.data1_pol      = 0,
		.data2_lane     = 3,
		.data2_pol      = 0,
		.data3_lane     = 4,
		.data3_pol      = 0,
		.data4_lane     = 5,
		.data4_pol      = 0,

		.type = OMAP_DSS_DSI_TYPE_VIDEO_MODE,
		.line_bufs	= 2,
	},

	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,
				.pck_div        = 2,
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
		},

		.dsi = {
			.regn           = 38,
			.regm           = 441,
			.regm_dispc     = 6,
			.regm_dsi       = 9,
			.lp_clk_div     = 5,
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
			.tlpx	= 12,
			.tclk = {
				.zero	 = 58,
				.prepare = 15,
				.trail	 = 16,
			},
			.ths = {
				.zero	 = 23,
				.prepare = 18,
				.exit	 = 33,
				.trail	 = 19,
			},
		},
	},

	.panel = {
		.timings = {
			.x_res		= 1280,
			.y_res		= 800,
			.pixel_clock	= 65183,
			.hfp		= 10,
			.hsw		= 20,
			.hbp		= 10,
			.vfp		= 4,
			.vsw		= 4,
			.vbp		= 4,
		},
		.width_in_um = 217000,
		.height_in_um = 135600,
	},

	.ctrl = {
		.pixel_size = 24,
	},

	.reset_gpio     = 102,
	.channel = OMAP_DSS_CHANNEL_LCD,
	.skip_init = false,

	.platform_enable = NULL,
	.platform_disable = NULL,
	.dispc_timings = NULL,
	.dsi_timings = NULL,
};

static struct omap_dss_device lcd_tc35876x_samhyd_wuxga = {
	.name                   = "lcd",
	.driver_name            = "tc358765",
	.type                   = OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi_data_tc35876x_samhyd_wuxga,
	.phy.dsi                = {
		.clk_lane       = 1,
		.clk_pol        = 0,
		.data1_lane     = 2,
		.data1_pol      = 0,
		.data2_lane     = 3,
		.data2_pol      = 0,
		.data3_lane     = 4,
		.data3_pol      = 0,
		.data4_lane     = 5,
		.data4_pol      = 0,

		.type = OMAP_DSS_DSI_TYPE_VIDEO_MODE,
		.line_bufs	= 0,
	},

	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,
				.pck_div        = 1,
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
		},

		.dsi = {
			.regn           = 17,
			.regm           = 287,
			.regm_dispc     = 9,
			.regm_dsi       = 7,
			.lp_clk_div     = 19,
			.offset_ddr_clk = 0,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
			.tlpx	= 18,
			.tclk = {
				.zero	 = 86,
				.prepare = 22,
				.trail	 = 22,
			},
			.ths = {
				.zero	 = 34,
				.prepare = 25,
				.exit	 = 47,
				.trail	 = 25,
			},
		},
	},

	.panel = {
		.timings = {
			.x_res		= 1920,
			.y_res		= 1200,
			.pixel_clock	= 162071,
			.hfp		= 40,
			.hsw		= 6,
			.hbp		= 248,
			.vfp		= 8,
			.vsw		= 6,
			.vbp		= 6,
		},
		.width_in_um = 191520,
		.height_in_um = 119700,
	},

	.ctrl = {
		.pixel_size = 18,
	},

	.reset_gpio     = 102,
	.channel = OMAP_DSS_CHANNEL_LCD,
	.skip_init = false,

	.platform_enable = NULL,
	.platform_disable = NULL,

	.dispc_timings = &dispc_timings_tc35876x_samhyd_wuxga,
	.dsi_timings = &dsi_timings_tc35876x_samhyd_wuxga,
};

static struct omap_dss_device tablet_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.panel = {
		.hdmi_default_cea_code = 34,
		.timings = {
			.x_res = 1920,
			.y_res = 1080,
			.pixel_clock = 74250,
			.hsw = 44,
			.hfp = 88,
			.hbp = 148,
			.vsw = 5,
			.vfp = 4,
			.vbp = 36,
		},
	},
	.clocks	= {
		.dispc	= {
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},
		.hdmi	= {
			.regn	= 15,
			.regm2	= 1,
			.max_pixclk_khz = 148500,
		},
	},
	.hpd_gpio = HDMI_GPIO_HPD,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};

static struct omap_dss_device *dss_devices_tc35876x_samhyd_wuxga[] = {
	&lcd_tc35876x_samhyd_wuxga, /* LCD device has to be first */
	&tablet_hdmi_device,
};

static struct omap_dss_device *dss_devices_tc35876x_sharp_lq101k1lyxx[] = {
	&lcd_tc35876x_sharp_lq101k1lyxx, /* LCD device has to be first */
	&tablet_hdmi_device,
};

static struct omap_dss_device *dss_devices_hdmi_default_display[] = {
	&tablet_hdmi_device,
};

static struct omap_dss_board_info tablet_dss_data_tc35876x_samhyd_wuxga = {
	.num_devices	= ARRAY_SIZE(dss_devices_tc35876x_samhyd_wuxga),
	.devices	= dss_devices_tc35876x_samhyd_wuxga,
	.default_device = &lcd_tc35876x_samhyd_wuxga,
};

static struct omap_dss_board_info tablet_dss_data_tc35876x_sharp_lq101k1lyxx = {
	.num_devices	= ARRAY_SIZE(dss_devices_tc35876x_sharp_lq101k1lyxx),
	.devices	= dss_devices_tc35876x_sharp_lq101k1lyxx,
	.default_device = &lcd_tc35876x_sharp_lq101k1lyxx,
};


static struct omap_dss_board_info tablet_dss_data_hdmi_default_display = {
	.num_devices	= ARRAY_SIZE(dss_devices_hdmi_default_display),
	.devices	= dss_devices_hdmi_default_display,
	.default_device	= &tablet_hdmi_device,
};
/* Allocate ( 18 + 9 ) MB for TILER1D slot size for WUXGA panel, total of
 * 54 MB of TILER1D
 */
static struct dsscomp_platform_data dsscomp_config_tc35876x_samhyd_wuxga = {
		.tiler1d_slotsz = (SZ_16M + SZ_2M + SZ_8M + SZ_1M),
};

static struct dsscomp_platform_data dsscomp_config_hdmi_display = {
		.tiler1d_slotsz = (SZ_16M + SZ_2M + SZ_8M + SZ_1M),
};

#ifdef CONFIG_FB_OMAP2_NUM_FBS
#define OMAPLFB_NUM_DEV CONFIG_FB_OMAP2_NUM_FBS
#else
#define OMAPLFB_NUM_DEV 1
#endif

static struct sgx_omaplfb_config omaplfb_config_tc35876x_samhyd_wuxga[OMAPLFB_NUM_DEV] = {
	{
	.vram_buffers = 2,
	.swap_chain_length = 2,
	}
};

static struct sgx_omaplfb_config omaplfb_config_hdmi_default_display[OMAPLFB_NUM_DEV] = {
	{
	.vram_buffers = 2,
	.swap_chain_length = 2,
	}
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data_tc35876x_samhyd_wuxga = {
	.num_configs = OMAPLFB_NUM_DEV,
	.configs = omaplfb_config_tc35876x_samhyd_wuxga,
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data_hdmi_default_display = {
	.num_configs = OMAPLFB_NUM_DEV,
	.configs = omaplfb_config_hdmi_default_display,
};
static struct omap_tablet_panel_data panel_data_tc35876x_samhyd_wuxga = {
	.board_info = &tablet_dss_data_tc35876x_samhyd_wuxga,
	.dsscomp_data = &dsscomp_config_tc35876x_samhyd_wuxga,
	.omaplfb_data = &omaplfb_plat_data_tc35876x_samhyd_wuxga,
};

static struct omap_tablet_panel_data panel_data_tc35876x_sharp_lq101k1lyxx = {
	.board_info = &tablet_dss_data_tc35876x_sharp_lq101k1lyxx,
	.dsscomp_data = NULL,
	.omaplfb_data = NULL,
};

static struct omap_tablet_panel_data panel_data_hdmi_default_display  = {
	.board_info = &tablet_dss_data_hdmi_default_display,
	.dsscomp_data = &dsscomp_config_hdmi_display,
	.omaplfb_data = &omaplfb_plat_data_hdmi_default_display,
};

static struct omap_tablet_panel_data *get_panel_data(enum omap_44xx_tablet_panel_type panel_type)
{

	if (omap_android_display_is_default(&tablet_hdmi_device)) {
		return &panel_data_hdmi_default_display;
	}

	switch (panel_type) {
	case TC35876x_SAMSUNG_HYDIS_WUXGA: /* HYDIS & Samsung equivalent */
		return &panel_data_tc35876x_samhyd_wuxga;
		break;
	case TC35876x_SHARP_LQ101K1LYxx_WXGA:
	default:
		if (omap_is_board_version(OMAP4_TABLET_1_0) ||
		    omap_is_board_version(OMAP4_TABLET_1_1) ||
		    omap_is_board_version(OMAP4_TABLET_1_2)) {
			lcd_tc35876x_sharp_lq101k1lyxx.panel.timings.x_res = 1024;
			lcd_tc35876x_sharp_lq101k1lyxx.panel.timings.y_res = 768;
		}
		return &panel_data_tc35876x_sharp_lq101k1lyxx;
	}
}

static void tablet_lcd_init(void)
{
	u32 reg;
	int status;

	/* Enable 3 lanes in DSI1 module, disable pull down */
	reg = omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);
	reg &= ~OMAP4_DSI1_LANEENABLE_MASK;
	reg |= 0x1f << OMAP4_DSI1_LANEENABLE_SHIFT;
	reg &= ~OMAP4_DSI1_PIPD_MASK;
	reg |= 0x1f << OMAP4_DSI1_PIPD_SHIFT;
	omap4_ctrl_pad_writel(reg, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_DSIPHY);

	status = gpio_request_one(get_panel_data(tablet_panel_type)->board_info->devices[0]->reset_gpio,
				GPIOF_OUT_INIT_LOW, "lcd_reset_gpio");
	if (status)
		pr_err("%s: Could not get lcd_reset_gpio\n", __func__);
}

static struct omapfb_platform_data tablet_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
	},
};

static struct i2c_board_info __initdata omap4xx_i2c_bus2_d2l_info[] = {
	{
		I2C_BOARD_INFO("tc358765_i2c_driver", 0x0f),
	},
};

static int __init set_tablet_panel_type(char *str)
{
	int retval = 0;
	if (!strncmp("2202-002", str, 8) ||
	    !strncmp("2102-002", str, 8))
		tablet_panel_type = TC35876x_SAMSUNG_HYDIS_WUXGA;
	else if (!strncmp("2156-003", str, 8))
		tablet_panel_type = TC35876x_SHARP_LQ101K1LYxx_WXGA;
	else {
		pr_err("%s: No valid omapdss.board_id sent from bootloader, "
		       "will default to TC35876x_SHARP_LQ101K1LYxx_WXGA\n",
			__func__);
		retval = 1;
	}
	return retval;
}

early_param("omapdss.board_id", set_tablet_panel_type);

void tablet_android_display_setup(struct omap_ion_platform_data *ion)
{
	struct omap_tablet_panel_data *panel_data = get_panel_data(tablet_panel_type);
	omap_android_display_setup(panel_data->board_info,
				   panel_data->dsscomp_data,
				   panel_data->omaplfb_data,
				   &tablet_fb_pdata,
				   ion);
}

int __init tablet_panel_init(void)
{
	tablet_lcd_init();
	tablet_hdmi_mux_init();

	omapfb_set_platform_data(&tablet_fb_pdata);

	omap_display_init(get_panel_data(tablet_panel_type)->board_info);
	platform_device_register(&omap4_tablet_disp_led);

	i2c_register_board_info(2, omap4xx_i2c_bus2_d2l_info,
		ARRAY_SIZE(omap4xx_i2c_bus2_d2l_info));

	return 0;
}
