/*
 * OMAP powerdomain control
 *
 * Copyright (C) 2007-2008, 2011-2012 Texas Instruments, Inc.
 * Copyright (C) 2007-2011 Nokia Corporation
 *
 * Written by Paul Walmsley
 * Added OMAP4 specific support by Abhijit Pagare <abhijitpagare@ti.com>
 * State counting code by Tero Kristo <tero.kristo@nokia.com>
 *
 * Contains some code previously from mach-omap2/pm-debug.c, which was:
 * Copyright (C) 2005 Texas Instruments, Inc.
 * Copyright (C) 2006-2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/seq_file.h>

#include <trace/events/power.h>

#include "cm2xxx_3xxx.h"
#include "prcm44xx.h"
#include "cm44xx.h"
#include "prm2xxx_3xxx.h"
#include "prm44xx.h"

#include <asm/cpu.h>

#include "powerdomain.h"
#include "clockdomain.h"

#include "soc.h"
#include "pm.h"

#define PWRDM_TRACE_STATES_FLAG	(1<<31)

/* Types of sleep_switch used in pwrdm_set_fpwrst() */
#define ALREADYACTIVE_SWITCH		0
#define FORCEWAKEUP_SWITCH		1
#define LOWPOWERSTATE_SWITCH		2
#define ERROR_SWITCH			3

/* pwrdm_list contains all registered struct powerdomains */
static LIST_HEAD(pwrdm_list);

static struct pwrdm_ops *arch_pwrdm;

/*
 * _fpwrst_names: human-readable functional powerstate names - should match
 *    the enum pwrdm_func_state order and names
 */
static const char * const _fpwrst_names[] = {
	"OFF", "OSWR", "CSWR", "INACTIVE", "ON"
};

/* Private functions */

static struct powerdomain *_pwrdm_lookup(const char *name)
{
	struct powerdomain *pwrdm, *temp_pwrdm;

	pwrdm = NULL;

	list_for_each_entry(temp_pwrdm, &pwrdm_list, node) {
		if (!strcmp(name, temp_pwrdm->name)) {
			pwrdm = temp_pwrdm;
			break;
		}
	}

	return pwrdm;
}

/**
 * _pwrdm_register - register a powerdomain
 * @pwrdm: struct powerdomain * to register
 *
 * Adds a powerdomain to the internal powerdomain list.  Returns
 * -EINVAL if given a null pointer, -EEXIST if a powerdomain is
 * already registered by the provided name, or 0 upon success.
 */
static int _pwrdm_register(struct powerdomain *pwrdm)
{
	int i;
	struct voltagedomain *voltdm;

	if (!pwrdm || !pwrdm->name)
		return -EINVAL;

	if (cpu_is_omap44xx() &&
	    pwrdm->prcm_partition == OMAP4430_INVALID_PRCM_PARTITION) {
		pr_err("powerdomain: %s: missing OMAP4 PRCM partition ID\n",
		       pwrdm->name);
		return -EINVAL;
	}

	if (_pwrdm_lookup(pwrdm->name))
		return -EEXIST;

	voltdm = voltdm_lookup(pwrdm->voltdm.name);
	if (!voltdm) {
		pr_err("powerdomain: %s: voltagedomain %s does not exist\n",
		       pwrdm->name, pwrdm->voltdm.name);
		return -EINVAL;
	}
	pwrdm->voltdm.ptr = voltdm;
	INIT_LIST_HEAD(&pwrdm->voltdm_node);
	voltdm_add_pwrdm(voltdm, pwrdm);
	spin_lock_init(&pwrdm->_lock);

	list_add(&pwrdm->node, &pwrdm_list);

	/* Initialize the powerdomain's state counter */
	for (i = 0; i < PWRDM_FPWRSTS_COUNT; i++)
		pwrdm->fpwrst_counter[i] = 0;

	arch_pwrdm->pwrdm_wait_transition(pwrdm);
	pwrdm->fpwrst = pwrdm_read_fpwrst(pwrdm);
	pwrdm->fpwrst_counter[pwrdm->fpwrst - PWRDM_FPWRST_OFFSET] = 1;
#ifdef CONFIG_PM_DEBUG
	pwrdm->timer = sched_clock();
#endif

	return 0;
}

/**
 * _pwrdm_save_clkdm_state_and_activate - prepare for power state change
 * @pwrdm: struct powerdomain * to operate on
 * @pwrst: power state to switch to
 * @hwsup: ptr to a bool to return whether the clkdm is hardware-supervised
 *
 * Determine whether the powerdomain needs to be turned on before
 * attempting to switch power states.  Called by
 * pwrdm_set_fpwrst().  NOTE that if the powerdomain contains
 * multiple clockdomains, this code assumes that the first clockdomain
 * supports software-supervised wakeup mode - potentially a problem.
 * Returns the power state switch mode currently in use (see the
 * "Types of sleep_switch" comment above).
 */
static u8 _pwrdm_save_clkdm_state_and_activate(struct powerdomain *pwrdm,
					       u8 pwrst, bool *hwsup)
{
	int curr_pwrst;
	u8 sleep_switch;

	curr_pwrst = arch_pwrdm->pwrdm_read_pwrst(pwrdm);
	if (curr_pwrst < 0) {
		WARN_ON(1);
		sleep_switch = ERROR_SWITCH;
	} else if (curr_pwrst < PWRDM_POWER_ON) {
		if (curr_pwrst > pwrst &&
		    pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE &&
		    arch_pwrdm->pwrdm_set_lowpwrstchange) {
			sleep_switch = LOWPOWERSTATE_SWITCH;
		} else {
			*hwsup = clkdm_in_hwsup(pwrdm->pwrdm_clkdms[0]);
			clkdm_wakeup_nolock(pwrdm->pwrdm_clkdms[0]);
			sleep_switch = FORCEWAKEUP_SWITCH;
		}
	} else {
		sleep_switch = ALREADYACTIVE_SWITCH;
	}

	return sleep_switch;
}

/**
 * _pwrdm_restore_clkdm_state - restore the clkdm hwsup state after pwrst change
 * @pwrdm: struct powerdomain * to operate on
 * @sleep_switch: return value from _pwrdm_save_clkdm_state_and_activate()
 * @hwsup: should @pwrdm's first clockdomain be set to hardware-supervised mode?
 *
 * Restore the clockdomain state perturbed by
 * _pwrdm_save_clkdm_state_and_activate(), and call the power state
 * bookkeeping code.  Called by pwrdm_set_fpwrst().  NOTE that if
 * the powerdomain contains multiple clockdomains, this assumes that
 * the first associated clockdomain supports either
 * hardware-supervised idle control in the register, or
 * software-supervised sleep.  No return value.
 */
static void _pwrdm_restore_clkdm_state(struct powerdomain *pwrdm,
				       u8 sleep_switch, bool hwsup)
{
	switch (sleep_switch) {
	case FORCEWAKEUP_SWITCH:
		if (hwsup)
			clkdm_allow_idle_nolock(pwrdm->pwrdm_clkdms[0]);
		else
			clkdm_sleep_nolock(pwrdm->pwrdm_clkdms[0]);
		break;
	case LOWPOWERSTATE_SWITCH:
		if (pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE &&
		    arch_pwrdm->pwrdm_set_lowpwrstchange)
			arch_pwrdm->pwrdm_set_lowpwrstchange(pwrdm);
		pwrdm_state_switch_nolock(pwrdm);
		break;
	}
}

/**
 * _pwrdm_pwrst_is_controllable - can software change the powerdomain pwrst?
 * @pwrdm: struct powerdomain * to test
 *
 * If the kernel can program the power state that the powerdomain
 * @pwrdm should enter next, return 1; otherwise, return 0.
 */
static bool _pwrdm_pwrst_is_controllable(struct powerdomain *pwrdm)
{
	return (!pwrdm->pwrsts || pwrdm->pwrsts == PWRSTS_ON) ? 0 : 1;
}

/**
 * _pwrdm_pwrst_can_change - can the power state of @pwrdm change?
 * @pwrdm: struct powerdomain * to test
 *
 * If the power state of the powerdomain represented by @pwrdm can
 * change (i.e., is not always on), and the kernel has some way to
 * detect this, return 1; otherwise, return 0.  XXX The current
 * implementation of this is based on an assumption and has not been
 * verified against all OMAPs.
 */
static bool _pwrdm_pwrst_can_change(struct powerdomain *pwrdm)
{
	return _pwrdm_pwrst_is_controllable(pwrdm);
}

/**
 * _pwrdm_logic_retst_is_controllable - can software change the logic retst?
 * @pwrdm: struct powerdomain * to test
 *
 * If the kernel can program the power state that the powerdomain
 * @pwrdm logic should enter when the @pwrdm enters the RETENTION
 * power state, return 1; otherwise, return 0.
 */
static bool _pwrdm_logic_retst_is_controllable(struct powerdomain *pwrdm)
{
	return (!pwrdm->pwrsts_logic_ret ||
		pwrdm->pwrsts_logic_ret == PWRSTS_RET) ? 0 : 1;
}

/**
 * _pwrdm_logic_retst_can_change - can the logic retst change on @pwrdm?
 * @pwrdm: struct powerdomain * to test
 *
 * If the logic powerstate for the powerdomain represented by @pwrdm
 * can ever be something other than the powerdomain's powerstate, and
 * the kernel has some way to detect this, return 1; otherwise, return
 * 0.  XXX The current implementation of this is based on an
 * assumption and has not been verified against all OMAPs.
 */
static bool _pwrdm_logic_retst_can_change(struct powerdomain *pwrdm)
{
	return _pwrdm_logic_retst_is_controllable(pwrdm);
}

/**
 * Search down then up for a valid state from a list of allowed
 * states.  Used by states conversion functions (_pwrdm_fpwrst_to_*)
 * to look for allowed power and logic states for a powerdomain.
 * _pwrdm_fpwrst_to_pwrst - Convert functional (i.e. logical) to
 * internal (i.e. registers) values for the power domains states.
 * @pwrdm: struct powerdomain * to convert the values for
 * @fpwrst: functional power state
 * @pwrdm_pwrst: ptr to u8 to return the power state in
 * @logic_retst: ptr to u8 to return the logic retention state in
 *
 * Returns the internal power state value for the power domain, or
 * -EINVAL in case of invalid parameters passed in.
 */
static int _pwrdm_fpwrst_to_pwrst(struct powerdomain *pwrdm, u8 fpwrst,
				  u8 *pwrdm_pwrst, u8 *logic_retst)
{
	switch (fpwrst) {
	case PWRDM_FUNC_PWRST_ON:
		*pwrdm_pwrst = PWRDM_POWER_ON;
		*logic_retst = PWRDM_POWER_RET;
		break;
	case PWRDM_FUNC_PWRST_INACTIVE:
		*pwrdm_pwrst = PWRDM_POWER_INACTIVE;
		*logic_retst = PWRDM_POWER_RET;
		break;
	case PWRDM_FUNC_PWRST_CSWR:
		*pwrdm_pwrst = PWRDM_POWER_RET;
		*logic_retst = PWRDM_POWER_RET;
		break;
	case PWRDM_FUNC_PWRST_OSWR:
		*pwrdm_pwrst = PWRDM_POWER_RET;
		*logic_retst = PWRDM_POWER_OFF;
		break;
	case PWRDM_FUNC_PWRST_OFF:
		*pwrdm_pwrst = PWRDM_POWER_OFF;
		/*
		 * logic_retst is set to PWRDM_POWER_RET in this case
		 * since the actual value does not matter, and because
		 * some powerdomains don't support a logic_retst of
		 * OFF.  XXX Maybe there's some way to indicate a
		 * 'don't care' value here?
		 */
		*logic_retst = PWRDM_POWER_RET;
		break;
	default:
		return -EINVAL;
	}

	pr_debug("powerdomain %s: convert fpwrst %0x to pwrst %0x\n",
		 pwrdm->name, fpwrst, *pwrdm_pwrst);

	return 0;
}

/**
 * _pwrdm_pwrst_to_fpwrst - Convert internal (i.e. registers) to
 * functional (i.e. logical) values for the power domains states.
 * @pwrdm: struct powerdomain * to convert the values for
 * @pwrst: internal powerdomain power state
 * @logic: internal powerdomain logic power state
 * @fpwrst: pointer to a u8 to store the corresponding functional power state to
 *
 * Returns the functional power state value for the power domain, or
 * -EINVAL in case of invalid parameters passed in.  @pwrdm, @logic, and @pwrst
 * are passed in, along with a pointer to the location to store the fpwrst to
 * in @fpwrst.
 */
static int _pwrdm_pwrst_to_fpwrst(struct powerdomain *pwrdm, u8 pwrst, u8 logic,
				  u8 *fpwrst)
{
	switch (pwrst) {
	case PWRDM_POWER_ON:
		*fpwrst = PWRDM_FUNC_PWRST_ON;
		break;
	case PWRDM_POWER_INACTIVE:
		*fpwrst = PWRDM_FUNC_PWRST_INACTIVE;
		break;
	case PWRDM_POWER_RET:
		if (logic == PWRDM_POWER_OFF)
			*fpwrst = PWRDM_FUNC_PWRST_OSWR;
		else if (logic == PWRDM_POWER_RET)
			*fpwrst = PWRDM_FUNC_PWRST_CSWR;
		else
			return -EINVAL;
		break;
	case PWRDM_POWER_OFF:
		*fpwrst = PWRDM_FUNC_PWRST_OFF;
		break;
	default:
		return -EINVAL;
	}

	pr_debug("powerdomain: convert pwrst (%0x,%0x) to fpwrst %0x\n",
		 pwrst, logic, *fpwrst);

	return 0;
}

/**
 * _set_logic_retst_and_pwrdm_pwrst - program logic retst and pwrdm next pwrst
 * @pwrdm: struct powerdomain * to program
 * @logic: logic retention state to attempt to program
 * @pwrst: powerdomain next-power-state to program
 *
 * Program the next-power-state and logic retention power state of the
 * powerdomain represented by @pwrdm to @pwrst and @logic,
 * respectively.  If the powerdomain next-power-state is not
 * software-controllable, returns 0; otherwise, passes along the
 * return value from pwrdm_set_logic_retst() if there is an error
 * returned by that function, otherwise, passes along the return value
 * from pwrdm_set_next_pwrst()
 */
static int _set_logic_retst_and_pwrdm_pwrst(struct powerdomain *pwrdm,
					    u8 logic, u8 pwrst)
{
	int ret;

	/*
	 * XXX Should return an error, but this means that our PM code
	 * will need to be much more careful about what it programs
	 */
	if (!_pwrdm_pwrst_is_controllable(pwrdm))
		return 0;

	if (!(pwrdm->pwrsts & (1 << pwrst)))
		return -EINVAL;

	if (pwrdm->pwrsts_logic_ret && pwrst == PWRDM_POWER_RET) {
		if (!(pwrdm->pwrsts_logic_ret & (1 << logic)))
			return -EINVAL;

		ret = arch_pwrdm->pwrdm_set_logic_retst(pwrdm, logic);
		if (ret) {
			pr_err("%s: unable to set logic state %0x of powerdomain: %s\n",
			       __func__, logic, pwrdm->name);
			return ret;
		}
	}

	ret = arch_pwrdm->pwrdm_set_next_pwrst(pwrdm, pwrst);
	if (ret)
		pr_err("%s: unable to set power state %0x of powerdomain: %s\n",
		       __func__, pwrst, pwrdm->name);

	return ret;
}

/**
 * _pwrdm_read_next_fpwrst - get next powerdomain func power state (lockless)
 * @pwrdm: struct powerdomain * to get power state
 *
 * Return the powerdomain @pwrdm's next functional power state.
 * Caller must hold @pwrdm->_lock.  Returns -EINVAL if the powerdomain
 * pointer is null or returns the next power state upon success.
 */
static int _pwrdm_read_next_fpwrst(struct powerdomain *pwrdm)
{
	int next_pwrst, next_logic, ret;
	u8 fpwrst;

	next_pwrst = arch_pwrdm->pwrdm_read_next_pwrst(pwrdm);
	if (next_pwrst < 0)
		return next_pwrst;

	next_logic = next_pwrst;
	if (_pwrdm_logic_retst_can_change(pwrdm) &&
	    arch_pwrdm->pwrdm_read_logic_pwrst) {
		next_logic = arch_pwrdm->pwrdm_read_logic_pwrst(pwrdm);
		if (next_logic < 0)
			return next_logic;
	}
	ret = _pwrdm_pwrst_to_fpwrst(pwrdm, next_pwrst, next_logic, &fpwrst);

	return (ret) ? ret : fpwrst;
}

/**
 * _pwrdm_read_fpwrst - get current func powerdomain power state (lockless)
 * @pwrdm: struct powerdomain * to get current functional power state
 *
 * Return the powerdomain @pwrdm's current functional power state.
 * Returns -EINVAL if the powerdomain pointer is null or returns the
 * current power state upon success.
 */
static int _pwrdm_read_fpwrst(struct powerdomain *pwrdm)
{
	int pwrst, logic_pwrst, ret;
	u8 fpwrst;

	if (!_pwrdm_pwrst_can_change(pwrdm))
		return PWRDM_FUNC_PWRST_ON;

	pwrst = arch_pwrdm->pwrdm_read_pwrst(pwrdm);
	if (pwrst < 0)
		return pwrst;

	logic_pwrst = pwrst;
	if (_pwrdm_logic_retst_can_change(pwrdm) &&
	    arch_pwrdm->pwrdm_read_logic_pwrst) {
		logic_pwrst = arch_pwrdm->pwrdm_read_logic_pwrst(pwrdm);
		if (logic_pwrst < 0)
			return logic_pwrst;
	}

	ret = _pwrdm_pwrst_to_fpwrst(pwrdm, pwrst, logic_pwrst, &fpwrst);

	return (ret) ? ret : fpwrst;
}

/**
 * _pwrdm_read_prev_fpwrst - get previous powerdomain func pwr state (lockless)
 * @pwrdm: struct powerdomain * to get previous functional power state
 *
 * Return the powerdomain @pwrdm's previous functional power state.
 * Returns -EINVAL if the powerdomain pointer is null or returns the
 * previous functional power state upon success.
 */
static int _pwrdm_read_prev_fpwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;
	int pwrst, logic_pwrst;
	u8 fpwrst;

	if (!_pwrdm_pwrst_can_change(pwrdm))
		return PWRDM_FUNC_PWRST_ON;

	pwrst = arch_pwrdm->pwrdm_read_prev_pwrst(pwrdm);
	if (pwrst < 0)
		return pwrst;

	logic_pwrst = pwrst;
	if (_pwrdm_logic_retst_can_change(pwrdm) &&
	    arch_pwrdm->pwrdm_read_prev_logic_pwrst) {
		logic_pwrst = arch_pwrdm->pwrdm_read_prev_logic_pwrst(pwrdm);
		if (logic_pwrst < 0)
			return logic_pwrst;
	}

	ret = _pwrdm_pwrst_to_fpwrst(pwrdm, pwrst, logic_pwrst, &fpwrst);

	return (ret) ? ret : fpwrst;
}

/**
 * _pwrdm_set_mem_onst - set memory power state while powerdomain ON
 * @pwrdm: struct powerdomain * to set
 * @bank: memory bank number to set (0-3)
 * @pwrst: one of the PWRDM_POWER_* macros
 *
 * Set the next power state @pwrst that memory bank @bank of the
 * powerdomain @pwrdm will enter when the powerdomain enters the ON
 * state.  @bank will be a number from 0 to 3, and represents different
 * types of memory, depending on the powerdomain.  Returns -EINVAL if
 * the powerdomain pointer is null or the target power state is not
 * not supported for this memory bank, -EEXIST if the target memory
 * bank does not exist or is not controllable, or returns 0 upon
 * success.
 */
static int _pwrdm_set_mem_onst(struct powerdomain *pwrdm, u8 bank, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->banks < (bank + 1))
		return -EEXIST;

	if (!(pwrdm->pwrsts_mem_on[bank] & (1 << pwrst)))
		return -EINVAL;

	pr_debug("powerdomain: %s: setting next memory powerstate for bank %0x while pwrdm-ON to %0x\n",
		 pwrdm->name, bank, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_mem_onst)
		ret = arch_pwrdm->pwrdm_set_mem_onst(pwrdm, bank, pwrst);

	return ret;
}

/**
 * _pwrdm_set_mem_retst - set memory power state while powerdomain in RET
 * @pwrdm: struct powerdomain * to set
 * @bank: memory bank number to set (0-3)
 * @pwrst: one of the PWRDM_POWER_* macros
 *
 * Set the next power state @pwrst that memory bank @bank of the
 * powerdomain @pwrdm will enter when the powerdomain enters the
 * RETENTION state.  Bank will be a number from 0 to 3, and represents
 * different types of memory, depending on the powerdomain.  @pwrst
 * will be either RETENTION or OFF, if supported.  Returns -EINVAL if
 * the powerdomain pointer is null or the target power state is not
 * not supported for this memory bank, -EEXIST if the target memory
 * bank does not exist or is not controllable, or returns 0 upon
 * success.
 */
static int _pwrdm_set_mem_retst(struct powerdomain *pwrdm, u8 bank, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->banks < (bank + 1))
		return -EEXIST;

	if (!(pwrdm->pwrsts_mem_ret[bank] & (1 << pwrst)))
		return -EINVAL;

	pr_debug("powerdomain: %s: setting next memory powerstate for bank %0x while pwrdm-RET to %0x\n",
		 pwrdm->name, bank, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_mem_retst)
		ret = arch_pwrdm->pwrdm_set_mem_retst(pwrdm, bank, pwrst);

	return ret;
}

/* XXX prev is wrong type */
/* XXX is sched_clock() correct to use here? */
/* Update timer for previous state */
static void _pwrdm_update_pwrst_time(struct powerdomain *pwrdm, int prev)
{
#ifdef CONFIG_PM_DEBUG
	s64 t;

	t = sched_clock();

	pwrdm->fpwrst_timer[prev - PWRDM_FPWRST_OFFSET] += t - pwrdm->timer;

	pwrdm->timer = t;
#endif
}

/**
 * _pwrdm_state_switch - record powerdomain usage data; track power state
 * (before powerdomain state transition)
 * @pwrdm: struct powerdomain * to observe
 *
 * If the powerdomain @pwrdm's current power state is not what we last
 * observed it to be, then increment the counter for that power state.
 * This is used to track context loss events, and for debugging.  Also
 * if CONFIG_PM_DEBUG=y, track the amount of time the powerdomain has
 * spent in the current power state.  Caller must hold pwrdm->_lock.
 * Intended to be called immediately before the powerdomain's power
 * state is likely to change.  XXX Note that the counts and durations
 * observed by this function may be inaccurate.  Powerdomains can
 * transition power states automatically, without the kernel being
 * involved -- for example, a device can DMA data from memory while
 * the MPU is asleep.  This function does not attempt to account for
 * that.  XXX It may be possible to skip this function completely if
 * PM debugging is not needed and off-mode and OSWR is disabled (e.g.,
 * no context loss events).  No return value.
 */
static void _pwrdm_state_switch(struct powerdomain *pwrdm)
{
	int fpwrst;

	fpwrst = _pwrdm_read_fpwrst(pwrdm);
	if (fpwrst != pwrdm->fpwrst)
		pwrdm->fpwrst_counter[fpwrst - PWRDM_FPWRST_OFFSET]++;

	_pwrdm_update_pwrst_time(pwrdm, pwrdm->fpwrst);

	pwrdm->fpwrst = fpwrst;
}

static int _pwrdm_pre_transition_cb(struct powerdomain *pwrdm, void *unused)
{
	pwrdm_clear_all_prev_pwrst(pwrdm);
	_pwrdm_state_switch(pwrdm);
	return 0;
}

/**
 * _pwrdm_post_transition_cb - record powerdomain usage data; track power state
 * (after powerdomain power state transition)
 * @pwrdm: struct powerdomain * to observe
 *
 * If the powerdomain @pwrdm's previous power state doesn't match our
 * recollection of the powerdomain's current power state, then
 * increment the counter for the previous power state.  And if the
 * powerdomain's previous power state doesn't match the current power
 * state, increment the counter for the current power state.  This
 * function is used to track context loss events, and for debugging.
 * Also if CONFIG_PM_DEBUG=y, track the approximate amount of time the
 * powerdomain has spent in the previous power state.  Caller must
 * hold pwrdm->_lock.  XXX Note that the counts and durations observed
 * by this function may be inaccurate.  Powerdomains can transition
 * power states automatically, without the kernel being involved --
 * for example, a device can DMA data from memory while the MPU is
 * asleep.  This function does not attempt to account for that.  XXX
 * It may be possible to skip this function completely if PM debugging
 * is not needed and off-mode and OSWR is disabled (e.g., no context
 * loss events).  No return value.
 */
static int _pwrdm_post_transition_cb(struct powerdomain *pwrdm, void *unused)
{
	int prev, next, fpwrst;
	int trace_state = 0;

	prev = _pwrdm_read_prev_fpwrst(pwrdm);
	if (pwrdm->fpwrst != prev)
		pwrdm->fpwrst_counter[prev - PWRDM_FPWRST_OFFSET]++;

	_pwrdm_update_pwrst_time(pwrdm, prev);

	/*
	 * If the power domain did not hit the desired state,
	 * generate a trace event with both the desired and hit states
	 */
	next = _pwrdm_read_next_fpwrst(pwrdm);
	if (next != prev) {
		trace_state = (PWRDM_TRACE_STATES_FLAG | next << 8 |
			       prev);
		trace_power_domain_target(pwrdm->name, trace_state,
					  smp_processor_id());
	}

	fpwrst = _pwrdm_read_fpwrst(pwrdm);
	if (fpwrst != prev)
		pwrdm->fpwrst_counter[fpwrst - PWRDM_FPWRST_OFFSET]++;

	pwrdm->fpwrst = fpwrst;

	return 0;
}

/* Public functions */

/**
 * pwrdm_register_platform_funcs - register powerdomain implementation fns
 * @po: func pointers for arch specific implementations
 *
 * Register the list of function pointers used to implement the
 * powerdomain functions on different OMAP SoCs.  Should be called
 * before any other pwrdm_register*() function.  Several function
 * pointers in @po are required to be non-null for the powerdomain
 * code to function: pwrdm_wait_transition, pwrdm_read_next_pwrst,
 * pwrdm_read_pwrst, pwrdm_set_next_pwrst, pwrdm_set_logic_retst, and
 * pwrdm_read_prev_pwrst.  Returns -EINVAL if @po is null, -EEXIST if
 * platform functions have already been registered, or 0 upon success.
 */
int pwrdm_register_platform_funcs(struct pwrdm_ops *po)
{
	if (!po)
		return -EINVAL;

	if (arch_pwrdm)
		return -EEXIST;

	if (!po->pwrdm_wait_transition || !po->pwrdm_read_next_pwrst ||
	    !po->pwrdm_read_pwrst || !po->pwrdm_set_next_pwrst ||
	    !po->pwrdm_set_logic_retst || !po->pwrdm_read_prev_pwrst)
		return -ENOENT;

	arch_pwrdm = po;

	return 0;
}

/**
 * pwrdm_register_pwrdms - register SoC powerdomains
 * @ps: pointer to an array of struct powerdomain to register
 *
 * Register the powerdomains available on a particular OMAP SoC.  Must
 * be called after pwrdm_register_platform_funcs().  May be called
 * multiple times.  Returns -EACCES if called before
 * pwrdm_register_platform_funcs(); -EINVAL if the argument @ps is
 * null; or 0 upon success.
 */
int pwrdm_register_pwrdms(struct powerdomain **ps)
{
	struct powerdomain **p = NULL;

	if (!arch_pwrdm)
		return -EEXIST;

	if (!ps)
		return -EINVAL;

	for (p = ps; *p; p++)
		_pwrdm_register(*p);

	return 0;
}

/**
 * pwrdm_complete_init - set up the powerdomain layer
 *
 * Do whatever is necessary to initialize registered powerdomains and
 * powerdomain code.  Currently, this programs the next power state
 * for each powerdomain to ON, and programs the memory bank power
 * states to follow the powerdomain power states.  This prevents
 * powerdomains from unexpectedly losing context or entering high
 * wakeup latency modes with non-power-management-enabled kernels.
 * Must be called after pwrdm_register_pwrdms().  Returns -EACCES if
 * called before pwrdm_register_pwrdms(), or 0 upon success.
 */
int pwrdm_complete_init(void)
{
	struct powerdomain *temp_p;
	int i;

	if (list_empty(&pwrdm_list))
		return -EACCES;

	list_for_each_entry(temp_p, &pwrdm_list, node) {
		for (i = 0; i < temp_p->banks; i++) {
			_pwrdm_set_mem_onst(temp_p, i, PWRDM_POWER_ON);
			_pwrdm_set_mem_retst(temp_p, i, PWRDM_POWER_RET);
		}
		WARN_ON(pwrdm_set_next_fpwrst(temp_p, PWRDM_FUNC_PWRST_ON));
	}

	return 0;
}

/**
 * pwrdm_lock - acquire a Linux spinlock on a powerdomain
 * @pwrdm: struct powerdomain * to lock
 *
 * Acquire the powerdomain spinlock on @pwrdm.  No return value.
 */
void pwrdm_lock(struct powerdomain *pwrdm)
	__acquires(&pwrdm->_lock)
{
	spin_lock_irqsave(&pwrdm->_lock, pwrdm->_lock_flags);
}

/**
 * pwrdm_unlock - release a Linux spinlock on a powerdomain
 * @pwrdm: struct powerdomain * to unlock
 *
 * Release the powerdomain spinlock on @pwrdm.  No return value.
 */
void pwrdm_unlock(struct powerdomain *pwrdm)
	__releases(&pwrdm->_lock)
{
	spin_unlock_irqrestore(&pwrdm->_lock, pwrdm->_lock_flags);
}

/**
 * pwrdm_lookup - look up a powerdomain by name, return a pointer
 * @name: name of powerdomain
 *
 * Find a registered powerdomain by its name @name.  Returns a pointer
 * to the struct powerdomain if found, or NULL otherwise.
 */
struct powerdomain *pwrdm_lookup(const char *name)
{
	struct powerdomain *pwrdm;

	if (!name)
		return NULL;

	pwrdm = _pwrdm_lookup(name);

	return pwrdm;
}

/**
 * pwrdm_for_each - call function on each registered clockdomain
 * @fn: callback function *
 *
 * Call the supplied function @fn for each registered powerdomain.
 * The callback function @fn can return anything but 0 to bail out
 * early from the iterator.  Returns the last return value of the
 * callback function, which should be 0 for success or anything else
 * to indicate failure; or -EINVAL if the function pointer is null.
 */
int pwrdm_for_each(int (*fn)(struct powerdomain *pwrdm, void *user),
		   void *user)
{
	struct powerdomain *temp_pwrdm;
	int ret = 0;

	if (!fn)
		return -EINVAL;

	list_for_each_entry(temp_pwrdm, &pwrdm_list, node) {
		ret = (*fn)(temp_pwrdm, user);
		if (ret)
			break;
	}

	return ret;
}

/**
 * pwrdm_add_clkdm - add a clockdomain to a powerdomain
 * @pwrdm: struct powerdomain * to add the clockdomain to
 * @clkdm: struct clockdomain * to associate with a powerdomain
 *
 * Associate the clockdomain @clkdm with a powerdomain @pwrdm.  This
 * enables the use of pwrdm_for_each_clkdm().  Returns -EINVAL if
 * presented with invalid pointers; -ENOMEM if memory could not be allocated;
 * or 0 upon success.
 */
int pwrdm_add_clkdm(struct powerdomain *pwrdm, struct clockdomain *clkdm)
{
	int i;
	int ret = -EINVAL;

	if (!pwrdm || !clkdm)
		return -EINVAL;

	pr_debug("powerdomain: %s: associating clockdomain %s\n",
		 pwrdm->name, clkdm->name);

	for (i = 0; i < PWRDM_MAX_CLKDMS; i++) {
		if (!pwrdm->pwrdm_clkdms[i])
			break;
#ifdef DEBUG
		if (pwrdm->pwrdm_clkdms[i] == clkdm) {
			ret = -EINVAL;
			goto pac_exit;
		}
#endif
	}

	if (i == PWRDM_MAX_CLKDMS) {
		pr_debug("powerdomain: %s: increase PWRDM_MAX_CLKDMS for clkdm %s\n",
			 pwrdm->name, clkdm->name);
		WARN_ON(1);
		ret = -ENOMEM;
		goto pac_exit;
	}

	pwrdm->pwrdm_clkdms[i] = clkdm;

	ret = 0;

pac_exit:
	return ret;
}

/**
 * pwrdm_del_clkdm - remove a clockdomain from a powerdomain
 * @pwrdm: struct powerdomain * to add the clockdomain to
 * @clkdm: struct clockdomain * to associate with a powerdomain
 *
 * Dissociate the clockdomain @clkdm from the powerdomain
 * @pwrdm. Returns -EINVAL if presented with invalid pointers; -ENOENT
 * if @clkdm was not associated with the powerdomain, or 0 upon
 * success.
 */
int pwrdm_del_clkdm(struct powerdomain *pwrdm, struct clockdomain *clkdm)
{
	int ret = -EINVAL;
	int i;

	if (!pwrdm || !clkdm)
		return -EINVAL;

	pr_debug("powerdomain: %s: dissociating clockdomain %s\n",
		 pwrdm->name, clkdm->name);

	for (i = 0; i < PWRDM_MAX_CLKDMS; i++)
		if (pwrdm->pwrdm_clkdms[i] == clkdm)
			break;

	if (i == PWRDM_MAX_CLKDMS) {
		pr_debug("powerdomain: %s: clkdm %s not associated?!\n",
			 pwrdm->name, clkdm->name);
		ret = -ENOENT;
		goto pdc_exit;
	}

	pwrdm->pwrdm_clkdms[i] = NULL;

	ret = 0;

pdc_exit:
	return ret;
}

/**
 * pwrdm_for_each_clkdm - call function on each clkdm in a pwrdm
 * @pwrdm: struct powerdomain * to iterate over
 * @fn: callback function *
 *
 * Call the supplied function @fn for each clockdomain in the powerdomain
 * @pwrdm.  The callback function can return anything but 0 to bail
 * out early from the iterator.  Returns -EINVAL if presented with
 * invalid pointers; or passes along the last return value of the
 * callback function, which should be 0 for success or anything else
 * to indicate failure.
 */
int pwrdm_for_each_clkdm(struct powerdomain *pwrdm,
			 int (*fn)(struct powerdomain *pwrdm,
				   struct clockdomain *clkdm))
{
	int ret = 0;
	int i;

	if (!fn)
		return -EINVAL;

	for (i = 0; i < PWRDM_MAX_CLKDMS && !ret; i++)
		ret = (*fn)(pwrdm, pwrdm->pwrdm_clkdms[i]);

	return ret;
}

/**
 * pwrdm_get_voltdm - return a ptr to the voltdm that this pwrdm resides in
 * @pwrdm: struct powerdomain *
 *
 * Return a pointer to the struct voltageomain that the specified powerdomain
 * @pwrdm exists in.
 */
struct voltagedomain *pwrdm_get_voltdm(struct powerdomain *pwrdm)
{
	return pwrdm->voltdm.ptr;
}

/**
 * pwrdm_clear_all_prev_pwrst - clear previous powerstate register for a pwrdm
 * @pwrdm: struct powerdomain * to clear
 *
 * Clear the powerdomain's previous power state register @pwrdm.
 * Clears the entire register, including logic and memory bank
 * previous power states.  Returns -EINVAL if the powerdomain pointer
 * is null, or returns 0 upon success.
 */
int pwrdm_clear_all_prev_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	/*
	 * XXX should get the powerdomain's current state here;
	 * warn & fail if it is not ON.
	 */

	pr_debug("powerdomain: %s: clearing previous power state reg\n",
		 pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_clear_all_prev_pwrst)
		ret = arch_pwrdm->pwrdm_clear_all_prev_pwrst(pwrdm);

	return ret;
}

/**
 * pwrdm_enable_hdwr_sar - enable automatic hardware SAR for a pwrdm
 * @pwrdm: struct powerdomain *
 *
 * Enable automatic context save-and-restore upon power state change
 * for some devices in the powerdomain @pwrdm.  Warning: this only
 * affects a subset of devices in a powerdomain; check the TRM
 * closely.  Returns -EINVAL if the powerdomain pointer is null or if
 * the powerdomain does not support automatic save-and-restore, or
 * returns 0 upon success.
 */
int pwrdm_enable_hdwr_sar(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (!(pwrdm->flags & PWRDM_HAS_HDWR_SAR))
		return ret;

	pr_debug("powerdomain: %s: setting SAVEANDRESTORE bit\n", pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_enable_hdwr_sar)
		ret = arch_pwrdm->pwrdm_enable_hdwr_sar(pwrdm);

	return ret;
}

/**
 * pwrdm_disable_hdwr_sar - disable automatic hardware SAR for a pwrdm
 * @pwrdm: struct powerdomain *
 *
 * Disable automatic context save-and-restore upon power state change
 * for some devices in the powerdomain @pwrdm.  Warning: this only
 * affects a subset of devices in a powerdomain; check the TRM
 * closely.  Returns -EINVAL if the powerdomain pointer is null or if
 * the powerdomain does not support automatic save-and-restore, or
 * returns 0 upon success.
 */
int pwrdm_disable_hdwr_sar(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (!(pwrdm->flags & PWRDM_HAS_HDWR_SAR))
		return ret;

	pr_debug("powerdomain: %s: clearing SAVEANDRESTORE bit\n", pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_disable_hdwr_sar)
		ret = arch_pwrdm->pwrdm_disable_hdwr_sar(pwrdm);

	return ret;
}

/**
 * pwrdm_has_hdwr_sar - test whether powerdomain supports hardware SAR
 * @pwrdm: struct powerdomain *
 *
 * Returns 1 if powerdomain @pwrdm supports hardware save-and-restore
 * for some devices, or 0 if it does not.
 */
bool pwrdm_has_hdwr_sar(struct powerdomain *pwrdm)
{
	return (pwrdm && pwrdm->flags & PWRDM_HAS_HDWR_SAR) ? 1 : 0;
}

int pwrdm_state_switch_nolock(struct powerdomain *pwrdm)
{
	int ret;

	if (!pwrdm || !arch_pwrdm)
		return -EINVAL;

	ret = arch_pwrdm->pwrdm_wait_transition(pwrdm);
	if (!ret)
		_pwrdm_state_switch(pwrdm);

	return ret;
}

int __deprecated pwrdm_state_switch(struct powerdomain *pwrdm)
{
	int ret;

	pwrdm_lock(pwrdm);
	ret = pwrdm_state_switch_nolock(pwrdm);
	pwrdm_unlock(pwrdm);

	return ret;
}

int pwrdm_pre_transition(struct powerdomain *pwrdm)
{
	if (pwrdm)
		_pwrdm_pre_transition_cb(pwrdm, NULL);
	else
		pwrdm_for_each(_pwrdm_pre_transition_cb, NULL);

	return 0;
}

int pwrdm_post_transition(struct powerdomain *pwrdm)
{
	if (pwrdm)
		_pwrdm_post_transition_cb(pwrdm, NULL);
	else
		pwrdm_for_each(_pwrdm_post_transition_cb, NULL);

	return 0;
}

/**
 * pwrdm_get_context_loss_count - get powerdomain's context loss count
 * @pwrdm: struct powerdomain * to wait for
 *
 * Context loss count is the sum of powerdomain off-mode counter, the
 * logic off counter and the per-bank memory off counter.  Returns negative
 * (and WARNs) upon error, otherwise, returns the context loss count.
 */
int pwrdm_get_context_loss_count(struct powerdomain *pwrdm)
{
	int count;

	if (!pwrdm) {
		WARN(1, "powerdomain: %s: pwrdm is null\n", __func__);
		return -ENODEV;
	}

	count = pwrdm->fpwrst_counter[PWRDM_FUNC_PWRST_OFF -
				      PWRDM_FPWRST_OFFSET];
	count += pwrdm->fpwrst_counter[PWRDM_FUNC_PWRST_OSWR -
				       PWRDM_FPWRST_OFFSET];

	/*
	 * Context loss count has to be a non-negative value. Clear the sign
	 * bit to get a value range from 0 to INT_MAX.
	 */
	count &= INT_MAX;

	pr_debug("powerdomain: %s: context loss count = %d\n",
		 pwrdm->name, count);

	return count;
}

/**
 * pwrdm_can_ever_lose_context - can this powerdomain ever lose context?
 * @pwrdm: struct powerdomain *
 *
 * Given a struct powerdomain * @pwrdm, returns 1 if the powerdomain
 * can lose either memory or logic context or if @pwrdm is invalid, or
 * returns 0 otherwise.  This function is not concerned with how the
 * powerdomain registers are programmed (i.e., to go off or not); it's
 * concerned with whether it's ever possible for this powerdomain to
 * go off while some other part of the chip is active.  This function
 * assumes that every powerdomain can go to either ON or INACTIVE.
 */
bool pwrdm_can_ever_lose_context(struct powerdomain *pwrdm)
{
	int i;

	if (IS_ERR_OR_NULL(pwrdm)) {
		pr_debug("powerdomain: %s: invalid powerdomain pointer\n",
			 __func__);
		return 1;
	}

	if (pwrdm->pwrsts & PWRSTS_OFF)
		return 1;

	if (pwrdm->pwrsts & PWRSTS_RET) {
		if (pwrdm->pwrsts_logic_ret & PWRSTS_OFF)
			return 1;

		for (i = 0; i < pwrdm->banks; i++)
			if (pwrdm->pwrsts_mem_ret[i] & PWRSTS_OFF)
				return 1;
	}

	for (i = 0; i < pwrdm->banks; i++)
		if (pwrdm->pwrsts_mem_on[i] & PWRSTS_OFF)
			return 1;

	return 0;
}

/* Public functions for functional power state handling */

/**
 * pwrdm_convert_fpwrst_to_name - return the name of a functional power state
 * @fpwrst: functional power state to return the name of
 *
 * Return a pointer to a string with the human-readable name of the
 * functional power state (e.g., "ON", "CSWR", etc.)  Intended for use
 * in debugging.  Returns NULL if @fpwrst is outside the range of the
 * known functional power states.
 */
const char *pwrdm_convert_fpwrst_to_name(u8 fpwrst)
{
	if (fpwrst < PWRDM_FPWRST_OFFSET || fpwrst >= PWRDM_MAX_FUNC_PWRSTS)
		return NULL;

	return _fpwrst_names[fpwrst - PWRDM_FPWRST_OFFSET];
}

/**
 * pwrdm_set_next_fpwrst - set next powerdomain functional power state
 * @pwrdm: struct powerdomain * to set
 * @fpwrst: one of the PWRDM_POWER_* macros
 *
 * Set the powerdomain @pwrdm's next power state to @fpwrst.  The
 * powerdomain may not enter this state immediately if the
 * preconditions for this state have not been satisfied.  Returns
 * -EINVAL if the powerdomain pointer is null or if the power state is
 * invalid for the powerdomin, or returns 0 upon success.
 */
int pwrdm_set_next_fpwrst(struct powerdomain *pwrdm, u8 fpwrst)
{
	u8 pwrst, logic;
	int ret;

	if (!pwrdm || IS_ERR(pwrdm))
		return -EINVAL;

	/*
	 * XXX Should return an error, but this means that our PM code
	 * will need to be much more careful about what it programs
	 */
	if (!_pwrdm_pwrst_is_controllable(pwrdm))
		return 0;

	ret = _pwrdm_fpwrst_to_pwrst(pwrdm, fpwrst, &pwrst, &logic);
	if (ret)
		return ret;

	pr_debug("%s: set fpwrst %0x to pwrdm %s\n", __func__, fpwrst,
		 pwrdm->name);

	/* Trace the pwrdm desired target state */
	trace_power_domain_target(pwrdm->name, fpwrst, smp_processor_id());

	pwrdm_lock(pwrdm);
	ret = _set_logic_retst_and_pwrdm_pwrst(pwrdm, logic, pwrst);
	pwrdm_unlock(pwrdm);

	return ret;
}

/**
 * pwrdm_read_next_fpwrst - get next powerdomain functional power state
 * @pwrdm: struct powerdomain * to get power state
 *
 * Return the powerdomain @pwrdm's next functional power state.
 * Returns -EINVAL if the powerdomain pointer is null or returns
 * the next power state upon success.
 */
int pwrdm_read_next_fpwrst(struct powerdomain *pwrdm)
{
	int next_pwrst, next_logic, ret;
	u8 fpwrst;

	if (!arch_pwrdm)
		return -EINVAL;

	pwrdm_lock(pwrdm);

	next_pwrst = arch_pwrdm->pwrdm_read_next_pwrst(pwrdm);
	if (next_pwrst < 0) {
		ret = next_pwrst;
		goto prnf_out;
	}

	next_logic = next_pwrst;
	if (_pwrdm_logic_retst_can_change(pwrdm) &&
	    arch_pwrdm->pwrdm_read_logic_pwrst) {
		next_logic = arch_pwrdm->pwrdm_read_logic_pwrst(pwrdm);
		if (next_logic < 0) {
			ret = next_logic;
			goto prnf_out;
		}
	}

	ret = _pwrdm_pwrst_to_fpwrst(pwrdm, next_pwrst, next_logic, &fpwrst);

prnf_out:
	pwrdm_unlock(pwrdm);

	return (ret) ? ret : fpwrst;
}

/**
 * pwrdm_set_fpwrst - program next powerdomain functional power state
 * @pwrdm: struct powerdomain * to set
 * @fpwrst: power domain functional state, one of the PWRDM_FUNC_* macros
 *
 * This programs the pwrdm next functional state, sets the dependencies
 * and waits for the state to be applied.
 */
int pwrdm_set_fpwrst(struct powerdomain *pwrdm, enum pwrdm_func_state fpwrst)
{
	u8 next_fpwrst, pwrst, logic, sleep_switch;
	int ret = 0;
	bool hwsup = false;

	if (!pwrdm || IS_ERR(pwrdm) || !arch_pwrdm)
		return -EINVAL;

	/*
	 * XXX Should return an error, but this means that our PM code
	 * will need to be much more careful about what it programs
	 */
	if (!_pwrdm_pwrst_is_controllable(pwrdm))
		return 0;

	ret = _pwrdm_fpwrst_to_pwrst(pwrdm, fpwrst, &pwrst, &logic);
	if (ret)
		return -EINVAL;

	pr_debug("%s: pwrdm %s: set fpwrst %0x\n", __func__, pwrdm->name,
		 fpwrst);

	pwrdm_lock(pwrdm);

	/*
	 * XXX quite heavyweight for what this is intended to do; the
	 * next fpwrst should simply be cached
	 */
	next_fpwrst = _pwrdm_read_next_fpwrst(pwrdm);
	if (next_fpwrst == fpwrst)
		goto psf_out;

	/* Trace the pwrdm desired target state */
	trace_power_domain_target(pwrdm->name, next_fpwrst,
				  smp_processor_id());

	sleep_switch = _pwrdm_save_clkdm_state_and_activate(pwrdm, fpwrst,
							    &hwsup);
	if (sleep_switch == ERROR_SWITCH) {
		ret = -EINVAL;
		goto psf_out;
	}

	ret = _set_logic_retst_and_pwrdm_pwrst(pwrdm, logic, pwrst);
	if (ret)
		pr_err("%s: unable to set power state of powerdomain: %s\n",
		       __func__, pwrdm->name);

	_pwrdm_restore_clkdm_state(pwrdm, sleep_switch, hwsup);

psf_out:
	pwrdm_unlock(pwrdm);

	return ret;
}

/**
 * pwrdm_read_fpwrst - get current functional powerdomain power state
 * @pwrdm: struct powerdomain * to get current functional power state
 *
 * Return the powerdomain @pwrdm's current functional power state.
 * Returns -EINVAL if the powerdomain pointer is null or returns the
 * current power state upon success.
 */
int pwrdm_read_fpwrst(struct powerdomain *pwrdm)
{
	int ret;

	if (!pwrdm || !arch_pwrdm)
		return -EINVAL;

	if (!_pwrdm_pwrst_can_change(pwrdm))
		return PWRDM_FUNC_PWRST_ON;

	pwrdm_lock(pwrdm);
	ret = _pwrdm_read_fpwrst(pwrdm);
	pwrdm_unlock(pwrdm);

	return ret;
}

/**
 * pwrdm_read_prev_fpwrst - get previous powerdomain functional power state
 * @pwrdm: struct powerdomain * to get previous functional power state
 *
 * Return the powerdomain @pwrdm's previous functional power state.
 * Returns -EINVAL if the powerdomain pointer is null or returns the
 * previous functional power state upon success.
 */
int pwrdm_read_prev_fpwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm || !arch_pwrdm)
		return -EINVAL;

	if (!_pwrdm_pwrst_can_change(pwrdm))
		return PWRDM_FUNC_PWRST_ON;

	pwrdm_lock(pwrdm);
	ret = _pwrdm_read_prev_fpwrst(pwrdm);
	pwrdm_unlock(pwrdm);

	return ret;
}

/**
 * pwrdm_supports_fpwrst - does the powerdomain @pwrdm support the @fpwrst power
 * state?
 * @pwrdm: struct powerdomain * pointing to a powerdomain to test
 * @fpwrst: functional power state
 *
 * Returns true if the powerdomain pointed to by @pwrdm can enter the
 * functional power state @fpwrst, or false if not.
 */
bool pwrdm_supports_fpwrst(struct powerdomain *pwrdm, u8 fpwrst)
{
	u8 pwrst, logic;
	int ret;

	if (!pwrdm || IS_ERR(pwrdm))
		return false;

	ret = _pwrdm_fpwrst_to_pwrst(pwrdm, fpwrst, &pwrst, &logic);
	if (ret)
		return false;

	if (pwrdm->pwrsts_logic_ret && pwrst == PWRDM_POWER_RET &&
	    !(pwrdm->pwrsts_logic_ret & (1 << logic)))
		return false;

	if (!(pwrdm->pwrsts & (1 << pwrst)))
		return false;

	return true;
}

/* Powerdomain debugfs-related functions */

/**
 * pwrdm_dbg_show_counter - generate debugfs data for the pwrdm pwrst counters
 * @pwrdm: struct powerdomain * to generate debugfs data for
 * @seq_file: struct seq_file * to write data to
 *
 * Dump the powerdomain @pwrdm's power state counters (and current
 * power state) to the seq_file @seq_file.  Currently called by the
 * mach-omap2/pm-debug.c code.  Returns 0.
 */
int pwrdm_dbg_show_counter(struct powerdomain *pwrdm, void *seq_file)
{
	struct seq_file *s = (struct seq_file *)seq_file;
	int i;
	u8 curr_fpwrst;

	if (!_pwrdm_pwrst_can_change(pwrdm))
		return 0;

	pwrdm_lock(pwrdm);

	curr_fpwrst = _pwrdm_read_fpwrst(pwrdm);
	if (pwrdm->fpwrst != curr_fpwrst)
		pr_err("pwrdm state mismatch(%s) %s != %s\n",
		       pwrdm->name,
		       pwrdm_convert_fpwrst_to_name(pwrdm->fpwrst),
		       pwrdm_convert_fpwrst_to_name(curr_fpwrst));

	seq_printf(s, "%s (%s)", pwrdm->name,
		   pwrdm_convert_fpwrst_to_name(pwrdm->fpwrst));
	for (i = PWRDM_FPWRST_OFFSET; i < PWRDM_MAX_FUNC_PWRSTS; i++)
		seq_printf(s, ",%s:%d", pwrdm_convert_fpwrst_to_name(i),
			   pwrdm->fpwrst_counter[i - PWRDM_FPWRST_OFFSET]);

	seq_printf(s, "\n");

	pwrdm_unlock(pwrdm);

	return 0;
}

/**
 * pwrdm_dbg_show_timer - generate debugfs data for the pwrdm pwrst timers
 * @pwrdm: struct powerdomain * to generate debugfs data for
 * @seq_file: struct seq_file * to write data to
 *
 * Dump the powerdomain @pwrdm's power state residency duration timings
 * to the seq_file @seq_file.  Currently called by the mach-omap2/pm-debug.c
 * code.  Returns 0.
 */
int pwrdm_dbg_show_timer(struct powerdomain *pwrdm, void *seq_file)
{
#ifdef CONFIG_PM_DEBUG
	struct seq_file *s = (struct seq_file *)seq_file;
	int i;

	if (!_pwrdm_pwrst_can_change(pwrdm))
		return 0;

	pwrdm_lock(pwrdm);

	pwrdm_state_switch_nolock(pwrdm);

	seq_printf(s, "%s (%s)", pwrdm->name,
		   pwrdm_convert_fpwrst_to_name(pwrdm->fpwrst));

	for (i = 0; i < PWRDM_FPWRSTS_COUNT; i++)
		seq_printf(s, ",%s:%lld",
			   pwrdm_convert_fpwrst_to_name(i + PWRDM_FPWRST_OFFSET),
			   pwrdm->fpwrst_timer[i]);

	seq_printf(s, "\n");

	pwrdm_unlock(pwrdm);
#endif
	return 0;
}

