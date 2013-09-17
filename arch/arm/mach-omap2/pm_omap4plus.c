/*
 * OMAP4PLUS Power Management Routines
 *
 * Copyright (C) 2010-2013 Texas Instruments, Inc.
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

#include "soc.h"
#include "common.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "pm.h"

struct power_state {
	struct powerdomain *pwrdm;
	u8 next_fpwrst;
#ifdef CONFIG_SUSPEND
	u8 saved_fpwrst;
#endif
	struct list_head node;
};

static LIST_HEAD(pwrst_list);
u32 cpu_suspend_state;
u32 pwrdm_next_state;

#ifdef CONFIG_SUSPEND
static int omap4_pm_suspend(void)
{
	struct power_state *pwrst;
	int prev_fpwrst;
	int ret = 0;
	u32 cpu_id = smp_processor_id();

	/* XXX Seems like these two loops could be combined into one loop? */

	/* Save current powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node)
		pwrst->saved_fpwrst = pwrdm_read_next_fpwrst(pwrst->pwrdm);

	/* Set targeted power domain states by suspend */
	list_for_each_entry(pwrst, &pwrst_list, node)
		WARN_ON(pwrdm_set_fpwrst(pwrst->pwrdm, pwrst->next_fpwrst));

	/*
	 * For MPUSS to hit power domain retention(CSWR or OSWR),
	 * CPU0 and CPU1 power domains need to be in OFF or DORMANT state,
	 * since CPU power domain CSWR is not supported by hardware
	 * Only master CPU follows suspend path. All other CPUs follow
	 * CPU hotplug path in system wide suspend. On OMAP4, CPU power
	 * domain CSWR is not supported by hardware.
	 * More details can be found in OMAP4430 TRM section 4.3.4.2.
	 */
	omap4_mpuss_enter_lowpower(cpu_id, cpu_suspend_state);

	/* Restore next powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		prev_fpwrst = pwrdm_read_prev_fpwrst(pwrst->pwrdm);
		if (prev_fpwrst != pwrst->next_fpwrst) {
			pr_info("Powerdomain (%s) didn't enter target state %s - entered state %s instead\n",
				pwrst->pwrdm->name,
				pwrdm_convert_fpwrst_to_name(pwrst->next_fpwrst),
				pwrdm_convert_fpwrst_to_name(prev_fpwrst));
			ret = -1;
		}
		WARN_ON(pwrdm_set_fpwrst(pwrst->pwrdm, pwrst->saved_fpwrst));
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

	pwrst = kmalloc(sizeof(struct power_state), GFP_ATOMIC);
	if (!pwrst)
		return -ENOMEM;

	pwrst->pwrdm = pwrdm;
	/*
	 * XXX This should be replaced by explicit lists of
	 * powerdomains with specific powerstates to set
	 */
	pwrst->next_fpwrst = pwrdm_next_state;
	if (!pwrdm_supports_fpwrst(pwrdm, pwrst->next_fpwrst))
		pwrst->next_fpwrst = PWRDM_FUNC_PWRST_ON;
	list_add(&pwrst->node, &pwrst_list);

	return WARN_ON(pwrdm_set_fpwrst(pwrst->pwrdm, pwrst->next_fpwrst));
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
	omap_do_wfi();
}

/**
 * omap4_init_static_deps - Init static clkdm dependencies on OMAP4
 *
 * The dynamic dependency between MPUSS -> MEMIF and
 * MPUSS -> L4_PER/L3_* and DUCATI -> L3_* doesn't work as
 * expected. The hardware recommendation is to enable static
 * dependencies for these to avoid system lock ups or random crashes.
 * The L4 wakeup depedency is added to workaround the OCP sync hardware
 * BUG with 32K synctimer which lead to incorrect timer value read
 * from the 32K counter. The BUG applies for GPTIMER1 and WDT2 which
 * are part of L4 wakeup clockdomain.
 */
static inline int omap4_init_static_deps(void)
{
	int ret;
	struct clockdomain *emif_clkdm, *mpuss_clkdm, *l3_1_clkdm, *l4wkup;
	struct clockdomain *ducati_clkdm, *l3_2_clkdm, *l4_per_clkdm;

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
		pr_err("Failed to add MPUSS -> L3/EMIF/L4PER, DUCATI -> L3 wakeup dependency\n");
	}

	return ret;
}

/**
 * omap5_init_static_deps - Init static clkdm dependencies on OMAP5
 *
 * The dynamic dependency between MPUSS -> EMIF is broken and has
 * not worked as expected. The hardware recommendation is to
 * enable static dependencies for these to avoid system
 * lock ups or random crashes.
 */
static inline int omap5_init_static_deps(void)
{
	struct clockdomain *mpuss_clkdm, *emif_clkdm;
	int ret;

	mpuss_clkdm = clkdm_lookup("mpu_clkdm");
	emif_clkdm = clkdm_lookup("emif_clkdm");
	if (!mpuss_clkdm || !emif_clkdm)
		return -EINVAL;

	ret = clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);
	if (ret)
		pr_err("Failed to add MPUSS -> EMIF wakeup dependency\n");

	return ret;
}


/**
 * omap4_pm_init - Init routine for OMAP4+ devices
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
int __init omap4_pm_init(void)
{
	int ret;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	pr_info("Power Management for TI OMAP4PLUS devices.\n");

	if (soc_is_dra7xx()) {
		cpu_suspend_state = PWRDM_FUNC_PWRST_ON;
		pwrdm_next_state = PWRDM_FUNC_PWRST_ON;
	} else {
		cpu_suspend_state = PWRDM_FUNC_PWRST_OFF;
		pwrdm_next_state = PWRDM_FUNC_PWRST_CSWR;
	}

	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains.\n");
		goto err2;
	}

	if (cpu_is_omap44xx())
		ret = omap4_init_static_deps();
	else if (soc_is_omap54xx() || soc_is_dra7xx())
		ret = omap5_init_static_deps();

	if (ret) {
		pr_err("Failed to initialise static dependencies.\n");
		goto err2;
	}

	ret = omap4_mpuss_init();
	if (ret) {
		pr_err("Failed to initialise OMAP4 MPUSS\n");
		goto err2;
	}

	(void) clkdm_for_each(omap_pm_clkdms_setup, NULL);

#ifdef CONFIG_SUSPEND
	omap_pm_suspend = omap4_pm_suspend;
#endif

	/* Overwrite the default arch_idle() */
	arm_pm_idle = omap_default_idle;

	if (cpu_is_omap44xx() || soc_is_omap54xx())
		omap4_idle_init();

err2:
	return ret;
}
