/*
 * OMAP MPUSS low power code
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * OMAP4430 MPUSS mainly consists of dual Cortex-A9 with per-CPU
 * Local timer and Watchdog, GIC, SCU, PL310 L2 cache controller,
 * CPU0 and CPU1 LPRM modules.
 * CPU0, CPU1 and MPUSS each have there own power domain and
 * hence multiple low power combinations of MPUSS are possible.
 *
 * The CPU0 and CPU1 can't support Closed switch Retention (CSWR)
 * because the mode is not supported by hw constraints of dormant
 * mode. While waking up from the dormant mode, a reset  signal
 * to the Cortex-A9 processor must be asserted by the external
 * power controller.
 *
 * With architectural inputs and hardware recommendations, only
 * below modes are supported from power gain vs latency point of view.
 *
 *	CPU0		CPU1		MPUSS
 *	----------------------------------------------
 *	ON		ON		ON
 *	ON(Inactive)	OFF		ON(Inactive)
 *	OFF		OFF		CSWR
 *	OFF		OFF		OSWR
 *	OFF		OFF		OFF(Device OFF *TBD)
 *	----------------------------------------------
 *
 * Note: CPU0 is the master core and it is the last CPU to go down
 * and first to wake-up when MPUSS low power states are excercised
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/linkage.h>
#include <linux/smp.h>

#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>
#include <asm/pgalloc.h>
#include <asm/suspend.h>
#include <asm/hardware/cache-l2x0.h>

#include "soc.h"
#include "common.h"
#include "omap44xx.h"
#include "omap4-sar-layout.h"
#include "pm.h"
#include "prcm_mpu44xx.h"
#include "prcm_mpu54xx.h"
#include "prminst44xx.h"
#include "prcm44xx.h"
#include "prm44xx.h"
#include "prm-regbits-44xx.h"

#ifdef CONFIG_SMP

struct omap4_cpu_pm_info {
	struct powerdomain *pwrdm;
	void __iomem *scu_sar_addr;
	void __iomem *wkup_sar_addr;
	void __iomem *l2x0_sar_addr;
	void (*secondary_startup)(void);
};

struct cpu_pm_ops {
	int (*finish_suspend)(unsigned long cpu_state);
	void (*resume)(void);
	void (*scu_prepare)(unsigned int cpu_id, u8 cpu_state);
	void (*hotplug_restart)(void);
};

extern int omap4_finish_suspend(unsigned long cpu_state);
extern void omap4_cpu_resume(void);
extern int omap5_finish_suspend(unsigned long cpu_state);
extern void omap5_cpu_resume(void);

static DEFINE_PER_CPU(struct omap4_cpu_pm_info, omap4_pm_info);
static struct powerdomain *mpuss_pd;
static void __iomem *sar_base;
static u32 cpu_context_offset, cpu_cswr_supported;

static int default_finish_suspend(unsigned long cpu_state)
{
	omap_do_wfi();
	return 0;
}

static void dummy_cpu_resume(void)
{}

static void dummy_scu_prepare(unsigned int cpu_id, u8 cpu_state)
{}

static struct cpu_pm_ops omap_pm_ops = {
	.finish_suspend		= default_finish_suspend,
	.resume			= dummy_cpu_resume,
	.scu_prepare		= dummy_scu_prepare,
	.hotplug_restart	= dummy_cpu_resume,
};

/*
 * Program the wakeup routine address for the CPU0 and CPU1
 * used for OFF or DORMANT wakeup.
 */
static inline void set_cpu_wakeup_addr(unsigned int cpu_id, u32 addr)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	/*
	 * XXX should not be writing directly into another IP block's
	 * address space!
	 */
	__raw_writel(addr, pm_info->wkup_sar_addr);
}

/*
 * Store the SCU power status value to scratchpad memory
 */
static void scu_pwrst_prepare(unsigned int cpu_id, u8 fpwrst)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);
	u32 scu_pwr_st;

	switch (fpwrst) {
	case PWRDM_FUNC_PWRST_CSWR:
	case PWRDM_FUNC_PWRST_OSWR: /* XXX is this accurate? */
		scu_pwr_st = SCU_PM_DORMANT;
		break;
	case PWRDM_FUNC_PWRST_OFF:
		scu_pwr_st = SCU_PM_POWEROFF;
		break;
	case PWRDM_FUNC_PWRST_ON:
	case PWRDM_FUNC_PWRST_INACTIVE:
	default:
		scu_pwr_st = SCU_PM_NORMAL;
		break;
	}

	/*
	 * XXX should not be writing directly into another IP block's
	 * address space!
	 */
	__raw_writel(scu_pwr_st, pm_info->scu_sar_addr);
}

/* Helper functions for MPUSS OSWR */
static inline void mpuss_clear_prev_logic_pwrst(void)
{
	u32 reg;

	reg = omap4_prminst_read_inst_reg(OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
	omap4_prminst_write_inst_reg(reg, OMAP4430_PRM_PARTITION,
		OMAP4430_PRM_MPU_INST, OMAP4_RM_MPU_MPU_CONTEXT_OFFSET);
}

static inline void cpu_clear_prev_logic_pwrst(unsigned int cpu_id)
{
	u32 reg;

	if (cpu_id) {
		reg = omap4_prcm_mpu_read_inst_reg(OMAP4430_PRCM_MPU_CPU1_INST,
					cpu_context_offset);
		omap4_prcm_mpu_write_inst_reg(reg, OMAP4430_PRCM_MPU_CPU1_INST,
					cpu_context_offset);
	} else {
		reg = omap4_prcm_mpu_read_inst_reg(OMAP4430_PRCM_MPU_CPU0_INST,
					cpu_context_offset);
		omap4_prcm_mpu_write_inst_reg(reg, OMAP4430_PRCM_MPU_CPU0_INST,
					cpu_context_offset);
	}
}

/*
 * Store the CPU cluster state for L2X0 low power operations.
 */
static void l2x0_pwrst_prepare(unsigned int cpu_id, unsigned int save_state)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu_id);

	/*
	 * XXX should not be writing directly into another IP block's
	 * address space!
	 */
	__raw_writel(save_state, pm_info->l2x0_sar_addr);
}

/*
 * Save the L2X0 AUXCTRL and POR value to SAR memory. Its used to
 * in every restore MPUSS OFF path.
 */
#ifdef CONFIG_CACHE_L2X0
static void save_l2x0_context(void)
{
	u32 val;
	void __iomem *l2x0_base = omap4_get_l2cache_base();
	if (l2x0_base) {
		val = __raw_readl(l2x0_base + L2X0_AUX_CTRL);
		__raw_writel(val, sar_base + L2X0_AUXCTRL_OFFSET);
		val = __raw_readl(l2x0_base + L2X0_PREFETCH_CTRL);
		__raw_writel(val, sar_base + L2X0_PREFETCH_CTRL_OFFSET);
	}
}
#else
static void save_l2x0_context(void)
{}
#endif

/**
 * omap4_mpuss_enter_lowpower: OMAP4 MPUSS Low Power Entry Function
 * The purpose of this function is to manage low power programming
 * of OMAP4 MPUSS subsystem
 * @cpu : CPU ID
 * @fpwrst: functional powerstate for the MPUSS to enter
 *
 * MPUSS states for the context save:
 * save_state =
 *	0 - Nothing lost and no need to save: MPUSS INACTIVE
 *	1 - CPUx L1 and logic lost: MPUSS CSWR
 *	2 - CPUx L1 and logic lost + GIC lost: MPUSS OSWR
 *	3 - CPUx L1 and logic lost + GIC + L2 lost: DEVICE OFF
 */
int omap4_mpuss_enter_lowpower(unsigned int cpu, u8 fpwrst)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu);
	unsigned int save_state = 0;
	unsigned int wakeup_cpu;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		return -ENXIO;

	switch (fpwrst) {
	case PWRDM_FUNC_PWRST_ON:
	case PWRDM_FUNC_PWRST_INACTIVE:
		save_state = 0;
		break;
	case PWRDM_FUNC_PWRST_OFF:
		save_state = 1;
		break;
	case PWRDM_FUNC_PWRST_CSWR:
	case PWRDM_FUNC_PWRST_OSWR:
		if (cpu_cswr_supported) {
			save_state = 0;
			break;
		}
	default:
		/*
		 * CPUx CSWR is invalid hardware state. Also CPUx OSWR
		 * doesn't make much scense, since logic is lost and $L1
		 * needs to be cleaned because of coherency. This makes
		 * CPUx OSWR equivalent to CPUX OFF and hence not supported
		 */
		WARN_ON(1);
		return -ENXIO;
	}

	pwrdm_pre_transition(NULL);

	/*
	 * Check MPUSS next state and save interrupt controller if needed.
	 * In MPUSS OSWR or device OFF, interrupt controller context is lost.
	 */
	mpuss_clear_prev_logic_pwrst();
	if (pwrdm_read_next_fpwrst(mpuss_pd) == PWRDM_FUNC_PWRST_OSWR)
		save_state = 2;

	cpu_clear_prev_logic_pwrst(cpu);
	WARN_ON(pwrdm_set_next_fpwrst(pm_info->pwrdm, fpwrst));
	set_cpu_wakeup_addr(cpu, virt_to_phys(omap_pm_ops.resume));
	omap_pm_ops.scu_prepare(cpu, fpwrst);
	l2x0_pwrst_prepare(cpu, save_state);

	/*
	 * Call low level function  with targeted low power state.
	 */
	if (save_state)
		cpu_suspend(save_state, omap_pm_ops.finish_suspend);
	else
		omap_pm_ops.finish_suspend(save_state);

	/*
	 * Restore the CPUx power state to ON otherwise CPUx
	 * power domain can transitions to programmed low power
	 * state while doing WFI outside the low powe code. On
	 * secure devices, CPUx does WFI which can result in
	 * domain transition
	 */
	wakeup_cpu = smp_processor_id();

	pwrdm_post_transition(NULL);

	WARN_ON(pwrdm_set_next_fpwrst(pm_info->pwrdm, PWRDM_FUNC_PWRST_ON));

	return 0;
}

/**
 * omap4_hotplug_cpu: OMAP4 CPU hotplug entry
 * @cpu : CPU ID
 * @fpwrst: functional power state to program the CPU powerdomain to enter
 */
int __cpuinit omap4_mpuss_hotplug_cpu(unsigned int cpu, u8 fpwrst)
{
	struct omap4_cpu_pm_info *pm_info = &per_cpu(omap4_pm_info, cpu);
	unsigned int cpu_state = 0;

	if (omap_rev() == OMAP4430_REV_ES1_0)
		return -ENXIO;

	if (fpwrst == PWRDM_FUNC_PWRST_OFF || fpwrst == PWRDM_FUNC_PWRST_OSWR)
		cpu_state = 1;

	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	WARN_ON(pwrdm_set_next_fpwrst(pm_info->pwrdm, fpwrst));
	set_cpu_wakeup_addr(cpu, virt_to_phys(omap_pm_ops.hotplug_restart));
	omap_pm_ops.scu_prepare(cpu, fpwrst);

	/*
	 * CPU never retuns back if targeted power state is OFF mode.
	 * CPU ONLINE follows normal CPU ONLINE ptah via
	 * omap_secondary_startup().
	 */
	omap_pm_ops.finish_suspend(cpu_state);

	WARN_ON(pwrdm_set_next_fpwrst(pm_info->pwrdm, PWRDM_FUNC_PWRST_ON));
	return 0;
}


/*
 * Enable Mercury Fast HG retention mode by default.
 */
static void enable_mercury_retention_mode(void)
{
	u32 reg;

	reg = omap4_prcm_mpu_read_inst_reg(OMAP54XX_PRCM_MPU_DEVICE_INST,
			OMAP54XX_PRCM_MPU_PRM_PSCON_COUNT_OFFSET);
	reg |= BIT(24) | BIT(25);
	omap4_prcm_mpu_write_inst_reg(reg, OMAP54XX_PRCM_MPU_DEVICE_INST,
			OMAP54XX_PRCM_MPU_PRM_PSCON_COUNT_OFFSET);
}

/*
 * Initialise OMAP4 MPUSS
 */
int __init omap4_mpuss_init(void)
{
	struct omap4_cpu_pm_info *pm_info;
	u32 cpu_wakeup_addr = 0;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		WARN(1, "Power Management not supported on OMAP4430 ES1.0\n");
		return -ENODEV;
	}

	sar_base = omap4_get_sar_ram_base();

	/* Initilaise per CPU PM information */
	if (cpu_is_omap44xx())
		cpu_wakeup_addr = CPU0_WAKEUP_NS_PA_ADDR_OFFSET;
	else if (soc_is_omap54xx())
		cpu_wakeup_addr = OMAP5_CPU0_WAKEUP_NS_PA_ADDR_OFFSET;
	pm_info = &per_cpu(omap4_pm_info, 0x0);
	pm_info->scu_sar_addr = sar_base + SCU_OFFSET0;
	pm_info->wkup_sar_addr = sar_base + cpu_wakeup_addr;
	pm_info->l2x0_sar_addr = sar_base + L2X0_SAVE_OFFSET0;
	pm_info->pwrdm = pwrdm_lookup("cpu0_pwrdm");
	if (!pm_info->pwrdm) {
		pr_err("Lookup failed for CPU0 pwrdm\n");
		return -ENODEV;
	}

	/* Clear CPU previous power domain state */
	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	cpu_clear_prev_logic_pwrst(0);

	/* Initialise CPU0 power domain state to ON */
	WARN_ON(pwrdm_set_next_fpwrst(pm_info->pwrdm, PWRDM_FUNC_PWRST_ON));

	if (cpu_is_omap44xx())
		cpu_wakeup_addr = CPU1_WAKEUP_NS_PA_ADDR_OFFSET;
	else if (soc_is_omap54xx())
		cpu_wakeup_addr = OMAP5_CPU1_WAKEUP_NS_PA_ADDR_OFFSET;
	pm_info = &per_cpu(omap4_pm_info, 0x1);
	pm_info->scu_sar_addr = sar_base + SCU_OFFSET1;
	pm_info->wkup_sar_addr = sar_base + cpu_wakeup_addr;
	pm_info->l2x0_sar_addr = sar_base + L2X0_SAVE_OFFSET1;

	pm_info->pwrdm = pwrdm_lookup("cpu1_pwrdm");
	if (!pm_info->pwrdm) {
		pr_err("Lookup failed for CPU1 pwrdm\n");
		return -ENODEV;
	}

	/* Clear CPU previous power domain state */
	pwrdm_clear_all_prev_pwrst(pm_info->pwrdm);
	cpu_clear_prev_logic_pwrst(1);

	/* Initialise CPU1 power domain state to ON */
	WARN_ON(pwrdm_set_next_fpwrst(pm_info->pwrdm, PWRDM_FUNC_PWRST_ON));

	mpuss_pd = pwrdm_lookup("mpu_pwrdm");
	if (!mpuss_pd) {
		pr_err("Failed to lookup MPUSS power domain\n");
		return -ENODEV;
	}
	pwrdm_clear_all_prev_pwrst(mpuss_pd);
	mpuss_clear_prev_logic_pwrst();

	/* Save device type on scratchpad for low level code to use */
	if (omap_type() != OMAP2_DEVICE_TYPE_GP)
		__raw_writel(1, sar_base + OMAP_TYPE_OFFSET);
	else
		__raw_writel(0, sar_base + OMAP_TYPE_OFFSET);

	save_l2x0_context();

	if (cpu_is_omap44xx()) {
		omap_pm_ops.finish_suspend = omap4_finish_suspend;
		omap_pm_ops.hotplug_restart = omap_secondary_startup;
		omap_pm_ops.resume = omap4_cpu_resume;
		omap_pm_ops.scu_prepare = scu_pwrst_prepare;
		cpu_context_offset = OMAP4_RM_CPU0_CPU0_CONTEXT_OFFSET;
	} else if (soc_is_omap54xx()) {
		omap_pm_ops.finish_suspend = omap5_finish_suspend;
		omap_pm_ops.hotplug_restart = omap5_secondary_startup;
		omap_pm_ops.resume = omap5_cpu_resume;
		cpu_context_offset = OMAP54XX_RM_CPU0_CPU0_CONTEXT_OFFSET;
		enable_mercury_retention_mode();
		cpu_cswr_supported = 1;
	}

	if (cpu_is_omap446x())
		omap_pm_ops.hotplug_restart = omap_secondary_startup_4460;

	return 0;
}

#endif
