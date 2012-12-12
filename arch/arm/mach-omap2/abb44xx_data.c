/*
 * OMAP44xx Adaptive Body-Bias (ABB) data
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Mike Turquette <mturquette@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "abb.h"
#include "vp.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"

static const struct omap_abb_ops omap4_abb_ops = {
	.check_tranxdone   = &omap4_prm_abb_check_txdone,
	.clear_tranxdone   = &omap4_prm_abb_clear_txdone,
};

static const struct omap_abb_common omap4_abb_common = {
	.opp_sel_mask		= OMAP4430_OPP_SEL_MASK,
	.opp_change_mask	= OMAP4430_OPP_CHANGE_MASK,
	.sr2en_mask		= OMAP4430_SR2EN_MASK,
	.active_fbb_sel_mask	= OMAP4430_ACTIVE_FBB_SEL_MASK,
	.active_rbb_sel_mask    = OMAP4430_ACTIVE_RBB_SEL_MASK,
	.sr2_wtcnt_value_mask	= OMAP4430_SR2_WTCNT_VALUE_MASK,
	.settling_time		= 50,
	.clock_cycles		= 16,
	.ops			= &omap4_abb_ops,
};

struct omap_abb_instance omap4_abb_mpu = {
	.setup_offs		= OMAP4_PRM_LDO_ABB_MPU_SETUP_OFFSET,
	.ctrl_offs		= OMAP4_PRM_LDO_ABB_MPU_CTRL_OFFSET,
	.prm_irq_id		= OMAP4_VP_VDD_MPU_ID,
	.common			= &omap4_abb_common,
};

struct omap_abb_instance omap4_abb_iva = {
	.setup_offs		= OMAP4_PRM_LDO_ABB_IVA_SETUP_OFFSET,
	.ctrl_offs		= OMAP4_PRM_LDO_ABB_IVA_CTRL_OFFSET,
	.prm_irq_id		= OMAP4_VP_VDD_IVA_ID,
	.common			= &omap4_abb_common,
};
