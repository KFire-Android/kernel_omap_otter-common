/*
 * OMAP4 DDR clock node
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Santosh Shilimkar <santosh.shilimkar@ti.com>
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <plat/common.h>
#include <plat/clockdomain.h>
#include <plat/prcm.h>

#include <mach/emif.h>
#include <mach/omap4-common.h>

#include "clock.h"
#include "clock44xx.h"
#include "cm.h"
#include "cm-regbits-44xx.h"

#define MAX_FREQ_UPDATE_TIMEOUT  100000
#define DPLL_REGM4XEN_ENABLE	0x1

static struct clockdomain *l3_emif_clkdm;

/**
 * omap4_core_dpll_m2_set_rate - set CORE DPLL M2 divider
 * @clk: struct clk * of DPLL to set
 * @rate: rounded target rate
 *
 * Programs the CM shadow registers to update CORE DPLL M2
 * divider. M2 divider is used to clock external DDR and its
 * reconfiguration on frequency change is managed through a
 * hardware sequencer. This is managed by the PRCM with EMIF
 * uding shadow registers.
 * Returns -EINVAL/-1 on error and 0 on success.
 */
int omap4_core_dpll_m2_set_rate(struct clk *clk, unsigned long rate)
{
	int i = 0;
	u32 validrate = 0, shadow_freq_cfg1 = 0, new_div = 0;

	if (!clk || !rate)
		return -EINVAL;

	validrate = omap2_clksel_round_rate_div(clk, rate, &new_div);
	if (validrate != rate)
		return -EINVAL;

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm)
		l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");

	/* Configures MEMIF domain in SW_WKUP */
	omap2_clkdm_wakeup(l3_emif_clkdm);

	/*
	 * Program EMIF timing parameters in EMIF shadow registers
	 * for targetted DRR clock.
	 * DDR Clock = core_dpll_m2 / 2
	 */
	omap_emif_setup_registers(validrate >> 1, LPDDR2_VOLTAGE_STABLE);

	/*
	 * FREQ_UPDATE sequence:
	 * - DLL_OVERRIDE=0 (DLL lock & code must not be overridden
	 *	after CORE DPLL lock)
	 * - DLL_RESET=1 (DLL must be reset upon frequency change)
	 * - DPLL_CORE_M2_DIV with same value as the one already
	 *	in direct register
	 * - DPLL_CORE_DPLL_EN=0x7 (to make CORE DPLL lock)
	 * - FREQ_UPDATE=1 (to start HW sequence)
	 */
	shadow_freq_cfg1 = (1 << OMAP4430_DLL_RESET_SHIFT) |
			(new_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOCKED << OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
			(1 << OMAP4430_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
				& OMAP4430_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Configures MEMIF domain back to HW_WKUP */
	omap2_clkdm_allow_idle(l3_emif_clkdm);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
				__func__);
		return -1;
	}

	return 0;
}


/**
 * omap4_prcm_freq_update - set freq_update bit
 *
 * Programs the CM shadow registers to update EMIF
 * parametrs. Few usecase only few registers needs to
 * be updated using prcm freq update sequence.
 * EMIF read-idle control and zq-config needs to be
 * updated for temprature alerts and voltage change
 * Returns -1 on error and 0 on success.
 */
int omap4_set_freq_update(void)
{
	u32 shadow_freq_cfg1;
	int i = 0;

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm)
		l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");

	/* Configures MEMIF domain in SW_WKUP */
	omap2_clkdm_wakeup(l3_emif_clkdm);

	/*
	 * FREQ_UPDATE sequence:
	 * - DLL_OVERRIDE=0 (DLL lock & code must not be overridden
	 *	after CORE DPLL lock)
	 * - FREQ_UPDATE=1 (to start HW sequence)
	 */
	shadow_freq_cfg1 = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1);
	shadow_freq_cfg1 |= (1 << OMAP4430_DLL_RESET_SHIFT) |
			   (1 << OMAP4430_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
				& OMAP4430_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Configures MEMIF domain back to HW_WKUP */
	omap2_clkdm_allow_idle(l3_emif_clkdm);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update failed\n",	__func__);
		return -1;
	}

	return 0;
}

int omap4_noncore_dpll_mn_bypass(struct clk *clk)
{
	int i, ret = 0;
	u32 reg;
	struct dpll_data *dd;

	if (!clk || !clk->dpll_data)
		return -EINVAL;

	dd = clk->dpll_data;

	if (!(clk->dpll_data->modes & (1 << DPLL_MN_BYPASS)))
		return -EINVAL;

	pr_debug("%s: configuring DPLL %s for MN bypass\n",
			__func__, clk->name);

	/* protect the DPLL during programming; usecount++ */
	clk_enable(dd->clk_bypass);

	omap4_prm_rmw_reg_bits(dd->enable_mask,
			(DPLL_MN_BYPASS << __ffs(dd->enable_mask)),
			dd->control_reg);

	/* wait for DPLL to enter bypass */
	for (i = 0; i < 1000000; i++) {
		reg = __raw_readl(dd->idlest_reg) & dd->mn_bypass_st_mask;
		if (reg)
			break;
	}

	if (reg) {
		if (clk->usecount) {
			/* DPLL is actually needed right now; usecount++ */
			clk_enable(dd->clk_bypass);
			clk_disable(clk->parent);
		}
		pr_err("%s: reparenting %s to %s, and setting old rate %lu to new rate %lu\n",
				__func__, clk->name, dd->clk_bypass->name,
				clk->rate, dd->clk_bypass->rate);
		clk_reparent(clk, dd->clk_bypass);
		clk->rate = dd->clk_bypass->rate;
	} else
		ret = -ENODEV;

	/* done programming, no need to protect DPLL; usecount-- */
	clk_disable(dd->clk_bypass);

	return ret;
}

unsigned long omap4_dpll_regm4xen_recalc(struct clk *clk)
{
	unsigned long rate;
	u32 reg;
	struct dpll_data *dd;

	if (!clk || !clk->dpll_data)
		return -EINVAL;

	dd = clk->dpll_data;

	rate = omap2_get_dpll_rate(clk);

	/* regm4xen adds a multiplier of 4 to DPLL calculations */
	reg = __raw_readl(dd->control_reg);
	if (reg & (DPLL_REGM4XEN_ENABLE << OMAP4430_DPLL_REGM4XEN_SHIFT))
		rate *= OMAP4430_REGM4XEN_MULT;

	return rate;
}

long omap4_dpll_regm4xen_round_rate(struct clk *clk, unsigned long target_rate)
{
	u32 reg;
	struct dpll_data *dd;

	dd = clk->dpll_data;

	/* REGM4XEN add 4x multiplier to MN dividers; check if it is set */
	reg = __raw_readl(dd->control_reg);
	reg &= OMAP4430_DPLL_REGM4XEN_MASK;
	if (reg)
		dd->max_multiplier = OMAP4430_MAX_DPLL_MULT * OMAP4430_REGM4XEN_MULT;
	else
		dd->max_multiplier = OMAP4430_MAX_DPLL_MULT;

	omap2_dpll_round_rate(clk, target_rate);

	if (reg) {
		/*
		 * FIXME this is lazy; we only support values of M that are
		 * divisible by 4 (a safe bet) and for which M/4 is >= 2
		 */
		if (dd->last_rounded_m % OMAP4430_REGM4XEN_MULT)
			pr_warn("%s: %s's M (%u) is not divisible by 4\n",
					__func__, clk->name, dd->last_rounded_m);

		if ((dd->last_rounded_m / OMAP4430_REGM4XEN_MULT) < 2)
			pr_warn("%s: %s's M (%u) is too low.  Try disabling REGM4XEN for this frequency\n",
					__func__, clk->name, dd->last_rounded_m);

		dd->last_rounded_m /= OMAP4430_REGM4XEN_MULT;
	}

	pr_debug("%s: last_rounded_m is %d, last_rounded_n is %d, last_rounded_rate is %lu\n",
			__func__, clk->dpll_data->last_rounded_m,
			clk->dpll_data->last_rounded_n,
			clk->dpll_data->last_rounded_rate);

	return clk->dpll_data->last_rounded_rate;
}
