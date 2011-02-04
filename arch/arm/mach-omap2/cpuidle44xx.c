/*
 * OMAP4 CPU IDLE Routines
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/clockchips.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <mach/omap4-common.h>
#include <mach/omap4-wakeupgen.h>
#include "pm.h"
#include "prm.h"
#include "pm.h"
#include "cm.h"
#include "cm-regbits-44xx.h"
#include "clock.h"

#ifdef CONFIG_CPU_IDLE

#define OMAP4_MAX_STATES	4

/* C1 - CPU0 WFI + CPU1 OFF + MPU ON   + CORE ON  */
#define OMAP4_STATE_C1		0
/* C2 - CPU0 INA + CPU1 OFF + MPU INA  + CORE INA  */
#define OMAP4_STATE_C2		1
/* C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE OSWR  */
#define OMAP4_STATE_C3		2
/* C4 - CPU0 OFF + CPU1 OFF + MPU OSWR + CORE OSWR */
#define OMAP4_STATE_C4		3

struct omap4_processor_cx {
	u8 valid;
	u8 type;
	u32 sleep_latency;
	u32 wakeup_latency;
	u32 cpu0_state;
	u32 cpu1_state;
	u32 mpu_state;
	u32 mpu_logic_state;
	u32 core_state;
	u32 core_logic_state;
	u32 threshold;
	u32 flags;
	const char *desc;
};

struct omap4_processor_cx omap4_power_states[OMAP4_MAX_STATES];
struct omap4_processor_cx current_cx_state;
struct powerdomain *mpu_pd, *cpu1_pd, *core_pd;

static struct cpuidle_params cpuidle_params_table[] = {
	/* C1 - CPU0 WFI + CPU1 OFF + MPU ON   + CORE ON  */
	{1,	2,	2,	5},
	/* C2 - CPU0 INA + CPU1 OFF + MPU INA  + CORE INA  */
	{1,	140,	160,	300},
	/* C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE OSWR  */
	{1,	1516,	3230,	15000},
	/* C4 - CPU0 OFF + CPU1 OFF + MPU OSWR + CORE OSWR */
	{1,	1644,	3298,	39000},
};

static int omap4_idle_bm_check(void)
{
	if (!omap4_can_sleep())
		return 1;
	return 0;
}

/**
 * omap4_enter_idle - Programs OMAP4 to enter the specified state
 * @dev: cpuidle device
 * @state: The target state to be programmed
 *
 * Called from the CPUidle framework to program the device to the
 * specified low power state selected by the governor.
 * Returns the amount of time spent in the low power state.
 */
static int omap4_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_state *state)
{
	struct omap4_processor_cx *cx = cpuidle_get_statedata(state);
	struct timespec ts_preidle, ts_postidle, ts_idle;
	u32 cpu1_state;
	int cpu_id = smp_processor_id();

	/* Used to keep track of the total time in idle */
	getnstimeofday(&ts_preidle);

	local_irq_disable();
	local_fiq_disable();

	/*
	 * Do only WFI for non-boot CPU(aux cores)
	 */
	if (dev->cpu) {
		wmb();
		do_wfi();
		goto return_sleep_time;
	}

	/*
	 * Do only a WFI as long as CPU1 is online
	 */
	if (num_online_cpus() > 1) {
		wmb();
		do_wfi();
		goto return_sleep_time;
	}

	/*
	 * Hold on till CPU1 hits OFF
	 */
	cpu1_state = pwrdm_read_pwrst(cpu1_pd);
	if (cpu1_state != PWRDM_POWER_OFF) {
		wmb();
		do_wfi();
		goto return_sleep_time;
	}

	if (cx->type > OMAP4_STATE_C1)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);

#ifdef CONFIG_PM_DEBUG
	pwrdm_pre_transition();
#endif
	pwrdm_set_logic_retst(mpu_pd, cx->mpu_logic_state);
	omap4_set_pwrdm_state(mpu_pd, cx->mpu_state);
	pwrdm_set_logic_retst(core_pd, cx->core_logic_state);
	omap4_set_pwrdm_state(core_pd, cx->core_state);

	omap4_enter_sleep(dev->cpu, cx->cpu0_state);


	/* restore the MPU and CORE states to ON */
	omap4_set_pwrdm_state(mpu_pd, PWRDM_POWER_ON);
	omap4_set_pwrdm_state(core_pd, PWRDM_POWER_ON);

#ifdef CONFIG_PM_DEBUG
	pwrdm_post_transition();
#endif
	if (cx->type > OMAP4_STATE_C1)
		clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);

return_sleep_time:
	getnstimeofday(&ts_postidle);
	ts_idle = timespec_sub(ts_postidle, ts_preidle);

	local_irq_enable();
	local_fiq_enable();


	return ts_idle.tv_nsec / NSEC_PER_USEC + ts_idle.tv_sec * USEC_PER_SEC;;
}

/**
 * omap4_enter_idle_bm - Checks for any bus activity
 * @dev: cpuidle device
 * @state: The target state to be programmed
 *
 * Used for C states with CPUIDLE_FLAG_CHECK_BM flag set. This
 * function checks for any pending activity and then programs the
 * device to the specified or a safer state.
 */
static int omap4_enter_idle_bm(struct cpuidle_device *dev,
			       struct cpuidle_state *state)
{
	if ((state->flags & CPUIDLE_FLAG_CHECK_BM) && omap4_idle_bm_check()) {
		BUG_ON(!dev->safe_state);
		state = dev->safe_state;
	}

	dev->last_state = state;
	return omap4_enter_idle(dev, state);
}

DEFINE_PER_CPU(struct cpuidle_device, omap4_idle_dev);

/**
 * omap4_init_power_states - Initialises the OMAP4 specific C states.
 *
 */
void omap_init_power_states(void)
{
	/*
	 * C1 - CPU0 WFI + CPU1 OFF + MPU ON + CORE ON
	 */
	omap4_power_states[OMAP4_STATE_C1].valid =
			cpuidle_params_table[OMAP4_STATE_C1].valid;
	omap4_power_states[OMAP4_STATE_C1].type = OMAP4_STATE_C1;
	omap4_power_states[OMAP4_STATE_C1].sleep_latency =
			cpuidle_params_table[OMAP4_STATE_C1].sleep_latency;
	omap4_power_states[OMAP4_STATE_C1].wakeup_latency =
			cpuidle_params_table[OMAP4_STATE_C1].wake_latency;
	omap4_power_states[OMAP4_STATE_C1].threshold =
			cpuidle_params_table[OMAP4_STATE_C1].threshold;
	omap4_power_states[OMAP4_STATE_C1].cpu0_state = PWRDM_POWER_ON;
	omap4_power_states[OMAP4_STATE_C1].cpu1_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C1].mpu_state = PWRDM_POWER_ON;
	omap4_power_states[OMAP4_STATE_C1].mpu_logic_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C1].core_state = PWRDM_POWER_ON;
	omap4_power_states[OMAP4_STATE_C1].core_logic_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C1].flags = CPUIDLE_FLAG_TIME_VALID;
	omap4_power_states[OMAP4_STATE_C1].desc = "MPU ON + CORE ON";

	/*
	 * C2 - CPU0 INA + CPU1 OFF + MPU INA + CORE INA
	 */
	omap4_power_states[OMAP4_STATE_C2].valid =
			cpuidle_params_table[OMAP4_STATE_C2].valid;
	omap4_power_states[OMAP4_STATE_C2].type = OMAP4_STATE_C2;
	omap4_power_states[OMAP4_STATE_C2].sleep_latency =
			cpuidle_params_table[OMAP4_STATE_C2].sleep_latency;
	omap4_power_states[OMAP4_STATE_C2].wakeup_latency =
			cpuidle_params_table[OMAP4_STATE_C2].wake_latency;
	omap4_power_states[OMAP4_STATE_C2].threshold =
			cpuidle_params_table[OMAP4_STATE_C2].threshold;
	omap4_power_states[OMAP4_STATE_C2].cpu0_state = PWRDM_POWER_INACTIVE;
	omap4_power_states[OMAP4_STATE_C2].cpu1_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C2].mpu_state = PWRDM_POWER_INACTIVE;
	omap4_power_states[OMAP4_STATE_C2].mpu_logic_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C2].core_state = PWRDM_POWER_INACTIVE;
	omap4_power_states[OMAP4_STATE_C2].core_logic_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C2].flags = CPUIDLE_FLAG_TIME_VALID |
		CPUIDLE_FLAG_CHECK_BM;
	omap4_power_states[OMAP4_STATE_C2].desc = "MPU INA + CORE INA";

	/*
	 * C3 - CPU0 OFF + CPU1 OFF + MPU CSWR + CORE OSWR
	 */
	omap4_power_states[OMAP4_STATE_C3].valid =
			cpuidle_params_table[OMAP4_STATE_C3].valid;
	omap4_power_states[OMAP4_STATE_C3].type = OMAP4_STATE_C3;
	omap4_power_states[OMAP4_STATE_C3].sleep_latency =
			cpuidle_params_table[OMAP4_STATE_C3].sleep_latency;
	omap4_power_states[OMAP4_STATE_C3].wakeup_latency =
			cpuidle_params_table[OMAP4_STATE_C3].wake_latency;
	omap4_power_states[OMAP4_STATE_C3].threshold =
			cpuidle_params_table[OMAP4_STATE_C3].threshold;
	omap4_power_states[OMAP4_STATE_C3].cpu0_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C3].cpu1_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C3].mpu_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C3].mpu_logic_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C3].core_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C3].core_logic_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C3].flags = CPUIDLE_FLAG_TIME_VALID |
		CPUIDLE_FLAG_CHECK_BM;
	omap4_power_states[OMAP4_STATE_C3].desc = "MPU CSWR + CORE OSWR";

	/*
	 * C4 - CPU0 OFF + CPU1 OFF + MPU OSWR + CORE OSWR
	 */
	omap4_power_states[OMAP4_STATE_C4].valid =
			cpuidle_params_table[OMAP4_STATE_C4].valid;
	omap4_power_states[OMAP4_STATE_C4].type = OMAP4_STATE_C4;
	omap4_power_states[OMAP4_STATE_C4].sleep_latency =
			cpuidle_params_table[OMAP4_STATE_C4].sleep_latency;
	omap4_power_states[OMAP4_STATE_C4].wakeup_latency =
			cpuidle_params_table[OMAP4_STATE_C4].wake_latency;
	omap4_power_states[OMAP4_STATE_C4].threshold =
			cpuidle_params_table[OMAP4_STATE_C4].threshold;
	omap4_power_states[OMAP4_STATE_C4].cpu0_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C4].cpu1_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C4].mpu_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C4].mpu_logic_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C4].core_state = PWRDM_POWER_RET;
	omap4_power_states[OMAP4_STATE_C4].core_logic_state = PWRDM_POWER_OFF;
	omap4_power_states[OMAP4_STATE_C4].flags = CPUIDLE_FLAG_TIME_VALID |
		CPUIDLE_FLAG_CHECK_BM;
	omap4_power_states[OMAP4_STATE_C4].desc = "MPU OSWR + CORE OSWR";
}

struct cpuidle_driver omap4_idle_driver = {
	.name =		"omap4_idle",
	.owner =	THIS_MODULE,
};

/**
 * omap4_idle_init - Init routine for OMAP4 idle
 *
 * Registers the OMAP4 specific cpuidle driver with the cpuidle
 * framework with the valid set of states.
 */
int __init omap4_idle_init(void)
{
	int cpu_id, i, count = 0;
	struct omap4_processor_cx *cx;
	struct cpuidle_state *state;
	struct cpuidle_device *dev;

	mpu_pd = pwrdm_lookup("mpu_pwrdm");
	cpu1_pd = pwrdm_lookup("cpu1_pwrdm");
	core_pd = pwrdm_lookup("core_pwrdm");

	omap_init_power_states();
	cpuidle_register_driver(&omap4_idle_driver);

	for_each_cpu(cpu_id, cpu_online_mask) {
		pr_err("CPUidle for CPU%d registered\n", cpu_id);
		dev = &per_cpu(omap4_idle_dev, cpu_id);
		dev->cpu = cpu_id;
		count = 0;
		for (i = OMAP4_STATE_C1; i < OMAP4_MAX_STATES; i++) {
			cx = &omap4_power_states[i];
			state = &dev->states[count];

			if (!cx->valid)
				continue;
			cpuidle_set_statedata(state, cx);
			state->exit_latency = cx->sleep_latency +
							cx->wakeup_latency;
			state->target_residency = cx->threshold;
			state->flags = cx->flags;
			if (cx->type == OMAP4_STATE_C1)
				dev->safe_state = state;
			state->enter = (state->flags & CPUIDLE_FLAG_CHECK_BM) ?
					omap4_enter_idle_bm : omap4_enter_idle;
			sprintf(state->name, "C%d", count+1);
			strncpy(state->desc, cx->desc, CPUIDLE_DESC_LEN);
			count++;
		}

		if (!count)
			return -EINVAL;
		dev->state_count = count;

		if (cpuidle_register_device(dev)) {
			printk(KERN_ERR "%s: CPUidle register device failed\n",
				__func__);
			return -EIO;
		}
	}

	return 0;
}
#else
int __init omap4_idle_init(void)
{
	return 0;
}
#endif /* CONFIG_CPU_IDLE */
