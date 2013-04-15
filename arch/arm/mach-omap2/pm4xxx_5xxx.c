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
#include <linux/mfd/omap_control.h>
#include <linux/power/smartreflex.h>

#include <plat/omap_device.h>
#include <plat/dvfs.h>

#include <mach/ctrl_module_wkup_44xx.h>
#include <mach/hardware.h>

#include "common.h"
#include "clockdomain.h"
#include "powerdomain.h"
#include "prm44xx.h"

#include "prcm44xx.h"
#include "prm-regbits-44xx.h"
#include "prminst44xx.h"
#include "prm54xx.h"
#include "scrm44xx.h"
#include "pm.h"
#include "voltage.h"
#include "prcm-debug.h"
#include "control.h"

#define EMIF_SDRAM_CONFIG_OFFSET	0x8
#define EMIF_SDRAM_CONFIG2_OFFSET	0xc

struct power_state {
	struct powerdomain *pwrdm;
	u32 next_state;
#ifdef CONFIG_SUSPEND
	u32 saved_state;
	u32 saved_logic_state;
	u32 context_loss_count;
#endif
	struct list_head node;
};

static LIST_HEAD(pwrst_list);
u16 pm44xx_errata;

static struct powerdomain *tesla_pwrdm;
static struct clockdomain *tesla_clkdm;
static struct clockdomain *emif_clkdm;
static struct clockdomain *mpuss_clkdm;
static struct clockdomain *abe_clkdm;
static int staticdep_wa_i745_applied;

static struct powerdomain *gpu_pd, *iva_pd;
static struct voltagedomain *mpu_vdd, *core_vdd, *mm_vdd;

static void omap4_syscontrol_lpddr_clk_io_errata_i736(bool enable);


/*
* HSI - OMAP4430-2.2BUG00055:
* HSI: DSP Swakeup generated is the same than MPU Swakeup.
* System can't enter in off mode due to the DSP.
*/
#define OMAP44xx_54xx_PM_ERRATUM_HSI_SWAKEUP_i702	BIT(0)
/*
 * Errata ID: i608: All OMAP4
 * On OMAP4, Retention-Till-Access Memory feature is not working
 * reliably and hardware recommendation is keep it disabled by
 * default
 */
#define OMAP44xx_54xx_PM_ERRATUM_RTA_i608		BIT(1)

/* MPU EMIF Static Dependency Needed due to i731 erratum ID for 443x */
#define OMAP44xx_54xx_PM_ERRATUM_MPU_EMIF_STATIC_DEP_NEEDED_i731	BIT(2)

/*
 * Dynamic Dependency cannot be permanently enabled due to i745 erratum ID
 * for 446x/447x
 * WA involves:
 * Enable MPU->EMIF SD before WFI and disable while coming out of WFI.
 * This works around system hang/lockups seen when only MPU->EMIF
 * dynamic dependency set. Allows dynamic dependency to be used
 * in all active use cases and get all the power savings accordingly.
 */
#define OMAP44xx_54xx_PM_ERRATUM_MPU_EMIF_NO_DYNDEP_IDLE_i745	BIT(3)

/* Errata ID: i736: All OMAP4
 * There is a HW bug in CMD PHY which gives ISO signals as same for both
 * PADn and PADp on differential IO pad, because of which IO leaks higher
 * as pull controls are differential internally and pull value does not
 * match A value.
 * Though there is no functionality impact due to this bug, it is seen
 * that by disabling the pulls there is a savings ~500uA in OSWR, but draws
 * ~300uA more during OFF mode.
 * Workaround:
 * To prevent an increase in leakage, it is recommended to disable the pull
 * logic for these I/Os except during off mode.
 * So the default state of the I/Os (to program at boot) will have pull
 * logic disable:
 * CONTROL_LPDDR2IO1_2[18:17]LPDDR2IO1_GR10_WD = 00
 * CONTROL_LPDDR2IO2_2[18:17]LPDDR2IO2_GR10_WD = 00
 * When entering off mode, these I/Os must be configured with pulldown enable:
 * CONTROL_LPDDR2IO1_2[18:17]LPDDR2IO1_GR10_WD = 10
 * CONTROL_LPDDR2IO2_2[18:17]LPDDR2IO2_GR10_WD = 10
 * When resuming from off mode, pull logic must be disabled.
 */
#define OMAP44xx_54xx_PM_ERRATUM_LPDDR_CLK_IO_i736		BIT(4)
#define LPDDR_WD_PULL_DOWN		0x02

/* Errata ID: un-named: All OMAP4
 * AUTO RET for IVA VDD Cannot be permanently enabled during OFF mode due to
 * potential race between IVA VDD entering RET and start of Device OFF mode.
 *
 * It is mandatory to have AUTO RET for IVA VDD exclusive with Device OFF mode.
 * In order to avoid lockup in OFF mode sequence, system must ensure IVA
 * voltage domain transitions straight from stable ON to OFF.
 *
 * In addition, management of IVA VDD SmartReflex sensor at exit of idle path
 * may introduce a misalignment between IVA Voltage Controller state and IVA
 * PRCM voltage FSM based on following identified scenario:
 *
 * IVA Voltage Controller is woken-up due to SmartReflex management while
 * IVA PRCM voltage FSM stays in RET in absence of any IVA module wake-up event
 * (which is not systematic in idle path as opposed to MPU and CORE VDDs being
 * necessarily woken up with MPU and CORE PDs).
 *
 * NOTE: This is updated work-around relaxes constraint of always holding
 * IVA AUTO RET disabled (now only before OFF), which in turn was preventing
 * IVA VDD from reaching RET and SYS_CLK from being automatically gated in
 * idle path.
 * TODO: Once this is available, update with final iXXX Errata number.
 *
 * WA involves:
 * Ensure stable ON-OFF transition for IVA VDD during OFF mode sequence.
 * Ensure VCON and PRCM FSM are synced despite IVA SR handling in idle path.
 * 1) AUTO RET for IVA VDD is enabled entering in idle path, disabled exiting
 *   idle path and IVA VDD is always woken-up with a SW dummy wake up.
 * 2) OFF mode is enabled only in Suspend path.
 * 3) AUTO RET for IVA VDD remains disabled in Suspend path (before OFF mode).
 */
#define OMAP44xx_54xx_PM_ERRATUM_IVA_AUTO_RET_IDLE_iXXX	BIT(5)

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
	list_for_each_entry(pwrst, &pwrst_list, node) {
		pwrst->context_loss_count =
			pwrdm_get_context_loss_count(pwrst->pwrdm);
		omap_set_pwrdm_state(pwrst->pwrdm, pwrst->next_state);
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
		int context_loss_count =
			pwrdm_get_context_loss_count(pwrst->pwrdm);

		prev_state = pwrdm_read_prev_pwrst(pwrst->pwrdm);
		curr_state = pwrdm_read_pwrst(pwrst->pwrdm);

		/*
		 * Contextloss count difference is enough to identify
		 * powerdomains that blocked suspend, except incase of
		 * always-on powerdomains
		 * Compare prev_state and next_state to avoid reporting
		 * always-on powerdomains as those blocking suspend
		 */
		if (pwrdm_power_state_gt(prev_state, pwrst->next_state) &&
		    context_loss_count == pwrst->context_loss_count) {
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

static int _pwrdms_set_to_on(struct powerdomain *pwrdm, void *unused)
{
	int r = 0;

	if (pwrdm->pwrsts)
		r = omap_set_pwrdm_state(pwrdm, PWRDM_POWER_ON);

	return r;
}

/**
 * omap4_pm_cold_reset() - Cold reset OMAP4
 * @reason:	why am I resetting?
 *
 * As per the TRM, it is recommended that we set all the power domains to
 * ON state before we trigger cold reset.
 */
int omap4_pm_cold_reset(char *reason)
{
	/* Switch ON all pwrst registers */
	if (pwrdm_for_each(_pwrdms_set_to_on, NULL))
		pr_err("%s: Failed to setup powerdomains to ON\n", __func__);
	/* Proceed even if failed */

	WARN(1, "Arch Cold reset has been triggered due to %s\n", reason);
	omap4_prminst_global_cold_sw_reset(); /* never returns */

	/* If we reached here - something bad went on.. */
	BUG();

	/* make the compiler happy */
	return -EINTR;
}

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


static void omap4_pm_force_wakeup_iva(void)
{
	/* Configures ABE clockdomain in SW_WKUP */
	if (clkdm_wakeup(abe_clkdm))
		pr_err("%s: Failed to force wakeup of %s\n",
			__func__, abe_clkdm->name);
	/* Configures ABE clockdomain back to HW_AUTO */
	else
		clkdm_allow_idle(abe_clkdm);
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
	bool is_suspend;

	/*
	 * Driver notifier calls should make use of is_suspend flag
	 * to differentiate idle Vs suspend path.
	 */
	is_suspend = !is_idle_task(current);

	if (is_suspend && smp_processor_id()) {
		WARN(1, "Called from processor %d, which is unexpected\n",
			smp_processor_id());
		return;
	}

	if (core_next_state != PWRDM_POWER_ON)
		omap2_gpio_prepare_for_idle(1);

	/*
	 * Need to keep thermal monitoring as long as Core is active.
	 * Check for the GPU powerdomain and Core state before idling
	 * the thermal.
	 */
	if (!is_suspend && gpu_pd
	     && (pwrdm_read_pwrst(gpu_pd) != PWRDM_POWER_ON)
	     && pwrdm_power_state_le(core_next_state, PWRDM_POWER_INACTIVE))
		omap_bandgap_prepare_for_idle();

	if (pwrdm_power_state_lt(mpu_next_state, PWRDM_POWER_INACTIVE))
		omap_sr_disable(mpu_vdd);
	if (pwrdm_power_state_lt(core_next_state, PWRDM_POWER_INACTIVE)) {
		omap_sr_disable(core_vdd);
		omap_sr_disable(mm_vdd);
	}

	if (is_pm44xx_54xx_erratum(MPU_EMIF_NO_DYNDEP_IDLE_i745) &&
	    pwrdm_power_state_le(core_next_state, PWRDM_POWER_INACTIVE)) {
		/* Configures MEMIF clockdomain in SW_WKUP */
		if (!emif_clkdm || !mpuss_clkdm) {
			pr_err("%s: MPU/EMIF clockdomains not found\n",
			       __func__);
		} else if (clkdm_wakeup(emif_clkdm)) {
			pr_err("%s: Failed to force wakeup of %s\n",
			       __func__, emif_clkdm->name);
		} else {
			/* Enable MPU-EMIF Static Dependency around WFI */
			if (clkdm_add_wkdep(mpuss_clkdm, emif_clkdm))
				pr_err("%s: Failed to Add wkdep %s->%s\n",
				       __func__, mpuss_clkdm->name,
				       emif_clkdm->name);
			else
				staticdep_wa_i745_applied = 1;

			/* Configures MEMIF clockdomain back to HW_AUTO */
			clkdm_allow_idle(emif_clkdm);
		}
	}

	/*
	* Do not enable IVA AUTO-RET if device targets OFF mode.
	* In such case, purpose of IVA AUTO-RET WA is to ensure
	* IVA domain goes straight from stable Voltage ON to OFF.
	*/
	if (is_pm44xx_54xx_erratum(IVA_AUTO_RET_IDLE_iXXX) &&
	    !is_suspend &&
	    pwrdm_power_state_lt(core_next_state, PWRDM_POWER_INACTIVE))
		/* Decrement IVA PD usecount and allow IVA VDD AUTO RET */
		pwrdm_usecount_dec(iva_pd);

	if (is_suspend)
		omap4_syscontrol_lpddr_clk_io_errata_i736(false);
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
	bool is_suspend;

	/*
	 * Driver notifier calls should make use of is_suspend flag
	 * to differentiate idle Vs suspend path.
	 */
	is_suspend = !is_idle_task(current);

	if (is_suspend && smp_processor_id()) {
		WARN(1, "Called from processor %d, which is unexpected\n",
			smp_processor_id());
		return;
	}

	if (is_suspend)
		omap4_syscontrol_lpddr_clk_io_errata_i736(true);

	if (core_next_state != PWRDM_POWER_ON)
		omap2_gpio_resume_after_idle();

	/* Coming out of Idle, start monitoring the thermal. */
	if (!is_suspend && gpu_pd
	    && (pwrdm_read_pwrst(gpu_pd) != PWRDM_POWER_ON)
	    && pwrdm_power_state_le(core_next_state, PWRDM_POWER_INACTIVE))
		omap_bandgap_resume_after_idle();

	/*
	* Ensure PRCM IVA Voltage FSM is ON upon exit of idle.
	* Upon IVA AUTO-RET disabling, trigger a Dummy SW Wakup on IVA domain.
	* Later on, upon enabling of IVA Smart-Reflex, IVA Voltage Controller
	* state will be ON as well. Both FSMs would now be aligned and safe
	* during active and for further attempts to Device OFF mode for which
	* IVA would go straight from ON to OFF.
	*/
	if (is_pm44xx_54xx_erratum(IVA_AUTO_RET_IDLE_iXXX) &&
	    !is_suspend &&
	    pwrdm_power_state_lt(core_next_state, PWRDM_POWER_INACTIVE)) {
		/* Increment PD IVA usecount and disable IVA VDD AUTO RET */
		pwrdm_usecount_inc(iva_pd);

		omap4_pm_force_wakeup_iva();
	}

	if (pwrdm_power_state_lt(mpu_next_state, PWRDM_POWER_INACTIVE))
		omap_sr_enable(mpu_vdd);
	if (pwrdm_power_state_lt(core_next_state, PWRDM_POWER_INACTIVE)) {
		omap_sr_enable(core_vdd);
		omap_sr_enable(mm_vdd);
	}

	/*
	 * NOTE: is_pm44xx_erratum is not strictly required, but retained for
	 * code context readability.
	 */
	if (is_pm44xx_54xx_erratum(MPU_EMIF_NO_DYNDEP_IDLE_i745) &&
	    staticdep_wa_i745_applied) {
		/* Configures MEMIF clockdomain in SW_WKUP */
		if (!emif_clkdm || !mpuss_clkdm) {
			pr_err("%s: MPU/EMIF clockdomains not found\n",
			       __func__);
		} else if (clkdm_wakeup(emif_clkdm))
			pr_err("%s: Failed to force wakeup of %s\n",
			       __func__, emif_clkdm->name);
		/* Disable MPU-EMIF Static Dependency on WFI exit */
		else if (clkdm_del_wkdep(mpuss_clkdm, emif_clkdm))
			pr_err("%s: Failed to remove wkdep %s->%s\n",
			       __func__, mpuss_clkdm->name,
			       emif_clkdm->name);
		/* Configures MEMIF clockdomain back to HW_AUTO */
		clkdm_allow_idle(emif_clkdm);
	}
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
	 * What state to program every Power domain can enter deepest to when
	 * not in suspend state?
	 */
	program_state = pwrdm_get_achievable_pwrst(pwrdm, PWRDM_POWER_OFF);

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
	struct clockdomain *l3_1_clkdm, *l4wkup;
	struct clockdomain *ducati_clkdm, *l3_2_clkdm, *l4_per_clkdm;
	int ret = 0;
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

	/* if we cannot ever enable dynamic dependencies. */
	if (is_pm44xx_54xx_erratum(MPU_EMIF_STATIC_DEP_NEEDED_i731))
		ret |= clkdm_add_wkdep(mpuss_clkdm, emif_clkdm);

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
	struct clockdomain *dss_clkdm, *wkupaon_clkdm;
	struct clockdomain *l3_2_clkdm, *l3_1_clkdm, *ipu_clkdm;
	int ret;

	/*
	 * The dynamic dependency between MPUSS -> EMIF
	 * doesn't work as expected. The hardware recommendation is to
	 * enable static dependencies for these to avoid system
	 * lock ups or random crashes.
	 */
	mpuss_clkdm = clkdm_lookup("mpu_clkdm");
	ipu_clkdm = clkdm_lookup("ipu_clkdm");
	emif_clkdm = clkdm_lookup("emif_clkdm");
	dss_clkdm = clkdm_lookup("dss_clkdm");
	l3_1_clkdm = clkdm_lookup("l3main1_clkdm");
	l3_2_clkdm = clkdm_lookup("l3main2_clkdm");

	if (!mpuss_clkdm || !emif_clkdm || !dss_clkdm ||
	    !l3_1_clkdm || !l3_2_clkdm || !ipu_clkdm)
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

	/*
	 * COBRA-1.0BUG00191: System hang occurs with IPU to L3_main1,
	 * L3_main2 Dynamic dependencies
	 * Therefore, use static dependency between IPU and L3
	 */
	ret |= clkdm_add_wkdep(ipu_clkdm, l3_1_clkdm);
	if (ret)
		pr_err("Failed to add IPUSS -> l3main1 wakeup dependency\n");

	/*
	 * COBRA-1.0BUG00191: System hang occurs with IPU to L3_main1,
	 * L3_main2 Dynamic dependencies
	 * Therefore, use static dependency between IPU and L3
	 */
	ret |= clkdm_add_wkdep(ipu_clkdm, l3_2_clkdm);
	if (ret)
		pr_err("Failed to add IPUSS -> l3main2 dependency\n");

	return ret;
}

static void __init omap_pm_setup_errata(void)
{
	if (cpu_is_omap443x())
		/* Dynamic Dependency errata for all 443x silicons */
		pm44xx_54xx_errata |=
		       OMAP44xx_54xx_PM_ERRATUM_MPU_EMIF_STATIC_DEP_NEEDED_i731;

	if (cpu_is_omap446x())
		pm44xx_54xx_errata |=
			OMAP44xx_54xx_PM_ERRATUM_MPU_EMIF_NO_DYNDEP_IDLE_i745;

	if (cpu_is_omap447x())
		pm44xx_54xx_errata |=
			OMAP44xx_54xx_PM_ERRATUM_MPU_EMIF_NO_DYNDEP_IDLE_i745;

	if (cpu_is_omap44xx()) {
		pm44xx_54xx_errata |= OMAP44xx_54xx_PM_ERRATUM_HSI_SWAKEUP_i702;
		pm44xx_54xx_errata |= OMAP44xx_54xx_PM_ERRATUM_RTA_i608;
		pm44xx_54xx_errata |=
			OMAP44xx_54xx_PM_ERRATUM_LPDDR_CLK_IO_i736;
		pm44xx_54xx_errata |=
			OMAP44xx_54xx_PM_ERRATUM_IVA_AUTO_RET_IDLE_iXXX;
	}
}

static void omap4_syscontrol_lpddr_clk_io_errata_i736(bool enable)
{
	u32 v = 0;

	if (!is_pm44xx_54xx_erratum(LPDDR_CLK_IO_i736))
		return;

	v =
	   omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO1_2);
	v &= ~OMAP4_LPDDR2IO1_GR10_WD_MASK;
	if (!enable)
		v |= LPDDR_WD_PULL_DOWN << OMAP4_LPDDR2IO1_GR10_WD_SHIFT;
	omap4_ctrl_pad_writel(v,
			      OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO1_2);

	v =
	   omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO2_2);
	v &= ~OMAP4_LPDDR2IO2_GR10_WD_MASK;
	if (!enable)
		v |= LPDDR_WD_PULL_DOWN << OMAP4_LPDDR2IO1_GR10_WD_SHIFT;
	omap4_ctrl_pad_writel(v,
			      OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO2_2);
}

static void __init omap4_syscontrol_setup_regs(void)
{
	u32 v;

	/* Disable LPDDR VREF manual control and enable Auto control */
	v =
	   omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO1_3);
	if (v & (OMAP4_LPDDR21_VREF_EN_CA_MASK | OMAP4_LPDDR21_VREF_EN_DQ_MASK))
		pr_warn("OMAP4: PM: LPDDR2IO1_3 VREF CA and DQ Manual control"
			" is wrongly set by bootloader\n");

	v &= ~(OMAP4_LPDDR21_VREF_EN_CA_MASK | OMAP4_LPDDR21_VREF_EN_DQ_MASK);
	v |= OMAP4_LPDDR21_VREF_AUTO_EN_CA_MASK |
	     OMAP4_LPDDR21_VREF_AUTO_EN_DQ_MASK;
	omap4_ctrl_pad_writel(
		v, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO1_3);

	v =
	   omap4_ctrl_pad_readl(OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO2_3);
	if (v & (OMAP4_LPDDR21_VREF_EN_CA_MASK | OMAP4_LPDDR21_VREF_EN_DQ_MASK))
		pr_warn("OMAP4: PM: LPDDR2IO2_3 VREF CA and DQ Manual control"
			" is wrongly set by bootloader\n");

	v &= ~(OMAP4_LPDDR21_VREF_EN_CA_MASK | OMAP4_LPDDR21_VREF_EN_DQ_MASK);
	v |= OMAP4_LPDDR21_VREF_AUTO_EN_CA_MASK |
	     OMAP4_LPDDR21_VREF_AUTO_EN_DQ_MASK;
	omap4_ctrl_pad_writel(
		v, OMAP4_CTRL_MODULE_PAD_CORE_CONTROL_LPDDR2IO2_3);

	omap4_syscontrol_lpddr_clk_io_errata_i736(true);
}

static void __init prcm_setup_regs(void)
{
	s16 dev_inst = cpu_is_omap44xx() ? OMAP4430_PRM_DEVICE_INST :
					   OMAP54XX_PRM_DEVICE_INST;

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

	/* Allow SRAM LDO to enter RET during  low power state*/
	if (cpu_is_omap446x() || cpu_is_omap447x()) {
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_RETMODE_ENABLE_MASK,
			0x1 << OMAP4430_RETMODE_ENABLE_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_LDO_SRAM_CORE_CTRL_OFFSET);
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_RETMODE_ENABLE_MASK,
			0x1 << OMAP4430_RETMODE_ENABLE_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_LDO_SRAM_MPU_CTRL_OFFSET);
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_RETMODE_ENABLE_MASK,
			0x1 << OMAP4430_RETMODE_ENABLE_SHIFT,
			OMAP4430_PRM_PARTITION,
			OMAP4430_PRM_DEVICE_INST,
			OMAP4_PRM_LDO_SRAM_IVA_CTRL_OFFSET);
	}

	/*
	 * Toggle CLKREQ in RET and OFF states
	 * 0x2: CLKREQ is de-asserted when system clock is not
	 * required by any function in the device and if all voltage
	 * domains are in RET or OFF state.
	 */
	omap4_prminst_write_inst_reg(0x2, OMAP4430_PRM_PARTITION,
				     dev_inst,
				     OMAP4_PRM_CLKREQCTRL_OFFSET);

	/*
	 * De-assert PWRREQ signal in Device OFF state
	 * 0x3: PWRREQ is de-asserted if all voltage domain are in
	 * OFF state. Conversely, PWRREQ is asserted upon any
	 * voltage domain entering or staying in ON or SLEEP or
	 * RET state.
	 */
	omap4_prminst_write_inst_reg(0x3, OMAP4430_PRM_PARTITION,
				     dev_inst,
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

static int __init _voltdm_sum_time(struct voltagedomain *voltdm, void *user)
{
	struct omap_voltdm_pmic *pmic;
	u32 *max_time = (u32 *)user;

	if (!voltdm || !max_time) {
		WARN_ON(1);
		return -EINVAL;
	}

	pmic = voltdm->pmic;
	if (pmic) {
		*max_time += DIV_ROUND_UP(voltdm->vc_param->on,
					  pmic->slew_rate);
		*max_time += pmic->switch_on_time;
	}

	return 0;
}

static void __init omap4_scrm_setup_timings(void)
{
	u32 val;
	u32 tstart, tshut;
	u32 reset_delay_time;

	/* Setup clksetup/clkshoutdown time for oscillator */
	omap_pm_get_oscillator(&tstart, &tshut);

	val = omap4_usec_to_val_scrm(tstart, OMAP4_SETUPTIME_SHIFT,
		OMAP4_SETUPTIME_MASK);
	val |= omap4_usec_to_val_scrm(tshut, OMAP4_DOWNTIME_SHIFT,
		OMAP4_DOWNTIME_MASK);
	omap4_prminst_write_inst_reg(val, OMAP4430_SCRM_PARTITION, 0x0,
				     OMAP4_SCRM_CLKSETUPTIME_OFFSET);

	/*
	 * Setup OMAP WARMRESET time:
	 * we use the sum of each voltage domain setup times to handle
	 * the worst case condition where the device resets from OFF mode.
	 * hence we leave PRM_VOLTSETUP_WARMRESET alone as this is
	 * already part of RSTTIME1 we program in.
	 * in addition, to handle oscillator switch off and switch back on
	 * (in case WDT triggered while CLKREQ goes low), we also
	 * add in the additional latencies.
	 */
	reset_delay_time = omap_pm_get_rsttime_latency();
	if (!voltdm_for_each(_voltdm_sum_time, (void *)&reset_delay_time)) {
		s16 dev_inst = cpu_is_omap44xx() ? OMAP4430_PRM_DEVICE_INST :
						   OMAP54XX_PRM_DEVICE_INST;

		reset_delay_time += tstart + tshut;
		val = omap4_usec_to_val_scrm(reset_delay_time,
					     OMAP4430_RSTTIME1_SHIFT,
					     OMAP4430_RSTTIME1_MASK);
		omap4_prminst_rmw_inst_reg_bits(OMAP4430_RSTTIME1_MASK,
						val,
						OMAP4430_PRM_PARTITION,
						dev_inst,
						OMAP4_PRM_RSTTIME_OFFSET);
	}

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
 * omap4_pm_init - Init routine for OMAP4+ PM
 *
 * Initializes all powerdomain and clockdomain target states
 * and all PRCM settings.
 */
int __init omap4_pm_init(void)
{
	int ret;
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

	if (cpu_is_omap44xx())
		omap4_syscontrol_setup_regs();

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

	if (is_pm44xx_54xx_erratum(IVA_AUTO_RET_IDLE_iXXX)) {
		/*
		 * Do Erratum initialization before MPUSS init and AUTO RET
		 * enabling
		 */
		abe_clkdm = clkdm_lookup("abe_clkdm");
		if (!abe_clkdm) {
			pr_err("Failed to lookup ABE clock domain\n");
			return -ENODEV;
		}

		iva_pd = pwrdm_lookup("ivahd_pwrdm");
		if (!iva_pd) {
			pr_err("Failed to lookup IVA power domain\n");
			return -ENODEV;
		}

		/* Increment PD IVA usecount and disable IVA VDD AUTO RET */
		pwrdm_usecount_inc(iva_pd);

		/*
		 * Wake up IVA to ensure that IVA VC, IVA PRCM and AVS
		 * are in sync
		 */
		omap4_pm_force_wakeup_iva();
	}

	/*
	 * XXX: voltage config is not still completely valid for
	 * all OMAP5 samples as different samples have different
	 * stability level. Hence disable voltage scaling during Low
	 * Power states for samples which do not support it.
	 */
	if (cpu_is_omap54xx() && !omap5_has_auto_ret()) {
		u32 mask = OMAP4430_VDD_MPU_I2C_DISABLE_MASK |
				OMAP4430_VDD_CORE_I2C_DISABLE_MASK |
				OMAP4430_VDD_IVA_I2C_DISABLE_MASK;

		mpu_voltdm = voltdm_lookup("mpu");
		if (!mpu_voltdm) {
			pr_err("Failed to get MPU voltdm\n");
			goto err2;
		}
		mpu_voltdm->rmw(mask, mask, OMAP4_PRM_VOLTCTRL_OFFSET);
	}

	ret = omap_mpuss_init();
	if (ret) {
		pr_err("Failed to initialise OMAP MPUSS\n");
		goto err2;
	}

	mpu_vdd = voltdm_lookup("mpu");
	if (!mpu_vdd) {
		pr_err("Failed to lookup MPU voltage domain\n");
		goto err2;
	}

	core_vdd = voltdm_lookup("core");
	if (!core_vdd) {
		pr_err("Failed to lookup CORE voltage domain\n");
		goto err2;
	}

	if (cpu_is_omap54xx())
		mm_vdd = voltdm_lookup("mm");
	else
		mm_vdd = voltdm_lookup("iva");
	if (!mm_vdd) {
		pr_err("Failed to lookup Multimedia(iva) voltage domain\n");
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

	gpu_pd = pwrdm_lookup(cpu_is_omap54xx() ? "gpu_pwrdm" : "gfx_pwrdm");

	if (!gpu_pd)
		pr_err("%s: Unable to get GPU power domain\n", __func__);

	/*
	 * HACK for EMIF and DDR3 on OMAP5
	 * Currently we see that anytime EMIF clock idle, we get DDR3
	 * corrupted. holding EMIF in no idle helps resolve this.
	 * ASSUME: EMIF1 configuration will also be used for EMIF2
	 */
	if (cpu_is_omap543x()) {
		void __iomem *emif1;
		u32 val;

		emif1 = ioremap(OMAP54XX_EMIF1_BASE, SZ_1M);

		BUG_ON(!emif1);

		val = __raw_readl(emif1 + EMIF_SDRAM_CONFIG_OFFSET);
		val >>= 29; /* Pick up SDRAM_TYPE */
		iounmap(emif1);

		/* If DDR3 is used, Deny EMIF idle */
		if (val == 0x3) {
			clkdm_deny_idle(emif_clkdm);
			WARN(1, "%s: EMIF Clock domain denied idle: DDR3 WA\n",
			     __func__);
		}
	}


err2:
	return ret;
}

/**
 * omap4_set_processor_device_opp - set processor device opp
 *
 * Initializes all processor devices to the right OPP.
 */
void __init omap4_set_processor_device_opp(void)
{
	int ret, i;
	char *init_devices[] = {"mpu", "iva"};

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
}

/**
 * omap4_init_cpuidle - initialize omap cpuidle driver
 *
 * Initialize omap cpuidle driver.
 */
void __init omap4_init_cpuidle(void)
{
	if (cpu_is_omap44xx()) {
		/* Overwrite the default arch_idle() */
		arm_pm_idle = omap_default_idle;
		omap4_idle_init();
	} else if (cpu_is_omap54xx()) {
		omap5_idle_init();
	}
}
