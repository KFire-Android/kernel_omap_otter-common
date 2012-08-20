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
#include <linux/clk.h>
#include <asm/system_misc.h>
#include <linux/io.h>
#include <linux/gpio.h>

#include <plat/omap_device.h>
#include <plat/dvfs.h>

#include <mach/ctrl_module_wkup_44xx.h>
#include <mach/hardware.h>

#include "common.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "prm44xx.h"
#include "prm54xx.h"
#include "prcm44xx.h"
#include "prm-regbits-44xx.h"
#include "prminst44xx.h"
#include "scrm44xx.h"
#include "pm.h"
#include "voltage.h"
#include "prcm-debug.h"

#define EMIF_SDRAM_CONFIG2_OFFSET	0xc

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
u16 pm44xx_errata;

static struct powerdomain *tesla_pwrdm;
static struct clockdomain *tesla_clkdm;

/*
* HSI - OMAP4430-2.2BUG00055:
* HSI: DSP Swakeup generated is the same than MPU Swakeup.
* System can't enter in off mode due to the DSP.
*/
#define OMAP44xx_54xx_PM_ERRATUM_HSI_SWAKEUP_i702	BIT(0)
/*
 * Errata ID: i608: All OMAP4
 * On OMAP4, Retention-Till-Access Memory feature is not working
 * reliably and hardware recommondation is keep it disabled by
 * default
 */
#define OMAP44xx_54xx_PM_ERRATUM_RTA_i608		BIT(1)

static u8 pm44xx_54xx_errata;
#define is_pm44xx_54xx_erratum(erratum) (pm44xx_54xx_errata & \
					OMAP44xx_54xx_PM_ERRATUM_##erratum)

#ifdef CONFIG_SUSPEND
/* Names of various power states achievable */
static char *power_state_names[] = {
	[PWRDM_POWER_OFF] = "OFF",
	[PWRDM_POWER_INACTIVE] = "INACTIVE",
	[PWRDM_POWER_ON] = "ON",
	[PWRDM_POWER_CSWR] = "CSWR",
	[PWRDM_POWER_OSWR] = "OSWR",
};

static int omap4_5_pm_suspend(void)
{
	struct power_state *pwrst;
	int prev_state, curr_state, ret = 0, r;
	u32 cpu_id = smp_processor_id();

	/* Save current powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		curr_state = pwrdm_read_pwrst(pwrst->pwrdm);
		pwrst->saved_state = pwrdm_read_next_pwrst(pwrst->pwrdm);
		/*
		 * warn that we might not actually achieve OFF mode
		 * TODO: Consider this for detection and potential abort
		 * of un-successful OFF mode transition
		 * MPU PD is expected to be mismatched due to CPUIdle
		 * deciding not to precisely program the next_state
		 * We will over-ride it here anyway, but this is not
		 * expected to be done for other power domains.
		 * TODO: consider static dependencies as well here.
		 */
		if (strcmp(pwrst->pwrdm->name, "mpu_pwrdm") &&
		    pwrdm_power_state_gt(curr_state,
					 pwrst->saved_state)) {
			pr_debug("Powerdomain(%s) may fail enter target %s "
				"(current=%s next_state=%s) OR static dep?\n",
				pwrst->pwrdm->name,
				power_state_names[pwrst->next_state],
				power_state_names[curr_state],
				power_state_names[pwrst->saved_state]);
		}
	}

	/* Set targeted power domain states by suspend */
	list_for_each_entry(pwrst, &pwrst_list, node)
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);

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
	prcmdebug_dump(PRCMDEBUG_LASTSLEEP);

	/* Restore next powerdomain state */
	list_for_each_entry(pwrst, &pwrst_list, node) {
		prev_state = pwrdm_read_prev_pwrst(pwrst->pwrdm);
		curr_state = pwrdm_read_pwrst(pwrst->pwrdm);
		if (pwrdm_power_state_gt(prev_state, pwrst->next_state)) {
			pr_info("Powerdomain (%s) didn't enter target state %s "
				"(achieved=%s current=%s saved=%s)\n",
				pwrst->pwrdm->name,
				power_state_names[pwrst->next_state],
				power_state_names[prev_state],
				power_state_names[curr_state],
				power_state_names[pwrst->saved_state]);
			ret = -1;
		}
		/*
		 * If current state is lower or equal to saved state
		 * just program next_pwrst, but dont need to force the
		 * transition
		 */
		if (!pwrdm_power_state_eq(pwrst->saved_state, PWRDM_POWER_ON) &&
		    pwrdm_power_state_le(curr_state, pwrst->saved_state))
			r = pwrdm_set_next_pwrst(pwrst->pwrdm,
						 pwrst->saved_state);
		else
			r = omap_set_pwrdm_state(pwrst->pwrdm,
						 pwrst->saved_state);
		if (r)
			pr_err("Powerdomain (%s) restore to %s Fail(%d)"
				"(attempted=%s achieved=%s current=%s)\n",
				pwrst->pwrdm->name,
				power_state_names[pwrst->saved_state],
				r,
				power_state_names[pwrst->next_state],
				power_state_names[prev_state],
				power_state_names[curr_state]);
	}
	if (ret)
		pr_crit("Could not enter target state in pm_suspend\n");
	else
		pr_info("Successfully put all powerdomains to target state\n");

	return 0;
}
#endif /* CONFIG_SUSPEND */


/* omap_pm_clear_dsp_wake_up - SW WA for hardcoded wakeup dependency
* from HSI to DSP
*
* Due to HW bug, same SWakeup signal is used for both MPU and DSP.
* Thus Swakeup will unexpectedly wakeup the DSP domain even if nothing runs on
* DSP. Since MPU is faster to process SWakeup, it acknowledges the Swakeup to
* HSI before the DSP has completed its domain transition. This leaves the DSP
* Power Domain in INTRANSITION state forever, and prevents the DEVICE-OFF mode.
*
* Workaround consists in :
* when a SWakeup is asserted from HSI to MPU (and DSP) :
*  - force a DSP SW wakeup
*  - wait DSP module to be fully ON
*  - Configure a DSP CLK CTRL to HW_AUTO
*  - Wait on DSP module to be OFF
*
*  Note : we detect a Swakeup is asserted to MPU by checking when an interrupt
*         is received while HSI module is ON.
*
*  Bug ref is HSI-C1BUG00106 : dsp swakeup generated by HSI same as mpu swakeup
*/
void omap_pm_clear_dsp_wake_up(void)
{
	int ret;
	int timeout = 10;

	if (!is_pm44xx_54xx_erratum(HSI_SWAKEUP_i702))
		return;

	if (!tesla_pwrdm || !tesla_clkdm) {
		WARN_ONCE(1, "%s: unable to use tesla workaround\n", __func__);
		return;
	}

	ret = pwrdm_read_pwrst(tesla_pwrdm);
	/*
	 * If current Tesla power state is in RET/OFF and not in transition,
	 * then not hit by errata.
	 */
	if (ret <= PWRDM_POWER_CSWR) {
		if (!(omap4_prminst_read_inst_reg(tesla_pwrdm->prcm_partition,
				tesla_pwrdm->prcm_offs, OMAP4_PM_PWSTST)
				& OMAP_INTRANSITION_MASK))
			return;
	}

	if (clkdm_wakeup(tesla_clkdm))
		pr_err("%s: Failed to force wakeup of %s\n", __func__,
					tesla_clkdm->name);

	/* This takes less than a few microseconds, hence in context */
	pwrdm_wait_transition(tesla_pwrdm);

	/*
	 * Check current power state of Tesla after transition, to make sure
	 * that Tesla is indeed turned ON.
	 */
	ret = pwrdm_read_pwrst(tesla_pwrdm);
	do  {
		pwrdm_wait_transition(tesla_pwrdm);
		ret = pwrdm_read_pwrst(tesla_pwrdm);
	} while ((ret < PWRDM_POWER_INACTIVE) && --timeout);

	if (!timeout)
		pr_err("%s: Tesla failed to transition to ON state!\n",
					__func__);

	timeout = 10;
	clkdm_allow_idle(tesla_clkdm);

	/* Ensure Tesla power state in OFF state */
	ret = pwrdm_read_pwrst(tesla_pwrdm);
	do {
		pwrdm_wait_transition(tesla_pwrdm);
		ret = pwrdm_read_pwrst(tesla_pwrdm);
	} while ((ret >= PWRDM_POWER_INACTIVE) && --timeout);

	if (!timeout)
		pr_err("%s: Tesla failed to transition to OFF state\n",
					__func__);
}

/**
 * omap_idle_core_drivers - function where core driver idle routines
 * to be called.
 * @mpu_next_state:	Next MPUSS PD power state that system will attempt
 * @core_next_state:	Next Core PD power state that system will attempt
 *
 * This function is called by cpuidle when system is entering low
 * power state. Drivers like GPIO, thermal, SR can hook into this function
 * to trigger idle transition.
 */
void omap_idle_core_notifier(int mpu_next_state, int core_next_state)
{
	if (core_next_state != PWRDM_POWER_ON)
		omap2_gpio_prepare_for_idle(1);
}

/**
 * omap_enable_core_drivers - function where core driver enable routines
 * to be called.
 * @mpu_next_state:	Next MPUSS PD power state that system will attempt
 * @core_next_state:	Next Core PD power state that system will attempt
 *
 * This function is called by cpuidle when system is exiting low
 * power state. Drivers like GPIO, thermal, SR can hook into this function
 * to enable the modules.
 */
void omap_enable_core_notifier(int mpu_next_state, int core_next_state)
{
	if (core_next_state != PWRDM_POWER_ON)
		omap2_gpio_resume_after_idle();
}

static int __init pwrdms_setup(struct powerdomain *pwrdm, void *unused)
{
	struct power_state *pwrst;
	unsigned int program_state;

	/*
	 * If there are no pwrsts OR we have ONLY_ON pwrst, dont bother
	 * controlling it
	 */
	if (!pwrdm->pwrsts || PWRSTS_ON == pwrdm->pwrsts)
		return 0;

	/*
	 * Skip CPU0 and CPU1 power domains. CPU1 is programmed
	 * through hotplug path and CPU0 explicitly programmed
	 * further down in the code path - program the init state
	 * explicitly, but we won't manage the power state for
	 * suspend path
	 */
	if (!strncmp(pwrdm->name, "cpu", 3)) {
		program_state = PWRDM_POWER_ON;
		goto do_program_state;
	}

	pwrst = kzalloc(sizeof(struct power_state), GFP_ATOMIC);
	if (!pwrst)
		return -ENOMEM;

	pwrst->pwrdm = pwrdm;

	/* Program power domain to what state on suspend */
	pwrst->next_state = pwrdm_get_achievable_pwrst(pwrdm, PWRDM_POWER_OFF);

	pr_debug("Default setup powerdomain %s: next_state =%d\n", pwrdm->name,
		 pwrst->next_state);

	list_add(&pwrst->node, &pwrst_list);

	/*
	 * Only MPUSS and core power domain is added in the list. MPUSS/core
	 * is also controlled in CPUidle, so don't explicitly program the
	 * power state for MPUSS, we will set it to ON instead of deepest
	 * power state.
	 */
	if (!strcmp(pwrdm->name, "mpu_pwrdm") ||
	    !strcmp(pwrdm->name, "core_pwrdm")) {
		program_state = PWRDM_POWER_ON;
		goto do_program_state;
	}

	/*
	 * Program L4PER to CSWR instead of OSWR in order to avoid glitches
	 * due to the HW bug on GPIOs for OMAP5430 ES1.0
	 * (Bug ID: OMAP5430-1.0BUG01667).
	 */
	if (omap_rev() == OMAP5430_REV_ES1_0 ||
	    omap_rev() == OMAP5432_REV_ES1_0) {
		if (!strcmp(pwrdm->name, "l4per_pwrdm")) {
			program_state = PWRDM_POWER_CSWR;
			goto do_program_state;
		}
	}

	/*
	 * What state to program every Power domain can enter deepest to when
	 * not in suspend state?
	 */
	program_state = pwrdm_get_achievable_pwrst(pwrdm, PWRDM_POWER_OSWR);

do_program_state:
	return omap_set_pwrdm_state(pwrdm, program_state);
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
	struct clockdomain *mpuss_clkdm, *emif_clkdm, *dss_clkdm, *wkupaon_clkdm;
	int ret;

	/*
	 * The dynamic dependency between MPUSS -> EMIF
	 * doesn't work as expected. The hardware recommendation is to
	 * enable static dependencies for these to avoid system
	 * lock ups or random crashes.
	 */
	mpuss_clkdm = clkdm_lookup("mpu_clkdm");
	emif_clkdm = clkdm_lookup("emif_clkdm");
	dss_clkdm = clkdm_lookup("dss_clkdm");
	if (!mpuss_clkdm || !emif_clkdm || !dss_clkdm)
		return -EINVAL;

	ret = clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);
	if (ret)
		pr_err("Failed to add MPUSS -> emif wakeup dependency\n");
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

	ret |= clkdm_add_wkdep(dss_clkdm, emif_clkdm);
	if (ret)
		pr_err("Failed to add dss -> l3main2/emif wakeup dependency\n");
	else
		WARN(1, "Enabled DSS->EMIF Static dependency, needs proper rootcause\n");

	return ret;
}

static void __init omap_pm_setup_errata(void)
{
	if (cpu_is_omap44xx())
		pm44xx_54xx_errata |= OMAP44xx_54xx_PM_ERRATUM_HSI_SWAKEUP_i702;
	if (cpu_is_omap44xx())
		pm44xx_54xx_errata |= OMAP44xx_54xx_PM_ERRATUM_RTA_i608;
}

static void __init prcm_setup_regs(void)
{
	/*
	 * Errata ID: i608: All OMAP4
	 * On OMAP4, Retention-Till-Access Memory feature is not working
	 * reliably and hardware recommondation is keep it disabled by
	 * default
	 */
	if (is_pm44xx_54xx_erratum(RTA_i608)) {
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
			0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_SRAM_WKUP_SETUP_OFFSET);
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
			0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_LDO_SRAM_CORE_SETUP_OFFSET);
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
			0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_LDO_SRAM_MPU_SETUP_OFFSET);
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_DISABLE_RTA_EXPORT_MASK,
			0x1 << OMAP4430_DISABLE_RTA_EXPORT_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_LDO_SRAM_IVA_SETUP_OFFSET);
	}

	/*
	 * Toggle CLKREQ in RET and OFF states
	 * 0x2: CLKREQ is de-asserted when system clock is not
	 * required by any function in the device and if all voltage
	 * domains are in RET or OFF state.
	 */
	omap4_prminst_write_inst_reg(0x2, OMAP4430_PRM_PARTITION,
				     OMAP4430_PRM_DEVICE_INST,
				     OMAP4_PRM_CLKREQCTRL_OFFSET);

	/*
	 * De-assert PWRREQ signal in Device OFF state
	 * 0x3: PWRREQ is de-asserted if all voltage domain are in
	 * OFF state. Conversely, PWRREQ is asserted upon any
	 * voltage domain entering or staying in ON or SLEEP or
	 * RET state.
	 */
	omap4_prminst_write_inst_reg(0x3, OMAP4430_PRM_PARTITION,
				     OMAP4430_PRM_DEVICE_INST,
				     OMAP4_PRM_PWRREQCTRL_OFFSET);
}

/**
 * omap4_usec_to_val_scrm - convert microsecond value to SCRM module bitfield
 * @usec: microseconds
 * @shift: number of bits to shift left
 * @mask: bitfield mask
 *
 * Converts microsecond value to OMAP4 SCRM bitfield. Bitfield is
 * shifted to requested position, and checked agains the mask value.
 * If larger, forced to the max value of the field (i.e. the mask itself.)
 * Returns the SCRM bitfield value.
 */
static u32 omap4_usec_to_val_scrm(u32 usec, int shift, u32 mask)
{
	u32 val;

	val = omap_usec_to_32k(usec) << shift;

	/* Check for overflow, if yes, force to max value */
	if (val > mask)
		val = mask;

	return val;
}

static void __init omap4_scrm_setup_timings(void)
{
	u32 val;
	u32 tstart, tshut;

	/* Setup clksetup/clkshoutdown time for oscillator */
	omap_pm_get_oscillator(&tstart, &tshut);

	val = omap4_usec_to_val_scrm(tstart, OMAP4_SETUPTIME_SHIFT,
		OMAP4_SETUPTIME_MASK);
	val |= omap4_usec_to_val_scrm(tshut, OMAP4_DOWNTIME_SHIFT,
		OMAP4_DOWNTIME_MASK);
	omap4_prminst_write_inst_reg(val, OMAP4430_SCRM_PARTITION, 0x0,
				     OMAP4_SCRM_CLKSETUPTIME_OFFSET);

	/* Setup max PMIC startup/shutdown time */
	omap_pm_get_oscillator_voltage_ramp_time(&tstart, &tshut);

	val = omap4_usec_to_val_scrm(tstart, OMAP4_WAKEUPTIME_SHIFT,
		OMAP4_WAKEUPTIME_MASK);
	val |= omap4_usec_to_val_scrm(tshut, OMAP4_SLEEPTIME_SHIFT,
		OMAP4_SLEEPTIME_MASK);
	omap4_prminst_write_inst_reg(val, OMAP4430_SCRM_PARTITION, 0x0,
				     OMAP4_SCRM_PMICSETUPTIME_OFFSET);
}

/**
 * omap_pm_init - Init routine for OMAP4 PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
static int __init omap_pm_init(void)
{
	int ret, i;
	char *init_devices[] = {"mpu", "iva"};
	struct voltagedomain *mpu_voltdm;

	if (!(cpu_is_omap44xx() || cpu_is_omap54xx()))
		return -ENODEV;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	pr_info("Power Management for TI OMAP4XX/OMAP5XXX devices.\n");

	/* setup the erratas */
	omap_pm_setup_errata();

	prcm_setup_regs();

	/*
	 * Work around for OMAP443x Errata i632: "LPDDR2 Corruption After OFF
	 * Mode Transition When CS1 Is Used On EMIF":
	 * Overwrite EMIF1/EMIF2
	 * SECURE_EMIF1_SDRAM_CONFIG2_REG
	 * SECURE_EMIF2_SDRAM_CONFIG2_REG
	 */
	if (cpu_is_omap443x()) {
		void __iomem *secure_ctrl_mod;
		void __iomem *emif1;
		void __iomem *emif2;
		u32 val;

		secure_ctrl_mod = ioremap(OMAP4_CTRL_MODULE_WKUP, SZ_4K);
		emif1 = ioremap(OMAP44XX_EMIF1_BASE, SZ_1M);
		emif2 = ioremap(OMAP44XX_EMIF2_BASE, SZ_1M);

		BUG_ON(!secure_ctrl_mod || !emif1 || !emif2);

		val = __raw_readl(emif1 + EMIF_SDRAM_CONFIG2_OFFSET);
		__raw_writel(val, secure_ctrl_mod +
			     OMAP4_CTRL_SECURE_EMIF1_SDRAM_CONFIG2_REG);
		val = __raw_readl(emif2 + EMIF_SDRAM_CONFIG2_OFFSET);
		__raw_writel(val, secure_ctrl_mod +
			     OMAP4_CTRL_SECURE_EMIF2_SDRAM_CONFIG2_REG);
		/* barrier to ensure everything is in sync */
		wmb();
		iounmap(secure_ctrl_mod);
		iounmap(emif1);
		iounmap(emif2);
	}

	/* setup platform related pmic/oscillator timings */
	omap4_scrm_setup_timings();

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

	/*
	 * XXX: voltage config is not still completely valid for
	 * all OMAP5 samples as different samples have different
	 * stability level. Hence disable voltage scaling during Low
	 * Power states for samples which do not support it.
	 */
	if (!omap5_has_auto_ret()) {
		mpu_voltdm = voltdm_lookup("mpu");
		if (!mpu_voltdm) {
			pr_err("Failed to get MPU voltdm\n");
			goto err2;
		}
		mpu_voltdm->write(OMAP4430_VDD_MPU_I2C_DISABLE_MASK |
				  OMAP4430_VDD_CORE_I2C_DISABLE_MASK |
				  OMAP4430_VDD_IVA_I2C_DISABLE_MASK,
				  OMAP4_PRM_VOLTCTRL_OFFSET);
	}

	ret = omap_mpuss_init();
	if (ret) {
		pr_err("Failed to initialise OMAP MPUSS\n");
		goto err2;
	}

	(void) clkdm_for_each(omap_pm_clkdms_setup, NULL);

	/*
	 * ROM code initializes IVAHD and TESLA clock registers during
	 * secure RAM restore phase on omap4430 EMU/HS devices, thus
	 * IVAHD / TESLA clock registers must be saved / restored
	 * during MPU OSWR / device off.
	 */
	if (omap_rev() >= OMAP4430_REV_ES1_0 &&
	    omap_rev() < OMAP4430_REV_ES2_3 &&
	    omap_type() != OMAP2_DEVICE_TYPE_GP)
		pm44xx_errata |= PM_OMAP4_ROM_IVAHD_TESLA_ERRATUM_xxx;

	/*
	 * Similar to above errata, ROM code modifies L3INSTR clock
	 * registers also and these must be saved / restored during
	 * MPU OSWR / device off.
	 */
	if (omap_type() != OMAP2_DEVICE_TYPE_GP)
		pm44xx_errata |= PM_OMAP4_ROM_L3INSTR_ERRATUM_xxx;

#ifdef CONFIG_SUSPEND
	omap_pm_suspend = omap4_5_pm_suspend;
#endif
	if (is_pm44xx_54xx_erratum(HSI_SWAKEUP_i702)) {
		tesla_pwrdm = pwrdm_lookup("tesla_pwrdm");
		tesla_clkdm = clkdm_lookup("tesla_clkdm");
	}

	/* Setup the scales for every init device appropriately */
	for (i = 0; i < ARRAY_SIZE(init_devices); i++) {
		struct omap_hwmod *oh = omap_hwmod_lookup(init_devices[i]);
		struct clk *clk;
		struct device *dev;
		unsigned int rate;

		if (!oh || !oh->od || !oh->main_clk) {
			pr_warn("%s: no hwmod or odev or clk for %s, [%d]\n",
				__func__, init_devices[i], i);
			pr_warn("%s:oh=%p od=%p clk=%p fail scale boot OPP.\n",
				__func__, oh, (oh) ? oh->od : NULL,
				(oh) ? oh->main_clk :  NULL);
			continue;
		}

		clk = oh->_clk;
		dev = &oh->od->pdev->dev;
		/* Get the current rate */
		rate = clk_get_rate(clk);

		/* Update DVFS framework with rate information */
		ret = omap_device_scale(dev, rate);
		if (ret) {
			dev_warn(dev, "%s unable to scale to %d - %d\n",
				 __func__, rate, ret);
			/* Continue to next device */
		}
	}

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
