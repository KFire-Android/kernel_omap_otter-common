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
#include <linux/platform_data/case_temperature_sensor.h>
#include <linux/thermal_framework.h>
#include <linux/omap_die_governor.h>
#include <linux/gpio_keys.h>
#include <linux/i2c/tmp103.h>
#include <linux/dev_info.h>

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <mach/hardware.h>
#include <mach/omap-secure.h>

#include <plat/drm.h>
#include <plat/board.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/remoteproc.h>
#include <plat/omap_apps_brd_id.h>

#ifdef CONFIG_SND_SOC_MAX98090
#include <sound/max98090.h>
#endif

#if defined(CONFIG_SND_SOC_MAX97236)
#include <sound/max97236.h>
#endif

#ifdef CONFIG_INPUT_BU52061_HALLSENSOR
#include <linux/input/bu52061.h>
#endif

#include "mux.h"
#include "hsmmc.h"
#include "common.h"
#include "control.h"
#include "common-board-devices.h"
#include "omap4_ion.h"
#include "omap_ram_console.h"
#include "board-bowser.h"
#include "board-bowser-idme.h"
#include "pm.h"

#ifdef CONFIG_SND_SOC_MAX98090
#define GPIO_CODEC_IRQ		26
#endif

#if defined(CONFIG_SND_SOC_MAX97236)
#define MAX97236_GPIO_IRQ	171
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
#include <linux/input/synaptics_dsx.h>
#define TM_TYPE1        (1)     // 2D only
#define TM_TYPE2        (2)     // 2D + 0D x 2
#define TM_TYPE3        (3)     // 2D + 0D x 4
#define GPIO_TOUCH_RESET 23
#define GPIO_TOUCH_IRQ   24
#endif

#ifdef CONFIG_INV_MPU_IIO
#include <linux/mpu.h>
#include <linux/irq.h>
#define GPIO_GRYO               4
#endif

#define GPIO_HALL_EFFECT       0

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
#if defined(CONFIG_SND_SOC_MAX97236)
struct max97236_pdata max97236_pdata = {
	.irq_gpio = MAX97236_GPIO_IRQ,
};
#endif

#ifdef CONFIG_CHARGER_SMB347
    #include <linux/power/smb347.h>
#endif

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_UTMI,
	.mode			= MUSB_PERIPHERAL,
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


/* GPIO_KEY for Bowser */
/* Config VolumeUp : GPIO_WK1 , VolumeDown : GPIO_50 */
static struct gpio_keys_button bowser_gpio_keys_buttons[] = {
	[0] = {
		.code			= KEY_VOLUMEUP,
		.gpio			= 1,
		.desc			= "SW1",
		.active_low		= 1,
		.wakeup			= 0,
		.debounce_interval	= 5,
	},
	[1] = {
		.code			= KEY_VOLUMEDOWN,
		.gpio			= 50,
		.desc			= "SW3",
		.active_low		= 1,
		.wakeup			= 0,
		.debounce_interval	= 0,
		},
	};

static struct gpio_keys_platform_data bowser_gpio_keys = {
	.buttons		= bowser_gpio_keys_buttons,
	.nbuttons		= ARRAY_SIZE(bowser_gpio_keys_buttons),
	.rep			= 1,
};

static struct platform_device bowser_gpio_keys_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.dev		= {
		.platform_data	= &bowser_gpio_keys,
	},
};

static struct case_temp_sensor_pdata case_sensor_pdata = {
	.source_domain = "pcb_case",
	.average_number = 20,
	.report_delay_ms = 250,
};

static struct platform_device bowser_case_sensor_device = {
	.name	= "case_temp_sensor",
	.id	= -1,
	.dev	= {
		.platform_data = &case_sensor_pdata,
	},
};

/* Initial set of thresholds for different thermal zones */
static struct omap_thermal_zone thermal_zones[] = {
	OMAP_THERMAL_ZONE("safe", 0, 25000, 85000, 250, 1000, 400),
	OMAP_THERMAL_ZONE("monitor", 0, 80000, 95000, 250, 250, 250),
	OMAP_THERMAL_ZONE("alert", 0, 90000, 105000, 250, 250, 150),
	OMAP_THERMAL_ZONE("critical", 1, 100000, 125000, 250, 250, 50),
};

static struct omap_die_governor_pdata omap_gov_pdata = {
	.zones = thermal_zones,
	.zones_num = ARRAY_SIZE(thermal_zones),
};
static struct case_policy soho_pre_evt2_pollicy = {
	.sys_threshold_hot = 78000,
	.sys_threshold_cold = 74000,
};

static struct case_policy soho_evt2_pollicy = {
	.sys_threshold_hot = 47000,
	.sys_threshold_cold = 44000,
	.case_subzone_number = 3,
};

static void __init soho_thermal_init(void)
{
	int board_id = idme_get_board_revision();

	if (board_id < 4)
		set_case_policy(&soho_pre_evt2_pollicy);
	else
		set_case_policy(&soho_evt2_pollicy);

	omap_die_governor_register_pdata(&omap_gov_pdata);
}

#ifdef CONFIG_INPUT_BU52061_HALLSENSOR
static struct bu52061_platform_data bu52061_data = {
	.gpio = GPIO_HALL_EFFECT,
};

static struct platform_device bu52061_platform_device = {
	.name	= "bu52061",
	.id	= -1,
	.dev	= {
		.platform_data = &bu52061_data,
	},
};
#endif

static struct dev_info_platform_data dev_info = {
	.dev_model = "SOHO",
};

static struct platform_device dev_info_device = {
	.name           = "dev_info",
	.id             = -1,
	.dev = {
		.platform_data = &dev_info,
	},
};

static struct platform_device *soho_devices[] __initdata = {
	/*Config key VolumeUp/VolumeDown*/
	&bowser_gpio_keys_device,
	&bowser_case_sensor_device,
#ifdef CONFIG_SND_SOC_MAX98090
	&bowser_maxim_audio,
#endif
#ifdef CONFIG_INPUT_BU52061_HALLSENSOR
	&bu52061_platform_device,
#endif
	&dev_info_device,
};

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4

static int __init omap4_touch_init(unsigned interrupt_gpio, bool configure)
{
	int error = 0;

	omap_mux_init_gpio(GPIO_TOUCH_RESET, OMAP_PIN_OUTPUT);

	error = gpio_request(GPIO_TOUCH_RESET, "touchscreen_reset");
	if (error < 0) {
		pr_err("%s:failed to request GPIO %d, error %d\n",
			__func__, GPIO_TOUCH_RESET, error);
		return error;
	}
	gpio_direction_output(GPIO_TOUCH_RESET, 0);
	gpio_direction_output(GPIO_TOUCH_RESET, 1);

	omap_mux_init_gpio(GPIO_TOUCH_IRQ, OMAP_PIN_INPUT_PULLUP);

	error = gpio_request(GPIO_TOUCH_IRQ, "touchscreen_irq");
	if (error < 0) {
		pr_err("%s:failed to request GPIO %d, error %d\n",
			__func__, GPIO_TOUCH_IRQ, error);
		return error;
	}

	error = gpio_direction_input(GPIO_TOUCH_IRQ);
	if (error < 0) {
		pr_err("%s: GPIO configuration failed: GPIO %d,error %d\n",
			__func__, GPIO_TOUCH_IRQ, error);
		gpio_free(GPIO_TOUCH_IRQ);
	}
	return error;
}

static unsigned char TM_TYPE1_f1a_button_codes[] = {};

static struct synaptics_rmi_f1a_button_map TM_TYPE1_f1a_button_map = {
	.nbuttons = ARRAY_SIZE(TM_TYPE1_f1a_button_codes),
	.map = TM_TYPE1_f1a_button_codes,
};

static struct synaptics_rmi4_platform_data rmi4_platformdata = {
	.irq_type = IRQF_TRIGGER_FALLING,
	.gpio = GPIO_TOUCH_IRQ,
/*	.gpio_config = omap4_touch_init,*/
	.f1a_button_map = &TM_TYPE1_f1a_button_map,
};

#endif

#ifdef CONFIG_INV_MPU_IIO
static struct mpu_platform_data mpu_gyro_data = {
	.int_config  = 0x10,
	.level_shifter = 0,
	.orientation = {   -1,  0,  0,
			   0,  1,  0,
			   0,  0,  -1 },
	.key = {0x85, 0x45, 0xf4, 0x9a, 0xfd, 0x77, 0x9a, 0xd2, 0xbc,
		0xe7, 0x64, 0xb7, 0x63, 0xc3, 0x6f, 0x96},
};
/*static struct ext_slave_platform_data mpu_compass_data = {
	.bus         = EXT_SLAVE_BUS_SECONDARY,
	.orientation = { 1, 0, 0,
			 0, 1, 0,
			 0, 0, -1 },
};*/

static void mpu6050b1_init(void)
{
	gpio_request(GPIO_GRYO, "MPUIRQ");
	gpio_direction_input(GPIO_GRYO);
}
#endif

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
	u32 reg;
	u16 control_pbias_offset = OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_PBIASLITE;


	omap_hsmmc_init(controllers);
	for (c = controllers; c->mmc; c++)
		omap4_twl6030_hsmmc_set_late_init(&c->pdev->dev);

	/*
	 * Powerdown MMC1 PBIAS. As MMC1 slot is not used on Jem, and
	 * the associated pads GPIO100-GPIO109 are not connected, we
	 * can safely turn pbias off. This is exactly what MMC driver on
	 * BlazeTablet does when no SD card is inserted into MMC1, so
	 * just steal the code from omap4_hsmmc1_before_set_reg.
	 */
	reg = omap4_ctrl_pad_readl(control_pbias_offset);
	reg &= ~(OMAP4_MMC1_PBIASLITE_PWRDNZ_MASK |
		OMAP4_MMC1_PWRDNZ_MASK |
		OMAP4_MMC1_PBIASLITE_VMODE_MASK);
	omap4_ctrl_pad_writel(reg, control_pbias_offset);

	return 0;
}

#ifdef CONFIG_CHARGER_SMB347
static struct regulator_consumer_supply soho_ldo7_supply[] = {
	{
		.supply = "smb347_ldo7",
	},
};
static struct smb347_platform_data smb347_pdata = {
	.regulator_name = "smb347_ldo7",
};
#endif

static struct regulator_init_data sdp4430_vusim = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 2900000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
				.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
#ifdef CONFIG_CHARGER_SMB347
	.num_consumer_supplies  = ARRAY_SIZE(soho_ldo7_supply),
	.consumer_supplies      = soho_ldo7_supply,
#endif
};

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

static struct regulator_consumer_supply omap4_vaux_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.1"),
};

static struct regulator_init_data jem_vaux1_idata = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies  = ARRAY_SIZE(omap4_vaux_supply),
	.consumer_supplies      = omap4_vaux_supply,
};

static struct twl4030_platform_data soho_twldata = {
	.vaux1		= &jem_vaux1_idata,
	.ldo2		= &jem_vaux1_idata,
	/* TWL6030 regulators at OMAP443X/446X based SOMs */
	.vusim		= &sdp4430_vusim,
	.vcxio		= &sdp4430_vcxio,
	/* TWL6032 regulators at OMAP447X based SOMs */
	.ldo7		= &sdp4430_vusim,
};

#if defined(CONFIG_TMP103_SENSOR)
static struct tmp103_platform_data tmp103_case_info = {
	.domain = "pcb_case",
};

static struct tmp103_platform_data tmp103_hotspot_info = {
	.domain = "pcb_hotspot",
};
#endif

static struct i2c_board_info __initdata bowser_i2c_2_PROTO_boardinfo[] = {
};

static struct i2c_board_info __initdata bowser_i2c_2_EVT1_boardinfo[] = {
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
	},
#endif
};

static struct i2c_board_info __initdata bowser_i2c_2_EVT2_boardinfo[] = {
#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
	},
#endif
#if defined(CONFIG_TMP103_SENSOR)
	{
		I2C_BOARD_INFO("tmp103_temp_sensor", 0x70),
		.platform_data = &tmp103_case_info,
	},
#endif
};

#if 0
static struct i2c_board_info __initdata bowser_i2c_2_boardinfo[] = {
};
#endif

static struct i2c_board_info __initdata bowser_i2c_3_boardinfo[] = {
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
#if defined(CONFIG_BATTERY_BQ27541)
	{
		I2C_BOARD_INFO("bq27541", 0x55),
	},
#endif
#ifdef CONFIG_CHARGER_SMB347
	{
		I2C_BOARD_INFO(SMB347_NAME, 0x5F),
		.platform_data = &smb347_pdata,
		.irq = GPIO_SMB347_IRQ,
	},
#endif
#if defined(CONFIG_TMP103_SENSOR)
	{
		I2C_BOARD_INFO("tmp103_temp_sensor", 0x70),
		.platform_data = &tmp103_case_info,
	},
#endif
};

static struct i2c_board_info __initdata bowser_i2c_4_PROTO_boardinfo[] = {

#ifdef CONFIG_INPUT_PIC12LF1822_PROXIMITY
	{
		I2C_BOARD_INFO(PIC12LF1822_PROXIMITY_NAME, 0x0d),
//		.platform_data = &pic12lf1822_proximity_pdata,
		.irq = GPIO_PIC12LF1822_PROXIMITY_IRQ, /* GPIO85 */
	},
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4
	{
		I2C_BOARD_INFO("synaptics_rmi4_i2c", 0x20),
		.platform_data = &rmi4_platformdata,
        },
#endif

#ifdef CONFIG_INV_MPU_IIO
	{
		I2C_BOARD_INFO("mpu6xxx", 0x68),
		.irq = GPIO_GRYO,
		.platform_data = &mpu_gyro_data,
	},
	/*{
		I2C_BOARD_INFO("ami306", 0x0F),
		.irq = OMAP_GPIO_IRQ(GPIO_GRYO),
		.platform_data = &mpu_compass_data,
	},*/
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

static struct i2c_board_info __initdata bowser_i2c_4_EVT1_boardinfo[] = {

#ifdef CONFIG_INPUT_PIC12LF1822_PROXIMITY
	{
		I2C_BOARD_INFO(PIC12LF1822_PROXIMITY_NAME, 0x0d),
//		.platform_data = &pic12lf1822_proximity_pdata,
		.irq = GPIO_PIC12LF1822_PROXIMITY_IRQ, /* GPIO85 */
	},
#endif

#ifdef CONFIG_INV_MPU_IIO
	{
		I2C_BOARD_INFO("mpu6xxx", 0x68),
		.irq = GPIO_GRYO,
		.platform_data = &mpu_gyro_data,
	},
	/*{
		I2C_BOARD_INFO("ami306", 0x0F),
		.irq = OMAP_GPIO_IRQ(GPIO_GRYO),
		.platform_data = &mpu_compass_data,
	},*/
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
	}
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

static int __init omap4_i2c_init(void)
{
	int i, j;
	struct bowser_i2c_info_t *inf;
	int board_id = idme_get_board_revision();

	for(i=0,inf = &bowser_i2c_info[0]; i < ARRAY_SIZE(bowser_i2c_info); ++i, ++inf)
	{
		printk("Configuring I2C HW spinlocks: bus=%d\n", inf->bus_id);
		omap_i2c_hwspinlock_init(inf->bus_id, inf->bus_id-1, &inf->bus_board_data);

		if (board_id < 3) {
			if( inf->bus_id == 2 ){
				inf->board_info = &bowser_i2c_2_PROTO_boardinfo[0];
				inf->board_info_size = ARRAY_SIZE(bowser_i2c_2_PROTO_boardinfo);
			}
			if( inf->bus_id == 4){
				inf->board_info = &bowser_i2c_4_PROTO_boardinfo[0];
				inf->board_info_size = ARRAY_SIZE(bowser_i2c_4_PROTO_boardinfo);
			}
		}else{
			if( inf->bus_id == 2 ){
				if (board_id == 3) {
					inf->board_info = &bowser_i2c_2_EVT1_boardinfo[0];
					inf->board_info_size =
						ARRAY_SIZE(bowser_i2c_2_EVT1_boardinfo);
				} else {
					inf->board_info = &bowser_i2c_2_EVT2_boardinfo[0];
					inf->board_info_size =
						ARRAY_SIZE(bowser_i2c_2_EVT2_boardinfo);
				}
			} else if (inf->bus_id == 3 && board_id > 3) {
				/* For PVT we skip the old temp sensor, last entry in list */
				if (board_id >= 7) {
					inf->board_info_size--;
				} else {
					/* Detach hotspot pcb sensor from case domain for
					EVT2+. New sensor is already attached above. */
					for (j = 0; j < ARRAY_SIZE(bowser_i2c_3_boardinfo); j++) {
						if (inf->board_info[j].platform_data ==
								&tmp103_case_info) {
							inf->board_info[j].platform_data =
								&tmp103_hotspot_info;
							break;
						}
					}
				}
			} else if (inf->bus_id == 4) {
				inf->board_info = &bowser_i2c_4_EVT1_boardinfo[0];
				inf->board_info_size = ARRAY_SIZE(bowser_i2c_4_EVT1_boardinfo);
			}
		}
		omap_register_i2c_bus_board_data(inf->bus_id, &inf->bus_board_data);
	}

	omap4_pmic_get_config(&soho_twldata, TWL_COMMON_PDATA_USB |
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
	omap4_pmic_init("twl6030", &soho_twldata,
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
		if (inf->bus_id == 3)
			omap2_i2c_pullup(inf->bus_id,
					I2C_PULLUP_STD_860_OM_FAST_500_OM);
	}

	return 0;
}

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {
	OMAP4_MUX(KPD_COL3, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP | OMAP_WAKEUP_EN),
	OMAP4_MUX(USBB2_ULPITLL_CLK, OMAP_MUX_MODE3 | OMAP_PIN_OUTPUT), // Mux
	OMAP4_MUX(SYS_NIRQ2, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
				OMAP_PIN_OFF_WAKEUPENABLE),
	OMAP4_MUX(SYS_NIRQ1, OMAP_MUX_MODE0 | OMAP_PIN_INPUT_PULLUP |
				OMAP_WAKEUP_EN),
#if defined(CONFIG_TOUCHSCREEN_SYNAPTICS_I2C_RMI4)
	/* TOUCH IRQ - GPIO 24 */
	OMAP4_MUX(DPM_EMU13, OMAP_MUX_MODE3 | OMAP_PIN_INPUT_PULLUP),  // internally pullup the IRQ line
#endif
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};

#else
#define board_mux	NULL
#endif

static void __init lpddr2_init(void)
{
	int sd_vendor = omap_sdram_vendor();

	if ((sd_vendor != SAMSUNG_SDRAM) && (sd_vendor != ELPIDA_SDRAM)
			&& (sd_vendor != HYNIX_SDRAM)
			&& (sd_vendor != MICRON_SDRAM)) {
		pr_err("%s: DDR vendor_id %d is not supported!\n",
				__func__, sd_vendor);
		return;
	}

	devinfo_set_emif(sd_vendor);

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

static void __init soho_platform_init(void)
{
#ifdef CONFIG_SND_SOC_MAX98090
	max98090_pdata.irq = gpio_to_irq(GPIO_CODEC_IRQ);
#endif

	omap_mux_init_gpio(MAX97236_GPIO_IRQ, OMAP_PIN_INPUT_PULLUP);

	/*
	 * Drive MSECURE high for TWL6030/6032 write access.
	 */
	omap_mux_init_signal("fref_clk3_req.gpio_wk30", OMAP_PIN_OUTPUT);
	gpio_request(30, "msecure");
	gpio_direction_output(30, 1);
}

int __init bowser_bt_init(void);

static void __init omap_soho_init(void)
{
	int package = OMAP_PACKAGE_CBS;

	lpddr2_init();

	omap4_mux_init(board_mux, NULL, package);
	omap_create_board_props();

	/*
	 * Any additional changes to platfrom data structures before they get
	 * registered.
	 */
	soho_platform_init();
	soho_thermal_init();
	omap4_touch_init(GPIO_TOUCH_IRQ, true);
	omap4_i2c_init();

	printk("%s: [AP]: Registering platform devices\n", __func__);
	platform_add_devices(soho_devices, ARRAY_SIZE(soho_devices));
#ifdef CONFIG_INV_MPU_IIO
	mpu6050b1_init();
#endif
	omap4_board_serial_init();

	omap_sdrc_init(NULL, NULL);
	omap4_twl6030_hsmmc_init(mmc);
	bowser_bt_init();
	bowser_wifi_init();

	usb_musb_init(&musb_board_data);

	omap_init_dmm_tiler();
	omap4_register_ion();

	bowser_panel_init();
	omap_enable_smartreflex_on_init();

	bowser_init_idme();

	printk("\nBoard Init Over\n\n");
}

static void __init omap_soho_reserve(void)
{
	omap_ram_console_init(OMAP_RAM_CONSOLE_START_DEFAULT,
			OMAP_RAM_CONSOLE_SIZE_DEFAULT);
	omap_rproc_reserve_cma(RPROC_CMA_OMAP4);
	bowser_android_display_setup(NULL);
	omap4_ion_init();
	omap4_secure_workspace_addr_default();
	omap_reserve();
}

MACHINE_START(OMAP4_BOWSER, "OMAP4 SOHO board")
	.atag_offset = 0x100,
	.reserve	= omap_soho_reserve,
	.map_io		= omap4_map_io,
	.init_early	= omap4430_init_early,
	.init_irq	= gic_init_irq,
	.handle_irq	= gic_handle_irq,
	.init_machine = omap_soho_init,
	.timer		= &omap4_timer,
	.restart	= omap_prcm_restart,
MACHINE_END
