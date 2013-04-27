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
	u32 core_state;
};

static struct omap4_idle_statedata omap4_idle_data[] = {
	/* C1 - CPU0 ON + CPU1 ON + MPU ON + CORE ON */
	{
		.cpu_state = PWRDM_POWER_ON,
		.mpu_state = PWRDM_POWER_ON,
		.core_state = PWRDM_POWER_ON,
	},
	/* C2 - CPU0 OFF + CPU1 OFF + MPU INA + CORE INA */
	{
		.cpu_state = PWRDM_POWER_OFF,
		.mpu_state = PWRDM_POWER_INACTIVE,
		.core_state = PWRDM_POWER_INACTIVE,
	},
	/* C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE CSWR */
	{
		.cpu_state = PWRDM_POWER_OFF,
		.mpu_state = PWRDM_POWER_CSWR,
		.core_state = PWRDM_POWER_CSWR,
	},
	/* C4 - CPU0 OFF + CPU1 OFF + MPU OSWR + CORE OSWR */
	{
		.cpu_state = PWRDM_POWER_OFF,
		.mpu_state = PWRDM_POWER_OSWR,
		.core_state = PWRDM_POWER_OSWR,
	},
};

static struct powerdomain *mpu_pd, *core_pd, *cpu_pd[NR_CPUS];
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

#include <asm/hardware/gic.h>


static int omap4_enter_idle_coupled(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			int index)
{
	struct omap4_idle_statedata *cx = &omap4_idle_data[index];
	int cpu_id = smp_processor_id();
	u32 mpuss_context_lost = 0;

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
		omap_set_pwrdm_state(core_pd, cx->core_state);

		/*
		 * Call idle CPU cluster PM enter notifier chain
		 * to save GIC and wakeupgen context.
		 */
		if (pwrdm_power_state_le(cx->mpu_state, PWRDM_POWER_OSWR))
			cpu_cluster_pm_enter();
	}

	if (dev->cpu == 0)
		omap_enter_lowpower(dev->cpu, cx->cpu_state);
	else
		omap_enter_lowpower(dev->cpu, PWRDM_POWER_OFF);

	cpu_done[dev->cpu] = true;

	mpuss_context_lost = omap_mpuss_read_prev_context_state();

	omap_set_pwrdm_state(mpu_pd, PWRDM_POWER_ON);
	omap_set_pwrdm_state(core_pd, PWRDM_POWER_ON);

	/* Wakeup CPU1 only if it is not offlined */
	if (dev->cpu == 0 && cpumask_test_cpu(1, cpu_online_mask)) {
		/*
		 * GIC distributor control register has changed between
		 * CortexA9 r1pX and r2pX. The Control Register secure
		 * banked version is now composed of 2 bits:
		 * bit 0 == Secure Enable
		 * bit 1 == Non-Secure Enable
		 * The Non-Secure banked register has not changed
		 * Because the ROM Code is based on the r1pX GIC, the CPU1
		 * GIC restoration will cause a problem to CPU0 Non-Secure SW.
		 * The workaround must be:
		 * 1) Before doing the CPU1 wakeup, CPU0 must disable
		 * the GIC distributor and wait until it will be enabled by CPU1
		 * 2) CPU1 must re-enable the GIC distributor on
		 * it's wakeup path.
		 */
		if (IS_PM44XX_ERRATUM(PM_OMAP4_ROM_SMP_BOOT_ERRATUM_xxx) &&
		    mpuss_context_lost)
			gic_dist_disable();

		clkdm_wakeup(cpu_clkdm[1]);
		clkdm_allow_idle(cpu_clkdm[1]);

		if (IS_PM44XX_ERRATUM(PM_OMAP4_ROM_SMP_BOOT_ERRATUM_xxx) &&
		    mpuss_context_lost) {
			while (gic_dist_disabled()) {
				udelay(1);
				cpu_relax();
			}
			gic_timer_retrigger();
		}
	}

	if (IS_PM44XX_ERRATUM(PM_OMAP4_ROM_SMP_BOOT_ERRATUM_xxx) && dev->cpu)
		gic_dist_enable();

	/*
	 * Call idle CPU PM exit notifier chain to restore
	 * VFP and per CPU IRQ context.
	 */
	cpu_pm_exit();

	/*
	 * Call idle CPU cluster PM exit notifier chain
	 * to restore GIC and wakeupgen context.
	 */
	if (mpuss_context_lost)
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
	.states = {
		{
			/* C1 - CPU0 ON + CPU1 ON + MPU ON + CORE ON*/
			.exit_latency = 2 + 2,
			.target_residency = 5,
			.flags = CPUIDLE_FLAG_TIME_VALID,
			.enter = omap4_enter_idle_simple,
			.name = "C1",
			.desc = "MPUSS ON CORE ON",
			.disable = 0,
		},
		{
			/* C2 - CPU0 OFF + CPU1 OFF + MPU INA + CORE INA*/
			.exit_latency = 350,
			.target_residency = 350,
			.flags = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_COUPLED,
			.enter = omap4_enter_idle_coupled,
			.name = "C2",
			.desc = "MPUSS INA CORE INA",
			.disable = 0,
		},
		{
			/* C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE CSWR*/
			.exit_latency = 328 + 440,
			.target_residency = 960,
			.flags = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_COUPLED,
			.enter = omap4_enter_idle_coupled,
			.name = "C3",
			.desc = "MPUSS CSWR CORE CSWR",
			.disable = 0,
		},
		{
			/* C4 - CPU0 OFF + CPU1 OFF + MPU OSWR + CORE OSWR*/
			.exit_latency = 460 + 518,
			.target_residency = 1100,
			.flags = CPUIDLE_FLAG_TIME_VALID | CPUIDLE_FLAG_COUPLED,
			.enter = omap4_enter_idle_coupled,
			.name = "C4",
			.desc = "MPUSS OSWR CORE OSWR",
			.disable = 0,
		},
	},
	.state_count = ARRAY_SIZE(omap4_idle_data),
	.safe_state_index = 0,
};

/**
 * omap4_idle_init - Init routine for OMAP4 idle
 *
 * Registers the OMAP4 specific cpuidle driver to the cpuidle
 * framework with the valid set of states.
 */
int __init omap4_idle_init(void)
{
	struct cpuidle_device *dev;
	int res;
	unsigned int cpu_id = 0;

	mpu_pd = pwrdm_lookup("mpu_pwrdm");
	cpu_pd[0] = pwrdm_lookup("cpu0_pwrdm");
	cpu_pd[1] = pwrdm_lookup("cpu1_pwrdm");
	core_pd = pwrdm_lookup("core_pwrdm");
	if ((!mpu_pd) || (!cpu_pd[0]) || (!cpu_pd[1]) || (!core_pd))
		return -ENODEV;

	cpu_clkdm[0] = clkdm_lookup("mpu0_clkdm");
	cpu_clkdm[1] = clkdm_lookup("mpu1_clkdm");
	if (!cpu_clkdm[0] || !cpu_clkdm[1])
		return -ENODEV;

	res = cpuidle_register_driver(&omap4_idle_driver);
	if (res) {
		pr_err("%s: CPUidle register failed %u\n", __func__, res);
		return res;
	}

	for_each_cpu(cpu_id, cpu_online_mask) {
		dev = &per_cpu(omap4_idle_dev, cpu_id);
		dev->cpu = cpu_id;
		dev->coupled_cpus = *cpu_online_mask;

		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ON, &cpu_id);

		if (cpuidle_register_device(dev)) {
			pr_err("%s: CPUidle register failed\n", __func__);
				return -EIO;
		}
	}

	return 0;
}
