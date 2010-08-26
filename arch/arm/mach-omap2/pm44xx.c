/*
 * OMAP4 Power Management Routines
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/pm.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/irq.h>

#include <plat/powerdomain.h>
#include <plat/clockdomain.h>
#include <mach/omap4-common.h>
#include <mach/omap4-wakeupgen.h>

#include "prm.h"
#include "pm.h"
#include "cm.h"
#include "cm-regbits-44xx.h"
#include "clock.h"

struct power_state {
	struct powerdomain *pwrdm;
	u32 next_state;
#ifdef CONFIG_SUSPEND
	u32 saved_state;
#endif
	struct list_head node;
};

static LIST_HEAD(pwrst_list);
static struct powerdomain *mpu_pwrdm;

#ifdef CONFIG_SUSPEND
static int omap4_pm_prepare(void)
{
	disable_hlt();
	return 0;
}

static int omap4_pm_suspend(void)
{
	struct power_state *pwrst;
	int state;
	u32 cpu_id = 0;

	/*
	 * Wakeup timer from suspend
	 */
	if (wakeup_timer_seconds || wakeup_timer_milliseconds)
		omap2_pm_wakeup_on_timer(wakeup_timer_seconds,
					 wakeup_timer_milliseconds);
	/*
	 * Clear all wakeup sources and keep
	 * only Debug UART, Keypad and GPT1 interrupt
	 * as a wakeup event from MPU/Device OFF
	 */
	omap4_wakeupgen_clear_all(cpu_id);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_UART3);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_KBD_CTL);
	omap4_wakeupgen_set_interrupt(cpu_id, OMAP44XX_IRQ_GPT1);

	/*
	 * Program the MPU and CPU0 to hit low
	 * power state
	 */
	pwrdm_clear_all_prev_pwrst(mpu_pwrdm);
	pwrdm_set_next_pwrst(mpu_pwrdm, PWRDM_POWER_OFF);
	omap4_enter_lowpower(cpu_id, PWRDM_POWER_OFF);

	/*
	* Enable all wakeup sources post wakeup
	*/
	omap4_wakeupgen_set_all(cpu_id);

	/* Print the previous power domain states */
	pr_info("Read Powerdomain states as ...\n");
	pr_info("0 : OFF, 1 : RETENTION, 2 : ON-INACTIVE, 3 : ON-ACTIVE\n");
	list_for_each_entry(pwrst, &pwrst_list, node) {
		if (!strcmp(pwrst->pwrdm->name, "cpu0_pwrdm") ||
			(!strcmp(pwrst->pwrdm->name, "cpu1_pwrdm")) ||
			(!strcmp(pwrst->pwrdm->name, "mpu_pwrdm"))) {
			state = pwrdm_read_prev_pwrst(pwrst->pwrdm);
			pr_info("Powerdomain (%s) entered state %d\n",
				       pwrst->pwrdm->name, state);
		}
	}
	return 0;
}

static int omap4_pm_enter(suspend_state_t suspend_state)
{
	int ret = 0;

	switch (suspend_state) {
	case PM_SUSPEND_STANDBY:
	case PM_SUSPEND_MEM:
		ret = omap4_pm_suspend();
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void omap4_pm_finish(void)
{
	enable_hlt();
	return;
}

static int omap4_pm_begin(suspend_state_t state)
{
	return 0;
}

static void omap4_pm_end(void)
{
	return;
}

static struct platform_suspend_ops omap_pm_ops = {
	.begin		= omap4_pm_begin,
	.end		= omap4_pm_end,
	.prepare	= omap4_pm_prepare,
	.enter		= omap4_pm_enter,
	.finish		= omap4_pm_finish,
	.valid		= suspend_valid_only_mem,
};
#endif /* CONFIG_SUSPEND */

/*
 * Enable hw supervised mode for all clockdomains if it's
 * supported. Initiate sleep transition for other clockdomains, if
 * they are not used
 */
static int __attribute__ ((unused)) __init
		clkdms_setup(struct clockdomain *clkdm, void *unused)
{
	if (clkdm->flags & CLKDM_CAN_ENABLE_AUTO)
		omap2_clkdm_allow_idle(clkdm);
	else if (clkdm->flags & CLKDM_CAN_FORCE_SLEEP &&
			atomic_read(&clkdm->usecount) == 0)
		omap2_clkdm_sleep(clkdm);
	return 0;
}


static int __init pwrdms_setup(struct powerdomain *pwrdm, void *unused)
{
	struct power_state *pwrst;

	if (!pwrdm->pwrsts)
		return 0;

	pwrst = kmalloc(sizeof(struct power_state), GFP_ATOMIC);
	if (!pwrst)
		return -ENOMEM;
	pwrst->pwrdm = pwrdm;
	pwrst->next_state = PWRDM_POWER_ON;
	list_add(&pwrst->node, &pwrst_list);

	return pwrdm_set_next_pwrst(pwrst->pwrdm, pwrst->next_state);
}

static void __init prcm_setup_regs(void)
{
	struct clk *dpll_abe_ck, *dpll_core_ck, *dpll_iva_ck;
	struct clk *dpll_mpu_ck, *dpll_per_ck, *dpll_usb_ck;
	struct clk *dpll_unipro_ck;

	/*Enable all the DPLL autoidle */
	dpll_abe_ck = clk_get(NULL, "dpll_abe_ck");
	omap3_dpll_allow_idle(dpll_abe_ck);
	dpll_core_ck = clk_get(NULL, "dpll_core_ck");
	omap3_dpll_allow_idle(dpll_core_ck);
	dpll_iva_ck = clk_get(NULL, "dpll_iva_ck");
	omap3_dpll_allow_idle(dpll_iva_ck);
	dpll_mpu_ck = clk_get(NULL, "dpll_mpu_ck");
	omap3_dpll_allow_idle(dpll_mpu_ck);
	dpll_per_ck = clk_get(NULL, "dpll_per_ck");
	omap3_dpll_allow_idle(dpll_per_ck);
	dpll_usb_ck = clk_get(NULL, "dpll_usb_ck");
	omap3_dpll_allow_idle(dpll_usb_ck);
	dpll_unipro_ck = clk_get(NULL, "dpll_unipro_ck");
	omap3_dpll_allow_idle(dpll_unipro_ck);

	/* Enable autogating for all DPLL post dividers */
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD, OMAP4_CM_DIV_M2_DPLL_MPU_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT1_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M4_DPLL_IVA_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M5_DPLL_IVA_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M3_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT1_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M4_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M5_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT3_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M6_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT4_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M7_DPLL_CORE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD, OMAP4_CM_DIV_M2_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK,
		OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M3_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT1_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M4_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M5_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT3_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M6_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_HSDIVIDER_CLKOUT4_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M7_DPLL_PER_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_ABE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTHIF_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM1_CKGEN_MOD,	OMAP4_CM_DIV_M3_DPLL_ABE_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUT_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_USB_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKDCOLDO_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD, OMAP4_CM_CLKDCOLDO_DPLL_USB_OFFSET);
	cm_rmw_mod_reg_bits(OMAP4430_DPLL_CLKOUTX2_GATE_CTRL_MASK, 0x0,
		OMAP4430_CM2_CKGEN_MOD,	OMAP4_CM_DIV_M2_DPLL_UNIPRO_OFFSET);
}

/**
 * omap4_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static int __init omap4_pm_init(void)
{
	int ret;

	if (!cpu_is_omap44xx())
		return -ENODEV;

	pr_err("Power Management for TI OMAP4.\n");

#ifdef CONFIG_PM
	prcm_setup_regs();

	ret = pwrdm_for_each(pwrdms_setup, NULL);
	if (ret) {
		pr_err("Failed to setup powerdomains\n");
		goto err2;
	}

	mpu_pwrdm = pwrdm_lookup("mpu_pwrdm");
	if (!mpu_pwrdm) {
		printk(KERN_ERR "Failed to get lookup for MPU pwrdm's\n");
		goto err2;
	}

	(void) clkdm_for_each(clkdms_setup, NULL);

	omap4_mpuss_init();
#endif

#ifdef CONFIG_SUSPEND
	suspend_set_ops(&omap_pm_ops);
#endif /* CONFIG_SUSPEND */

	omap4_idle_init();

err2:
	return ret;
}
late_initcall(omap4_pm_init);
