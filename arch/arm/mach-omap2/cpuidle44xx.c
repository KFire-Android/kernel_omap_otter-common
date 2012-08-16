/*
 * OMAP4 CPU idle Routines
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/export.h>
#include <linux/clockchips.h>

#include <asm/proc-fns.h>

#include "common.h"
#include "pm.h"
#include "prm.h"
#include "clockdomain.h"

/* Machine specific information to be recorded in the C-state driver_data */
struct omap4_idle_statedata {
	u32 cpu_state;
	u32 mpu_state;
	u8 valid;
};

static struct cpuidle_params cpuidle_params_table[] = {
	/* C1 - CPU0 ON + CPU1 ON + MPU ON */
	{.exit_latency = 2 + 2 , .target_residency = 5, .valid = 1},
	/* C2- CPU0 OFF + CPU1 OFF + MPU CSWR */
	{.exit_latency = 328 + 440 , .target_residency = 960, .valid = 1},
	/* C3 - CPU0 OFF + CPU1 OFF + MPU OSWR */
	{.exit_latency = 460 + 518 , .target_residency = 1100, .valid = 1},
};

#define OMAP4_NUM_STATES ARRAY_SIZE(cpuidle_params_table)

static struct omap4_idle_statedata omap4_idle_data[OMAP4_NUM_STATES];
static struct powerdomain *mpu_pd, *cpu_pd[NR_CPUS];
static struct clockdomain *cpu_clkdm[NR_CPUS];

static atomic_t abort_barrier;
static bool cpu_done[NR_CPUS];

/**
 * omap4_enter_idle_coupled_[simple/coupled] - OMAP4 cpuidle entry functions
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: the index of state to be entered
 *
 * Called from the CPUidle framework to program the device to the
 * specified low power state selected by the governor.
 * Returns the amount of time spent in the low power state.
 */
static int omap4_enter_idle_simple(struct cpuidle_device *dev,
				   struct cpuidle_driver *drv,
				   int index)
{
	local_fiq_disable();
	omap_do_wfi();
	local_fiq_enable();

	return index;
}

static int omap4_enter_idle_coupled(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			int index)
{
	struct omap4_idle_statedata *cx =
			cpuidle_get_statedata(&dev->states_usage[index]);
	int cpu_id = smp_processor_id();

	local_fiq_disable();

	/*
	 * CPU0 has to wait and stay ON until CPU1 is OFF state.
	 * This is necessary to honour hardware recommondation
	 * of triggeing all the possible low power modes once CPU1 is
	 * out of coherency and in OFF mode.
	 */
	if (dev->cpu == 0 && cpumask_test_cpu(1, cpu_online_mask)) {
		while (pwrdm_read_pwrst(cpu_pd[1]) != PWRDM_POWER_OFF) {
			cpu_relax();

			/*
			 * CPU1 could have already entered & exited idle
			 * without hitting off because of a wakeup
			 * or a failed attempt to hit off mode.  Check for
			 * that here, otherwise we could spin forever
			 * waiting for CPU1 off.
			 */
			if (cpu_done[1])
			    goto fail;

		}
	}

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);

	/*
	 * Call idle CPU PM enter notifier chain so that
	 * VFP and per CPU interrupt context is saved.
	 */
	cpu_pm_enter();

	if (dev->cpu == 0) {
		omap_set_pwrdm_state(mpu_pd, cx->mpu_state);

		/*
		 * Call idle CPU cluster PM enter notifier chain
		 * to save GIC and wakeupgen context.
		 */
		if (cx->mpu_state == PWRDM_POWER_OSWR)
			cpu_cluster_pm_enter();
	}

	omap_enter_lowpower(dev->cpu, cx->cpu_state);
	cpu_done[dev->cpu] = true;

	/* Wakeup CPU1 only if it is not offlined */
	if (dev->cpu == 0 && cpumask_test_cpu(1, cpu_online_mask)) {
		clkdm_wakeup(cpu_clkdm[1]);
		clkdm_allow_idle(cpu_clkdm[1]);
	}

	/*
	 * Call idle CPU PM exit notifier chain to restore
	 * VFP and per CPU IRQ context.
	 */
	cpu_pm_exit();

	/*
	 * Call idle CPU cluster PM exit notifier chain
	 * to restore GIC and wakeupgen context.
	 */
	if (omap_mpuss_read_prev_context_state())
		cpu_cluster_pm_exit();

	clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);

fail:
	cpuidle_coupled_parallel_barrier(dev, &abort_barrier);
	cpu_done[dev->cpu] = false;

	local_fiq_enable();

	return index;
}

static DEFINE_PER_CPU(struct cpuidle_device, omap4_idle_dev);

static struct cpuidle_driver omap4_idle_driver = {
	.name				= "omap4_idle",
	.owner				= THIS_MODULE,
	.en_core_tk_irqen		= 1,
};

static inline void _fill_cstate(struct cpuidle_driver *drv,
					int idx, const char *descr, int flags)
{
	struct cpuidle_state *state = &drv->states[idx];

	state->exit_latency	= cpuidle_params_table[idx].exit_latency;
	state->target_residency	= cpuidle_params_table[idx].target_residency;
	state->flags		= (CPUIDLE_FLAG_TIME_VALID | flags);
	if (state->flags & CPUIDLE_FLAG_COUPLED)
		state->enter		= omap4_enter_idle_coupled;
	else
		state->enter		= omap4_enter_idle_simple;
	sprintf(state->name, "C%d", idx + 1);
	strncpy(state->desc, descr, CPUIDLE_DESC_LEN);
}

static inline struct omap4_idle_statedata *_fill_cstate_usage(
					struct cpuidle_device *dev,
					int idx)
{
	struct omap4_idle_statedata *cx = &omap4_idle_data[idx];
	struct cpuidle_state_usage *state_usage = &dev->states_usage[idx];

	cx->valid		= cpuidle_params_table[idx].valid;
	cpuidle_set_statedata(state_usage, cx);

	return cx;
}



/**
 * omap4_idle_init - Init routine for OMAP4 idle
 *
 * Registers the OMAP4 specific cpuidle driver to the cpuidle
 * framework with the valid set of states.
 */
int __init omap4_idle_init(void)
{
	struct omap4_idle_statedata *cx;
	struct cpuidle_device *dev;
	struct cpuidle_driver drv = omap4_idle_driver;
	bool drv_reg = false;
	unsigned int cpu_id = 0;

	mpu_pd = pwrdm_lookup("mpu_pwrdm");
	cpu_pd[0] = pwrdm_lookup("cpu0_pwrdm");
	cpu_pd[1] = pwrdm_lookup("cpu1_pwrdm");
	if ((!mpu_pd) || (!cpu_pd[0]) || (!cpu_pd[1]))
		return -ENODEV;

	cpu_clkdm[0] = clkdm_lookup("mpu0_clkdm");
	cpu_clkdm[1] = clkdm_lookup("mpu1_clkdm");
	if (!cpu_clkdm[0] || !cpu_clkdm[1])
		return -ENODEV;

	for_each_cpu(cpu_id, cpu_online_mask) {
		drv.safe_state_index = -1;
		dev = &per_cpu(omap4_idle_dev, cpu_id);
		dev->cpu = cpu_id;
		dev->state_count = 0;
		dev->coupled_cpus = *cpu_online_mask;
		drv.state_count = 0;

		/* C1 - CPU0 ON + CPU1 ON + MPU ON */
		_fill_cstate(&drv, 0, "MPUSS ON", 0);
		drv.safe_state_index = 0;
		cx = _fill_cstate_usage(dev, 0);
		cx->valid = 1;	/* C1 is always valid */
		cx->cpu_state = PWRDM_POWER_ON;
		cx->mpu_state = PWRDM_POWER_ON;
		dev->state_count++;
		drv.state_count++;

		/* C2 - CPU0 OFF + CPU1 OFF + MPU CSWR */
		_fill_cstate(&drv, 1, "MPUSS CSWR", CPUIDLE_FLAG_COUPLED);
		cx = _fill_cstate_usage(dev, 1);
		cx->cpu_state = PWRDM_POWER_OFF;
		cx->mpu_state = PWRDM_POWER_CSWR;
		dev->state_count++;
		drv.state_count++;

		/* C3 - CPU0 OFF + CPU1 OFF + MPU OSWR */
		_fill_cstate(&drv, 2, "MPUSS OSWR", CPUIDLE_FLAG_COUPLED);
		cx = _fill_cstate_usage(dev, 2);
		cx->cpu_state = PWRDM_POWER_OFF;
		cx->mpu_state = PWRDM_POWER_OSWR;
		dev->state_count++;
		drv.state_count++;

		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ON, &cpu_id);
		if (!drv_reg) {
			/* Copy over */
			omap4_idle_driver = drv;
			if (!cpuidle_register_driver(&omap4_idle_driver))
				drv_reg = false;
		}

		if (cpuidle_register_device(dev)) {
			pr_err("%s: CPUidle register failed\n", __func__);
				return -EIO;
		}
	}

	return 0;
}
