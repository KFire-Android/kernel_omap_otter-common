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
#include "mux.h"
#include <linux/i2c/twl.h>
#include <plat/common.h>
#include <plat/usb.h>
#include <plat/omap-serial.h>
#include "common-board-devices.h"

#include "board-4430kc1-tablet.h"

static struct regulator_consumer_supply vmmc_supply[] = {
	{ .supply = "vmmc", .dev_name = "omap_hsmmc.0", },
};

/* VMMC1 for MMC1 card */
static struct regulator_init_data sdp4430_vmmc = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3000000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
	},
	.num_consumer_supplies		= 1,
	.consumer_supplies		= vmmc_supply,
};

static struct regulator_init_data sdp4430_vpp = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 2500000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.initial_state          = PM_SUSPEND_MEM,
	},
};

static struct regulator_consumer_supply audio_supply[] = {
	{ .supply = "audio-pwr", },
};

static struct regulator_init_data sdp4430_vusim = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies      = audio_supply,
};

static struct regulator_init_data sdp4430_vana = {
	.constraints = {
		.min_uV			= 2100000,
		.max_uV			= 2100000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.always_on		= true,
	},
};

static struct regulator_consumer_supply sdp4430_vcxio_supply[] = {
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dss"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.0"),
	REGULATOR_SUPPLY("vdds_dsi", "omapdss_dsi.1"),
};

static struct regulator_init_data sdp4430_vcxio = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.always_on		= true,
	},
	.num_consumer_supplies	= ARRAY_SIZE(sdp4430_vcxio_supply),
	.consumer_supplies	= sdp4430_vcxio_supply,
};

static struct regulator_consumer_supply vdac_supply[] = {
	{
		.supply = "hdmi_vref",
	},
};

static struct regulator_init_data sdp4430_vdac = {
	.constraints = {
		.min_uV			= 1800000,
		.max_uV			= 1800000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.always_on		= true,
	},
	.num_consumer_supplies  = ARRAY_SIZE(vdac_supply),
	.consumer_supplies      = vdac_supply,
};

static struct regulator_consumer_supply vusb_supply[] = {
	REGULATOR_SUPPLY("vusb", "twl6030_usb"),
//	{ .supply = "usb-phy", },
};

static struct regulator_init_data sdp4430_vusb = {
	.constraints = {
		.min_uV			= 3300000,
		.max_uV			= 3300000,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.initial_state          = PM_SUSPEND_MEM,
	},
	.num_consumer_supplies		= 1,
	.consumer_supplies		= vusb_supply,
};

static struct regulator_consumer_supply emmc_supply[] = {
	REGULATOR_SUPPLY("vmmc", "omap_hsmmc.1"),
};

static struct regulator_init_data sdp4430_vaux1 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.always_on		= true,
	},
	.num_consumer_supplies		= 1,
	.consumer_supplies		= emmc_supply,
};

static struct regulator_consumer_supply gsensor_supply[] = {
	{ .supply = "g-sensor-pwr", },
};

static struct regulator_init_data sdp4430_vaux2 = {
	.constraints = {
		.min_uV			= 1200000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
		.always_on		= true,
	},
    .num_consumer_supplies = 1,
    .consumer_supplies = gsensor_supply,
};

static struct regulator_consumer_supply vaux3_supply[] = {
	{ .supply = "vaux3", },
};

static struct regulator_init_data sdp4430_vaux3 = {
	.constraints = {
		.min_uV			= 1000000,
		.max_uV			= 3300000,
		.apply_uV		= true,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL | REGULATOR_MODE_STANDBY,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_STATUS,
		.state_mem		= { .enabled = false, .disabled = true, },
	},
	.num_consumer_supplies		= 1,
	.consumer_supplies		= vaux3_supply,
};

static struct twl4030_madc_platform_data sdp4430_gpadc_data = {
	.irq_line = -1,
};

#define SUMMIT_STAT 31
static struct regulator_init_data sdp4430_clk32kg = {
	.constraints = {
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.always_on		= true,
	},
};

static struct twl4030_usb_data omap4_usbphy_data = {
	.phy_init	= omap4430_phy_init,
	.phy_exit	= omap4430_phy_exit,
	.phy_power	= omap4430_phy_power,
	.phy_set_clock	= omap4430_phy_set_clk,
	.phy_suspend	= omap4430_phy_suspend,
};

static struct twl4030_platform_data sdp4430_twldata = {
	.irq_base	= TWL6030_IRQ_BASE,
	.irq_end	= TWL6030_IRQ_END,

	/* TWL6030 regulators at OMAP443X/4460 based SOMs */
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

	/* TWL6030/6032 common resources */
	.clk32kg	= &sdp4430_clk32kg,

	/* children */
//	.bci            = &sdp4430_bci_data,
	.usb		= &omap4_usbphy_data,
//	.codec          = &twl6040_codec,
	.madc           = &sdp4430_gpadc_data,
};

void __init omap4_power_init(void)
{
	omap4_pmic_init("twl6030", &sdp4430_twldata);
}

