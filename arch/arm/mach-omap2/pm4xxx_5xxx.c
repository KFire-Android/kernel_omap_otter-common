/*
 * OMAP4/5 Power Management Routines
 *
 * Copyright (C) 2010-2011 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <asm/system_misc.h>

#include "common.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "pm.h"

struct power_state {
	struct powerdomain *pwrdm;
	u32 next_state;
#ifdef CONFIG_SUSPEND
	u32 saved_state;
	u32 saved_logic_state;
#endif
	struct list_head node;
};

static LIST_HEAD(pwrst_list);

#ifdef CONFIG_SUSPEND
static int omap4_5_pm_suspend(void)
{
	struct power_state *pwrst;
	int state, ret = 0;
	u32 cpu_id = smp_processor_id();

	/* Save current powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->saved_state = pwrdm_read_next_pwrst(pwrst->pwrdm);
		pwrst->saved_logic_state = pwrdm_read_logic_retst(pwrst->pwrdm);
	}

	/* Set targeted power domain states by suspend */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
		pwrdm_set_logic_retst(pwrst->pwrdm, PWRDM_POWER_OFF);
	}

	/*
	 * For MPUSS to hit power domain retention(CSWR or OSWR),
	 * CPU0 and CPU1 power domains need to be in OFF or DORMANT state,
	 * since CPU power domain CSWR is not supported by hardware
	 * Only master CPU follows suspend path. All other CPUs follow
	 * CPU hotplug path in system wide suspend. On OMAP4, CPU power
	 * domain CSWR is not supported by hardware.
	 * More details can be found in OMAP4430 TRM section 4.3.4.2.
	 */
	omap_enter_lowpower(cpu_id, PWRDM_POWER_OFF);

	/* Restore next powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		state = pwrdm_read_prev_pwrst(pwrst->pwrdm);
		if (state > pwrst->next_state) {
			pr_info("Powerdomain (%s) didn't enter "
			       "target state %d\n",
			       pwrst->pwrdm->name, pwrst->next_state);
			ret = -1;
		}
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->saved_state);
		pwrdm_set_logic_retst(pwrst->pwrdm, pwrst->saved_logic_state);
	}
	if (ret)
		pr_crit("Could not enter target state in pm_suspend\n");
	else
		pr_info("Successfully put all powerdomains to target state\n");

	return 0;
}
#endif /* CONFIG_SUSPEND */

static int __init pwrdms_setup(struct powerdomain *pwrdm, void *unused)
{
	struct power_state *pwrst;

	if (!pwrdm->pwrsts)
		return 0;

	/*
	 * Skip CPU0 and CPU1 power domains. CPU1 is programmed
	 * through hotplug path and CPU0 explicitly programmed
	 * further down in the code path
	 */
	if (!strncmp(pwrdm->name, "cpu", 3))
		return 0;

	/*
	 * FIXME: Remove this check when core retention is supported
	 * Only MPUSS power domain is added in the list.
	 */
	if (strcmp(pwrdm->name, "mpu_pwrdm"))
		return 0;

	pwrst = kmalloc(sizeof(struct power_state), GFP_ATOMIC);
	if (!pwrst)
		return -ENOMEM;

	pwrst->pwrdm = pwrdm;
	pwrst->next_state = PWRDM_POWER_RET;
	list_add(&pwrst->node, &pwrst_list);

	return omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
}

/**
 * omap_default_idle - OMAP4 default ilde routine.'
 *
 * Implements OMAP4 memory, IO ordering requirements which can't be addressed
 * with default cpu_do_idle() hook. Used by all CPUs with !CONFIG_CPUIDLE and
 * by secondary CPU with CONFIG_CPUIDLE.
 */
static void omap_default_idle(void)
{
	local_fiq_disable();

	omap_do_wfi();

	local_fiq_enable();
}

static inline int omap4_init_static_deps(void)
{
	struct clockdomain *emif_clkdm, *mpuss_clkdm, *l3_1_clkdm, *l4wkup;
	struct clockdomain *ducati_clkdm, *l3_2_clkdm, *l4_per_clkdm;
	int ret;
	/*
	 * The dynamic dependency between MPUSS -> MEMIF and
	 * MPUSS -> L4_PER/L3_* and DUCATI -> L3_* doesn't work as
	 * expected. The hardware recommendation is to enable static
	 * dependencies for these to avoid system lock ups or random crashes.
	 * The L4 wakeup depedency is added to workaround the OCP sync hardware
	 * BUG with 32K synctimer which lead to incorrect timer value read
	 * from the 32K counter. The BUG applies for GPTIMER1 and WDT2 which
	 * are part of L4 wakeup clockdomain.
	 */
	mpuss_clkdm = clkdm_lookup("mpuss_clkdm");
	emif_clkdm = clkdm_lookup("l3_emif_clkdm");
	l3_1_clkdm = clkdm_lookup("l3_1_clkdm");
	l3_2_clkdm = clkdm_lookup("l3_2_clkdm");
	l4_per_clkdm = clkdm_lookup("l4_per_clkdm");
	l4wkup = clkdm_lookup("l4_wkup_clkdm");
	ducati_clkdm = clkdm_lookup("ducati_clkdm");
	if ((!mpuss_clkdm) || (!emif_clkdm) || (!l3_1_clkdm) || (!l4wkup) ||
		(!l3_2_clkdm) || (!ducati_clkdm) || (!l4_per_clkdm))
		return -EINVAL;

	ret = clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l3_1_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l3_2_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l4_per_clkdm);
	ret |= clkdm_add_wkdep(mpuss_clkdm, l4wkup);
	ret |= clkdm_add_wkdep(ducati_clkdm, l3_1_clkdm);
	ret |= clkdm_add_wkdep(ducati_clkdm, l3_2_clkdm);
	if (ret) {
		pr_err("Failed to add MPUSS -> L3/EMIF/L4PER, DUCATI -> L3 "
				"wakeup dependency\n");
	}

	return ret;
}

static inline int omap5_init_static_deps(void)
{
	struct clockdomain *mpuss_clkdm, *emif_clkdm, *wkupaon_clkdm;
	int ret;

	/*
	 * The dynamic dependency between MPUSS -> EMIF
	 * doesn't work as expected. The hardware recommendation is to
	 * enable static dependencies for these to avoid system
	 * lock ups or random crashes.
	 */
	mpuss_clkdm = clkdm_lookup("mpu_clkdm");
	emif_clkdm = clkdm_lookup("emif_clkdm");
	if (!mpuss_clkdm || !emif_clkdm)
		return -EINVAL;

	ret = clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);

	/*
	 * The L4 wakeup depedency is added to workaround the OCP sync hardware
	 * BUG with 32K synctimer which lead to incorrect timer value read
	 * from the 32K counter. The BUG applies for GPTIMER1 and WDT2 which
	 * are part of L4 wakeup clockdomain. The 32K synctimer bug will be
	 * fixed in ES2.0 but not the GPTIMER1 or WDT2 bugs. The WDT2 is never
	 * read by the driver and so no workaround is needed. The GPTIMER1 bug
	 * is handled by the GPTIMER driver. Therefore, only enable the static
	 * dependency on ES1.0 for the 32K synctimer.
	 */
	if ((omap_rev() == OMAP5430_REV_ES1_0) ||
			(omap_rev() == OMAP5432_REV_ES1_0)) {
		wkupaon_clkdm = clkdm_lookup("wkupaon_clkdm");
		if (!wkupaon_clkdm)
			return -EINVAL;

		ret |= clkdm_add_wkdep(mpuss_clkdm, wkupaon_clkdm);
	}

	if (ret)
		pr_err("Failed to add MPUSS wakeup dependencies\n");

	return ret;
}

/**
 * omap_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static int __init omap_pm_init(void)
{
	int ret;

	if (!(cpu_is_omap44xx() || cpu_is_omap54xx()))
		return -ENODEV;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	pr_info("Power Management for TI OMAP4XX/OMAP5XXX devices.\n");

	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains\n");
		goto err2;
	}

	if (cpu_is_omap44xx())
		ret = omap4_init_static_deps();
	else
		ret = omap5_init_static_deps();

	if (ret) {
		pr_err("Failed to initialise static depedencies\n");
		goto err2;
	}

	ret = omap_mpuss_init();
	if (ret) {
		pr_err("Failed to initialise OMAP MPUSS\n");
		goto err2;
	}

	(void) clkdm_for_each(omap_pm_clkdms_setup, NULL);

#ifdef CONFIG_SUSPEND
	omap_pm_suspend = omap4_5_pm_suspend;
#endif

	if (cpu_is_omap44xx()) {
		/* Overwrite the default arch_idle() */
		arm_pm_idle = omap_default_idle;
		omap4_idle_init();
	} else if (cpu_is_omap54xx()) {
		omap5_idle_init();
	}

err2:
	return ret;
}
late_initcall(omap_pm_init);
