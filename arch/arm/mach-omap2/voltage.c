/*
 * OMAP3/OMAP4 Voltage Management Routines
 *
 * Author: Thara Gopinath	<thara@ti.com>
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 *
 * Copyright (C) 2008 Nokia Corporation
 * Kalle Jokiniemi
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Thara Gopinath <thara@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/plist.h>
#include <linux/slab.h>

#include <plat/omap-pm.h>
#include <plat/omap34xx.h>
#include <plat/opp.h>
#include <plat/opp_twl_tps.h>
#include <plat/clock.h>
#include <plat/common.h>
#include <plat/voltage.h>

#include "prm-regbits-34xx.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"

#define VP_IDLE_TIMEOUT		200
#define VP_TRANXDONE_TIMEOUT	300

#ifdef CONFIG_PM_DEBUG
static struct dentry *voltage_dir;
#endif

/* VP SR debug support */
u32 enable_sr_vp_debug;

/* PRM voltage module */
static u32 volt_mod;

/* Voltage processor register offsets */
struct vp_reg_offs {
	u8 vpconfig;
	u8 vstepmin;
	u8 vstepmax;
	u8 vlimitto;
	u8 vstatus;
	u8 voltage;
};

/* Voltage Processor bit field values, shifts and masks */
struct vp_reg_val {
	/* VPx_VPCONFIG */
	u32 vpconfig_erroroffset;
	u16 vpconfig_errorgain;
	u32 vpconfig_errorgain_mask;
	u8 vpconfig_errorgain_shift;
	u32 vpconfig_initvoltage_mask;
	u8 vpconfig_initvoltage_shift;
	u32 vpconfig_timeouten;
	u32 vpconfig_initvdd;
	u32 vpconfig_forceupdate;
	u32 vpconfig_vpenable;
	/* VPx_VSTEPMIN */
	u8 vstepmin_stepmin;
	u16 vstepmin_smpswaittimemin;
	u8 vstepmin_stepmin_shift;
	u8 vstepmin_smpswaittimemin_shift;
	/* VPx_VSTEPMAX */
	u8 vstepmax_stepmax;
	u16 vstepmax_smpswaittimemax;
	u8 vstepmax_stepmax_shift;
	u8 vstepmax_smpswaittimemax_shift;
	/* VPx_VLIMITTO */
	u16 vlimitto_vddmin;
	u16 vlimitto_vddmax;
	u16 vlimitto_timeout;
	u16 vlimitto_vddmin_shift;
	u16 vlimitto_vddmax_shift;
	u16 vlimitto_timeout_shift;
	/* PRM_IRQSTATUS*/
	u32 tranxdone_status;
};

/**
 * omap_vdd_user_list	- The per vdd user list
 *
 * @dev		: The device asking for the vdd to be set at a particular
 *		  voltage
 * @node	: The list head entry
 * @volt	: The voltage requested by the device <dev>
 */
struct omap_vdd_user_list {
	struct device *dev;
	struct plist_node node;
	u32 volt;
};

/**
 * omap_vdd_info - Per Voltage Domain info
 *
 * @volt_data		: voltage table having the distinct voltages supported
 *			  by the domain and other associated per voltage data.
 * @vp_offs		: structure containing the offsets for various
 *			  vp registers
 * @vp_reg		: the register values, shifts, masks for various
 *			  vp registers
 * @volt_clk		: the clock associated with the vdd.
 * @opp_dev		: the 'struct device' associated with this vdd.
 * @user_lock		: the lock to be used by the plist user_list
 * @user_list		: the list head maintaining the various users
 *			  of this vdd with the voltage requested by each user.
 * @volt_data_count	: Number of distinct voltages supported by this vdd.
 * @nominal_volt	: Nominal voltaged for this vdd.
 * cmdval_reg		: Voltage controller cmdval register.
 * @vdd_sr_reg		: The smartreflex register associated with this VDD.
 */
struct omap_vdd_info{
	struct omap_volt_data *volt_data;
	struct vp_reg_offs vp_offs;
	struct vp_reg_val vp_reg;
	struct clk *volt_clk;
	struct device *opp_dev;
	struct voltagedomain voltdm;
	spinlock_t user_lock;
	struct plist_head user_list;
	struct mutex scaling_mutex;
	int volt_data_count;
	unsigned long nominal_volt;
	u8 cmdval_reg;
	u8 vdd_sr_reg;
};
static struct omap_vdd_info *vdd_info;
/*
 * Number of scalable voltage domains.
 */
static int no_scalable_vdd;

/* OMAP3 VDD sturctures */
static struct omap_vdd_info omap3_vdd_info[] = {
	{
		.vp_offs = {
			.vpconfig = OMAP3_PRM_VP1_CONFIG_OFFSET,
			.vstepmin = OMAP3_PRM_VP1_VSTEPMIN_OFFSET,
			.vstepmax = OMAP3_PRM_VP1_VSTEPMAX_OFFSET,
			.vlimitto = OMAP3_PRM_VP1_VLIMITTO_OFFSET,
			.vstatus = OMAP3_PRM_VP1_STATUS_OFFSET,
			.voltage = OMAP3_PRM_VP1_VOLTAGE_OFFSET,
		},
		.voltdm = {
			.name = "mpu",
		},
	},
	{
		.vp_offs = {
			.vpconfig = OMAP3_PRM_VP2_CONFIG_OFFSET,
			.vstepmin = OMAP3_PRM_VP2_VSTEPMIN_OFFSET,
			.vstepmax = OMAP3_PRM_VP2_VSTEPMAX_OFFSET,
			.vlimitto = OMAP3_PRM_VP2_VLIMITTO_OFFSET,
			.vstatus = OMAP3_PRM_VP2_STATUS_OFFSET,
			.voltage = OMAP3_PRM_VP2_VOLTAGE_OFFSET,
		},
		.voltdm = {
			.name = "core",
		},
	},
};

#define OMAP3_NO_SCALABLE_VDD ARRAY_SIZE(omap3_vdd_info)

/* OMAP4 VDD sturctures */
static struct omap_vdd_info omap4_vdd_info[] = {
	{
		.vp_offs = {
			.vpconfig = OMAP4_PRM_VP_MPU_CONFIG_OFFSET,
			.vstepmin = OMAP4_PRM_VP_MPU_VSTEPMIN_OFFSET,
			.vstepmax = OMAP4_PRM_VP_MPU_VSTEPMAX_OFFSET,
			.vlimitto = OMAP4_PRM_VP_MPU_VLIMITTO_OFFSET,
			.vstatus = OMAP4_PRM_VP_MPU_STATUS_OFFSET,
			.voltage = OMAP4_PRM_VP_MPU_VOLTAGE_OFFSET,
		},
		.voltdm = {
			.name = "mpu",
		},
	},
	{
		.vp_offs = {
			.vpconfig = OMAP4_PRM_VP_IVA_CONFIG_OFFSET,
			.vstepmin = OMAP4_PRM_VP_IVA_VSTEPMIN_OFFSET,
			.vstepmax = OMAP4_PRM_VP_IVA_VSTEPMAX_OFFSET,
			.vlimitto = OMAP4_PRM_VP_IVA_VLIMITTO_OFFSET,
			.vstatus = OMAP4_PRM_VP_IVA_STATUS_OFFSET,
			.voltage = OMAP4_PRM_VP_IVA_VOLTAGE_OFFSET,
		},
		.voltdm = {
			.name = "iva",
		},
	},
	{
		.vp_offs = {
			.vpconfig = OMAP4_PRM_VP_CORE_CONFIG_OFFSET,
			.vstepmin = OMAP4_PRM_VP_CORE_VSTEPMIN_OFFSET,
			.vstepmax = OMAP4_PRM_VP_CORE_VSTEPMAX_OFFSET,
			.vlimitto = OMAP4_PRM_VP_CORE_VLIMITTO_OFFSET,
			.vstatus = OMAP4_PRM_VP_CORE_STATUS_OFFSET,
			.voltage = OMAP4_PRM_VP_CORE_VOLTAGE_OFFSET,
		},
		.voltdm = {
			.name = "core",
		},
	},
};
#define OMAP4_NO_SCALABLE_VDD ARRAY_SIZE(omap4_vdd_info)

/*
 * Default voltage controller settings.
 */
static struct omap_volt_vc_data vc_config = {
	.clksetup = 0xff,
	.voltsetup_time1 = 0xfff,
	.voltsetup_time2 = 0xfff,
	.voltoffset = 0xff,
	.voltsetup2 = 0xff,
	.vdd0_on = 0x30,        /* 1.2v */
	.vdd0_onlp = 0x20,      /* 1.0v */
	.vdd0_ret = 0x1e,       /* 0.975v */
	.vdd0_off = 0x00,       /* 0.6v */
	.vdd1_on = 0x2c,        /* 1.15v */
	.vdd1_onlp = 0x20,      /* 1.0v */
	.vdd1_ret = 0x1e,       /* .975v */
	.vdd1_off = 0x00,       /* 0.6v */
};

/*
 * Default PMIC Data
 */
static struct omap_volt_pmic_info volt_pmic_info = {
	.slew_rate = 4000,
	.step_size = 12500,
};

/*
 * Structures containing OMAP3430/OMAP3630 voltage supported and various
 * data associated with it per voltage domain basis. Smartreflex Ntarget
 * values are left as 0 as they have to be populated by smartreflex
 * driver after reading the efuse.
 */

/* VDD1 */
static struct omap_volt_data omap34xx_vdd1_volt_data[] = {
	{.volt_nominal = 975000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1075000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1200000, .sr_errminlimit = 0xF9, .vp_errgain = 0x18},
	{.volt_nominal = 1270000, .sr_errminlimit = 0xF9, .vp_errgain = 0x18},
	{.volt_nominal = 1350000, .sr_errminlimit = 0xF9, .vp_errgain = 0x18},
};

static struct omap_volt_data omap36xx_vdd1_volt_data[] = {
	{.volt_nominal = 930000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1100000, .sr_errminlimit = 0xF9, .vp_errgain = 0x16},
	{.volt_nominal = 1260000, .sr_errminlimit = 0xFA, .vp_errgain = 0x23},
	{.volt_nominal = 1350000, .sr_errminlimit = 0xFA, .vp_errgain = 0x27},
};

/* VDD2 */
static struct omap_volt_data omap34xx_vdd2_volt_data[] = {
	{.volt_nominal = 975000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1050000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1150000, .sr_errminlimit = 0xF9, .vp_errgain = 0x18},
};

static struct omap_volt_data omap36xx_vdd2_volt_data[] = {
	{.volt_nominal = 930000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1137500, .sr_errminlimit = 0xF9, .vp_errgain = 0x16},
};

/*
 * Structures containing OMAP4430 voltage supported and various
 * data associated with it per voltage domain basis. Smartreflex Ntarget
 * values are left as 0 as they have to be populated by smartreflex
 * driver after reading the efuse.
 */
static struct omap_volt_data omap44xx_vdd_mpu_volt_data[] = {
	{.volt_nominal = 930000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1100000, .sr_errminlimit = 0xF9, .vp_errgain = 0x16},
	{.volt_nominal = 1260000, .sr_errminlimit = 0xFA, .vp_errgain = 0x23},
	{.volt_nominal = 1350000, .sr_errminlimit = 0xFA, .vp_errgain = 0x27},
};

static struct omap_volt_data omap44xx_vdd_iva_volt_data[] = {
	{.volt_nominal = 930000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1100000, .sr_errminlimit = 0xF9, .vp_errgain = 0x16},
	{.volt_nominal = 1260000, .sr_errminlimit = 0xFA, .vp_errgain = 0x23},
};

static struct omap_volt_data omap44xx_vdd_core_volt_data[] = {
	{.volt_nominal = 930000, .sr_errminlimit = 0xF4, .vp_errgain = 0x0C},
	{.volt_nominal = 1100000, .sr_errminlimit = 0xF9, .vp_errgain = 0x16},
};

/* By default VPFORCEUPDATE is the chosen method of voltage scaling */
static bool voltscale_vpforceupdate = true;

static inline u32 voltage_read_reg(u8 offset)
{
	return prm_read_mod_reg(volt_mod, offset);
}

static inline void voltage_write_reg(u8 offset, u32 value)
{
	prm_write_mod_reg(value, volt_mod, offset);
}

/* Voltage debugfs support */
#ifdef CONFIG_PM_DEBUG
static int vp_debug_get(void *data, u64 *val)
{
	u16 *option = data;

	if (!option) {
		pr_warning("Wrong paramater passed\n");
		return -EINVAL;
	}

	*val = *option;
	return 0;
}

static int vp_debug_set(void *data, u64 val)
{
	if (enable_sr_vp_debug) {
		u32 *option = data;
		if (!option) {
			pr_warning("Wrong paramater passed\n");
			return -EINVAL;
		}
		*option = val;
	} else {
		pr_notice("DEBUG option not enabled!"
			"echo 1 > pm_debug/enable_sr_vp_debug - to enable\n");
	}
	return 0;
}

static int vp_volt_debug_get(void *data, u64 *val)
{
	struct omap_vdd_info *vdd = (struct omap_vdd_info *) data;
	u8 vsel;

	if (!vdd) {
		pr_warning("Wrong paramater passed\n");
		return -EINVAL;
	}

	vsel = voltage_read_reg(vdd->vp_offs.voltage);
	pr_notice("curr_vsel = %x\n", vsel);
	*val = omap_twl_vsel_to_uv(vsel);

	return 0;
}

static int nom_volt_debug_get(void *data, u64 *val)
{
	struct omap_vdd_info *vdd = (struct omap_vdd_info *) data;

	if (!vdd) {
		pr_warning("Wrong paramater passed\n");
		return -EINVAL;
	}

	*val = omap_voltage_get_nom_volt(&vdd->voltdm);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vp_debug_fops, vp_debug_get, vp_debug_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(vp_volt_debug_fops, vp_volt_debug_get, NULL, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(nom_volt_debug_fops, nom_volt_debug_get, NULL,
								"%llu\n");
#endif

static void vp_latch_vsel(struct omap_vdd_info *vdd)
{
	u32 vpconfig;
	unsigned long uvdc;
	char vsel;

	uvdc = omap_voltage_get_nom_volt(&vdd->voltdm);
	if (!uvdc) {
		pr_warning("%s: unable to find current voltage for vdd_%s\n",
			__func__, vdd->voltdm.name);
		return;
	}
	vsel = omap_twl_uv_to_vsel(uvdc);
	vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
	vpconfig &= ~(vdd->vp_reg.vpconfig_initvoltage_mask |
			vdd->vp_reg.vpconfig_initvdd);
	vpconfig |= vsel << vdd->vp_reg.vpconfig_initvoltage_shift;

	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	/* Trigger initVDD value copy to voltage processor */
	voltage_write_reg(vdd->vp_offs.vpconfig,
			(vpconfig | vdd->vp_reg.vpconfig_initvdd));

	/* Clear initVDD copy trigger bit */
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);
}

/* OMAP3 specific voltage init functions */
/*
 * Intializes the voltage controller registers with the PMIC and board
 * specific parameters and voltage setup times for OMAP3. If the board
 * file does not populate the voltage controller parameters through
 * omap3_pm_init_vc, default values specified in vc_config is used.
 */
static void __init omap3_init_voltagecontroller(void)
{
	voltage_write_reg(OMAP3_PRM_VC_SMPS_SA_OFFSET,
			(OMAP3_SRI2C_SLAVE_ADDR <<
			 OMAP3430_PRM_VC_SMPS_SA_SA1_SHIFT) |
			(OMAP3_SRI2C_SLAVE_ADDR <<
			 OMAP3430_PRM_VC_SMPS_SA_SA0_SHIFT));
	voltage_write_reg(OMAP3_PRM_VC_SMPS_VOL_RA_OFFSET,
			(OMAP3_VDD2_SR_CONTROL_REG << OMAP3430_VOLRA1_SHIFT) |
			(OMAP3_VDD1_SR_CONTROL_REG << OMAP3430_VOLRA0_SHIFT));
	voltage_write_reg(OMAP3_PRM_VC_CMD_VAL_0_OFFSET,
			(vc_config.vdd0_on << OMAP3430_VC_CMD_ON_SHIFT) |
			(vc_config.vdd0_onlp << OMAP3430_VC_CMD_ONLP_SHIFT) |
			(vc_config.vdd0_ret << OMAP3430_VC_CMD_RET_SHIFT) |
			(vc_config.vdd0_off << OMAP3430_VC_CMD_OFF_SHIFT));
	voltage_write_reg(OMAP3_PRM_VC_CMD_VAL_1_OFFSET,
			(vc_config.vdd1_on << OMAP3430_VC_CMD_ON_SHIFT) |
			(vc_config.vdd1_onlp << OMAP3430_VC_CMD_ONLP_SHIFT) |
			(vc_config.vdd1_ret << OMAP3430_VC_CMD_RET_SHIFT) |
			(vc_config.vdd1_off << OMAP3430_VC_CMD_OFF_SHIFT));
	voltage_write_reg(OMAP3_PRM_VC_CH_CONF_OFFSET,
			OMAP3430_CMD1_MASK | OMAP3430_RAV1_MASK);
	voltage_write_reg(OMAP3_PRM_VC_I2C_CFG_OFFSET,
			OMAP3430_MCODE_SHIFT | OMAP3430_HSEN_MASK);
	/* Write setup times */
	voltage_write_reg(OMAP3_PRM_CLKSETUP_OFFSET, vc_config.clksetup);
	voltage_write_reg(OMAP3_PRM_VOLTSETUP1_OFFSET,
			(vc_config.voltsetup_time2 <<
			 OMAP3430_SETUP_TIME2_SHIFT) |
			(vc_config.voltsetup_time1 <<
			 OMAP3430_SETUP_TIME1_SHIFT));
	voltage_write_reg(OMAP3_PRM_VOLTOFFSET_OFFSET, vc_config.voltoffset);
	voltage_write_reg(OMAP3_PRM_VOLTSETUP2_OFFSET, vc_config.voltsetup2);
}

/* Sets up all the VDD related info for OMAP3 */
static void __init omap3_vdd_data_configure(struct omap_vdd_info *vdd)
{
	unsigned long curr_volt;
	struct omap_volt_data *volt_data;
	struct clk *sys_ck;
	u32 sys_clk_speed, timeout_val, waittime;

	if (!strcmp(vdd->voltdm.name, "mpu")) {
		if (cpu_is_omap3630()) {
			vdd->vp_reg.vlimitto_vddmin =
					OMAP3630_VP1_VLIMITTO_VDDMIN;
			vdd->vp_reg.vlimitto_vddmax =
					OMAP3630_VP1_VLIMITTO_VDDMAX;
			vdd->volt_data = omap36xx_vdd1_volt_data;
			vdd->volt_data_count =
					ARRAY_SIZE(omap36xx_vdd1_volt_data);
		} else {
			vdd->vp_reg.vlimitto_vddmin =
					OMAP3430_VP1_VLIMITTO_VDDMIN;
			vdd->vp_reg.vlimitto_vddmax =
					OMAP3430_VP1_VLIMITTO_VDDMAX;
			vdd->volt_data = omap34xx_vdd1_volt_data;
			vdd->volt_data_count =
					ARRAY_SIZE(omap34xx_vdd1_volt_data);
		}
		vdd->volt_clk = clk_get(NULL, "dpll1_ck");
		WARN(IS_ERR(vdd->volt_clk), "unable to get clock for vdd_%s\n",
				vdd->voltdm.name);
		vdd->opp_dev = omap2_get_mpuss_device();
		vdd->vp_reg.tranxdone_status = OMAP3430_VP1_TRANXDONE_ST_MASK;
		vdd->cmdval_reg = OMAP3_PRM_VC_CMD_VAL_0_OFFSET;
		vdd->vdd_sr_reg = OMAP3_VDD1_SR_CONTROL_REG;
	} else if (!strcmp(vdd->voltdm.name, "core")) {
		if (cpu_is_omap3630()) {
			vdd->vp_reg.vlimitto_vddmin =
					OMAP3630_VP2_VLIMITTO_VDDMIN;
			vdd->vp_reg.vlimitto_vddmax =
					OMAP3630_VP2_VLIMITTO_VDDMAX;
			vdd->volt_data = omap36xx_vdd2_volt_data;
			vdd->volt_data_count =
					ARRAY_SIZE(omap36xx_vdd2_volt_data);
		} else {
			vdd->vp_reg.vlimitto_vddmin =
					OMAP3430_VP2_VLIMITTO_VDDMIN;
			vdd->vp_reg.vlimitto_vddmax =
					OMAP3430_VP2_VLIMITTO_VDDMAX;
			vdd->volt_data = omap34xx_vdd2_volt_data;
			vdd->volt_data_count =
					ARRAY_SIZE(omap34xx_vdd2_volt_data);
		}
		vdd->volt_clk = clk_get(NULL, "l3_ick");
		WARN(IS_ERR(vdd->volt_clk), "unable to get clock for vdd_%s\n",
				vdd->voltdm.name);
		vdd->opp_dev = omap2_get_l3_device();
		vdd->vp_reg.tranxdone_status = OMAP3430_VP2_TRANXDONE_ST_MASK;
		vdd->cmdval_reg = OMAP3_PRM_VC_CMD_VAL_1_OFFSET;
		vdd->vdd_sr_reg = OMAP3_VDD2_SR_CONTROL_REG;
	} else {
		pr_warning("%s: vdd_%s does not exisit in OMAP3\n",
			__func__, vdd->voltdm.name);
		return;
	}

	curr_volt = omap_voltage_get_nom_volt(&vdd->voltdm);
	if (!curr_volt) {
		pr_warning("%s: unable to find current voltage for vdd_%s\n",
			__func__, vdd->voltdm.name);
		return;
	}

	volt_data = omap_voltage_get_voltdata(&vdd->voltdm, curr_volt);
	if (IS_ERR(volt_data)) {
		pr_warning("%s: Unable to get volt table for vdd_%s at init",
			__func__, vdd->voltdm.name);
		return;
	}
	/*
	 * Sys clk rate is require to calculate vp timeout value and
	 * smpswaittimemin and smpswaittimemax.
	 */
	sys_ck = clk_get(NULL, "sys_ck");
	if (IS_ERR(sys_ck)) {
		pr_warning("%s: Could not get the sys clk to calculate"
			"various vdd_%s params\n", __func__, vdd->voltdm.name);
		return;
	}
	sys_clk_speed = clk_get_rate(sys_ck);
	clk_put(sys_ck);
	/* Divide to avoid overflow */
	sys_clk_speed /= 1000;

	/* Nominal/Reset voltage of the VDD */
	vdd->nominal_volt = 1200000;

	/* VPCONFIG bit fields */
	vdd->vp_reg.vpconfig_erroroffset = (OMAP3_VP_CONFIG_ERROROFFSET <<
				 OMAP3430_ERROROFFSET_SHIFT);
	vdd->vp_reg.vpconfig_errorgain = volt_data->vp_errgain;
	vdd->vp_reg.vpconfig_errorgain_mask = OMAP3430_ERRORGAIN_MASK;
	vdd->vp_reg.vpconfig_errorgain_shift = OMAP3430_ERRORGAIN_SHIFT;
	vdd->vp_reg.vpconfig_initvoltage_shift = OMAP3430_INITVOLTAGE_SHIFT;
	vdd->vp_reg.vpconfig_initvoltage_mask = OMAP3430_INITVOLTAGE_MASK;
	vdd->vp_reg.vpconfig_timeouten = OMAP3430_TIMEOUTEN_MASK;
	vdd->vp_reg.vpconfig_initvdd = OMAP3430_INITVDD_MASK;
	vdd->vp_reg.vpconfig_forceupdate = OMAP3430_FORCEUPDATE_MASK;
	vdd->vp_reg.vpconfig_vpenable = OMAP3430_VPENABLE_MASK;

	/* VSTEPMIN VSTEPMAX bit fields */
	waittime = ((volt_pmic_info.step_size / volt_pmic_info.slew_rate) *
				sys_clk_speed) / 1000;
	vdd->vp_reg.vstepmin_smpswaittimemin = waittime;
	vdd->vp_reg.vstepmax_smpswaittimemax = waittime;
	vdd->vp_reg.vstepmin_stepmin = OMAP3_VP_VSTEPMIN_VSTEPMIN;
	vdd->vp_reg.vstepmax_stepmax = OMAP3_VP_VSTEPMAX_VSTEPMAX;
	vdd->vp_reg.vstepmin_smpswaittimemin_shift =
				OMAP3430_SMPSWAITTIMEMIN_SHIFT;
	vdd->vp_reg.vstepmax_smpswaittimemax_shift =
				OMAP3430_SMPSWAITTIMEMAX_SHIFT;
	vdd->vp_reg.vstepmin_stepmin_shift = OMAP3430_VSTEPMIN_SHIFT;
	vdd->vp_reg.vstepmax_stepmax_shift = OMAP3430_VSTEPMAX_SHIFT;

	/* VLIMITTO bit fields */
	timeout_val = (sys_clk_speed * OMAP3_VP_VLIMITTO_TIMEOUT_US) / 1000;
	vdd->vp_reg.vlimitto_timeout = timeout_val;
	vdd->vp_reg.vlimitto_vddmin_shift = OMAP3430_VDDMIN_SHIFT;
	vdd->vp_reg.vlimitto_vddmax_shift = OMAP3430_VDDMAX_SHIFT;
	vdd->vp_reg.vlimitto_timeout_shift = OMAP3430_TIMEOUT_SHIFT;
}

/* OMAP4 specific voltage init functions */
static void __init omap4_init_voltagecontroller(void)
{
	voltage_write_reg(OMAP4_PRM_VC_SMPS_SA_OFFSET,
			(OMAP4_SRI2C_SLAVE_ADDR <<
			 OMAP4430_SA_VDD_CORE_L_0_6_SHIFT) |
			(OMAP4_SRI2C_SLAVE_ADDR <<
			 OMAP4430_SA_VDD_IVA_L_PRM_VC_SMPS_SA_SHIFT) |
			(OMAP4_SRI2C_SLAVE_ADDR <<
			 OMAP4430_SA_VDD_MPU_L_PRM_VC_SMPS_SA_SHIFT));
	voltage_write_reg(OMAP4_PRM_VC_VAL_SMPS_RA_VOL_OFFSET,
			(OMAP4_VDD_MPU_SR_VOLT_REG <<
			 OMAP4430_VOLRA_VDD_MPU_L_SHIFT) |
			(OMAP4_VDD_IVA_SR_VOLT_REG <<
			 OMAP4430_VOLRA_VDD_IVA_L_SHIFT) |
			(OMAP4_VDD_CORE_SR_VOLT_REG <<
			 OMAP4430_VOLRA_VDD_CORE_L_SHIFT));
	voltage_write_reg(OMAP4_PRM_VC_CFG_CHANNEL_OFFSET,
			OMAP4430_RAV_VDD_MPU_L_MASK |
			OMAP4430_CMD_VDD_MPU_L_MASK |
			OMAP4430_RAV_VDD_IVA_L_MASK |
			OMAP4430_CMD_VDD_IVA_L_MASK |
			OMAP4430_RAV_VDD_CORE_L_MASK |
			OMAP4430_CMD_VDD_CORE_L_MASK);
	/*
	 * Configure SR I2C in HS Mode. Is there really a need to configure
	 * i2c in the normal mode??
	 */
/*	voltage_write_reg(OMAP4_PRM_VC_CFG_I2C_MODE_OFFSET,
			0x0 << OMAP4430_HSMCODE_SHIFT);
	voltage_write_reg(OMAP4_PRM_VC_CFG_I2C_CLK_OFFSET,
			(0x0A << OMAP4430_HSSCLL_SHIFT |
			0x05 << OMAP4430_HSSCLH_SHIFT));*/
	voltage_write_reg(OMAP4_PRM_VC_CFG_I2C_CLK_OFFSET,
			(0x60 << OMAP4430_SCLL_SHIFT |
			0x26 << OMAP4430_SCLH_SHIFT));
	/* TODO: Configure setup times and CMD_VAL values*/
}

/* Sets up all the VDD related info for OMAP4 */
static void __init omap4_vdd_data_configure(struct omap_vdd_info *vdd)
{
	unsigned long curr_volt;
	struct omap_volt_data *volt_data;
	struct clk *sys_ck;
	u32 sys_clk_speed, timeout_val, waittime;

	if (!strcmp(vdd->voltdm.name, "mpu")) {
		vdd->vp_reg.vlimitto_vddmin = OMAP4_VP_MPU_VLIMITTO_VDDMIN;
		vdd->vp_reg.vlimitto_vddmax = OMAP4_VP_MPU_VLIMITTO_VDDMAX;
		vdd->volt_data = omap44xx_vdd_mpu_volt_data;
		vdd->volt_data_count = ARRAY_SIZE(omap44xx_vdd_mpu_volt_data);
		vdd->volt_clk = clk_get(NULL, "dpll_mpu_ck");
		WARN(IS_ERR(vdd->volt_clk), "unable to get clock for vdd_%s\n",
				vdd->voltdm.name);
		vdd->opp_dev = omap2_get_mpuss_device();
		vdd->vp_reg.tranxdone_status =
				OMAP4430_VP_MPU_TRANXDONE_ST_MASK;
		vdd->cmdval_reg = OMAP4_PRM_VC_VAL_CMD_VDD_MPU_L_OFFSET;
		vdd->vdd_sr_reg = OMAP4_VDD_MPU_SR_VOLT_REG;
	} else if (!strcmp(vdd->voltdm.name, "core")) {
		vdd->vp_reg.vlimitto_vddmin = OMAP4_VP_CORE_VLIMITTO_VDDMIN;
		vdd->vp_reg.vlimitto_vddmax = OMAP4_VP_CORE_VLIMITTO_VDDMAX;
		vdd->volt_data = omap44xx_vdd_core_volt_data;
		vdd->volt_data_count = ARRAY_SIZE(omap44xx_vdd_core_volt_data);
		vdd->volt_clk = clk_get(NULL, "l3_div_ck");
		WARN(IS_ERR(vdd->volt_clk), "unable to get clock for vdd_%s\n",
				vdd->voltdm.name);
		vdd->opp_dev = omap2_get_l3_device();
		vdd->vp_reg.tranxdone_status =
				OMAP4430_VP_CORE_TRANXDONE_ST_MASK;
		vdd->cmdval_reg = OMAP4_PRM_VC_VAL_CMD_VDD_CORE_L_OFFSET;
		vdd->vdd_sr_reg = OMAP4_VDD_CORE_SR_VOLT_REG;
	} else if (!strcmp(vdd->voltdm.name, "iva")) {
		vdd->vp_reg.vlimitto_vddmin = OMAP4_VP_IVA_VLIMITTO_VDDMIN;
		vdd->vp_reg.vlimitto_vddmax = OMAP4_VP_IVA_VLIMITTO_VDDMAX;
		vdd->volt_data = omap44xx_vdd_iva_volt_data;
		vdd->volt_data_count = ARRAY_SIZE(omap44xx_vdd_iva_volt_data);
		vdd->volt_clk = clk_get(NULL, "dpll_iva_m5x2_ck");
		WARN(IS_ERR(vdd->volt_clk), "unable to get clock for vdd_%s\n",
				vdd->voltdm.name);
		vdd->opp_dev = omap2_get_iva_device();
		vdd->vp_reg.tranxdone_status =
			OMAP4430_VP_IVA_TRANXDONE_ST_MASK;
		vdd->cmdval_reg = OMAP4_PRM_VC_VAL_CMD_VDD_IVA_L_OFFSET;
		vdd->vdd_sr_reg = OMAP4_VDD_IVA_SR_VOLT_REG;
	} else {
		pr_warning("%s: vdd_%s does not exisit in OMAP4\n",
			__func__, vdd->voltdm.name);
		return;
	}

	curr_volt = omap_voltage_get_nom_volt(&vdd->voltdm);
	if (!curr_volt) {
		pr_warning("%s: unable to find current voltage for vdd_%s\n",
			__func__, vdd->voltdm.name);
		return;
	}

	volt_data = omap_voltage_get_voltdata(&vdd->voltdm, curr_volt);
	if (IS_ERR(volt_data)) {
		pr_warning("%s: Unable to get volt table for vdd_%s at init",
			__func__, vdd->voltdm.name);
		return;
	}
	/*
	 * Sys clk rate is require to calculate vp timeout value and
	 * smpswaittimemin and smpswaittimemax.
	 */
	sys_ck = clk_get(NULL, "sys_clkin_ck");
	if (IS_ERR(sys_ck)) {
		pr_warning("%s: Could not get the sys clk to calculate"
			"various vdd_%s params\n", __func__, vdd->voltdm.name);
		return;
	}
	sys_clk_speed = clk_get_rate(sys_ck);
	clk_put(sys_ck);
	/* Divide to avoid overflow */
	sys_clk_speed /= 1000;

	/* Nominal/Reset voltage of the VDD */
	vdd->nominal_volt = 1200000;

	/* VPCONFIG bit fields */
	vdd->vp_reg.vpconfig_erroroffset =
				(OMAP4_VP_CONFIG_ERROROFFSET <<
				 OMAP4430_ERROROFFSET_SHIFT);
	vdd->vp_reg.vpconfig_errorgain = volt_data->vp_errgain;
	vdd->vp_reg.vpconfig_errorgain_mask = OMAP4430_ERRORGAIN_MASK;
	vdd->vp_reg.vpconfig_errorgain_shift = OMAP4430_ERRORGAIN_SHIFT;
	vdd->vp_reg.vpconfig_initvoltage_shift = OMAP4430_INITVOLTAGE_SHIFT;
	vdd->vp_reg.vpconfig_initvoltage_mask = OMAP4430_INITVOLTAGE_MASK;
	vdd->vp_reg.vpconfig_timeouten = OMAP4430_TIMEOUTEN_MASK;
	vdd->vp_reg.vpconfig_initvdd = OMAP4430_INITVDD_MASK;
	vdd->vp_reg.vpconfig_forceupdate = OMAP4430_FORCEUPDATE_MASK;
	vdd->vp_reg.vpconfig_vpenable = OMAP4430_VPENABLE_MASK;

	/* VSTEPMIN VSTEPMAX bit fields */
	waittime = ((volt_pmic_info.step_size / volt_pmic_info.slew_rate) *
				sys_clk_speed) / 1000;
	vdd->vp_reg.vstepmin_smpswaittimemin = waittime;
	vdd->vp_reg.vstepmax_smpswaittimemax = waittime;
	vdd->vp_reg.vstepmin_stepmin = OMAP4_VP_VSTEPMIN_VSTEPMIN;
	vdd->vp_reg.vstepmax_stepmax = OMAP4_VP_VSTEPMAX_VSTEPMAX;
	vdd->vp_reg.vstepmin_smpswaittimemin_shift =
		OMAP4430_SMPSWAITTIMEMIN_SHIFT;
	vdd->vp_reg.vstepmax_smpswaittimemax_shift =
		OMAP4430_SMPSWAITTIMEMAX_SHIFT;
	vdd->vp_reg.vstepmin_stepmin_shift = OMAP4430_VSTEPMIN_SHIFT;
	vdd->vp_reg.vstepmax_stepmax_shift = OMAP4430_VSTEPMAX_SHIFT;

	/* VLIMITTO bit fields */
	timeout_val = (sys_clk_speed * OMAP4_VP_VLIMITTO_TIMEOUT_US) / 1000;
	vdd->vp_reg.vlimitto_timeout = timeout_val;
	vdd->vp_reg.vlimitto_vddmin_shift = OMAP4430_VDDMIN_SHIFT;
	vdd->vp_reg.vlimitto_vddmax_shift = OMAP4430_VDDMAX_SHIFT;
	vdd->vp_reg.vlimitto_timeout_shift = OMAP4430_TIMEOUT_SHIFT;
}

/* Generic voltage init functions */
static void __init init_voltageprocessor(struct omap_vdd_info *vdd)
{
	u32 vpconfig;

	vpconfig = vdd->vp_reg.vpconfig_erroroffset |
			(vdd->vp_reg.vpconfig_errorgain <<
			vdd->vp_reg.vpconfig_errorgain_shift) |
			vdd->vp_reg.vpconfig_timeouten;

	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	voltage_write_reg(vdd->vp_offs.vstepmin,
			(vdd->vp_reg.vstepmin_smpswaittimemin <<
			vdd->vp_reg.vstepmin_smpswaittimemin_shift) |
			(vdd->vp_reg.vstepmin_stepmin <<
			vdd->vp_reg.vstepmin_stepmin_shift));

	voltage_write_reg(vdd->vp_offs.vstepmax,
			(vdd->vp_reg.vstepmax_smpswaittimemax <<
			vdd->vp_reg.vstepmax_smpswaittimemax_shift) |
			(vdd->vp_reg.vstepmax_stepmax <<
			vdd->vp_reg.vstepmax_stepmax_shift));

	voltage_write_reg(vdd->vp_offs.vlimitto,
			(vdd->vp_reg.vlimitto_vddmax <<
			vdd->vp_reg.vlimitto_vddmax_shift) |
			(vdd->vp_reg.vlimitto_vddmin <<
			vdd->vp_reg.vlimitto_vddmin_shift) |
			(vdd->vp_reg.vlimitto_timeout <<
			vdd->vp_reg.vlimitto_timeout_shift));

	/* Set the init voltage */
	vp_latch_vsel(vdd);

	vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
	/* Force update of voltage */
	voltage_write_reg(vdd->vp_offs.vpconfig,
			(vpconfig | vdd->vp_reg.vpconfig_forceupdate));
	/* Clear force bit */
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);
}

static void __init vdd_data_configure(struct omap_vdd_info *vdd)
{
#ifdef CONFIG_PM_DEBUG
	struct dentry *vdd_debug;
	char name[16];
#endif

	if (cpu_is_omap34xx())
		omap3_vdd_data_configure(vdd);
	else if (cpu_is_omap44xx())
		omap4_vdd_data_configure(vdd);

	/* Init the plist */
	spin_lock_init(&vdd->user_lock);
	plist_head_init(&vdd->user_list, &vdd->user_lock);
	/* Init the DVFS mutex */
	mutex_init(&vdd->scaling_mutex);

#ifdef CONFIG_PM_DEBUG
	strcpy(name, "vdd_");
	strcat(name, vdd->voltdm.name);
	vdd_debug = debugfs_create_dir(name, voltage_dir);
	(void) debugfs_create_file("vp_errorgain", S_IRUGO | S_IWUGO,
				vdd_debug,
				&(vdd->vp_reg.vpconfig_errorgain),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_smpswaittimemin", S_IRUGO | S_IWUGO,
				vdd_debug,
				&(vdd->vp_reg.vstepmin_smpswaittimemin),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_stepmin", S_IRUGO | S_IWUGO, vdd_debug,
				&(vdd->vp_reg.vstepmin_stepmin),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_smpswaittimemax", S_IRUGO | S_IWUGO,
				vdd_debug,
				&(vdd->vp_reg.vstepmax_smpswaittimemax),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_stepmax", S_IRUGO | S_IWUGO, vdd_debug,
				&(vdd->vp_reg.vstepmax_stepmax),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_vddmax", S_IRUGO | S_IWUGO, vdd_debug,
				&(vdd->vp_reg.vlimitto_vddmax),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_vddmin", S_IRUGO | S_IWUGO, vdd_debug,
				&(vdd->vp_reg.vlimitto_vddmin),
				&vp_debug_fops);
	(void) debugfs_create_file("vp_timeout", S_IRUGO | S_IWUGO, vdd_debug,
				&(vdd->vp_reg.vlimitto_timeout),
				&vp_debug_fops);
	(void) debugfs_create_file("curr_vp_volt", S_IRUGO, vdd_debug,
				(void *) vdd, &vp_volt_debug_fops);
	(void) debugfs_create_file("curr_nominal_volt", S_IRUGO, vdd_debug,
				(void *) vdd, &nom_volt_debug_fops);
#endif
}
static void __init init_voltagecontroller(void)
{
	if (cpu_is_omap34xx())
		omap3_init_voltagecontroller();
	else if (cpu_is_omap44xx())
		omap4_init_voltagecontroller();
}

/*
 * vc_bypass_scale_voltage - VC bypass method of voltage scaling
 */
static int vc_bypass_scale_voltage(struct omap_vdd_info *vdd,
		unsigned long target_volt)
{
	struct omap_volt_data *volt_data;
	u32 vc_bypass_value, vc_cmdval, vc_valid, vc_bypass_val_reg_offs;
	u32 vp_errgain_val, vc_cmd_on_mask;
	u32 loop_cnt = 0, retries_cnt = 0;
	u32 smps_steps = 0, smps_delay = 0;
	u8 vc_data_shift, vc_slaveaddr_shift, vc_regaddr_shift;
	u8 vc_cmd_on_shift;
	u8 target_vsel, current_vsel, sr_i2c_slave_addr;

	if (cpu_is_omap34xx()) {
		vc_cmd_on_shift = OMAP3430_VC_CMD_ON_SHIFT;
		vc_cmd_on_mask = OMAP3430_VC_CMD_ON_MASK;
		vc_data_shift = OMAP3430_DATA_SHIFT;
		vc_slaveaddr_shift = OMAP3430_SLAVEADDR_SHIFT;
		vc_regaddr_shift = OMAP3430_REGADDR_SHIFT;
		vc_valid = OMAP3430_VALID_MASK;
		vc_bypass_val_reg_offs = OMAP3_PRM_VC_BYPASS_VAL_OFFSET;
		sr_i2c_slave_addr = OMAP3_SRI2C_SLAVE_ADDR;
	} else {
		pr_warning("%s: vc bypass method of voltage scaling"
			"not supported\n", __func__);
		return -EINVAL;
	}

	/* Get volt_data corresponding to target_volt */
	volt_data = omap_voltage_get_voltdata(&vdd->voltdm, target_volt);
	if (IS_ERR(volt_data)) {
		/*
		 * If a match is not found but the target voltage is
		 * is the nominal vdd voltage allow scaling
		 */
		if (target_volt != vdd->nominal_volt) {
			pr_warning("%s: Unable to get volt table for vdd_%s"
				"during voltage scaling. Some really Wrong!",
				__func__, vdd->voltdm.name);
			return -ENODATA;
		}
		volt_data = NULL;
	}

	target_vsel = omap_twl_uv_to_vsel(target_volt);
	current_vsel = voltage_read_reg(vdd->vp_offs.voltage);
	smps_steps = abs(target_vsel - current_vsel);

	/* Setting the ON voltage to the new target voltage */
	vc_cmdval = voltage_read_reg(vdd->cmdval_reg);
	vc_cmdval &= ~vc_cmd_on_mask;
	vc_cmdval |= (target_vsel << vc_cmd_on_shift);
	voltage_write_reg(vdd->cmdval_reg, vc_cmdval);

	/*
	 * Setting vp errorgain based on the voltage. If the debug option is
	 * enabled allow the override of errorgain from user side
	 */
	if (!enable_sr_vp_debug && volt_data) {
		vp_errgain_val = voltage_read_reg(vdd->vp_offs.vpconfig);
		vdd->vp_reg.vpconfig_errorgain = volt_data->vp_errgain;
		vp_errgain_val &= ~vdd->vp_reg.vpconfig_errorgain_mask;
		vp_errgain_val |= vdd->vp_reg.vpconfig_errorgain <<
				vdd->vp_reg.vpconfig_errorgain_shift;
		voltage_write_reg(vdd->vp_offs.vpconfig, vp_errgain_val);
	}

	vc_bypass_value = (target_vsel << vc_data_shift) |
			(vdd->vdd_sr_reg << vc_regaddr_shift) |
			(sr_i2c_slave_addr << vc_slaveaddr_shift);

	voltage_write_reg(vc_bypass_val_reg_offs, vc_bypass_value);

	voltage_write_reg(vc_bypass_val_reg_offs, vc_bypass_value | vc_valid);
	vc_bypass_value = voltage_read_reg(vc_bypass_val_reg_offs);

	while ((vc_bypass_value & vc_valid) != 0x0) {
		loop_cnt++;
		if (retries_cnt > 10) {
			pr_warning("%s: Loop count exceeded in check SR I2C"
				"write during voltgae scaling\n", __func__);
			return -ETIMEDOUT;
		}
		if (loop_cnt > 50) {
			retries_cnt++;
			loop_cnt = 0;
			udelay(10);
		}
		vc_bypass_value = voltage_read_reg(vc_bypass_val_reg_offs);
	}

	/* SMPS slew rate / step size. 2us added as buffer. */
	smps_delay = ((smps_steps * volt_pmic_info.step_size) /
			volt_pmic_info.slew_rate) + 2;
	udelay(smps_delay);
	return 0;
}

/* VP force update method of voltage scaling */
static int vp_forceupdate_scale_voltage(struct omap_vdd_info *vdd,
		unsigned long target_volt)
{
	struct omap_volt_data *volt_data;
	u32 vc_cmd_on_mask, vc_cmdval, vpconfig;
	u32 smps_steps = 0, smps_delay = 0;
	int timeout = 0;
	u8 target_vsel, current_vsel;
	u8 vc_cmd_on_shift;
	u8 prm_irqst_reg_offs, ocp_mod;

	if (cpu_is_omap34xx()) {
		vc_cmd_on_shift = OMAP3430_VC_CMD_ON_SHIFT;
		vc_cmd_on_mask = OMAP3430_VC_CMD_ON_MASK;
		prm_irqst_reg_offs = OMAP3_PRM_IRQSTATUS_MPU_OFFSET;
		ocp_mod = OCP_MOD;
	} else if (cpu_is_omap44xx()) {
		vc_cmd_on_shift = OMAP4430_ON_SHIFT;
		vc_cmd_on_mask = OMAP4430_ON_MASK;
		if (!strcmp(vdd->voltdm.name, "mpu"))
			prm_irqst_reg_offs = OMAP4_PRM_IRQSTATUS_MPU_2_OFFSET;
		else
			prm_irqst_reg_offs = OMAP4_PRM_IRQSTATUS_MPU_OFFSET;
		ocp_mod = OMAP4430_PRM_OCP_SOCKET_MOD;
	}

	/* Get volt_data corresponding to the target_volt */
	volt_data = omap_voltage_get_voltdata(&vdd->voltdm, target_volt);
	if (IS_ERR(volt_data)) {
		/*
		 * If a match is not found but the target voltage is
		 * is the nominal vdd voltage allow scaling
		 */
		if (target_volt != vdd->nominal_volt) {
			pr_warning("%s: Unable to get voltage table for vdd_%s"
				"during voltage scaling. Some really Wrong!",
				__func__, vdd->voltdm.name);
			return -ENODATA;
		}
		volt_data = NULL;
	}

	target_vsel = omap_twl_uv_to_vsel(target_volt);
	current_vsel = voltage_read_reg(vdd->vp_offs.voltage);
	smps_steps = abs(target_vsel - current_vsel);

	/* Setting the ON voltage to the new target voltage */
	vc_cmdval = voltage_read_reg(vdd->cmdval_reg);
	vc_cmdval &= ~vc_cmd_on_mask;
	vc_cmdval |= (target_vsel << vc_cmd_on_shift);
	voltage_write_reg(vdd->cmdval_reg, vc_cmdval);

	/*
	 * Getting  vp errorgain based on the voltage. If the debug option is
	 * enabled allow the override of errorgain from user side.
	 */
	if (!enable_sr_vp_debug && volt_data)
		vdd->vp_reg.vpconfig_errorgain =
					volt_data->vp_errgain;
	/*
	 * Clear all pending TransactionDone interrupt/status. Typical latency
	 * is <3us
	 */
	while (timeout++ < VP_TRANXDONE_TIMEOUT) {
		prm_write_mod_reg(vdd->vp_reg.tranxdone_status,
				ocp_mod, prm_irqst_reg_offs);
		if (!(prm_read_mod_reg(ocp_mod, prm_irqst_reg_offs) &
				vdd->vp_reg.tranxdone_status))
				break;
		udelay(1);
	}

	if (timeout >= VP_TRANXDONE_TIMEOUT) {
		pr_warning("%s: vdd_%s TRANXDONE timeout exceeded."
			"Voltage change aborted", __func__, vdd->voltdm.name);
		return -ETIMEDOUT;
	}
	/* Configure for VP-Force Update */
	vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
	vpconfig &= ~(vdd->vp_reg.vpconfig_initvdd |
			vdd->vp_reg.vpconfig_forceupdate |
			vdd->vp_reg.vpconfig_initvoltage_mask |
			vdd->vp_reg.vpconfig_errorgain_mask);
	vpconfig |= ((target_vsel <<
			vdd->vp_reg.vpconfig_initvoltage_shift) |
			(vdd->vp_reg.vpconfig_errorgain <<
			 vdd->vp_reg.vpconfig_errorgain_shift));
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	/* Trigger initVDD value copy to voltage processor */
	vpconfig |= vdd->vp_reg.vpconfig_initvdd;
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	/* Force update of voltage */
	vpconfig |= vdd->vp_reg.vpconfig_forceupdate;
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	timeout = 0;
	/*
	 * Wait for TransactionDone. Typical latency is <200us.
	 * Depends on SMPSWAITTIMEMIN/MAX and voltage change
	 */
	omap_test_timeout((prm_read_mod_reg(ocp_mod, prm_irqst_reg_offs) &
			vdd->vp_reg.tranxdone_status),
			VP_TRANXDONE_TIMEOUT, timeout);

	if (timeout >= VP_TRANXDONE_TIMEOUT)
		pr_err("%s: vdd_%s TRANXDONE timeout exceeded."
			"TRANXDONE never got set after the voltage update\n",
			__func__, vdd->voltdm.name);
	/*
	 * Wait for voltage to settle with SW wait-loop.
	 * SMPS slew rate / step size. 2us added as buffer.
	 */
	smps_delay = ((smps_steps * volt_pmic_info.step_size) /
			volt_pmic_info.slew_rate) + 2;
	udelay(smps_delay);

	/*
	 * Disable TransactionDone interrupt , clear all status, clear
	 * control registers
	 */
	timeout = 0;
	while (timeout++ < VP_TRANXDONE_TIMEOUT) {
		prm_write_mod_reg(vdd->vp_reg.tranxdone_status,
				ocp_mod, prm_irqst_reg_offs);
		if (!(prm_read_mod_reg(ocp_mod, prm_irqst_reg_offs) &
				vdd->vp_reg.tranxdone_status))
				break;
		udelay(1);
	}
	if (timeout >= VP_TRANXDONE_TIMEOUT)
		pr_warning("%s: vdd_%s TRANXDONE timeout exceeded while trying"
			"to clear the TRANXDONE status\n",
			__func__, vdd->voltdm.name);

	vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
	/* Clear initVDD copy trigger bit */
	vpconfig &= ~vdd->vp_reg.vpconfig_initvdd;;
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);
	/* Clear force bit */
	vpconfig &= ~vdd->vp_reg.vpconfig_forceupdate;
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	return 0;
}

/* Public functions */
/**
 * omap_voltage_get_nom_volt : Gets the current non-auto-compensated voltage
 * @voltdm	: pointer to the VDD for which current voltage info is needed
 *
 * API to get the current non-auto-compensated voltage for a VDD.
 * Returns 0 in case of error else returns the current voltage for the VDD.
 */
unsigned long omap_voltage_get_nom_volt(struct voltagedomain *voltdm)
{
	struct omap_opp *opp;
	struct omap_vdd_info *vdd;
	unsigned long freq;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return 0;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	freq = vdd->volt_clk->rate;
	opp = opp_find_freq_ceil(vdd->opp_dev, &freq);
	if (IS_ERR(opp)) {
		pr_warning("%s: Unable to find OPP for vdd_%s freq%ld\n",
			__func__, voltdm->name, freq);
		return 0;
	}

	/*
	 * Use higher freq voltage even if an exact match is not available
	 * we are probably masking a clock framework bug, so warn
	 */
	if (unlikely(freq != vdd->volt_clk->rate))
		pr_warning("%s: Available freq %ld != dpll freq %ld.\n",
			__func__, freq, vdd->volt_clk->rate);

	return opp_get_voltage(opp);
}

/**
 * omap_vp_get_curr_volt : API to get the current vp voltage.
 * @voltdm: pointer to the VDD.
 *
 * This API returns the current voltage for the specified voltage processor
 */
unsigned long omap_vp_get_curr_volt(struct voltagedomain *voltdm)
{
	struct omap_vdd_info *vdd;
	u8 curr_vsel;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return 0;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	curr_vsel = voltage_read_reg(vdd->vp_offs.voltage);
	return omap_twl_vsel_to_uv(curr_vsel);
}

/**
 * omap_voltage_add_userreq : API to keep track of various requests to
 *			    scale the VDD and returns the best possible
 *			    voltage the VDD can be put to.
 * @volt_domain: pointer to the voltage domain.
 * @dev : the device pointer.
 * @volt : the voltage which is requested by the device.
 *
 * This API is to be called before the actual voltage scaling is
 * done to determine what is the best possible voltage the VDD can
 * be put to. This API adds the device <dev> in the user list of the
 * vdd <volt_domain> with <volt> as the requested voltage. The user list
 * is a plist with the priority element absolute voltage values.
 * The API then finds the maximum of all the requested voltages for
 * the VDD and returns it back through <volt> pointer itself.
 * Returns error value in case of any errors.
 */
int omap_voltage_add_userreq(struct voltagedomain *voltdm, struct device *dev,
		unsigned long *volt)
{
	struct omap_vdd_info *vdd;
	struct omap_vdd_user_list *user;
	struct plist_node *node;
	int found = 0;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return -EINVAL;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	mutex_lock(&vdd->scaling_mutex);

	plist_for_each_entry(user, &vdd->user_list, node) {
		if (user->dev == dev) {
			found = 1;
			break;
		}
	}

	if (!found) {
		user = kzalloc(sizeof(struct omap_vdd_user_list), GFP_KERNEL);
		if (!user) {
			pr_err("%s: Unable to creat a new user for vdd_%s\n",
				__func__, voltdm->name);
			mutex_unlock(&vdd->scaling_mutex);
			return -ENOMEM;
		}
		user->dev = dev;
	} else {
		plist_del(&user->node, &vdd->user_list);
	}

	plist_node_init(&user->node, *volt);
	plist_add(&user->node, &vdd->user_list);
	node = plist_first(&vdd->user_list);
	*volt = node->prio;

	mutex_unlock(&vdd->scaling_mutex);

	return 0;
}

/**
 * omap_vp_enable : API to enable a particular VP
 * @voltdm: pointer to the VDD whose VP is to be enabled.
 *
 * This API enables a particular voltage processor. Needed by the smartreflex
 * class drivers.
 */
void omap_vp_enable(struct voltagedomain *voltdm)
{
	struct omap_vdd_info *vdd;
	u32 vpconfig;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	/* If VP is already enabled, do nothing. Return */
	if (voltage_read_reg(vdd->vp_offs.vpconfig) &
				vdd->vp_reg.vpconfig_vpenable)
		return;
	/*
	 * This latching is required only if VC bypass method is used for
	 * voltage scaling during dvfs.
	 */
	if (!voltscale_vpforceupdate)
		vp_latch_vsel(vdd);

	/*
	 * If debug is enabled, it is likely that the following parameters
	 * were set from user space so rewrite them.
	 */
	if (enable_sr_vp_debug) {
		vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
		vpconfig |= (vdd->vp_reg.vpconfig_errorgain <<
			vdd->vp_reg.vpconfig_errorgain_shift);
		voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

		voltage_write_reg(vdd->vp_offs.vstepmin,
			(vdd->vp_reg.vstepmin_smpswaittimemin <<
			vdd->vp_reg.vstepmin_smpswaittimemin_shift) |
			(vdd->vp_reg.vstepmin_stepmin <<
			vdd->vp_reg.vstepmin_stepmin_shift));

		voltage_write_reg(vdd->vp_offs.vstepmax,
			(vdd->vp_reg.vstepmax_smpswaittimemax <<
			vdd->vp_reg.vstepmax_smpswaittimemax_shift) |
			(vdd->vp_reg.vstepmax_stepmax <<
			vdd->vp_reg.vstepmax_stepmax_shift));

		voltage_write_reg(vdd->vp_offs.vlimitto,
			(vdd->vp_reg.vlimitto_vddmax <<
			vdd->vp_reg.vlimitto_vddmax_shift) |
			(vdd->vp_reg.vlimitto_vddmin <<
			vdd->vp_reg.vlimitto_vddmin_shift) |
			(vdd->vp_reg.vlimitto_timeout <<
			vdd->vp_reg.vlimitto_timeout_shift));
	}

	vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
	/* Enable VP */
	voltage_write_reg(vdd->vp_offs.vpconfig,
				vpconfig | vdd->vp_reg.vpconfig_vpenable);
}

/**
 * omap_vp_disable : API to disable a particular VP
 * @voltdm: pointer to the VDD whose VP is to be disabled.
 *
 * This API disables a particular voltage processor. Needed by the smartreflex
 * class drivers.
 */
void omap_vp_disable(struct voltagedomain *voltdm)
{
	struct omap_vdd_info *vdd;
	u32 vpconfig;
	int timeout;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	/* If VP is already disabled, do nothing. Return */
	if (!(voltage_read_reg(vdd->vp_offs.vpconfig) &
				vdd->vp_reg.vpconfig_vpenable)) {
		pr_warning("%s: Trying to disable VP for vdd_%s when"
			"it is already disabled\n", __func__, voltdm->name);
		return;
	}

	/* Disable VP */
	vpconfig = voltage_read_reg(vdd->vp_offs.vpconfig);
	vpconfig &= ~vdd->vp_reg.vpconfig_vpenable;
	voltage_write_reg(vdd->vp_offs.vpconfig, vpconfig);

	/*
	 * Wait for VP idle Typical latency is <2us. Maximum latency is ~100us
	 */
	omap_test_timeout((voltage_read_reg(vdd->vp_offs.vstatus)),
				VP_IDLE_TIMEOUT, timeout);

	if (timeout >= VP_IDLE_TIMEOUT)
		pr_warning("%s: vdd_%s idle timedout\n",
			__func__, voltdm->name);
	return;
}

/**
 * omap_voltage_scale_vdd : API to scale voltage of a particular voltage domain.
 * @voltdm: pointer to the VDD which is to be scaled.
 * @target_volt : The target voltage of the voltage domain
 *
 * This API should be called by the kernel to do the voltage scaling
 * for a particular voltage domain during dvfs or any other situation.
 */
int omap_voltage_scale_vdd(struct voltagedomain *voltdm,
		unsigned long target_volt)
{
	struct omap_vdd_info *vdd;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return -EINVAL;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	if (voltscale_vpforceupdate)
		return vp_forceupdate_scale_voltage(vdd, target_volt);
	else
		return vc_bypass_scale_voltage(vdd, target_volt);
}



/**
 * omap_voltage_reset : Resets the voltage of a particular voltage domain
 * to that of the current OPP.
 * @voltdm: pointer to the VDD whose voltage is to be reset.
 *
 * This API finds out the correct voltage the voltage domain is supposed
 * to be at and resets the voltage to that level. Should be used expecially
 * while disabling any voltage compensation modules.
 */
void omap_voltage_reset(struct voltagedomain *voltdm)
{
	unsigned long target_uvdc;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return;
	}

	target_uvdc = omap_voltage_get_nom_volt(voltdm);
	if (!target_uvdc) {
		pr_err("%s: unable to find current voltage for vdd_%s\n",
			__func__, voltdm->name);
		return;
	}
	omap_voltage_scale_vdd(voltdm, target_uvdc);
}

/**
 * omap_change_voltscale_method : API to change the voltage scaling method.
 * @voltscale_method : the method to be used for voltage scaling.
 *
 * This API can be used by the board files to change the method of voltage
 * scaling between vpforceupdate and vcbypass. The parameter values are
 * defined in voltage.h
 */
void omap_change_voltscale_method(int voltscale_method)
{
	switch (voltscale_method) {
	case VOLTSCALE_VPFORCEUPDATE:
		voltscale_vpforceupdate = true;
		return;
	case VOLTSCALE_VCBYPASS:
		voltscale_vpforceupdate = false;
		return;
	default:
		pr_warning("%s: Trying to change the method of voltage scaling"
			"to an unsupported one!\n", __func__);
	}
}

/**
 * omap_voltage_init_vc - polpulates vc_config with values specified in
 *			  board file
 * @setup_vc - the structure with various vc parameters
 *
 * Updates vc_config with the voltage setup times and other parameters as
 * specified in setup_vc. vc_config is later used in init_voltagecontroller
 * to initialize the voltage controller registers. Board files should call
 * this function with the correct volatge settings corresponding
 * the particular PMIC and chip.
 */
void __init omap_voltage_init_vc(struct omap_volt_vc_data *setup_vc)
{
	if (!setup_vc)
		return;

	vc_config.clksetup = setup_vc->clksetup;
	vc_config.voltsetup_time1 = setup_vc->voltsetup_time1;
	vc_config.voltsetup_time2 = setup_vc->voltsetup_time2;
	vc_config.voltoffset = setup_vc->voltoffset;
	vc_config.voltsetup2 = setup_vc->voltsetup2;
	vc_config.vdd0_on = setup_vc->vdd0_on;
	vc_config.vdd0_onlp = setup_vc->vdd0_onlp;
	vc_config.vdd0_ret = setup_vc->vdd0_ret;
	vc_config.vdd0_off = setup_vc->vdd0_off;
	vc_config.vdd1_on = setup_vc->vdd1_on;
	vc_config.vdd1_onlp = setup_vc->vdd1_onlp;
	vc_config.vdd1_ret = setup_vc->vdd1_ret;
	vc_config.vdd1_off = setup_vc->vdd1_off;
}

/**
 * omap_voltage_get_volttable : API to get the voltage table associated with a
 *			    particular voltage domain.
 *
 * @voltdm: pointer to the VDD for which the voltage table is required
 * @volt_data : the voltage table for the particular vdd which is to be
 *		populated by this API
 * This API populates the voltage table associated with a VDD into the
 * passed parameter pointer. Returns the count of distinct voltages
 * supported by this vdd.
 *
 */
int omap_voltage_get_volttable(struct voltagedomain *voltdm,
		struct omap_volt_data **volt_data)
{
	struct omap_vdd_info *vdd;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return 0;
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	*volt_data = vdd->volt_data;
	return vdd->volt_data_count;
}

/**
 * omap_voltage_get_voltdata : API to get the voltage table entry for a
 *				particular voltage
 * @voltdm: pointer to the VDD whose voltage table has to be searched
 * @volt : the voltage to be searched in the voltage table
 *
 * This API searches through the voltage table for the required voltage
 * domain and tries to find a matching entry for the passed voltage volt.
 * If a matching entry is found volt_data is populated with that entry.
 * This API searches only through the non-compensated voltages int the
 * voltage table.
 * Returns pointer to the voltage table entry corresponding to volt on
 * sucess. Returns -ENODATA if no voltage table exisits for the passed voltage
 * domain or if there is no matching entry.
 */
struct omap_volt_data *omap_voltage_get_voltdata(struct voltagedomain *voltdm,
		unsigned long volt)
{
	struct omap_vdd_info *vdd;
	int i;

	if (!voltdm || IS_ERR(voltdm)) {
		pr_warning("%s: VDD specified does not exist!\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	vdd = container_of(voltdm, struct omap_vdd_info, voltdm);

	if (!vdd->volt_data) {
		pr_warning("%s: voltage table does not exist for vdd_%s\n",
			__func__, voltdm->name);
		return ERR_PTR(-ENODATA);
	}

	for (i = 0; i < vdd->volt_data_count; i++) {
		if (vdd->volt_data[i].volt_nominal == volt)
			return &vdd->volt_data[i];
	}

	pr_notice("%s: Unable to match the current voltage with the voltage"
		"table for vdd_%s\n", __func__, voltdm->name);

	return ERR_PTR(-ENODATA);
}

/**
 * omap_voltage_register_pmic : API to register PMIC specific data
 * @pmic_info : the structure containing pmic info
 *
 * This API is to be called by the borad file to specify the pmic specific
 * info as present in omap_volt_pmic_info structure. A default pmic info
 * table is maintained in the driver volt_pmic_info. If the board file do
 * not override the default table using this API, the default values wiil
 * be used in the driver.
 */
void omap_voltage_register_pmic(struct omap_volt_pmic_info *pmic_info)
{
	volt_pmic_info.slew_rate = pmic_info->slew_rate;
	volt_pmic_info.step_size = pmic_info->step_size;
}

/**
 * omap_voltage_domain_get	: API to get the voltage domain pointer
 * @name : Name of the voltage domain
 *
 * This API looks up in the global vdd_info struct for the
 * existence of voltage domain <name>. If it exists, the API returns
 * a pointer to the voltage domain structure corresponding to the
 * VDD<name>. Else retuns error pointer.
 */
struct voltagedomain *omap_voltage_domain_get(char *name)
{
	int i;

	if (!vdd_info) {
		pr_err("%s: Voltage driver init not yet happened.Faulting!\n",
			__func__);
		return ERR_PTR(-EINVAL);
	}

	if (!name) {
		pr_err("%s: No name to get the votage domain!\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	for (i = 0; i < no_scalable_vdd; i++) {
		if (!(strcmp(name, vdd_info[i].voltdm.name)))
			return &vdd_info[i].voltdm;
	}

	return ERR_PTR(-EINVAL);
}

/**
 * omap_voltage_init : Volatage init API which does VP and VC init.
 */
static int __init omap_voltage_init(void)
{
	int i;

	if (!(cpu_is_omap34xx() || cpu_is_omap44xx())) {
		pr_warning("%s: voltage driver support not added\n", __func__);
		return 0;
	}

#ifdef CONFIG_PM_DEBUG
	voltage_dir = debugfs_create_dir("voltage", pm_dbg_main_dir);
#endif
	if (cpu_is_omap34xx()) {
		volt_mod = OMAP3430_GR_MOD;
		vdd_info = omap3_vdd_info;
		no_scalable_vdd = OMAP3_NO_SCALABLE_VDD;
	} else if (cpu_is_omap44xx()) {
		volt_mod = OMAP4430_PRM_DEVICE_MOD;
		vdd_info = omap4_vdd_info;
		no_scalable_vdd = OMAP4_NO_SCALABLE_VDD;
	}
	init_voltagecontroller();
	for (i = 0; i < no_scalable_vdd; i++) {
		vdd_data_configure(&vdd_info[i]);
		init_voltageprocessor(&vdd_info[i]);
	}
	return 0;
}
device_initcall(omap_voltage_init);
