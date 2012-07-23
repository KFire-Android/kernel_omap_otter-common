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
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>

#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/common.h>

#include "clock.h"
#include "clock44xx.h"
#include "cm-regbits-44xx.h"
#include "cm-regbits-54xx.h"
#include "cm.h"
#include "clockdomain.h"
#include "cm1_44xx.h"
#include "cm1_54xx.h"
#include "iomap.h"
#include "common.h"
#include "cm2_44xx.h"
#include "prcm44xx.h"
#include "cminst44xx.h"
#include "cm44xx.h"

#define MAX_DPLL_WAIT_TRIES	1000000
#define MAX_FREQ_UPDATE_TIMEOUT  100000
#define OMAP_1_4GHz	1400000000
#define OMAP_1GHz		1000000000
#define OMAP_920MHz		920000000
#define OMAP_748MHz		748000000

static struct clockdomain *l3_emif_clkdm;
static struct srcu_notifier_head core_dpll_change_notify_list;


struct dpll_reg {
	u16 offset;
	u32 val;
};

struct omap4_dpll_regs {
	char *name;
	u32 mod_partition;
	u32 mod_inst;
	struct dpll_reg clkmode;
	struct dpll_reg autoidle;
	struct dpll_reg idlest;
	struct dpll_reg clksel;
	struct dpll_reg div_m2;
	struct dpll_reg div_m3;
	struct dpll_reg div_m4;
	struct dpll_reg div_m5;
	struct dpll_reg div_m6;
	struct dpll_reg div_m7;
	struct dpll_reg clkdcoldo;
};

/* MPU DPLL */
static struct omap4_dpll_regs omap4_omap5_dpll_regs_mpu = {
	.name		= "mpu",
	.mod_partition	= OMAP4430_CM1_PARTITION,
	.mod_inst	= OMAP4430_CM1_CKGEN_INST,
	.clkmode	= {.offset = OMAP4_CM_CLKMODE_DPLL_MPU_OFFSET},
	.autoidle	= {.offset = OMAP4_CM_AUTOIDLE_DPLL_MPU_OFFSET},
	.idlest		= {.offset = OMAP4_CM_IDLEST_DPLL_MPU_OFFSET},
	.clksel		= {.offset = OMAP4_CM_CLKSEL_DPLL_MPU_OFFSET},
	.div_m2		= {.offset = OMAP4_CM_DIV_M2_DPLL_MPU_OFFSET},
};

/* IVA DPLL */
static struct omap4_dpll_regs omap4_omap5_dpll_regs_iva = {
	.name		= "iva",
	.mod_partition	= OMAP4430_CM1_PARTITION,
	.mod_inst	= OMAP4430_CM1_CKGEN_INST,
	.clkmode	= {.offset = OMAP4_CM_CLKMODE_DPLL_IVA_OFFSET},
	.autoidle	= {.offset = OMAP4_CM_AUTOIDLE_DPLL_IVA_OFFSET},
	.idlest		= {.offset = OMAP4_CM_IDLEST_DPLL_IVA_OFFSET},
	.clksel		= {.offset = OMAP4_CM_CLKSEL_DPLL_IVA_OFFSET},
	.div_m4		= {.offset = OMAP4_CM_DIV_M4_DPLL_IVA_OFFSET},
	.div_m5		= {.offset = OMAP4_CM_DIV_M5_DPLL_IVA_OFFSET},
};

/* ABE DPLL */
static struct omap4_dpll_regs omap4_omap5_dpll_regs_abe = {
	.name		= "abe",
	.mod_partition	= OMAP4430_CM1_PARTITION,
	.mod_inst	= OMAP4430_CM1_CKGEN_INST,
	.clkmode	= {.offset = OMAP4_CM_CLKMODE_DPLL_ABE_OFFSET},
	.autoidle	= {.offset = OMAP4_CM_AUTOIDLE_DPLL_ABE_OFFSET},
	.idlest		= {.offset = OMAP4_CM_IDLEST_DPLL_ABE_OFFSET},
	.clksel		= {.offset = OMAP4_CM_CLKSEL_DPLL_ABE_OFFSET},
	.div_m2		= {.offset = OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET},
	.div_m3		= {.offset = OMAP4_CM_DIV_M3_DPLL_ABE_OFFSET},
};

/* USB DPLL */
static struct omap4_dpll_regs omap4_omap5_dpll_regs_usb = {
	.name		= "usb",
	.mod_partition	= OMAP4430_CM2_PARTITION,
	.mod_inst	= OMAP4430_CM2_CKGEN_INST,
	.clkmode	= {.offset = OMAP4_CM_CLKMODE_DPLL_USB_OFFSET},
	.autoidle	= {.offset = OMAP4_CM_AUTOIDLE_DPLL_USB_OFFSET},
	.idlest		= {.offset = OMAP4_CM_IDLEST_DPLL_USB_OFFSET},
	.clksel		= {.offset = OMAP4_CM_CLKSEL_DPLL_USB_OFFSET},
	.div_m2		= {.offset = OMAP4_CM_DIV_M2_DPLL_USB_OFFSET},
	.clkdcoldo	= {.offset = OMAP4_CM_CLKDCOLDO_DPLL_USB_OFFSET},
};

/* PER DPLL */
static struct omap4_dpll_regs omap4_omap5_dpll_regs_per = {
	.name		= "per",
	.mod_partition	= OMAP4430_CM2_PARTITION,
	.mod_inst	= OMAP4430_CM2_CKGEN_INST,
	.clkmode	= {.offset = OMAP4_CM_CLKMODE_DPLL_PER_OFFSET},
	.autoidle	= {.offset = OMAP4_CM_AUTOIDLE_DPLL_PER_OFFSET},
	.idlest		= {.offset = OMAP4_CM_IDLEST_DPLL_PER_OFFSET},
	.clksel		= {.offset = OMAP4_CM_CLKSEL_DPLL_PER_OFFSET},
	.div_m2		= {.offset = OMAP4_CM_DIV_M2_DPLL_PER_OFFSET},
	.div_m3		= {.offset = OMAP4_CM_DIV_M3_DPLL_PER_OFFSET},
	.div_m4		= {.offset = OMAP4_CM_DIV_M4_DPLL_PER_OFFSET},
	.div_m5		= {.offset = OMAP4_CM_DIV_M5_DPLL_PER_OFFSET},
	.div_m6		= {.offset = OMAP4_CM_DIV_M6_DPLL_PER_OFFSET},
	.div_m7		= {.offset = OMAP4_CM_DIV_M7_DPLL_PER_OFFSET},
};

static struct omap4_dpll_regs *dpll_regs[] = {
	&omap4_omap5_dpll_regs_mpu,
	&omap4_omap5_dpll_regs_iva,
	&omap4_omap5_dpll_regs_abe,
	&omap4_omap5_dpll_regs_usb,
	&omap4_omap5_dpll_regs_per,
	NULL,
};

static int dpll_dump_level;
#define DPLL_DUMP_LEVEL_BEFORE	(1 << 0)
#define DPLL_DUMP_LEVEL_AFTER	(1 << 1)
#define DPLL_DUMP_LEVEL_RESTORE	(1 << 2)

/* Enable control only if PM advanced debug is active in build */
#ifdef CONFIG_PM_ADVANCED_DEBUG
module_param(dpll_dump_level, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dpll_dump_level,
		 "DUMP dpll: (1)before,(2)after suspend,(4)around restore");
#endif

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
	struct omap_dpll_notifier notify;

	if (!clk || !rate)
		return -EINVAL;

	validrate = omap2_clksel_round_rate_div(clk, rate, &new_div);
	if (validrate != rate)
		return -EINVAL;

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm) {
		if (cpu_is_omap44xx())
			l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
		else
			l3_emif_clkdm = clkdm_lookup("emif_clkdm");
		if (!l3_emif_clkdm) {
			pr_err("%s: clockdomain lookup failed\n", __func__);
			return -EINVAL;
		}
	}

	/* Configures MEMIF domain in SW_WKUP */
	clkdm_wakeup(l3_emif_clkdm);

	/*
	* Program EMIF timing parameters in EMIF shadow registers
	* for targeted DRR clock.
	*/
	/* DDR frequency is half of M2 output in OMAP4 */
	if (cpu_is_omap44xx())
		/* DDR Clock = core_dpll_m2 / 2 */
		notify.rate = validrate >> 1;
	else
		notify.rate = validrate;
	srcu_notifier_call_chain(&core_dpll_change_notify_list,
				 OMAP_CORE_DPLL_PRECHANGE, (void *)&notify);

	/*
	 * FREQ_UPDATE sequence:
	 * - DLL_OVERRIDE=0 (DLL lock & code must not be overridden
	 *	after CORE DPLL lock)
	 * - DLL_RESET=0 (DLL must NOT be reset upon frequency change)
	 * - DPLL_CORE_M2_DIV with same value as the one already
	 *	in direct register
	 * - DPLL_CORE_DPLL_EN=0x7 (to make CORE DPLL lock)
	 * - FREQ_UPDATE=1 (to start HW sequence)
	 */
	shadow_freq_cfg1 = (0 << OMAP4430_DLL_RESET_SHIFT) |
			(new_div << OMAP4430_DPLL_CORE_M2_DIV_SHIFT) |
			(DPLL_LOCKED << OMAP4430_DPLL_CORE_DPLL_EN_SHIFT) |
			(1 << OMAP4430_FREQ_UPDATE_SHIFT);
	shadow_freq_cfg1 &= ~OMAP4430_DLL_OVERRIDE_2_2_MASK;
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
				& OMAP4430_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	srcu_notifier_call_chain(&core_dpll_change_notify_list,
				 OMAP_CORE_DPLL_POSTCHANGE, (void *)&notify);

	/* Configures MEMIF domain back to HW_WKUP */
	clkdm_allow_idle(l3_emif_clkdm);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
		       __func__);
		return -ETIMEDOUT;
	}

	/* Update the clock change */
	clk->rate = validrate;

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
int omap4_prcm_freq_update(void)
{
	u32 shadow_freq_cfg1;
	int i = 0;

	if (!l3_emif_clkdm) {
		pr_err("%s: clockdomain lookup failed\n", __func__);
		return -EINVAL;
	}

	/* Configures MEMIF domain in SW_WKUP */
	clkdm_wakeup(l3_emif_clkdm);

	/*
	 * FREQ_UPDATE sequence:
	 * - DLL_OVERRIDE=0 (DLL lock & code must not be overridden
	 *	after CORE DPLL lock)
	 * - FREQ_UPDATE=1 (to start HW sequence)
	 */
	shadow_freq_cfg1 = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1);
	/* Be explicit in NOT setting DLL reset */
	shadow_freq_cfg1 &= ~(1 << OMAP4430_DLL_RESET_SHIFT);
	shadow_freq_cfg1 |= (1 << OMAP4430_FREQ_UPDATE_SHIFT);
	shadow_freq_cfg1 &= ~OMAP4430_DLL_OVERRIDE_2_2_MASK;
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/* wait for the configuration to be applied */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
				& OMAP4430_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Configures MEMIF domain back to HW_WKUP */
	clkdm_allow_idle(l3_emif_clkdm);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update failed\n",	__func__);
		return -ETIMEDOUT;
	}

	return 0;
}

/**
 * omap4_core_dpll_m5_set_rate - set CORE DPLL M5 divider
 * @clk: struct clk * of DPLL to set
 * @rate: rounded target rate
 *
 * Programs the CM shadow registers to update CORE DPLL M5
 * divider. M5 divider is used to clock l3 and GPMC. GPMC
 * reconfiguration on frequency change is managed through a
 * hardware sequencer using shadow registers.
 * Returns -EINVAL/-1 on error and 0 on success.
 */
int omap4_core_dpll_m5x2_set_rate(struct clk *clk, unsigned long rate)
{
	int i = 0;
	u32 validrate = 0, shadow_freq_cfg2 = 0, shadow_freq_cfg1, new_div = 0;

	if (!clk || !rate)
		return -EINVAL;

	/* Just to avoid look-up on every call to speed up */
	if (!l3_emif_clkdm) {
		if (cpu_is_omap44xx())
			l3_emif_clkdm = clkdm_lookup("l3_emif_clkdm");
		else
			l3_emif_clkdm = clkdm_lookup("emif_clkdm");
		if (!l3_emif_clkdm) {
			pr_err("%s: clockdomain lookup failed\n", __func__);
			return -EINVAL;
		}
	}

	/* Configures MEMIF domain in SW_WKUP */
	clkdm_wakeup(l3_emif_clkdm);

	validrate = omap2_clksel_round_rate_div(clk, rate, &new_div);
	if (validrate != rate)
		return -EINVAL;

	/*
	 * FREQ_UPDATE sequence:
	 * - DPLL_CORE_M5_DIV with new value of M5 post-divider on
	 *   L3 clock generation path
	 * - CLKSEL_L3=1 (unchanged)
	 * - CLKSEL_CORE=0 (unchanged)
	 * - GPMC_FREQ_UPDATE=1
	 */

	shadow_freq_cfg2 = (new_div << OMAP4430_DPLL_CORE_M5_DIV_SHIFT) |
				(1 << OMAP4430_CLKSEL_L3_SHADOW_SHIFT) |
				(1 << OMAP4430_FREQ_UPDATE_SHIFT);

	__raw_writel(shadow_freq_cfg2, OMAP4430_CM_SHADOW_FREQ_CONFIG2);

	/* Write to FREQ_UPDATE of SHADOW_FREQ_CONFIG1 to trigger transition */
	shadow_freq_cfg1 = __raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1);
	shadow_freq_cfg1 |= (1 << OMAP4430_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg1, OMAP4430_CM_SHADOW_FREQ_CONFIG1);

	/*
	 * wait for the configuration to be applied by Polling
	 * FREQ_UPDATE of SHADOW_FREQ_CONFIG1
	 */
	omap_test_timeout(((__raw_readl(OMAP4430_CM_SHADOW_FREQ_CONFIG1)
				& OMAP4430_GPMC_FREQ_UPDATE_MASK) == 0),
				MAX_FREQ_UPDATE_TIMEOUT, i);

	/* Configures MEMIF domain back to HW_WKUP */
	clkdm_allow_idle(l3_emif_clkdm);

	/* Disable GPMC FREQ_UPDATE */
	shadow_freq_cfg2 &= ~(1 << OMAP4430_FREQ_UPDATE_SHIFT);
	__raw_writel(shadow_freq_cfg2, OMAP4430_CM_SHADOW_FREQ_CONFIG2);

	if (i == MAX_FREQ_UPDATE_TIMEOUT) {
		pr_err("%s: Frequency update for CORE DPLL M2 change failed\n",
		       __func__);
		return -ETIMEDOUT;
	}

	/* Update the clock change */
	clk->rate = validrate;

	return 0;
}

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

	dpll_rate = omap2_get_dpll_rate(clk->parent);

	if (rate != dpll_rate)
		clk->parent->set_rate(clk->parent, rate);

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

	if (!clk || !clk->parent)
		return -EINVAL;

	dd = clk->parent->dpll_data;

	if (!dd)
		return -EINVAL;

	return omap2_get_dpll_rate(clk->parent);
}

static void omap5_mpu_dpll_update_children(unsigned long rate)
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

int omap5_mpu_dpll_set_rate(struct clk *clk, unsigned long rate)
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

	/*
	 * On OMAP5430, to obtain MPU DPLL frequency higher
	 * than 1.4GHz, DCC (Duty Cycle Correction) needs to
	 * be enabled.
	 * And needs to be kept disabled for <= 1.4 Ghz.
	 */
	dpll_rate = omap2_get_dpll_rate(clk->parent);
	v = __raw_readl(dd->mult_div1_reg);
	if (rate <= OMAP_1_4GHz) {
		if (rate == dpll_rate)
			return 0;
		/* If DCC is enabled, disable it */
		if (v & OMAP54XX_DCC_EN_MASK) {
			v &= ~OMAP54XX_DCC_EN_MASK;
			__raw_writel(v, dd->mult_div1_reg);
		}
		clk->parent->set_rate(clk->parent, rate);
	} else {
		/*
		 * On OMAP54530, the MPU clk for frequencies higher than 1.4Ghz
		 * is sourced from CLKOUTX2_M3, instead of CLKOUT_M2, while
		 * value of M3 is fixed to 1. Hence for frequencies higher
		 * than 1 Ghz, lock the DPLL at half the rate so the
		 * CLKOUTX2_M3 then matches the requested rate.
		 */
		if (rate == dpll_rate/2)
			return 0;
		v |= OMAP54XX_DCC_EN_MASK;
		__raw_writel(v, dd->mult_div1_reg);
		clk->parent->set_rate(clk->parent, rate/2);
	}

	if (rate != clk->rate)
		omap5_mpu_dpll_update_children(rate);
	clk->rate = rate;

	return 0;
}

long omap5_mpu_dpll_round_rate(struct clk *clk, unsigned long rate)
{
	if (!clk || !rate || !clk->parent)
		return -EINVAL;

	if (clk->parent->round_rate)
		return clk->parent->round_rate(clk->parent, rate);
	else
		return 0;
}

unsigned long omap5_mpu_dpll_recalc(struct clk *clk)
{
	struct dpll_data *dd;
	u32 v;

	if (!clk || !clk->parent)
		return -EINVAL;

	dd = clk->parent->dpll_data;

	if (!dd)
		return -EINVAL;

	v = __raw_readl(dd->mult_div1_reg);
	if (v & OMAP54XX_DCC_EN_MASK)
		return omap2_get_dpll_rate(clk->parent) * 2;
	else
		return omap2_get_dpll_rate(clk->parent);
}

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

/**
 * omap4_dpll_regm4xen_recalc - compute DPLL rate, considering REGM4XEN bit
 * @clk: struct clk * of the DPLL to compute the rate for
 *
 * Compute the output rate for the OMAP4 DPLL represented by @clk.
 * Takes the REGM4XEN bit into consideration, which is needed for the
 * OMAP4 ABE DPLL.  Returns the DPLL's output rate (before M-dividers)
 * upon success, or 0 upon error.
 */
unsigned long omap4_dpll_regm4xen_recalc(struct clk *clk)
{
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
long omap4_dpll_regm4xen_round_rate(struct clk *clk, unsigned long target_rate)
{
	u32 v;
	struct dpll_data *dd;
	long r;

	if (!clk || !clk->dpll_data)
		return -EINVAL;

	dd = clk->dpll_data;

	/* regm4xen adds a multiplier of 4 to DPLL calculations */
	v = __raw_readl(dd->control_reg) & OMAP4430_DPLL_REGM4XEN_MASK;

	if (v)
		target_rate = target_rate / OMAP4430_REGM4XEN_MULT;

	r = omap2_dpll_round_rate(clk, target_rate);
	if (r == ~0)
		return r;

	if (v)
		clk->dpll_data->last_rounded_rate *= OMAP4430_REGM4XEN_MULT;

	return clk->dpll_data->last_rounded_rate;
}

/**
 * omap4_dpll_read_reg - reads DPLL register value
 * @dpll_reg: DPLL register to read
 *
 * Reads the value of a single DPLL register.
 */
static inline u32 omap4_dpll_read_reg(struct omap4_dpll_regs *dpll_reg,
				      struct dpll_reg *tuple)
{
	if (tuple->offset)
		return omap4_cminst_read_inst_reg(dpll_reg->mod_partition,
						  dpll_reg->mod_inst,
						  tuple->offset);
	return 0;
}

/**
 * omap4_dpll_store_reg - stores DPLL register value to memory location
 * @dpll_reg: DPLL register to save
 * @tuple: save address
 *
 * Saves a single DPLL register content to memory location defined by
 * @tuple before entering device off mode.
 */
static inline void omap4_dpll_store_reg(struct omap4_dpll_regs *dpll_reg,
					struct dpll_reg *tuple)
{
	tuple->val = omap4_dpll_read_reg(dpll_reg, tuple);
}

/**
 * omap4_dpll_print_reg - dump out a single DPLL register value
 * @dpll_reg: register to dump
 * @name: name of the register
 * @tuple: content of the register
 *
 * Helper dump function to print out a DPLL register value in case
 * of restore failures.
 */
static void omap4_dpll_print_reg(struct omap4_dpll_regs *dpll_reg, char *name,
				 struct dpll_reg *tuple)
{
	if (tuple->offset)
		pr_warn("%s - offset = 0x%04x, value = 0x%08x\n", name,
			tuple->offset, tuple->val);
}

/*
 * omap4_dpll_dump_regs - dump out DPLL registers
 * @dpll_reg: DPLL to dump
 *
 * Dump out the contents of the registers for a DPLL. Called if a
 * restore for DPLL fails to lock.
 */
static void omap4_dpll_dump_regs(struct omap4_dpll_regs *dpll_reg)
{
	pr_warn("DPLL: Dump of dpll %s[part=%x inst=%x]:\n",
		dpll_reg->name, dpll_reg->mod_partition,
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
	if (dpll_reg->idlest.offset)
		pr_warn("idlest[0x%04x]: val: before=0x%08x after=0x%08x\n",
			dpll_reg->idlest.offset,
			dpll_reg->idlest.val,
			omap4_dpll_read_reg(dpll_reg, &dpll_reg->idlest));
}

/**
 * omap4_dpll_prepare_off - stores DPLL settings before off mode
 *
 * Saves all DPLL register settings. This must be called before
 * entering device off.
 */
void omap4_dpll_prepare_off(void)
{
	struct omap4_dpll_regs **dpll_regs_list = dpll_regs;

	if (dpll_dump_level & DPLL_DUMP_LEVEL_BEFORE)
		pr_warn("========= Before suspending(Begin) ==============\n");

	while (NULL != *dpll_regs_list) {
		struct omap4_dpll_regs *dpll_reg = *dpll_regs_list;

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

		if (dpll_dump_level & DPLL_DUMP_LEVEL_BEFORE)
			omap4_dpll_dump_regs(dpll_reg);

		dpll_regs_list++;
	}

	if (dpll_dump_level & DPLL_DUMP_LEVEL_BEFORE)
		pr_warn("========= Before suspending(End) ==============\n");
}

/**
 * omap4_wait_dpll_lock - wait for a DPLL lock
 * @dpll_reg: DPLL to wait for
 *
 * Waits for a DPLL lock after restore.
 */
static void omap4_wait_dpll_lock(struct omap4_dpll_regs *dpll_reg)
{
	int j = 0;
	u32 status;

	/* Return if we dont need to lock. */
	if ((dpll_reg->clkmode.val & OMAP4430_DPLL_EN_MASK) !=
	     DPLL_LOCKED << OMAP4430_DPLL_EN_SHIFT)
		return;

	while (1) {
		status = (omap4_dpll_read_reg(dpll_reg, &dpll_reg->idlest)
			  & OMAP4430_ST_DPLL_CLK_MASK)
			 >> OMAP4430_ST_DPLL_CLK_SHIFT;
		if (status == 0x1)
			break;
		if (j == MAX_DPLL_WAIT_TRIES) {
			/* If we are unable to lock, warn and move on.. */
			pr_warn("%s: Unable to lock dpll %s[part=%x inst=%x]:\n",
				__func__, dpll_reg->name,
				dpll_reg->mod_partition, dpll_reg->mod_inst);
			omap4_dpll_dump_regs(dpll_reg);
			break;
		}
		j++;
		udelay(1);
	}
}

/**
 * omap4_dpll_restore_reg - restores a single register for a DPLL
 * @dpll_reg: DPLL to restore
 * @tuple: register value to restore
 *
 * Restores a single register for a DPLL.
 */
static inline void omap4_dpll_restore_reg(struct omap4_dpll_regs *dpll_reg,
					  struct dpll_reg *tuple)
{
	if (tuple->offset)
		omap4_cminst_write_inst_reg(tuple->val, dpll_reg->mod_partition,
					    dpll_reg->mod_inst, tuple->offset);
}

/**
 * omap4_dpll_resume_off - restore DPLL settings after device off
 *
 * Restores all DPLL settings. Must be called after wakeup from device
 * off.
 */
void omap4_dpll_resume_off(void)
{
	struct omap4_dpll_regs **dpll_regs_list = dpll_regs;

	if (dpll_dump_level & DPLL_DUMP_LEVEL_AFTER)
		pr_warn("========= After suspending(Begin) ==============\n");

	while (NULL != *dpll_regs_list) {
		struct omap4_dpll_regs *dpll_reg = *dpll_regs_list;

		if (dpll_dump_level & DPLL_DUMP_LEVEL_RESTORE) {
			pr_warn("DUMP before restore dpll %s:\n",
				dpll_reg->name);
			omap4_dpll_dump_regs(dpll_reg);
		}

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

		if (dpll_dump_level & DPLL_DUMP_LEVEL_RESTORE) {
			pr_warn("DUMP after restore dpll %s:\n",
				dpll_reg->name);
			omap4_dpll_dump_regs(dpll_reg);
		}

		dpll_regs_list++;
	}

	if (dpll_dump_level & DPLL_DUMP_LEVEL_AFTER)
		pr_warn("========= After suspending(End) ==============\n");
}

int omap4_core_dpll_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&core_dpll_change_notify_list, nb);
}

int omap4_core_dpll_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&core_dpll_change_notify_list,
					      nb);
}

static int __init __init_core_dpll_notifier_list(void)
{
	srcu_init_notifier_head(&core_dpll_change_notify_list);
	return 0;
}
core_initcall(__init_core_dpll_notifier_list);
