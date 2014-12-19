/*
 * OMAP4-specific DPLL control functions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Rajendra Nayak
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/spinlock.h>

#ifdef CONFIG_OMAP4_DPLL_CASCADING
#include <linux/console.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include "dvfs.h"
#include "smartreflex.h"
#endif

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/common.h>

#include <mach/emif.h>
#include <mach/omap4-common.h>

#include "clock.h"
#include "clock44xx.h"
#include "cm.h"
#include "cm44xx.h"
#include "cm1_44xx.h"
#include "cm2_44xx.h"
#include "cminst44xx.h"
#include "clock44xx.h"
#include "clockdomain.h"
#include "cm-regbits-44xx.h"
#include "prcm44xx.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"
#include "prminst44xx.h"

#define MAX_FREQ_UPDATE_TIMEOUT  100000

static struct clockdomain *l3_emif_clkdm;
static DEFINE_SPINLOCK(l3_emif_lock);

#ifdef CONFIG_OMAP4_DPLL_CASCADING
static struct dpll_cascade_saved_state {
	unsigned long dpll_mpu_ck_rate;
	unsigned long dpll_iva_ck_rate;
	unsigned long dpll_core_ck_rate;
	unsigned long dpll_per_ck_rate;
	unsigned long div_mpu_hs_clk_div;
	unsigned long div_iva_hs_clk_div;
	unsigned long div_core_ck_div;
	unsigned long dpll_core_m5x2_ck_div;
	unsigned long dpll_core_m2_div;
	u32 clkreqctrl;
	struct clk *iva_hsd_byp_clk_mux_ck_parent;
	struct clk *core_hsd_byp_clk_mux_ck_parent;
	struct clk *per_hsd_byp_clk_mux_ck_parent;
} state;

struct dpll_cascading_blocker {
	struct device *dev;
	struct list_head node;
};

static atomic_t in_dpll_cascading = ATOMIC_INIT(0);
static LIST_HEAD(dpll_cascading_blocker_list);

static struct clockdomain *l3_init_clkdm, *l4_per_clkdm;
static struct clockdomain *abe_44xx_clkdm;
static struct voltagedomain *vdd_mpu, *vdd_iva, *vdd_core;

static struct clk *sys_clkin_ck;
static struct clk *dpll_abe_ck, *dpll_abe_m3x2_ck;
static struct clk *dpll_mpu_ck, *dpll_mpu_m2_ck, *div_mpu_hs_clk;
static struct clk *dpll_iva_ck, *div_iva_hs_clk, *iva_hsd_byp_clk_mux_ck;
static struct clk *dpll_per_ck, *per_hsd_byp_clk_mux_ck, *per_hs_clk_div_ck;
static struct clk *dpll_core_ck, *dpll_core_x2_ck;
static struct clk *dpll_core_m2_ck, *dpll_core_m5x2_ck;
static struct clk *core_hsd_byp_clk_mux_ck;
static struct clk *div_core_ck, *l3_div_ck, *l4_div_ck;
static struct clk *gpmc_ick;

static bool dpll_cascading_inited;
#endif

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
	unsigned long flags;

	if (!clk || !rate)
		return -EINVAL;

	validrate = omap2_clksel_round_rate_div(clk, rate, &new_div);
	if (validrate != rate)
		return -EINVAL;

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm) {
		l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
		if (!l3_emif_clkdm) {
			pr_err("%s: clockdomain lookup failed\n", __func__);
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&l3_emif_lock, flags);

	/* Configures MEMIF domain in SW_WKUP & increment usecount for clks */
	clkdm_wakeup(l3_emif_clkdm);

	/*
	 * Errata ID: i728
	 *
	 * DESCRIPTION:
	 *
	 * If during a small window the following three events occur:
	 *
	 * 1) The EMIF_PWR_MGMT_CTRL[7:4] REG_SR_TIM SR_TIMING counter expires
	 * 2) Frequency change update is requested CM_SHADOW_FREQ_CONFIG1
	 *    FREQ_UPDATE set to 1
	 * 3) OCP access is requested
	 *
	 * There will be clock instability on the DDR interface.
	 *
	 * WORKAROUND:
	 *
	 * Prevent event 1) while event 2) is happening.
	 *
	 * Disable the self-refresh when requesting a frequency change.
	 * Before requesting a frequency change, program
	 * EMIF_PWR_MGMT_CTRL[10:8] REG_LP_MODE to 0x0
	 * (omap_emif_frequency_pre_notify)
	 *
	 * When the frequency change is completed, reprogram
	 * EMIF_PWR_MGMT_CTRL[10:8] REG_LP_MODE to 0x2.
	 * (omap_emif_frequency_post_notify)
	 */
	omap_emif_frequency_pre_notify();

	/*
	 * maybe program core m5 divider here
	 * definitely program m3, m6 & m7 dividers here
	 * DDR clock = DPLL_CORE_M2_CK / 2.  Program EMIF timing
	 * parameters in EMIF shadow registers for validrate divided
	 * by 2.
	 */
	omap_emif_setup_registers(validrate >> 1, LPDDR2_VOLTAGE_STABLE);

	/*
	 * program DPLL_CORE_M2_DIV with same value as the one already
	 * in direct register and lock DPLL_CORE
	 * DLL_RESET=0 (DLL must NOT be reset upon frequency change)
	 */
	shadow_freq_cfg1 =
		(new_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
		(DPLL_LOCKED << OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
		(0 << OMAP4430_DLL_RESET_SHIFT) |
		(1 << OMAP4430_FREQ_UPDATE_SHIFT);

	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
					& OMAP4430_FREQ_UPDATE_MASK) == 0),
			MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Re-enable DDR self refresh */
	omap_emif_frequency_post_notify();

	/* put MEMIF clkdm back to HW_AUTO & decrement usecount for clks */
	clkdm_allow_idle(l3_emif_clkdm);

	spin_unlock_irqrestore(&l3_emif_lock, flags);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
				__func__);
		return -1;
	}

	/* Update the clock change */
	clk->rate = validrate;
	return 0;
}

#ifdef CONFIG_OMAP4_DPLL_CASCADING
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
	u32 shadow_freq_cfg1 = 0, shadow_freq_cfg2 = 0;
	struct clk *new_parent;
	struct dpll_data *dd;
	unsigned long flags;
	int res = 0;

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
	if (!l3_emif_clkdm) {
		l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
		if (!l3_emif_clkdm) {
			pr_err("%s: clockdomain lookup failed\n", __func__);
			return -EINVAL;
		}
	}

	spin_lock_irqsave(&l3_emif_lock, flags);

	/* Make sure MEMIF clkdm is in SW_WKUP & GPMC clocks are active */
	clkdm_wakeup(l3_emif_clkdm);
	omap2_clk_enable(gpmc_ick);

	/*
	 * Errata ID: i728
	 *
	 * DESCRIPTION:
	 *
	 * If during a small window the following three events occur:
	 *
	 * 1) The EMIF_PWR_MGMT_CTRL[7:4] REG_SR_TIM SR_TIMING counter expires
	 * 2) Frequency change update is requested CM_SHADOW_FREQ_CONFIG1
	 *    FREQ_UPDATE set to 1
	 * 3) OCP access is requested
	 *
	 * There will be clock instability on the DDR interface.
	 *
	 * WORKAROUND:
	 *
	 * Prevent event 1) while event 2) is happening.
	 *
	 * Disable the self-refresh when requesting a frequency change.
	 * Before requesting a frequency change, program
	 * EMIF_PWR_MGMT_CTRL[10:8] REG_LP_MODE to 0x0
	 * (omap_emif_frequency_pre_notify)
	 *
	 * When the frequency change is completed, reprogram
	 * EMIF_PWR_MGMT_CTRL[10:8] REG_LP_MODE to 0x2.
	 * (omap_emif_frequency_post_notify)
	 */
	omap_emif_frequency_pre_notify();

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
		 * program CM_DIV_M2_DPLL_CORE.DPLL_CLKOUT_DIV
		 * for divide by two, ensure DLL_OVERRIDE = '1'
		 * and put DPLL_CORE into LP Bypass
		 */
		m2_div = omap4_prm_read_bits_shift(dpll_core_m2_ck->clksel_reg,
				dpll_core_m2_ck->clksel_mask);
		/*
		* DLL_RESET=0 (DLL must NOT be reset upon frequency change)
		*/
		shadow_freq_cfg1 =
			(m2_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOW_POWER_BYPASS <<
			 OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
			(0 << OMAP4430_DLL_RESET_SHIFT) |
			(1 << OMAP4430_FREQ_UPDATE_SHIFT) |
			(1 << OMAP4430_DLL_OVERRIDE_SHIFT);
		__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

		new_parent = dd->clk_bypass;
	} else {
		if (dd->last_rounded_rate != rate)
			rate = clk->round_rate(clk, rate);

		if (dd->last_rounded_rate == 0) {
			res = -EINVAL;
			goto out;
		}

		if (omap4_prm_read_bits_shift(dd->idlest_reg,
					OMAP4430_ST_DPLL_CLK_MASK)) {
				WARN_ONCE(1, "%s: DPLL core should be in"
					"Low Power Bypass\n", __func__);
				res = -EINVAL;
				goto out;
		}

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
		 * program DPLL_CORE_M2_DIV with same value
		 * as the one already in direct register, ensure
		 * DLL_OVERRIDE = '0' and lock DPLL_CORE
		 */
		m2_div = omap4_prm_read_bits_shift(dpll_core_m2_ck->clksel_reg,
				dpll_core_m2_ck->clksel_mask);

		/*
		* DLL_RESET=0 (DLL must NOT be reset upon frequency change)
		*/
		shadow_freq_cfg1 =
			(m2_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOCKED << OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
			(0 << OMAP4430_DLL_RESET_SHIFT) |
			(1 << OMAP4430_FREQ_UPDATE_SHIFT) |
			(0 << OMAP4430_DLL_OVERRIDE_SHIFT);
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

	/* Re-enable DDR self refresh */
	omap_emif_frequency_post_notify();

	/* Configures MEMIF domain back to HW_WKUP & let GPMC clocks to idle */
	clkdm_allow_idle(l3_emif_clkdm);
	omap2_clk_disable(gpmc_ick);

out:
	spin_unlock_irqrestore(&l3_emif_lock, flags);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
				__func__);
		res = -EBUSY;
	}

	return res;
}

static int __init omap4_dpll_low_power_cascade_init_clocks(void)
{
	sys_clkin_ck = clk_get(NULL, "sys_clkin_ck");
	dpll_abe_ck = clk_get(NULL, "dpll_abe_ck");
	dpll_abe_m3x2_ck = clk_get(NULL, "dpll_abe_m3x2_ck");
	dpll_mpu_ck = clk_get(NULL, "dpll_mpu_ck");
	dpll_mpu_m2_ck = clk_get(NULL, "dpll_mpu_m2_ck");
	div_mpu_hs_clk = clk_get(NULL, "div_mpu_hs_clk");
	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	div_iva_hs_clk = clk_get(NULL, "div_iva_hs_clk");
	iva_hsd_byp_clk_mux_ck = clk_get(NULL, "iva_hsd_byp_clk_mux_ck");
	dpll_per_ck = clk_get(NULL, "dpll_per_ck");
	per_hsd_byp_clk_mux_ck = clk_get(NULL, "per_hsd_byp_clk_mux_ck");
	per_hs_clk_div_ck = clk_get(NULL, "per_hs_clk_div_ck");
	dpll_core_ck = clk_get(NULL, "dpll_core_ck");
	dpll_core_x2_ck = clk_get(NULL, "dpll_core_x2_ck");
	dpll_core_m2_ck = clk_get(NULL, "dpll_core_m2_ck");
	dpll_core_m5x2_ck = clk_get(NULL, "dpll_core_m5x2_ck");
	core_hsd_byp_clk_mux_ck = clk_get(NULL, "core_hsd_byp_clk_mux_ck");
	div_core_ck = clk_get(NULL, "div_core_ck");
	l3_div_ck = clk_get(NULL, "l3_div_ck");
	l4_div_ck = clk_get(NULL, "l4_div_ck");
	gpmc_ick = clk_get(NULL, "gpmc_ick");

	l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
	l3_init_clkdm = clkdm_lookup("l3_init_clkdm");
	l4_per_clkdm = clkdm_lookup("l4_per_clkdm");
	abe_44xx_clkdm = clkdm_lookup("abe_clkdm");

	/* look up the three scalable voltage domains */
	vdd_mpu = voltdm_lookup("mpu");
	vdd_iva = voltdm_lookup("iva");
	vdd_core = voltdm_lookup("core");

	if (!sys_clkin_ck || !dpll_abe_ck || !dpll_abe_m3x2_ck ||
		!dpll_mpu_ck || !dpll_mpu_m2_ck || !div_mpu_hs_clk ||
		!dpll_iva_ck || !div_iva_hs_clk || !iva_hsd_byp_clk_mux_ck ||
		!dpll_per_ck || !per_hsd_byp_clk_mux_ck || !per_hs_clk_div_ck ||
		!dpll_core_ck || !dpll_core_x2_ck || !dpll_core_m2_ck ||
		!dpll_core_m5x2_ck ||
		!core_hsd_byp_clk_mux_ck || !div_core_ck ||
		!l3_div_ck || !l4_div_ck || !gpmc_ick ||
		!l3_emif_clkdm || !l3_init_clkdm || !l4_per_clkdm ||
		!abe_44xx_clkdm || !vdd_mpu || !vdd_iva || !vdd_core)
		dpll_cascading_inited = false;
	else
		dpll_cascading_inited = true;

	return 0;
}
late_initcall(omap4_dpll_low_power_cascade_init_clocks);

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
static int omap4_dpll_low_power_cascade_enter(void)
{
	int ret = 0;
	u32 mask;

	if (!dpll_cascading_inited) {
		pr_warn("%s: failed to get all necessary clocks\n", __func__);
		return -ENODEV;
	}

	omap_sr_disable(vdd_mpu);
	omap_sr_disable(vdd_iva);
	omap_sr_disable(vdd_core);
	atomic_set(&in_dpll_cascading, true);

	/* prevent DPLL_ABE & DPLL_CORE from idling */
	omap3_dpll_deny_idle(dpll_abe_ck);
	omap3_dpll_deny_idle(dpll_core_ck);

	/* put ABE clock domain SW_WKUP */
	clkdm_wakeup(abe_44xx_clkdm);

	/* WA: prevent DPLL_ABE_M3X2 clock from auto-gating */
	omap4_prm_rmw_reg_bits(BIT(8), BIT(8),
			dpll_abe_m3x2_ck->clksel_reg);

	/* 1. Bypass CORE DPLL */
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
	state.dpll_core_m5x2_ck_div =
		omap4_prm_read_bits_shift(dpll_core_m5x2_ck->clksel_reg,
				dpll_core_m5x2_ck->clksel_mask);
	state.dpll_core_m2_div =
		omap4_prm_read_bits_shift(dpll_core_m2_ck->clksel_reg,
				dpll_core_m2_ck->clksel_mask);

	ret =  clk_set_rate(div_core_ck, dpll_core_m5x2_ck->rate);
	ret |= clk_set_rate(dpll_core_ck,
				dpll_core_ck->dpll_data->clk_bypass->rate);
	ret |= clk_set_rate(dpll_core_m5x2_ck, dpll_core_x2_ck->rate);
	if (ret) {
		pr_err("%s: failed setting CORE clock rates\n", __func__);
		goto core_clock_set_rate_fail;
	} else
		pr_debug("%s: DPLL_CORE rates set successfully\n",
				__func__);

	/* 2. divide MPU/IVA bypass clocks by 2 (when we bypass DPLL_CORE) */
	state.div_mpu_hs_clk_div =
		omap4_prm_read_bits_shift(div_mpu_hs_clk->clksel_reg,
				div_mpu_hs_clk->clksel_mask);
	state.div_iva_hs_clk_div =
		omap4_prm_read_bits_shift(div_iva_hs_clk->clksel_reg,
				div_iva_hs_clk->clksel_mask);
	clk_set_rate(div_mpu_hs_clk, div_mpu_hs_clk->parent->rate);
	clk_set_rate(div_iva_hs_clk, div_iva_hs_clk->parent->rate / 2);

	/* 3. Bypass IVA DPLL */
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

	/* bypass DPLL_IVA */
	state.dpll_iva_ck_rate = dpll_iva_ck->rate;
	ret = clk_set_rate(dpll_iva_ck,
			dpll_iva_ck->dpll_data->clk_bypass->rate);
	if (ret) {
		pr_err("%s: DPLL_IVA failed to enter Low Power bypass\n",
				__func__);
		goto dpll_iva_bypass_fail;
	} else
		pr_debug("%s: DPLL_IVA entered Low Power bypass\n", __func__);

	/* 4. Bypass MPU DPLL */
	/* bypass DPLL_MPU */
	state.dpll_mpu_ck_rate = dpll_mpu_ck->rate;
	ret = clk_set_rate(dpll_mpu_ck,
			dpll_mpu_ck->dpll_data->clk_bypass->rate);
	if (ret) {
		pr_err("%s: DPLL_MPU failed to enter Low Power bypass\n",
				__func__);
		goto dpll_mpu_bypass_fail;
	} else
		pr_debug("%s: DPLL_MPU entered Low Power bypass\n", __func__);

	/* 5. Bypass PER DPLL */
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

	/* bypass DPLL_PER */
	state.dpll_per_ck_rate = dpll_per_ck->rate;

	ret = clk_set_rate(dpll_per_ck,
			dpll_per_ck->dpll_data->clk_bypass->rate);
	if (ret) {
		pr_err("%s: DPLL_PER failed to enter Low Power bypass\n",
				__func__);
		goto dpll_per_bypass_fail;
	} else
		pr_debug("%s: DPLL_PER entered Low Power bypass\n", __func__);

	__raw_writel(1, OMAP4430_CM_L4_WKUP_CLKSEL);

	/* never de-assert CLKREQ while in DPLL cascading scheme */
	state.clkreqctrl = __raw_readl(OMAP4430_PRM_CLKREQCTRL);
	__raw_writel(0x4, OMAP4430_PRM_CLKREQCTRL);

	/* PRCM requires target clockdomain to be woken up before
	 * changing its Static Dependencies settings
	 */

	/* Note: Domains not built with SW_WKUP capability like
	 * L3_1, L3_2, L4_CFG and L4_WKUP may stall idle transition
	 * during 1 idle cycle if SD is changed while domain is OFF
	 */

	/* Configures MEMIF clockdomain in SW_WKUP */
	if (cpu_is_omap443x())
		clkdm_wakeup(l3_emif_clkdm);

	/* Configures L3INIT clockdomain in SW_WKUP */
	clkdm_wakeup(l3_init_clkdm);

	/* Configures L4_PER clockdomain in SW_WKUP */
	clkdm_wakeup(l4_per_clkdm);

	if (cpu_is_omap443x()) {
		/* Disable SD for MPU towards EMIF, L3_1, L3_2, L3INIT
		 * and L4CFG clockdomains */
		mask = OMAP4430_MEMIF_STATDEP_MASK |
			OMAP4430_L3_2_STATDEP_MASK |
			OMAP4430_L4CFG_STATDEP_MASK |
			OMAP4430_L3_1_STATDEP_MASK |
			OMAP4430_L3INIT_STATDEP_MASK;
	} else {
		/* Disable SD for MPU towards L3_1, L3_2, L3INIT
		 * clockdomains */
		mask = OMAP4430_L3_2_STATDEP_MASK |
			OMAP4430_L3_1_STATDEP_MASK |
			OMAP4430_L3INIT_STATDEP_MASK;
	}

	omap4_cminst_rmw_inst_reg_bits(mask, 0,
					OMAP4430_CM1_PARTITION,
					OMAP4430_CM1_MPU_INST,
					OMAP4_CM_MPU_STATICDEP_OFFSET);

	/* Configures MEMIF clockdomain back to HW_AUTO */
	if (cpu_is_omap443x())
		clkdm_allow_idle(l3_emif_clkdm);

	/* Configures L3INIT clockdomain back to HW_AUTO */
	clkdm_allow_idle(l3_init_clkdm);

	/* Configures L4_PER clockdomain back to HW_AUTO */
	clkdm_allow_idle(l4_per_clkdm);

	recalculate_root_clocks();

	goto sr_enable;

dpll_per_bypass_fail:
	clk_set_rate(dpll_per_ck, state.dpll_per_ck_rate);
per_bypass_clk_reparent_fail:
	clk_set_parent(per_hsd_byp_clk_mux_ck,
			state.per_hsd_byp_clk_mux_ck_parent);
dpll_mpu_bypass_fail:
	clk_set_rate(div_mpu_hs_clk, (div_mpu_hs_clk->parent->rate /
				(1 << state.div_mpu_hs_clk_div)));
	clk_set_rate(dpll_mpu_ck, state.dpll_mpu_ck_rate);
dpll_iva_bypass_fail:
	clk_set_rate(div_iva_hs_clk, (div_iva_hs_clk->parent->rate /
				(1 << state.div_iva_hs_clk_div)));
	clk_set_rate(dpll_iva_ck, state.dpll_iva_ck_rate);
iva_bypass_clk_reparent_fail:
	clk_set_parent(iva_hsd_byp_clk_mux_ck,
			state.iva_hsd_byp_clk_mux_ck_parent);
core_clock_set_rate_fail:
	/* FIXME make this follow the sequence below */
	clk_set_rate(dpll_core_m5x2_ck,
		(dpll_core_m5x2_ck->parent->rate /
			state.dpll_core_m5x2_ck_div));
	clk_set_rate(dpll_core_ck,
			state.dpll_core_ck_rate);
	clk_set_rate(div_core_ck, (div_core_ck->parent->rate /
						state.div_core_ck_div));
core_bypass_clock_reparent_fail:
	clk_set_parent(core_hsd_byp_clk_mux_ck,
				state.core_hsd_byp_clk_mux_ck_parent);
	omap4_prm_rmw_reg_bits(BIT(8), 0,
			dpll_abe_m3x2_ck->clksel_reg);
	clkdm_allow_idle(abe_44xx_clkdm);
	omap3_dpll_allow_idle(dpll_abe_ck);
	omap3_dpll_allow_idle(dpll_core_ck);
	atomic_set(&in_dpll_cascading, false);

sr_enable:
	omap_sr_enable(vdd_mpu, omap_voltage_get_curr_vdata(vdd_mpu));
	omap_sr_enable(vdd_iva, omap_voltage_get_curr_vdata(vdd_iva));
	omap_sr_enable(vdd_core, omap_voltage_get_curr_vdata(vdd_core));
	return ret;
}

static int omap4_dpll_low_power_cascade_exit(void)
{
	int ret = 0;
	u32 mask;

	if (!dpll_cascading_inited) {
		pr_warn("%s: failed to get all necessary clocks\n", __func__);
		atomic_set(&in_dpll_cascading, false);
		return -ENODEV;
	}

	omap_sr_disable(vdd_mpu);
	omap_sr_disable(vdd_iva);
	omap_sr_disable(vdd_core);

	/* PRCM requires target clockdomain to be woken up before
	 * changing its Static Dependencies settings
	 */

	/* Note: Domains not built with SW_WKUP capability like
	 * L3_1, L3_2, L4_CFG and L4_WKUP may stall idle transition
	 * during 1 idle cycle if SD is changed while domain is OFF
	 */

	/* Configures MEMIF clockdomain in SW_WKUP */
	if (cpu_is_omap443x())
		clkdm_wakeup(l3_emif_clkdm);

	/* Configures L3INIT clockdomain in SW_WKUP */
	clkdm_wakeup(l3_init_clkdm);

	/* Configures L4_PER clockdomain in SW_WKUP */
	clkdm_wakeup(l4_per_clkdm);

	if (cpu_is_omap443x()) {
		/* Enable SD for MPU towards EMIF, L3_1, L3_2, L3INIT,
		 * and L4CFG clockdomains */
		mask = OMAP4430_MEMIF_STATDEP_MASK |
			OMAP4430_L3_2_STATDEP_MASK |
			OMAP4430_L4CFG_STATDEP_MASK |
			OMAP4430_L3_1_STATDEP_MASK |
			OMAP4430_L3INIT_STATDEP_MASK;
	} else {
		/* Enable SD for MPU towards L3_1, L3_2, L3INIT
		 * clockdomains */
		mask = OMAP4430_L3_2_STATDEP_MASK |
			OMAP4430_L3_1_STATDEP_MASK |
			OMAP4430_L3INIT_STATDEP_MASK;
	}

	omap4_cminst_rmw_inst_reg_bits(mask, mask,
					OMAP4430_CM1_PARTITION,
					OMAP4430_CM1_MPU_INST,
					OMAP4_CM_MPU_STATICDEP_OFFSET);

	/* Configures MEMIF clockdomain back to HW_AUTO */
	if (cpu_is_omap443x())
		clkdm_allow_idle(l3_emif_clkdm);

	/* Configures L3INIT clockdomain back to HW_AUTO */
	clkdm_allow_idle(l3_init_clkdm);

	/* Configures L4_PER clockdomain back to HW_AUTO */
	clkdm_allow_idle(l4_per_clkdm);

	/* lock DPLL_PER */
	ret = clk_set_rate(dpll_per_ck, state.dpll_per_ck_rate);
	if (ret)
		pr_err("%s: DPLL_PER failed to relock\n", __func__);

	/* restore DPLL_PER bypass clock */
	ret = clk_set_parent(per_hsd_byp_clk_mux_ck,
			state.per_hsd_byp_clk_mux_ck_parent);
	if (ret)
		pr_err("%s: failed to restore DPLL_PER bypass clock\n",
				__func__);

	/* lock DPLL_MPU */
	ret = clk_set_rate(dpll_mpu_ck, state.dpll_mpu_ck_rate);
	if (ret)
		pr_err("%s: DPLL_MPU failed to relock\n", __func__);

	/* lock DPLL_IVA */
	ret = clk_set_rate(dpll_iva_ck, state.dpll_iva_ck_rate);
	if (ret)
		pr_err("%s: DPLL_IVA failed to relock\n", __func__);

	/* restore DPLL_IVA bypass clock */
	ret = clk_set_parent(iva_hsd_byp_clk_mux_ck,
			state.iva_hsd_byp_clk_mux_ck_parent);
	if (ret)
		pr_err("%s: failed to restore DPLL_IVA bypass clock\n",
				__func__);

	/* restore bypass clock rates */
	clk_set_rate(div_mpu_hs_clk, (div_mpu_hs_clk->parent->rate /
				(1 << state.div_mpu_hs_clk_div)));
	clk_set_rate(div_iva_hs_clk, (div_iva_hs_clk->parent->rate /
				(1 << state.div_iva_hs_clk_div)));

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
	omap4_prm_rmw_reg_bits(BIT(8), 0,
			dpll_abe_m3x2_ck->clksel_reg);

	/* allow ABE clock domain to idle again */
	clkdm_allow_idle(abe_44xx_clkdm);

	/* allow DPLL_ABE & DPLL_CORE to idle again */
	omap3_dpll_allow_idle(dpll_core_ck);
	omap3_dpll_allow_idle(dpll_abe_ck);

	/* DPLLs are configured, so let SYSCK idle again */
	__raw_writel(0, OMAP4430_CM_L4_WKUP_CLKSEL);

	/* restore CLKREQ behavior */
	__raw_writel(state.clkreqctrl, OMAP4430_PRM_CLKREQCTRL);

	recalculate_root_clocks();
	atomic_set(&in_dpll_cascading, false);
	omap_sr_enable(vdd_mpu, omap_voltage_get_curr_vdata(vdd_mpu));
	omap_sr_enable(vdd_iva, omap_voltage_get_curr_vdata(vdd_iva));
	omap_sr_enable(vdd_core, omap_voltage_get_curr_vdata(vdd_core));
	return ret;
}

#endif

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
int omap4_prcm_freq_update(void)
{
	u32 shadow_freq_cfg1;
	int i = 0;
	unsigned long flags;

	if (!l3_emif_clkdm) {
		pr_err("%s: clockdomain lookup failed\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&l3_emif_lock, flags);
	/* Configures MEMIF domain in SW_WKUP */
	clkdm_wakeup(l3_emif_clkdm);

	/* Disable DDR self refresh (Errata ID: i728) */
	omap_emif_frequency_pre_notify();

	/*
	 * FREQ_UPDATE sequence:
	 * - DLL_OVERRIDE=0 (DLL lock & code must not be overridden
	 *	after CORE DPLL lock)
	 * - FREQ_UPDATE=1 (to start HW sequence)
	 */
	shadow_freq_cfg1 = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1);
	shadow_freq_cfg1 &= ~(1 << OMAP4430_DLL_RESET_SHIFT);
	shadow_freq_cfg1 |= (1 << OMAP4430_FREQ_UPDATE_SHIFT);
	shadow_freq_cfg1 &= ~OMAP4430_DLL_OVERRIDE_MASK;
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
				& OMAP4430_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Re-enable DDR self refresh */
	omap_emif_frequency_post_notify();

	/* Configures MEMIF domain back to HW_WKUP */
	clkdm_allow_idle(l3_emif_clkdm);

	spin_unlock_irqrestore(&l3_emif_lock, flags);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update failed (call from %pF)\n",
			__func__, (void *)_RET_IP_);
		return -1;
	}

	return 0;
}

/* Use a very high retry count - we should not hit this condition */
#define MAX_DPLL_WAIT_TRIES	1000000

#define OMAP_1_5GHz	1500000000
#define OMAP_1_2GHz	1200000000
#define OMAP_1GHz	1000000000
#define OMAP_920MHz	920000000
#define OMAP_748MHz	748000000

/* Supported only on OMAP4 */
int omap4_dpllmx_gatectrl_read(struct clk *clk)
{
	u32 v;
	u32 mask;

	if (!clk || !clk->clksel_reg || !cpu_is_omap44xx())
		return -EINVAL;

	mask = clk->flags & CLOCK_CLKOUTX2 ?
			OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK :
			OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK;

	v = __raw_readl(clk->clksel_reg);
	v &= mask;
	v >>= __ffs(mask);

	return v;
}

void omap4_dpllmx_allow_gatectrl(struct clk *clk)
{
	u32 v;
	u32 mask;

	if (!clk || !clk->clksel_reg || !cpu_is_omap44xx())
		return;

	mask = clk->flags & CLOCK_CLKOUTX2 ?
			OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK :
			OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK;

	v = __raw_readl(clk->clksel_reg);
	/* Clear the bit to allow gatectrl */
	v &= ~mask;
	__raw_writel(v, clk->clksel_reg);
}

void omap4_dpllmx_deny_gatectrl(struct clk *clk)
{
	u32 v;
	u32 mask;

	if (!clk || !clk->clksel_reg || !cpu_is_omap44xx())
		return;

	mask = clk->flags & CLOCK_CLKOUTX2 ?
			OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK :
			OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK;

	v = __raw_readl(clk->clksel_reg);
	/* Set the bit to deny gatectrl */
	v |= mask;
	__raw_writel(v, clk->clksel_reg);
}

const struct clkops clkops_omap4_dpllmx_ops = {
	.allow_idle	= omap4_dpllmx_allow_gatectrl,
	.deny_idle	= omap4_dpllmx_deny_gatectrl,
};

static void omap4460_mpu_dpll_update_children(unsigned long rate)
{
	u32 v;

	/*
	 * The interconnect frequency to EMIF should
	 * be switched between MPU clk divide by 4 (for
	 * frequencies higher than 920Mhz) and MPU clk divide
	 * by 2 (for frequencies lower than or equal to 920Mhz)
	 * Also the async bridge to ABE must be MPU clk divide
	 * by 8 for MPU clk > 748Mhz and MPU clk divide by 4
	 * for lower frequencies.
	 */
	v = __raw_readl(OMAP4430_CM_MPU_MPU_CLKCTRL);
	if (rate > OMAP_920MHz)
		v |= OMAP4460_CLKSEL_EMIF_DIV_MODE_MASK;
	else
		v &= ~OMAP4460_CLKSEL_EMIF_DIV_MODE_MASK;

	if (rate > OMAP_748MHz)
		v |= OMAP4460_CLKSEL_ABE_DIV_MODE_MASK;
	else
		v &= ~OMAP4460_CLKSEL_ABE_DIV_MODE_MASK;
	__raw_writel(v, OMAP4430_CM_MPU_MPU_CLKCTRL);
}

int omap4460_mpu_dpll_set_rate(struct clk *clk, unsigned long rate)
{
	struct dpll_data *dd;
	u32 v;
	unsigned long dpll_rate;

	if (!clk || !rate || !clk->parent)
		return -EINVAL;

	dd = clk->parent->dpll_data;

	if (!dd)
		return -EINVAL;

	if (!clk->parent->set_rate)
		return -EINVAL;

	if (rate > clk->rate)
		omap4460_mpu_dpll_update_children(rate);

	/*
	 * To obtain MPU DPLL frequency higher than 1GHz, On OMAP4470,
	 * DCC (Duty Cycle Correction) needs to be enabled.
	 * And needs to be kept disabled for < 1 Ghz.
	 *
	 * OMAP4460 has a HW issue with DCC so until proper WA is found
	 * DCC shouldn't be used at any frequency.
	 */
	dpll_rate = omap2_get_dpll_rate(clk->parent);
	if (!cpu_is_omap447x() || rate <= OMAP_1GHz) {
		/* If DCC is enabled, disable it */
		v = __raw_readl(dd->mult_div1_reg);
		if (v & OMAP4460_DCC_EN_MASK) {
			v &= ~OMAP4460_DCC_EN_MASK;
			__raw_writel(v, dd->mult_div1_reg);
		}

		if (rate != dpll_rate)
			clk->parent->set_rate(clk->parent, rate);
	} else {
		/*
		 * On OMAP4470, the MPU clk for frequencies higher than 1Ghz
		 * is sourced from CLKOUTX2_M3, instead of CLKOUT_M2, while
		 * value of M3 is fixed to 1. Hence for frequencies higher
		 * than 1 Ghz, lock the DPLL at half the rate so the
		 * CLKOUTX2_M3 then matches the requested rate.
		 */
		if (rate != dpll_rate * 2)
			clk->parent->set_rate(clk->parent, rate / 2);

		v = __raw_readl(dd->mult_div1_reg);
		v &= ~OMAP4460_DCC_COUNT_MAX_MASK;
		v |= (5 << OMAP4460_DCC_COUNT_MAX_SHIFT);
		__raw_writel(v, dd->mult_div1_reg);

		v |= OMAP4460_DCC_EN_MASK;
		__raw_writel(v, dd->mult_div1_reg);
	}

	if (rate < clk->rate)
		omap4460_mpu_dpll_update_children(rate);

	clk->rate = rate;

	return 0;
}

long omap4460_mpu_dpll_round_rate(struct clk *clk, unsigned long rate)
{
	if (!clk || !rate || !clk->parent)
		return -EINVAL;

	if (clk->parent->round_rate)
		return clk->parent->round_rate(clk->parent, rate);
	else
		return 0;
}

unsigned long omap4460_mpu_dpll_recalc(struct clk *clk)
{
	struct dpll_data *dd;
	u32 v;

	if (!clk || !clk->parent)
		return -EINVAL;

	dd = clk->parent->dpll_data;

	if (!dd)
		return -EINVAL;

	v = __raw_readl(dd->mult_div1_reg);
	if (v & OMAP4460_DCC_EN_MASK)
		return omap2_get_dpll_rate(clk->parent) * 2;
	else
		return omap2_get_dpll_rate(clk->parent);
}

unsigned long omap4_dpll_regm4xen_recalc(struct clk *clk)
{
       u32 v;
       unsigned long rate;
       struct dpll_data *dd;

       if (!clk || !clk->dpll_data)
               return -EINVAL;

       dd = clk->dpll_data;

       rate = omap2_get_dpll_rate(clk);

       /* regm4xen adds a multiplier of 4 to DPLL calculations */
       v = __raw_readl(dd->control_reg);
       if (v & OMAP4430_DPLL_REGM4XEN_MASK)
               rate *= OMAP4430_REGM4XEN_MULT;

       return rate;
}

long omap4_dpll_regm4xen_round_rate(struct clk *clk, unsigned long target_rate)
{
       u32 v;
       struct dpll_data *dd;

       if (!clk || !clk->dpll_data)
               return -EINVAL;

       dd = clk->dpll_data;

       /* regm4xen adds a multiplier of 4 to DPLL calculations */
       v = __raw_readl(dd->control_reg) & OMAP4430_DPLL_REGM4XEN_MASK;

       if (v)
               target_rate = target_rate / OMAP4430_REGM4XEN_MULT;

       omap2_dpll_round_rate(clk, target_rate);

       if (v)
               clk->dpll_data->last_rounded_rate *= OMAP4430_REGM4XEN_MULT;

       return clk->dpll_data->last_rounded_rate;
}

struct dpll_reg_tuple {
	u16 addr;
	u32 val;
};

struct omap4_dpll_regs {
	char *name;
	u32 mod_partition;
	u32 mod_inst;
	struct dpll_reg_tuple clkmode;
	struct dpll_reg_tuple autoidle;
	struct dpll_reg_tuple idlest;
	struct dpll_reg_tuple clksel;
	struct dpll_reg_tuple div_m2;
	struct dpll_reg_tuple div_m3;
	struct dpll_reg_tuple div_m4;
	struct dpll_reg_tuple div_m5;
	struct dpll_reg_tuple div_m6;
	struct dpll_reg_tuple div_m7;
	struct dpll_reg_tuple clkdcoldo;
};

static struct omap4_dpll_regs dpll_regs[] = {
	/* MPU DPLL */
	{ .name		= "mpu",
	  .mod_partition = OMAP4430_CM1_PARTITION,
	  .mod_inst	= OMAP4430_CM1_CKGEN_INST,
	  .clkmode	= {.addr = OMAP4_CM_CLKMODE_DPLL_MPU_OFFSET},
	  .autoidle	= {.addr = OMAP4_CM_AUTOIDLE_DPLL_MPU_OFFSET},
	  .idlest	= {.addr = OMAP4_CM_IDLEST_DPLL_MPU_OFFSET},
	  .clksel	= {.addr = OMAP4_CM_CLKSEL_DPLL_MPU_OFFSET},
	  .div_m2	= {.addr = OMAP4_CM_DIV_M2_DPLL_MPU_OFFSET},
	},
	/* IVA DPLL */
	{ .name		= "iva",
	  .mod_partition = OMAP4430_CM1_PARTITION,
	  .mod_inst	= OMAP4430_CM1_CKGEN_INST,
	  .clkmode	= {.addr = OMAP4_CM_CLKMODE_DPLL_IVA_OFFSET},
	  .autoidle	= {.addr = OMAP4_CM_AUTOIDLE_DPLL_IVA_OFFSET},
	  .idlest	= {.addr = OMAP4_CM_IDLEST_DPLL_IVA_OFFSET},
	  .clksel	= {.addr = OMAP4_CM_CLKSEL_DPLL_IVA_OFFSET},
	  .div_m4	= {.addr = OMAP4_CM_DIV_M4_DPLL_IVA_OFFSET},
	  .div_m5	= {.addr = OMAP4_CM_DIV_M5_DPLL_IVA_OFFSET},
	},
	/* ABE DPLL */
	{ .name		= "abe",
	  .mod_partition = OMAP4430_CM1_PARTITION,
	  .mod_inst	= OMAP4430_CM1_CKGEN_INST,
	  .clkmode	= {.addr = OMAP4_CM_CLKMODE_DPLL_ABE_OFFSET},
	  .autoidle	= {.addr = OMAP4_CM_AUTOIDLE_DPLL_ABE_OFFSET},
	  .idlest	= {.addr = OMAP4_CM_IDLEST_DPLL_ABE_OFFSET},
	  .clksel	= {.addr = OMAP4_CM_CLKSEL_DPLL_ABE_OFFSET},
	  .div_m2	= {.addr = OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET},
	  .div_m3	= {.addr = OMAP4_CM_DIV_M3_DPLL_ABE_OFFSET},
	},
	/* USB DPLL */
	{ .name		= "usb",
	  .mod_partition = OMAP4430_CM2_PARTITION,
	  .mod_inst	= OMAP4430_CM2_CKGEN_INST,
	  .clkmode	= {.addr = OMAP4_CM_CLKMODE_DPLL_USB_OFFSET},
	  .autoidle	= {.addr = OMAP4_CM_AUTOIDLE_DPLL_USB_OFFSET},
	  .idlest	= {.addr = OMAP4_CM_IDLEST_DPLL_USB_OFFSET},
	  .clksel	= {.addr = OMAP4_CM_CLKSEL_DPLL_USB_OFFSET},
	  .div_m2	= {.addr = OMAP4_CM_DIV_M2_DPLL_USB_OFFSET},
	  .clkdcoldo	= {.addr = OMAP4_CM_CLKDCOLDO_DPLL_USB_OFFSET},
	 },
	/* PER DPLL */
	{ .name		= "per",
	  .mod_partition = OMAP4430_CM2_PARTITION,
	  .mod_inst	= OMAP4430_CM2_CKGEN_INST,
	  .clkmode	= {.addr = OMAP4_CM_CLKMODE_DPLL_PER_OFFSET},
	  .autoidle	= {.addr = OMAP4_CM_AUTOIDLE_DPLL_PER_OFFSET},
	  .idlest	= {.addr = OMAP4_CM_IDLEST_DPLL_PER_OFFSET},
	  .clksel	= {.addr = OMAP4_CM_CLKSEL_DPLL_PER_OFFSET},
	  .div_m2	= {.addr = OMAP4_CM_DIV_M2_DPLL_PER_OFFSET},
	  .div_m3	= {.addr = OMAP4_CM_DIV_M3_DPLL_PER_OFFSET},
	  .div_m4	= {.addr = OMAP4_CM_DIV_M4_DPLL_PER_OFFSET},
	  .div_m5	= {.addr = OMAP4_CM_DIV_M5_DPLL_PER_OFFSET},
	  .div_m6	= {.addr = OMAP4_CM_DIV_M6_DPLL_PER_OFFSET},
	  .div_m7	= {.addr = OMAP4_CM_DIV_M7_DPLL_PER_OFFSET},
	},
};

static inline void omap4_dpll_store_reg(struct omap4_dpll_regs *dpll_reg,
					struct dpll_reg_tuple *tuple)
{
	if (tuple->addr)
		tuple->val =
		    omap4_cminst_read_inst_reg(dpll_reg->mod_partition,
					       dpll_reg->mod_inst, tuple->addr);
}

void omap4_dpll_prepare_off(void)
{
	u32 i;
	struct omap4_dpll_regs *dpll_reg = dpll_regs;

	for (i = 0; i < ARRAY_SIZE(dpll_regs); i++, dpll_reg++) {
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->clkmode);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->autoidle);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->clksel);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->div_m2);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->div_m3);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->div_m4);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->div_m5);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->div_m6);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->div_m7);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->clkdcoldo);
		omap4_dpll_store_reg(dpll_reg, &dpll_reg->idlest);
	}
}

static void omap4_dpll_print_reg(struct omap4_dpll_regs *dpll_reg, char *name,
				 struct dpll_reg_tuple *tuple)
{
	if (tuple->addr)
		pr_warn("%s - Address offset = 0x%08x, value=0x%08x\n", name,
			tuple->addr, tuple->val);
}

static void omap4_dpll_dump_regs(struct omap4_dpll_regs *dpll_reg)
{
	pr_warn("%s: Unable to lock dpll %s[part=%x inst=%x]:\n",
		__func__, dpll_reg->name, dpll_reg->mod_partition,
		dpll_reg->mod_inst);
	omap4_dpll_print_reg(dpll_reg, "clksel", &dpll_reg->clksel);
	omap4_dpll_print_reg(dpll_reg, "div_m2", &dpll_reg->div_m2);
	omap4_dpll_print_reg(dpll_reg, "div_m3", &dpll_reg->div_m3);
	omap4_dpll_print_reg(dpll_reg, "div_m4", &dpll_reg->div_m4);
	omap4_dpll_print_reg(dpll_reg, "div_m5", &dpll_reg->div_m5);
	omap4_dpll_print_reg(dpll_reg, "div_m6", &dpll_reg->div_m6);
	omap4_dpll_print_reg(dpll_reg, "div_m7", &dpll_reg->div_m7);
	omap4_dpll_print_reg(dpll_reg, "clkdcoldo", &dpll_reg->clkdcoldo);
	omap4_dpll_print_reg(dpll_reg, "clkmode", &dpll_reg->clkmode);
	omap4_dpll_print_reg(dpll_reg, "autoidle", &dpll_reg->autoidle);
	if (dpll_reg->idlest.addr)
		pr_warn("idlest - Address offset = 0x%08x, before val=0x%08x"
			" after = 0x%08x\n", dpll_reg->idlest.addr,
			dpll_reg->idlest.val,
			omap4_cminst_read_inst_reg(dpll_reg->mod_partition,
						   dpll_reg->mod_inst,
						   dpll_reg->idlest.addr));
}

static void omap4_wait_dpll_lock(struct omap4_dpll_regs *dpll_reg)
{
	int j = 0;

	/* Return if we dont need to lock. */
	if ((dpll_reg->clkmode.val & OMAP4430_DPLL_EN_MASK) !=
	     DPLL_LOCKED << OMAP4430_DPLL_EN_SHIFT);
		return;

	while ((omap4_cminst_read_inst_reg(dpll_reg->mod_partition,
					    dpll_reg->mod_inst,
					    dpll_reg->idlest.addr)
		 & OMAP4430_ST_DPLL_CLK_MASK) !=
		 0x1 << OMAP4430_ST_DPLL_CLK_SHIFT
	       && j < MAX_DPLL_WAIT_TRIES) {
		j++;
		udelay(1);
	}

	/* if we are unable to lock, warn and move on.. */
	if (j == MAX_DPLL_WAIT_TRIES)
		omap4_dpll_dump_regs(dpll_reg);
}

static inline void omap4_dpll_restore_reg(struct omap4_dpll_regs *dpll_reg,
					  struct dpll_reg_tuple *tuple)
{
	if (tuple->addr)
		omap4_cminst_write_inst_reg(tuple->val, dpll_reg->mod_partition,
					    dpll_reg->mod_inst, tuple->addr);
}

void omap4_dpll_resume_off(void)
{
	u32 i;
	struct omap4_dpll_regs *dpll_reg = dpll_regs;

	for (i = 0; i < ARRAY_SIZE(dpll_regs); i++, dpll_reg++) {
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->clksel);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->div_m2);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->div_m3);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->div_m4);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->div_m5);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->div_m6);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->div_m7);
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->clkdcoldo);

		/* Restore clkmode after the above registers are restored */
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->clkmode);

		omap4_wait_dpll_lock(dpll_reg);

		/* Restore autoidle settings after the dpll is locked */
		omap4_dpll_restore_reg(dpll_reg, &dpll_reg->autoidle);
	}
}

/*
 * PRCM bug WA: i723 errata. DPLL_ABE must be reconfigured
 * before clocks initialize when boot by warm reset
 */
void omap4_dpll_abe_reconfigure(void)
{
	u32 reset_state, warm_state;
	int i = 0;

	/* Read the last reset state */
	reset_state = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
						OMAP4430_PRM_DEVICE_INST,
						OMAP4_PRM_RSTST_OFFSET);

	/* Global Warm Reset is already cared to WA before warm resetting */
	warm_state = OMAP4430_MPU_WDT_RST_MASK |
			OMAP4430_EXTERNAL_WARM_RST_MASK |
			OMAP4430_GLOBAL_WARM_SW_RST_MASK;

	/* don't care WA when not warm reset */
	if (!(reset_state & warm_state))
		return;

	/* Support only sys_clk is 38.4 MHz */
	if (omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
					OMAP4430_PRM_CKGEN_INST,
					OMAP4_CM_SYS_CLKSEL_OFFSET) != 0x7)
		return;

	/* Reconfigure DPLL_MULT bits of CM_CLKSEL_DPLL_ABE */
	omap4_cminst_rmw_inst_reg_bits(OMAP4430_DPLL_MULT_MASK,
					0x2ee << OMAP4430_DPLL_MULT_SHIFT,
					OMAP4430_CM1_PARTITION,
					OMAP4430_CM1_CKGEN_INST,
					OMAP4_CM_CLKSEL_DPLL_ABE_OFFSET);

	/* Lock the ABE DPLL */
	omap4_cminst_rmw_inst_reg_bits(OMAP4430_DPLL_EN_MASK,
					OMAP4430_DPLL_EN_MASK,
					OMAP4430_CM1_PARTITION,
					OMAP4430_CM1_CKGEN_INST,
					OMAP4_CM_CLKMODE_DPLL_ABE_OFFSET);

	/* Wait until DPLL is locked */
	while (((omap4_cminst_read_inst_reg(OMAP4430_CM1_PARTITION,
					OMAP4430_CM1_CKGEN_INST,
					OMAP4_CM_IDLEST_DPLL_ABE_OFFSET) &
					OMAP4430_ST_DPLL_CLK_MASK) != 0x1) &&
						(i < MAX_DPLL_WAIT_TRIES)) {
		i++;
		udelay(1);
	}

	if (i >= MAX_DPLL_WAIT_TRIES)
		pr_err("Warm Reset WA: failed to lock the ABE DPLL\n");
	else
		pr_info("Warm Reset WA: succeeded to reconfigure the ABE DPLL\n");
}

#ifdef CONFIG_OMAP4_DPLL_CASCADING
int omap4_dpll_cascading_blocker_hold(struct device *dev)
{
	struct dpll_cascading_blocker *blocker;
	int list_was_empty = 0;
	int ret = 0;

#ifdef CONFIG_OMAP4_ONLY_OMAP4430_DPLL_CASCADING
	if (!cpu_is_omap443x())
		return ret;
#endif
	if (!dev)
		return -EINVAL;

	/*
	 * We need take a console lock due to change uart frequency.
	 * We cannot do it under omap_dvfs_lock because it can lead to
	 * deadlock in situation when some thread has taken the console
	 * lock and trying to scale waiting for the omap_dvfs_lock that has
	 * been taken already by dpll_cascading.
	 */
	console_lock();
	mutex_lock(&omap_dvfs_lock);

	if (list_empty(&dpll_cascading_blocker_list))
		list_was_empty = 1;

	/* bail early if constraint for this device already exists */
	list_for_each_entry(blocker, &dpll_cascading_blocker_list, node) {
		if (blocker->dev == dev)
			goto out;
	}

	/* add new member to list of devices blocking dpll cascading entry */
	blocker = kzalloc(sizeof(struct dpll_cascading_blocker), GFP_KERNEL);
	if (!blocker) {
		pr_err("%s: Unable to create a new blocker\n",
				__func__);
		ret = -ENOMEM;
		goto out;
	}
	blocker->dev = dev;

	list_add(&blocker->node, &dpll_cascading_blocker_list);

	if (list_was_empty && omap4_is_in_dpll_cascading() &&
		omap4_abe_can_enter_dpll_cascading()) {

		/* exit point of DPLL cascading */
		ret = omap4_dpll_low_power_cascade_exit();
		cpufreq_interactive_set_timer_rate(200 * USEC_PER_MSEC, 1);
	}
out:
	mutex_unlock(&omap_dvfs_lock);
	console_unlock();
	return ret;
}
EXPORT_SYMBOL(omap4_dpll_cascading_blocker_hold);

int omap4_dpll_cascading_blocker_release(struct device *dev)
{
	struct dpll_cascading_blocker *blocker;
	int ret = 0;
	int found = 0;

#ifdef CONFIG_OMAP4_ONLY_OMAP4430_DPLL_CASCADING
	if (!cpu_is_omap443x())
		return ret;
#endif
	if (!dev)
		return -EINVAL;

	/*
	 * We need take a console lock due to change uart frequency.
	 * We cannot do it under omap_dvfs_lock because it can lead to
	 * deadlock in situation when some thread has taken the console
	 * lock and trying to scale waiting for the omap_dvfs_lock that has
	 * been taken already by dpll_cascading.
	 */
	console_lock();
	mutex_lock(&omap_dvfs_lock);

	/* bail early if list is empty */
	if (list_empty(&dpll_cascading_blocker_list))
		goto out;

	/* find the list entry that matches the device */
	list_for_each_entry(blocker, &dpll_cascading_blocker_list, node) {
		if (blocker->dev == dev) {
			found = 1;
			break;
		}
	}

	/* bail if constraint for this device does not exist */
	if (!found) {
		ret = -EINVAL;
		goto out;
	}

	list_del(&blocker->node);

	if (list_empty(&dpll_cascading_blocker_list)
		&& !omap4_is_in_dpll_cascading()
		&& omap4_abe_can_enter_dpll_cascading()) {

		cpufreq_interactive_set_timer_rate(200 * USEC_PER_MSEC, 0);
		/* enter point of DPLL cascading */
		ret = omap4_dpll_low_power_cascade_enter();
		if (ret)
			cpufreq_interactive_set_timer_rate(
						200 * USEC_PER_MSEC, 1);
	}
out:
	mutex_unlock(&omap_dvfs_lock);
	console_unlock();
	return ret;
}
EXPORT_SYMBOL(omap4_dpll_cascading_blocker_release);

bool omap4_is_in_dpll_cascading(void)
{
	return atomic_read(&in_dpll_cascading);
}

#else
int omap4_dpll_cascading_blocker_hold(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(omap4_dpll_cascading_blocker_hold);

int omap4_dpll_cascading_blocker_release(struct device *dev)
{
	return 0;
}
EXPORT_SYMBOL(omap4_dpll_cascading_blocker_release);
#endif
