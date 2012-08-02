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
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/leds-omap4430sdp-display.h>
#include <linux/platform_device.h>
#include <linux/omapfb.h>
#include <video/omapdss.h>
#include <linux/leds_pwm.h>
#include <linux/leds.h>
#include <linux/delay.h>
// #include <linux/pwm_backlight.h>

#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/consumer.h>

#include <plat/vram.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/dmtimer.h>
#include <plat/omap_device.h>
#include <plat/android-display.h>

#include "board-4430kc1-tablet.h"
#include "control.h"
#include "mux.h"
#include "mux44xx.h"
#include "dmtimer.h"


#define LED_PWM2ON			0x03
#define LED_PWM2OFF			0x04
#define TWL6030_TOGGLE3			0x92


static int tablet_panel_enable_lcd(struct omap_dss_device *dssdev)
{
	pr_info("Boxer LCD Enable!\n");
	return 0;
}

static void tablet_panel_disable_lcd(struct omap_dss_device *dssdev)
{
  	pr_info("Boxer LCD Disable!\n");
}

static struct omap_dss_device tablet_lcd_device = {
	.phy		= {
		.dpi	= {
			.data_lines	= 24,
		},
	},
	.clocks		= {
		.dispc	= {
			.channel	= {
				.lck_div        = 1,
				.pck_div        = 4,
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DISPC,
		},
#if 0
		.dsi	= {
			.regn		= 16, /*it is (N+1)*/
			.regm		= 115,
			.regm_dispc	= 3,
			.regm_dsi	= 3,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DSI,
		},
#endif
	},
        .panel          = {
		.config		= OMAP_DSS_LCD_TFT | OMAP_DSS_LCD_IVS |
				  OMAP_DSS_LCD_IHS,
		.timings	= {
			.x_res          = 1024,
			.y_res          = 600,
			.pixel_clock    = 46000, /* in kHz */
			.hfp            = 160,   /* HFP fix 160 */
			.hsw            = 10,    /* HSW = 1~140 */
			.hbp            = 150,   /* HSW + HBP = 160 */
			.vfp            = 12,    /* VFP fix 12 */
			.vsw            = 3,     /* VSW = 1~20 */
			.vbp            = 20,    /* VSW + VBP = 23 */
		},
        	.width_in_um = 158000,
        	.height_in_um = 92000,
        },
#if 0
	.ctrl = {
		.pixel_size = 24,
	},
#endif
	.name			= "lcd2",
	.driver_name		= "otter1_panel_drv",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
  	.platform_enable	= tablet_panel_enable_lcd,
  	.platform_disable	= tablet_panel_disable_lcd,
	.max_backlight_level	= 255,
};

static struct omap_dss_device *sdp4430_dss_devices[] = {
	&tablet_lcd_device,
};

static struct omap_dss_board_info sdp4430_dss_data = {
	.num_devices	=	ARRAY_SIZE(sdp4430_dss_devices),
	.devices	=	sdp4430_dss_devices,
	.default_device	=	&tablet_lcd_device,
};

#define OTTER_FB_RAM_SIZE                SZ_16M /* 1920×1080*4 * 2 */
static struct omapfb_platform_data sdp4430_fb_data = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = OTTER_FB_RAM_SIZE,
			},
		},
	},
};

static struct spi_board_info tablet_spi_board_info[] __initdata = {
	{
		.modalias		= "otter1_disp_spi",
		.bus_num		= 4,     /* McSPI4 */
		.chip_select		= 0,
		.max_speed_hz		= 375000,
	},
};

static void __init sdp4430_init_display_led(void)
{
	twl_i2c_write_u8(TWL_MODULE_PWM, 0xFF, LED_PWM2ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x7F, LED_PWM2OFF);
	//twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x30, TWL6030_TOGGLE3);
    twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x08, TWL6030_TOGGLE3);
    twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x38, TWL6030_TOGGLE3);
}

static void sdp4430_set_primary_brightness(u8 brightness)
{
	if (brightness > 1) {
		if (brightness == 255)
			brightness = 0x7f;
		else
			brightness = (~(brightness/2)) & 0x7f;

		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x30, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL_MODULE_PWM, brightness, LED_PWM2ON);
	} else if (brightness <= 1) {
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x08, TWL6030_TOGGLE3);
		twl_i2c_write_u8(TWL6030_MODULE_ID1, 0x38, TWL6030_TOGGLE3);
	}
}

static struct omap4430_sdp_disp_led_platform_data sdp4430_disp_led_data = {
	.flags = LEDS_CTRL_AS_ONE_DISPLAY,
	.display_led_init = sdp4430_init_display_led,
	.primary_display_set = sdp4430_set_primary_brightness,
};

static struct platform_device sdp4430_disp_led = {
	.name	=	"display_led",
	.id	=	-1,
	.dev	= {
		.platform_data = &sdp4430_disp_led_data,
	},
};

static struct platform_device sdp4430_keypad_led = {
	.name	=	"keypad_led",
	.id	=	-1,
	.dev	= {
		.platform_data = NULL,
	},
};

static void bkl_set_power(struct omap_pwm_led_platform_data *self, int on_off)
{
	pr_debug("%s: on_off:%d\n", __func__, on_off);

	msleep(1);
}

static struct omap_pwm_led_platform_data board_backlight_data = {
	.name			= "lcd-backlight",
	.default_trigger	= "backlight",
	.intensity_timer	= 10,
	.bkl_max		= 254,
	.bkl_min		= 0,
	.bkl_freq		= 30000,
//	.set_power		= &bkl_set_power,
};

static struct platform_device board_backlight_device = {
	.name		= "omap_pwm_led",
	.id		= 0,
	.dev.platform_data = &board_backlight_data,
};


static struct platform_device __initdata *sdp4430_panel_devices[] = {
	&sdp4430_disp_led,
	&sdp4430_keypad_led,
	&board_backlight_device,
};

void omap4_kc1_android_display_setup(struct omap_ion_platform_data *ion)
{
	omap_android_display_setup(&sdp4430_dss_data,
				   NULL,
				   NULL,
				   &sdp4430_fb_data,
				   ion);
}

void __init omap4_kc1_display_init(void)
{
	int ret;

	omapfb_set_platform_data(&sdp4430_fb_data);

	spi_register_board_info(tablet_spi_board_info,	ARRAY_SIZE(tablet_spi_board_info));

	omap_mux_enable_wkup("sys_nirq1");
	omap_mux_enable_wkup("sys_nirq2");

	platform_add_devices(sdp4430_panel_devices, ARRAY_SIZE(sdp4430_panel_devices));
	omap_display_init(&sdp4430_dss_data);
}

