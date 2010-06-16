/*
 * OMAP4 MPUSS low power code
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Author:
 *      Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * OMAP4430 MPUSS mainly consist of dual Cortex-A9 with per-cpu
 * Local timer and Watchdog, GIC, SCU, PL310 L2 cache controller,
 * CPU0 and CPU1 LPRM modules.
 * CPU0, CPU1 and MPUSS have there own power domain and
 * hence multiple low power combinations of MPUSS are possible.
 *
 * The CPU0 and CPU1 can't support Close switch Retention (CSWR)
 * because the mode is not supported by hw constraints of dormant
 * mode. While waking up from the dormant modei,a reset  signal
 * to the Cortex-A9 processor must be asserted by the external
 * power control mechanism
 *
 * With architectural inputs and hardware recommendations, only
 * below modes are supported from power gain vs latency point of view.
 *
 *	CPU0		CPU1		MPUSS
 *	----------------------------------------------
 *	ON(Inactive)	ON(Inactive)	ON(Inactive)
 *	ON(Inactive)	OFF		ON(Inactive)
 *	OFF		OFF		CSWR
 *	OFF		OFF		OSWR (*TBD)
 *	OFF		OFF		OFF* (*TBD)
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
#include <linux/smp.h>
#include <asm/tlbflush.h>
#include <asm/smp_scu.h>

#include <plat/powerdomain.h>
#include <mach/omap4-common.h>

#ifdef CONFIG_SMP
/*
 * CPUx Wakeup Non-Secure Physical Address offsets
 */
#define CPU0_WAKEUP_NS_PA_ADDR_OFFSET		0xa04
#define CPU1_WAKEUP_NS_PA_ADDR_OFFSET		0xa08

/*
 * Scratch pad memory offsets for temorary usages
 */
#define TABLE_ADDRESS_OFFSET			0x01
#define TABLE_VALUE_OFFSET			0x00
#define CR_VALUE_OFFSET				0x02


static struct powerdomain *cpu0_pwrdm, *cpu1_pwrdm;

/*
 * Program the wakeup routine address for the CPU's
 * from OFF/OSWR
 */
static inline void setup_wakeup_routine(unsigned int cpu_id)
{
	if (cpu_id)
		writel(virt_to_phys(omap4_cpu_wakeup_addr()),
				sar_ram_base + CPU1_WAKEUP_NS_PA_ADDR_OFFSET);
	else
		writel(virt_to_phys(omap4_cpu_wakeup_addr()),
				sar_ram_base + CPU0_WAKEUP_NS_PA_ADDR_OFFSET);
}

/*
 * Read CPU's previous power state
 */
static inline unsigned int read_cpu_prev_pwrst(unsigned int cpu_id)
{
	if (cpu_id)
		return pwrdm_read_prev_pwrst(cpu1_pwrdm);
	else
		return pwrdm_read_prev_pwrst(cpu0_pwrdm);
}

/*
 * Clear the CPUx powerdomain's previous power state
 */
static inline void clear_cpu_prev_pwrst(unsigned int cpu_id)
{
	if (cpu_id)
		pwrdm_clear_all_prev_pwrst(cpu1_pwrdm);
	else
		pwrdm_clear_all_prev_pwrst(cpu0_pwrdm);
}

/*
 * Function to restore the table entry that
 * was modified for enabling MMU
 */
static void restore_mmu_table_entry(void)
{
	u32 *scratchpad_address;
	u32 previous_value, control_reg_value;
	u32 *address;

	/*
	 * Get address of entry that was modified
	 */
	scratchpad_address = sar_ram_base + MMU_OFFSET;
	address = (u32 *)readl(scratchpad_address + TABLE_ADDRESS_OFFSET);
	/*
	 * Get the previous value which needs to be restored
	 */
	previous_value = readl(scratchpad_address + TABLE_VALUE_OFFSET);
	address = __va(address);
	*address = previous_value;
	flush_tlb_all();
	control_reg_value = readl(scratchpad_address + CR_VALUE_OFFSET);
	/*
	 * Restore the Control register
	 */
	set_cr(control_reg_value);
}

/*
 * Program the CPU power state using SCU power state register
 */
static void scu_pwrst_prepare(unsigned int cpu_id, unsigned int cpu_state)
{
	u32 scu_pwr_st, regvalue;

	switch (cpu_state) {
	case PWRDM_POWER_RET:
		/* DORMANT */
		scu_pwr_st = 0x02;
		break;
	case PWRDM_POWER_OFF:
		scu_pwr_st = 0x03;
		break;
	default:
		/* Not supported */
		return;
	}

	regvalue = readl(scu_base + SCU_CPU_STATUS);
	if (cpu_id)
		regvalue |= scu_pwr_st << 8;
	else
		regvalue |= scu_pwr_st;

	/*
	 * Store the SCU power status value
	 * to scratchpad memory
	 */
	writel(regvalue, sar_ram_base + SCU_OFFSET);
}

/*
 * OMAP4 MPUSS Low Power Entry Function
 *
 * The purpose of this function is to manage low power programming
 * of OMAP4 MPUSS subsystem
 * Paramenters:
 *	cpu : CPU ID
 *	power_state: Targetted Low power state.
 */
void omap4_enter_lowpower(unsigned int cpu, unsigned int power_state)
{
	unsigned int save_state, wakeup_cpu;

	if (cpu > NR_CPUS)
		return;

	/*
	 * Low power state not supported on ES1.0 silicon
	 */
	if (omap_rev() == OMAP4430_REV_ES1_0) {
		wmb();
		do_wfi();
		return;
	}

	switch (power_state) {
	case PWRDM_POWER_ON:
		save_state = 0;
		break;
	case PWRDM_POWER_OFF:
		save_state = 1;
		setup_wakeup_routine(cpu);
		break;
	case PWRDM_POWER_RET:
		/*
		 * CPUx CSWR is invalid hardware state. Additionally
		 * CPUx OSWR  doesn't give any gain vs CPUxOFF and
		 * hence not supported
		 */
	default:
		/* Invalid state */
		pr_debug("Invalid CPU low power state\n");
		return;
	}

	/*
	 * Program the CPU targeted state
	 */
	clear_cpu_prev_pwrst(cpu);
	if (cpu)
		pwrdm_set_next_pwrst(cpu1_pwrdm, power_state);
	else
		pwrdm_set_next_pwrst(cpu0_pwrdm, power_state);
	scu_pwrst_prepare(cpu, power_state);

	/*
	 * Call low level routine to enter to
	 * targeted power state
	 */
	__omap4_cpu_suspend(cpu, save_state);

	/*
	 * Check the CPUx previous power state
	 */
	wakeup_cpu = hard_smp_processor_id();
	if (read_cpu_prev_pwrst(wakeup_cpu) == PWRDM_POWER_OFF) {
		cpu_init();
		restore_mmu_table_entry();
	}
}

void __init omap4_mpuss_init(void)
{
	cpu0_pwrdm = pwrdm_lookup("cpu0_pwrdm");
	cpu1_pwrdm = pwrdm_lookup("cpu1_pwrdm");
	if (!cpu0_pwrdm || !cpu1_pwrdm)
		pr_err("Failed to get lookup for CPUx pwrdm's\n");
}

#else

void omap4_enter_lowpower(unsigned int cpu, unsigned int power_state)
{
		wmb();
		do_wfi();
		return;
}
void __init omap4_mpuss_init(void)
{
}
#endif
