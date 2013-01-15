/*
 * arch/arm/mach-omap2/board-44xx-tablet-panel.c
 *
 * Copyright (C) 2011,2012 Texas Instruments
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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c/twl.h>
#include <linux/leds-omap4430sdp-display.h>

#include <plat/board.h>
#include <plat/vram.h>
#include <plat/android-display.h>

#include <video/omapdss.h>
#include <video/omap-panel-tc358765.h>

#include "mux.h"
#include "control.h"
#include "board-44xx-tablet.h"

#define TABLET_FB_RAM_SIZE		SZ_16M /* 1920×1080*4 * 2 */

/* PWM2 and TOGGLE3 register offsets */
#define LED_PWM2ON		0x03
#define LED_PWM2OFF		0x04
#define TWL6030_TOGGLE3		0x92
#define PWM2EN			BIT(5)
#define PWM2S			BIT(4)
#define PWM2R			BIT(3)
#define PWM2CTL_MASK		(PWM2EN | PWM2S | PWM2R)

/* HDMI GPIOs */
#define HDMI_GPIO_CT_CP_HPD			60 /* HPD mode enable/disable */
#define HDMI_GPIO_HPD				63 /* Hot plug pin for HDMI */
#define HDMI_GPIO_LS_OE				41 /* Level shifter for HDMI */

static void omap4_tablet_init_display_led(void)
{
	/* Set maximum brightness on init */
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x00, LED_PWM2ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x00, LED_PWM2OFF);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, PWM2S | PWM2EN, TWL6030_TOGGLE3);
}

static void omap4_tablet_set_primary_brightness(u8 brightness)
{
	u8 val;
	static unsigned enabled = 0x01;

	if (brightness) {

		/*
		 * Converting brightness 8-bit value into 7-bit value
		 * for PWM cycles.
		 * We need to check brightness maximum value for set
		 * PWM2OFF equal to PWM2ON.
		 * Aditionally checking for 1 for prevent seting maximum
		 * brightness in this case.
		 */

		brightness >>= (brightness ^ 0xFF) ?
				 ((brightness ^ 0x01) ? 1 : 0) : 8;

		if (twl_i2c_write_u8(TWL_MODULE_PWM, brightness, LED_PWM2OFF))
				goto io_err;

		/* Enable PWM2 just once */
		if (!enabled) {
			if (twl_i2c_read_u8(TWL6030_MODULE_ID1, &val,
							TWL6030_TOGGLE3))
				goto io_err;

			val &= ~PWM2CTL_MASK;
			val |= (PWM2S | PWM2EN);
			if (twl_i2c_write_u8(TWL6030_MODULE_ID1, val,
					 TWL6030_TOGGLE3))
				goto io_err;
			enabled = 0x01; }
	} else {
		/* Disable PWM2 just once */
		if (enabled) {
			if (twl_i2c_read_u8(TWL6030_MODULE_ID1, &val,
							TWL6030_TOGGLE3))
				goto io_err;
			val &= ~PWM2CTL_MASK;
			val |= PWM2R;
			if (twl_i2c_write_u8(TWL6030_MODULE_ID1, val,
							TWL6030_TOGGLE3))
				goto io_err;
			val |= (PWM2EN | PWM2S);
			if (twl_i2c_write_u8(TWL6030_MODULE_ID1, val,
							TWL6030_TOGGLE3))
				goto io_err;
			enabled = 0x00;
		}
	}
	return;
io_err:
	pr_err("%s: Error occured during adjust PWM2\n", __func__);
}

static struct omap4430_sdp_disp_led_platform_data tablet_disp_led_data = {
	.display_led_init = omap4_tablet_init_display_led,
	.primary_display_set = omap4_tablet_set_primary_brightness,
};

static struct platform_device tablet_disp_led = {
		.name	=	"display_led",
		.id	=	-1,
		.dev	= {
		.platform_data = &tablet_disp_led_data,
		},
};

static struct tc358765_board_data tablet_dsi_panel = {
	.lp_time	= 0x4,
	.clrsipo	= 0x3,
	.lv_is		= 0x1,
	.lv_nd		= 0x6,
	.evtmode	= 0x1,
	.vsdelay	= 0xf02,
};

struct omap_video_timings tablet_dispc_timings = {
	.x_res = 1280,
	.y_res = 800,
	.hfp = 243,
	.hsw = 9,
	.hbp = 20,
	.vfp = 6,
	.vsw = 2,
	.vbp = 4,
};

static struct omap_dss_device tablet_lcd_device = {
	.name                   = "lcd",
	.driver_name            = "tc358765",
	.type                   = OMAP_DISPLAY_TYPE_DSI,
	.data			= &tablet_dsi_panel,
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

		.module		= 0,
	},

	.clocks = {
		.dispc = {
			 .channel = {
				.lck_div        = 1,
				.pck_div        = 2,
				.lcd_clk_src    =
					OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
		},

		.dsi = {
			.regn           = 38,
			.regm           = 441,
			.regm_dispc     = 6,
			.regm_dsi       = 9,
			.lp_clk_div     = 5,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},

	.panel = {
		.timings = {
			.x_res		= 1280,
			.y_res		= 800,
			.pixel_clock	= 63530,
			.hfp		= 6,
			.hsw		= 12,
			.hbp		= 6,
			.vfp		= 4,
			.vsw		= 4,
			.vbp		= 4,
		},
		.dsi_mode = OMAP_DSS_DSI_VIDEO_MODE,
		.dsi_vm_data = {
				.hsa			= 0,
				.hfp			= 4,
				.hbp			= 3,
				.vsa			= 2,
				.vfp			= 6,
				.vbp			= 4,

				.vp_de_pol		= true,
				.vp_vsync_pol		= true,
				.vp_hsync_pol		= false,
				.vp_hsync_end		= false,
				.vp_vsync_end		= false,

				.blanking_mode		= 0,
				.hsa_blanking_mode	= 1,
				.hfp_blanking_mode	= 1,
				.hbp_blanking_mode	= 1,

				.ddr_clk_always_on	= true,

				.window_sync		= 4,
		}
	},

	.ctrl = {
		.pixel_size = 24,
	},

	.reset_gpio     = 102,
	.channel = OMAP_DSS_CHANNEL_LCD,
	.platform_enable = NULL,
	.platform_disable = NULL,
	.dispc_timings = &tablet_dispc_timings,
};

static int tablet_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	return 0;
}

static void tablet_panel_disable_hdmi(struct omap_dss_device *dssdev)
{
}

static struct omap_dss_hdmi_data tablet_hdmi_data = {
	.hpd_gpio = HDMI_GPIO_HPD,
	.ls_oe_gpio = HDMI_GPIO_LS_OE,
	.ct_cp_hpd_gpio = HDMI_GPIO_CT_CP_HPD,
};

static struct omap_dss_device tablet_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.platform_enable = tablet_panel_enable_hdmi,
	.platform_disable = tablet_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
	.data = &tablet_hdmi_data,
};

static struct omap_dss_device *tablet_dss_devices[] = {
	&tablet_lcd_device,
	&tablet_hdmi_device,
};

static struct omap_dss_device *dss_devices_hdmi_default_display[] = {
	&tablet_hdmi_device,
};

static struct dsscomp_platform_data dsscomp_config_hdmi_display = {
	.tiler1d_slotsz = (SZ_16M + SZ_2M + SZ_8M + SZ_1M),
};

static struct sgx_omaplfb_config omaplfb_config_hdmi_default_display[] = {
	{
		.vram_buffers = 2,
		.swap_chain_length = 2,
	}
};

static struct sgx_omaplfb_platform_data omaplfb_plat_data_hdmi_default_display = {
	.num_configs = ARRAY_SIZE(omaplfb_config_hdmi_default_display),
	.configs = omaplfb_config_hdmi_default_display,
};

static struct omap_dss_board_info tablet_dss_data = {
	.num_devices	= ARRAY_SIZE(tablet_dss_devices),
	.devices	= tablet_dss_devices,
	.default_device	= &tablet_lcd_device,
};

static struct omap_dss_board_info tablet_dss_data_hdmi_default_display = {
	.num_devices    = ARRAY_SIZE(dss_devices_hdmi_default_display),
	.devices        = dss_devices_hdmi_default_display,
	.default_device = &tablet_hdmi_device,
};

static struct omapfb_platform_data tablet_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
	},
};

static void tablet_hdmi_init(void)
{
	/*
	 * OMAP4460SDP/Blaze and OMAP4430 ES2.3 SDP/Blaze boards and
	 * later have external pull up on the HDMI I2C lines
	 */
	if (cpu_is_omap446x() || omap_rev() > OMAP4430_REV_ES2_2)
		omap_hdmi_init(OMAP_HDMI_SDA_SCL_EXTERNAL_PULLUP);
	else
		omap_hdmi_init(0);

	omap_mux_init_gpio(HDMI_GPIO_LS_OE, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(HDMI_GPIO_CT_CP_HPD, OMAP_PIN_OUTPUT);
	omap_mux_init_gpio(HDMI_GPIO_HPD, OMAP_PIN_INPUT_PULLDOWN);
}

static void __init tablet_lcd_init(void)
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

	status = gpio_request_one(tablet_lcd_device.reset_gpio,
				GPIOF_OUT_INIT_LOW, "lcd_reset_gpio");
	gpio_set_value(tablet_lcd_device.reset_gpio, 1);
	if (status)
		pr_err("%s: Could not get lcd_reset_gpio\n", __func__);
}

static struct i2c_board_info __initdata omap4xx_i2c_bus2_d2l_info[] = {
	{
		I2C_BOARD_INFO("tc358765_i2c_driver", 0x0f),
	},
};

void tablet_android_display_setup(void)
{
	if (omap_android_display_is_default(&tablet_hdmi_device))
		omap_android_display_setup(
			&tablet_dss_data_hdmi_default_display,
			&dsscomp_config_hdmi_display,
			&omaplfb_plat_data_hdmi_default_display,
			&tablet_fb_pdata);
	else
		omap_android_display_setup(&tablet_dss_data,
			NULL,
			NULL,
			&tablet_fb_pdata);
}

int __init tablet_display_init(void)
{
	omap_mux_init_signal("fref_clk4_out.fref_clk4_out",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);

	platform_device_register(&tablet_disp_led);
	tablet_lcd_init();

	omapfb_set_platform_data(&tablet_fb_pdata);
	omap_vram_set_sdram_vram(TABLET_FB_RAM_SIZE, 0);
	if (omap_android_display_is_default(&tablet_hdmi_device))
		omap_display_init(&tablet_dss_data_hdmi_default_display);
	else
		omap_display_init(&tablet_dss_data);

	tablet_hdmi_init();

	i2c_register_board_info(2, omap4xx_i2c_bus2_d2l_info,
		ARRAY_SIZE(omap4xx_i2c_bus2_d2l_info));

	return 0;
}
