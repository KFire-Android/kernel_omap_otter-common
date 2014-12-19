/* OMAP Identity file for OMAP4 boards.
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Based on
 * mach-omap2/board-44xx-tablet.c
 * mach-omap2/board-4430sdp.c
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/tps6130x.h>
#include "mux.h"
#include <linux/i2c/twl.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/omap-serial.h>
#include <linux/mfd/twl6040-codec.h>
#include "common-board-devices.h"
#include <linux/power/ti-fg.h>

/* EDV Configuration */
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

/* OCV Configuration */
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

/* General Battery Cell Configuration */
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

static struct regulator_consumer_supply vmmc_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.0"),
};

/* VMMC1 for MMC1 card */
static struct regulator_init_data vmmc = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled       = true,
		}
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = vmmc_supply,
};

static struct regulator_init_data vpp = {
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
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data vusim = {
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
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on = true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_consumer_supply vcxio_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi1"),
};

static struct regulator_init_data vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on	= true,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
	.num_consumer_supplies	= ARRAY_SIZE(vcxio_supply),
	.consumer_supplies	= vcxio_supply,
};

static struct regulator_consumer_supply vdac_supply[] = {
	{
		.supply = "hdmi_vref",
	},
};

static struct regulator_init_data vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.always_on	= true,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
	.num_consumer_supplies  = ARRAY_SIZE(vdac_supply),
	.consumer_supplies      = vdac_supply,
};

static struct regulator_consumer_supply vusb_supply[] = {
	REGULATOR_SUPPLY("vusb", "twl6030_usb"),
};

static struct regulator_init_data vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 =	REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
	.num_consumer_supplies  = ARRAY_SIZE(vusb_supply),
	.consumer_supplies      = vusb_supply,
};

static struct regulator_consumer_supply vaux_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.1"),
};

static struct regulator_init_data vaux1 = {
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
			.disabled       = true,
		},
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = vaux_supply,
};

static struct regulator_consumer_supply vaux2_supply[] = {
	REGULATOR_SUPPLY("av-switch", "soc-audio"),
};

static struct regulator_init_data vaux2 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 2800000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL
					| REGULATOR_MODE_STANDBY,
		.valid_ops_mask	 = REGULATOR_CHANGE_VOLTAGE
					| REGULATOR_CHANGE_MODE
					| REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled       = true,
		}
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= vaux2_supply,
};

static struct regulator_consumer_supply cam2_supply[] = {
	{
		.supply = "cam2pwr",
	},
};

static struct regulator_init_data vaux3 = {
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
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = cam2_supply,
};

static struct regulator_init_data clk32kg = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_init_data clk32kaudio = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct regulator_init_data v2v1 = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data sysen = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data regen1 = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
		.state_mem = {
			.disabled	= true,
		},
		.initial_state		= PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data vcore1	= {
	.constraints = {
		.valid_ops_mask         = REGULATOR_CHANGE_STATUS,
		.always_on              = true,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data vcore2	= {
	.constraints = {
		.valid_ops_mask         = REGULATOR_CHANGE_STATUS,
		.always_on              = true,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data vcore3 = {
	.constraints = {
		.valid_ops_mask         = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_init_data vmem = {
	.constraints = {
		.valid_ops_mask         = REGULATOR_CHANGE_STATUS,
		.state_mem = {
			.disabled       = true,
		},
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static int batt_table[] = {
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

static struct twl4030_bci_platform_data bci_data = {
	.monitoring_interval		= 10,
	.max_charger_currentmA		= 1500,
	.max_charger_voltagemV		= 4560,
	.max_bat_voltagemV		= 4200,
	.low_bat_voltagemV		= 3300,
	.battery_tmp_tbl		= batt_table,
	.tblsize			= ARRAY_SIZE(batt_table),
	.cell_cfg			= &cell_cfg,
};

static struct twl4030_usb_data omap4_usbphy_data = {
	.phy_init	= omap4430_phy_init,
	.phy_exit	= omap4430_phy_exit,
	.phy_power	= omap4430_phy_power,
	.phy_suspend	= omap4430_phy_suspend,
};

static struct twl4030_codec_audio_data twl6040_audio = {
	/* single-step ramp for headset and handsfree */
	.hs_left_step   = 0x0f,
	.hs_right_step  = 0x0f,
	.hf_left_step   = 0x1d,
	.hf_right_step  = 0x1d,
	.vddhf_uV	= 4075000,
};

static struct twl4030_codec_vibra_data twl6040_vibra = {
	.max_timeout	= 15000,
	.initial_vibrate = 0,
	.voltage_raise_speed = 0x26,
};

static int twl6040_init(void)
{
	u8 rev = 0;
	int ret;

	ret = twl_i2c_read_u8(TWL_MODULE_AUDIO_VOICE,
				&rev, TWL6040_REG_ASICREV);
	if (ret)
		return ret;

	/*
	 * ERRATA: Reset value of PDM_UL buffer logic is 1 (VDDVIO)
	 * when AUDPWRON = 0, which causes current drain on this pin's
	 * pull-down on OMAP side. The workaround consists of disabling
	 * pull-down resistor of ABE_PDM_UL_DATA pin
	 * Impacted revisions: ES1.1, ES1.2 (both share same ASICREV value),
	 * ES1.3, ES2.0 and ES2.2
	 */
	if ((rev == TWL6040_REV_1_1) ||
	    (rev == TWL6040_REV_1_3) ||
	    (rev == TWL6041_REV_2_0) ||
	    (rev == TWL6041_REV_2_2)) {
		omap_mux_init_signal("abe_pdm_ul_data.abe_pdm_ul_data",
			OMAP_PIN_INPUT);
	}

	return 0;
}

static struct twl4030_codec_data twl6040_codec = {
	.audio          = &twl6040_audio,
	.vibra          = &twl6040_vibra,
	.audpwron_gpio  = 127,
	.naudint_irq    = OMAP44XX_IRQ_SYS_2N,
	.irq_base       = TWL6040_CODEC_IRQ_BASE,
	.init		= twl6040_init,
};

static struct twl4030_madc_platform_data twl6030_gpadc = {
	.irq_line = -1,
};

static struct twl4030_platform_data twldata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* TWL6030 regulators at OMAP443X/4460 based SOMs */
	.vmmc		= &vmmc,
	.vpp		= &vpp,
	.vusim		= &vusim,
	.vana		= &vana,
	.vcxio		= &vcxio,
	.vdac		= &vdac,
	.vusb		= &vusb,
	.vaux1		= &vaux1,
	.vaux2		= &vaux2,
	.vaux3		= &vaux3,

	/* TWL6032 regulators at OMAP447X based SOMs */
	.ldo1		= &vpp,
	.ldo2		= &vaux1,
	.ldo3		= &vaux3,
	.ldo4		= &vaux2,
	.ldo5		= &vmmc,
	.ldo6		= &vcxio,
	.ldo7		= &vusim,
	.ldoln		= &vdac,
	.ldousb		= &vusb,

	/* TWL6030/6032 common resources */
	.clk32kg	= &clk32kg,
	.clk32kaudio	= &clk32kaudio,

	/* SMPS */
	.vdd1		= &vcore1,
	.vdd2		= &vcore2,
	.v2v1		= &v2v1,

	/* children */
	.bci		= &bci_data,
	.usb		= &omap4_usbphy_data,
	.codec		= &twl6040_codec,
	.madc		= &twl6030_gpadc,

	/* External control pins */
	.sysen		= &sysen,
	.regen1		= &regen1,
};

void __init omap4_power_init(void)
{
	/*
	 * VCORE3 & VMEM are not used in 4460. By register it to regulator
	 * framework will ensures that resources are disabled.
	 */
	if (cpu_is_omap446x()) {
		twldata.vdd3 = &vcore3;
		twldata.vmem = &vmem;
	}

	omap4_pmic_init("twl6030", &twldata);
}
