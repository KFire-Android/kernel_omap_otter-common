/*
 * OMAP36xx Adaptive Body-Bias (ABB) data
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
#include "prm2xxx_3xxx.h"
#include "prm-regbits-34xx.h"

static const struct omap_abb_ops omap36xx_abb_ops = {
	.check_tranxdone   = &omap3_prm_abb_check_txdone,
	.clear_tranxdone   = &omap3_prm_abb_clear_txdone,
};

static const struct omap_abb_common omap36xx_abb_common = {
	.opp_sel_mask		= OMAP3630_OPP_SEL_MASK,
	.opp_change_mask	= OMAP3630_OPP_CHANGE_MASK,
	.sr2en_mask		= OMAP3630_SR2EN_MASK,
	.active_fbb_sel_mask	= OMAP3630_ACTIVE_FBB_SEL_MASK,
	.sr2_wtcnt_value_mask	= OMAP3630_SR2_WTCNT_VALUE_MASK,
	.settling_time		= 30,
	.clock_cycles		= 8,
	.ops			= &omap36xx_abb_ops,
};

/* SETUP & CTRL registers swapped names in OMAP4; thus 36xx looks strange */
struct omap_abb_instance omap36xx_abb_mpu = {
	.setup_offs		= OMAP3_PRM_LDO_ABB_CTRL_OFFSET,
	.ctrl_offs		= OMAP3_PRM_LDO_ABB_SETUP_OFFSET,
	.prm_irq_id		= OMAP3_VP_VDD_MPU_ID,
	.common			= &omap36xx_abb_common,
};
