/*
 * OMAP5 Voltage Management Routines
 *
 * Based on voltagedomains44xx_data.c
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/init.h>

#include "common.h"

#include "prm54xx.h"
#include "voltage.h"
#include "omap_opp_data.h"
#include "vc.h"
#include "vp.h"

static const struct omap_vfsm_instance omap5_vdd_mpu_vfsm = {
	.voltsetup_reg = OMAP54XX_PRM_VOLTSETUP_MPU_RET_SLEEP_OFFSET,
	.voltsetup_off_reg = OMAP54XX_PRM_VOLTSETUP_MPU_OFF_OFFSET,
};

static struct omap_vdd_info omap5_vdd_mpu_info;

static const struct omap_vfsm_instance omap5_vdd_mm_vfsm = {
	.voltsetup_reg = OMAP54XX_PRM_VOLTSETUP_MM_RET_SLEEP_OFFSET,
	.voltsetup_off_reg = OMAP54XX_PRM_VOLTSETUP_MM_OFF_OFFSET,
};
static struct omap_vdd_info omap5_vdd_mm_info;

static const struct omap_vfsm_instance omap5_vdd_core_vfsm = {
	.voltsetup_reg = OMAP54XX_PRM_VOLTSETUP_CORE_RET_SLEEP_OFFSET,
	.voltsetup_off_reg = OMAP54XX_PRM_VOLTSETUP_CORE_OFF_OFFSET,
};
static struct omap_vdd_info omap5_vdd_core_info;

static struct voltagedomain omap5_voltdm_mpu = {
	.name = "mpu",
	.scalable = true,
	.read = omap4_prm_vcvp_read,
	.write = omap4_prm_vcvp_write,
	.rmw = omap4_prm_vcvp_rmw,
	.vc = &omap5_vc_mpu,
	.vfsm = &omap5_vdd_mpu_vfsm,
	.vp = &omap5_vp_mpu,
	.vdd = &omap5_vdd_mpu_info,
	.auto_ret = true,
};

static struct voltagedomain omap5_voltdm_mm = {
	.name = "mm",
	.scalable = true,
	.read = omap4_prm_vcvp_read,
	.write = omap4_prm_vcvp_write,
	.rmw = omap4_prm_vcvp_rmw,
	.vc = &omap5_vc_mm,
	.vfsm = &omap5_vdd_mm_vfsm,
	.vp = &omap5_vp_mm,
	.vdd = &omap5_vdd_mm_info,
	/* disable auto_ret for mm domain due to known issues */
	.auto_ret = false,
};

static struct voltagedomain omap5_voltdm_core = {
	.name = "core",
	.scalable = true,
	.read = omap4_prm_vcvp_read,
	.write = omap4_prm_vcvp_write,
	.rmw = omap4_prm_vcvp_rmw,
	.vc = &omap5_vc_core,
	.vfsm = &omap5_vdd_core_vfsm,
	.vp = &omap5_vp_core,
	.vdd = &omap5_vdd_core_info,
	.auto_ret = true,
};

static struct voltagedomain omap5_voltdm_wkup = {
	.name = "wkup",
};

static struct voltagedomain *voltagedomains_omap5[] __initdata = {
	&omap5_voltdm_mpu,
	&omap5_voltdm_mm,
	&omap5_voltdm_core,
	&omap5_voltdm_wkup,
	NULL,
};

static const char *sys_clk_name __initdata = "sys_clkin";

void __init omap54xx_voltagedomains_init(void)
{
	struct voltagedomain *voltdm;
	int i;

	/*
	 * XXX Will depend on the process, validation, and binning
	 * for the currently-running IC
	 */
#ifdef CONFIG_PM_OPP
	if (omap_rev() == OMAP5430_REV_ES1_0) {
		omap5_voltdm_mpu.volt_data = omap5430_vdd_mpu_volt_data;
		omap5_vdd_mpu_info.dep_vdd_info = omap5430_vddmpu_dep_info;

		omap5_voltdm_mm.volt_data = omap5430_vdd_mm_volt_data;
		omap5_vdd_mm_info.dep_vdd_info = omap5430_vddmm_dep_info;

		omap5_voltdm_core.volt_data = omap5430_vdd_core_volt_data;
	} else if (omap_rev() == OMAP5432_REV_ES1_0) {
		omap5_voltdm_mpu.volt_data = omap5432_vdd_mpu_volt_data;
		omap5_vdd_mpu_info.dep_vdd_info = omap5432_vddmpu_dep_info;

		omap5_voltdm_mm.volt_data = omap5432_vdd_mm_volt_data;
		omap5_vdd_mm_info.dep_vdd_info = omap5432_vddmm_dep_info;

		omap5_voltdm_core.volt_data = omap5432_vdd_core_volt_data;
	}
#endif

	omap5_voltdm_mpu.vp_param = &omap5_mpu_vp_data;
	omap5_voltdm_mm.vp_param = &omap5_mm_vp_data;
	omap5_voltdm_core.vp_param = &omap5_core_vp_data;

	if (omap_rev() == OMAP5430_REV_ES1_0 ||
	    omap_rev() == OMAP5432_REV_ES1_0) {
		omap5_voltdm_mpu.vc_param = &omap5_es1_mpu_vc_data;
		omap5_voltdm_mm.vc_param = &omap5_es1_mm_vc_data;
		omap5_voltdm_core.vc_param = &omap5_es1_core_vc_data;
	} else {
		/* TBD: Update once ES2.0+ data is available */
		WARN(1, "OMAP revision = 0x%08X - NO VC param!\n", omap_rev());
	}

	for (i = 0; voltdm = voltagedomains_omap5[i], voltdm; i++)
		voltdm->sys_clk.name = sys_clk_name;

	voltdm_init(voltagedomains_omap5);
};

static int __init init_volt_domain_notifier_list(void)
{
	return __init_volt_domain_notifier_list(voltagedomains_omap5);
}
pure_initcall(init_volt_domain_notifier_list);
