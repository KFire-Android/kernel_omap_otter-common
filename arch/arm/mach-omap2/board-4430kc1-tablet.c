/*
 * Board support file for OMAP4430KC1 tablet.
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Author: Santosh Shilimkar <santosh.shilimkar@ti.com>
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * Based on mach-omap2/board-3430sdp.c
 * Based on mach-omap2/board-4430sdp.c
 * Based on mach-omap2/board-44xx-tablet.c
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
// #include <linux/hwspinlock.h>
#include <linux/i2c/twl.h>
#include <linux/leds-omap4430sdp-display.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/twl6040-vib.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/omapfb.h>
#include <linux/wl12xx.h>
#include <linux/skbuff.h>
#include <linux/reboot.h>
#include <linux/ti_wilink_st.h>
#include <linux/bootmem.h>
#include <plat/omap-serial.h>
#include <linux/memblock.h>
#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <mach/emif.h>
#include <mach/lpddr2-elpida.h>
#include <mach/dmm.h>
#include <mach/omap4_ion.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/board.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_apps_brd_id.h>
#include <plat/omap-serial.h>
#include <linux/wakelock.h>
#include <plat/remoteproc.h>
#include <video/omapdss.h>
#include <plat/vram.h>
#include <plat/omap-pm.h>

#include <plat/dmtimer.h>

#include "omap_ram_console.h"
#include "common-board-devices.h"
#include "mux.h"
#include "hsmmc.h"
#include "timer-gp.h"
#include "control.h"
#include "pm.h"
#include "prm-regbits-44xx.h"
#include "prm44xx.h"
#include "voltage.h"

#include "board-4430kc1-tablet.h"

#define WILINK_UART_DEV_NAME		"/dev/ttyO1"
#define BLUETOOTH_UART			(0x1)
#define CONSOLE_UART			(0x2)

//#define OMAP4_LCD_EN_GPIO		28

#define OMAP4_TOUCH_RESET_GPIO	18

#define OMAP4_TOUCH_IRQ_1		35
#define OMAP4_TOUCH_IRQ_2		36
#define OMAP_UART_GPIO_MUX_MODE_143	143

#define PANEL_IRQ			34


#define GPIO_WIFI_PMENA			54
#define GPIO_WIFI_IRQ			53

#define OMAP4SDP_MDM_PWR_EN_GPIO	157

#define MBID0_GPIO			174
#define MBID1_GPIO			173
#define MBID2_GPIO			178
#define MBID3_GPIO			177
#define PANELID0_GPIO			176
#define PANELID1_GPIO			175
#define TOUCHID0_GPIO			50
#define TOUCHID1_GPIO			51

#define TWL6030_RTC_GPIO		6

static struct wake_lock uart_lock;


static struct platform_device sdp4430_aic3110 = {
        .name           = "tlv320aic3110-codec",
        .id             = -1,
};

/* Panel Power */
static struct regulator_consumer_supply lcd_supply[] = {
//	REGULATOR_SUPPLY("vlcd", "otter1_panel_drv"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi2"),
//	{ .supply = "vdds_dsi", },
};

static struct regulator_init_data lcd_vinit = {
	.constraints = {
		.min_uV = 3300000,
		.max_uV = 3300000,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(lcd_supply),
	.consumer_supplies = lcd_supply,
};

static struct fixed_voltage_config lcd_reg_data = {
	.supply_name = "vdd_lcd",
	.microvolts = 3300000,
	.gpio = 47,
	.enable_high = 1,
	.enabled_at_boot = 0,
	.init_data = &lcd_vinit,
};

static struct platform_device lcd_regulator_device = {
	.name   = "reg-fixed-voltage",
	.id     = -1,
	.dev    = {
		.platform_data = &lcd_reg_data,
	},
};

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
}


static struct platform_device __initdata *sdp4430_devices[] = {
     &sdp4430_aic3110,
     &lcd_regulator_device,
};

static struct omap_board_config_kernel __initdata sdp4430_config[] = {
};

static void __init omap_4430sdp_init_early(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
#ifdef CONFIG_OMAP_32K_TIMER
	omap2_gp_clockevent_set_gptimer(1);
#endif
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

static int sdp4430_wifi_power_state;

static int wifi_set_power(struct device *dev, int slot, int power_on, int vdd)
{
	printk(KERN_WARNING"%s: %d\n", __func__, power_on);

	gpio_set_value(GPIO_WIFI_PMENA, power_on);

	sdp4430_wifi_power_state = power_on;

	return 0;
}

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc = 2,
		.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR,
		.gpio_cd = -EINVAL,
		.gpio_wp = -EINVAL,
		.ocr_mask = MMC_VDD_165_195,
		.nonremovable = true,
#ifdef CONFIG_PM_RUNTIME
		.power_saving	= true,
#endif
	},
	{
		.mmc = 1,
		.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR,
		.gpio_wp = -EINVAL,
#ifdef CONFIG_PM_RUNTIME
		.power_saving	= true,
#endif
	},
#if 0
	{
		.mmc = 5,
		.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA,
		.gpio_cd = -EINVAL,
		.gpio_wp = 4,
		.ocr_mask = MMC_VDD_165_195,
		.nonremovable = true,
	},
#endif
	{
		.mmc = 5,
		.caps = MMC_CAP_4_BIT_DATA | MMC_CAP_POWER_OFF_CARD,
		.gpio_cd = -EINVAL,
		.gpio_wp = -EINVAL,
		.ocr_mask = MMC_VDD_165_195,
		.nonremovable = true,
	},
	{}	/* Terminator */
};

#if 0
static struct wl12xx_platform_data __initdata omap4_kc1_wlan_data = {
	.irq = OMAP_GPIO_IRQ(GPIO_WIFI_IRQ),
	.board_ref_clock = WL12XX_REFCLOCK_26,
	.board_tcxo_clock = 0,
};
#endif
static struct wl12xx_platform_data __initdata omap4_kc1_wlan_data = {
	.irq = OMAP_GPIO_IRQ(GPIO_WIFI_IRQ),
	.board_ref_clock = WL12XX_REFCLOCK_38,
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
	.id		= 1,
	.dev = { .platform_data = &sdp4430_vwlan, }
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

	/* Set the MMC5 (wlan) power function */
	if (pdev->id == 4)
		pdata->slots[0].set_power = wifi_set_power;

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

/***** I2C BOARD INIT ****/

static struct i2c_board_info __initdata sdp4430_i2c_boardinfo_dvt[] = {
#ifdef CONFIG_BATTERY_BQ27541_Q
	{
		I2C_BOARD_INFO("bq27541", 0x55),
	},
#endif
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
};

static struct i2c_board_info __initdata sdp4430_i2c_2_boardinfo[] = {
	{
#ifdef CONFIG_TOUCHSCREEN_ILITEK
		I2C_BOARD_INFO("ilitek_i2c", 0x41),
		 .irq = OMAP_GPIO_IRQ(OMAP4_TOUCH_IRQ_1),
#endif
	},
};

static struct i2c_board_info __initdata sdp4430_i2c_3_boardinfo[] = {
#ifdef CONFIG_DEMO_HDMI
	{
		I2C_BOARD_INFO("adi7526", 0x39),
		.irq = OMAP_GPIO_IRQ(OMAP4_ADI7526_IRQ),
	},
#endif
};

static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo[] = {
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("tmp105", 0x49), },
#endif
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18), },
};

static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo_c1c[] = {
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("lm75", 0x48), },
#endif
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18), },
};

static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo_dvt[] = {
#ifdef CONFIG_SUMMIT_SMB347_Q
	{ I2C_BOARD_INFO("summit_smb347", 0x6), .irq = OMAP_GPIO_IRQ(OMAP4_CHARGER_IRQ), },
#endif    
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("tmp105", 0x49), },
#endif
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18), },
};

static struct i2c_board_info __initdata sdp4430_i2c_4_boardinfo_pvt[] = {
#ifdef CONFIG_SUMMIT_SMB347_Q
	{ I2C_BOARD_INFO("summit_smb347", 0x6), .irq = OMAP_GPIO_IRQ(OMAP4_CHARGER_IRQ), },
#endif    
/* Added for STK ALS*/
	{ I2C_BOARD_INFO("stk_als22x7_addr1", 0x20>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr2", 0x22>>1), },
	{ I2C_BOARD_INFO("stk_als22x7_addr3", 0x70>>1), },
// for gsensor	
	{ I2C_BOARD_INFO("bma250", 0x18), },
//add for Temp-sensor
#ifdef CONFIG_SENSORS_LM75
	{ I2C_BOARD_INFO("tmp105", 0x48), },
#endif
};

static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_2_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_3_bus_pdata;
static struct omap_i2c_bus_board_data __initdata sdp4430_i2c_4_bus_pdata;

static void __init omap_i2c_hwspinlock_init(int bus_id, unsigned int spinlock_id,
				struct omap_i2c_bus_board_data *pdata)
{
	/* spinlock_id should be -1 for a generic lock request */
	if (spinlock_id < 0)
		pdata->handle = hwspin_lock_request();
	else
		pdata->handle = hwspin_lock_request_specific(spinlock_id);

	if (pdata->handle != NULL) {
		pdata->hwspin_lock_timeout = hwspin_lock_timeout;
		pdata->hwspin_unlock = hwspin_unlock;
	} else {
		pr_err("I2C hwspinlock request failed for bus %d\n", bus_id);
	}
}

static void otter_set_osc_timings(void)
{
	/* Device Oscilator
	 * tstart = 2ms + 2ms = 4ms.
	 * tshut = Not defined in oscillator data sheet so setting to 1us
	 */
	omap_pm_set_osc_lp_time(4000, 1);
}

static int __init omap4_i2c_init(void)
{
	int err;

	omap_i2c_hwspinlock_init(1, 0, &sdp4430_i2c_bus_pdata);
	omap_i2c_hwspinlock_init(2, 1, &sdp4430_i2c_2_bus_pdata);
	omap_i2c_hwspinlock_init(3, 2, &sdp4430_i2c_3_bus_pdata);
	omap_i2c_hwspinlock_init(4, 3, &sdp4430_i2c_4_bus_pdata);

	omap_register_i2c_bus_board_data(1, &sdp4430_i2c_bus_pdata);
	omap_register_i2c_bus_board_data(2, &sdp4430_i2c_2_bus_pdata);
	omap_register_i2c_bus_board_data(3, &sdp4430_i2c_3_bus_pdata);
	omap_register_i2c_bus_board_data(4, &sdp4430_i2c_4_bus_pdata);

	omap4_power_init();

	/*
	 * Phoenix Audio IC needs I2C1 to
	 * start with 400 KHz or less
	 */
        if (quanta_mbid < 0x04) {
		err = i2c_register_board_info(1, sdp4430_i2c_boardinfo, ARRAY_SIZE(sdp4430_i2c_boardinfo));
        }
	// DVT
        else {
		err = i2c_register_board_info(1, sdp4430_i2c_boardinfo_dvt, ARRAY_SIZE(sdp4430_i2c_boardinfo_dvt));
        }
	if (err)
	  return err;

	omap_register_i2c_bus(2, 400, sdp4430_i2c_2_boardinfo, ARRAY_SIZE(sdp4430_i2c_2_boardinfo));
	omap_register_i2c_bus(3, 400, sdp4430_i2c_3_boardinfo, ARRAY_SIZE(sdp4430_i2c_3_boardinfo));
	if (quanta_mbid<0x02) {
		omap_register_i2c_bus(4, 400, sdp4430_i2c_4_boardinfo_c1c, ARRAY_SIZE(sdp4430_i2c_4_boardinfo_c1c));
	}
	else if (quanta_mbid<0x04) {
		omap_register_i2c_bus(4, 400, sdp4430_i2c_4_boardinfo, ARRAY_SIZE(sdp4430_i2c_4_boardinfo));
	}
	// DVT
	else if (quanta_mbid<0x06) {
		pr_info("quanta_mbid < 0x06\n");
		omap_register_i2c_bus(4, 400, sdp4430_i2c_4_boardinfo_dvt, ARRAY_SIZE(sdp4430_i2c_4_boardinfo_dvt));
	}
	// PVT
	else {
		pr_info("quanta_mbid >= 0x06\n");
		omap_register_i2c_bus(4, 400, sdp4430_i2c_4_boardinfo_pvt, ARRAY_SIZE(sdp4430_i2c_4_boardinfo_pvt));
	}

	regulator_has_full_constraints();

	return 0;
}

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

static bool enable_suspend_off = true;
module_param(enable_suspend_off, bool, S_IRUSR | S_IRGRP | S_IROTH);

/******** END I2C BOARD INIT ********/

#if defined(CONFIG_TOUCHSCREEN_ILITEK)
static void omap_ilitek_init(void)
{
	//printk("~~~~~~~~%s\n", __func__);
	omap_mux_init_signal("dpm_emu7.gpio_18", OMAP_PIN_OUTPUT | OMAP_PIN_OFF_NONE);
        
        if (gpio_request(OMAP4_TOUCH_RESET_GPIO , "ilitek_reset_gpio_18") <0 ){
		pr_err("Touch IRQ reset request failed\n");
                return;
	}
        gpio_direction_output(OMAP4_TOUCH_RESET_GPIO, 0);
        gpio_set_value(OMAP4_TOUCH_RESET_GPIO, 1);

	if (gpio_request(OMAP4_TOUCH_IRQ_1, "Touch IRQ") < 0) {
		pr_err("Touch IRQ GPIO request failed\n");
		return;
	}
	gpio_direction_input(OMAP4_TOUCH_IRQ_1);

}
#endif //CONFIG_TOUCHSCREEN_ILITEK


#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux __initdata board_mux[] = {
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

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
static struct emif_device_details __initdata emif_devices = {
	.cs0_device = &lpddr2_elpida_2G_S4_dev,
};

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

static void omap4_kc1_wifi_mux_init(void)
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

static void __init omap4_kc1_wifi_init(void)
{
	omap4_kc1_wifi_mux_init();
	if (wl12xx_set_platform_data(&omap4_kc1_wlan_data))
		pr_err("Error setting wl12xx data\n");
	platform_device_register(&omap_vwlan_device);
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
	if (gpio_is_valid(OMAP4SDP_MDM_PWR_EN_GPIO)) {
		gpio_request(OMAP4SDP_MDM_PWR_EN_GPIO, "USBB1 PHY VMDM_3V3");
		gpio_direction_output(OMAP4SDP_MDM_PWR_EN_GPIO, 1);
	}

	usbhs_init(&usbhs_bdata);

	return;

}
#else
static void __init omap4_ehci_ohci_init(void){}
#endif

static int kc1_notifier_call(struct notifier_block *this,
					unsigned long code, void *cmd)
{
	void __iomem *sar_base;
	u32 v = OMAP4430_RST_GLOBAL_COLD_SW_MASK;

	sar_base = omap4_get_sar_ram_base();

	if (!sar_base)
		return notifier_from_errno(-ENOMEM);

	if ((code == SYS_RESTART) && (cmd != NULL)) {
		/* cmd != null; case: warm boot */
		if (!strcmp(cmd, "bootloader")) {
			/* Save reboot mode in scratch memory */
			strcpy(sar_base + 0xA0C, cmd);
			v |= OMAP4430_RST_GLOBAL_WARM_SW_MASK;
		} else if (!strcmp(cmd, "recovery")) {
			/* Save reboot mode in scratch memory */
			strcpy(sar_base + 0xA0C, cmd);
			v |= OMAP4430_RST_GLOBAL_WARM_SW_MASK;
		} else {
			v |= OMAP4430_RST_GLOBAL_COLD_SW_MASK;
		}
	}

	omap4_prm_write_inst_reg(0xfff, OMAP4430_PRM_DEVICE_INST,
			OMAP4_RM_RSTST);
	omap4_prm_write_inst_reg(v, OMAP4430_PRM_DEVICE_INST, OMAP4_RM_RSTCTRL);
	v = omap4_prm_read_inst_reg(WKUP_MOD, OMAP4_RM_RSTCTRL);

	return NOTIFY_DONE;
}

static struct notifier_block kc1_reboot_notifier = {
	.notifier_call = kc1_notifier_call,
};

static void __init omap_kc1_init(void)
{
	int package = OMAP_PACKAGE_CBS;

	quanta_boardids();
	if (omap_rev() == OMAP4430_REV_ES1_0)
		package = OMAP_PACKAGE_CBL;

	omap4_mux_init(board_mux, NULL, package);

	omap_emif_setup_device_details(&emif_devices, &emif_devices);

	omap_board_config = sdp4430_config;
	omap_board_config_size = ARRAY_SIZE(sdp4430_config);

	omap_init_board_version(0);

	omap4_create_board_props();
//	register_reboot_notifier(&kc1_reboot_notifier);

	otter_set_osc_timings();
	omap4_i2c_init();
	enable_rtc_gpio();
	omap_dmm_init();

	omap4_display_init();

	omap4_register_ion();
	platform_add_devices(sdp4430_devices, ARRAY_SIZE(sdp4430_devices));

	gpio_request(0, "sysok");

//	wake_lock_init(&uart_lock, WAKE_LOCK_SUSPEND, "uart_wake_lock");
//	omap_serial_init();

	omap4_twl6030_hsmmc_init(mmc);
	gpio_request(42, "OMAP_GPIO_ADC");
	gpio_direction_output(42, 0);

	/*For smb347*/
	//Enable charger interrupt wakeup function.
	omap_mux_init_signal("fref_clk4_req.gpio_wk7", \
		OMAP_PIN_INPUT_PULLUP|OMAP_PIN_OFF_WAKEUPENABLE|OMAP_PIN_OFF_INPUT_PULLUP);

	if(quanta_mbid>=4) {
		//EN pin
		omap_mux_init_signal("c2c_data12.gpio_101", OMAP_PIN_OUTPUT |OMAP_PIN_OFF_OUTPUT_LOW);
		//SUSP
		omap_mux_init_signal("uart4_rx.gpio_155", OMAP_PIN_OUTPUT |OMAP_PIN_OFF_OUTPUT_HIGH);
		gpio_request(101, "CHARGE-en");
		gpio_direction_output(101, 0);
		gpio_request(155, "CHARGE-SUSP");
		gpio_direction_output(155, 1);
	}
	omap4_kc1_wifi_init();

	omap4_ehci_ohci_init();
	usb_musb_init(&musb_board_data);

	omap4_kc1_display_init();
#if defined(CONFIG_TOUCHSCREEN_ILITEK)
	omap_ilitek_init();
#endif //CONFIG_TOUCHSCREEN_ILITEK

	gpio_request(119, "ADO_SPK_ENABLE");
	gpio_direction_output(119, 1);
	gpio_set_value(119, 1);
	gpio_request(120, "SKIPB_GPIO");
	gpio_direction_output(120, 1);
	gpio_set_value(120, 1);

	// Qunata_diagnostic 20110506 set GPIO 171 172 to be input
	omap_writew(omap_readw(0x4a10017C) | 0x011B, 0x4a10017C); // KPD_COL3
	omap_writew(omap_readw(0x4a10017C) & ~0x04, 0x4a10017C);
	 
	omap_writew(omap_readw(0x4a10017C) | 0x011B, 0x4a10017E); // KPD_COL4
	omap_writew(omap_readw(0x4a10017C) & ~0x04, 0x4a10017E);
	////////////////////////////////////////////

	omap_enable_smartreflex_on_init();
	if (enable_suspend_off)
		omap_pm_enable_off_mode();
}

static void __init omap_4430sdp_map_io(void)
{
	omap2_set_globals_443x();
	omap44xx_map_common_io();
}

static void __init omap_4430sdp_reserve(void)
{
	omap_init_ram_size();

#ifdef CONFIG_ION_OMAP
	omap4_kc1_android_display_setup(get_omap_ion_platform_data());
	omap_ion_init();
#else
	omap4_kc1_android_display_setup(NULL);
#endif
	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT, OMAP_RAM_CONSOLE_SIZE_DEFAULT);

	/* do the static reservations first */
	memblock_remove(PHYS_ADDR_SMC_MEM, PHYS_ADDR_SMC_SIZE);
	memblock_remove(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE);
	/* ipu needs to recognize secure input buffer area as well */
	omap_ipu_set_static_mempool(PHYS_ADDR_DUCATI_MEM, PHYS_ADDR_DUCATI_SIZE + OMAP4_ION_HEAP_SECURE_INPUT_SIZE);
#ifdef CONFIG_OMAP_REMOTE_PROC_DSP
	memblock_remove(PHYS_ADDR_TESLA_MEM, PHYS_ADDR_TESLA_SIZE);
	omap_dsp_set_static_mempool(PHYS_ADDR_TESLA_MEM, PHYS_ADDR_TESLA_SIZE);
#endif

	omap_reserve();
}

MACHINE_START(OMAP_4430SDP, "OMAP4430")
	.boot_params	= 0x80000100,
	.reserve	= omap_4430sdp_reserve,
	.map_io		= omap_4430sdp_map_io,
	.init_early	= omap_4430sdp_init_early,
	.init_irq	= gic_init_irq,
	.init_machine	= omap_kc1_init,
	.timer		= &omap_timer,
MACHINE_END
