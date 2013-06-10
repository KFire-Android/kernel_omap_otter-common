/*
 * linux/arch/arm/mach-omap2/cpuidle34xx.c
 *
 * OMAP3 CPU IDLE Routines
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * Copyright (C) 2007 Texas Instruments, Inc.
 * Karthik Dasu <karthik-dp@ti.com>
 *
 * Copyright (C) 2006 Nokia Corporation
 * Tony Lindgren <tony@atomide.com>
 *
 * Copyright (C) 2005 Texas Instruments, Inc.
 * Richard Woodruff <r-woodruff2@ti.com>
 *
 * Based on pm.c for omap2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/export.h>
#include <linux/cpu_pm.h>

#include "powerdomain.h"
#include "clockdomain.h"

#include "pm.h"
#include "control.h"
#include "common.h"

/* Mach specific information to be recorded in the C-state driver_data */
struct omap3_idle_statedata {
	u8 mpu_fpwrst;
	u8 core_fpwrst;
	u8 per_min_fpwrst;
	u8 flags;
};

/*
 * Possible flag bits for struct omap3_idle_statedata.flags:
 *
 * OMAP_CPUIDLE_CX_NO_CLKDM_IDLE: don't allow the MPU clockdomain to go
 *    inactive.  This in turn prevents the MPU DPLL from entering autoidle
 *    mode, so wakeup latency is greatly reduced, at the cost of additional
 *    energy consumption.  This also prevents the CORE clockdomain from
 *    entering idle.
 */
#define OMAP_CPUIDLE_CX_NO_CLKDM_IDLE		BIT(0)

static struct powerdomain *mpu_pd, *core_pd, *per_pd, *cam_pd;

/*
 * Prevent PER OFF if CORE is not in RETention or OFF as this would
 * disable PER wakeups completely.
 */
static struct omap3_idle_statedata omap3_idle_data[] = {
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_ON,
		.core_fpwrst = PWRDM_FUNC_PWRST_ON,
		/* In C1 do not allow PER state lower than CORE state */
		.per_min_fpwrst = PWRDM_FUNC_PWRST_ON,
		.flags = OMAP_CPUIDLE_CX_NO_CLKDM_IDLE,
	},
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_ON,
		.core_fpwrst = PWRDM_FUNC_PWRST_ON,
		.per_min_fpwrst = PWRDM_FUNC_PWRST_CSWR,
	},
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_CSWR,
		.core_fpwrst = PWRDM_FUNC_PWRST_ON,
		.per_min_fpwrst = PWRDM_FUNC_PWRST_CSWR,
	},
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_OFF,
		.core_fpwrst = PWRDM_FUNC_PWRST_ON,
		.per_min_fpwrst = PWRDM_FUNC_PWRST_CSWR,
	},
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_CSWR,
		.core_fpwrst = PWRDM_FUNC_PWRST_CSWR,
		.per_min_fpwrst = PWRDM_FUNC_PWRST_OFF,
	},
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_OFF,
		.core_fpwrst = PWRDM_FUNC_PWRST_CSWR,
		.per_min_fpwrst = PWRDM_FUNC_PWRST_OFF,
	},
	{
		.mpu_fpwrst = PWRDM_FUNC_PWRST_OFF,
		.core_fpwrst = PWRDM_FUNC_PWRST_OFF,
		.per_min_fpwrst = PWRDM_FUNC_PWRST_OFF,
	},
};

/* Private functions */

static int __omap3_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	struct omap3_idle_statedata *cx = &omap3_idle_data[index];

	if (omap_irq_pending() || need_resched())
		goto return_sleep_time;

	/* Deny idle for C1 */
	if (cx->flags & OMAP_CPUIDLE_CX_NO_CLKDM_IDLE) {
		clkdm_deny_idle(mpu_pd->pwrdm_clkdms[0]);
	} else {
		pwrdm_set_next_fpwrst(mpu_pd, cx->mpu_fpwrst);
		pwrdm_set_next_fpwrst(core_pd, cx->core_fpwrst);
	}

	/*
	 * Call idle CPU PM enter notifier chain so that
	 * VFP context is saved.
	 */
	if (cx->mpu_fpwrst == PWRDM_FUNC_PWRST_OFF)
		cpu_pm_enter();

	/* Execute ARM wfi */
	omap_sram_idle();

	/*
	 * Call idle CPU PM enter notifier chain to restore
	 * VFP context.
	 */
	if (cx->mpu_fpwrst == PWRDM_FUNC_PWRST_OFF &&
	    pwrdm_read_prev_fpwrst(mpu_pd) == PWRDM_FUNC_PWRST_OFF)
		cpu_pm_exit();

	/* Re-allow idle for C1 */
	if (cx->flags & OMAP_CPUIDLE_CX_NO_CLKDM_IDLE)
		clkdm_allow_idle(mpu_pd->pwrdm_clkdms[0]);

return_sleep_time:
	return index;
}

/**
 * omap3_enter_idle - Programs OMAP3 to enter the specified state
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: the index of state to be entered
 *
 * Called from the CPUidle framework to program the device to the
 * specified target state selected by the governor.
 */
static inline int omap3_enter_idle(struct cpuidle_device *dev,
				struct cpuidle_driver *drv,
				int index)
{
	return cpuidle_wrap_enter(dev, drv, index, __omap3_enter_idle);
}

/**
 * next_valid_state - Find next valid C-state
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: Index of currently selected c-state
 *
 * If the state corresponding to index is valid, index is returned back
 * to the caller. Else, this function searches for a lower c-state which is
 * still valid (as defined in omap3_power_states[]) and returns its index.
 *
 * A state is valid if the 'valid' field is enabled and
 * if it satisfies the enable_off_mode condition.
 */
static int next_valid_state(struct cpuidle_device *dev,
			    struct cpuidle_driver *drv, int index)
{
	struct omap3_idle_statedata *cx = &omap3_idle_data[index];
	u8 mpu_deepest_fpwrst = PWRDM_FUNC_PWRST_CSWR;
	u8 core_deepest_fpwrst = PWRDM_FUNC_PWRST_CSWR;
	int idx;
	int next_index = 0; /* C1 is the default value */

	if (enable_off_mode) {
		mpu_deepest_fpwrst = PWRDM_FUNC_PWRST_OFF;
		/*
		 * Erratum i583: valable for ES rev < Es1.2 on 3630.
		 * CORE OFF mode is not supported in a stable form, restrict
		 * instead the CORE state to RET.
		 */
		if (!IS_PM34XX_ERRATUM(PM_SDRC_WAKEUP_ERRATUM_i583))
			core_deepest_fpwrst = PWRDM_FUNC_PWRST_OFF;
	}

	/* Check if current state is valid */
	if ((cx->mpu_fpwrst >= mpu_deepest_fpwrst) &&
	    (cx->core_fpwrst >= core_deepest_fpwrst))
		return index;

	/*
	 * Drop to next valid state.
	 * Start search from the next (lower) state.
	 */
	for (idx = index - 1; idx >= 0; idx--) {
		cx = &omap3_idle_data[idx];
		if ((cx->mpu_fpwrst >= mpu_deepest_fpwrst) &&
		    (cx->core_fpwrst >= core_deepest_fpwrst)) {
			next_index = idx;
			break;
		}
	}

	return next_index;
}

/**
 * omap3_enter_idle_bm - Checks for any bus activity
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: array index of target state to be programmed
 *
 * This function checks for any pending activity and then programs
 * the device to the specified or a safer state.
 */
static int omap3_enter_idle_bm(struct cpuidle_device *dev,
			       struct cpuidle_driver *drv,
			       int index)
{
	int new_state_idx, ret;
	int per_next_fpwrst, per_saved_fpwrst;
	struct omap3_idle_statedata *cx;

	/*
	 * Use only C1 if CAM is active.
	 * CAM does not have wakeup capability in OMAP3.
	 * XXX This workaround belongs in the hwmod code & data
	 * as a hwmod flag
	 */
	if (pwrdm_read_fpwrst(cam_pd) == PWRDM_FUNC_PWRST_ON)
		new_state_idx = drv->safe_state_index;
	else
		new_state_idx = next_valid_state(dev, drv, index);

	/*
	 * FIXME: we currently manage device-specific idle states
	 *        for PER and CORE in combination with CPU-specific
	 *        idle states.  This is wrong, and device-specific
	 *        idle management needs to be separated out into
	 *        its own code.
	 */

	/* Program PER state */
	cx = &omap3_idle_data[new_state_idx];

	per_next_fpwrst = pwrdm_read_next_fpwrst(per_pd);
	WARN_ON(per_next_fpwrst < 0);
	per_saved_fpwrst = per_next_fpwrst;
	if (per_next_fpwrst < cx->per_min_fpwrst) {
		per_next_fpwrst = cx->per_min_fpwrst;
		WARN_ON(pwrdm_set_next_fpwrst(per_pd, per_next_fpwrst));
	}

	ret = omap3_enter_idle(dev, drv, new_state_idx);

	/* Restore original PER state if it was modified */
	if (per_next_fpwrst != per_saved_fpwrst)
		WARN_ON(pwrdm_set_next_fpwrst(per_pd, per_saved_fpwrst));

	return ret;
}

static DEFINE_PER_CPU(struct cpuidle_device, omap3_idle_dev);

static struct cpuidle_driver omap3_idle_driver = {
	.name             = "omap3_idle",
	.owner            = THIS_MODULE,
	.en_core_tk_irqen = 1,
	.states = {
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 2 + 2,
			.target_residency = 5,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C1",
			.desc		  = "MPU ON + CORE ON",
		},
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 10 + 10,
			.target_residency = 30,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C2",
			.desc		  = "MPU ON + CORE ON",
		},
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 50 + 50,
			.target_residency = 300,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C3",
			.desc		  = "MPU RET + CORE ON",
		},
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 1500 + 1800,
			.target_residency = 4000,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C4",
			.desc		  = "MPU OFF + CORE ON",
		},
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 2500 + 7500,
			.target_residency = 12000,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C5",
			.desc		  = "MPU RET + CORE RET",
		},
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 3000 + 8500,
			.target_residency = 15000,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C6",
			.desc		  = "MPU OFF + CORE RET",
		},
		{
			.enter		  = omap3_enter_idle_bm,
			.exit_latency	  = 10000 + 30000,
			.target_residency = 30000,
			.flags		  = CPUIDLE_FLAG_TIME_VALID,
			.name		  = "C7",
			.desc		  = "MPU OFF + CORE OFF",
		},
	},
	.state_count = ARRAY_SIZE(omap3_idle_data),
	.safe_state_index = 0,
};

/* Public functions */

/**
 * omap3_idle_init - Init routine for OMAP3 idle
 *
 * Registers the OMAP3 specific cpuidle driver to the cpuidle
 * framework with the valid set of states.
 */
int __init omap3_idle_init(void)
{
	struct cpuidle_device *dev;

	mpu_pd = pwrdm_lookup("mpu_pwrdm");
	core_pd = pwrdm_lookup("core_pwrdm");
	per_pd = pwrdm_lookup("per_pwrdm");
	cam_pd = pwrdm_lookup("cam_pwrdm");

	if (!mpu_pd || !core_pd || !per_pd || !cam_pd)
		return -ENODEV;

	cpuidle_register_driver(&omap3_idle_driver);

	dev = &per_cpu(omap3_idle_dev, smp_processor_id());
	dev->cpu = 0;

	if (cpuidle_register_device(dev)) {
		printk(KERN_ERR "%s: CPUidle register device failed\n",
		       __func__);
		return -EIO;
	}

	return 0;
}
