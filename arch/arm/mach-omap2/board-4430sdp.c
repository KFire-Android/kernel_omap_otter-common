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
#include <linux/cdc_tcxo.h>
#include <linux/usb/otg.h>
#include <linux/spi/spi.h>
#include <linux/hwspinlock.h>
#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/i2c/bq2415x.h>
#include <linux/i2c/tmp102.h>
#include <linux/mfd/twl6040.h>
#include <linux/gpio_keys.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/tps6130x.h>
#include <linux/leds.h>
#include <linux/leds_pwm.h>
#include <linux/platform_data/omap4-keypad.h>
#include <linux/platform_data/thermistor_sensor.h>
#include <linux/platform_data/lm75_platform_data.h>
#include <linux/omap_die_governor.h>

#include <mach/hardware.h>
#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/board.h>
#include <plat/vram.h>
#include "common.h"
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/omap4-keypad.h>
#include <plat/drm.h>
#include <plat/remoteproc.h>
#include <plat/omap_apps_brd_id.h>
#include <video/omapdss.h>
#include <video/omap-panel-nokia-dsi.h>
#include <video/omap-panel-picodlp.h>
#include <linux/wl12xx.h>
#include <linux/platform_data/omap-abe-twl6040.h>
#include "omap4_ion.h"
#include <linux/leds-omap4430sdp-display.h>
#include <linux/omap4_duty_cycle_governor.h>
#include <plat/rpmsg_resmgr.h>

#include "mux.h"
#include "hsmmc.h"
#include "control.h"
#include "common-board-devices.h"
#include "board-44xx-blaze.h"
#include "omap_ram_console.h"
#include "pm.h"

#define SDP4430_FB_RAM_SIZE		SZ_16M /* 1920×1080*4 * 2 */

#define ETH_KS8851_IRQ			34
#define ETH_KS8851_POWER_ON		48
#define ETH_KS8851_QUART		138
#define OMAP4_SFH7741_SENSOR_OUTPUT_GPIO	184
#define OMAP4_SFH7741_ENABLE_GPIO		188
#define HDMI_GPIO_CT_CP_HPD 60 /* HPD mode enable/disable */
#define HDMI_GPIO_LS_OE 41 /* Level shifter for HDMI */
#define HDMI_GPIO_HPD  63 /* Hotplug detect */
#define DISPLAY_SEL_GPIO	59	/* LCD2/PicoDLP switch */
#define DLP_POWER_ON_GPIO	40

#define GPIO_WIFI_PMENA		54
#define GPIO_WIFI_IRQ		53

#define FIXED_REG_VBAT_ID	0
#define FIXED_REG_VWLAN_ID	1
#define TPS62361_GPIO   7

#define LED_SEC_DISP_GPIO	27

/* PWM2 and TOGGLE3 register offsets */
#define LED_PWM2ON		0x03
#define LED_PWM2OFF		0x04
#define LED_TOGGLE3		0x92
#define TWL6030_TOGGLE3		0x92
#define PWM2EN			BIT(5)
#define PWM2S			BIT(4)
#define PWM2R			BIT(3)
#define PWM2CTL_MASK		(PWM2EN | PWM2S | PWM2R)

static struct twl6040 *twl6040_driver_data;

static const uint32_t sdp4430_keymap[] = {
	KEY(0, 0, KEY_E),
	KEY(0, 1, KEY_R),
	KEY(0, 2, KEY_T),
	KEY(0, 3, KEY_HOME),
	KEY(0, 4, KEY_F5),
	KEY(0, 5, KEY_UNKNOWN),
	KEY(0, 6, KEY_I),
	KEY(0, 7, KEY_LEFTSHIFT),

	KEY(1, 0, KEY_D),
	KEY(1, 1, KEY_F),
	KEY(1, 2, KEY_G),
	KEY(1, 3, KEY_SEND),
	KEY(1, 4, KEY_F6),
	KEY(1, 5, KEY_UNKNOWN),
	KEY(1, 6, KEY_K),
	KEY(1, 7, KEY_ENTER),

	KEY(2, 0, KEY_X),
	KEY(2, 1, KEY_C),
	KEY(2, 2, KEY_V),
	KEY(2, 3, KEY_END),
	KEY(2, 4, KEY_F7),
	KEY(2, 5, KEY_UNKNOWN),
	KEY(2, 6, KEY_DOT),
	KEY(2, 7, KEY_CAPSLOCK),

	KEY(3, 0, KEY_Z),
	KEY(3, 1, KEY_KPPLUS),
	KEY(3, 2, KEY_B),
	KEY(3, 3, KEY_F1),
	KEY(3, 4, KEY_F8),
	KEY(3, 5, KEY_UNKNOWN),
	KEY(3, 6, KEY_O),
	KEY(3, 7, KEY_SPACE),

	KEY(4, 0, KEY_W),
	KEY(4, 1, KEY_Y),
	KEY(4, 2, KEY_U),
	KEY(4, 3, KEY_F2),
	KEY(4, 4, KEY_VOLUMEUP),
	KEY(4, 5, KEY_UNKNOWN),
	KEY(4, 6, KEY_L),
	KEY(4, 7, KEY_LEFT),

	KEY(5, 0, KEY_S),
	KEY(5, 1, KEY_H),
	KEY(5, 2, KEY_J),
	KEY(5, 3, KEY_F3),
	KEY(5, 4, KEY_F9),
	KEY(5, 5, KEY_VOLUMEDOWN),
	KEY(5, 6, KEY_M),
	KEY(5, 7, KEY_RIGHT),

	KEY(6, 0, KEY_Q),
	KEY(6, 1, KEY_A),
	KEY(6, 2, KEY_N),
	KEY(6, 3, KEY_BACK),
	KEY(6, 4, KEY_BACKSPACE),
	KEY(6, 5, KEY_UNKNOWN),
	KEY(6, 6, KEY_P),
	KEY(6, 7, KEY_UP),

	KEY(7, 0, KEY_PROG1),
	KEY(7, 1, KEY_PROG2),
	KEY(7, 2, KEY_PROG3),
	KEY(7, 3, KEY_PROG4),
	KEY(7, 4, KEY_F4),
	KEY(7, 5, KEY_UNKNOWN),
	KEY(7, 6, KEY_OK),
	KEY(7, 7, KEY_DOWN),
};
static struct omap_device_pad keypad_pads[] = {
	{	.name   = "kpd_col1.kpd_col1",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "kpd_col1.kpd_col1",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "kpd_col2.kpd_col2",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "kpd_col3.kpd_col3",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "kpd_col4.kpd_col4",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "kpd_col5.kpd_col5",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "gpmc_a23.kpd_col7",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "gpmc_a22.kpd_col6",
		.enable = OMAP_WAKEUP_EN | OMAP_MUX_MODE1,
	},
	{	.name   = "kpd_row0.kpd_row0",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "kpd_row1.kpd_row1",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "kpd_row2.kpd_row2",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "kpd_row3.kpd_row3",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "kpd_row4.kpd_row4",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "kpd_row5.kpd_row5",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "gpmc_a18.kpd_row6",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
	{	.name   = "gpmc_a19.kpd_row7",
		.enable = OMAP_PULL_ENA | OMAP_PULL_UP | OMAP_WAKEUP_EN |
			OMAP_MUX_MODE1 | OMAP_INPUT_EN,
	},
};

static struct matrix_keymap_data sdp4430_keymap_data = {
	.keymap			= sdp4430_keymap,
	.keymap_size		= ARRAY_SIZE(sdp4430_keymap),
};

static struct omap4_keypad_platform_data sdp4430_keypad_data = {
	.keymap_data		= &sdp4430_keymap_data,
	.rows			= 8,
	.cols			= 8,
};

static struct omap_board_data keypad_data = {
	.id	    		= 1,
	.pads	 		= keypad_pads,
	.pads_cnt       	= ARRAY_SIZE(keypad_pads),
};

#ifdef CONFIG_OMAP4_DUTY_CYCLE_GOVERNOR

static struct pcb_section omap4_duty_governor_pcb_sections[] = {
	{
		.pcb_temp_level			= DUTY_GOVERNOR_DEFAULT_TEMP,
		.max_opp			= 1200000,
		.duty_cycle_enabled		= true,
		.tduty_params = {
			.nitro_rate		= 1200000,
			.cooling_rate		= 1008000,
			.nitro_interval		= 20000,
			.nitro_percentage	= 24,
		},
	},
};

static void init_duty_governor(void)
{
	omap4_duty_pcb_section_reg(omap4_duty_governor_pcb_sections,
				   ARRAY_SIZE
				   (omap4_duty_governor_pcb_sections));
}
#else
static void init_duty_governor(void){}
#endif /*CONFIG_OMAP4_DUTY_CYCLE*/

/* Initial set of thresholds for different thermal zones */
static struct omap_thermal_zone thermal_zones[] = {
	OMAP_THERMAL_ZONE("safe", 0, 25000, 65000, 250, 1000, 400),
	OMAP_THERMAL_ZONE("monitor", 0, 60000, 80000, 250, 250,	250),
	OMAP_THERMAL_ZONE("alert", 0, 75000, 90000, 250, 250, 150),
	OMAP_THERMAL_ZONE("critical", 1, 85000,	115000,	250, 250, 50),
};

static struct omap_die_governor_pdata omap_gov_pdata = {
	.zones = thermal_zones,
	.zones_num = ARRAY_SIZE(thermal_zones),
};

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
		.name	= "omap4:blue:user",
		.gpio	= 169,
	},
	{
		.name	= "omap4:red:user",
		.gpio	= 170,
	},
	{
		.name	= "omap4:green:user",
		.gpio	= 139,
	},

};

static struct gpio_keys_button sdp4430_gpio_keys[] = {
	{
		.desc			= "Proximity Sensor",
		.type			= EV_SW,
		.code			= SW_FRONT_PROXIMITY,
		.gpio			= OMAP4_SFH7741_SENSOR_OUTPUT_GPIO,
		.active_low		= 0,
	}
};

static struct gpio_led_platform_data sdp4430_led_data = {
	.leds	= sdp4430_gpio_leds,
	.num_leds	= ARRAY_SIZE(sdp4430_gpio_leds),
};

static struct led_pwm sdp4430_pwm_leds[] = {
	{
		.name		= "omap4:green:chrg",
		.pwm_id		= 1,
		.max_brightness	= 255,
		.pwm_period_ns	= 7812500,
	},
};

static struct led_pwm_platform_data sdp4430_pwm_data = {
	.num_leds	= ARRAY_SIZE(sdp4430_pwm_leds),
	.leds		= sdp4430_pwm_leds,
};

static struct platform_device sdp4430_leds_pwm = {
	.name	= "leds_pwm",
	.id	= -1,
	.dev	= {
		.platform_data = &sdp4430_pwm_data,
	},
};

static struct thermistor_pdata thermistor_device_data = {
	.name = "thermistor_sensor",
	.slope = 1142,
	.offset = -393,
	.domain = "pcb",
};

static struct platform_device thermistor = {
	.name   =       "thermistor",
	.id     =       -1,
	.dev    = {
		.platform_data = &thermistor_device_data,
	},
};

static int omap_prox_activate(struct device *dev)
{
	gpio_set_value(OMAP4_SFH7741_ENABLE_GPIO , 1);
	return 0;
}

static void omap_prox_deactivate(struct device *dev)
{
	gpio_set_value(OMAP4_SFH7741_ENABLE_GPIO , 0);
}

static struct gpio_keys_platform_data sdp4430_gpio_keys_data = {
	.buttons	= sdp4430_gpio_keys,
	.nbuttons	= ARRAY_SIZE(sdp4430_gpio_keys),
	.enable		= omap_prox_activate,
	.disable	= omap_prox_deactivate,
};

static struct platform_device sdp4430_gpio_keys_device = {
	.name	= "gpio-keys",
	.id	= -1,
	.dev	= {
		.platform_data	= &sdp4430_gpio_keys_data,
	},
};

static struct platform_device sdp4430_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data = &sdp4430_led_data,
	},
};
static struct spi_board_info sdp4430_spi_board_info[] __initdata = {
	{
		.modalias               = "ks8851",
		.bus_num                = 1,
		.chip_select            = 0,
		.max_speed_hz           = 24000000,
		/*
		 * .irq is set to gpio_to_irq(ETH_KS8851_IRQ)
		 * in omap_4430sdp_init
		 */
	},
};

static struct gpio sdp4430_eth_gpios[] __initdata = {
	{ ETH_KS8851_POWER_ON,	GPIOF_OUT_INIT_HIGH,	"eth_power"	},
	{ ETH_KS8851_QUART,	GPIOF_OUT_INIT_HIGH,	"quart"		},
	{ ETH_KS8851_IRQ,	GPIOF_IN,		"eth_irq"	},
};

static int __init omap_ethernet_init(void)
{
	int status;

	/* Request of GPIO lines */
	status = gpio_request_array(sdp4430_eth_gpios,
				    ARRAY_SIZE(sdp4430_eth_gpios));
	if (status)
		pr_err("Cannot request ETH GPIOs\n");

	return status;
}

static struct regulator_consumer_supply sdp4430_vbat_supply[] = {
	REGULATOR_SUPPLY("vddvibl", "twl6040-vibra"),
	REGULATOR_SUPPLY("vddvibr", "twl6040-vibra"),
};

static struct regulator_init_data sdp4430_vbat_data = {
	.constraints = {
		.always_on	= 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(sdp4430_vbat_supply),
	.consumer_supplies	= sdp4430_vbat_supply,
};

static struct fixed_voltage_config sdp4430_vbat_pdata = {
	.supply_name	= "VBAT",
	.microvolts	= 3750000,
	.init_data	= &sdp4430_vbat_data,
	.gpio		= -EINVAL,
};

static struct platform_device sdp4430_vbat = {
	.name		= "reg-fixed-voltage",
	.id		= FIXED_REG_VBAT_ID,
	.dev = {
		.platform_data = &sdp4430_vbat_pdata,
	},
};

static struct platform_device sdp4430_dmic_codec = {
	.name	= "dmic-codec",
	.id	= -1,
};

static struct platform_device sdp4430_spdif_dit_codec = {
	.name           = "spdif-dit",
	.id             = -1,
};

static struct platform_device sdp4430_hdmi_audio_codec = {
	.name	= "hdmi-audio-codec",
	.id	= -1,
};

static struct omap_abe_twl6040_data sdp4430_abe_audio_data = {
	.card_name = "SDP4430",
	.has_hs		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_hf		= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_ep		= 1,
	.has_aux	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,
	.has_vibra	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,

	.has_abe	= 1,
	.has_dmic	= 1,
	.has_hsmic	= 1,
	.has_mainmic	= 1,
	.has_submic	= 1,
	.has_afm	= ABE_TWL6040_LEFT | ABE_TWL6040_RIGHT,

	.jack_detection = 1,
	/* MCLK input is 38.4MHz */
	.mclk_freq	= 38400000,
};

static struct platform_device sdp4430_abe_audio = {
	.name		= "omap-abe-twl6040",
	.id		= -1,
	.dev = {
		.platform_data = &sdp4430_abe_audio_data,
	},
};

static struct platform_device *sdp4430_devices[] __initdata = {
	&sdp4430_gpio_keys_device,
	&sdp4430_leds_gpio,
	&sdp4430_leds_pwm,
	&sdp4430_vbat,
	&sdp4430_dmic_codec,
	&sdp4430_spdif_dit_codec,
	&sdp4430_abe_audio,
	&sdp4430_hdmi_audio_codec,
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
	.mode			= MUSB_OTG,
	.power			= 100,
};

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 2,
		.caps		=  MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable   = true,
		.ocr_mask	= MMC_VDD_29_30,
		.no_off_init	= true,
		.built_in	= true,
	},
	{
		.mmc		= 1,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
	},
	{
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.pm_caps	= MMC_PM_KEEP_POWER,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195,
		.nonremovable	= true,
	},
	{}	/* Terminator */
};

static struct regulator_consumer_supply omap4_sdp4430_vmmc5_supply = {
	.supply = "vmmc",
	.dev_name = "omap_hsmmc.4",
};

static struct regulator_init_data sdp4430_vmmc5 = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &omap4_sdp4430_vmmc5_supply,
};

static struct fixed_voltage_config sdp4430_vwlan = {
	.supply_name		= "vwl1271",
	.microvolts		= 1800000, /* 1.8V */
	.gpio			= GPIO_WIFI_PMENA,
	.startup_delay		= 70000, /* 70msec */
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &sdp4430_vmmc5,
};

static struct platform_device omap_vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= FIXED_REG_VWLAN_ID,
	.dev = {
		.platform_data = &sdp4430_vwlan,
	},
};

static int omap4_twl6030_hsmmc_late_init(struct device *dev)
{
	int irq = 0;
	struct platform_device *pdev = container_of(dev,
				struct platform_device, dev);
	struct omap_mmc_platform_data *pdata = dev->platform_data;

	/* Setting MMC1 Card detect Irq */
	if (pdev->id == 0) {
		irq = twl6030_mmc_card_detect_config();
		if (irq < 0) {
			pr_err("Failed configuring MMC1 card detect\n");
			return irq;
		}
		pdata->slots[0].card_detect_irq = irq;
		pdata->slots[0].card_detect = twl6030_mmc_card_detect;
	}
	return 0;
}

static __init void omap4_twl6030_hsmmc_set_late_init(struct device *dev)
{
	struct omap_mmc_platform_data *pdata;

	/* dev can be null if CONFIG_MMC_OMAP_HS is not set */
	if (!dev) {
		pr_err("Failed %s\n", __func__);
		return;
	}
	pdata = dev->platform_data;
	pdata->init =	omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(&c->pdev->dev);

	return 0;
}

static int tps6130x_enable(int on)
{
	u8 mask;
	int ret;

	if (!twl6040_driver_data) {
		pr_err("%s: invalid twl6040 driver data\n", __func__);
		return -EINVAL;
	}

	/*
	 * tps6130x NRESET driven by:
	 * - GPO2 in TWL6040
	 * - GPO in TWL6041 (only one GPO supported)
	 */
	if (twl6040_driver_data->rev >= TWL6041_REV_2_0)
		mask = TWL6040_GPO1;
	else
		mask = TWL6040_GPO2;

	if (on)
		ret = twl6040_set_bits(twl6040_driver_data,
				       TWL6040_REG_GPOCTL, mask);
	else
		ret = twl6040_clear_bits(twl6040_driver_data,
					 TWL6040_REG_GPOCTL, mask);

	if (ret < 0)
		pr_err("%s: failed to write GPOCTL %d\n", __func__, ret);
	return ret;
}

static struct tps6130x_platform_data tps6130x_pdata = {
	.chip_enable	= tps6130x_enable,
};

static struct regulator_consumer_supply twl6040_vddhf_supply[] = {
	REGULATOR_SUPPLY("vddhf", "twl6040-codec"),
};

static struct regulator_init_data twl6040_vddhf_data = {
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

static struct twl6040_codec_data twl6040_codec = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step	= 0x0f,
	.hs_right_step	= 0x0f,
	.hf_left_step	= 0x1d,
	.hf_right_step	= 0x1d,
	.vddhf_uV	= 4075000,
};

static struct twl6040_vibra_data twl6040_vibra = {
	.vibldrv_res = 8,
	.vibrdrv_res = 3,
	.viblmotor_res = 10,
	.vibrmotor_res = 10,
	.vddvibl_uV = 0,	/* fixed volt supply - VBAT */
	.vddvibr_uV = 0,	/* fixed volt supply - VBAT */

	.max_timeout = 15000,
	.initial_vibrate = 0,
	.voltage_raise_speed = 0x26,
};

static struct i2c_board_info sdp4430_i2c_1_tps6130x_boardinfo = {
	I2C_BOARD_INFO("tps6130x", 0x33),
	.platform_data = &twl6040_vddhf_data,
};

static int twl6040_platform_init(struct twl6040 *twl6040)
{
	if (cpu_is_omap447x())
		return 0;

	if (!twl6040) {
		pr_err("%s: invalid twl6040 driver data\n", __func__);
		return -EINVAL;
	}

	twl6040_driver_data = twl6040;

	tps6130x_pdata.adapter = i2c_get_adapter(1);
	if (!tps6130x_pdata.adapter) {
		pr_err("%s: can't get i2c adapter\n", __func__);
		return -ENODEV;
	}

	tps6130x_pdata.client = i2c_new_device(tps6130x_pdata.adapter,
		&sdp4430_i2c_1_tps6130x_boardinfo);
	if (!tps6130x_pdata.client) {
		pr_err("%s: can't add i2c device\n", __func__);
		i2c_put_adapter(tps6130x_pdata.adapter);
		return -ENODEV;
	}

	return 0;
}

static int twl6040_platform_exit(struct twl6040 *twl6040)
{
	if (cpu_is_omap447x())
		return 0;

	if (tps6130x_pdata.client) {
		i2c_unregister_device(tps6130x_pdata.client);
		tps6130x_pdata.client = NULL;
		i2c_put_adapter(tps6130x_pdata.adapter);
	}

	return 0;
}

static struct twl6040_platform_data twl6040_data = {
	.codec		= &twl6040_codec,
	.vibra		= &twl6040_vibra,
	.audpwron_gpio	= 127,
	.platform_init	= twl6040_platform_init,
	.platform_exit	= twl6040_platform_exit,
};

/*
 * Setup CFG_TRANS mode as follows:
 * 0x00 (OFF) when in OFF state(bit offset 4)
 * - these bits a read only, so don't touch them
 * 0x00 (OFF) when in sleep (bit offset 2)
 * 0x01 (PWM/PFM Auto) when in ACTive state (bit offset 0)
 */
#define TWL6030_REG_VCOREx_CFG_TRANS_MODE		(0x00 << 4 | \
		TWL6030_RES_OFF_CMD << TWL6030_REG_CFG_TRANS_SLEEP_CMD_OFFSET |\
		TWL6030_RES_AUTO_CMD << TWL6030_REG_CFG_TRANS_ACT_CMD_OFFSET)

#define TWL6030_REG_VCOREx_CFG_TRANS_MODE_DESC "OFF=OFF SLEEP=OFF ACT=AUTO"

/* OMAP4430 - All vcores: 1, 2 and 3 should go down with PREQ */
static struct twl_reg_setup_array omap4430_twl6030_setup[] = {
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VCORE1_CFG_TRANS,
		.val = TWL6030_REG_VCOREx_CFG_TRANS_MODE,
		.desc = "VCORE1 " TWL6030_REG_VCOREx_CFG_TRANS_MODE_DESC,
	},
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VCORE2_CFG_TRANS,
		.val = TWL6030_REG_VCOREx_CFG_TRANS_MODE,
		.desc = "VCORE2 " TWL6030_REG_VCOREx_CFG_TRANS_MODE_DESC,
	},
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VCORE3_CFG_TRANS,
		.val = TWL6030_REG_VCOREx_CFG_TRANS_MODE,
		.desc = "VCORE3 " TWL6030_REG_VCOREx_CFG_TRANS_MODE_DESC,
	},
	{ .desc = NULL} /* TERMINATOR */
};

/* OMAP4460 - VCORE3 is unused, 1 and 2 should go down with PREQ */
static struct twl_reg_setup_array omap4460_twl6030_setup[] = {
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VCORE1_CFG_TRANS,
		.val = TWL6030_REG_VCOREx_CFG_TRANS_MODE,
		.desc = "VCORE 1" TWL6030_REG_VCOREx_CFG_TRANS_MODE_DESC,
	},
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VCORE2_CFG_TRANS,
		.val = TWL6030_REG_VCOREx_CFG_TRANS_MODE,
		.desc = "VCORE2 " TWL6030_REG_VCOREx_CFG_TRANS_MODE_DESC,
	},
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_CFG_SMPS_PD,
		.val = 0x77,
		.desc = "VCORE1 disable PD on shutdown",
	},
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VCORE3_CFG_GRP,
		.val = 0x00,
		.desc = "VCORE3 - remove binding to groups",
	},
	{
		.mod_no = TWL6030_MODULE_ID0,
		.addr = TWL6030_REG_VMEM_CFG_GRP,
		.val = 0x00,
		.desc = "VMEM - remove binding to groups",
	},
	{ .desc = NULL} /* TERMINATOR */
};

static struct twl4030_platform_data sdp4430_twldata;

/*
 * The Clock Driver Chip (TCXO) on OMAP4 based SDP needs to be programmed
 * to output CLK1 based on REQ1 from OMAP.
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
		0,
	},
};

static struct bq2415x_platform_data tablet_bqdata = {
		.max_charger_voltagemV = 4200,
		.max_charger_currentmA = 1550,
};

static struct i2c_board_info __initdata sdp4430_i2c_boardinfo[] = {
	{
		I2C_BOARD_INFO("cdc_tcxo_driver", 0x6c),
		.platform_data = &sdp4430_cdc_data,
	},
	{
		I2C_BOARD_INFO("bq24156", 0x6a),
		.platform_data = &tablet_bqdata,
	},
};

/* TMP102 PCB Temperature sensor close to OMAP
 * values for .slope and .offset are taken from OMAP5 structure,
 * as TMP102 sensor was not used by domains other then CPU
 */
static struct tmp102_platform_data tmp102_omap_info = {
	.slope = 470,
	.slope_cpu = 1063,
	.offset = -1272,
	.offset_cpu = -477,
	.domain = "pcb", /* for hotspot extrapolation */
};

static struct i2c_board_info __initdata sdp4430_i2c_4_tmp102_boardinfo[] = {
	{
			I2C_BOARD_INFO("tmp102_temp_sensor", 0x48),
			.platform_data = &tmp102_omap_info,
	},
};

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
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", \
								bus_id);
	}
}

static struct omap_i2c_bus_board_data __initdata omap4_i2c_1_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap4_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap4_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata omap4_i2c_4_bus_pdata;

static int __init omap4_i2c_init(void)
{
	omap_i2c_hwspinlock_init(1, 0, &omap4_i2c_1_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &omap4_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &omap4_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &omap4_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &omap4_i2c_1_bus_pdata);
	omap_register_i2c_bus_board_data(2, &omap4_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &omap4_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &omap4_i2c_4_bus_pdata);

	omap4_pmic_get_config(&sdp4430_twldata, TWL_COMMON_PDATA_USB |
			TWL_COMMON_PDATA_MADC | \
			TWL_COMMON_PDATA_BCI |
			TWL_COMMON_PDATA_THERMAL,
			TWL_COMMON_REGULATOR_VDAC |
			TWL_COMMON_REGULATOR_VAUX1 |
			TWL_COMMON_REGULATOR_VAUX2 |
			TWL_COMMON_REGULATOR_VAUX3 |
			TWL_COMMON_REGULATOR_VMMC |
			TWL_COMMON_REGULATOR_VPP |
			TWL_COMMON_REGULATOR_VUSIM |
			TWL_COMMON_REGULATOR_VANA |
			TWL_COMMON_REGULATOR_VCXIO |
			TWL_COMMON_REGULATOR_VUSB |
			TWL_COMMON_REGULATOR_CLK32KG |
			TWL_COMMON_REGULATOR_V1V8 |
			TWL_COMMON_REGULATOR_V2V1 |
			TWL_COMMON_REGULATOR_SYSEN |
			TWL_COMMON_REGULATOR_CLK32KAUDIO |
			TWL_COMMON_REGULATOR_REGEN1);

	/* Add one-time registers configuration */
	if (cpu_is_omap443x())
		sdp4430_twldata.reg_setup_script = omap4430_twl6030_setup;
	else if (cpu_is_omap446x())
		sdp4430_twldata.reg_setup_script = omap4460_twl6030_setup;

	/*
	 * tps6130x regulator provides VDDHF supply for hands-free module
	 * (part of twl6040) only on OMAP4430 and OMAP4460 boards.
	 */
	if (cpu_is_omap447x() && twl6040_data.codec)
		twl6040_data.codec->vddhf_uV = 0;

	omap4_pmic_init("twl6030", &sdp4430_twldata,
			&twl6040_data, OMAP44XX_IRQ_SYS_2N);
	i2c_register_board_info(1, sdp4430_i2c_boardinfo,
				ARRAY_SIZE(sdp4430_i2c_boardinfo));
	omap_register_i2c_bus(2, 400, NULL, 0);

	if (cpu_is_omap447x())
		i2c_register_board_info(4, sdp4430_i2c_4_tmp102_boardinfo,
					ARRAY_SIZE
					(sdp4430_i2c_4_tmp102_boardinfo));

	/*
	 * This will allow unused regulator to be shutdown. This flag
	 * should be set in the board file. Before regulators are registered.
	 */
	regulator_has_full_constraints();

	/*
	 * Drive MSECURE high for TWL6030/6032 write access.
	 */
	omap_mux_init_signal("fref_clk0_out.gpio_wk6", OMAP_PIN_OUTPUT);
	gpio_request(6, "msecure");
	gpio_direction_output(6, 1);

	return 0;
}

static int sdp4430_panel_enable_hdmi(struct omap_dss_device *dssdev)
{
	return 0;
}

static void sdp4430_panel_disable_hdmi(struct omap_dss_device *dssdev)
{
}

static void blaze_init_display_led(void)
{
	/* Set maximum brightness on init */
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x00, LED_PWM2ON);
	twl_i2c_write_u8(TWL_MODULE_PWM, 0x00, LED_PWM2OFF);
	twl_i2c_write_u8(TWL6030_MODULE_ID1, PWM2S | PWM2EN, TWL6030_TOGGLE3);
}

static void blaze_set_primary_brightness(u8 brightness)
{
	u8 val;
	static unsigned enabled = 0x01;

	if (brightness) {

		/*
		 * Converting brightness 8-bit value into 7-bit value
		 * for PWM cycles.
		 * We need to check brightness maximum value for set
		 * PWM2OFF equal to PWM2ON.
		 * Additionally checking for 1 for prevent seting maximum
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
			enabled = 0x01;
		}
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

static void blaze_set_secondary_brightness(u8 brightness)
{
	if (brightness > 0)
		brightness = 1;

	gpio_set_value(LED_SEC_DISP_GPIO, brightness);
}

static struct omap4430_sdp_disp_led_platform_data blaze_disp_led_data = {
	.flags = LEDS_CTRL_AS_ONE_DISPLAY,
	.display_led_init = blaze_init_display_led,
	.primary_display_set = blaze_set_primary_brightness,
	.secondary_display_set = blaze_set_secondary_brightness,
};

static struct platform_device blaze_disp_led = {
	.name	=	"display_led",
	.id	=	-1,
	.dev	= {
		.platform_data = &blaze_disp_led_data,
	},
};

static struct nokia_dsi_panel_data dsi1_panel = {
		.name		= "taal",
		.reset_gpio	= 102,
		.use_ext_te	= false,
		.ext_te_gpio	= 101,
		.esd_interval	= 0,
};

static struct omap_dss_device sdp4430_lcd_device = {
	.name			= "lcd",
	.driver_name		= "taal",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi1_panel,
	.phy.dsi		= {
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,

		.module		= 0,
	},

	.clocks = {
		.dispc = {
			.channel = {
				/* Logic Clock = 172.8 MHz */
				.lck_div	= 1,
				/* Pixel Clock = 34.56 MHz */
				.pck_div	= 5,
				.lcd_clk_src	= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn		= 16,	/* Fint = 2.4 MHz */
			.regm		= 180,	/* DDR Clock = 216 MHz */
			.regm_dispc	= 5,	/* PLL1_CLK1 = 172.8 MHz */
			.regm_dsi	= 5,	/* PLL1_CLK2 = 172.8 MHz */

			.lp_clk_div	= 10,	/* LP Clock = 8.64 MHz */
			.dsi_fclk_src	= OMAP_DSS_CLK_SRC_DSI_PLL_HSDIV_DSI,
		},
	},
	.channel		= OMAP_DSS_CHANNEL_LCD,
};

static struct nokia_dsi_panel_data dsi2_panel = {
		.name		= "taal",
		.reset_gpio	= 104,
		.use_ext_te	= false,
		.ext_te_gpio	= 103,
		.esd_interval	= 0,
};

static struct omap_dss_device sdp4430_lcd2_device = {
	.name			= "lcd2",
	.driver_name		= "taal",
	.type			= OMAP_DISPLAY_TYPE_DSI,
	.data			= &dsi2_panel,
	.phy.dsi		= {
		.clk_lane	= 1,
		.clk_pol	= 0,
		.data1_lane	= 2,
		.data1_pol	= 0,
		.data2_lane	= 3,
		.data2_pol	= 0,

		.module		= 1,
	},

	.clocks = {
		.dispc = {
			.channel = {
				/* Logic Clock = 172.8 MHz */
				.lck_div	= 1,
				/* Pixel Clock = 34.56 MHz */
				.pck_div	= 5,
				.lcd_clk_src	= OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DISPC,
			},
			.dispc_fclk_src	= OMAP_DSS_CLK_SRC_FCK,
		},

		.dsi = {
			.regn		= 16,	/* Fint = 2.4 MHz */
			.regm		= 180,	/* DDR Clock = 216 MHz */
			.regm_dispc	= 5,	/* PLL1_CLK1 = 172.8 MHz */
			.regm_dsi	= 5,	/* PLL1_CLK2 = 172.8 MHz */

			.lp_clk_div	= 10,	/* LP Clock = 8.64 MHz */
			.dsi_fclk_src	= OMAP_DSS_CLK_SRC_DSI2_PLL_HSDIV_DSI,
		},
	},
	.channel		= OMAP_DSS_CHANNEL_LCD2,
};

static void sdp4430_lcd_init(void)
{
	int r;

	r = gpio_request_one(dsi1_panel.reset_gpio, GPIOF_DIR_OUT,
		"lcd1_reset_gpio");
	if (r)
		pr_err("%s: Could not get lcd1_reset_gpio\n", __func__);

	r = gpio_request_one(dsi2_panel.reset_gpio, GPIOF_DIR_OUT,
		"lcd2_reset_gpio");
	if (r)
		pr_err("%s: Could not get lcd2_reset_gpio\n", __func__);
}

static struct omap_dss_hdmi_data sdp4430_hdmi_data = {
	.hpd_gpio = HDMI_GPIO_HPD,
	.ls_oe_gpio = HDMI_GPIO_LS_OE,
	.ct_cp_hpd_gpio = HDMI_GPIO_CT_CP_HPD,
};

static struct omap_dss_device sdp4430_hdmi_device = {
	.name = "hdmi",
	.driver_name = "hdmi_panel",
	.type = OMAP_DISPLAY_TYPE_HDMI,
	.platform_enable = sdp4430_panel_enable_hdmi,
	.platform_disable = sdp4430_panel_disable_hdmi,
	.channel = OMAP_DSS_CHANNEL_DIGIT,
	.data = &sdp4430_hdmi_data,
};

static struct picodlp_panel_data sdp4430_picodlp_pdata = {
	.picodlp_adapter_id	= 2,
	.emu_done_gpio		= 44,
	.pwrgood_gpio		= 45,
};

static void sdp4430_picodlp_init(void)
{
	int r;
	const struct gpio picodlp_gpios[] = {
		{DLP_POWER_ON_GPIO, GPIOF_OUT_INIT_LOW,
			"DLP POWER ON"},
		{sdp4430_picodlp_pdata.emu_done_gpio, GPIOF_IN,
			"DLP EMU DONE"},
		{sdp4430_picodlp_pdata.pwrgood_gpio, GPIOF_OUT_INIT_LOW,
			"DLP PWRGOOD"},
	};

	r = gpio_request_array(picodlp_gpios, ARRAY_SIZE(picodlp_gpios));
	if (r)
		pr_err("Cannot request PicoDLP GPIOs, error %d\n", r);
}

static int sdp4430_panel_enable_picodlp(struct omap_dss_device *dssdev)
{
	gpio_set_value(DISPLAY_SEL_GPIO, 0);
	gpio_set_value(DLP_POWER_ON_GPIO, 1);

	return 0;
}

static void sdp4430_panel_disable_picodlp(struct omap_dss_device *dssdev)
{
	gpio_set_value(DLP_POWER_ON_GPIO, 0);
	gpio_set_value(DISPLAY_SEL_GPIO, 1);
}

static struct omap_dss_device sdp4430_picodlp_device = {
	.name			= "picodlp",
	.driver_name		= "picodlp_panel",
	.type			= OMAP_DISPLAY_TYPE_DPI,
	.phy.dpi.data_lines	= 24,
	.channel		= OMAP_DSS_CHANNEL_LCD2,
	.platform_enable	= sdp4430_panel_enable_picodlp,
	.platform_disable	= sdp4430_panel_disable_picodlp,
	.data			= &sdp4430_picodlp_pdata,
};

static struct omap_dss_device *sdp4430_dss_devices[] = {
	&sdp4430_lcd_device,
	&sdp4430_hdmi_device,
	&sdp4430_lcd2_device,
	&sdp4430_picodlp_device,
};

static struct omap_dss_board_info sdp4430_dss_data = {
	.num_devices	= ARRAY_SIZE(sdp4430_dss_devices),
	.devices	= sdp4430_dss_devices,
	.default_device	= &sdp4430_lcd_device,
};

static void __init omap_4430sdp_display_init(void)
{
	int r;

	omap_mux_init_signal("fref_clk4_out.fref_clk4_out",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);

	/* Enable LCD2 by default (instead of Pico DLP) */
	r = gpio_request_one(DISPLAY_SEL_GPIO, GPIOF_OUT_INIT_HIGH,
			"display_sel");
	if (r)
		pr_err("%s: Could not get display_sel GPIO\n", __func__);

	platform_device_register(&blaze_disp_led);
	sdp4430_lcd_init();
	sdp4430_picodlp_init();
	omap_vram_set_sdram_vram(SDP4430_FB_RAM_SIZE, 0);
	omap_display_init(&sdp4430_dss_data);
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

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP4_MUX(USBB2_ULPITLL_CLK, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT
					| OMAP_PULL_ENA),
	OMAP4_MUX(SYS_NIRQ2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
					| OMAP_PIN_OFF_WAKEUPENABLE),
	OMAP4_MUX(SYS_NIRQ1, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP
					| OMAP_PIN_OFF_WAKEUPENABLE),

	/* IO optimization pdpu and offmode settings to reduce leakage */
	OMAP4_MUX(GPMC_A17, OMAP_MUX_MODE3 | OMAP_INPUT_EN),
	OMAP4_MUX(GPMC_NBE1, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT),
	OMAP4_MUX(GPMC_NCS4, OMAP_MUX_MODE3 | OMAP_INPUT_EN),
	OMAP4_MUX(GPMC_NCS5, OMAP_MUX_MODE3 | OMAP_PULL_ENA | OMAP_PULL_UP
					| OMAP_OFF_EN | OMAP_OFF_PULL_EN),
	OMAP4_MUX(GPMC_NCS7, OMAP_MUX_MODE3 | OMAP_INPUT_EN),
	OMAP4_MUX(GPMC_NBE1, OMAP_MUX_MODE3 | OMAP_PULL_ENA | OMAP_PULL_UP
					| OMAP_OFF_EN | OMAP_OFF_PULL_EN),
	OMAP4_MUX(GPMC_WAIT0, OMAP_MUX_MODE3 | OMAP_INPUT_EN),
	OMAP4_MUX(GPMC_NOE, OMAP_MUX_MODE1 | OMAP_INPUT_EN),
	OMAP4_MUX(MCSPI1_CS1, OMAP_MUX_MODE3 | OMAP_PULL_ENA | OMAP_PULL_UP
					| OMAP_OFF_EN | OMAP_OFF_PULL_EN),
	OMAP4_MUX(MCSPI1_CS2, OMAP_MUX_MODE3 | OMAP_PULL_ENA | OMAP_PULL_UP
					| OMAP_OFF_EN | OMAP_OFF_PULL_EN),
	OMAP4_MUX(SDMMC5_CLK, OMAP_MUX_MODE0 | OMAP_INPUT_EN | OMAP_OFF_EN
					| OMAP_OFF_PULL_EN),
	OMAP4_MUX(GPMC_NCS1, OMAP_MUX_MODE3 | OMAP_INPUT_EN | OMAP_WAKEUP_EN),
	OMAP4_MUX(GPMC_A24, OMAP_MUX_MODE3 | OMAP_INPUT_EN | OMAP_WAKEUP_EN),

	OMAP4_MUX(ABE_MCBSP1_DR, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLDOWN),
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

#else
#define board_mux	NULL
 #endif

static void __init omap4_sdp4430_wifi_mux_init(void)
{
	omap_mux_init_gpio(GPIO_WIFI_IRQ, OMAP_PIN_INPUT |
				OMAP_PIN_OFF_WAKEUPENABLE);
	omap_mux_init_gpio(GPIO_WIFI_PMENA, OMAP_PIN_OUTPUT);

	omap_mux_init_signal("sdmmc5_cmd.sdmmc5_cmd",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("sdmmc5_clk.sdmmc5_clk",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("sdmmc5_dat0.sdmmc5_dat0",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("sdmmc5_dat1.sdmmc5_dat1",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("sdmmc5_dat2.sdmmc5_dat2",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);
	omap_mux_init_signal("sdmmc5_dat3.sdmmc5_dat3",
				OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP);

}

static struct wl12xx_platform_data omap4_sdp4430_wlan_data __initdata = {
	.board_ref_clock = WL12XX_REFCLOCK_26,
	.board_tcxo_clock = WL12XX_TCXOCLOCK_26,
};

static void __init omap4_sdp4430_wifi_init(void)
{
	int ret;

	omap4_sdp4430_wifi_mux_init();
	omap4_sdp4430_wlan_data.irq = gpio_to_irq(GPIO_WIFI_IRQ);
	ret = wl12xx_set_platform_data(&omap4_sdp4430_wlan_data);
	if (ret)
		pr_err("Error setting wl12xx data: %d\n", ret);
	ret = platform_device_register(&omap_vwlan_device);
	if (ret)
		pr_err("Error registering wl12xx device: %d\n", ret);
}

#if defined(CONFIG_USB_EHCI_HCD_OMAP) || defined(CONFIG_USB_OHCI_HCD_OMAP3)
static const struct usbhs_omap_board_data usbhs_bdata __initconst = {
	.port_mode[0] = OMAP_EHCI_PORT_MODE_PHY,
	.port_mode[1] = OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0,
	.port_mode[2] = OMAP_USBHS_PORT_MODE_UNUSED,
	.phy_reset  = false,
	.reset_gpio_port[0]  = -EINVAL,
	.reset_gpio_port[1]  = -EINVAL,
	.reset_gpio_port[2]  = -EINVAL
};

static void __init omap4_ehci_ohci_init(void)
{
	omap_mux_init_signal("usbb2_ulpitll_clk.gpio_157", \
		OMAP_PIN_OUTPUT | \
		OMAP_PIN_OFF_NONE);

	/* Power on the ULPI PHY */
	if (gpio_is_valid(BLAZE_MDM_PWR_EN_GPIO)) {
		gpio_request(BLAZE_MDM_PWR_EN_GPIO, "USBB1 PHY VMDM_3V3");
		gpio_direction_output(BLAZE_MDM_PWR_EN_GPIO, 1);
	}

	usbhs_init(&usbhs_bdata);

	return;
}
#else
static void __init omap4_ehci_ohci_init(void){}
#endif

#if defined(CONFIG_TI_EMIF) || defined(CONFIG_TI_EMIF_MODULE)
static struct __devinitdata emif_custom_configs custom_configs = {
	.mask	= EMIF_CUSTOM_CONFIG_LPMODE,
	.lpmode	= EMIF_LP_MODE_SELF_REFRESH,
	.lpmode_timeout_performance = 512,
	.lpmode_timeout_power = 512,
	/* only at OPP100 should we use performance value */
	.lpmode_freq_threshold = 400000000,
};
#endif

static void set_osc_timings(void)
{
	/* Device Oscilator
	 * tstart = 2ms + 2ms = 4ms.
	 * tshut = Not defined in oscillator data sheet so setting to 1us
	 */
	omap_pm_setup_oscillator(4000, 1);
}

static struct omap_rprm_regulator omap4_sdp4430_rprm_regulators[] = {
	{
		.name = "cam2pwr",
	},
};

static void __init omap_4430sdp_init(void)
{
	int status;
	int package = OMAP_PACKAGE_CBS;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		package = OMAP_PACKAGE_CBL;

#if defined(CONFIG_TI_EMIF) || defined(CONFIG_TI_EMIF_MODULE)
	if (cpu_is_omap447x()) {
		omap_emif_set_device_details(1, &lpddr2_elpida_4G_S4_info,
				lpddr2_elpida_4G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
				&lpddr2_elpida_S4_min_tck, &custom_configs);
		omap_emif_set_device_details(2, &lpddr2_elpida_4G_S4_info,
				lpddr2_elpida_4G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
				&lpddr2_elpida_S4_min_tck, &custom_configs);
	} else {
		omap_emif_set_device_details(1, &lpddr2_elpida_2G_S4_x2_info,
				lpddr2_elpida_2G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
				&lpddr2_elpida_S4_min_tck, &custom_configs);
		omap_emif_set_device_details(2, &lpddr2_elpida_2G_S4_x2_info,
				lpddr2_elpida_2G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
				&lpddr2_elpida_S4_min_tck, &custom_configs);
	}
#endif

	omap4_mux_init(board_mux, NULL, package);
	omap_init_board_version(0);
	omap_create_board_props();

	set_osc_timings();
	omap4_i2c_init();
	blaze_sensor_init();
	blaze_touch_init();
	platform_add_devices(sdp4430_devices, ARRAY_SIZE(sdp4430_devices));
	omap4_board_serial_init();
	omap_sdrc_init(NULL, NULL);
	omap4_sdp4430_wifi_init();
	omap4_twl6030_hsmmc_init(mmc);

	omap4_ehci_ohci_init();
	usb_musb_init(&musb_board_data);

	omap4plus_connectivity_init();
	status = omap_ethernet_init();
	if (status) {
		pr_err("Ethernet initialization failed: %d\n", status);
	} else {
		sdp4430_spi_board_info[0].irq = gpio_to_irq(ETH_KS8851_IRQ);
		spi_register_board_info(sdp4430_spi_board_info,
				ARRAY_SIZE(sdp4430_spi_board_info));
	}

	status = omap4_keyboard_init(&sdp4430_keypad_data, &keypad_data);
	if (status)
		pr_err("Keypad initialization failed: %d\n", status);

	omap_init_dmm_tiler();
	omap4_register_ion();
	omap_4430sdp_display_init();
	blaze_keypad_init();

	init_duty_governor();
	omap_die_governor_register_pdata(&omap_gov_pdata);
	if (cpu_is_omap446x()) {
		/* Vsel0 = gpio, vsel1 = gnd */
		status = omap_tps6236x_board_setup(true, TPS62361_GPIO, -1,
					OMAP_PIN_OFF_OUTPUT_HIGH, -1);
		if (status)
			pr_err("TPS62361 initialization failed: %d\n", status);
	}

	omap_enable_smartreflex_on_init();
	if (cpu_is_omap446x())
		platform_device_register(&thermistor);

	omap_rprm_regulator_init(omap4_sdp4430_rprm_regulators,
				ARRAY_SIZE(omap4_sdp4430_rprm_regulators));
}

static void __init omap_4430sdp_reserve(void)
{
	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT,
			OMAP_RAM_CONSOLE_SIZE_DEFAULT);
	omap_rproc_reserve_cma(RPROC_CMA_OMAP4);
	omap4_ion_init();
	omap_reserve();
}

static void __init omap_4430sdp_init_early(void)
{
	omap4430_init_early();
	if (cpu_is_omap446x())
		omap_tps6236x_gpio_no_reset_wa(TPS62361_GPIO, -1, 32);
}


MACHINE_START(OMAP_4430SDP, "OMAP4 Blaze board")
	/* Maintainer: Santosh Shilimkar - Texas Instruments Inc */
	.atag_offset	= 0x100,
	.reserve	= omap_4430sdp_reserve,
	.map_io		= omap4_map_io,
	.init_early	= omap_4430sdp_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_4430sdp_init,
	.timer		= &omap4_timer,
	.restart	= omap_prcm_restart,
MACHINE_END
