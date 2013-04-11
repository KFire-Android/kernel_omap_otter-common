/*
 * OMAP powerdomain control
 *
 * Copyright (C) 2007-2008, 2011 Texas Instruments, Inc.
 * Copyright (C) 2007-2011 Nokia Corporation
 *
 * Written by Paul Walmsley
 * Added OMAP4 specific support by Abhijit Pagare <abhijitpagare@ti.com>
 * State counting code by Tero Kristo <tero.kristo@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ratelimit.h>
#include <linux/pm_qos.h>
#include <trace/events/power.h>

#include "cm2xxx_3xxx.h"
#include "prcm44xx.h"
#include "cm44xx.h"
#include "prm2xxx_3xxx.h"
#include "prm44xx.h"

#include <asm/cpu.h>
#include <plat/cpu.h>
#include "powerdomain.h"
#include "powerdomain-private.h"
#include "clockdomain.h"
#include <plat/prcm.h>

#include "pm.h"

#define PWRDM_TRACE_STATES_FLAG	(1<<31)

enum {
	PWRDM_STATE_HIGH2LOW = 0,
	PWRDM_STATE_LOW2HIGH,
};

static unsigned char _pwrdm_state_idx_lookup[] = {
	[PWRDM_POWER_OFF] = 1,
	[PWRDM_POWER_OSWR] = 2,
	[PWRDM_POWER_CSWR] = 3,
	[PWRDM_POWER_INACTIVE] = 4,
	[PWRDM_POWER_ON] = 5,
};

/* index to state lookup table */
static unsigned char _pwrdm_idx_state_lookup[] = {
	[1] = PWRDM_POWER_OFF,
	[2] = PWRDM_POWER_OSWR,
	[3] = PWRDM_POWER_CSWR,
	[4] = PWRDM_POWER_INACTIVE,
	[5] = PWRDM_POWER_ON,
};

/* Fwd declarations */
static __maybe_unused int pwrdm_set_mem_onst(struct powerdomain *pwrdm, u8 bank,
					     u8 pwrst);
static __maybe_unused int pwrdm_read_mem_pwrst(struct powerdomain *pwrdm,
					       u8 bank);
static int pwrdm_set_mem_retst(struct powerdomain *pwrdm, u8 bank, u8 pwrst);
static __maybe_unused int pwrdm_read_mem_retst(struct powerdomain *pwrdm,
					       u8 bank);
static int pwrdm_read_prev_mem_pwrst(struct powerdomain *pwrdm, u8 bank);

static int pwrdm_set_logic_retst(struct powerdomain *pwrdm, u8 pwrst);
static int pwrdm_read_logic_retst(struct powerdomain *pwrdm);
static int pwrdm_read_logic_pwrst(struct powerdomain *pwrdm);
static int pwrdm_read_prev_logic_pwrst(struct powerdomain *pwrdm);

/* pwrdm_list contains all registered struct powerdomains */
static LIST_HEAD(pwrdm_list);

static struct pwrdm_ops *arch_pwrdm;

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

	spin_lock_init(&pwrdm->lock);
	list_add(&pwrdm->node, &pwrdm_list);

	/* Initialize the powerdomain's state counter */
	for (i = 0; i < PWRDM_MAX_PWRSTS; i++)
		pwrdm->state_counter[i] = 0;

	pwrdm->ret_logic_off_counter = 0;
	for (i = 0; i < pwrdm->banks; i++)
		pwrdm->ret_mem_off_counter[i] = 0;

	/* Initialize the per-device wake-up constraints framework data */
	mutex_init(&pwrdm->wkup_lat_plist_lock);
	plist_head_init(&pwrdm->wkup_lat_plist_head);
	pwrdm->wkup_lat_next_state = PWRDM_POWER_OFF;

	/* Initialize the pwrdm state */
	pwrdm_wait_transition(pwrdm);
	pwrdm->state = pwrdm_read_pwrst(pwrdm);
	pwrdm->state_counter[_PWRDM_STATE_COUNT_IDX(pwrdm->state)] = 1;
	if (_pwrdm_state_compare_int(pwrdm->state, PWRDM_POWER_ON,
				     PWRDM_COMPARE_PWRST_EQ))
		pwrdm_clear_all_prev_pwrst(pwrdm);

	pr_debug("powerdomain: registered %s\n", pwrdm->name);

	return 0;
}

static void _update_logic_membank_counters(struct powerdomain *pwrdm)
{
	int i;
	u8 prev_logic_pwrst, prev_mem_pwrst;

	/*
	 * All powerdomains have logic status bit. Read
	 * logic status bit directly. Checking for
	 * supported logic states in pwrdm->pwrsts_logic_ret
	 * is unnecessary.
	 */
	prev_logic_pwrst = pwrdm_read_prev_logic_pwrst(pwrdm);

	if (prev_logic_pwrst == PWRDM_POWER_OFF)
		pwrdm->ret_logic_off_counter++;

	for (i = 0; i < pwrdm->banks; i++) {
		prev_mem_pwrst = pwrdm_read_prev_mem_pwrst(pwrdm, i);

		if (prev_mem_pwrst == PWRDM_POWER_OFF)
			pwrdm->ret_mem_off_counter[i]++;
	}
}

static inline void _pwrdm_state_counter_update(struct powerdomain *pwrdm,
					       int state)
{

	pwrdm->state_counter[_PWRDM_STATE_COUNT_IDX(state)]++;
	if ((state == PWRDM_POWER_OSWR) ||
	    (state == PWRDM_POWER_OFF))
		_update_logic_membank_counters(pwrdm);
}

static int _pwrdm_state_switch(struct powerdomain *pwrdm, int flag)
{

	int prev_state, current_state, next_state, saved_state;

	if (pwrdm == NULL) {
		WARN_ONCE(1, "null pwrdm\n");
		pr_err_ratelimited("%s: powerdomain: null pwrdm param\n",
				   __func__);
		return -EINVAL;
	}

	current_state = pwrdm_read_pwrst(pwrdm);
	prev_state = pwrdm_read_prev_pwrst(pwrdm);
	next_state = pwrdm_read_next_pwrst(pwrdm);
	saved_state = pwrdm->state;

	switch (flag) {
	case PWRDM_STATE_HIGH2LOW:
		if (_pwrdm_state_compare_int(current_state, next_state,
					     PWRDM_COMPARE_PWRST_GT))
			/* Transition to lower power state hasn't occurred */
			pwrdm->high2low_transition_enable = true;
		else if (current_state != saved_state) {
			/*
			 * Transition to the low power state has occurred.
			 * Multiple calls to this function could be made
			 * from clock framework and powerdomain framework
			 * to update counters for a single transition
			 * Therefore, update the counters only once on the
			 * very first call.
			 */
			pwrdm->high2low_transition_enable = false;
			_pwrdm_state_counter_update(pwrdm, current_state);
			pm_dbg_update_time(pwrdm, saved_state);
		}
		break;
	case PWRDM_STATE_LOW2HIGH:
		if (_pwrdm_state_compare_int(current_state, prev_state,
					     PWRDM_COMPARE_PWRST_GT)) {
			/* transition to a higher power state occurred*/
			_pwrdm_state_counter_update(pwrdm, current_state);

			if (pwrdm->high2low_transition_enable) {
				/*
				 * This flag will be set only if the
				 * the powerdomain did not transition to
				 * the programmed low power state by the
				 * time this function was called.
				 * The powerdomain would have transitioned
				 * to the low power state later.
				 * Therefore, update the counter of the
				 * prev state here
				 */
				_pwrdm_state_counter_update(pwrdm, prev_state);
				pwrdm->high2low_transition_enable = false;
			}
			/*
			 * Prev state of the powerdomain has now been processed
			 * clear it to ensure, counters are not incremented
			 * multiple times for a single power state transition
			 */
			pwrdm_clear_all_prev_pwrst(pwrdm);
			pm_dbg_update_time(pwrdm, prev_state);
		}
		break;
	default:
		pr_err_ratelimited("%s: powerdomain %s: bad flag %d\n",
				   __func__, pwrdm->name, flag);
		return -EINVAL;
	}


	pwrdm->state = current_state;

	return 0;
}

/**
 * _pwrdm_wakeuplat_update_pwrst - Update power domain power state if needed
 * @pwrdm: struct powerdomain * to which requesting device belongs to.
 * @min_latency: the allowed wake-up latency for the given power domain. A
 *  value of PM_QOS_DEV_LAT_DEFAULT_VALUE means 'no constraint' on the pwrdm.
 *
 * Finds the power domain next power state that fulfills the constraint.
 * Programs a new target state if it is different from current power state.
 * The power domains get the next power state programmed directly in the
 * registers.
 *
 * Returns 0 in case of success, -EINVAL in case of invalid parameters,
 * or the return value from omap_set_pwrdm_state.
 */
static int _pwrdm_wakeuplat_update_pwrst(struct powerdomain *pwrdm,
					 long min_latency)
{
	int ret = 0, state, state_idx, new_state = PWRDM_POWER_ON;

	/*
	 * Find the next supported power state with
	 *  wakeup latency <= min_latency.
	 * Pick the lower state if no constraint on the pwrdm
	 *  (min_latency == PM_QOS_DEV_LAT_DEFAULT_VALUE).
	 * Skip the states marked as unsupported (UNSUP_STATE).
	 * If no power state found, fall back to PWRDM_POWER_ON.
	 */
	for (state_idx = 1; state_idx < PWRDM_MAX_POWER_PWRSTS; state_idx++) {
		state = _pwrdm_idx_state_lookup[state_idx];

		if (pwrdm->wakeup_lat[state] == UNSUP_STATE)
			continue;
		if (min_latency == PM_QOS_DEV_LAT_DEFAULT_VALUE ||
		    pwrdm->wakeup_lat[state] <= min_latency) {
			new_state = state;
			break;
		}
	}

	if (state_idx == PWRDM_MAX_POWER_PWRSTS) {
		WARN(1, "%s: NO matching pwrst (powerdomain %s, min_lat=%ld)\n",
		     __func__, pwrdm->name, min_latency);
		return -EINVAL;
	}

	pwrdm->wkup_lat_next_state = new_state;
	ret = omap_set_pwrdm_state(pwrdm, new_state);

	pr_debug("%s: pwrst for %s: curr=%d, next=%d, min_latency=%ld, new_state=%d\n",
		 __func__, pwrdm->name, pwrdm_read_pwrst(pwrdm),
		 pwrdm_read_next_pwrst(pwrdm), min_latency, new_state);

	return ret;
}


/* Public functions */

bool _pwrdm_state_compare_int(int a, int b, u8 flag)
{
	int a_idx = _pwrdm_state_idx_lookup[a];
	int b_idx = _pwrdm_state_idx_lookup[b];

	if (flag & PWRDM_COMPARE_PWRST_EQ && a_idx == b_idx)
		return true;
	if (flag & PWRDM_COMPARE_PWRST_GT && a_idx > b_idx)
		return true;
	if (flag & PWRDM_COMPARE_PWRST_LT && a_idx < b_idx)
		return true;

	return false;
}

/**
 * pwrdm_register_platform_funcs - register powerdomain implementation fns
 * @po: func pointers for arch specific implementations
 *
 * Register the list of function pointers used to implement the
 * powerdomain functions on different OMAP SoCs.  Should be called
 * before any other pwrdm_register*() function.  Returns -EINVAL if
 * @po is null, -EEXIST if platform functions have already been
 * registered, or 0 upon success.
 */
int pwrdm_register_platform_funcs(struct pwrdm_ops *po)
{
	if (!po)
		return -EINVAL;

	if (arch_pwrdm)
		return -EEXIST;

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
 * for each powerdomain to ON.  This prevents powerdomains from
 * unexpectedly losing context or entering high wakeup latency modes
 * with non-power-management-enabled kernels.  Must be called after
 * pwrdm_register_pwrdms().  Returns -EACCES if called before
 * pwrdm_register_pwrdms(), or 0 upon success.
 */
int pwrdm_complete_init(void)
{
	struct powerdomain *temp_p;

	if (list_empty(&pwrdm_list))
		return -EACCES;

	list_for_each_entry(temp_p, &pwrdm_list, node)
		pwrdm_set_next_pwrst(temp_p, PWRDM_POWER_ON);

	return 0;
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

	pr_debug("powerdomain: associating clockdomain %s with powerdomain "
		 "%s\n", clkdm->name, pwrdm->name);

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
		pr_debug("powerdomain: increase PWRDM_MAX_CLKDMS for "
			 "pwrdm %s clkdm %s\n", pwrdm->name, clkdm->name);
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

	pr_debug("powerdomain: dissociating clockdomain %s from powerdomain "
		 "%s\n", clkdm->name, pwrdm->name);

	for (i = 0; i < PWRDM_MAX_CLKDMS; i++)
		if (pwrdm->pwrdm_clkdms[i] == clkdm)
			break;

	if (i == PWRDM_MAX_CLKDMS) {
		pr_debug("powerdomain: clkdm %s not associated with pwrdm "
			 "%s ?!\n", clkdm->name, pwrdm->name);
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
 * pwrdm_get_mem_bank_count - get number of memory banks in this powerdomain
 * @pwrdm: struct powerdomain *
 *
 * Return the number of controllable memory banks in powerdomain @pwrdm,
 * starting with 1.  Returns -EINVAL if the powerdomain pointer is null.
 */
int pwrdm_get_mem_bank_count(struct powerdomain *pwrdm)
{
	if (!pwrdm)
		return -EINVAL;

	return pwrdm->banks;
}

static int _match_pwrst(u32 pwrsts, u8 pwrst, u8 default_pwrst)
{
	bool found = true;
	int new_pwrst = pwrst;

	/* Search down */
	while (!(pwrsts & (1 << new_pwrst))) {
		if (new_pwrst == PWRDM_POWER_OFF) {
			found = false;
			break;
		}
		new_pwrst--;
	}

	if (found)
		goto done;

	/* Search up */
	new_pwrst = pwrst;
	while (!(pwrsts & (1 << new_pwrst))) {
		if (new_pwrst == default_pwrst)
			break;
		new_pwrst++;
	}
done:
	return new_pwrst;
}

/**
 * pwrdm_get_achievable_pwrst() - Provide achievable pwrst
 * @pwrdm: struct powerdomain * to check on
 * @req_pwrst: minimum state we would like to hit
 * (one of the PWRDM_POWER* macros)
 *
 * Power domains have varied capabilities. When attempting a low power
 * state such as OFF/RET, a specific min requested state may not be
 * supported on the power domain. This is because a combination
 * of system power states where the parent PD's state is not in line
 * with expectation can result in system instabilities.
 *
 * The achievable power state is first looked for in the lower power
 * states in order to maximize the power savings, then if not found
 * in the higher power states.
 *
 * Returns the achievable functional power state, or -EINVAL in case of
 * invalid parameters.
 */
int pwrdm_get_achievable_pwrst(struct powerdomain *pwrdm, u8 req_pwrst)
{
	int pwrst = req_pwrst;
	int logic = PWRDM_POWER_RET;
	int new_pwrst, new_logic;

	if (!pwrdm || IS_ERR(pwrdm)) {
		pr_err("%s: invalid params: pwrdm=%p, req_pwrst=%0x\n",
		       __func__, pwrdm, req_pwrst);
		return -EINVAL;
	}

	switch (req_pwrst) {
	case PWRDM_POWER_OFF:
		logic = PWRDM_POWER_OFF;
		break;
	case PWRDM_POWER_OSWR:
		logic = PWRDM_POWER_OFF;
		/* Fall through */
	case PWRDM_POWER_CSWR:
		pwrst = PWRDM_POWER_RET;
		break;
	}

	/*
	 * If no lower power state found, fallback to the higher
	 * power states.
	 * PWRDM_POWER_ON is always valid.
	 */
	new_pwrst = _match_pwrst(pwrdm->pwrsts, pwrst,
				 PWRDM_POWER_ON);
	/*
	 * If no lower logic state found, fallback to the higher
	 * logic states.
	 * PWRDM_POWER_RET is always valid.
	 */
	new_logic = _match_pwrst(pwrdm->pwrsts_logic_ret, logic,
				 PWRDM_POWER_RET);

	if (new_pwrst == PWRDM_POWER_RET) {
		if (new_logic == PWRDM_POWER_OFF)
			new_pwrst = PWRDM_POWER_OSWR;
		else
			new_pwrst = PWRDM_POWER_CSWR;
	}

	pr_debug("%s(%s, req_pwrst=%0x) returns %0x\n", __func__,
		 pwrdm->name, req_pwrst, new_pwrst);

	return new_pwrst;
}

/* Types of sleep_switch used in omap_set_pwrdm_state */
#define FORCEWAKEUP_SWITCH	0
#define LOWPOWERSTATE_SWITCH	1

/**
 * omap_set_pwrdm_state - program next powerdomain power state
 * @pwrdm: struct powerdomain * to set
 * @pwrst: one of the PWRDM_POWER_* macros
 *
 * This programs the pwrdm next state, sets the dependencies
 * and waits for the state to be applied.
 */
int omap_set_pwrdm_state(struct powerdomain *pwrdm, u32 pwrst)
{
	u8 curr_pwrst, next_pwrst, prev_pwrst;
	int sleep_switch = -1, ret = 0, hwsup = 0;

	if (!pwrdm || IS_ERR(pwrdm)) {
		pr_err_ratelimited("%s: powerdomain: bad pwrdm\n",
				   __func__);
		return -EINVAL;
	}

	pwrst = pwrdm_get_achievable_pwrst(pwrdm, pwrst);

	spin_lock(&pwrdm->lock);

	next_pwrst = pwrdm_read_next_pwrst(pwrdm);
	curr_pwrst = pwrdm_read_pwrst(pwrdm);
	prev_pwrst = pwrdm_read_prev_pwrst(pwrdm);
	/*
	 * we do not need to do anything IFF it is SURE that
	 * current power domain state is the same and the programmed
	 * next power domain state and that is the same state that
	 * we have been asked to go to
	 */
	if (curr_pwrst == next_pwrst && next_pwrst == pwrst)
		goto out;

	if (curr_pwrst != PWRDM_POWER_ON) {
		int curr_pwrst_idx;
		int pwrst_idx;

		curr_pwrst_idx = _pwrdm_state_idx_lookup[curr_pwrst];
		pwrst_idx = _pwrdm_state_idx_lookup[pwrst];
		if ((curr_pwrst_idx > pwrst_idx) &&
		    (pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE)) {
			sleep_switch = LOWPOWERSTATE_SWITCH;
		} else {
			hwsup = clkdm_in_hwsup(pwrdm->pwrdm_clkdms[0]);
			clkdm_wakeup(pwrdm->pwrdm_clkdms[0]);
			sleep_switch = FORCEWAKEUP_SWITCH;
		}
	}

	ret = pwrdm_set_next_pwrst(pwrdm, pwrst);
	if (ret)
		pr_err("%s: unable to set power state of powerdomain: %s\n",
		       __func__, pwrdm->name);

	switch (sleep_switch) {
	case FORCEWAKEUP_SWITCH:
		/*
		 * power state counters are updated from clkdm functions
		 * as needed
		 */
		if (hwsup)
			clkdm_allow_idle(pwrdm->pwrdm_clkdms[0]);
		else
			clkdm_sleep(pwrdm->pwrdm_clkdms[0]);
		break;
	case LOWPOWERSTATE_SWITCH:
		pwrdm_set_lowpwrstchange(pwrdm);
		pwrdm_wait_transition(pwrdm);
		pwrdm_state_high2low_counter_update(pwrdm);
		break;
	default:
		/*
		 * we are here because the powerdomain is ON, likely
		 * due to a wakeup interrupt. Just update the counters
		 * The two possible cases, why we could be here are
		 * a powerdomain wakeup due to a pending interrupt, or
		 * via suspend
		 */
		if (_pwrdm_state_compare_int(curr_pwrst, prev_pwrst,
					     PWRDM_COMPARE_PWRST_GT))
				pwrdm_state_low2high_counter_update(pwrdm);
		else if (_pwrdm_state_compare_int(curr_pwrst, pwrst,
						  PWRDM_COMPARE_PWRST_GT))
				pwrdm_state_high2low_counter_update(pwrdm);
	}
out:
	spin_unlock(&pwrdm->lock);
	return ret;
}

/**
 * pwrdm_set_next_pwrst - set next powerdomain power state
 * @pwrdm: struct powerdomain * to set
 * @pwrst: one of the PWRDM_POWER_* macros
 *
 * Set the powerdomain @pwrdm's next power state to @pwrst.  The powerdomain
 * may not enter this state immediately if the preconditions for this state
 * have not been satisfied.  Returns -EINVAL if the powerdomain pointer is
 * null or if the power state is invalid for the powerdomin, or returns 0
 * upon success.
 */
int pwrdm_set_next_pwrst(struct powerdomain *pwrdm, u8 pwrst)
{
	int ret = -EINVAL;
	u8 logic_pwrst = PWRDM_POWER_RET, set_pwrst = pwrst;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->pwrsts == PWRSTS_ON)
		return 0;

	switch (pwrst) {
	case PWRDM_POWER_CSWR:
	case PWRDM_POWER_OSWR:
		set_pwrst = PWRDM_POWER_RET;
		if (pwrst == PWRDM_POWER_OSWR)
			logic_pwrst = PWRDM_POWER_OFF;
		break;
	}

	if (!(pwrdm->pwrsts & (1 << set_pwrst))) {
		pr_err_ratelimited("%s: powerdomain %s: bad pwrst %d\n",
				   __func__, pwrdm->name, pwrst);
		return -EINVAL;
	}

	pr_debug("powerdomain: setting next powerstate for %s to %0x\n",
		 pwrdm->name, pwrst);

	switch (pwrst) {
	case PWRDM_POWER_CSWR:
	case PWRDM_POWER_OSWR:
		pwrdm_set_logic_retst(pwrdm, logic_pwrst);
		break;
	}

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_next_pwrst) {
		/* Trace the pwrdm desired target state */
		trace_power_domain_target(pwrdm->name, pwrst,
					  smp_processor_id());
		/* Program the pwrdm desired target state */
		ret = arch_pwrdm->pwrdm_set_next_pwrst(pwrdm, set_pwrst);
	}

	return ret;
}

/**
 * pwrdm_read_next_pwrst - get next powerdomain power state
 * @pwrdm: struct powerdomain * to get power state
 *
 * Return the powerdomain @pwrdm's next power state.  Returns -EINVAL
 * if the powerdomain pointer is null or returns the next power state
 * upon success.
 */
int pwrdm_read_next_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->pwrsts == PWRSTS_ON)
		return PWRDM_POWER_ON;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_next_pwrst)
		ret = arch_pwrdm->pwrdm_read_next_pwrst(pwrdm);

	if (ret == PWRDM_POWER_RET) {
		if (pwrdm_read_logic_retst(pwrdm) == PWRDM_POWER_OFF)
			ret = PWRDM_POWER_OSWR;
		else
			ret = PWRDM_POWER_CSWR;
	}
	return ret;
}

/**
 * pwrdm_read_pwrst - get current powerdomain power state
 * @pwrdm: struct powerdomain * to get power state
 *
 * Return the powerdomain @pwrdm's current power state.	Returns -EINVAL
 * if the powerdomain pointer is null or returns the current power state
 * upon success. Note that if the power domain only supports the ON state
 * then just return ON as the current state.
 */
int pwrdm_read_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->pwrsts == PWRSTS_ON)
		return PWRDM_POWER_ON;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_pwrst)
		ret = arch_pwrdm->pwrdm_read_pwrst(pwrdm);

	if (ret == PWRDM_POWER_RET) {
		if (pwrdm_read_logic_pwrst(pwrdm) == PWRDM_POWER_OFF)
			ret = PWRDM_POWER_OSWR;
		else
			ret = PWRDM_POWER_CSWR;
	}

	return ret;
}

/**
 * pwrdm_read_prev_pwrst - get previous powerdomain power state
 * @pwrdm: struct powerdomain * to get previous power state
 *
 * Return the powerdomain @pwrdm's previous power state.  Returns -EINVAL
 * if the powerdomain pointer is null or returns the previous power state
 * upon success.
 */
int pwrdm_read_prev_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->pwrsts == PWRSTS_ON)
		return PWRDM_POWER_ON;

	if (arch_pwrdm && arch_pwrdm->pwrdm_lost_context_rff &&
	    arch_pwrdm->pwrdm_lost_context_rff(pwrdm))
		return PWRDM_POWER_OFF;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_prev_pwrst)
		ret = arch_pwrdm->pwrdm_read_prev_pwrst(pwrdm);

	if (ret == PWRDM_POWER_RET) {
		if (pwrdm_read_prev_logic_pwrst(pwrdm) == PWRDM_POWER_OFF)
			ret = PWRDM_POWER_OSWR;
		else
			ret = PWRDM_POWER_CSWR;
	}

	return ret;
}

/**
 * pwrdm_read_device_off_state - Read device off state
 *
 * Reads the device OFF state. Returns 1 if set else zero if
 * device off is not set. And return invalid if arch_pwrdm
 * is not popluated with read_next_off api.
 */
int pwrdm_read_device_off_state(void)
{
	int ret = 0;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_next_off)
		ret = arch_pwrdm->pwrdm_read_next_off();

	return ret;
}

/**
 * pwrdm_enable_off_mode - Set device off state
 * @enable:	true - sets device level OFF mode configuration,
 *		false - unsets the OFF mode configuration
 *
 * This enables DEVICE OFF bit for OMAPs which support
 * Device OFF.
 */
void pwrdm_enable_off_mode(bool enable)
{
	if (arch_pwrdm && arch_pwrdm->pwrdm_read_next_off)
		arch_pwrdm->pwrdm_enable_off(enable);

	return;
}

/**
 * pwrdm_set_logic_retst - set powerdomain logic power state upon retention
 * @pwrdm: struct powerdomain * to set
 * @pwrst: one of the PWRDM_POWER_* macros
 *
 * Set the next power state @pwrst that the logic portion of the
 * powerdomain @pwrdm will enter when the powerdomain enters retention.
 * This will be either RETENTION or OFF, if supported.  Returns
 * -EINVAL if the powerdomain pointer is null or the target power
 * state is not not supported, or returns 0 upon success.
 */
static int pwrdm_set_logic_retst(struct powerdomain *pwrdm, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (!(pwrdm->pwrsts_logic_ret & (1 << pwrst))) {
		pr_err_ratelimited("%s: powerdomain %s: bad pwrst %d\n",
				   __func__, pwrdm->name, pwrst);
		return -EINVAL;
	}

	pr_debug("powerdomain: setting next logic powerstate for %s to %0x\n",
		 pwrdm->name, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_logic_retst)
		ret = arch_pwrdm->pwrdm_set_logic_retst(pwrdm, pwrst);

	return ret;
}

/**
 * pwrdm_set_mem_onst - set memory power state while powerdomain ON
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
static __maybe_unused int pwrdm_set_mem_onst(struct powerdomain *pwrdm, u8 bank,
					     u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->banks < (bank + 1)) {
		pr_err_ratelimited("%s: powerdomain %s: bad bank %d\n",
				   __func__, pwrdm->name, bank);
		return -EEXIST;
	}

	if (!(pwrdm->pwrsts_mem_on[bank] & (1 << pwrst))) {
		pr_err_ratelimited("%s: powerdomain %s: bank %d bad pwrst %d\n",
				   __func__, pwrdm->name, bank, pwrst);
		return -EINVAL;
	}

	pr_debug("powerdomain: setting next memory powerstate for domain %s "
		 "bank %0x while pwrdm-ON to %0x\n", pwrdm->name, bank, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_mem_onst)
		ret = arch_pwrdm->pwrdm_set_mem_onst(pwrdm, bank, pwrst);

	return ret;
}

/**
 * pwrdm_set_mem_retst - set memory power state while powerdomain in RET
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
static __maybe_unused int pwrdm_set_mem_retst(struct powerdomain *pwrdm,
					      u8 bank, u8 pwrst)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (pwrdm->banks < (bank + 1)) {
		pr_err_ratelimited("%s: powerdomain %s: bad bank %d\n",
				   __func__, pwrdm->name, bank);
		return -EEXIST;
	}

	if (!(pwrdm->pwrsts_mem_ret[bank] & (1 << pwrst))) {
		pr_err_ratelimited("%s: powerdomain %s: bank %d bad pwrst %d\n",
				   __func__, pwrdm->name, bank, pwrst);
		return -EINVAL;
	}

	pr_debug("powerdomain: setting next memory powerstate for domain %s "
		 "bank %0x while pwrdm-RET to %0x\n", pwrdm->name, bank, pwrst);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_mem_retst)
		ret = arch_pwrdm->pwrdm_set_mem_retst(pwrdm, bank, pwrst);

	return ret;
}

/**
 * pwrdm_read_logic_pwrst - get current powerdomain logic retention power state
 * @pwrdm: struct powerdomain * to get current logic retention power state
 *
 * Return the power state that the logic portion of powerdomain @pwrdm
 * will enter when the powerdomain enters retention.  Returns -EINVAL
 * if the powerdomain pointer is null or returns the logic retention
 * power state upon success.
 */
static int pwrdm_read_logic_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_logic_pwrst)
		ret = arch_pwrdm->pwrdm_read_logic_pwrst(pwrdm);

	return ret;
}

/**
 * pwrdm_read_prev_logic_pwrst - get previous powerdomain logic power state
 * @pwrdm: struct powerdomain * to get previous logic power state
 *
 * Return the powerdomain @pwrdm's previous logic power state.  Returns
 * -EINVAL if the powerdomain pointer is null or returns the previous
 * logic power state upon success.
 */
static int pwrdm_read_prev_logic_pwrst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_prev_logic_pwrst)
		ret = arch_pwrdm->pwrdm_read_prev_logic_pwrst(pwrdm);

	return ret;
}

/**
 * pwrdm_read_logic_retst - get next powerdomain logic power state
 * @pwrdm: struct powerdomain * to get next logic power state
 *
 * Return the powerdomain pwrdm's logic power state.  Returns -EINVAL
 * if the powerdomain pointer is null or returns the next logic
 * power state upon success.
 */
static int pwrdm_read_logic_retst(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_logic_retst)
		ret = arch_pwrdm->pwrdm_read_logic_retst(pwrdm);

	return ret;
}

/**
 * pwrdm_read_mem_pwrst - get current memory bank power state
 * @pwrdm: struct powerdomain * to get current memory bank power state
 * @bank: memory bank number (0-3)
 *
 * Return the powerdomain @pwrdm's current memory power state for bank
 * @bank.  Returns -EINVAL if the powerdomain pointer is null, -EEXIST if
 * the target memory bank does not exist or is not controllable, or
 * returns the current memory power state upon success.
 */
static __maybe_unused int pwrdm_read_mem_pwrst(struct powerdomain *pwrdm,
					       u8 bank)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (pwrdm->banks < (bank + 1)) {
		pr_err_ratelimited("%s: powerdomain %s: bad bank %d\n",
				   __func__, pwrdm->name, bank);
		return ret;
	}

	if (pwrdm->flags & PWRDM_HAS_MPU_QUIRK)
		bank = 1;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_mem_pwrst)
		ret = arch_pwrdm->pwrdm_read_mem_pwrst(pwrdm, bank);

	return ret;
}

/**
 * pwrdm_read_prev_mem_pwrst - get previous memory bank power state
 * @pwrdm: struct powerdomain * to get previous memory bank power state
 * @bank: memory bank number (0-3)
 *
 * Return the powerdomain @pwrdm's previous memory power state for
 * bank @bank.  Returns -EINVAL if the powerdomain pointer is null,
 * -EEXIST if the target memory bank does not exist or is not
 * controllable, or returns the previous memory power state upon
 * success.
 */
static int pwrdm_read_prev_mem_pwrst(struct powerdomain *pwrdm, u8 bank)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (pwrdm->banks < (bank + 1)) {
		pr_err_ratelimited("%s: powerdomain %s: bad bank %d\n",
				   __func__, pwrdm->name, bank);
		return ret;
	}

	if (pwrdm->flags & PWRDM_HAS_MPU_QUIRK)
		bank = 1;

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_prev_mem_pwrst)
		ret = arch_pwrdm->pwrdm_read_prev_mem_pwrst(pwrdm, bank);

	return ret;
}

/**
 * pwrdm_read_mem_retst - get next memory bank power state
 * @pwrdm: struct powerdomain * to get mext memory bank power state
 * @bank: memory bank number (0-3)
 *
 * Return the powerdomain pwrdm's next memory power state for bank
 * x.  Returns -EINVAL if the powerdomain pointer is null, -EEXIST if
 * the target memory bank does not exist or is not controllable, or
 * returns the next memory power state upon success.
 */
static __maybe_unused int pwrdm_read_mem_retst(struct powerdomain *pwrdm,
					       u8 bank)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return ret;

	if (pwrdm->banks < (bank + 1)) {
		pr_err_ratelimited("%s: powerdomain %s: bad bank %d\n",
				   __func__, pwrdm->name, bank);
		return ret;
	}

	if (arch_pwrdm && arch_pwrdm->pwrdm_read_mem_retst)
		ret = arch_pwrdm->pwrdm_read_mem_retst(pwrdm, bank);

	return ret;
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

	if (pwrdm->pwrsts == PWRSTS_ON)
		return 0;

	/*
	 * XXX should get the powerdomain's current state here;
	 * warn & fail if it is not ON.
	 */

	pr_debug("powerdomain: clearing previous power state reg for %s\n",
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

	if (!(pwrdm->flags & PWRDM_HAS_HDWR_SAR)) {
		pr_err_ratelimited("%s: powerdomain %s: no HDSAR in flag %d\n",
				   __func__, pwrdm->name, pwrdm->flags);
		return ret;
	}

	pr_debug("powerdomain: %s: setting SAVEANDRESTORE bit\n",
		 pwrdm->name);

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

	if (!(pwrdm->flags & PWRDM_HAS_HDWR_SAR)) {
		pr_err_ratelimited("%s: powerdomain %s: no HDSAR in flag %d\n",
				   __func__, pwrdm->name, pwrdm->flags);
		return ret;
	}

	pr_debug("powerdomain: %s: clearing SAVEANDRESTORE bit\n",
		 pwrdm->name);

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

/**
 * pwrdm_set_lowpwrstchange - Request a low power state change
 * @pwrdm: struct powerdomain *
 *
 * Allows a powerdomain to transtion to a lower power sleep state
 * from an existing sleep state without waking up the powerdomain.
 * Returns -EINVAL if the powerdomain pointer is null or if the
 * powerdomain does not support LOWPOWERSTATECHANGE, or returns 0
 * upon success.
 */
int pwrdm_set_lowpwrstchange(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (!(pwrdm->flags & PWRDM_HAS_LOWPOWERSTATECHANGE)) {
		pr_err_ratelimited("%s: powerdomain %s:no lowpwrch in flag%d\n",
				   __func__, pwrdm->name, pwrdm->flags);
		return -EINVAL;
	}

	pr_debug("powerdomain: %s: setting LOWPOWERSTATECHANGE bit\n",
		 pwrdm->name);

	if (arch_pwrdm && arch_pwrdm->pwrdm_set_lowpwrstchange)
		ret = arch_pwrdm->pwrdm_set_lowpwrstchange(pwrdm);

	return ret;
}

/**
 * pwrdm_wait_transition - wait for powerdomain power transition to finish
 * @pwrdm: struct powerdomain * to wait for
 *
 * If the powerdomain @pwrdm is in the process of a state transition,
 * spin until it completes the power transition, or until an iteration
 * bailout value is reached. Returns -EINVAL if the powerdomain
 * pointer is null, -EAGAIN if the bailout value was reached, or
 * returns 0 upon success.
 */
int pwrdm_wait_transition(struct powerdomain *pwrdm)
{
	int ret = -EINVAL;

	if (!pwrdm)
		return -EINVAL;

	if (arch_pwrdm && arch_pwrdm->pwrdm_wait_transition)
		ret = arch_pwrdm->pwrdm_wait_transition(pwrdm);

	return ret;
}

/*
 * pwrdm_state_high2low_counter_update() - handle a high to low change
 * @pwrdm:	powerdomain to change
 *
 * Update counters when a powerdomain transitioned from high to low
 */
void pwrdm_state_high2low_counter_update(struct powerdomain *pwrdm)
{
	_pwrdm_state_switch(pwrdm, PWRDM_STATE_HIGH2LOW);
}

/*
 * pwrdm_state_low2high_counter_update() - handle a low to high change
 * @pwrdm:	powerdomain to change
 *
 * Update counters when a powerdomain has transitioned from low power state
 * to high.
 */
void pwrdm_state_low2high_counter_update(struct powerdomain *pwrdm)
{
	_pwrdm_state_switch(pwrdm, PWRDM_STATE_LOW2HIGH);
}

/**
 * pwrdm_clkdm_enable - increment powerdomain usecount
 * @pwrdm: struct powerdomain *
 *
 * Increases the usecount for a powerdomain. Called from clockdomain
 * code once a clockdomain's usecount reaches zero, i.e. it is ready
 * to idle. Only usecounts are protected with atomic operation as this
 * function is not expected to perform any other critical operation
 * that can be impacted due to race conditions.
 *
 * Returns updated usecount
 */
int pwrdm_usecount_inc(struct powerdomain *pwrdm)
{
	int val;

	if (!pwrdm) {
		pr_warn("%s: pwrdm = NULL\n", __func__);
		return -EINVAL;
	}

	val = atomic_inc_return(&pwrdm->usecount);

	if (val == 1)
		voltdm_pwrdm_enable(pwrdm->voltdm.ptr);

	return val;
}

/**
 * pwrdm_clkdm_disable - decrease powerdomain usecount
 * @pwrdm: struct powerdomain *
 *
 * Decreases the usecount for a powerdomain. Called from clockdomain
 * code once a clockdomain becomes active.
 * Only usecounts are protected with atomic operation as this
 * function is not expected to perform any other critical operation
 * that can be impacted due to race conditions.
 *
 * Returns updated usecount
 */
int pwrdm_usecount_dec(struct powerdomain *pwrdm)
{
	int usecount, ret;

	if (!pwrdm) {
		pr_warn("%s: pwrdm = NULL\n", __func__);
		return -EINVAL;
	}

	ret = atomic_add_unless(&pwrdm->usecount, -1, 0);

	if (!ret) {
		pr_warn("%s: pwrdm %s usecount already 0\n",
			__func__, pwrdm->name);
		return -EPERM;
	}

	usecount = atomic_read(&pwrdm->usecount);

	if (usecount == 0)
		voltdm_pwrdm_disable(pwrdm->voltdm.ptr);

	return usecount;
}


/**
 * pwrdm_get_usecount - return powerdomain usecount
 * @pwrdm: struct powerdomain *
 *
 * Returns the current usecount for a powerdomain.
 *
 */
int pwrdm_get_usecount(struct powerdomain *pwrdm)
{
	return atomic_read(&pwrdm->usecount);
}

/**
 * pwrdm_wakeuplat_update_constraint - Set or update a powerdomain wakeup
 *  latency constraint and apply it
 * @pwrdm: struct powerdomain * which the constraint applies to
 * @cookie: constraint identifier, used for tracking
 * @min_latency: minimum wakeup latency constraint (in microseconds) for
 *  the given pwrdm
 *
 * Tracks the constraints by @cookie.
 * Constraint set/update: Adds a new entry to powerdomain's wake-up latency
 * constraint list. If the constraint identifier already exists in the list,
 * the old value is overwritten.
 *
 * Applies the aggregated constraint value for the given pwrdm by calling
 * _pwrdm_wakeuplat_update_pwrst.
 *
 * Returns 0 upon success, -ENOMEM in case of memory shortage, -EINVAL in
 * case of invalid latency value, or the return value from
 * _pwrdm_wakeuplat_update_pwrst.
 *
 * The caller must check the validity of the parameters.
 */
int pwrdm_wakeuplat_update_constraint(struct powerdomain *pwrdm, void *cookie,
				      long min_latency)
{
	struct pwrdm_wkup_constraints_entry *tmp_user, *new_user, *user = NULL;
	long value = PM_QOS_DEV_LAT_DEFAULT_VALUE;
	int free_new_user = 0;

	if (!pwrdm) {
		WARN(1, "powerdomain: %s: NULL pwrdm parameter!\n", __func__);
		return -EINVAL;
	}

	pr_debug("powerdomain: %s: pwrdm %s, cookie=0x%p, min_latency=%ld\n",
		 __func__, pwrdm->name, cookie, min_latency);

	if (min_latency <= PM_QOS_DEV_LAT_DEFAULT_VALUE) {
		pr_warn("%s: min_latency >= PM_QOS_DEV_LAT_DEFAULT_VALUE\n",
			__func__);
		return -EINVAL;
	}

	/* Allocate a new entry for insertion in the list */
	new_user = kzalloc(sizeof(struct pwrdm_wkup_constraints_entry),
			   GFP_KERNEL);
	if (!new_user) {
		pr_err("%s: FATAL ERROR: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&pwrdm->wkup_lat_plist_lock);

	/* Check if there already is a constraint for cookie */
	plist_for_each_entry(tmp_user, &pwrdm->wkup_lat_plist_head, node) {
		if (tmp_user->cookie == cookie) {
			user = tmp_user;
			break;
		}
	}

	if (!user) {
		/* Add new entry to the list */
		user = new_user;
		user->cookie = cookie;
	} else {
		/* If nothing to update, job done */
		if (user->node.prio == min_latency) {
			mutex_unlock(&pwrdm->wkup_lat_plist_lock);
			kfree(new_user);
			return 0;
		}

		/* Update existing entry */
		plist_del(&user->node, &pwrdm->wkup_lat_plist_head);
		free_new_user = 1;
	}

	plist_node_init(&user->node, min_latency);
	plist_add(&user->node, &pwrdm->wkup_lat_plist_head);

	/* Find the aggregated constraint value from the list */
	if (!plist_head_empty(&pwrdm->wkup_lat_plist_head))
		value = plist_first(&pwrdm->wkup_lat_plist_head)->prio;

	mutex_unlock(&pwrdm->wkup_lat_plist_lock);

	/* Free the newly allocated entry if not in use */
	if (free_new_user)
		kfree(new_user);

	/* Apply the constraint to the pwrdm */
	pr_debug("powerdomain: %s: pwrdm %s, value=%ld\n",
		 __func__, pwrdm->name, value);
	return _pwrdm_wakeuplat_update_pwrst(pwrdm, value);
}

/**
 * pwrdm_wakeuplat_remove_constraint - Release a powerdomain wakeup latency
 *  constraint and apply it
 * @pwrdm: struct powerdomain * which the constraint applies to
 * @cookie: constraint identifier, used for tracking
 *
 * Tracks the constraints by @cookie.
 * Constraint removal: Removes the identifier's entry from powerdomain's
 * wakeup latency constraint list.
 *
 * Applies the aggregated constraint value for the given pwrdm by calling
 * _pwrdm_wakeuplat_update_pwrst.
 *
 * Returns 0 upon success, -EINVAL in case the constraint to remove is not
 * existing, or the return value from _pwrdm_wakeuplat_update_pwrst.
 *
 * The caller must check the validity of the parameters.
 */
int pwrdm_wakeuplat_remove_constraint(struct powerdomain *pwrdm, void *cookie)
{
	struct pwrdm_wkup_constraints_entry *tmp_user, *user = NULL;
	long value = PM_QOS_DEV_LAT_DEFAULT_VALUE;

	if (!pwrdm) {
		WARN(1, "powerdomain: %s: NULL pwrdm parameter!\n", __func__);
		return -EINVAL;
	}

	pr_debug("powerdomain: %s: pwrdm %s, cookie=0x%p\n",
		 __func__, pwrdm->name, cookie);

	mutex_lock(&pwrdm->wkup_lat_plist_lock);

	/* Check if there is a constraint for cookie */
	plist_for_each_entry(tmp_user, &pwrdm->wkup_lat_plist_head, node) {
		if (tmp_user->cookie == cookie) {
			user = tmp_user;
			break;
		}
	}

	/* If constraint not existing or list empty, return -EINVAL */
	if (!user)
		goto out;

	/* Remove the constraint from the list */
	plist_del(&user->node, &pwrdm->wkup_lat_plist_head);

	/* Find the aggregated constraint value from the list */
	if (!plist_head_empty(&pwrdm->wkup_lat_plist_head))
		value = plist_first(&pwrdm->wkup_lat_plist_head)->prio;

	mutex_unlock(&pwrdm->wkup_lat_plist_lock);

	/* Release the constraint memory */
	kfree(user);

	/* Apply the constraint to the pwrdm */
	pr_debug("powerdomain: %s: pwrdm %s, value=%ld\n",
		 __func__, pwrdm->name, value);
	return _pwrdm_wakeuplat_update_pwrst(pwrdm, value);

out:
	mutex_unlock(&pwrdm->wkup_lat_plist_lock);
	return -EINVAL;
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
	int i, count;

	if (!pwrdm) {
		WARN(1, "powerdomain: %s: pwrdm is null\n", __func__);
		return -ENODEV;
	}

	count = pwrdm->state_counter[PWRDM_POWER_OFF];
	count += pwrdm->ret_logic_off_counter;

	for (i = 0; i < pwrdm->banks; i++)
		count += pwrdm->ret_mem_off_counter[i];

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
