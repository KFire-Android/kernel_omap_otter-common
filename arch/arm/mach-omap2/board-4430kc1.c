/*
 * Board support file for OMAP4430 SDP.
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Author: Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Based on mach-omap2/board-3430sdp.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/usb/otg.h>
#include <linux/spi/spi.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/machine.h>
#if 0
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#endif
#include <linux/leds-omap4430sdp-display.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/twl6040-vib.h>
#include <linux/wl12xx.h>
#include <linux/bootmem.h>

#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <mach/emif.h>
#include <mach/lpddr2-elpida.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/control.h>
#include <plat/timer-gp.h>
#include <plat/display.h>
#include <plat/usb.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#ifdef CONFIG_SERIAL_OMAP
#include <plat/omap-serial.h>
#include <plat/serial.h>
#endif
#include <linux/wakelock.h>
#include <plat/opp_twl_tps.h>
#include <plat/mmc.h>

#include <plat/hwspinlock.h>
#include <plat/dmtimer.h>
#include "mux.h"
#include "hsmmc.h"
#include "smartreflex-class3.h"
#include "board-4430sdp-wifi.h"

#include <linux/skbuff.h>
#include <linux/ti_wilink_st.h>

#define OMAP4_LCD_EN_GPIO		28

#define OMAP4_TOUCH_RESET_GPIO			18
#define OMAP4_TOUCH_IRQ_1		35
#define OMAP4_CHARGER_IRQ       7
#define OMAP4SDP_MDM_PWR_EN_GPIO	157
#define OMAP_UART_GPIO_MUX_MODE_143	143

#ifdef CONFIG_DEMO_HDMI
#define OMAP4_ADI7526_IRQ		122
#define OMAP4_ADI7526_5V_EN		56
#endif

#define LED_SEC_DISP_GPIO 27
#define DSI2_GPIO_59	59
#define PANEL_IRQ       34

#define LED_PWM2ON		0x03
#define LED_PWM2OFF		0x04
#define LED_TOGGLE3		0x92

#define GPIO_WIFI_PMENA		54
#define GPIO_WIFI_IRQ		53

#define MBID0_GPIO 174
#define MBID1_GPIO 173
#define MBID2_GPIO 178
#define MBID3_GPIO 177
#define PANELID0_GPIO 176
#define PANELID1_GPIO 175
#define TOUCHID0_GPIO 50
#define TOUCHID1_GPIO 51

#define TWL6030_RTC_GPIO 6
#define BLUETOOTH_UART UART2
#define BLUETOOTH_UART_DEV_NAME "/dev/ttyO1"
#define CONSOLE_UART UART3

static struct wake_lock uart_lock;
static struct platform_device sdp4430_hdmi_audio_device = {
	.name		= "hdmi-dai",
	.id		= -1,
};

static struct platform_device sdp4430_aic3110 = {
        .name           = "tlv320aic3110-codec",
        .id             = -1,
};

#if defined(CONFIG_SENSORS_OMAP_BANDGAP)
static struct platform_device sdp4430_omap_bandgap_sensor = {
	.name           = "omap_bandgap_sensor",
	.id             = -1,
};
#endif

#if defined(CONFIG_SENSORS_PMIC_THERMAL)
static struct platform_device sdp4430_pmic_thermal_sensor = {
	.name		= "pmic_thermal_sensor",
	.id		= -1,
};
#endif

/* Board IDs */
static u8 quanta_mbid;
static u8 quanta_touchid;
static u8 quanta_panelid;
u8 quanta_get_mbid(void)
{
	return quanta_mbid;
}
EXPORT_SYMBOL(quanta_get_mbid);

u8 quanta_get_touchid(void)
{
	return quanta_touchid;
}
EXPORT_SYMBOL(quanta_get_touchid);

u8 quanta_get_panelid(void)
{
	return quanta_panelid;
}
EXPORT_SYMBOL(quanta_get_panelid);

static void __init quanta_boardids(void)
{
    gpio_request(MBID0_GPIO, "MBID0");
    gpio_direction_input(MBID0_GPIO);
    gpio_request(MBID1_GPIO, "MBID1");
    gpio_direction_input(MBID1_GPIO);
    gpio_request(MBID2_GPIO, "MBID2");
    gpio_direction_input(MBID2_GPIO);
    gpio_request(MBID3_GPIO, "MBID3");
    gpio_direction_input(MBID3_GPIO);
    gpio_request(PANELID0_GPIO, "PANELID0");
    gpio_direction_input(PANELID0_GPIO);
    gpio_request(PANELID1_GPIO, "PANELID1");
    gpio_direction_input(PANELID1_GPIO);
    gpio_request(TOUCHID0_GPIO, "TOUCHID0");
    gpio_direction_input(TOUCHID0_GPIO);
    gpio_request(TOUCHID1_GPIO, "TOUCHID1");
    gpio_direction_input(TOUCHID1_GPIO);
    quanta_mbid=gpio_get_value(MBID0_GPIO) | ( gpio_get_value(MBID1_GPIO)<<1)
        | ( gpio_get_value(MBID2_GPIO)<<2) | ( gpio_get_value(MBID3_GPIO)<<3);
    quanta_touchid = gpio_get_value(TOUCHID0_GPIO) | ( gpio_get_value(TOUCHID1_GPIO)<<1);
    quanta_panelid = gpio_get_value(PANELID0_GPIO) | ( gpio_get_value(PANELID1_GPIO)<<1);
    //printk("=%s===andrew====== mbid=%d  touchid=%d   panelid=%d\n", __func__,quanta_mbid,quanta_touchid,quanta_panelid);
}

static struct spi_board_info sdp4430_spi_board_info[] __initdata = {
#if 0
	{
		.modalias               = "ks8851",
		.bus_num                = 1,
		.chip_select            = 0,
		.max_speed_hz           = 24000000,
		.irq                    = ETH_KS8851_IRQ,
	},
#endif
	{
		.modalias		= "otter1_disp_spi",
		.bus_num		= 4,     /* McSPI4 */
		.chip_select		= 0,
		.max_speed_hz		= 375000,
	},
};

#if 0
static struct gpio_led sdp4430_gpio_leds[] = {
	{
		.name	= "omap4:green:debug0",
		.gpio	= 61,
	},
	{
		.name	= "omap4:green:debug1",
		.gpio	= 30,
	},
	{
		.name	= "omap4:green:debug2",
		.gpio	= 7,
	},
	{
		.name	= "omap4:green:debug3",
		.gpio	= 8,
	},
	{
		.name	= "omap4:green:debug4",
		.gpio	= 50,
	},
	{
		.name	= "blue",
		.default_trigger = "timer",
		.gpio	= 169,
	},
	{
		.name	= "red",
		.default_trigger = "timer",
		.gpio	= 170,
	},
	{
		.name	= "green",
		.default_trigger = "timer",
		.gpio	= 139,
	},
};

static struct gpio_led_platform_data sdp4430_led_data = {
	.leds	= sdp4430_gpio_leds,
	.num_leds	= ARRAY_SIZE(sdp4430_gpio_leds),
};

static struct platform_device sdp4430_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &sdp4430_led_data,
	},
};

static struct led_pwm sdp4430_pwm_leds[] = {
	{
	.name = "battery-led",
	.pwm_id = 1,
	.max_brightness = 255,
	.pwm_period_ns = 7812500,
	},
};

static struct led_pwm_platform_data sdp4430_pwm_data = {
	.num_leds = 1,
	.leds = sdp4430_pwm_leds,
};

static struct platform_device sdp4430_leds_pwm = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data = &sdp4430_pwm_data,
	},
};
#endif

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

static void sdp4430_set_secondary_brightness(u8 brightness)
{
	if (brightness > 0)
		brightness = 1;

	gpio_set_value(LED_SEC_DISP_GPIO, brightness);
}

static struct omap4430_sdp_disp_led_platform_data sdp4430_disp_led_data = {
	.flags = LEDS_CTRL_AS_ONE_DISPLAY,
	.display_led_init = sdp4430_init_display_led,
	.primary_display_set = sdp4430_set_primary_brightness,
	//.secondary_display_set = sdp4430_set_secondary_brightness,
};

static void __init omap_disp_led_init(void)
{
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

}

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

void kc1_led_set_power(struct omap_pwm_led_platform_data *self,
        int on_off)
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

static struct omap_pwm_led_platform_data kc1_led_data = {
    .name = "backlight",
    .intensity_timer = 10,
    //.def_on = 1,
    .def_brightness = 0x7F,
    //.blink_timer = 11,
    //.set_power = kc1_led_set_power,
};

static struct platform_device kc1_led_device = {
    .name       = "omap_pwm_led",
    .id     = -1,
    .dev        = {
        .platform_data = &kc1_led_data,
    },
};

static int sdp4430_panel_enable_otter1(struct omap_dss_device *dssdev)
{
#if 0
	gpio_request(DLP_4430_GPIO_59, "DLP DISPLAY SEL");
	gpio_direction_output(DLP_4430_GPIO_59, 0);
	gpio_request(DLP_4430_GPIO_45, "DLP PARK");
	gpio_direction_output(DLP_4430_GPIO_45, 0);
	gpio_request(DLP_4430_GPIO_40, "DLP PHY RESET");
	gpio_direction_output(DLP_4430_GPIO_40, 0);
	mdelay(100);

	gpio_set_value(DLP_4430_GPIO_45, 1);
	gpio_set_value(DLP_4430_GPIO_40, 1);
	mdelay(300);

	gpio_request(101, "OTTER1 LCD PWR EN");
	gpio_direction_output(101, 1);
	gpio_request(102, "OTTER1 BL PWR EN");
	gpio_direction_output(102, 1);
	gpio_request(103, "OTTER1 CABC0");
	gpio_direction_output(103, 0);
	gpio_request(104, "OTTER1 CABC1");
	gpio_direction_output(104, 0);
#else
	gpio_request(37, "OMAP_RGB_SHTDN");
	gpio_direction_output(37, 1);
	gpio_set_value(37, 1);
#endif

	return 0;
}



static void sdp4430_panel_disable_otter1(struct omap_dss_device *dssdev)

{
	gpio_set_value(DLP_4430_GPIO_40, 0);
	gpio_set_value(DLP_4430_GPIO_45, 0);
}

static struct omap_dss_device sdp4430_otter1_device = {
	.name			= "lcd2",
	.driver_name		= "otter1_panel_drv",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	//.platform_enable	= sdp4430_panel_enable_otter1,
	//.platform_disable	= sdp4430_panel_disable_otter1,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
        .panel          = {
        .width_in_mm = 158,
        .height_in_mm = 92,
        },
};

#if 0
static struct nokia_dsi_panel_data dsi_panel = {
		.name	= "taal",
		.reset_gpio	= 102,
		.use_ext_te	= false,
		.ext_te_gpio	= 101,
		.use_esd_check	= false,
		.set_backlight	= NULL,
};

static struct nokia_dsi_panel_data dsi2_panel = {
		.name   = "taal2",
		.reset_gpio     = 104,
		.use_ext_te     = false,
		.ext_te_gpio    = 103,
		.use_esd_check  = false,
		.set_backlight  = NULL,
};

static struct omap_dss_device sdp4430_lcd_device = {
	.name			= "lcd",
	.driver_name		= "taal",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi_panel,
	.phy.dsi		= {
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,
		.div		= {
			.lck_div	= 1,
			.pck_div	= 5,
			.regm		= 150,
			.regn		= 17,
			.regm_dispc	= 4,
			.regm_dsi	= 4,
			.lp_clk_div	= 8,
		},
	},
	.channel		= OMAP_DSS_CHANNEL_LCD,
};

static struct omap_dss_device sdp4430_lcd2_device = {
	.name			= "lcd2",
	.driver_name		= "taal2",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi2_panel,
	.phy.dsi		= {
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,
		.div		= {
			.lck_div	= 1,
			.pck_div	= 5,
			.regm		= 150,
			.regn		= 17,
			.regm_dispc	= 4,
			.regm_dsi	= 4,
			.lp_clk_div	= 8,
		},
	},
	.channel		= OMAP_DSS_CHANNEL_LCD2,
};
#endif

#ifdef CONFIG_OMAP2_DSS_HDMI
static int sdp4430_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	gpio_request(HDMI_GPIO_60 , "hdmi_gpio_60");
	gpio_request(HDMI_GPIO_41 , "hdmi_gpio_41");
	gpio_direction_output(HDMI_GPIO_60, 1);
	gpio_direction_output(HDMI_GPIO_41, 1);
	gpio_set_value(HDMI_GPIO_60, 0);
	gpio_set_value(HDMI_GPIO_41, 0);
	mdelay(5);
	gpio_set_value(HDMI_GPIO_60, 1);
	gpio_set_value(HDMI_GPIO_41, 1);
	return 0;
}

static void sdp4430_panel_disable_hdmi(struct omap_dss_device *dssdev)
{
}

static __attribute__ ((unused)) void __init sdp4430_hdmi_init(void)
{
}

static struct omap_dss_device sdp4430_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.phy.dpi.data_lines = 24,
	.platform_enable = sdp4430_panel_enable_hdmi,
	.platform_disable = sdp4430_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
};
#endif /* CONFIG_OMAP2_DSS_HDMI */

#ifdef CONFIG_PANEL_PICO_DLP
static int sdp4430_panel_enable_pico_DLP(struct omap_dss_device *dssdev)
{
	/* int i = 0; */

	gpio_request(DLP_4430_GPIO_59, "DLP DISPLAY SEL");
	gpio_direction_output(DLP_4430_GPIO_59, 0);
	gpio_request(DLP_4430_GPIO_45, "DLP PARK");
	gpio_direction_output(DLP_4430_GPIO_45, 0);
	gpio_request(DLP_4430_GPIO_40, "DLP PHY RESET");
	gpio_direction_output(DLP_4430_GPIO_40, 0);
	/* gpio_request(DLP_4430_GPIO_44, "DLP READY RESET");
	gpio_direction_input(DLP_4430_GPIO_44); */
	mdelay(500);

	gpio_set_value(DLP_4430_GPIO_59, 1);
	gpio_set_value(DLP_4430_GPIO_45, 1);
	mdelay(1000);

	gpio_set_value(DLP_4430_GPIO_40, 1);
	mdelay(1000);

	/* FIXME with the MLO gpio changes,
		gpio read is not retuning correct value even though
		it is  set in hardware so the check is comment
		till the problem is fixed */
	/* while (i == 0)
		i = gpio_get_value(DLP_4430_GPIO_44); */

	mdelay(2000);
	return 0;
}

static void sdp4430_panel_disable_pico_DLP(struct omap_dss_device *dssdev)
{
	gpio_set_value(DLP_4430_GPIO_40, 0);
	gpio_set_value(DLP_4430_GPIO_45, 0);
	gpio_set_value(DLP_4430_GPIO_59, 0);
}

static struct omap_dss_device sdp4430_picoDLP_device = {
	.name			= "pico_DLP",
	.driver_name		= "picoDLP_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.platform_enable	= sdp4430_panel_enable_pico_DLP,
	.platform_disable	= sdp4430_panel_disable_pico_DLP,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
};
#endif /* CONFIG_PANEL_PICO_DLP */

static struct omap_dss_device *sdp4430_dss_devices[] = {
//	&sdp4430_lcd_device,
//	&sdp4430_lcd2_device,
	&sdp4430_otter1_device,
#ifdef CONFIG_OMAP2_DSS_HDMI
	&sdp4430_hdmi_device,
#endif /* CONFIG_OMAP2_DSS_HDMI */
#ifdef CONFIG_PANEL_PICO_DLP
	&sdp4430_picoDLP_device,
#endif /* CONFIG_PANEL_PICO_DLP */
};

static struct omap_dss_board_info sdp4430_dss_data = {
	.num_devices	=	ARRAY_SIZE(sdp4430_dss_devices),
	.devices	=	sdp4430_dss_devices,
	.default_device	=	&sdp4430_otter1_device,
};

#if 0
static unsigned long retry_suspend;
int plat_kim_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct kim_data_s *kim_gdata;
	struct st_data_s *core_data;
	kim_gdata = dev_get_drvdata(&pdev->dev);
	core_data = kim_gdata->core_data;
	 if (st_ll_getstate(core_data) != ST_LL_INVALID) {
		 /*Prevent suspend until sleep indication from chip*/
		   while(st_ll_getstate(core_data) != ST_LL_ASLEEP &&
				   (retry_suspend++ < 5)) {
			   return -1;
		   }
	 }
	return 0;
}
int plat_kim_resume(struct platform_device *pdev)
{
	retry_suspend = 0;
	return 0;
}
/* wl128x BT, FM, GPS connectivity chip */
struct ti_st_plat_data wilink_pdata = {
	.nshutdown_gpio = 55,
	.dev_name = BLUETOOTH_UART_DEV_NAME,
	.flow_cntrl = 1,
	.baud_rate = 3000000,
	.suspend = plat_kim_suspend,
	.resume = plat_kim_resume,
};
static struct platform_device wl128x_device = {
	.name		= "kim",
	.id		= -1,
	.dev.platform_data = &wilink_pdata,
};
static struct platform_device btwilink_device = {
	.name = "btwilink",
	.id = -1,
};

#ifdef CONFIG_TI_ST
static bool is_bt_active(void)
{
	struct platform_device  *pdev;
	struct kim_data_s       *kim_gdata;

	pdev = &wl128x_device;
	kim_gdata = dev_get_drvdata(&pdev->dev);
	if (st_ll_getstate(kim_gdata->core_data) != ST_LL_ASLEEP &&
			st_ll_getstate(kim_gdata->core_data) != ST_LL_INVALID)
		return true;
	else
		return false;
}
#else
#define is_bt_active NULL
#endif
#endif

static struct platform_device *sdp4430_devices[] __initdata = {
	&sdp4430_disp_led,
//	&sdp4430_proximity_device,
//	&sdp4430_leds_pwm,
//	&sdp4430_leds_gpio,
//	&wl128x_device,
//	&btwilink_device,
//	&sdp4430_hdmi_audio_device,
	&sdp4430_keypad_led,
        &sdp4430_aic3110,
#if defined(CONFIG_SENSORS_OMAP_BANDGAP)
	&sdp4430_omap_bandgap_sensor,
#endif
#if defined(CONFIG_SENSORS_PMIC_THERMAL)
	&sdp4430_pmic_thermal_sensor,
#endif
  &kc1_led_device,
};

static void __init omap_4430sdp_init_irq(void)
{
	omap2_init_common_hw(NULL, NULL);
#ifdef CONFIG_OMAP_32K_TIMER
	omap2_gp_clockevent_set_gptimer(1);
#endif
	gic_init_irq();
	sr_class3_init();
}

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode			= MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_HDRC_HCD)
	.mode			= MUSB_HOST,
#elif defined(CONFIG_USB_GADGET_MUSB_HDRC)
	.mode			= MUSB_PERIPHERAL,
#endif
	.power			= 100,
};

#ifndef CONFIG_TIWLAN_SDIO
static int wifi_set_power(struct device *dev, int slot, int power_on, int vdd)
{
	static int power_state;

	pr_debug("Powering %s wifi", (power_on ? "on" : "off"));

	if (power_on == power_state)
		return 0;
	power_state = power_on;

	if (power_on) {
		gpio_set_value(GPIO_WIFI_PMENA, 1);
		mdelay(15);
		gpio_set_value(GPIO_WIFI_PMENA, 0);
		mdelay(1);
		gpio_set_value(GPIO_WIFI_PMENA, 1);
		mdelay(70);
	} else {
		gpio_set_value(GPIO_WIFI_PMENA, 0);
	}

	return 0;
}
#endif

static struct twl4030_usb_data omap4_usbphy_data = {
	.phy_init	= omap4430_phy_init,
	.phy_exit	= omap4430_phy_exit,
	.phy_power	= omap4430_phy_power,
	.phy_set_clock	= omap4430_phy_set_clk,
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 2,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA |
					MMC_CAP_1_8V_DDR,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.nonremovable   = true,
#ifdef CONFIG_PM_RUNTIME
		.power_saving	= true,
#endif
	},
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA |
					MMC_CAP_1_8V_DDR,
		.gpio_wp	= -EINVAL,
#ifdef CONFIG_PM_RUNTIME
		.power_saving	= true,
#endif
	},
#ifdef CONFIG_TIWLAN_SDIO
	{
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp        = 4,
		.ocr_mask	= MMC_VDD_165_195,
	},
#else
	{
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.nonremovable	= true,
	},
#endif
	{}	/* Terminator */
};

#ifndef CONFIG_TIWLAN_SDIO
static struct wl12xx_platform_data omap4_panda_wlan_data __initdata = {
	.irq = OMAP_GPIO_IRQ(GPIO_WIFI_IRQ),
	.board_ref_clock = WL12XX_REFCLOCK_26,
	.board_tcxo_clock = 0,
};
#endif

static struct regulator_consumer_supply sdp4430_vmmc_supply[] = {
	{
		.supply = "vmmc",
		.dev_name = "mmci-omap-hs.0",
	},
};

static struct regulator_consumer_supply sdp4430_gsensor_supply[] = {
	{
		.supply = "g-sensor-pwr",
	},
};

static struct regulator_consumer_supply sdp4430_audio_supply[] = {
	{
		.supply = "audio-pwr",
	},
};
static struct regulator_consumer_supply sdp4430_emmc_supply[] = {
	{
		.supply = "emmc-pwr",
	},
};
static struct regulator_consumer_supply sdp4430_vaux3_supply[] = {
	{
		.supply = "vaux3",
	},
};
static struct regulator_consumer_supply sdp4430_vusb_supply[] = {
    {
        .supply = "usb-phy",
    },
};
static int omap4_twl6030_hsmmc_late_init(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	/* Setting MMC1 Card detect Irq */
	if (pdev->id == 0) {
		ret = twl6030_mmc_card_detect_config();
		if (ret)
			pr_err("Failed configuring MMC1 card detect\n");
		pdata->slots[0].card_detect_irq = TWL6030_IRQ_BASE +
						MMCDETECT_INTR_OFFSET;
		pdata->slots[0].card_detect = twl6030_mmc_card_detect;
	}

#ifndef CONFIG_TIWLAN_SDIO
	/* Set the MMC5 (wlan) power function */
	if (pdev->id == 4)
		pdata->slots[0].set_power = wifi_set_power;
#endif

	return ret;
}

static __init void omap4_twl6030_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev)
		return;

	pdata = dev->platform_data;
	pdata->init = omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap2_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(c->dev);

	return 0;
}

static struct regulator_init_data sdp4430_vaux1 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
		.always_on	= true,
	},
    .num_consumer_supplies = 1,
    .consumer_supplies = sdp4430_emmc_supply,
};

static struct regulator_init_data sdp4430_vaux2 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},

		.always_on	= true,

	},
    .num_consumer_supplies = 1,
    .consumer_supplies = sdp4430_gsensor_supply,
};

static struct regulator_init_data sdp4430_vaux3 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = sdp4430_vaux3_supply,
};

/* VMMC1 for MMC1 card */
static struct regulator_init_data sdp4430_vmmc = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
//		.always_on		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = sdp4430_vmmc_supply,
};

static struct regulator_init_data sdp4430_vpp = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
	},
};

static struct regulator_init_data sdp4430_vusim = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
        //.boot_on        = true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = sdp4430_audio_supply,
};

static struct regulator_init_data sdp4430_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
		.always_on	= true,
	},
};

static struct regulator_init_data sdp4430_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
		.always_on	= true,
	},
};

static struct regulator_init_data sdp4430_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
		.always_on	= true,
	},
};

static struct regulator_init_data sdp4430_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.enabled	= false,
			.disabled	= true,
		},
	},
    .num_consumer_supplies  = 1,
    .consumer_supplies      = sdp4430_vusb_supply,
};

static struct twl4030_madc_platform_data sdp4430_gpadc_data = {
	.irq_line	= 1,
};
#define SUMMIT_STAT          31
static struct twl6030_qcharger_platform_data kc1_charger_data={
        .interrupt_pin = OMAP4_CHARGER_IRQ,
};
#if 0
static int sdp4430_batt_table[] = {
	/* adc code for temperature in degree C */
	929, 925, /* -2 ,-1 */
	920, 917, 912, 908, 904, 899, 895, 890, 885, 880, /* 00 - 09 */
	875, 869, 864, 858, 853, 847, 841, 835, 829, 823, /* 10 - 19 */
	816, 810, 804, 797, 790, 783, 776, 769, 762, 755, /* 20 - 29 */
	748, 740, 732, 725, 718, 710, 703, 695, 687, 679, /* 30 - 39 */
	671, 663, 655, 647, 639, 631, 623, 615, 607, 599, /* 40 - 49 */
	591, 583, 575, 567, 559, 551, 543, 535, 527, 519, /* 50 - 59 */
	511, 504, 496 /* 60 - 62 */
};

static struct twl4030_bci_platform_data sdp4430_bci_data = {
	.monitoring_interval		= 10,
	.max_charger_currentmA		= 1500,
	.max_charger_voltagemV		= 4560,
	.max_bat_voltagemV		= 4200,
	.low_bat_voltagemV		= 3300,
	.battery_tmp_tbl		= sdp4430_batt_table,
	.tblsize			= ARRAY_SIZE(sdp4430_batt_table),
};
#endif
#if 0
static void omap4_audio_conf(void)
{
	/* twl6040 naudint */
	omap_mux_init_signal("sys_nirq2.sys_nirq2", \
		OMAP_PIN_INPUT_PULLUP);
}

static int tps6130x_enable(int on)
{
        u8 val = 0;
        int ret;

        ret = twl_i2c_read_u8(TWL_MODULE_AUDIO_VOICE, &val, TWL6040_REG_GPOCTL);
        if (ret < 0) {
		pr_err("%s: failed to read GPOCTL %d\n", __func__, ret);
                return ret;
	}

	/* TWL6040 GPO2 connected to TPS6130X NRESET */
	if (on)
		val |= TWL6040_GPO2;
	else
		val &= ~TWL6040_GPO2;

	ret = twl_i2c_write_u8(TWL_MODULE_AUDIO_VOICE, val, TWL6040_REG_GPOCTL);
	if (ret < 0)
		pr_err("%s: failed to write GPOCTL %d\n", __func__, ret);

	return ret;
}

struct tps6130x_platform_data tps6130x_pdata = {
	.chip_enable	= tps6130x_enable,
};

static struct regulator_consumer_supply twl6040_vddhf_supply[] = {
	REGULATOR_SUPPLY("vddhf", "twl6040-codec"),
};

static struct regulator_init_data twl6040_vddhf = {
	.constraints = {
		.min_uV			= 4075000,
		.max_uV			= 4950000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(twl6040_vddhf_supply),
	.consumer_supplies	= twl6040_vddhf_supply,
	.driver_data		= &tps6130x_pdata,
};

static struct twl4030_codec_audio_data twl6040_audio = {
	.vddhf_uV	= 4075000,
};

static struct twl4030_codec_vibra_data twl6040_vibra = {
	.max_timeout	= 15000,
	.initial_vibrate = 0,
};

static struct twl4030_codec_data twl6040_codec = {
	.audio		= &twl6040_audio,
	.vibra		= &twl6040_vibra,
	.audpwron_gpio	= 127,
	.naudint_irq	= OMAP44XX_IRQ_SYS_2N,
	.irq_base	= TWL6040_CODEC_IRQ_BASE,
};
#endif

static struct regulator_init_data sdp4430_clk32kg = {
       .constraints = {
		.valid_ops_mask         = REGULATOR_CHANGE_STATUS,
       },
};

static struct twl4030_platform_data sdp4430_twldata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* Regulators */
	.vmmc		= &sdp4430_vmmc,
	.vpp		= &sdp4430_vpp,
	.vusim		= &sdp4430_vusim,
	.vana		= &sdp4430_vana,
	.vcxio		= &sdp4430_vcxio,
	.vdac		= &sdp4430_vdac,
	.vusb		= &sdp4430_vusb,
	.vaux1		= &sdp4430_vaux1,
	.vaux2		= &sdp4430_vaux2,
	.vaux3		= &sdp4430_vaux3,
	.usb		= &omap4_usbphy_data,
	.clk32kg        = &sdp4430_clk32kg,
	.madc           = &sdp4430_gpadc_data,
//    .qcharger   =&kc1_charger_data,
//	.bci            = &sdp4430_bci_data,

	/* children */
//	.codec          = &twl6040_codec,
};

#if 0
static struct bq2415x_platform_data sdp4430_bqdata = {
	.max_charger_voltagemV = 4200,
	.max_charger_currentmA = 1550,
};

/*
 * The Clock Driver Chip (TCXO) on OMAP4 based SDP needs to
 * be programmed to output CLK1 based on REQ1 from OMAP.
 * By default CLK1 is driven based on an internal REQ1INT signal
 * which is always set to 1.
 * Doing this helps gate sysclk (from CLK1) to OMAP while OMAP
 * is in sleep states.
 */
static struct cdc_tcxo_platform_data sdp4430_cdc_data = {
	.buf = {
		CDC_TCXO_REQ4INT | CDC_TCXO_REQ1INT |
		CDC_TCXO_REQ4POL | CDC_TCXO_REQ3POL |
		CDC_TCXO_REQ2POL | CDC_TCXO_REQ1POL,

		CDC_TCXO_MREQ4 | CDC_TCXO_MREQ3 |
		CDC_TCXO_MREQ2 | CDC_TCXO_MREQ1,

		CDC_TCXO_LDOEN1,

		0 },
};

static struct cma3000_platform_data cma3000_platform_data = {
	.def_poll_rate = 200,
	.fuzz_x = 25,
	.fuzz_y = 25,
	.fuzz_z = 25,
	.g_range = CMARANGE_8G,
	.mode = CMAMODE_MEAS400,
	.mdthr = 0x8,
	.mdfftmr = 0x33,
	.ffthr = 0x8,
	.irqflags = IRQF_TRIGGER_HIGH,
};

static struct pico_platform_data picodlp_platform_data[] = {
	[0] = { /* DLP Controller */
		.gpio_intr = 40,
	},
};
#endif
static struct i2c_board_info __initdata sdp4430_i2c_boardinfo_dvt[] = {
#ifdef CONFIG_BATTERY_BQ27541_Q
    {
        I2C_BOARD_INFO("bq27541", 0x55),
    },
#endif 
    {
	     I2C_BOARD_INFO("twl6030", 0x48),
	     .flags = I2C_CLIENT_WAKE,
	     .irq = OMAP44XX_IRQ_SYS_1N,
	     .platform_data = &sdp4430_twldata,
    },
};
static struct i2c_board_info __initdata sdp4430_i2c_boardinfo[] = {
#ifdef CONFIG_BATTERY_BQ27541_Q
    {
        I2C_BOARD_INFO("bq27541", 0x55),
    },
#endif
#ifdef CONFIG_SUMMIT_SMB347_Q
    {
        I2C_BOARD_INFO("summit_smb347", 0x6),
        .irq = OMAP_GPIO_IRQ(OMAP4_CHARGER_IRQ),
    },
#endif    
    {
	     I2C_BOARD_INFO("twl6030", 0x48),
	     .flags = I2C_CLIENT_WAKE,
	     .irq = OMAP44XX_IRQ_SYS_1N,
	     .platform_data = &sdp4430_twldata,
    },
#if 0    
    {
       I2C_BOARD_INFO("bq27541", 0x55),
    },
#endif    
#if 0
	{
		I2C_BOARD_INFO("bq24156", 0x6a),
		.platform_data = &sdp4430_bqdata,
	},
#ifdef CONFIG_BATTERY_BQ27541    
	{
		.type = "bq27541_Battery",
		.addr = 0x55,
	},
#endif
#ifdef CONFIG_SUMMIT_SMB347
	{
		.type = "summit_smb347",
		.addr = 0x06,
		.irq = OMAP_GPIO_IRQ(OMAP4_CHARGER_IRQ),
	},
#endif    
#endif
};

static struct i2c_board_info __initdata sdp4430_i2c_2_boardinfo[] = {
	{	// chris 2011_0124
		I2C_BOARD_INFO("ilitek_i2c", 0x41),
		.irq = OMAP_GPIO_IRQ(OMAP4_TOUCH_IRQ_1),
	},
#if 0
	{
		I2C_BOARD_INFO("tm12xx_ts_primary", 0x4b),
		.platform_data = &tm12xx_platform_data[0],
	},
	{
		I2C_BOARD_INFO("picoDLP_i2c_driver", 0x1b),
		.platform_data = &picodlp_platform_data[0],
	},
#endif
};

static struct i2c_board_info __initdata sdp4430_i2c_3_boardinfo[] = {
#ifdef CONFIG_DEMO_HDMI
	{
		I2C_BOARD_INFO("adi7526", 0x39),
		.irq = OMAP_GPIO_IRQ(OMAP4_ADI7526_IRQ),
	},
#endif
/* Mistral: Updated this array to include the AIC3110 Audio Codec 
	{
		I2C_BOARD_INFO("tlv320aic3110", 0x18),
	},*/
};

static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo[] = {
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("tmp105", 0x49),	 },
#endif
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18),				},
};

static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo_c1c[] = {
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("lm75", 0x48),	 },
#endif
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18),				},
};
static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo_dvt[] = {
#ifdef CONFIG_SUMMIT_SMB347_Q
    {
        I2C_BOARD_INFO("summit_smb347", 0x6),
        .irq = OMAP_GPIO_IRQ(OMAP4_CHARGER_IRQ),
    },
#endif    
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("tmp105", 0x49),	 },
#endif
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18),				},
};
static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo_pvt[] = {
#ifdef CONFIG_SUMMIT_SMB347_Q
    {
        I2C_BOARD_INFO("summit_smb347", 0x6),
        .irq = OMAP_GPIO_IRQ(OMAP4_CHARGER_IRQ),
    },
#endif    
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18),				},
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("tmp105", 0x48),	 },
#endif
};

static struct usbhs_omap_platform_data usbhs_pdata __initconst = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset  = false,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL
};

static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_4_bus_pdata;

/*
 * LPDDR2 Configeration Data:
 * The memory organisation is as below :
 *	EMIF1 - CS0 -	2 Gb
 *		CS1 -	2 Gb
 *	EMIF2 - CS0 -	2 Gb
 *		CS1 -	2 Gb
 *	--------------------
 *	TOTAL -		8 Gb
 *
 * Same devices installed on EMIF1 and EMIF2
 */
static __initdata struct emif_device_details emif_devices = {
	.cs0_device = &elpida_2G_S4,
//	.cs1_device = &elpida_2G_S4
};

static void __init omap_i2c_hwspinlock_init(int bus_id, unsigned int
			spinlock_id, struct omap_i2c_bus_board_data *pdata)
{
	pdata->handle = hwspinlock_request_specific(spinlock_id);
	if (pdata->handle != NULL) {
		pdata->hwspinlock_lock = hwspinlock_lock;
		pdata->hwspinlock_unlock = hwspinlock_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", bus_id);
	}
}

static int __init omap4_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &sdp4430_i2c_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &sdp4430_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &sdp4430_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &sdp4430_i2c_4_bus_pdata);

	regulator_has_full_constraints();

	/*
	 * Phoenix Audio IC needs I2C1 to
	 * start with 400 KHz or less
	 */
        if (quanta_mbid<0x04)
	omap_register_i2c_bus(1, 400, &sdp4430_i2c_bus_pdata,
		sdp4430_i2c_boardinfo, ARRAY_SIZE(sdp4430_i2c_boardinfo));
        else//DVT
	omap_register_i2c_bus(1, 400, &sdp4430_i2c_bus_pdata,
		sdp4430_i2c_boardinfo_dvt, ARRAY_SIZE(sdp4430_i2c_boardinfo_dvt));
	omap_register_i2c_bus(2, 400, &sdp4430_i2c_2_bus_pdata,
		sdp4430_i2c_2_boardinfo, ARRAY_SIZE(sdp4430_i2c_2_boardinfo));
	omap_register_i2c_bus(3, 400, &sdp4430_i2c_3_bus_pdata,
		sdp4430_i2c_3_boardinfo, ARRAY_SIZE(sdp4430_i2c_3_boardinfo));
	if (quanta_mbid<0x02)
	omap_register_i2c_bus(4, 400, &sdp4430_i2c_4_bus_pdata,
		sdp4430_i2c_4_boardinfo_c1c, ARRAY_SIZE(sdp4430_i2c_4_boardinfo_c1c));
	else if (quanta_mbid<0x04)
	omap_register_i2c_bus(4, 400, &sdp4430_i2c_4_bus_pdata,
		sdp4430_i2c_4_boardinfo, ARRAY_SIZE(sdp4430_i2c_4_boardinfo));	
	else if (quanta_mbid<0x06)//DVT
	omap_register_i2c_bus(4, 400, &sdp4430_i2c_4_bus_pdata,
		sdp4430_i2c_4_boardinfo_dvt, ARRAY_SIZE(sdp4430_i2c_4_boardinfo_dvt));
	else //PVT
	omap_register_i2c_bus(4, 400, &sdp4430_i2c_4_bus_pdata,
		sdp4430_i2c_4_boardinfo_pvt, ARRAY_SIZE(sdp4430_i2c_4_boardinfo_pvt));
	return 0;
}
#if 1
static void __init omap4_display_init(void)
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
#else
static void __init omap4_display_init(void)
{
	void __iomem *phymux_base = NULL;
	u32 val = 0xFFFFC000;

	phymux_base = ioremap(0x4A100000, 0x1000);

	/* Turning on DSI PHY Mux*/
	__raw_writel(val, phymux_base + 0x618);
	/* Set mux to choose GPIO 101 for Taal 1 ext te line*/
	val = __raw_readl(phymux_base + 0x90);
	val = (val & 0xFFFFFFE0) | 0x11B;
	__raw_writel(val, phymux_base + 0x90);

	/* Set mux to choose GPIO 103 for Taal 2 ext te line*/
	val = __raw_readl(phymux_base + 0x94);
	val = (val & 0xFFFFFFE0) | 0x11B;
	__raw_writel(val, phymux_base + 0x94);
	iounmap(phymux_base);
	/* Panel Taal reset and backlight GPIO init */
	gpio_request(dsi_panel.reset_gpio, "dsi1_en_gpio");
	gpio_direction_output(dsi_panel.reset_gpio, 0);
	//gpio_request(dsi2_panel.reset_gpio, "dsi2_en_gpio");
	//gpio_direction_output(dsi2_panel.reset_gpio, 0);
}
#endif

#ifdef CONFIG_TOUCHSCREEN_ILITEK
// chris 2011_0124
static void omap_ilitek_init(void)
{
#ifndef CONFIG_DEMO_HDMI
	//printk("~~~~~~~~%s\n", __func__);
	omap_mux_init_signal("dpm_emu7.gpio_18", \
                OMAP_PIN_OUTPUT | \
                OMAP_PIN_OFF_NONE);
        
        if (gpio_request(OMAP4_TOUCH_RESET_GPIO , "ilitek_reset_gpio_18") <0 ){
		pr_err("Touch IRQ reset request failed\n");
                return;
	}
        gpio_direction_output(OMAP4_TOUCH_RESET_GPIO, 0);
        gpio_set_value(OMAP4_TOUCH_RESET_GPIO, 1);
#else
	omap_mux_init_signal("gpmc_nadv_ale.gpio_56", \
                OMAP_PIN_OUTPUT | \
                OMAP_PIN_OFF_NONE);
        
        if (gpio_request(OMAP4_ADI7526_5V_EN , "adi7526_5v_en_gpio_56") <0 ){
		pr_err("ADI7526_5V_EN request failed\n");
                return;
	}
        gpio_direction_output(OMAP4_ADI7526_5V_EN, 0);
        gpio_set_value(OMAP4_ADI7526_5V_EN, 1);

	omap_mux_init_signal("abe_dmic_din3.gpio_122", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);

	if (gpio_request(OMAP4_ADI7526_IRQ, "ADI7526 IRQ") < 0) {
		pr_err("ADI7526 IRQ GPIO request failed\n");
		return;
	}
	gpio_direction_input(OMAP4_ADI7526_IRQ);

#endif

	if (gpio_request(OMAP4_TOUCH_IRQ_1, "Touch IRQ") < 0) {
		pr_err("Touch IRQ GPIO request failed\n");
		return;
	}
	gpio_direction_input(OMAP4_TOUCH_IRQ_1);
}
#endif //CONFIG_TOUCHSCREEN_ILITEK

//chris 2010_1223 start 
static void panel_enable(void)
{
    omap_mux_init_signal("dpm_emu17.gpio_28", \
	OMAP_PIN_OUTPUT | \
	OMAP_PIN_OFF_NONE);
	if (gpio_request(OMAP4_LCD_EN_GPIO, "gpio 28") < 0) {
		pr_err("GPIO 28 request failed\n");
		return;
	}
	gpio_direction_output(OMAP4_LCD_EN_GPIO, 1);
}
//chris 2010_1223 end

/*
 * As OMAP4430 mux HSI and USB signals, when HSI is used (for instance HSI
 * modem is plugged) we should configure HSI pad conf and disable some USB
 * configurations.
 * HSI usage is declared using bootargs variable:
 * board-4430sdp.modem_ipc=hsi
 * Any other or missing value will not setup HSI pad conf, and port_mode[0]
 * will be used by USB.
 * Variable modem_ipc is used to catch bootargs parameter value.
 */

static char *modem_ipc = "n/a";

module_param(modem_ipc, charp, 0);
MODULE_PARM_DESC(modem_ipc, "Modem IPC setting");

static void omap_4430hsi_pad_conf(void)
{
	/*
	 * HSI pad conf: hsi1_ca/ac_wake/flag/data/ready
	 * Also configure gpio_92/95/157/187 used by modem
	 */

	/* hsi1_cawake */
	omap_mux_init_signal("usbb1_ulpitll_clk.hsi1_cawake", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE | \
		OMAP_PIN_OFF_WAKEUPENABLE);
	/* hsi1_caflag */
	omap_mux_init_signal("usbb1_ulpitll_dir.hsi1_caflag", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_cadata */
	omap_mux_init_signal("usbb1_ulpitll_stp.hsi1_cadata", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_acready */
	omap_mux_init_signal("usbb1_ulpitll_nxt.hsi1_acready", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_OUTPUT_LOW);
	/* hsi1_acwake */
	omap_mux_init_signal("usbb1_ulpitll_dat0.hsi1_acwake", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_acdata */
	omap_mux_init_signal("usbb1_ulpitll_dat1.hsi1_acdata", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_acflag */
	omap_mux_init_signal("usbb1_ulpitll_dat2.hsi1_acflag", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* hsi1_caready */
	omap_mux_init_signal("usbb1_ulpitll_dat3.hsi1_caready", \
		OMAP_PIN_INPUT | \
		OMAP_PIN_OFF_NONE);
	/* gpio_92 */
	omap_mux_init_signal("usbb1_ulpitll_dat4.gpio_92", \
		OMAP_PULL_ENA);
	/* gpio_95 */
	omap_mux_init_signal("usbb1_ulpitll_dat7.gpio_95", \
		OMAP_PIN_INPUT_PULLDOWN | \
		OMAP_PIN_OFF_NONE);
	/* gpio_157 */
	omap_mux_init_signal("usbb2_ulpitll_clk.gpio_157", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
	/* gpio_187 */
	omap_mux_init_signal("sys_boot3.gpio_187", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);
}

static void enable_board_wakeup_source(void)
{
	int gpio_val;
#if 0
	gpio_val = omap_mux_get_gpio(OMAP4_SFH7741_SENSOR_OUTPUT_GPIO);
	if ((gpio_val & OMAP44XX_PADCONF_WAKEUPENABLE0) == 0) {
		gpio_val |= OMAP44XX_PADCONF_WAKEUPENABLE0;
		omap_mux_set_gpio(gpio_val, OMAP4_SFH7741_SENSOR_OUTPUT_GPIO);
	}
#endif
	/*
	 * Enable IO daisy for sys_nirq1/2, to be able to
	 * wakeup from interrupts from PMIC/Audio IC.
	 * Needed only in Device OFF mode.
	 */
	omap_mux_enable_wakeup("sys_nirq1");
	omap_mux_enable_wakeup("sys_nirq2");

	if (!strcmp(modem_ipc, "hsi")) {
		/*
		 * Enable IO daisy for HSI CAWAKE line, to be able to
		 * wakeup from interrupts from Modem.
		 * Needed only in Device OFF mode.
		 */
		omap_mux_enable_wakeup("usbb1_ulpitll_clk.hsi1_cawake");
	}

}

static struct omap_volt_pmic_info omap_pmic_core = {
	.name = "twl",
	.slew_rate = 4000,
	.step_size = 12500,
	.i2c_addr = 0x12,
	.i2c_vreg = 0x61,
	.i2c_cmdreg = 0x62,
	.vsel_to_uv = omap_twl_vsel_to_uv,
	.uv_to_vsel = omap_twl_uv_to_vsel,
	.onforce_cmd = omap_twl_onforce_cmd,
	.on_cmd = omap_twl_on_cmd,
	.sleepforce_cmd = omap_twl_sleepforce_cmd,
	.sleep_cmd = omap_twl_sleep_cmd,
	.vp_config_erroroffset = 0,
	.vp_vstepmin_vstepmin = 0x01,
	.vp_vstepmax_vstepmax = 0x04,
	.vp_vlimitto_timeout_us = 0x200,
	.vp_vlimitto_vddmin = 0xA,
	.vp_vlimitto_vddmax = 0x28,
};

static struct omap_volt_pmic_info omap_pmic_mpu = {
	.name = "twl",
	.slew_rate = 4000,
	.step_size = 12500,
	.i2c_addr = 0x12,
	.i2c_vreg = 0x55,
	.i2c_cmdreg = 0x56,
	.vsel_to_uv = omap_twl_vsel_to_uv,
	.uv_to_vsel = omap_twl_uv_to_vsel,
	.onforce_cmd = omap_twl_onforce_cmd,
	.on_cmd = omap_twl_on_cmd,
	.sleepforce_cmd = omap_twl_sleepforce_cmd,
	.sleep_cmd = omap_twl_sleep_cmd,
	.vp_config_erroroffset = 0,
	.vp_vstepmin_vstepmin = 0x01,
	.vp_vstepmax_vstepmax = 0x04,
	.vp_vlimitto_timeout_us = 0x200,
	.vp_vlimitto_vddmin = 0xA,
	.vp_vlimitto_vddmax = 0x39,
};

static struct omap_volt_pmic_info omap_pmic_iva = {
	.name = "twl",
	.slew_rate = 4000,
	.step_size = 12500,
	.i2c_addr = 0x12,
	.i2c_vreg = 0x5b,
	.i2c_cmdreg = 0x5c,
	.vsel_to_uv = omap_twl_vsel_to_uv,
	.uv_to_vsel = omap_twl_uv_to_vsel,
	.onforce_cmd = omap_twl_onforce_cmd,
	.on_cmd = omap_twl_on_cmd,
	.sleepforce_cmd = omap_twl_sleepforce_cmd,
	.sleep_cmd = omap_twl_sleep_cmd,
	.vp_config_erroroffset = 0,
	.vp_vstepmin_vstepmin = 0x01,
	.vp_vstepmax_vstepmax = 0x04,
	.vp_vlimitto_timeout_us = 0x200,
	.vp_vlimitto_vddmin = 0xA,
	.vp_vlimitto_vddmax = 0x2D,
};

static struct omap_volt_vc_data vc_config = {
	.vdd0_on = 1375000,        /* 1.375v */
	.vdd0_onlp = 1375000,      /* 1.375v */
	.vdd0_ret = 837500,       /* 0.8375v */
	.vdd0_off = 0,		/* 0 v */
	.vdd1_on = 1300000,        /* 1.3v */
	.vdd1_onlp = 1300000,      /* 1.3v */
	.vdd1_ret = 837500,       /* 0.8375v */
	.vdd1_off = 0,		/* 0 v */
	.vdd2_on = 1200000,        /* 1.2v */
	.vdd2_onlp = 1200000,      /* 1.2v */
	.vdd2_ret = 837500,       /* .8375v */
	.vdd2_off = 0,		/* 0 v */
};

void plat_hold_wakelock(void *up, int flag)
{
	struct uart_omap_port *up2 = (struct uart_omap_port *)up;
	/*Specific wakelock for bluetooth usecases*/
	if ((up2->pdev->id == BLUETOOTH_UART)
		&& ((flag == WAKELK_TX) || (flag == WAKELK_RX)))
		wake_lock_timeout(&uart_lock, 2*HZ);

	/*Specific wakelock for console usecases*/
	if ((up2->pdev->id == CONSOLE_UART)
		&& ((flag == WAKELK_IRQ) || (flag == WAKELK_RESUME)))
		wake_lock_timeout(&uart_lock, 5*HZ);
	return;
}

static struct omap_uart_port_info omap_serial_platform_data[] = {
	{
		.use_dma	= 0,
		.dma_rx_buf_size = DEFAULT_RXDMA_BUFSIZE,
		.dma_rx_poll_rate = DEFAULT_RXDMA_POLLRATE,
		.dma_rx_timeout = DEFAULT_RXDMA_TIMEOUT,
		.idle_timeout	= DEFAULT_IDLE_TIMEOUT,
		.flags		= 1,
		.plat_hold_wakelock = NULL,
		.plat_omap_bt_active = NULL,
		.rts_padconf	= 0,
		.rts_override	= 0,
		.cts_padconf	= 0,
		.padconf	= OMAP4_CTRL_MODULE_PAD_SDMMC1_CMD_OFFSET,
		.padconf_wake_ev = 0,
		.wk_mask	= 0,
	},
	{
		.use_dma	= 0,
		.dma_rx_buf_size = DEFAULT_RXDMA_BUFSIZE,
		.dma_rx_poll_rate = DEFAULT_RXDMA_POLLRATE,
		.dma_rx_timeout = DEFAULT_RXDMA_TIMEOUT,
		.idle_timeout	= DEFAULT_IDLE_TIMEOUT,
		.flags		= 1,
		.plat_hold_wakelock = NULL,
		.rts_padconf	= OMAP4_CTRL_MODULE_PAD_UART2_RTS_OFFSET,
		.rts_override	= 0,
		.cts_padconf	= OMAP4_CTRL_MODULE_PAD_UART2_CTS_OFFSET,
		.padconf	= OMAP4_CTRL_MODULE_PAD_UART2_RX_OFFSET,
		.padconf_wake_ev =
			OMAP4_CTRL_MODULE_PAD_CORE_PADCONF_WAKEUPEVENT_3,
		.wk_mask	=
			OMAP4_UART2_TX_DUPLICATEWAKEUPEVENT_MASK |
			OMAP4_UART2_RX_DUPLICATEWAKEUPEVENT_MASK |
			OMAP4_UART2_RTS_DUPLICATEWAKEUPEVENT_MASK |
			OMAP4_UART2_CTS_DUPLICATEWAKEUPEVENT_MASK,
	},
	{
		.use_dma	= 0,
		.dma_rx_buf_size = DEFAULT_RXDMA_BUFSIZE,
		.dma_rx_poll_rate = DEFAULT_RXDMA_POLLRATE,
		.dma_rx_timeout = DEFAULT_RXDMA_TIMEOUT,
		.idle_timeout	= DEFAULT_IDLE_TIMEOUT,
		.flags		= 1,
		.plat_hold_wakelock = plat_hold_wakelock,
		.rts_padconf	= 0,
		.rts_override	= 0,
		.padconf	= OMAP4_CTRL_MODULE_PAD_UART3_RX_IRRX_OFFSET,
		.padconf_wake_ev =
			OMAP4_CTRL_MODULE_PAD_CORE_PADCONF_WAKEUPEVENT_4,
		.wk_mask	=
			OMAP4_UART3_TX_IRTX_DUPLICATEWAKEUPEVENT_MASK |
			OMAP4_UART3_RX_IRRX_DUPLICATEWAKEUPEVENT_MASK |
			OMAP4_UART3_RTS_SD_DUPLICATEWAKEUPEVENT_MASK |
			OMAP4_UART3_CTS_RCTX_DUPLICATEWAKEUPEVENT_MASK,
	},
	{
		.use_dma	= 0,
		.dma_rx_buf_size = DEFAULT_RXDMA_BUFSIZE,
		.dma_rx_poll_rate = DEFAULT_RXDMA_POLLRATE,
		.dma_rx_timeout = DEFAULT_RXDMA_TIMEOUT,
		.idle_timeout	= DEFAULT_IDLE_TIMEOUT,
		.flags		= 1,
		.plat_hold_wakelock = NULL,
		.rts_padconf	= 0,
		.rts_override	= 0,

	},
	{
		.flags		= 0
	}
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
#ifndef CONFIG_TIWLAN_SDIO
	/* WLAN IRQ - GPIO 53 */
	OMAP4_MUX(GPMC_NCS3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT),
	/* WLAN_EN - GPIO 54 */
	OMAP4_MUX(GPMC_NWP, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT),
	/* WLAN SDIO: MMC5 CMD */
	OMAP4_MUX(SDMMC5_CMD, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC5 CLK */
	OMAP4_MUX(SDMMC5_CLK, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	/* WLAN SDIO: MMC5 DAT[0-3] */
	OMAP4_MUX(SDMMC5_DAT0, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT1, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
	OMAP4_MUX(SDMMC5_DAT3, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP),
#endif
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE

static struct resource ram_console_resource = {
    .start  = 0x8E000000,
    .end    = 0x8E000000 + 0x40000 - 1,
    .flags  = IORESOURCE_MEM,
};

static struct platform_device ram_console_device = {
    .name           = "ram_console",
    .id             = 0,
    .num_resources  = 1,
    .resource       = &ram_console_resource,
};

static inline void ramconsole_init(void)
{
	platform_device_register(&ram_console_device);
}

static inline void ramconsole_reserve_sdram(void)
{
	reserve_bootmem(ram_console_resource.start,
	                ram_console_resource.end - ram_console_resource.start + 1, 0);
}

#else

static inline void ramconsole_init(void) {}
static inline void ramconsole_reserve_sdram(void) {}

#endif /* CONFIG_ANDROID_RAM_CONSOLE */

static void enable_rtc_gpio(void){
	/* To access twl registers we enable gpio6
	 * we need this so the RTC driver can work.
	 */
	gpio_request(TWL6030_RTC_GPIO, "h_SYS_DRM_MSEC");
	gpio_direction_output(TWL6030_RTC_GPIO, 1);

	omap_mux_init_signal("fref_clk0_out.gpio_wk6", \
		OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE);
	return;
}

#ifndef CONFIG_TIWLAN_SDIO
static void omap4_4430sdp_wifi_init(void)
{
	if (gpio_request(GPIO_WIFI_PMENA, "wl12xx") ||
	    gpio_direction_output(GPIO_WIFI_PMENA, 0))
		pr_err("Error initializing up WLAN_EN\n");
	if (wl12xx_set_platform_data(&omap4_panda_wlan_data))
		pr_err("Error setting wl12xx data\n");
}
#endif
static void __init omap_4430sdp_init(void)
{
	int status;
	int package = OMAP_PACKAGE_CBS;

	quanta_boardids();
	if (omap_rev() == OMAP4430_REV_ES1_0)
		package = OMAP_PACKAGE_CBL;
	omap4_mux_init(board_mux, package);

	omap_emif_setup_device_details(&emif_devices, &emif_devices);
	omap_init_emif_timings();

	enable_rtc_gpio();
	//omap4_audio_conf();
	ramconsole_init();
	omap4_i2c_init();
	omap4_display_init();
	//omap_disp_led_init();
	platform_add_devices(sdp4430_devices, ARRAY_SIZE(sdp4430_devices));

	gpio_request(0,"sysok");

	wake_lock_init(&uart_lock, WAKE_LOCK_SUSPEND, "uart_wake_lock");
	omap_serial_init(omap_serial_platform_data);
	omap4_twl6030_hsmmc_init(mmc);
gpio_request(42,"OMAP_GPIO_ADC");
gpio_direction_output(42,0);
    /*For smb347*/
    //Enable charger interrupt wakeup function.
    omap_mux_init_signal("fref_clk4_req.gpio_wk7", \
            OMAP_PIN_INPUT_PULLUP |OMAP_PIN_OFF_WAKEUPENABLE|OMAP_PIN_OFF_INPUT_PULLUP);
    if(quanta_mbid>=4){
        //EN pin
        omap_mux_init_signal("c2c_data12.gpio_101", \
                OMAP_PIN_OUTPUT |OMAP_PIN_OFF_OUTPUT_LOW);
        //SUSP
        omap_mux_init_signal("uart4_rx.gpio_155", \
                OMAP_PIN_OUTPUT |OMAP_PIN_OFF_OUTPUT_HIGH);
        gpio_request(101, "CHARGE-en");
        gpio_direction_output(101, 0);
        gpio_request(155, "CHARGE-SUSP");
        gpio_direction_output(155, 1);
    }
#ifdef CONFIG_TIWLAN_SDIO
	config_wlan_mux();
#else
	omap4_4430sdp_wifi_init();
#endif

	/* Power on the ULPI PHY */
	//if (gpio_is_valid(OMAP4SDP_MDM_PWR_EN_GPIO)) {
		/* FIXME: Assumes pad is muxed for GPIO mode */
	//	gpio_request(OMAP4SDP_MDM_PWR_EN_GPIO, "USBB1 PHY VMDM_3V3");
	//	gpio_direction_output(OMAP4SDP_MDM_PWR_EN_GPIO, 1);
	//}

	/*
	 * Test board-4430sdp.modem_ipc bootargs value to detect if HSI pad
	 * conf is required
	 */
	//pr_info("Configured modem_ipc: %s", modem_ipc);
	//if (!strcmp(modem_ipc, "hsi")) {
	//	pr_info("Modem HSI detected, set USB port_mode[0] as UNUSED");
		/* USBB1 I/O pads conflict with HSI1 port */
	//	usbhs_pdata.port_mode[0] = OMAP_USBHS_PORT_MODE_UNUSED;
		/* Setup HSI pad conf for OMAP4430 platform */
	//	omap_4430hsi_pad_conf();
	//} else
	//	pr_info("Modem HSI not detected");

	usb_uhhtll_init(&usbhs_pdata);
	usb_musb_init(&musb_board_data);

	spi_register_board_info(sdp4430_spi_board_info,	ARRAY_SIZE(sdp4430_spi_board_info));

	omap_display_init(&sdp4430_dss_data);
	enable_board_wakeup_source();
	omap_voltage_register_pmic(&omap_pmic_core, "core");
	omap_voltage_register_pmic(&omap_pmic_mpu, "mpu");
	omap_voltage_register_pmic(&omap_pmic_iva, "iva");
	omap_voltage_init_vc(&vc_config);
#ifdef CONFIG_TOUCHSCREEN_ILITEK
	omap_ilitek_init();	// chris 2011_0124
#endif //CONFIG_TOUCHSCREEN_ILITEK

    gpio_request(119, "ADO_SPK_ENABLE");
    gpio_direction_output(119, 1);
    gpio_set_value(119, 1);
    gpio_request(120, "SKIPB_GPIO");
    gpio_direction_output(120, 1);
    gpio_set_value(120, 1);
 // Qunata_diagnostic 20110506 set GPIO 171 172 to be input

	omap_writew(omap_readw(0x4a10017C) | 0x011B, 0x4a10017C); 
	omap_writew(omap_readw(0x4a10017C) & ~0x04, 0x4a10017C);
	 
	omap_writew(omap_readw(0x4a10017C) | 0x011B, 0x4a10017E); 
	omap_writew(omap_readw(0x4a10017C) & ~0x04, 0x4a10017E);
////////////////////////////////////////////
}

static void __init omap_4430sdp_map_io(void)
{
	ramconsole_reserve_sdram();
	omap2_set_globals_443x();
	omap44xx_map_common_io();
}

MACHINE_START(OMAP_4430SDP, "OMAP4430")
	/* Maintainer: Santosh Shilimkar - Texas Instruments Inc */
	.phys_io	= 0x48000000,
	.io_pg_offst	= ((0xfa000000) >> 18) & 0xfffc,
	.boot_params	= 0x80000100,
	.map_io		= omap_4430sdp_map_io,
	.init_irq	= omap_4430sdp_init_irq,
	.init_machine	= omap_4430sdp_init,
	.timer		= &omap_timer,
MACHINE_END
