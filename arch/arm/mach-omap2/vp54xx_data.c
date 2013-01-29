/*
 * OMAP5 Voltage Processor (VP) data
 *
 * Copyright (C) 2011-2012 Texas Instruments, Inc.
 * Vishwanath BS <vishwanath.bs@ti.com>
 *
 * Copyright (C) 2007, 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Lesly A M <x0080970@ti.com>
 * Thara Gopinath <thara@ti.com>
 *
 * Copyright (C) 2008, 2011 Nokia Corporation
 * Kalle Jokiniemi
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>

#include <plat/common.h>

#include "pm.h"
#include "prm44xx.h"
#include "prm54xx.h"
#include "prm-regbits-54xx.h"

#include "voltage.h"

#include "vp.h"

/* OMAP4 is hooked such that only a cold reset will reset VP */
static void omap5_vp_recover(u8 vp_id)
{
	omap4_pm_cold_reset("Voltage Processor Recovery");
}

static const struct omap_vp_ops omap5_vp_ops = {
	.check_txdone = omap4_prm_vp_check_txdone,
	.clear_txdone = omap4_prm_vp_clear_txdone,
	.recover = omap5_vp_recover,
};

/*
 * VP data common to 54xx chips
 * XXX This stuff presumably belongs in the vp44xx.c or vp.c file.
 */
static const struct omap_vp_common omap5_vp_common = {
	.vpconfig_erroroffset_mask = OMAP54XX_ERROROFFSET_MASK,
	.vpconfig_errorgain_mask = OMAP54XX_ERRORGAIN_MASK,
	.vpconfig_initvoltage_mask = OMAP54XX_INITVOLTAGE_MASK,
	.vpconfig_timeouten = OMAP54XX_TIMEOUTEN_MASK,
	.vpconfig_initvdd = OMAP54XX_INITVDD_MASK,
	.vpconfig_forceupdate = OMAP54XX_FORCEUPDATE_MASK,
	.vpconfig_vpenable = OMAP54XX_VPENABLE_MASK,
	.vstatus_vpidle = OMAP54XX_VPINIDLE_MASK,
	.vstepmin_smpswaittimemin_shift = OMAP54XX_SMPSWAITTIMEMIN_SHIFT,
	.vstepmax_smpswaittimemax_shift = OMAP54XX_SMPSWAITTIMEMAX_SHIFT,
	.vstepmin_stepmin_shift = OMAP54XX_VSTEPMIN_SHIFT,
	.vstepmax_stepmax_shift = OMAP54XX_VSTEPMAX_SHIFT,
	.vlimitto_vddmin_shift = OMAP54XX_VDDMIN_SHIFT,
	.vlimitto_vddmax_shift = OMAP54XX_VDDMAX_SHIFT,
	.vlimitto_timeout_shift = OMAP54XX_TIMEOUT_SHIFT,
	.vpvoltage_mask = OMAP54XX_VPVOLTAGE_MASK,
	.ops = &omap5_vp_ops,
};

struct omap_vp_instance omap5_vp_mpu = {
	.id = OMAP5_VP_VDD_MPU_ID,
	.common = &omap5_vp_common,
	.vpconfig = OMAP54XX_PRM_VP_MPU_CONFIG_OFFSET,
	.vstepmin = OMAP54XX_PRM_VP_MPU_VSTEPMIN_OFFSET,
	.vstepmax = OMAP54XX_PRM_VP_MPU_VSTEPMAX_OFFSET,
	.vlimitto = OMAP54XX_PRM_VP_MPU_VLIMITTO_OFFSET,
	.vstatus = OMAP54XX_PRM_VP_MPU_STATUS_OFFSET,
	.voltage = OMAP54XX_PRM_VP_MPU_VOLTAGE_OFFSET,
};

struct omap_vp_instance omap5_vp_mm = {
	.id = OMAP5_VP_VDD_MM_ID,
	.common = &omap5_vp_common,
	.vpconfig = OMAP54XX_PRM_VP_MM_CONFIG_OFFSET,
	.vstepmin = OMAP54XX_PRM_VP_MM_VSTEPMIN_OFFSET,
	.vstepmax = OMAP54XX_PRM_VP_MM_VSTEPMAX_OFFSET,
	.vlimitto = OMAP54XX_PRM_VP_MM_VLIMITTO_OFFSET,
	.vstatus = OMAP54XX_PRM_VP_MM_STATUS_OFFSET,
	.voltage = OMAP54XX_PRM_VP_MM_VOLTAGE_OFFSET,
};

struct omap_vp_instance omap5_vp_core = {
	.id = OMAP5_VP_VDD_CORE_ID,
	.common = &omap5_vp_common,
	.vpconfig = OMAP54XX_PRM_VP_CORE_CONFIG_OFFSET,
	.vstepmin = OMAP54XX_PRM_VP_CORE_VSTEPMIN_OFFSET,
	.vstepmax = OMAP54XX_PRM_VP_CORE_VSTEPMAX_OFFSET,
	.vlimitto = OMAP54XX_PRM_VP_CORE_VLIMITTO_OFFSET,
	.vstatus = OMAP54XX_PRM_VP_CORE_STATUS_OFFSET,
	.voltage = OMAP54XX_PRM_VP_CORE_VOLTAGE_OFFSET,
};

struct omap_vp_param omap5_mpu_vp_data = {
	.vddmin			= 650000,
	.vddmax			= 1250000,
};

struct omap_vp_param omap5_mm_vp_data = {
	.vddmin			= 650000,
	.vddmax			= 1120000,
};

struct omap_vp_param omap5_core_vp_data = {
	.vddmin			= 810000,
	.vddmax			= 1040000,
};
