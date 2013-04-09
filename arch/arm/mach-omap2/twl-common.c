/*
 * twl-common.c
 *
 * Copyright (C) 2011 Texas Instruments, Inc..
 * Author: Peter Ujfalusi <peter.ujfalusi@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/i2c.h>
#include <linux/i2c/twl.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/twl6040.h>

#include <plat/i2c.h>
#include <plat/usb.h>

#include "twl-common.h"
#include "pm.h"
#include "mux.h"
#include <linux/power/ti-fg.h>

static struct i2c_board_info __initdata pmic_i2c_board_info = {
	.addr		= 0x48,
	.flags		= I2C_CLIENT_WAKE,
};

static struct i2c_board_info __initdata omap4_i2c1_board_info[] = {
	{
		.addr		= 0x48,
		.flags		= I2C_CLIENT_WAKE,
	},
	{
		I2C_BOARD_INFO("twl6040", 0x4b),
	},
};

static struct i2c_board_info __initdata omap5_i2c1_generic_info[] = {
	{
		I2C_BOARD_INFO("twl6035", 0x48),
		.flags	= I2C_CLIENT_WAKE,
	},
	{
		I2C_BOARD_INFO("twl6040", 0x4b),
	},
};

static int twl6040_pdm_ul_errata(void)
{
	struct omap_mux_partition *part;
	u16 reg_offset, pdm_ul;

	if (cpu_is_omap44xx())
		reg_offset = OMAP4_CTRL_MODULE_PAD_ABE_PDM_UL_DATA_OFFSET;
	else if (cpu_is_omap54xx())
		reg_offset = OMAP5_CTRL_MODULE_PAD_ABEMCPDM_UL_DATA_OFFSET;
	else
		return 0;

	part = omap_mux_get("core");
	if (!part) {
		pr_err("failed to apply PDM_UL errata\n");
		return -EINVAL;
	}

	/*
	 * ERRATA: Reset value of PDM_UL buffer logic is 1 (VDDVIO)
	 * when AUDPWRON = 0, which causes current drain on this pin's
	 * pull-down on OMAP side. The workaround consists of disabling
	 * pull-down resistor of ABE_PDM_UL_DATA pin
	 */
	pdm_ul = omap_mux_read(part, reg_offset);
	omap_mux_write(part, pdm_ul & ~OMAP_PULL_ENA, reg_offset);

	return 0;
}

void __init omap_pmic_init(int bus, u32 clkrate,
			   const char *pmic_type, int pmic_irq,
			   struct twl4030_platform_data *pmic_data)
{
	strncpy(pmic_i2c_board_info.type, pmic_type,
		sizeof(pmic_i2c_board_info.type));
	pmic_i2c_board_info.irq = pmic_irq;
	pmic_i2c_board_info.platform_data = pmic_data;

	omap_register_i2c_bus(bus, clkrate, &pmic_i2c_board_info, 1);
}

void __init omap4_pmic_init(const char *pmic_type,
		    struct twl4030_platform_data *pmic_data,
		    struct twl6040_platform_data *twl6040_data, int twl6040_irq)
{
	/* PMIC part*/
	strncpy(omap4_i2c1_board_info[0].type, pmic_type,
		sizeof(omap4_i2c1_board_info[0].type));
	omap4_i2c1_board_info[0].irq = OMAP44XX_IRQ_SYS_1N;
	omap4_i2c1_board_info[0].platform_data = pmic_data;

	/* TWL6040 audio IC part */
	twl6040_data->pdm_ul_errata = twl6040_pdm_ul_errata;
	omap4_i2c1_board_info[1].irq = twl6040_irq;
	omap4_i2c1_board_info[1].platform_data = twl6040_data;

	omap_register_i2c_bus(1, 400, omap4_i2c1_board_info, 2);

}

/**
 * omap5_pmic_init() - PMIC init for i2c
 * @bus_id:	which bus
 * @speed:	speed
 * @pmic_type:	name of pmic
 * @pmic_data:	pointer to pmic data
 * @audio_data:	audio data
 * @audio_irq: irq for audio
 * @bd_info:	other board specific i2c_board_info data
 * @num_bd_info: count
 *
 * To be called after register_i2c_bus is called by board file
 */
void __init omap5_pmic_init(int bus_id, const char *pmic_type, int pmic_irq,
			    struct palmas_platform_data *pmic_data,
			    const char *audio_type, int audio_irq,
			    struct twl6040_platform_data *audio_data)
{
	/* PMIC part*/
	strncpy(omap5_i2c1_generic_info[0].type, pmic_type,
		sizeof(omap5_i2c1_generic_info[0].type));
	omap5_i2c1_generic_info[0].irq = pmic_irq;
	omap5_i2c1_generic_info[0].platform_data = pmic_data;

	/* TWL6040 audio IC part */
	strncpy(omap5_i2c1_generic_info[1].type, audio_type,
		sizeof(omap5_i2c1_generic_info[1].type));
	audio_data->pdm_ul_errata = twl6040_pdm_ul_errata;
	omap5_i2c1_generic_info[1].irq = audio_irq;
	omap5_i2c1_generic_info[1].platform_data = audio_data;

	i2c_register_board_info(bus_id, omap5_i2c1_generic_info,
				ARRAY_SIZE(omap5_i2c1_generic_info));
}

void __init omap_pmic_late_init(void)
{
	/* Init the OMAP TWL parameters (if PMIC has been registerd) */
	if (!pmic_i2c_board_info.irq && !omap4_i2c1_board_info[0].irq &&
	    !omap5_i2c1_generic_info[0].irq)
		return;
	omap_twl_init();
	omap_tps6236x_init();
	omap_palmas_init();
}

#if defined(CONFIG_ARCH_OMAP3)
static struct twl4030_usb_data omap3_usb_pdata = {
	.usb_mode	= T2_USB_MODE_ULPI,
};

static int omap3_batt_table[] = {
/* 0 C */
30800, 29500, 28300, 27100,
26000, 24900, 23900, 22900, 22000, 21100, 20300, 19400, 18700, 17900,
17200, 16500, 15900, 15300, 14700, 14100, 13600, 13100, 12600, 12100,
11600, 11200, 10800, 10400, 10000, 9630,  9280,  8950,  8620,  8310,
8020,  7730,  7460,  7200,  6950,  6710,  6470,  6250,  6040,  5830,
5640,  5450,  5260,  5090,  4920,  4760,  4600,  4450,  4310,  4170,
4040,  3910,  3790,  3670,  3550
};

static struct twl4030_bci_platform_data omap3_bci_pdata = {
	.battery_tmp_tbl	= omap3_batt_table,
	.tblsize		= ARRAY_SIZE(omap3_batt_table),
};

static struct twl4030_madc_platform_data omap3_madc_pdata = {
	.irq_line	= 1,
};

static struct twl4030_codec_data omap3_codec;

static struct twl4030_audio_data omap3_audio_pdata = {
	.audio_mclk = 26000000,
	.codec = &omap3_codec,
};

static struct regulator_consumer_supply omap3_vdda_dac_supplies[] = {
	REGULATOR_SUPPLY("vdda_dac", "omapdss_venc"),
};

static struct regulator_init_data omap3_vdac_idata = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap3_vdda_dac_supplies),
	.consumer_supplies	= omap3_vdda_dac_supplies,
};

static struct regulator_consumer_supply omap3_vpll2_supplies[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.0"),
};

static struct regulator_init_data omap3_vpll2_idata = {
	.constraints = {
		.min_uV                 = 1800000,
		.max_uV                 = 1800000,
		.valid_modes_mask       = REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask         = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies		= ARRAY_SIZE(omap3_vpll2_supplies),
	.consumer_supplies		= omap3_vpll2_supplies,
};

void __init omap3_pmic_get_config(struct twl4030_platform_data *pmic_data,
				  u32 pdata_flags, u32 regulators_flags)
{
	if (!pmic_data->irq_base)
		pmic_data->irq_base = TWL4030_IRQ_BASE;
	if (!pmic_data->irq_end)
		pmic_data->irq_end = TWL4030_IRQ_END;

	/* Common platform data configurations */
	if (pdata_flags & TWL_COMMON_PDATA_USB && !pmic_data->usb)
		pmic_data->usb = &omap3_usb_pdata;

	if (pdata_flags & TWL_COMMON_PDATA_BCI && !pmic_data->bci)
		pmic_data->bci = &omap3_bci_pdata;

	if (pdata_flags & TWL_COMMON_PDATA_MADC && !pmic_data->madc)
		pmic_data->madc = &omap3_madc_pdata;

	if (pdata_flags & TWL_COMMON_PDATA_AUDIO && !pmic_data->audio)
		pmic_data->audio = &omap3_audio_pdata;

	/* Common regulator configurations */
	if (regulators_flags & TWL_COMMON_REGULATOR_VDAC && !pmic_data->vdac)
		pmic_data->vdac = &omap3_vdac_idata;

	if (regulators_flags & TWL_COMMON_REGULATOR_VPLL2 && !pmic_data->vpll2)
		pmic_data->vpll2 = &omap3_vpll2_idata;
}
#endif /* CONFIG_ARCH_OMAP3 */

#if defined(CONFIG_ARCH_OMAP4)
static struct twl4030_usb_data omap4_usb_pdata = {
};

static struct twl4030_madc_platform_data omap4_madc_pdata = {
	.irq_line = -1,
};

/* Fuel Gauge EDV Configuration */
static struct edv_config edv_cfg = {
	.averaging = true,
	.seq_edv = 5,
	.filter_light = 155,
	.filter_heavy = 199,
	.overload_current = 1000,
	.edv = {
		{3150, 0},
		{3450, 4},
		{3600, 10},
	},
};

/* Fuel Gauge OCV Configuration */
static struct ocv_config ocv_cfg = {
	.voltage_diff = 75,
	.current_diff = 30,

	.sleep_enter_current = 60,
	.sleep_enter_samples = 3,

	.sleep_exit_current = 100,
	.sleep_exit_samples = 3,

	.long_sleep_current = 500,
	.ocv_period = 300,
	.relax_period = 600,

	.flat_zone_low = 35,
	.flat_zone_high = 65,

	.max_ocv_discharge = 1300,

	.table = {
		3300, 3603, 3650, 3662, 3700,
		3723, 3734, 3746, 3756, 3769,
		3786, 3807, 3850, 3884, 3916,
		3949, 3990, 4033, 4077, 4129,
		4193
	},
};

/* General OMAP4 Battery Cell Configuration */
static struct cell_config cell_cfg =  {
	.cc_voltage = 4175,
	.cc_current = 250,
	.cc_capacity = 15,
	.seq_cc = 5,

	.cc_polarity = true,
	.cc_out = true,
	.ocv_below_edv1 = false,

	.design_capacity = 4000,
	.design_qmax = 4100,
	.r_sense = 10,

	.qmax_adjust = 1,
	.fcc_adjust = 2,

	.max_overcharge = 100,
	.electronics_load = 200, /* *10 uAh */

	.max_increment = 150,
	.max_decrement = 150,
	.low_temp = 119,
	.deep_dsg_voltage = 30,
	.max_dsg_estimate = 300,
	.light_load = 100,
	.near_full = 500,
	.cycle_threshold = 3500,
	.recharge = 300,

	.mode_switch_capacity = 5,

	.call_period = 2,

	.ocv = &ocv_cfg,
	.edv = &edv_cfg,
};

static int omap4_batt_table[] = {
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

static struct twl4030_bci_platform_data omap4_bci_pdata = {
	.monitoring_interval		= 10,
	.max_charger_currentmA		= 1500,
	.max_charger_voltagemV		= 4560,
	.max_bat_voltagemV		= 4200,
	.low_bat_voltagemV		= 3300,
	.battery_tmp_tbl		= omap4_batt_table,
	.tblsize			= ARRAY_SIZE(omap4_batt_table),
	.cell_cfg			= &cell_cfg,
};

static struct regulator_init_data omap4_vdac_idata = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.supply_regulator	= "V2V1",
};

static struct regulator_consumer_supply omap4_vaux_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.1"),
};

static struct regulator_init_data omap4_vaux1_idata = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3000000,
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
	.num_consumer_supplies  = ARRAY_SIZE(omap4_vaux_supply),
	.consumer_supplies      = omap4_vaux_supply,
};

static struct regulator_consumer_supply omap4_vaux2_supply[] = {
	REGULATOR_SUPPLY("av-switch", "omap-abe-twl6040"),
};

static struct regulator_init_data omap4_vaux2_idata = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies  = ARRAY_SIZE(omap4_vaux2_supply),
	.consumer_supplies      = omap4_vaux2_supply,
};

static struct regulator_consumer_supply omap4_cam2_supply[] = {
	REGULATOR_SUPPLY("cam2pwr", NULL),
};

static struct regulator_init_data omap4_vaux3_idata = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies  = ARRAY_SIZE(omap4_cam2_supply),
	.consumer_supplies      = omap4_cam2_supply,
};

static struct regulator_consumer_supply omap4_vmmc_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.0"),
};

/* VMMC1 for MMC1 card */
static struct regulator_init_data omap4_vmmc_idata = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,

	},
	.num_consumer_supplies  = ARRAY_SIZE(omap4_vmmc_supply),
	.consumer_supplies      = omap4_vmmc_supply,
};

static struct regulator_init_data omap4_vpp_idata = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data omap4_vusim_idata = {
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
};

static struct regulator_init_data omap4_vana_idata = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.enabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct regulator_consumer_supply omap4_vcxio_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.0"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.1"),
};

static struct regulator_init_data omap4_vcxio_idata = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap4_vcxio_supply),
	.consumer_supplies	= omap4_vcxio_supply,
	.supply_regulator	= "V2V1",
};

static struct regulator_consumer_supply omap4_vusb_supply[] = {
		REGULATOR_SUPPLY("vusb", "twl6030_usb"),
};

static struct regulator_init_data omap4_vusb_idata = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap4_vusb_supply),
	.consumer_supplies	= omap4_vusb_supply,
};

static struct regulator_init_data omap4_clk32kg_idata = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_consumer_supply omap4_v1v8_supply[] = {
	REGULATOR_SUPPLY("vio", "1-004b"),
};

static struct regulator_init_data omap4_v1v8_idata = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.enabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap4_v1v8_supply),
	.consumer_supplies	= omap4_v1v8_supply,
};

static struct regulator_consumer_supply omap4_v2v1_supply[] = {
	REGULATOR_SUPPLY("v2v1", "1-004b"),
};

static struct regulator_init_data omap4_v2v1_idata = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap4_v2v1_supply),
	.consumer_supplies	= omap4_v2v1_supply,
};

static struct regulator_init_data omap4_ext_v2v1_idata = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
	},
	.num_consumer_supplies	= ARRAY_SIZE(omap4_v2v1_supply),
	.consumer_supplies	= omap4_v2v1_supply,
	.supply_regulator	= "SYSEN",
};

static struct regulator_init_data omap4_sysen_idata = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data omap4_clk32kaudio_idata = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_init_data omap4_regen1_idata = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct twl6030_thermal_data omap4_thermal_pdata = {
	.hotdie_cfg = TWL6030_HOTDIE_130C,
};

void __init omap4_pmic_get_config(struct twl4030_platform_data *pmic_data,
				  u32 pdata_flags, u32 regulators_flags)
{
	if (!pmic_data->irq_base)
		pmic_data->irq_base = TWL6030_IRQ_BASE;
	if (!pmic_data->irq_end)
		pmic_data->irq_end = TWL6030_IRQ_END;

	/* Common platform data configurations */
	if (pdata_flags & TWL_COMMON_PDATA_USB && !pmic_data->usb)
		pmic_data->usb = &omap4_usb_pdata;

	if (pdata_flags & TWL_COMMON_PDATA_BCI && !pmic_data->bci)
		pmic_data->bci = &omap4_bci_pdata;

	if (pdata_flags & TWL_COMMON_PDATA_MADC && !pmic_data->madc)
		pmic_data->madc = &omap4_madc_pdata;

	if (pdata_flags & TWL_COMMON_PDATA_THERMAL && !pmic_data->thermal)
		pmic_data->thermal = &omap4_thermal_pdata;

	/* Common regulator configurations */
	if (regulators_flags & TWL_COMMON_REGULATOR_VDAC) {
		if (!pmic_data->vdac)
			pmic_data->vdac = &omap4_vdac_idata;
		if (!pmic_data->ldoln)
			pmic_data->ldoln = &omap4_vdac_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VAUX1) {
		if (!pmic_data->vaux1)
			pmic_data->vaux1 = &omap4_vaux1_idata;
		if (!pmic_data->ldo2)
			pmic_data->ldo2 = &omap4_vaux1_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VAUX2) {
		if (!pmic_data->vaux2)
			pmic_data->vaux2 = &omap4_vaux2_idata;
		if (!pmic_data->ldo4)
			pmic_data->ldo4 = &omap4_vaux2_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VAUX3) {
		if (!pmic_data->vaux3)
			pmic_data->vaux3 = &omap4_vaux3_idata;
		if (!pmic_data->ldo3)
			pmic_data->ldo3 = &omap4_vaux3_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VMMC) {
		if (!pmic_data->vmmc)
			pmic_data->vmmc = &omap4_vmmc_idata;
		if (!pmic_data->ldo5)
			pmic_data->ldo5 = &omap4_vmmc_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VPP) {
		if (!pmic_data->vpp)
			pmic_data->vpp = &omap4_vpp_idata;
		if (!pmic_data->ldo1)
			pmic_data->ldo1 = &omap4_vpp_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VUSIM) {
		if (!pmic_data->vusim)
			pmic_data->vusim = &omap4_vusim_idata;
		if (!pmic_data->ldo7)
			pmic_data->ldo7 = &omap4_vusim_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VANA && !pmic_data->vana)
		pmic_data->vana = &omap4_vana_idata;

	if (regulators_flags & TWL_COMMON_REGULATOR_VCXIO) {
		if (!pmic_data->vcxio)
			pmic_data->vcxio = &omap4_vcxio_idata;
		if (!pmic_data->ldo6)
			pmic_data->ldo6 = &omap4_vcxio_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_VUSB) {
		if (!pmic_data->vusb)
			pmic_data->vusb = &omap4_vusb_idata;
		if (!pmic_data->ldousb)
			pmic_data->ldousb = &omap4_vusb_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_CLK32KG &&
	    !pmic_data->clk32kg)
		pmic_data->clk32kg = &omap4_clk32kg_idata;

	if (regulators_flags & TWL_COMMON_REGULATOR_V1V8) {
		if (!pmic_data->v1v8)
			pmic_data->v1v8 = &omap4_v1v8_idata;
		if (!pmic_data->smps4)
			pmic_data->smps4 = &omap4_v1v8_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_V2V1) {
		if (!pmic_data->v2v1)
			pmic_data->v2v1 = &omap4_v2v1_idata;
		if (!pmic_data->ext_v2v1)
			pmic_data->ext_v2v1 = &omap4_ext_v2v1_idata;
	}

	if (regulators_flags & TWL_COMMON_REGULATOR_SYSEN &&
	    !pmic_data->sysen)
		pmic_data->sysen = &omap4_sysen_idata;

	if (regulators_flags & TWL_COMMON_REGULATOR_CLK32KAUDIO &&
	    !pmic_data->clk32kaudio)
		pmic_data->clk32kaudio = &omap4_clk32kaudio_idata;

	if (regulators_flags & TWL_COMMON_REGULATOR_REGEN1 &&
	    !pmic_data->regen1)
		pmic_data->regen1 = &omap4_regen1_idata;
}
#endif /* CONFIG_ARCH_OMAP4 */

/**
 * omap_pmic_register_data() - Register the PMIC information to OMAP mapping
 * @map:    array ending with a empty element representing the maps
 */
int __init omap_pmic_register_data(struct omap_pmic_map *map)
{
	struct voltagedomain *voltdm;
	int r;

	if (!map)
		return 0;

	while (map->name) {
		if (cpu_is_omap34xx() && !(map->cpu & PMIC_CPU_OMAP3))
			goto next;

		if (cpu_is_omap443x() && !(map->cpu & PMIC_CPU_OMAP4430))
			goto next;

		if (cpu_is_omap446x() && !(map->cpu & PMIC_CPU_OMAP4460))
			goto next;

		if (cpu_is_omap447x() && !(map->cpu & PMIC_CPU_OMAP4470))
			goto next;

		if (cpu_is_omap54xx() && !(map->cpu & PMIC_CPU_OMAP54XX))
			goto next;

		voltdm = voltdm_lookup(map->name);
		if (IS_ERR_OR_NULL(voltdm)) {
			pr_err("%s: unable to find map %s\n", __func__,
			       map->name);
			goto next;
		}
		if (IS_ERR_OR_NULL(map->pmic_data)) {
			pr_warning("%s: domain[%s] has no pmic data\n",
				   __func__, map->name);
			goto next;
		}

		r = omap_voltage_register_pmic(voltdm, map->pmic_data);
		if (r) {
			pr_warning("%s: domain[%s] register returned %d\n",
				   __func__, map->name, r);
			goto next;
		}
		if (map->special_action) {
			r = map->special_action(voltdm);
			WARN(r, "%s: domain[%s] action returned %d\n", __func__,
			     map->name, r);
		}
next:
		map++;
	}

	return 0;
}
