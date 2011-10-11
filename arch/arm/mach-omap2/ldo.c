/*
 * OMAP3/4 LDO users core
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com/
 * Mike Turquette <mturquette@ti.com>
 * Nishanth Menon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/string.h>

#include <plat/cpu.h>

#include <mach/ctrl_module_core_44xx.h>

#include "voltage.h"
#include "ldo.h"
#include "control.h"

#define OMAP4460_MPU_OPP_DPLL_TRIM	BIT(18)
#define OMAP4460_MPU_OPP_DPLL_TURBO_RBB	BIT(20)

/* voltages are not defined in a header... yay duplication! */
#define OMAP4460_VDD_MPU_OPPTURBO_UV	1317000

/**
 * _is_abb_enabled() - check if abb is enabled
 * @voltdm:	voltage domain to check for
 * @abb:	abb instance pointer
 *
 * Returns true if enabled, else returns false
 */
static inline bool _is_abb_enabled(struct voltagedomain *voltdm,
				   struct omap_ldo_abb_instance *abb)
{
	return (voltdm->read(abb->setup_reg) & abb->setup_bits->enable_mask) ?
	    true : false;
}

/**
 * _abb_set_availability() - sets the availability of the ABB LDO
 * @voltdm:	voltage domain for which we would like to set
 * @abb:	abb instance pointer
 * @available:	should I enable/disable the LDO?
 *
 * Depending on the request, it enables/disables the LDO if it was not
 * in that state already.
 */
static inline void _abb_set_availability(struct voltagedomain *voltdm,
					 struct omap_ldo_abb_instance *abb,
					 bool available)
{
	if (_is_abb_enabled(voltdm, abb) == available)
		return;

	voltdm->rmw(abb->setup_bits->enable_mask,
		    (available) ? abb->setup_bits->enable_mask : 0,
		    abb->setup_reg);
}

/**
 * _abb_clear_tranx() - clear abb tranxdone event
 * @voltdm:	voltage domain we are operating on
 * @abb:	pointer to the abb instance
 *
 * Returns -ETIMEDOUT if the event is not cleared on time.
 */
static int _abb_clear_tranx(struct voltagedomain *voltdm,
			    struct omap_ldo_abb_instance *abb)
{
	int timeout;
	int ret;

	/* clear interrupt status */
	timeout = 0;
	while (timeout++ < abb->tranx_timeout) {
		abb->ops->clear_txdone(abb->prm_irq_id);

		ret = abb->ops->check_txdone(abb->prm_irq_id);
		if (!ret)
			break;

		udelay(1);
	}

	if (timeout >= abb->tranx_timeout) {
		pr_warning("%s:%s: ABB TRANXDONE timeout(timeout=%d)\n",
			   __func__, voltdm->name, timeout);
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 * _abb_set_abb() - helper to actually set ABB (NOMINAL/FAST/SLOW)
 * @voltdm:	voltage domain we are operating on
 * @abb_type:	ABB type we want to set
 */
static int _abb_set_abb(struct voltagedomain *voltdm, int abb_type)
{
	struct omap_ldo_abb_instance *abb = voltdm->abb;
	int ret;

	ret = _abb_clear_tranx(voltdm, abb);
	if (ret)
		return ret;

	/* Select FBB or RBB */
	if (abb_type == OMAP_ABB_SLOW_OPP)
		voltdm->rmw(abb->setup_bits->active_fbb_mask |
				abb->setup_bits->active_rbb_mask,
				abb->setup_bits->active_rbb_mask,
				abb->setup_reg);
	else
		voltdm->rmw(abb->setup_bits->active_fbb_mask |
				abb->setup_bits->active_rbb_mask,
				abb->setup_bits->active_fbb_mask,
				abb->setup_reg);

	/* program next state of ABB ldo */
	voltdm->rmw(abb->ctrl_bits->opp_sel_mask,
		    abb_type << __ffs(abb->ctrl_bits->opp_sel_mask),
		    abb->ctrl_reg);

	/* initiate ABB ldo change */
	voltdm->rmw(abb->ctrl_bits->opp_change_mask,
		    abb->ctrl_bits->opp_change_mask, abb->ctrl_reg);

	/* clear interrupt status */
	ret = _abb_clear_tranx(voltdm, abb);

	return ret;
}

/**
 * _abb_scale() - wrapper which does the necessary things for pre and post scale
 * @voltdm:		voltage domain to operate on
 * @target_volt:	voltage we are going to
 * @is_prescale:	are we doing a prescale operation?
 *
 * NOTE: We expect caller ensures that a specific voltdm is modified
 * sequentially. All locking is expected to be implemented by users
 * of LDO functions
 */
static int _abb_scale(struct voltagedomain *voltdm,
		      unsigned long target_volt, bool is_prescale)
{
	int ret = 0;
	struct omap_volt_data *target_vdata;
	int curr_abb, target_abb;
	struct omap_ldo_abb_instance *abb;

	/* get per-voltage ABB data */
	target_vdata = omap_voltage_get_voltdata(voltdm, target_volt);
	if (IS_ERR_OR_NULL(target_vdata)) {
		pr_err("%s:%s: Invalid volt data tv=%p!\n", __func__,
		       voltdm->name, target_vdata);
		return -EINVAL;
	}

	abb = voltdm->abb;
	if (IS_ERR_OR_NULL(abb)) {
		WARN(1, "%s:%s: no abb structure!\n", __func__, voltdm->name);
		return -EINVAL;
	}

	curr_abb = abb->__cur_abb_type;
	target_abb = target_vdata->abb_type;

	pr_debug("%s: %s: Enter: t_v=%ld scale=%d c_abb=%d t_abb=%d ret=%d\n",
		 __func__, voltdm->name, target_volt, is_prescale, curr_abb,
		 target_abb, ret);

	/* If we were'nt booting and there is no change, we get out */
	if (target_abb == curr_abb && voltdm->curr_volt)
		goto out;

	/* Do we have an invalid ABB entry? scream for a fix! */
	if (curr_abb == OMAP_ABB_NONE || target_abb == OMAP_ABB_NONE) {
		WARN(1, "%s:%s: INVALID abb entries? curr=%d target=%d\n",
		     __func__, voltdm->name, curr_abb, target_abb);
		return -EINVAL;
	}

	/*
	 * We set up ABB as follows:
	 * if we are scaling *to* a voltage which needs ABB, do it in post
	 * if we are scaling *from* a voltage which needs ABB, do it in pre
	 * So, if the conditions are in reverse, we just return happy
	 */
	if (is_prescale && (target_abb > curr_abb))
		goto out;

	if (!is_prescale && (target_abb < curr_abb))
		goto out;

	/* Time to set ABB now */
	ret = _abb_set_abb(voltdm, target_abb);
	if (!ret) {
		abb->__cur_abb_type = target_abb;
		pr_debug("%s: %s:  scaled - t_abb=%d!\n", __func__,
			 voltdm->name, target_abb);
	} else {
		pr_warning("%s: %s:  failed scale: t_abb=%d (%d)!\n", __func__,
			   voltdm->name, target_abb, ret);
	}

out:
	pr_debug("%s: %s:Exit: t_v=%ld scale=%d c_abb=%d t_abb=%d ret=%d\n",
		 __func__, voltdm->name, target_volt, is_prescale, curr_abb,
		 target_abb, ret);
	return ret;

}

/**
 * omap_ldo_abb_pre_scale() - Enable required ABB strategy before voltage scale
 * @voltdm:		voltage domain to operate on
 * @target_volt:	target voltage we moved to.
 */
int omap_ldo_abb_pre_scale(struct voltagedomain *voltdm,
			   unsigned long target_volt)
{
	return _abb_scale(voltdm, target_volt, true);
}

/**
 * omap_ldo_abb_pre_scale() - Enable required ABB strategy after voltage scale
 * @voltdm:		voltage domain operated on
 * @target_volt:	target voltage we are going to
 */
int omap_ldo_abb_post_scale(struct voltagedomain *voltdm,
			    unsigned long target_volt)
{
	return _abb_scale(voltdm, target_volt, false);
}

/**
 * omap_ldo_abb_init() - initialize the ABB LDO for associated for this domain
 * @voltdm:	voltdm for which we need to initialize the ABB LDO
 *
 * Programs up the the configurations that dont change in the domain
 *
 * Return 0 if all goes fine, else returns appropriate error value
 */
void __init omap_ldo_abb_init(struct voltagedomain *voltdm)
{
	u32 sys_clk_rate;
	u32 cycle_rate;
	u32 settling_time;
	u32 wait_count_val;
	u32 reg, trim, rbb;
	struct omap_ldo_abb_instance *abb;
	struct omap_volt_data *volt_data;

	if (IS_ERR_OR_NULL(voltdm)) {
		pr_err("%s: No voltdm?\n", __func__);
		return;
	}
	if (!voltdm->read || !voltdm->write || !voltdm->rmw) {
		pr_err("%s: No read/write/rmw API for accessing vdd_%s regs\n",
		       __func__, voltdm->name);
		return;
	}

	abb = voltdm->abb;
	if (IS_ERR_OR_NULL(abb))
		return;
	if (IS_ERR_OR_NULL(abb->ctrl_bits) || IS_ERR_OR_NULL(abb->setup_bits)) {
		pr_err("%s: Corrupted ABB configuration on vdd_%s regs\n",
		       __func__, voltdm->name);
		return;
	}

	/*
	 * SR2_WTCNT_VALUE must be programmed with the expected settling time
	 * for ABB ldo transition.  This value depends on the cycle rate for
	 * the ABB IP (varies per OMAP family), and the system clock frequency
	 * (varies per board).  The formula is:
	 *
	 * SR2_WTCNT_VALUE = SettlingTime / (CycleRate / SystemClkRate))
	 * where SettlingTime is in micro-seconds and SystemClkRate is in MHz.
	 *
	 * To avoid dividing by zero multiply both CycleRate and SettlingTime
	 * by 10 such that the final result is the one we want.
	 */

	/* Convert SYS_CLK rate to MHz & prevent divide by zero */
	sys_clk_rate = DIV_ROUND_CLOSEST(voltdm->sys_clk.rate, 1000000);
	cycle_rate = abb->cycle_rate * 10;
	settling_time = abb->settling_time * 10;

	/* Calculate cycle rate */
	cycle_rate = DIV_ROUND_CLOSEST(cycle_rate, sys_clk_rate);

	/* Calulate SR2_WTCNT_VALUE */
	wait_count_val = DIV_ROUND_CLOSEST(settling_time, cycle_rate);

	voltdm->rmw(abb->setup_bits->wait_count_mask,
		    wait_count_val << __ffs(abb->setup_bits->wait_count_mask),
		    abb->setup_reg);

	/*
	 * Determine MPU ABB state at OPP_TURBO on 4460
	 *
	 * On 4460 all OPPs have preset states for the MPU's ABB LDO, except
	 * for OPP_TURBO.  OPP_TURBO may require bypass, FBB or RBB depending
	 * on a combination of characterisation data blown into eFuse register
	 * CONTROL_STD_FUSE_OPP_DPLL_1.
	 *
	 * Bits 18 & 19 of that register signify DPLL_MPU trim (see
	 * arch/arm/mach-omap2/omap4-trim-quirks.c).  OPP_TURBO might put MPU's
	 * ABB LDO into bypass or FBB based on this value.
	 *
	 * Bit 20 siginifies if RBB should be enabled.  If set it will always
	 * override the values from bits 18 & 19.
	 *
	 * The table below captures the valid combinations:
	 *
	 * Bit 18|Bit 19|Bit 20|ABB type
	 * 0	  0	 0	bypass
	 * 0	  1	 0	bypass	(invalid combo)
	 * 1	  0	 0	FBB	(2.4GHz DPLL_MPU)
	 * 1	  1	 0	FBB	(3GHz DPLL_MPU)
	 * 0	  0	 1	RBB
	 * 0	  1	 1	RBB	(invalid combo)
	 * 1	  0	 1	RBB	(2.4GHz DPLL_MPU)
	 * 1	  1	 1	RBB	(3GHz DPLL_MPU)
	 */
	if (cpu_is_omap446x() && !strcmp("mpu", voltdm->name)) {
		/* read eFuse register here */
		reg = omap_ctrl_readl(OMAP4_CTRL_MODULE_CORE_STD_FUSE_OPP_DPLL_1);
		trim = reg & OMAP4460_MPU_OPP_DPLL_TRIM;
		rbb = reg & OMAP4460_MPU_OPP_DPLL_TURBO_RBB;

		volt_data = omap_voltage_get_voltdata(voltdm,
				OMAP4460_VDD_MPU_OPPTURBO_UV);

		/* OPP_TURBO is FAST_OPP (FBB) by default */
		if (rbb)
			volt_data->abb_type = OMAP_ABB_SLOW_OPP;
		else if (!trim)
			volt_data->abb_type = OMAP_ABB_NOMINAL_OPP;

	}
	/* Enable ABB */
	_abb_set_availability(voltdm, abb, true);

	/*
	 * Beware of the bootloader!
	 * Initialize current abb type based on what we read off the reg.
	 * we cant trust the initial state based off boot voltage's volt_data
	 * even. Not all bootloaders are nice :(
	 */
	abb->__cur_abb_type = (voltdm->read(abb->ctrl_reg) &
			       abb->ctrl_bits->opp_sel_mask) >>
				    __ffs(abb->ctrl_bits->opp_sel_mask);

	return;
}
