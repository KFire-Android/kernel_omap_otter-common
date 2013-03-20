/**
 * OMAP and TWL PMIC specific intializations.
 *
 * Copyright (C) 2011 Texas Instruments Incorporated.
 * Vishwanat BS
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/i2c/twl.h>

#include "voltage.h"
#include "twl-common.h"
#include "pm.h"

#define OMAP5_SRI2C_SLAVE_ADDR		0x12
#define OMAP5_VDD_MPU_SR_VOLT_REG	0x23
#define OMAP5_VDD_MPU_SR_CMD_REG	0x22
#define OMAP5_VDD_MM_SR_VOLT_REG	0x2B
#define OMAP5_VDD_MM_SR_CMD_REG		0x2A
#define OMAP5_VDD_CORE_SR_VOLT_REG	0x37
#define OMAP5_VDD_CORE_SR_CMD_REG	0x36

static u8 twl6035_uv_to_vsel(unsigned long uv)
{
	if (uv > 1650000) {
		pr_err("%s:OUT OF RANGE! non mapped uv for %ld\n",
		       __func__, uv);
		return 0;
	}

	if (uv == 0)
		return 0;

	return DIV_ROUND_UP((uv - 500000), 10000) + 6;
}

static unsigned long twl6035_vsel_to_uv(const u8 vsel)
{
	u8 temp_vsel = vsel;

	if (vsel > 127) {
		pr_err("%s:OUT OF RANGE! non mapped vsel %d\n",
		       __func__, vsel);
		return 0;
	}

	if (vsel == 0)
		return 0;

	if (temp_vsel > 121)
		temp_vsel = 121;

	temp_vsel = temp_vsel - 6;
	if (temp_vsel < 0)
		return 500000;
	else
		return 500000 + temp_vsel * 10000;
}

static struct omap_voltdm_pmic twl6035_vcore1_pmic = {
	.slew_rate			= 220,	/* BUG00117 - TI internal ref */
	.step_size			= 10000,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP5_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP5_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP5_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 0,
	.vddmax			= 1650000,
	.vp_timeout_us		= OMAP5_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP5_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP5_VDD_MPU_SR_VOLT_REG,
	.cmd_reg_addr		= OMAP5_VDD_MPU_SR_CMD_REG,
	.i2c_high_speed		= true,
	.i2c_scll_low		= 0x15,
	.i2c_scll_high		= 0xe,
	.i2c_hscll_low		= 0x0B,
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6035_vsel_to_uv,
	.uv_to_vsel		= twl6035_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6035_vcore2_pmic = {
	.slew_rate			= 220,	/* BUG00117 - TI internal ref */
	.step_size			= 10000,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP5_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP5_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP5_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 0,
	.vddmax			= 1650000,
	.vp_timeout_us		= OMAP5_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP5_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP5_VDD_MM_SR_VOLT_REG,
	.cmd_reg_addr		= OMAP5_VDD_MM_SR_CMD_REG,
	.i2c_high_speed		= true,
	.i2c_scll_low		= 0x15,
	.i2c_scll_high		= 0xe,
	.i2c_hscll_low		= 0x0B,
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv			= twl6035_vsel_to_uv,
	.uv_to_vsel			= twl6035_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6035_vcore3_pmic = {
	.slew_rate			= 220,	/* BUG00117 - TI internal ref */
	.step_size			= 10000,
	.startup_time		= 500,
	.shutdown_time		= 500,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP5_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP5_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP5_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 0,
	.vddmax			= 1650000,
	.vp_timeout_us		= OMAP5_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= OMAP5_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= OMAP5_VDD_CORE_SR_VOLT_REG,
	.cmd_reg_addr		= OMAP5_VDD_CORE_SR_CMD_REG,
	.i2c_high_speed		= true,
	.i2c_scll_low		= 0x15,
	.i2c_scll_high		= 0xe,
	.i2c_hscll_low		= 0x0B,
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv			= twl6035_vsel_to_uv,
	.uv_to_vsel			= twl6035_uv_to_vsel,
};

static __initdata struct omap_pmic_map omap_twl_map[] = {
	{
		.name = "mpu",
		.cpu = PMIC_CPU_OMAP54XX,
		.pmic_data = &twl6035_vcore1_pmic,
	},
	{
		.name = "core",
		.cpu = PMIC_CPU_OMAP54XX,
		.pmic_data = &twl6035_vcore3_pmic,
	},
	{
		.name = "mm",
		.cpu = PMIC_CPU_OMAP54XX,
		.pmic_data = &twl6035_vcore2_pmic,

	},
	/* Terminator */
	{ .name = NULL, .pmic_data = NULL},
};

int __init omap_palmas_init(void)
{
	return omap_pmic_register_data(omap_twl_map);
}
