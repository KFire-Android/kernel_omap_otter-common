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
static struct clk *dpll_core_m2_ck;
static struct clk *emif1_fck, *emif2_fck;

/**
 * omap4_core_dpll_m2_set_rate - set CORE DPLL M2 divider
 * @clk: struct clk * of DPLL to set
 * @rate: rounded target rate
 *
 * Programs the CM shadow registers to update CORE DPLL M2 divider. M2 divider
 * is used to clock external DDR and its reconfiguration on frequency change
 * is managed through a hardware sequencer. This is managed by the PRCM with
 * EMIF using shadow registers.  If rate specified matches DPLL_CORE's bypass
 * clock rate then put it in Low-Power Bypass.
 * Returns negative int on error and 0 on success.
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
	if (!emif1_fck)
		emif1_fck = clk_get(NULL, "emif1_fck");
	if (!emif2_fck)
		emif2_fck = clk_get(NULL, "emif2_fck");

	/* put MEMIF domain in SW_WKUP & increment usecount for clks */
	omap2_clkdm_wakeup(l3_emif_clkdm);
	omap2_clk_enable(emif1_fck);
	omap2_clk_enable(emif2_fck);

	/*
	 * maybe program core m5 divider here
	 * definitely program m3, m6 & m7 dividers here
	 */

	/*
	 * DDR clock = DPLL_CORE_M2_CK / 2.  Program EMIF timing
	 * parameters in EMIF shadow registers for validrate divided
	 * by 2.
	 */
	omap_emif_setup_registers(validrate / 2, LPDDR2_VOLTAGE_STABLE);

	/*
	 * program DPLL_CORE_M2_DIV with same value as the one already
	 * in direct register and lock DPLL_CORE
	 */
	shadow_freq_cfg1 =
		(new_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
		(DPLL_LOCKED << OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
		(1 << OMAP4430_DLL_RESET_SHIFT) |
		(1 << OMAP4430_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
					& OMAP4430_FREQ_UPDATE_MASK) == 0),
			MAX_FREQ_UPDATE_TIMEOUT, i);

	/* put MEMIF clkdm back to HW_AUTO & decrement usecount for clks */
	omap2_clkdm_allow_idle(l3_emif_clkdm);
	omap2_clk_disable(emif1_fck);
	omap2_clk_disable(emif2_fck);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
				__func__);
		return -1;
	}

	return 0;
}

/**
 * omap4_core_dpll_set_rate - set the rate for the CORE DPLL
 * @clk: struct clk * of the DPLL to set
 * @rate: rounded target rate
 *
 * Program the CORE DPLL, including handling of EMIF frequency changes on M2
 * divider.  Returns 0 on success, otherwise a negative error code.
 */
int omap4_core_dpll_set_rate(struct clk *clk, unsigned long rate)
{
	int i = 0, m2_div;
	u32 mask, reg;
	u32 shadow_freq_cfg1 = 0;
	struct clk *new_parent;
	struct dpll_data *dd;

	if (!clk  || !rate)
		return -EINVAL;

	if (!clk->dpll_data)
		return -EINVAL;

	dd = clk->dpll_data;

	if (rate == clk->rate)
		return 0;

	/* enable reference and bypass clocks */
	omap2_clk_enable(dd->clk_bypass);
	omap2_clk_enable(dd->clk_ref);

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm)
		l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
	if (!emif1_fck)
		emif1_fck = clk_get(NULL, "emif1_fck");
	if (!emif2_fck)
		emif2_fck = clk_get(NULL, "emif2_fck");
	if (!dpll_core_m2_ck)
		dpll_core_m2_ck = clk_get(NULL, "dpll_core_m2_ck");

	/* Make sure MEMIF clkdm is in SW_WKUP and EMIF modules are func */
	omap2_clkdm_wakeup(l3_emif_clkdm);
	omap2_clk_enable(emif1_fck);
	omap2_clk_enable(emif2_fck);

	/*
	 * maybe program core m5 divider here
	 * definitely program m3, m6 & m7 dividers here
	 */

	/* check for bypass rate */
	if (rate == dd->clk_bypass->rate &&
			clk->dpll_data->modes & (1 << DPLL_LOW_POWER_BYPASS)) {
		/*
		 * DDR clock = DPLL_CORE_M2_CK / 2.  Program EMIF timing
		 * parameters in EMIF shadow registers for bypass clock rate
		 * divided by 2
		 */
		omap_emif_setup_registers(rate / 2, LPDDR2_VOLTAGE_STABLE);

		/*
		 * FIXME PRCM functional spec says we should program
		 * CM_SHADOW_FREQ_CONFIG2.CLKSEL_L3 to 0 (corresponds to
		 * CM_CLKSEL_CORE.CLKSEL_L3) for normal bypass operation.
		 * This means L3_CLK is CORE_CLK divided by 1.  Same spec says
		 * the value should be 1 when entering DPLL cascading.  All of
		 * this assumes GPMC can scale frequency on the fly.  Too many
		 * unknowns, skipping this for now...
		 */

		/*
		 * program CM_DIV_M2_DPLL_CORE.DPLL_CLKOUT_DIV for divide by
		 * two and put DPLL_CORE into LP Bypass
		 */
		m2_div = omap4_prm_read_bits_shift(dpll_core_m2_ck->clksel_reg,
				dpll_core_m2_ck->clksel_mask);

		shadow_freq_cfg1 =
			(m2_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOW_POWER_BYPASS <<
			 OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
			(1 << OMAP4430_DLL_RESET_SHIFT) |
			(1 << OMAP4430_FREQ_UPDATE_SHIFT);
		__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

		new_parent = dd->clk_bypass;
	} else {
		if (dd->last_rounded_rate != rate)
			rate = clk->round_rate(clk, rate);

		if (dd->last_rounded_rate == 0)
			return -EINVAL;

		/*
		 * DDR clock = DPLL_CORE_M2_CK / 2.  Program EMIF timing
		 * parameters in EMIF shadow registers for rate divided
		 * by 2.
		 */
		omap_emif_setup_registers(rate / 2, LPDDR2_VOLTAGE_STABLE);

		/*
		 * FIXME skipping bypass part of omap3_noncore_dpll_program.
		 * also x-loader's configure_core_dpll_no_lock bypasses
		 * DPLL_CORE directly through CM_CLKMODE_DPLL_CORE via MN
		 * bypass; no shadow register necessary!
		 */

		mask = (dd->mult_mask | dd->div1_mask);
		reg  = (dd->last_rounded_m << __ffs(dd->mult_mask)) |
			((dd->last_rounded_n - 1) << __ffs(dd->div1_mask));

		/* program mn divider values */
		omap4_prm_rmw_reg_bits(mask, reg, dd->mult_div1_reg);

		/*
		 * FIXME PRCM functional spec says we should program
		 * CM_SHADOW_FREQ_CONFIG2.CLKSEL_L3 to 1 (corresponds to
		 * CM_CLKSEL_CORE.CLKSEL_L3) for normal bypass operation.
		 * This means L3_CLK is CORE_CLK divided by 2.  Same spec says
		 * the value should be 0 when exiting DPLL cascading.  All of
		 * this assumes GPMC can scale frequency on the fly.  Too many
		 * unknowns, skipping this for now...
		 */

		/*
		 * program DPLL_CORE_M2_DIV with same value as the one already
		 * in direct register and lock DPLL_CORE
		 */
		m2_div = omap4_prm_read_bits_shift(dpll_core_m2_ck->clksel_reg,
				dpll_core_m2_ck->clksel_mask);

		shadow_freq_cfg1 =
			(m2_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOCKED << OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
			(1 << OMAP4430_DLL_RESET_SHIFT) |
			(1 << OMAP4430_FREQ_UPDATE_SHIFT);
		__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

		new_parent = dd->clk_ref;
	}

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
					& OMAP4430_FREQ_UPDATE_MASK) == 0),
			MAX_FREQ_UPDATE_TIMEOUT, i);

	/*
	 * Switch the parent clock in the heirarchy, and make sure that the
	 * new parent's usecount is correct.  Note: we enable the new parent
	 * before disabling the old to avoid any unnecessary hardware
	 * disable->enable transitions.
	 */
	if (clk->usecount) {
		omap2_clk_enable(new_parent);
		omap2_clk_disable(clk->parent);
	}
	clk_reparent(clk, new_parent);
	clk->rate = rate;

	/* Configures MEMIF domain back to HW_WKUP */
	omap2_clkdm_allow_idle(l3_emif_clkdm);
	omap2_clk_disable(emif1_fck);
	omap2_clk_disable(emif2_fck);

	/*
	 * FIXME PRCM functional spec says we should set GPMC_FREQ_UPDATE bit
	 * here, but we're not even handling CM_SHADOW_FREQ_CONFIG2 at all.
	 */

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
