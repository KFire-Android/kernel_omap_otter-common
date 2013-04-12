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

#include "soc.h"
#include "clock.h"
#include "clock44xx.h"
#include "cm-regbits-44xx.h"
#include "cm-regbits-54xx.h"
#include "cm1_54xx.h"
#include "iomap.h"

/*
 * Maximum DPLL input frequency (FINT) and output frequency (FOUT) that
 * can supported when using the DPLL low-power mode. Frequencies are
 * defined in OMAP4430/60 Public TRM section 3.6.3.3.2 "Enable Control,
 * Status, and Low-Power Operation Mode".
 */
#define OMAP4_DPLL_LP_FINT_MAX	1000000
#define OMAP4_DPLL_LP_FOUT_MAX	100000000

#define OMAP_1GHz			1000000000

/* Supported only on OMAP4 */
int omap4_dpllmx_gatectrl_read(struct clk_hw_omap *clk)
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

void omap4_dpllmx_allow_gatectrl(struct clk_hw_omap *clk)
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

void omap4_dpllmx_deny_gatectrl(struct clk_hw_omap *clk)
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

const struct clk_hw_omap_ops clkhwops_omap4_dpllmx = {
	.allow_idle	= omap4_dpllmx_allow_gatectrl,
	.deny_idle      = omap4_dpllmx_deny_gatectrl,
};

/**
 * omap4_dpll_lpmode_recalc - compute DPLL low-power setting
 * @dd: pointer to the dpll data structure
 *
 * Calculates if low-power mode can be enabled based upon the last
 * multiplier and divider values calculated. If low-power mode can be
 * enabled, then the bit to enable low-power mode is stored in the
 * last_rounded_lpmode variable. This implementation is based upon the
 * criteria for enabling low-power mode as described in the OMAP4430/60
 * Public TRM section 3.6.3.3.2 "Enable Control, Status, and Low-Power
 * Operation Mode".
 */
static void omap4_dpll_lpmode_recalc(struct dpll_data *dd)
{
	long fint, fout;

	fint = __clk_get_rate(dd->clk_ref) / (dd->last_rounded_n + 1);
	fout = fint * dd->last_rounded_m;

	if ((fint < OMAP4_DPLL_LP_FINT_MAX) && (fout < OMAP4_DPLL_LP_FOUT_MAX))
		dd->last_rounded_lpmode = 1;
	else
		dd->last_rounded_lpmode = 0;
}

/**
 * omap4_dpll_regm4xen_recalc - compute DPLL rate, considering REGM4XEN bit
 * @clk: struct clk * of the DPLL to compute the rate for
 *
 * Compute the output rate for the OMAP4 DPLL represented by @clk.
 * Takes the REGM4XEN bit into consideration, which is needed for the
 * OMAP4 ABE DPLL.  Returns the DPLL's output rate (before M-dividers)
 * upon success, or 0 upon error.
 */
unsigned long omap4_dpll_regm4xen_recalc(struct clk_hw *hw,
			unsigned long parent_rate)
{
	struct clk_hw_omap *clk = to_clk_hw_omap(hw);
	u32 v;
	unsigned long rate;
	struct dpll_data *dd;

	if (!clk || !clk->dpll_data)
		return 0;

	dd = clk->dpll_data;

	rate = omap2_get_dpll_rate(clk);

	/* regm4xen adds a multiplier of 4 to DPLL calculations */
	v = __raw_readl(dd->control_reg);
	if (v & OMAP4430_DPLL_REGM4XEN_MASK)
		rate *= OMAP4430_REGM4XEN_MULT;

	return rate;
}

/**
 * omap4_dpll_regm4xen_round_rate - round DPLL rate, considering REGM4XEN bit
 * @clk: struct clk * of the DPLL to round a rate for
 * @target_rate: the desired rate of the DPLL
 *
 * Compute the rate that would be programmed into the DPLL hardware
 * for @clk if set_rate() were to be provided with the rate
 * @target_rate.  Takes the REGM4XEN bit into consideration, which is
 * needed for the OMAP4 ABE DPLL.  Returns the rounded rate (before
 * M-dividers) upon success, -EINVAL if @clk is null or not a DPLL, or
 * ~0 if an error occurred in omap2_dpll_round_rate().
 */
long omap4_dpll_regm4xen_round_rate(struct clk_hw *hw,
				    unsigned long target_rate,
				    unsigned long *parent_rate)
{
	struct clk_hw_omap *clk = to_clk_hw_omap(hw);
	struct dpll_data *dd;
	long r;

	if (!clk || !clk->dpll_data)
		return -EINVAL;

	dd = clk->dpll_data;

	dd->last_rounded_m4xen = 0;

	/*
	 * First try to compute the DPLL configuration for
	 * target rate without using the 4X multiplier.
	 */
	r = omap2_dpll_round_rate(hw, target_rate, NULL);
	if (r != ~0)
		goto out;

	/*
	 * If we did not find a valid DPLL configuration, try again, but
	 * this time see if using the 4X multiplier can help. Enabling the
	 * 4X multiplier is equivalent to dividing the target rate by 4.
	 */
	r = omap2_dpll_round_rate(hw, target_rate / OMAP4430_REGM4XEN_MULT,
				  NULL);
	if (r == ~0)
		return r;

	dd->last_rounded_rate *= OMAP4430_REGM4XEN_MULT;
	dd->last_rounded_m4xen = 1;

out:
	omap4_dpll_lpmode_recalc(dd);

	return dd->last_rounded_rate;
}

static void omap5_mpu_dpll_update_children(unsigned long rate)
{
	u32 v;

	/*
	 * The interconnect frequency to EMIF should
	 * be switched between MPU clk divide by 8 (for
	 * frequencies higher than 1000Mhz i.e OPP) and MPU clk divide
	 * by 4 (for frequencies lower than or equal to 1000Mhz)
	 * Also the async bridge to ABE must be MPU clk divide
	 * by 16 for MPU clk > 1000Mhz and MPU clk divide by 8
	 * for lower frequencies. See note under chapter 2.1
	 * "Micro Processor Unit (MPU)" of
	 * "OMAP543x ES2.0 Operating Conditions Addendum v0.5".
	 */
	v = __raw_readl(OMAP54XX_CM_MPU_MPU_CLKCTRL);
	if (rate > OMAP_1GHz)
		v |= OMAP54XX_CLKSEL_EMIF_DIV_MODE_MASK;
	else
		v &= ~OMAP54XX_CLKSEL_EMIF_DIV_MODE_MASK;

	if (rate > OMAP_1GHz)
		v |= OMAP54XX_CLKSEL_ABE_DIV_MODE_MASK;
	else
		v &= ~OMAP54XX_CLKSEL_ABE_DIV_MODE_MASK;
	__raw_writel(v, OMAP54XX_CM_MPU_MPU_CLKCTRL);
}

/**
 * omap5_mpu_dpll_set_rate - set non-core DPLL rate
 * @clk: struct clk * of DPLL to set
 * @rate: rounded target rate
 *
 * Returns -EINVAL upon error, or 0 upon success.
 */
int omap5_mpu_dpll_set_rate(struct clk_hw *hw, unsigned long rate,
					unsigned long parent_rate)
{
	int ret = 0, clk_rate;

	clk_rate = __clk_get_rate(hw->clk);

	if (rate > clk_rate)
		omap5_mpu_dpll_update_children(rate);

	if (rate != clk_rate)
		ret = omap3_noncore_dpll_set_rate(hw, rate, parent_rate);

	if (ret)
		return ret;

	if (rate < clk_rate)
			omap5_mpu_dpll_update_children(rate);

	return ret;
}
