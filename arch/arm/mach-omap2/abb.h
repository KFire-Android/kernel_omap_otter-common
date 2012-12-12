/*
 * OMAP Adaptive Body-Bias structure and macro definitions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Mike Turquette <mturquette@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_ABB_H
#define __ARCH_ARM_MACH_OMAP2_ABB_H

#include <linux/kernel.h>

#include "voltage.h"

/* NOMINAL_OPP bypasses the ABB ldo, FAST_OPP sets it to Forward Body-Bias */
#define OMAP_ABB_NOMINAL_OPP	0
#define OMAP_ABB_FAST_OPP	1
#define OMAP_ABB_SLOW_OPP	3
#define OMAP_ABB_NO_LDO		(~0)

/* Time for the ABB ldo to settle after transition (in micro-seconds) */
#define ABB_TRANXDONE_TIMEOUT	50

/*
 * struct omap_abb_ops - per-OMAP operations needed for ABB transition
 *
 * @check_tranxdone: return status of ldo transition from PRM_IRQSTATUS
 * @clear_tranxdone: clear ABB transition status bit from PRM_IRQSTATUS
 */
struct omap_abb_ops {
	u32 (*check_tranxdone)(u8 irq_id);
	void (*clear_tranxdone)(u8 irq_id);
};

/*
 * struct omap_abb_common - ABB data common to an OMAP family
 *
 * @opp_sel_mask: CTRL reg uses this to program next state of ldo
 * @opp_change_mask: CTRL reg uses this to initiate ldo state change
 * @sr2_wtcnt_value_mask: SETUP reg uses this to program ldo settling time
 * @sr2en_mask: SETUP reg uses this to enable/disable ldo
 * @active_fbb_sel_mask: SETUP reg uses this to enable/disable FBB operation
 * @active_rbb_sel_mask: SETUP reg uses this to enable/disable RBB operation
 * @settling_time: number of micro-seconds it takes for ldo to transition
 * @clock_cycles: settling_time is counted in multiples of clock cycles
 * @ops: pointer to common ops for manipulating PRM_IRQSTATUS bits
 */
struct omap_abb_common {
	u32 opp_sel_mask;
	u32 opp_change_mask;
	u32 sr2_wtcnt_value_mask;
	u32 sr2en_mask;
	u32 active_fbb_sel_mask;
	u32 active_rbb_sel_mask;
	unsigned long settling_time;
	unsigned long clock_cycles;
	const struct omap_abb_ops *ops;
};

/*
 * struct omap_abb_instance - data for each instance of ABB ldo
 *
 * @setup_offs: PRM register offset for initial configuration of ABB ldo
 * @ctrl_offs: PRM register offset for active programming of ABB ldo
 * @prm_irq_id: IRQ handle used to resolve IRQSTATUS offset & masks
 * @enabled: track whether ABB ldo is enabled or disabled
 * @common: pointer to common data for all ABB ldo's
 * @_opp_sel: internally track last programmed state of ABB ldo.  DO NOT USE
 */
struct omap_abb_instance {
	u8 setup_offs;
	u8 ctrl_offs;
	u8 prm_irq_id;
	bool enabled;
	const struct omap_abb_common *common;
	u8 _opp_sel;
};

extern struct omap_abb_instance omap36xx_abb_mpu;

extern struct omap_abb_instance omap4_abb_mpu;
extern struct omap_abb_instance omap4_abb_iva;

extern struct omap_abb_instance omap5_abb_mpu;
extern struct omap_abb_instance omap5_abb_mm;

void omap_abb_init(struct voltagedomain *voltdm);
void omap_abb_enable(struct voltagedomain *voltdm);
void omap_abb_disable(struct voltagedomain *voltdm);
int omap_abb_pre_scale(struct voltagedomain *voltdm,
		struct omap_volt_data *target_volt_data);
int omap_abb_post_scale(struct voltagedomain *voltdm,
		struct omap_volt_data *target_volt_data);

#endif
