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
#include <linux/pwm_backlight.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>

#include <linux/i2c/twl.h>

#include <plat/vram.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/dmtimer.h>
#include <plat/omap_device.h>

#include "board-4430kc1-tablet.h"
#include "control.h"
#include "mux.h"
#include "mux44xx.h"
#include "dmtimer.h"


#define OMAP4_LCD_EN_GPIO		28
#define LED_SEC_DISP_GPIO		27 /* brightness = dsi1_bl_gpio */
#define DSI2_GPIO_59			59 /* direction output = dsi2_bl_glpio */

#define LED_PWM2ON			0x03
#define LED_PWM2OFF			0x04
#define TWL6030_TOGGLE3			0x92

#if 0
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

static struct omap4430_sdp_disp_led_platform_data sdp4430_disp_led_data __initdata = {
	.flags = LEDS_CTRL_AS_ONE_DISPLAY,
	.display_led_init = sdp4430_init_display_led,
	.primary_display_set = sdp4430_set_primary_brightness,
};
#endif

static struct regulator_consumer_supply lcd_supply[] = {
	{ .supply = "vlcd" },
};

static struct regulator_init_data lcd_vinit = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = lcd_supply,
};

static struct fixed_voltage_config lcd_reg_data = {
	.supply_name = "vdd_lcd",
	.microvolts = 3300000,
	.gpio = 47,
	.enable_high = 1,
	.enabled_at_boot = 1,
	.init_data = &lcd_vinit,
};

static struct platform_device lcd_regulator_device = {
	.name   = "reg-fixed-voltage",
	.id     = -1,
	.dev    = {
		.platform_data = &lcd_reg_data,
	},
};

void kc1_led_set_power(struct omap_pwm_led_platform_data *self, int on_off)
{
	if (on_off) {
		gpio_request(119, "ADO_SPK_ENABLE");
		gpio_direction_output(119, 1);
		gpio_set_value(119, 1);
		gpio_request(120, "SKIPB_GPIO");
		gpio_direction_output(120, 1);
		gpio_set_value(120, 1);
	} else {
		gpio_request(119, "ADO_SPK_ENABLE");
		gpio_direction_output(119, 0);
		gpio_set_value(119, 0);
		gpio_request(120, "SKIPB_GPIO");
		gpio_direction_output(120, 0);
		gpio_set_value(120, 0);
	}
}

static int tablet_panel_enable_lcd(struct omap_dss_device *dssdev)
{
	pr_info("Boxer LCD Enable!\n");
	return 0;
}

static void tablet_panel_disable_lcd(struct omap_dss_device *dssdev)
{
  	pr_info("Boxer LCD Disable!\n");
}

static int tablet_set_bl_intensity(struct omap_dss_device *dssdev, int brightness)
{
	pr_info("Boxer LCD Set BL Intensity == %d!\n", brightness);
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
	return 0;
}

static struct omap_dss_device tablet_lcd_device = {
	.phy		= {
		.dpi	= {
			.data_lines	= 24,
		},
	},
#if 0
	.clocks		= {
		.dispc	= {
			.channel	= {
				.lck_div        = 1,
				.pck_div        = 4,
				.lcd_clk_src    = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src = OMAP_DSS_CLK_SRC_FCK,
		},
		.dsi	= {
			.regn		= 16, /*it is (N+1)*/
			.regm		= 115,
			.regm_dispc	= 3,
			.regm_dsi	= 3,
			.dsi_fclk_src   = OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DSI,
		},
	},
#endif
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
//	.reset_gpio     	= 102,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
  	.platform_enable	= tablet_panel_enable_lcd,
  	.platform_disable	= tablet_panel_disable_lcd,
 	// .set_backlight	= tablet_set_bl_intensity,
//	.caps			= OMAP_DSS_DISPLAY_CAP_MANUAL_UPDATE,
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

static struct spi_board_info tablet_spi_board_info[] __initdata = {
	{
		.modalias		= "otter1_disp_spi",
		.bus_num		= 4,     /* McSPI4 */
		.chip_select		= 0,
		.max_speed_hz		= 375000,
	},
};

static int kc1_backlight_init(struct device *dev)
{
	struct platform_pwm_backlight_data *data = dev->platform_data;
	pr_info("OMAP PWM LED (pwm-backlight) at GP timer %d\n", data->pwm_id);
	return 0;
}

static int kc1_backlight_notify(struct device *dev, int brightness)
{
	pr_info("kc1_backlight_notify (brightness == %d)\n", brightness);
	if (!brightness) {
		/* Power Timer Off */
	} else {
		/* Enable Timer */
	}
	return brightness;
}

static void kc1_backlight_exit(struct device *dev)
{
	pr_info("kc1_backlight_exit\n");
}

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id         = 10,
	.max_brightness = 0xFF,
	.dft_brightness = 0x7F,
	.pwm_period_ns  = 32768, //7812500
	.init           = kc1_backlight_init,
	.notify		= kc1_backlight_notify,
	.exit           = kc1_backlight_exit,
	// .check_fb	= kc1_backlight_checkfb,
};

static struct platform_device kc1_backlight = {
	.name = "pwm-backlight",
	.dev  = {
		.platform_data = &backlight_data,
	},
	.id   = -1,
};

static struct platform_device __initdata *sdp4430_panel_devices[] = {
	&lcd_regulator_device,
	&kc1_backlight,
};

static void kc1_pmic_mux_init(void)
{
	/*
	 * Enable IO daisy for sys_nirq1/2, to be able to
	 * wakeup from interrupts from PMIC/Audio IC.
	 * Needed only in Device OFF mode.
	 */
	omap_mux_init_signal("sys_nirq1", OMAP_PIN_INPUT_PULLUP | OMAP_WAKEUP_EN);
	omap_mux_init_signal("sys_nirq2", OMAP_PIN_INPUT_PULLUP | OMAP_WAKEUP_EN);
}

void __init omap4_kc1_display_init(void)
{
	spi_register_board_info(tablet_spi_board_info,	ARRAY_SIZE(tablet_spi_board_info));

	omap_display_init(&sdp4430_dss_data);

	platform_add_devices(sdp4430_panel_devices, ARRAY_SIZE(sdp4430_panel_devices));

	kc1_pmic_mux_init();
}

