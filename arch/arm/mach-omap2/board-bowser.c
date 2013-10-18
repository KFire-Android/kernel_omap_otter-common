/*
 * Board support file for OMAP44XX Tablet.
 *
 * Copyright (C) 2009 - 2012 Texas Instruments
 *
 * Authors:
 *	 Dan Murphy <dmurphy@ti.com>
 *	 Volodymyr Riazantsev <v.riazantsev@ti.com>
 *	 Aneesh V <aneesh@ti.com>
 *
 * Based on mach-omap2/board-4430sdp.c
 *   by Santosh Shilimkar <santosh.shilimkar@ti.com>
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
#include <linux/hwspinlock.h>
#include <linux/i2c/twl.h>
#include <linux/cdc_tcxo.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/platform_data/omap-abe-twl6040.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/mmc.h>

#include <mach/hardware.h>

#include <plat/drm.h>
#include <plat/board.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/remoteproc.h>
#include <plat/omap_apps_brd_id.h>
#if defined(CONFIG_TOUCHSCREEN_CYPRESS_TTSP)
    #include <linux/input/touch_platform.h>
#endif

#include <sound/wm8962.h>
#include <sound/max98090.h>
#include <sound/max97236.h>

#ifdef CONFIG_MPU_SENSORS_MPU6050B1
#include <linux/mpu.h>
#define GPIO_GRYO               4
#endif
#ifdef CONFIG_INPUT_BU52061_HALLSENSOR
#define HALL_EFFECT 0  // Hall sensor output pin -- gpio_wk0
#endif

#include "mux.h"
#include "hsmmc.h"
#include "common.h"
#include "control.h"
#include "common-board-devices.h"
#include "omap4_ion.h"
#include "omap_ram_console.h"
#include "board-bowser.h"
#include "pm.h"


#define GPIO_CODEC_IRQ	26

#if defined(CONFIG_SND_SOC_MAX97236)
#define MAX97236_GPIO_IRQ	171

struct max97236_pdata max97236_pdata = {
	.irq_gpio = MAX97236_GPIO_IRQ,
};
#endif

#if defined(CONFIG_SND_SOC_WM8962)
static struct wm8962_pdata wm8962_pdata = {
	.gpio_init = {
		[1] = WM8962_GPIO_FN_IRQ,              /* GPIO2 is IRQ */
		[4] = 0x8000 | WM8962_GPIO_FN_DMICDAT, /* GPIO5 DMICDAT */
		[5] = WM8962_GPIO_FN_DMICCLK,          /* GPIO6 DMICCLK */
	},
	.mic_cfg = 1, /*MICBIAS high, threasholds minimum*/
};
#endif

#if defined(CONFIG_SND_SOC_MAX98090)
struct max98090_pdata max98090_pdata;

static struct platform_device bowser_maxim_audio = {
	.name		= "bowser-maxim", // matches platform driver name
	.id		= -1,
	.dev = {
		.platform_data = &max98090_pdata,
	},
};

#endif

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
	.mode			= MUSB_OTG,
	/* TODO: verify this power value */
	.power			= 100,
};

extern struct mmc_platform_data bowser_wifi_data;

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 2,
		.caps		=  MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.nonremovable   = true,
		.ocr_mask	= MMC_VDD_29_30,
		.no_off_init	= true,
	},
	{
		.name           = "bcmdhd_wlan",
		.mmc		= 5,
		.caps		= MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA, // | MMC_CAP_1_8V_DDR,
		.pm_caps	= MMC_PM_KEEP_POWER,
		.gpio_cd	= -EINVAL,
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_165_195 | MMC_VDD_20_21,
		.nonremovable	= true,
		.built_in   = 1,
		.mmc_data   = &bowser_wifi_data,
	},
	{}	/* Terminator */
};

static struct regulator_consumer_supply sdp4430_vcxio_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

/*
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
	// MCLK input is 38.4MHz
	.mclk_freq	= 38400000,
};
*/

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
	pdata->init = omap4_twl6030_hsmmc_late_init;
}

static int __init omap4_twl6030_hsmmc_init(struct omap2_hsmmc_info *controllers)
{
	struct omap2_hsmmc_info *c;

	omap_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(&c->pdev->dev);

	return 0;
}

static struct regulator_init_data sdp4430_vcxio = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask  = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on      = true,
	},
	.num_consumer_supplies  = ARRAY_SIZE(sdp4430_vcxio_supply),
	.consumer_supplies      = sdp4430_vcxio_supply,
};

//static struct twl4030_platform_data bowser_twldata;
static struct twl4030_platform_data bowser_twldata = {
	/* TWL6030 regulators at OMAP443X/446X based SOMs */
	.vcxio          = &sdp4430_vcxio,
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

static struct i2c_board_info __initdata bowser_i2c_2_boardinfo[] = {
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_TTSP
	{
		I2C_BOARD_INFO(CY_I2C_NAME, CY_I2C_TCH_ADR),
		.platform_data = &cyttsp4_i2c_touch_platform_data,
		.irq = GPIO_TOUCH_IRQ,
	},
#endif
};

static struct i2c_board_info __initdata bowser_i2c_3_boardinfo[] = {
#if defined(CONFIG_BATTERY_BQ27541)
	{
		I2C_BOARD_INFO("bq27541", 0x55),
	},
#endif
#if defined(CONFIG_SND_SOC_WM8962)
	{
		I2C_BOARD_INFO("wm8962", 0x1a),
		.platform_data = &wm8962_pdata,
		.irq = GPIO_CODEC_IRQ,
	},
#endif
#if defined(CONFIG_SND_SOC_MAX98090)
	{
		I2C_BOARD_INFO("max98090", 0x10),
		.platform_data = &max98090_pdata,
	},
#endif
#if defined(CONFIG_SND_SOC_MAX97236)
	{
		I2C_BOARD_INFO("max97236", 0x40),
		.platform_data = &max97236_pdata,
	},
#endif
};

static struct i2c_board_info __initdata bowser_i2c_4_boardinfo[] = {

#ifdef CONFIG_INPUT_PIC12LF1822_PROXIMITY
	{
		I2C_BOARD_INFO(PIC12LF1822_PROXIMITY_NAME, 0x0d),
//		.platform_data = &pic12lf1822_proximity_pdata,
		.irq = GPIO_PIC12LF1822_PROXIMITY_IRQ, /* GPIO85 */
	},
#endif

#ifdef CONFIG_MPU_SENSORS_MPU6050B1
	{
		I2C_BOARD_INFO("mpu6050", 0x68),
		.irq = GPIO_GRYO,
		.platform_data = &mpu_gyro_data,
	},
	{
		I2C_BOARD_INFO("ami306", 0x0F),
		.irq = GPIO_GRYO,
		.platform_data = &mpu_compass_data,
	},
#endif

#ifdef CONFIG_INPUT_MAX44007
	{
		I2C_BOARD_INFO(MAX44007_NAME, 0x5A),
		.platform_data = &max44007_pdata,
		.irq = GPIO_MAX44007_IRQ,    /* GPIO36 */
	},
#endif

#ifdef CONFIG_MAX1161X
	/* Maxim MAX11613 ADC */
	{
		I2C_BOARD_INFO("max11613", 0x34),
	},
#endif
};

struct bowser_i2c_info_t {
	struct i2c_board_info* board_info;
	struct omap_i2c_bus_board_data bus_board_data;
	uint16_t speed;
	uint8_t  board_info_size;
	uint8_t  bus_id;
};
static struct bowser_i2c_info_t __initdata bowser_i2c_info[] = {
	{
		.bus_id = 1,
	},
	{
		.bus_id = 2,
		.speed = 400,
		.board_info = &bowser_i2c_2_boardinfo[0],
		.board_info_size = ARRAY_SIZE(bowser_i2c_2_boardinfo),
	},
	{
		.bus_id = 3,
		.speed = 400,
		.board_info = &bowser_i2c_3_boardinfo[0],
		.board_info_size = ARRAY_SIZE(bowser_i2c_3_boardinfo),
	},
	{
		.bus_id = 4,
		.speed = 400,
		.board_info = &bowser_i2c_4_boardinfo[0],
		.board_info_size = ARRAY_SIZE(bowser_i2c_4_boardinfo),
	}
};

static int __init omap4_i2c_init(void)
{
	int i;
	struct bowser_i2c_info_t *inf;

	for(i=0,inf = &bowser_i2c_info[0]; i < ARRAY_SIZE(bowser_i2c_info); ++i, ++inf)
	{
		printk("Configuring I2C HW spinlocks: bus=%d\n", inf->bus_id);
		omap_i2c_hwspinlock_init(inf->bus_id, inf->bus_id-1, &inf->bus_board_data);
		omap_register_i2c_bus_board_data(inf->bus_id, &inf->bus_board_data);
	}

	omap4_pmic_get_config(&bowser_twldata, TWL_COMMON_PDATA_USB |
			TWL_COMMON_PDATA_MADC,
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
	omap4_pmic_init("twl6030", &bowser_twldata,
			NULL, OMAP44XX_IRQ_SYS_2N);

	for(i=0, inf = &bowser_i2c_info[0]; i < ARRAY_SIZE(bowser_i2c_info); ++i, ++inf)
	{
		int j;
		struct i2c_board_info *bi;

		for(j=0, bi = &inf->board_info[0]; j < inf->board_info_size; ++j, ++bi) {
			if(bi->irq != 0)
				bi->irq = gpio_to_irq(bi->irq);
		}
		if(!inf->speed) continue;
		printk("Registering I2C devices: bus=%d; speed=%d\n", inf->bus_id, inf->speed);
		omap_register_i2c_bus(inf->bus_id, inf->speed, inf->board_info, inf->board_info_size);
	}

	return 0;
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP4_MUX(USBB2_ULPITLL_CLK, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT), // Mux
	OMAP4_MUX(SYS_NIRQ2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
				OMAP_PIN_OFF_WAKEUPENABLE),
#ifdef CONFIG_INPUT_PIC12LF1822_PROXIMITY
	/* PROXIMITY IRQ - GPIO 85 */
	OMAP4_MUX(USBB1_ULPITLL_STP, OMAP_MUX_MODE3 | OMAP_PIN_INPUT),
#endif
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT)  || defined(CONFIG_TOUCHSCREEN_ATMEL_MXT_MODULE)
	/* TOUCH IRQ - GPIO 24 */
	OMAP4_MUX(DPM_EMU13, OMAP_MUX_MODE3 | OMAP_PIN_INPUT),
#endif
	// ey: since bowser 7 is using the NALA board which is same as Tate, PWM from the panel
	// so, disable the following line to use the Tate code
	//	OMAP4_MUX(DPM_EMU18, OMAP_MUX_MODE1 | OMAP_PULL_ENA),/* LCDBKL_PWM, GPTimer 10 */
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

#else
#define board_mux	NULL
#endif

static void __init lpddr2_init(void)
{
	int sd_vendor = omap_sdram_vendor();

	if ((sd_vendor != SAMSUNG_SDRAM) && (sd_vendor != ELPIDA_SDRAM)
			&& (sd_vendor != HYNIX_SDRAM)) {
		pr_err("%s: DDR vendor_id %d is not supported!\n",
				__func__, sd_vendor);
		return;
	}

	if ((omap_sdram_density() & SDRAM_DENSITY_MASK) == SDRAM_DENSITY_2CS) {
		omap_emif_set_device_details(1, &lpddr2_elpida_2G_S4_x2_info,
				lpddr2_elpida_2G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
		omap_emif_set_device_details(2, &lpddr2_elpida_2G_S4_x2_info,
				lpddr2_elpida_2G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_2G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
		pr_info("%s: 2cs ddr detected\n", __func__);
	} else {
		omap_emif_set_device_details(1, &lpddr2_elpida_4G_S4_info,
				lpddr2_elpida_4G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
		omap_emif_set_device_details(2, &lpddr2_elpida_4G_S4_info,
				lpddr2_elpida_4G_S4_timings,
				ARRAY_SIZE(lpddr2_elpida_4G_S4_timings),
				&lpddr2_elpida_S4_min_tck, NULL);
		pr_info("%s: 1cs ddr detected\n", __func__);
	}
}

static struct platform_device *bowser_devices[] __initdata = {
#ifdef CONFIG_SND_SOC_MAX98090
	&bowser_maxim_audio,
#endif
};

static void bowser_platform_init(void)
{
#ifdef CONFIG_SND_SOC_MAX98090
	max98090_pdata.irq = gpio_to_irq(GPIO_CODEC_IRQ);
#endif

	/*
	 * Drive MSECURE high for TWL6030/6032 write access.
	 */
	omap_mux_init_gpio(MAX97236_GPIO_IRQ, OMAP_PIN_INPUT_PULLUP);

	omap_mux_init_signal("fref_clk3_req.gpio_wk30", OMAP_PIN_OUTPUT);
	gpio_request(30, "msecure");
	gpio_direction_output(30, 1);

}

int __init bowser_bt_init(void);
int __init bowser_wifi_init(void);

static void __init omap_bowser_init(void)
{
	int package = OMAP_PACKAGE_CBS;

	lpddr2_init();

	omap4_mux_init(board_mux, NULL, package);
	omap_create_board_props();
#if defined(CONFIG_TOUCHSCREEN_CYPRESS_TTSP)
	touch_gpio_init();
#endif

	/*
	 * Any additional changes to platform data structures before they get
	 * registered.
	 */
	bowser_platform_init();
	omap4_i2c_init();

	printk("%s: [AP]: Registering platform devices\n", __func__);
	platform_add_devices(&bowser_devices[0], ARRAY_SIZE(bowser_devices));

	omap4_board_serial_init();

	omap_sdrc_init(NULL, NULL);
	omap4_twl6030_hsmmc_init(mmc);
	bowser_bt_init();
	bowser_wifi_init();

	usb_musb_init(&musb_board_data);

	omap_init_dmm_tiler();
	bowser_android_display_setup(NULL);
	omap4_register_ion();

	bowser_panel_init();

	omap_enable_smartreflex_on_init();

	printk("\nBoard Init Over\n\n");
}

static void __init omap_bowser_reserve(void)
{
	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT,
			OMAP_RAM_CONSOLE_SIZE_DEFAULT);
	omap_rproc_reserve_cma(RPROC_CMA_OMAP4);
	omap4_ion_init();
	omap_reserve();
}

MACHINE_START(OMAP4_BOWSER, "OMAP4 Bowser board")
	.atag_offset	= 0x100,
	.reserve	= omap_bowser_reserve,
	.map_io		= omap4_map_io,
	.init_early	= omap4430_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine	= omap_bowser_init,
	.timer		= &omap4_timer,
	.restart	= omap_prcm_restart,
MACHINE_END
