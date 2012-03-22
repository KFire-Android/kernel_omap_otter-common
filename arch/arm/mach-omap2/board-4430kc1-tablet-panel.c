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

#include <linux/i2c/twl.h>

#include <plat/vram.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/dmtimer.h>

#include "board-4430kc1-tablet.h"
#include "control.h"
#include "mux.h"


#define OMAP4_LCD_EN_GPIO		28
#define LED_SEC_DISP_GPIO		27 /* brightness = dsi1_bl_gpio */
#define DSI2_GPIO_59			59 /* direction output = dsi2_bl_glpio */

#define LED_PWM2ON			0x03
#define LED_PWM2OFF			0x04
#define TWL6030_TOGGLE3			0x92

void omap4_display_init(void)
{
	void __iomem *phymux_base = NULL;
	u32 val;

	phymux_base = ioremap(0x4A100000, 0x1000);

	/* GPIOs 101, 102 */
	val = __raw_readl(phymux_base + 0x90);
	val = (val & 0xFFF8FFF8) | 0x00030003;
	__raw_writel(val, phymux_base + 0x90);

	/* GPIOs 103, 104 */
	val = __raw_readl(phymux_base + 0x94);
	val = (val & 0xFFF8FFF8) | 0x00030003;
	__raw_writel(val, phymux_base + 0x94);

	iounmap(phymux_base);
}

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

void omap4_disp_led_init(void)
{
	pr_info("%s: enter\n", __func__);

	/* Seconday backlight control */
	gpio_request(DSI2_GPIO_59, "dsi2_bl_gpio");
	gpio_direction_output(DSI2_GPIO_59, 0);

	if (sdp4430_disp_led_data.flags & LEDS_CTRL_AS_ONE_DISPLAY) {
		pr_info("%s: Configuring as one display LED\n", __func__);
		gpio_set_value(DSI2_GPIO_59, 1);
	}
	gpio_request(LED_SEC_DISP_GPIO, "dsi1_bl_gpio");
	gpio_direction_output(LED_SEC_DISP_GPIO, 1);
	mdelay(120);
	gpio_set_value(LED_SEC_DISP_GPIO, 0);

	pr_info("%s: exit\n", __func__);
}

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

static struct omap_dss_device sdp4430_otter1_device = {
	.name			= "lcd2",
	.driver_name		= "otter1_panel_drv",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
#if 0
        .panel          = {
        	.width_in_um = 158,
        	.height_in_um = 92,
        },
#endif
};

static struct omap_dss_device *sdp4430_dss_devices[] = {
	&sdp4430_otter1_device,
};

static struct omap_dss_board_info sdp4430_dss_data = {
	.num_devices	=	ARRAY_SIZE(sdp4430_dss_devices),
	.devices	=	sdp4430_dss_devices,
	.default_device	=	&sdp4430_otter1_device,
};

static struct spi_board_info tablet_spi_board_info[] __initdata = {
	{
		.modalias		= "otter1_disp_spi",
		.bus_num		= 4,     /* McSPI4 */
		.chip_select		= 0,
		.max_speed_hz		= 375000,
	},
};

#define KC1_FB_RAM_SIZE                SZ_16M /* 1920×1080*4 * 2 */
static struct omapfb_platform_data kc1_fb_pdata = {
	.mem_desc = {
		.region_cnt = 1,
		.region = {
			[0] = {
				.size = KC1_FB_RAM_SIZE,
			},
		},
	},
};

static struct platform_device __initdata  sdp4430_disp_led = {
	.name = "display_led",
	.id = -1,
	.dev = {
		.platform_data = &sdp4430_disp_led_data,
	},
};

static struct led_pwm tablet_pwm_leds[] = {
	{
		.name		= "backlight",
		.pwm_id		= 2,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
};

static struct led_pwm_platform_data tablet_pwm_data = {
	.num_leds	= ARRAY_SIZE(tablet_pwm_leds),
	.leds		= tablet_pwm_leds,
};

static struct platform_device tablet_leds_pwm = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data = &tablet_pwm_data,
	},
};

static struct platform_device __initdata *sdp4430_panel_devices[] = {
//	&sdp4430_disp_led,
//	&sdp4430_keypad_led,
	&tablet_leds_pwm,
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

void omap4_kc1_display_init(void)
{
	platform_add_devices(sdp4430_panel_devices, ARRAY_SIZE(sdp4430_panel_devices));
	spi_register_board_info(tablet_spi_board_info,	ARRAY_SIZE(tablet_spi_board_info));
	omap_vram_set_sdram_vram(KC1_FB_RAM_SIZE, 0);
	omapfb_set_platform_data(&kc1_fb_pdata);
	omap_display_init(&sdp4430_dss_data);

	kc1_pmic_mux_init();
}

