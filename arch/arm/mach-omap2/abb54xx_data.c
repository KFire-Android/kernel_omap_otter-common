/*
 * OMAP54xx Adaptive Body-Bias (ABB) data
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 * Andrii Tseglytskyi <andrii.tseglytskyi@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <mach/ctrl_module_wkup_54xx.h>

#include "abb.h"
#include "vp.h"
#include "prm54xx.h"
#include "prm44xx.h"
#include "prm-regbits-54xx.h"
#include "iomap.h"

static struct omap_abb_ops omap5_abb_ops = {
	.check_tranxdone  = omap4_prm_abb_check_txdone,
	.clear_tranxdone  = omap4_prm_abb_clear_txdone,
};

static const struct omap_abb_common omap5_abb_common = {
	.opp_sel_mask		= OMAP54XX_OPP_SEL_MASK,
	.opp_change_mask	= OMAP54XX_OPP_CHANGE_MASK,
	.sr2en_mask		= OMAP54XX_SR2EN_MASK,
	.active_fbb_sel_mask	= OMAP54XX_ACTIVE_FBB_SEL_MASK,
	.sr2_wtcnt_value_mask	= OMAP54XX_SR2_WTCNT_VALUE_MASK,
	.settling_time		= 50,
	.clock_cycles		= 16,
	.ops			= &omap5_abb_ops,
};

struct omap_abb_instance omap5_abb_mpu = {
	.setup_offs		= OMAP54XX_PRM_ABBLDO_MPU_SETUP_OFFSET,
	.ctrl_offs		= OMAP54XX_PRM_ABBLDO_MPU_CTRL_OFFSET,
	.prm_irq_id		= OMAP5_VP_VDD_MPU_ID,
	.common			= &omap5_abb_common,
	.mux_control_reg	= OMAP54XX_CTRL_MODULE_WKUP_REGADDR(OMAP5_CTRL_MODULE_WKUP_LDOVBB_MPU_VOLTAGE_CTRL),
	.mux_control_mask	= OMAP5_LDOVBBMPU_FBB_MUX_CTRL_MASK,
	.mux_control_vset_out_mask = OMAP5_LDOVBBMPU_FBB_VSET_OUT_MASK,
};

struct omap_abb_instance omap5_abb_mm = {
	.setup_offs		= OMAP54XX_PRM_ABBLDO_MM_SETUP_OFFSET,
	.ctrl_offs		= OMAP54XX_PRM_ABBLDO_MM_CTRL_OFFSET,
	.prm_irq_id		= OMAP5_VP_VDD_MM_ID,
	.common			= &omap5_abb_common,
	.mux_control_reg	= OMAP54XX_CTRL_MODULE_WKUP_REGADDR(OMAP5_CTRL_MODULE_WKUP_LDOVBB_MM_VOLTAGE_CTRL),
	.mux_control_mask	= OMAP5_LDOVBBMM_FBB_MUX_CTRL_MASK,
	.mux_control_vset_out_mask = OMAP5_LDOVBBMM_FBB_VSET_OUT_MASK,
};
