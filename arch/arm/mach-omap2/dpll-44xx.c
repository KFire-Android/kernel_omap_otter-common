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
#include <linux/cpufreq.h>

#include <plat/common.h>
#include <plat/clockdomain.h>
#include <plat/prcm.h>
#include <plat/opp.h>
#include <plat/omap-serial.h>

#include <mach/emif.h>
#include <mach/omap4-common.h>

#include "clock.h"
#include "clock44xx.h"
#include "cm.h"
#include "cm-regbits-44xx.h"
#include "prm-regbits-44xx.h"

#define MAX_FREQ_UPDATE_TIMEOUT  100000
#define DPLL_REGM4XEN_ENABLE	0x1
#define LP_196M_RATE		196608000
#define LP_98M_RATE		98304000
#define LP_DELAY		1000000

static struct delayed_work	lpmode_work;

bool omap4_lpmode = false;

static struct clockdomain *l3_emif_clkdm;
static struct clk *dpll_core_m2_ck, *dpll_core_m5x2_ck;
static struct clk *gpmc_ick;

static struct dpll_cascade_saved_state {
	unsigned long dpll_mpu_ck_rate;
	unsigned long dpll_iva_ck_rate;
	unsigned long dpll_per_ck_rate;
	unsigned long div_mpu_hs_clk_div;
	unsigned long div_iva_hs_clk_div;
	struct clk *iva_hsd_byp_clk_mux_ck_parent;
	struct clk *core_hsd_byp_clk_mux_ck_parent;
	unsigned long div_core_ck_div;
	unsigned long l3_div_ck_div;
	unsigned long l4_div_ck_div;
	unsigned long dpll_core_m2_ck_div;
	unsigned long dpll_core_m5x2_ck_div;
	struct clk *l4_wkup_clk_mux_ck_parent;
	struct clk *pmd_stm_clock_mux_ck_parent;
	struct clk *pmd_trace_clk_mux_ck_parent;
	u32 clkreqctrl;
	unsigned long dpll_core_ck_rate;
	u32 dpll_core_m2_div;
	unsigned long cm2_scale_fclk_div;
	struct clk *per_hsd_byp_clk_mux_ck_parent;
	unsigned int cpufreq_policy_min_rate;
	unsigned int cpufreq_policy_max_rate;
	unsigned int cpufreq_policy_cur_rate;
	struct omap_opp *mpu_opp;
} state;

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

	/* put MEMIF domain in SW_WKUP & increment usecount for clks */
	omap2_clkdm_wakeup(l3_emif_clkdm);

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
	int i = 0, m2_div, m5_div;
	u32 mask, reg;
	u32 shadow_freq_cfg1 = 0, shadow_freq_cfg2 = 0;
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
	if (!dpll_core_m2_ck)
		dpll_core_m2_ck = clk_get(NULL, "dpll_core_m2_ck");
	if (!dpll_core_m5x2_ck)
		dpll_core_m5x2_ck = clk_get(NULL, "dpll_core_m5x2_ck");
	if (!gpmc_ick)
		gpmc_ick = clk_get(NULL, "gpmc_ick");

	/* Make sure MEMIF clkdm is in SW_WKUP & GPMC clocks are active */
	omap2_clkdm_wakeup(l3_emif_clkdm);
	omap2_clk_enable(gpmc_ick);

	/* FIXME set m3, m6 & m7 rates here? */

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
		 * program CM_DIV_M5_DPLL_CORE.DPLL_CLKOUT_DIV into shadow
		 * register as well as L3_CLK freq and update GPMC frequency
		 *
		 * HACK: hardcode L3_CLK = CORE_CLK / 2 for DPLL cascading
		 * HACK: hardcode CORE_CLK = CORE_X2_CLK / 2 for DPLL
		 * cascading
		 */
		m5_div = omap4_prm_read_bits_shift(dpll_core_m5x2_ck->clksel_reg,
				dpll_core_m5x2_ck->clksel_mask);

		shadow_freq_cfg2 =
			(m5_div << OMAP4430_DPLL_CORE_M5_DIV_SHIFT) |
			(1 << OMAP4430_CLKSEL_L3_SHADOW_SHIFT) |
			(0 << OMAP4430_CLKSEL_CORE_1_1_SHIFT) |
			(1 << OMAP4430_GPMC_FREQ_UPDATE_SHIFT);

		__raw_writel(shadow_freq_cfg2, OMAP4430_CM_SHADOW_FREQ_CONFIG2);

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
		 * program CM_DIV_M5_DPLL_CORE.DPLL_CLKOUT_DIV into shadow
		 * register as well as L3_CLK freq and update GPMC frequency
		 *
		 * HACK: hardcode L3_CLK = CORE_CLK / 2 for DPLL cascading
		 * HACK: hardcode CORE_CLK = CORE_X2_CLK / 1 for DPLL
		 * cascading
		 */
		m5_div = omap4_prm_read_bits_shift(dpll_core_m5x2_ck->clksel_reg,
				dpll_core_m5x2_ck->clksel_mask);

		shadow_freq_cfg2 =
			(m5_div << OMAP4430_DPLL_CORE_M5_DIV_SHIFT) |
			(1 << OMAP4430_CLKSEL_L3_SHADOW_SHIFT) |
			(0 << OMAP4430_CLKSEL_CORE_1_1_SHIFT) |
			(1 << OMAP4430_GPMC_FREQ_UPDATE_SHIFT);

		__raw_writel(shadow_freq_cfg2, OMAP4430_CM_SHADOW_FREQ_CONFIG2);

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

	/* clear GPMC_FREQ_UPDATE bit */
	shadow_freq_cfg2 = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG2);
	shadow_freq_cfg2 &= ~1;
	__raw_writel(shadow_freq_cfg2, OMAP4430_CM_SHADOW_FREQ_CONFIG2);

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

	/* disable reference and bypass clocks */
	omap2_clk_disable(dd->clk_bypass);
	omap2_clk_disable(dd->clk_ref);

	/* Configures MEMIF domain back to HW_WKUP & let GPMC clocks to idle */
	omap2_clkdm_allow_idle(l3_emif_clkdm);
	omap2_clk_disable(gpmc_ick);

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

void omap4_dpll_low_power_cascade_check_timer(struct work_struct *dwork)
{
	int delay;

	if (num_online_cpus() > 1) {
		delay = usecs_to_jiffies(LP_DELAY);

		schedule_delayed_work_on(0, &lpmode_work, delay);
	} else
		omap4_dpll_low_power_cascade_enter();
}

int omap4_dpll_low_power_cascade_check_entry()
{
	int delay = usecs_to_jiffies(LP_DELAY);

	INIT_DELAYED_WORK_DEFERRABLE(&lpmode_work,
			omap4_dpll_low_power_cascade_check_timer);

	return schedule_delayed_work_on(0, &lpmode_work, delay);
}

/**
 * omap4_dpll_low_power_cascade - configure system for low power DPLL cascade
 *
 * The low power DPLL cascading scheme is a way to have a mostly functional
 * system running with only one locked DPLL and all of the others in bypass.
 * While this might be useful for many use cases, the primary target is low
 * power audio playback.  The steps to enter this state are roughly:
 *
 * Reparent DPLL_ABE so that it is fed by SYS_32K_CK
 * Set magical REGM4XEN bit so DPLL_ABE MN dividers are multiplied by four
 * Lock DPLL_ABE at 196.608MHz and bypass DPLL_CORE, DPLL_MPU & DPLL_IVA
 * Reparent DPLL_CORE so that is fed by DPLL_ABE
 * Reparent DPLL_MPU & DPLL_IVA so that they are fed by DPLL_CORE
 */
int omap4_dpll_low_power_cascade_enter()
{
	int ret = 0;
	struct clk *dpll_abe_ck, *dpll_abe_m3x2_ck;
	struct clk *dpll_mpu_ck, *div_mpu_hs_clk;
	struct clk *dpll_iva_ck, *div_iva_hs_clk, *iva_hsd_byp_clk_mux_ck;
	struct clk *dpll_per_ck, *func_48m_fclk;
	struct clk *per_hsd_byp_clk_mux_ck, *per_hs_clk_div_ck;
	struct clk *dpll_core_ck, *dpll_core_x2_ck;
	struct clk *dpll_core_m2_ck, *dpll_core_m5x2_ck, *dpll_core_m6x2_ck;
	struct clk *core_hsd_byp_clk_mux_ck;
	struct clk *div_core_ck, *l3_div_ck, *l4_div_ck;
	struct clk *l4_wkup_clk_mux_ck, *lp_clk_div_ck;
	struct clk *pmd_stm_clock_mux_ck, *pmd_trace_clk_mux_ck;
	struct clockdomain *emu_sys_44xx_clkdm, *abe_44xx_clkdm;
	struct device *mpu_dev;
	struct cpufreq_policy *cp;
	struct omap_opp *opp;

	dpll_abe_ck = clk_get(NULL, "dpll_abe_ck");
	dpll_mpu_ck = clk_get(NULL, "dpll_mpu_ck");
	div_mpu_hs_clk = clk_get(NULL, "div_mpu_hs_clk");
	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	div_iva_hs_clk = clk_get(NULL, "div_iva_hs_clk");
	iva_hsd_byp_clk_mux_ck = clk_get(NULL, "iva_hsd_byp_clk_mux_ck");
	dpll_core_ck = clk_get(NULL, "dpll_core_ck");
	dpll_core_m2_ck = clk_get(NULL, "dpll_core_m2_ck");
	dpll_core_m5x2_ck = clk_get(NULL, "dpll_core_m5x2_ck");
	dpll_core_m6x2_ck = clk_get(NULL, "dpll_core_m6x2_ck");
	dpll_abe_m3x2_ck = clk_get(NULL, "dpll_abe_m3x2_ck");
	dpll_core_x2_ck = clk_get(NULL, "dpll_core_x2_ck");
	core_hsd_byp_clk_mux_ck = clk_get(NULL, "core_hsd_byp_clk_mux_ck");
	div_core_ck = clk_get(NULL, "div_core_ck");
	l4_wkup_clk_mux_ck = clk_get(NULL, "l4_wkup_clk_mux_ck");
	lp_clk_div_ck = clk_get(NULL, "lp_clk_div_ck");
	pmd_stm_clock_mux_ck = clk_get(NULL, "pmd_stm_clock_mux_ck");
	pmd_trace_clk_mux_ck = clk_get(NULL, "pmd_trace_clk_mux_ck");
	l3_div_ck = clk_get(NULL, "l3_div_ck");
	l4_div_ck = clk_get(NULL, "l4_div_ck");
	dpll_per_ck = clk_get(NULL, "dpll_per_ck");
	func_48m_fclk = clk_get(NULL, "func_48m_fclk");
	per_hsd_byp_clk_mux_ck = clk_get(NULL, "per_hsd_byp_clk_mux_ck");
	per_hs_clk_div_ck = clk_get(NULL, "per_hs_clk_div_ck");

	emu_sys_44xx_clkdm = clkdm_lookup("emu_sys_44xx_clkdm");
	abe_44xx_clkdm = clkdm_lookup("abe_clkdm");

	if (!dpll_abe_ck || !dpll_mpu_ck || !div_mpu_hs_clk || !dpll_iva_ck ||
		!div_iva_hs_clk || !iva_hsd_byp_clk_mux_ck || !dpll_core_m2_ck
		|| !dpll_abe_m3x2_ck || !div_core_ck || !dpll_core_x2_ck ||
		!core_hsd_byp_clk_mux_ck || !dpll_core_m5x2_ck ||
		!l4_wkup_clk_mux_ck || !lp_clk_div_ck || !pmd_stm_clock_mux_ck
		|| !pmd_trace_clk_mux_ck || !dpll_core_m6x2_ck ||
		!dpll_core_ck || !dpll_per_ck || !func_48m_fclk
		|| !per_hsd_byp_clk_mux_ck || !per_hs_clk_div_ck) {

		pr_warn("%s: failed to get all necessary clocks\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	/* prevent DPLL_ABE & DPLL_CORE from idling */
	omap3_dpll_deny_idle(dpll_abe_ck);
	omap3_dpll_deny_idle(dpll_core_ck);

	/* put ABE clock domain SW_WKUP */
	omap2_clkdm_wakeup(abe_44xx_clkdm);

	/* WA: prevent DPLL_ABE_M3X2 clock from auto-gating */
	omap4_prm_rmw_reg_bits(BIT(8), BIT(8),
			dpll_abe_m3x2_ck->clksel_reg);

	/* drive DPLL_CORE bypass clock from DPLL_ABE (CLKINPULOW) */
	state.core_hsd_byp_clk_mux_ck_parent = core_hsd_byp_clk_mux_ck->parent;
	ret = clk_set_parent(core_hsd_byp_clk_mux_ck, dpll_abe_m3x2_ck);
	if (ret) {
		pr_err("%s: failed reparenting DPLL_CORE bypass clock to ABE_M3X2\n",
				__func__);
		goto core_bypass_clock_reparent_fail;
	} else
		pr_debug("%s: DPLL_CORE bypass clock reparented to ABE_M3X2\n",
				__func__);

	/*
	 * bypass DPLL_CORE, configure EMIF for the new rate
	 * CORE_CLK = CORE_X2_CLK
	 */
	state.dpll_core_ck_rate = dpll_core_ck->rate;

	state.div_core_ck_div =
		omap4_prm_read_bits_shift(div_core_ck->clksel_reg,
				div_core_ck->clksel_mask);

	state.l3_div_ck_div =
		omap4_prm_read_bits_shift(l3_div_ck->clksel_reg,
				l3_div_ck->clksel_mask);

	state.l4_div_ck_div =
		omap4_prm_read_bits_shift(l4_div_ck->clksel_reg,
				l4_div_ck->clksel_mask);

	state.dpll_core_m5x2_ck_div =
		omap4_prm_read_bits_shift(dpll_core_m5x2_ck->clksel_reg,
				dpll_core_m5x2_ck->clksel_mask);

	state.dpll_core_m2_div =
		omap4_prm_read_bits_shift(dpll_core_m2_ck->clksel_reg,
				dpll_core_m2_ck->clksel_mask);

	ret =  clk_set_rate(div_core_ck, dpll_core_m5x2_ck->rate);
	ret |= clk_set_rate(dpll_core_ck, LP_196M_RATE);
	ret |= clk_set_rate(dpll_core_m5x2_ck, dpll_core_x2_ck->rate);
	if (ret) {
		pr_err("%s: failed setting CORE clock rates\n", __func__);
		goto core_clock_set_rate_fail;
	} else
		pr_debug("%s: DPLL_CORE bypass clock reparented to ABE_M3X2\n",
				__func__);

	/* divide MPU/IVA bypass clocks by 2 (for when we bypass DPLL_CORE) */
	state.div_mpu_hs_clk_div =
		omap4_prm_read_bits_shift(div_mpu_hs_clk->clksel_reg,
				div_mpu_hs_clk->clksel_mask);
	state.div_iva_hs_clk_div =
		omap4_prm_read_bits_shift(div_iva_hs_clk->clksel_reg,
				div_iva_hs_clk->clksel_mask);
	clk_set_rate(div_mpu_hs_clk, div_mpu_hs_clk->parent->rate);
	clk_set_rate(div_iva_hs_clk, div_iva_hs_clk->parent->rate / 2);

	/* select CLKINPULOW (div_iva_hs_clk) as DPLL_IVA bypass clock */
	state.iva_hsd_byp_clk_mux_ck_parent = iva_hsd_byp_clk_mux_ck->parent;
	ret = clk_set_parent(iva_hsd_byp_clk_mux_ck, div_iva_hs_clk);
	if (ret) {
		pr_err("%s: failed reparenting DPLL_IVA bypass clock to CLKINPULOW\n",
				__func__);
		goto iva_bypass_clk_reparent_fail;
	} else
		pr_debug("%s: reparented DPLL_IVA bypass clock to CLKINPULOW\n",
				__func__);

	/* select CLKINPULOW (per_hs_clk_div_ck) as DPLL_PER bypass clock */
	state.per_hsd_byp_clk_mux_ck_parent = per_hsd_byp_clk_mux_ck->parent;
	ret = clk_set_parent(per_hsd_byp_clk_mux_ck, per_hs_clk_div_ck);
	if (ret) {
		pr_debug("%s: failed reparenting DPLL_PER bypass clock to CLKINPULOW\n",
				__func__);
		goto per_bypass_clk_reparent_fail;
	} else
		pr_debug("%s: reparented DPLL_PER bypass clock to CLKINPULOW\n",
				__func__);

	/* bypass DPLL_MPU */
	state.dpll_mpu_ck_rate = dpll_mpu_ck->rate;

	mpu_dev = omap2_get_mpuss_device();
	opp = opp_find_voltage(mpu_dev, 1005000, false);
	opp_enable(opp);
	state.mpu_opp = opp;

	cp = cpufreq_cpu_get(0);
	state.cpufreq_policy_cur_rate = cp->cur;
	state.cpufreq_policy_min_rate = cp->min;
	state.cpufreq_policy_max_rate = cp->max;

	/* cpufreq takes in KHz */
	cp->min = LP_196M_RATE / 1000;
	cp->max = LP_196M_RATE / 1000;
	cpufreq_cpu_put(cp);
	ret = cpufreq_driver_target(cp, LP_196M_RATE / 1000, CPUFREQ_RELATION_H);
	if (ret) {
		pr_err("%s: DPLL_MPU failed to enter Low Power bypass\n",
				__func__);
		goto dpll_mpu_bypass_fail;
	} else
		pr_debug("%s: DPLL_MPU entered Low Power bypass\n", __func__);

	omap4_lpmode = true;

	/* bypass DPLL_IVA */
	state.dpll_iva_ck_rate = dpll_iva_ck->rate;
	ret = omap3_noncore_dpll_set_rate(dpll_iva_ck,
			dpll_iva_ck->dpll_data->clk_bypass->rate);
	if (ret) {
		pr_err("%s: DPLL_IVA failed to enter Low Power bypass\n",
				__func__);
		goto dpll_iva_bypass_fail;
	} else
		pr_debug("%s: DPLL_IVA entered Low Power bypass\n", __func__);

	/* bypass DPLL_PER */
	state.dpll_per_ck_rate = dpll_per_ck->rate;
	ret = omap3_noncore_dpll_set_rate(dpll_per_ck,
			dpll_per_ck->dpll_data->clk_bypass->rate);
	if (ret) {
		pr_debug("%s: DPLL_PER failed to enter Low Power bypass\n",
				__func__);
		goto dpll_per_bypass_fail;
	} else
		pr_debug("%s: DPLL_PER entered Low Power bypass\n",__func__);

	/* run PER functional clocks at full speed */
	state.cm2_scale_fclk_div =
		omap4_prm_read_bits_shift(func_48m_fclk->clksel_reg,
				func_48m_fclk->clksel_mask);
	clk_set_rate(func_48m_fclk, (func_48m_fclk->parent->rate / 4));

	/* recalibarte UART rate for half rates */
	omap_uart_recalibrate_baud(1);

	__raw_writel(1, OMAP4430_CM_L4_WKUP_CLKSEL);

	/* never de-assert CLKREQ while in DPLL cascading scheme */
	state.clkreqctrl = __raw_readl(OMAP4430_PRM_CLKREQCTRL);
	__raw_writel(0x4, OMAP4430_PRM_CLKREQCTRL);

	/* drive PM debug clocks from CORE_M6X2 and allow the clkdm to idle */
	/*state.pmd_stm_clock_mux_ck_parent = pmd_stm_clock_mux_ck->parent;
	state.pmd_trace_clk_mux_ck_parent = pmd_trace_clk_mux_ck->parent;
	ret =  clk_set_parent(pmd_stm_clock_mux_ck, dpll_core_m6x2_ck);
	ret |= clk_set_parent(pmd_trace_clk_mux_ck, dpll_core_m6x2_ck);
	if (ret)
		pr_err("%s: failed reparenting PMD clocks to ABE LP clock\n",
				__func__);
	else
		pr_debug("%s: reparented PMD clocks to ABE LP clock\n",
				__func__);

	omap2_clkdm_allow_idle(emu_sys_44xx_clkdm);*/

	recalculate_root_clocks();

	goto out;

dpll_per_bypass_fail:
	clk_set_rate(div_iva_hs_clk, (div_iva_hs_clk->parent->rate /
				(1 << state.div_iva_hs_clk_div)));
	clk_set_rate(dpll_iva_ck, state.dpll_iva_ck_rate);
per_bypass_clk_reparent_fail:
	clk_set_parent(per_hsd_byp_clk_mux_ck,
			state.per_hsd_byp_clk_mux_ck_parent);
dpll_iva_bypass_fail:
	clk_set_rate(div_iva_hs_clk, (div_iva_hs_clk->parent->rate /
				(1 << state.div_iva_hs_clk_div)));
	clk_set_rate(dpll_iva_ck, state.dpll_iva_ck_rate);
dpll_mpu_bypass_fail:
	omap4_lpmode = false;
	clk_set_rate(div_mpu_hs_clk, (div_mpu_hs_clk->parent->rate /
				(1 << state.div_mpu_hs_clk_div)));
	clk_set_rate(dpll_mpu_ck, state.dpll_mpu_ck_rate);
iva_bypass_clk_reparent_fail:
	clk_set_parent(iva_hsd_byp_clk_mux_ck,
			state.iva_hsd_byp_clk_mux_ck_parent);
core_clock_set_rate_fail:
	/* FIXME make this follow the sequence below */
	clk_set_rate(dpll_core_m5x2_ck, (dpll_core_m5x2_ck->parent->rate /
				state.dpll_core_m5x2_ck_div));
	clk_set_rate(dpll_core_ck, (dpll_core_ck->parent->rate /
				state.dpll_core_m2_ck_div));
	clk_set_rate(div_core_ck, (div_core_ck->parent->rate /
				state.div_core_ck_div));
core_bypass_clock_reparent_fail:
	clk_set_parent(iva_hsd_byp_clk_mux_ck,
			state.iva_hsd_byp_clk_mux_ck_parent);
	omap4_prm_rmw_reg_bits(BIT(8), BIT(8),
			dpll_abe_m3x2_ck->clksel_reg);
	omap2_clkdm_allow_idle(abe_44xx_clkdm);
	omap3_dpll_allow_idle(dpll_abe_ck);
	omap3_dpll_allow_idle(dpll_core_ck);
out:
	return ret;
}

int omap4_dpll_low_power_cascade_exit()
{
	int ret = 0;
	struct clk *sys_clkin_ck;
	struct clk *dpll_abe_ck, *dpll_abe_m3x2_ck;
	struct clk *dpll_mpu_ck, *div_mpu_hs_clk;
	struct clk *dpll_iva_ck, *div_iva_hs_clk, *iva_hsd_byp_clk_mux_ck;
	struct clk *dpll_core_ck, *dpll_core_x2_ck;
	struct clk *dpll_core_m2_ck, *dpll_core_m5x2_ck, *dpll_core_m6x2_ck;
	struct clk *core_hsd_byp_clk_mux_ck;
	struct clk *div_core_ck, *l3_div_ck, *l4_div_ck;
	struct clk *dpll_per_ck, *func_48m_fclk;
	struct clk *per_hsd_byp_clk_mux_ck, *per_hs_clk_div_ck;
	struct clk *l4_wkup_clk_mux_ck, *lp_clk_div_ck;
	struct clk *pmd_stm_clock_mux_ck, *pmd_trace_clk_mux_ck;
	struct clockdomain *emu_sys_44xx_clkdm, *abe_44xx_clkdm;
	struct cpufreq_policy *cp;

	sys_clkin_ck = clk_get(NULL, "sys_clkin_ck");
	dpll_abe_ck = clk_get(NULL, "dpll_abe_ck");
	dpll_mpu_ck = clk_get(NULL, "dpll_mpu_ck");
	div_mpu_hs_clk = clk_get(NULL, "div_mpu_hs_clk");
	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	div_iva_hs_clk = clk_get(NULL, "div_iva_hs_clk");
	iva_hsd_byp_clk_mux_ck = clk_get(NULL, "iva_hsd_byp_clk_mux_ck");
	dpll_core_ck = clk_get(NULL, "dpll_core_ck");
	dpll_core_m2_ck = clk_get(NULL, "dpll_core_m2_ck");
	dpll_core_m5x2_ck = clk_get(NULL, "dpll_core_m5x2_ck");
	dpll_core_m6x2_ck = clk_get(NULL, "dpll_core_m6x2_ck");
	dpll_abe_m3x2_ck = clk_get(NULL, "dpll_abe_m3x2_ck");
	dpll_core_x2_ck = clk_get(NULL, "dpll_core_x2_ck");
	core_hsd_byp_clk_mux_ck = clk_get(NULL, "core_hsd_byp_clk_mux_ck");
	div_core_ck = clk_get(NULL, "div_core_ck");
	l3_div_ck = clk_get(NULL, "l3_div_ck");
	l4_div_ck = clk_get(NULL, "l4_div_ck");
	l4_wkup_clk_mux_ck = clk_get(NULL, "l4_wkup_clk_mux_ck");
	lp_clk_div_ck = clk_get(NULL, "lp_clk_div_ck");
	pmd_stm_clock_mux_ck = clk_get(NULL, "pmd_stm_clock_mux_ck");
	pmd_trace_clk_mux_ck = clk_get(NULL, "pmd_trace_clk_mux_ck");
	dpll_per_ck = clk_get(NULL, "dpll_per_ck");
	func_48m_fclk = clk_get(NULL, "func_48m_fclk");
	per_hsd_byp_clk_mux_ck = clk_get(NULL, "per_hsd_byp_clk_mux_ck");
	per_hs_clk_div_ck = clk_get(NULL, "per_hs_clk_div_ck");

	emu_sys_44xx_clkdm = clkdm_lookup("emu_sys_44xx_clkdm");
	abe_44xx_clkdm = clkdm_lookup("abe_clkdm");

	if (!dpll_abe_ck || !dpll_mpu_ck || !div_mpu_hs_clk || !dpll_iva_ck ||
		!div_iva_hs_clk || !iva_hsd_byp_clk_mux_ck || !dpll_core_m2_ck
		|| !dpll_abe_m3x2_ck || !div_core_ck || !dpll_core_x2_ck ||
		!core_hsd_byp_clk_mux_ck || !dpll_core_m5x2_ck ||
		!l4_wkup_clk_mux_ck || !lp_clk_div_ck || !pmd_stm_clock_mux_ck
		|| !pmd_trace_clk_mux_ck || !dpll_core_m6x2_ck
		|| !sys_clkin_ck || !dpll_core_ck || !l3_div_ck || !l4_div_ck
		|| !dpll_per_ck || !func_48m_fclk || !per_hsd_byp_clk_mux_ck
		|| !per_hs_clk_div_ck) {
		pr_warn("%s: failed to get all necessary clocks\n", __func__);
		ret = -ENODEV;
		goto out;
	}

	if (delayed_work_pending(&lpmode_work))
		cancel_delayed_work_sync(&lpmode_work);

	if (!omap4_lpmode)
		return 0;

	omap4_lpmode = false;

	cp = cpufreq_cpu_get(0);
	cp->min = state.cpufreq_policy_min_rate;
	cp->max = state.cpufreq_policy_max_rate;
	cpufreq_cpu_put(cp);
	cpufreq_driver_target(cp, state.cpufreq_policy_cur_rate,
			CPUFREQ_RELATION_H);
	opp_disable(state.mpu_opp);

	/* lock DPLL_IVA */
	ret = omap3_noncore_dpll_set_rate(dpll_iva_ck, state.dpll_iva_ck_rate);
	if (ret)
		pr_err("%s: DPLL_IVA failed to relock\n", __func__);

	/* restore PER functional clocks frequency */
	clk_set_rate(func_48m_fclk, (func_48m_fclk->parent->rate /
				(1 << state.cm2_scale_fclk_div)));

	/* lock DPLL_PER */
	ret = omap3_noncore_dpll_set_rate(dpll_per_ck, state.dpll_per_ck_rate);
	if (ret)
		pr_err("%s: DPLL_PER failed to relock\n", __func__);

	/* restore bypass clock rates */
	clk_set_rate(div_mpu_hs_clk, (div_mpu_hs_clk->parent->rate /
				(1 << state.div_mpu_hs_clk_div)));
	clk_set_rate(div_iva_hs_clk, (div_iva_hs_clk->parent->rate /
				(1 << state.div_iva_hs_clk_div)));

	/* restore DPLL_IVA bypass clock */
	ret = clk_set_parent(iva_hsd_byp_clk_mux_ck,
			state.iva_hsd_byp_clk_mux_ck_parent);
	if (ret)
		pr_err("%s: failed to restore DPLL_IVA bypass clock\n",
				__func__);

	/* restore DPLL_PER bypass clock */
	ret = clk_set_parent(per_hsd_byp_clk_mux_ck,
			state.per_hsd_byp_clk_mux_ck_parent);
	if (ret)
		pr_err("%s: failed to restore DPLL_PER bypass clock\n",
				__func__);

	/* recalibarte UART rate for normal clock rates */
	omap_uart_recalibrate_baud(0);

	/* restore CORE clock rates */
	ret = clk_set_rate(div_core_ck, (div_core_ck->parent->rate /
				(1 << state.div_core_ck_div)));
	omap4_prm_rmw_reg_bits(dpll_core_m2_ck->clksel_mask,
			state.dpll_core_m2_div,
			dpll_core_m2_ck->clksel_reg);
	ret |=  clk_set_rate(dpll_core_m5x2_ck,
			(dpll_core_m5x2_ck->parent->rate /
			 state.dpll_core_m5x2_ck_div));
	ret |= clk_set_rate(dpll_core_ck, state.dpll_core_ck_rate);
	if (ret)
		pr_err("%s: failed to restore CORE clock rates\n", __func__);

	/* drive DPLL_CORE bypass clock from SYS_CK (CLKINP) */
	ret = clk_set_parent(core_hsd_byp_clk_mux_ck,
			state.core_hsd_byp_clk_mux_ck_parent);
	if (ret)
		pr_err("%s: failed restoring DPLL_CORE bypass clock parent\n",
				__func__);

	/* WA: allow DPLL_ABE_M3X2 clock to auto-gate */
	omap4_prm_rmw_reg_bits(BIT(8), 0x0,
			dpll_abe_m3x2_ck->clksel_reg);

	/* allow ABE clock domain to idle again */
	omap2_clkdm_allow_idle(abe_44xx_clkdm);

	/* allow DPLL_ABE & DPLL_CORE to idle again */
	omap3_dpll_allow_idle(dpll_core_ck);
	omap3_dpll_allow_idle(dpll_abe_ck);

	/* DPLLs are configured, so let SYSCK idle again */

	__raw_writel(0, OMAP4430_CM_L4_WKUP_CLKSEL);


	/* restore CLKREQ behavior */
	__raw_writel(state.clkreqctrl, OMAP4430_PRM_CLKREQCTRL);

	/* drive PM debug clocks from CORE_M6X2 and allow the clkdm to idle */
	/*ret =  clk_set_parent(pmd_stm_clock_mux_ck,
			state.pmd_stm_clock_mux_ck_parent);
	ret |= clk_set_parent(pmd_trace_clk_mux_ck,
			state.pmd_trace_clk_mux_ck_parent);
	if (ret)
		pr_debug("%s: failed restoring parent to PMD clocks\n",
				__func__);*/

	recalculate_root_clocks();

out:
	return ret;
}
