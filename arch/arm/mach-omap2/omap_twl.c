/**
 * OMAP and TWL PMIC specific intializations.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated.
 * Thara Gopinath
 * Copyright (C) 2009 Texas Instruments Incorporated.
 * Nishanth Menon
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
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
#include "pm.h"
#include "twl-common.h"

/* PMIC SmartReflex registers */

/* TWL4030 */
#define TWL4030_SRI2C_SLAVE_ADDR	0x12
#define TWL4030_VDD1_SR_CONTROL_REG	0x00
#define TWL4030_VDD2_SR_CONTROL_REG	0x01

/* TWL6030 */
#define TWL6030_SRI2C_SLAVE_ADDR	0x12
#define TWL6030_VCORE1_SR_VOLT_REG	0x55
#define TWL6030_VCORE1_SR_CMD_REG	0x56
#define TWL6030_VCORE2_SR_VOLT_REG	0x5B
#define TWL6030_VCORE2_SR_CMD_REG	0x5C
#define TWL6030_VCORE3_SR_VOLT_REG	0x61
#define TWL6030_VCORE3_SR_CMD_REG	0x62
#define TWL6030_SMPS_OFFSET_REG	0xB0

/* TWL6032 */
#define TWL6032_SRI2C_SLAVE_ADDR	0x12
#define TWL6032_SMPS5_SR_VOLT_REG	0x49
#define TWL6032_SMPS5_SR_CMD_REG	0x4A
#define TWL6032_SMPS1_SR_VOLT_REG	0x55
#define TWL6032_SMPS1_SR_CMD_REG	0x56
#define TWL6032_SMPS2_SR_VOLT_REG	0x5B
#define TWL6032_SMPS2_SR_CMD_REG	0x5C

#define TWL603x_SMPS_OFFSET_RW_ACCESS_MASK	BIT(7)
#define TWL603x_SMPS1_OFFSET_EN_MASK		BIT(3)
#define TWL603x_SMPS_OFFSET_EN_ALL_MASK	0x7F


enum {
	SMPS_OFFSET_UNINITIALIZED,
	SMPS_OFFSET_DISABLED,
	SMPS_OFFSET_ENABLED
};

static int smps_offset_state = SMPS_OFFSET_UNINITIALIZED;

/*
 * Flag to ensure Smartreflex bit in TWL
 * being cleared in board file is not overwritten.
 */
static bool __initdata twl_sr_enable_autoinit;

#define TWL4030_DCDC_GLOBAL_CFG        0x06
#define REG_SMPS_OFFSET         0xE0
#define SMARTREFLEX_ENABLE     BIT(3)

static unsigned long twl4030_vsel_to_uv(const u8 vsel)
{
	return (((vsel * 125) + 6000)) * 100;
}

static u8 twl4030_uv_to_vsel(unsigned long uv)
{
	return DIV_ROUND_UP(uv - 600000, 12500);
}

static unsigned long twl6030_vsel_to_uv(const u8 vsel)
{
	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET
	 * efuse register the voltage range supported in
	 * standard mode can be either between 0.6V - 1.3V or
	 * 0.7V - 1.4V. We should know the current state of
	 * SMPS_OFFSET before performing any conversations.
	 */
	if (SMPS_OFFSET_UNINITIALIZED == smps_offset_state)
		pr_err("%s: smps offset is not initialized\n", __func__);

	/* Special case for 0 voltage */
	if (!vsel)
		return 0;

	/*
	 * There is no specific formula for voltage to vsel
	 * conversion above 1.3V. There are special hardcoded
	 * values for voltages above 1.3V. Currently we are
	 * hardcoding only for 1.35 V which is used for 1GH OPP for
	 * OMAP4430.
	 */
	if (vsel == 0x3A)
		return 1350000;

	if (SMPS_OFFSET_ENABLED == smps_offset_state)
		return ((((vsel - 1) * 1266) + 70900)) * 10;
	else
		return ((((vsel - 1) * 1266) + 60770)) * 10;
}

static u8 twl6030_uv_to_vsel(unsigned long uv)
{
	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET
	 * efuse register the voltage range supported in
	 * standard mode can be either between 0.6V - 1.3V or
	 * 0.7V - 1.4V. We should know the current state of
	 * SMPS_OFFSET before performing any conversations.
	 */
	if (SMPS_OFFSET_UNINITIALIZED == smps_offset_state)
		pr_err("%s: smps offset is not initialized\n", __func__);

	/* Special case for 0 voltage */
	if (!uv)
		return 0x00;

	/*
	 * There is no specific formula for voltage to vsel
	 * conversion above 1.3V. There are special hardcoded
	 * values for voltages above 1.3V. Currently we are
	 * hardcoding only for 1.35 V which is used for 1GH OPP for
	 * OMAP4430.
	 */
	if (uv > twl6030_vsel_to_uv(0x39)) {
		if (uv == 1350000)
			return 0x3A;
		pr_err("%s:OUT OF RANGE! non mapped vsel for %ld Vs max %ld\n",
			__func__, uv, twl6030_vsel_to_uv(0x39));
		return 0x39;
	}

	if (SMPS_OFFSET_ENABLED == smps_offset_state)
		return DIV_ROUND_UP(uv - 709000, 12660) + 1;
	else
		return DIV_ROUND_UP(uv - 607700, 12660) + 1;
}

static int twl603x_set_offset(struct voltagedomain *vd)
{
	u8 smps_offset_val;
	int r;

	/*
	 * In TWL6030 depending on the value of SMPS_OFFSET efuse register
	 * the voltage range supported in standard mode can be either
	 * between 0.6V - 1.3V or 0.7V - 1.4V.
	 * In TWL6030 ES1.0 SMPS_OFFSET efuse is programmed to all 0's where as
	 * starting from TWL6030 ES1.1 the efuse is programmed to 1.
	 * At this point of execution there is no way to identify TWL type,
	 * so use SMPS1/VCORE1 only to identify if SMPS_OFFSET enabled or not
	 * for on both TWL6030/TWL6032 and assume that if SMPS_OFFSET is
	 * enabled for SMPS1/VCORE1 then it's enabled for all SMPS regulators.
	 */
	if (SMPS_OFFSET_UNINITIALIZED != smps_offset_state)
		/* already configured */
		return 0;

	r = twl_i2c_read_u8(TWL_MODULE_PM_RECEIVER, &smps_offset_val,
			TWL6030_SMPS_OFFSET_REG);
	if (r) {
		WARN(1, "%s: No SMPS OFFSET value=??? " \
			"read failed %d, max val might be wrong\n",
			__func__, r);
		/* Nothing we can do */
		return r;
	}

	/* Check if SMPS offset already set */
	if ((smps_offset_val & TWL603x_SMPS1_OFFSET_EN_MASK) ==
	     TWL603x_SMPS1_OFFSET_EN_MASK) {
		smps_offset_state = SMPS_OFFSET_ENABLED;
		return 0;
	}

	smps_offset_state = SMPS_OFFSET_DISABLED;

	/* Check if TWL firmware lets us write */
	if (!(smps_offset_val & TWL603x_SMPS_OFFSET_RW_ACCESS_MASK)) {
		WARN(1, "%s: No SMPS OFFSET value=0x%02x " \
			"update not possible, max val might be wrong\n",
			__func__, smps_offset_val);
		/* Nothing we can do */
		return -EACCES;
	}

	/* Attempt to set offset for all */
	r = twl_i2c_write_u8(TWL_MODULE_PM_RECEIVER,
			     TWL603x_SMPS_OFFSET_RW_ACCESS_MASK |
			     TWL603x_SMPS_OFFSET_EN_ALL_MASK,
			     TWL6030_SMPS_OFFSET_REG);
	if (r) {
		WARN(1, "%s: No SMPS OFFSET value=0x%02x " \
			"update failed %d, max val might be wrong\n",
			__func__, smps_offset_val, r);
		/* Nothing we can do */
		return r;
	}

	/* Check if SMPS offset now set */
	r = twl_i2c_read_u8(TWL_MODULE_PM_RECEIVER, &smps_offset_val,
			TWL6030_SMPS_OFFSET_REG);
	if (r) {
		WARN(1, "%s: No SMPS OFFSET value=0x%02x " \
			"check(r=%d) failed, max val might be wrong\n",
			__func__, smps_offset_val, r);
		/* Nothing we can do */
		return r;
	}

	if (!(smps_offset_val & TWL603x_SMPS1_OFFSET_EN_MASK)) {
		WARN(1, "%s: No SMPS OFFSET value=0x%02x " \
			"check failed (r!=w), max val might be wrong\n",
			__func__, smps_offset_val);
		/* Nothing we can do */
		return -EFAULT;
	}

	smps_offset_state = SMPS_OFFSET_ENABLED;

	return 0;
}

static struct omap_voltdm_pmic twl4030_vdd1_pmic = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.vp_erroroffset		= OMAP3_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP3_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP3_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 600000,
	.vddmax			= 1450000,
	.vp_timeout_us		= OMAP3_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL4030_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL4030_VDD1_SR_CONTROL_REG,
	.i2c_high_speed		= true,
	.vsel_to_uv		= twl4030_vsel_to_uv,
	.uv_to_vsel		= twl4030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl4030_vdd2_pmic = {
	.slew_rate		= 4000,
	.step_size		= 12500,
	.vp_erroroffset		= OMAP3_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP3_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP3_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 600000,
	.vddmax			= 1450000,
	.vp_timeout_us		= OMAP3_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL4030_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL4030_VDD2_SR_CONTROL_REG,
	.i2c_high_speed		= true,
	.vsel_to_uv		= twl4030_vsel_to_uv,
	.uv_to_vsel		= twl4030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6030_vcore1_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 709000,
	.vddmax			= 1418000,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL6030_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL6030_VCORE1_SR_VOLT_REG,
	.cmd_reg_addr		= TWL6030_VCORE1_SR_CMD_REG,
	.i2c_high_speed		= true,
#ifdef CONFIG_MACH_OMAP4_BOWSER
	.i2c_scll_low		= 0x2B,
	.i2c_scll_high		= 0x29,
	.i2c_hscll_low		= 0x0F,
#else
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
#endif
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6030_vcore2_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 709000,
	.vddmax			= 1418000,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL6030_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL6030_VCORE2_SR_VOLT_REG,
	.cmd_reg_addr		= TWL6030_VCORE2_SR_CMD_REG,
	.i2c_high_speed		= true,
#ifdef CONFIG_MACH_OMAP4_BOWSER
	.i2c_scll_low		= 0x2B,
	.i2c_scll_high		= 0x29,
	.i2c_hscll_low		= 0x0F,
#else
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
#endif
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6030_vcore3_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 709000,
	.vddmax			= 1418000,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL6030_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL6030_VCORE3_SR_VOLT_REG,
	.cmd_reg_addr		= TWL6030_VCORE3_SR_CMD_REG,
	.i2c_high_speed		= true,
#ifdef CONFIG_MACH_OMAP4_BOWSER
	.i2c_scll_low		= 0x2B,
	.i2c_scll_high		= 0x29,
	.i2c_hscll_low		= 0x0F,
#else
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
#endif
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6032_smps1_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 709000,
	.vddmax			= 1418000,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL6032_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL6032_SMPS1_SR_VOLT_REG,
	.cmd_reg_addr		= TWL6032_SMPS1_SR_CMD_REG,
	.i2c_high_speed		= true,
#ifdef CONFIG_MACH_OMAP4_BOWSER
	.i2c_scll_low		= 0x2B,
	.i2c_scll_high		= 0x29,
	.i2c_hscll_low		= 0x0F,
#else
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
#endif
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6032_smps2_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 709000,
	.vddmax			= 1418000,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL6032_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL6032_SMPS2_SR_VOLT_REG,
	.cmd_reg_addr		= TWL6032_SMPS2_SR_CMD_REG,
	.i2c_high_speed		= true,
#ifdef CONFIG_MACH_OMAP4_BOWSER
	.i2c_scll_low		= 0x2B,
	.i2c_scll_high		= 0x29,
	.i2c_hscll_low		= 0x0F,
#else
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
#endif
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static struct omap_voltdm_pmic twl6032_smps5_pmic = {
	.slew_rate		= 9000,
	.step_size		= 12660,
	.switch_on_time		= 549,
	.vp_erroroffset		= OMAP4_VP_CONFIG_ERROROFFSET,
	.vp_vstepmin		= OMAP4_VP_VSTEPMIN_VSTEPMIN,
	.vp_vstepmax		= OMAP4_VP_VSTEPMAX_VSTEPMAX,
	.vddmin			= 709000,
	.vddmax			= 1418000,
	.vp_timeout_us		= OMAP4_VP_VLIMITTO_TIMEOUT_US,
	.i2c_slave_addr		= TWL6032_SRI2C_SLAVE_ADDR,
	.volt_reg_addr		= TWL6032_SMPS5_SR_VOLT_REG,
	.cmd_reg_addr		= TWL6032_SMPS5_SR_CMD_REG,
	.i2c_high_speed		= true,
#ifdef CONFIG_MACH_OMAP4_BOWSER
	.i2c_scll_low		= 0x2B,
	.i2c_scll_high		= 0x29,
	.i2c_hscll_low		= 0x0F,
#else
	.i2c_scll_low		= 0x28,
	.i2c_scll_high		= 0x2C,
	.i2c_hscll_low		= 0x0B,
#endif
	.i2c_hscll_high		= 0x00,
	.vsel_to_uv		= twl6030_vsel_to_uv,
	.uv_to_vsel		= twl6030_uv_to_vsel,
};

static int __init twl_set_sr(struct voltagedomain *voltdm)
{
	int r = 0;

	/*
	 * The smartreflex bit on twl4030 specifies if the setting of voltage
	 * is done over the I2C_SR path. Since this setting is independent of
	 * the actual usage of smartreflex AVS module, we enable TWL SR bit
	 * by default irrespective of whether smartreflex AVS module is enabled
	 * on the OMAP side or not. This is because without this bit enabled,
	 * the voltage scaling through vp forceupdate/bypass mechanism of
	 * voltage scaling will not function on TWL over I2C_SR.
	 */
	if (!twl_sr_enable_autoinit)
		r = omap3_twl_set_sr_bit(true);

	return r;
}

static __initdata struct omap_pmic_map omap_twl_map[] = {
	{
		.name = "mpu_iva",
		.cpu = PMIC_CPU_OMAP3,
		.pmic_data = &twl4030_vdd1_pmic,
		.special_action = twl_set_sr,
	},
	{
		.name = "core",
		.cpu = PMIC_CPU_OMAP3,
		.pmic_data = &twl4030_vdd2_pmic,
	},
	{
		.name = "mpu",
		.cpu = PMIC_CPU_OMAP4430,
		.pmic_data = &twl6030_vcore1_pmic,
		.special_action = twl603x_set_offset,
	},
	{
		.name = "mpu",
		.cpu = PMIC_CPU_OMAP4470,
		.pmic_data = &twl6032_smps1_pmic,
		.special_action = twl603x_set_offset,
	},
	{
		.name = "core",
		.cpu = PMIC_CPU_OMAP4430,
		.pmic_data = &twl6030_vcore3_pmic,
	},
	{
		.name = "core",
		.cpu = PMIC_CPU_OMAP4460,
		.pmic_data = &twl6030_vcore1_pmic,
		.special_action = twl603x_set_offset,
	},
	{
		.name = "core",
		.cpu = PMIC_CPU_OMAP4470,
		.pmic_data = &twl6032_smps2_pmic,
	},
	{
		.name = "iva",
		.cpu = PMIC_CPU_OMAP4430 | PMIC_CPU_OMAP4460,
		.pmic_data = &twl6030_vcore2_pmic,
	},
	{
		.name = "iva",
		.cpu = PMIC_CPU_OMAP4470,
		.pmic_data = &twl6032_smps5_pmic,
	},
	/* Terminator */
	{ .name = NULL, .pmic_data = NULL},
};

int __init omap_twl_init(void)
{
	return omap_pmic_register_data(omap_twl_map);
}

/**
 * omap3_twl_set_sr_bit() - Set/Clear SR bit on TWL
 * @enable: enable SR mode in twl or not
 *
 * If 'enable' is true, enables Smartreflex bit on TWL 4030 to make sure
 * voltage scaling through OMAP SR works. Else, the smartreflex bit
 * on twl4030 is cleared as there are platforms which use OMAP3 and T2 but
 * use Synchronized Scaling Hardware Strategy (ENABLE_VMODE=1) and Direct
 * Strategy Software Scaling Mode (ENABLE_VMODE=0), for setting the voltages,
 * in those scenarios this bit is to be cleared (enable = false).
 *
 * Returns 0 on success, error is returned if I2C read/write fails.
 */
int __init omap3_twl_set_sr_bit(bool enable)
{
	u8 temp;
	int ret;
	if (twl_sr_enable_autoinit)
		pr_warning("%s: unexpected multiple calls\n", __func__);

	ret = twl_i2c_read_u8(TWL4030_MODULE_PM_RECEIVER, &temp,
					TWL4030_DCDC_GLOBAL_CFG);
	if (ret)
		goto err;

	if (enable)
		temp |= SMARTREFLEX_ENABLE;
	else
		temp &= ~SMARTREFLEX_ENABLE;

	ret = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, temp,
				TWL4030_DCDC_GLOBAL_CFG);
	if (!ret) {
		twl_sr_enable_autoinit = true;
		return 0;
	}
err:
	pr_err("%s: Error access to TWL4030 (%d)\n", __func__, ret);
	return ret;
}
