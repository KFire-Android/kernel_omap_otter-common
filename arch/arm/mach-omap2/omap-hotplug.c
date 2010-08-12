/*
 * OMAP4 SMP cpu-hotplug support
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Author:
 *      Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Platform file needed for the OMAP4 SMP. This file is based on arm
 * realview smp platform.
 * Copyright (c) 2002 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/smp.h>
#include <linux/completion.h>

#include <asm/cacheflush.h>
#include <mach/omap4-common.h>
#include <mach/omap4-wakeupgen.h>
#include <plat/powerdomain.h>
#include <plat/clockdomain.h>

static DECLARE_COMPLETION(cpu_killed);

int platform_cpu_kill(unsigned int cpu)
{
	int ret;

	ret = wait_for_completion_timeout(&cpu_killed, 5000);
	pr_notice("CPU%u: shutdown\n", cpu);

	return ret;
}

/*
 * platform-specific code to shutdown a CPU
 * Called with IRQs disabled
 */
void platform_cpu_die(unsigned int cpu)
{
	unsigned int this_cpu = hard_smp_processor_id();
	struct clockdomain *cpu1_clkdm;

	if (cpu != this_cpu) {
		pr_crit("platform_cpu_die running on %u, should be %u\n",
			   this_cpu, cpu);
		BUG();
	}
	complete(&cpu_killed);
	flush_cache_all();
	wmb();

	/*
	 * we're ready for shutdown now, so do it
	 */
	if (omap_modify_auxcoreboot0(0x0, 0x200) != 0x0)
		pr_err("Secure clear status failed\n");

	for (;;) {
		/*
		 * Enter into low power state
		 * clear all interrupt wakeup sources
		 */
		omap4_wakeupgen_clear_all(cpu);
#ifdef CONFIG_PM
		omap4_enter_lowpower(cpu, PWRDM_POWER_OFF);
#else
		wmb();
		do_wfi();
#endif
		if (omap_read_auxcoreboot0() == cpu) {
			/*
			 * OK, proper wakeup, we're done
			 */
			this_cpu = hard_smp_processor_id();
			omap4_wakeupgen_set_all(this_cpu);

			/*
			 * Restore clock domain to HW_AUTO
			 */
			cpu1_clkdm = clkdm_lookup("mpu1_clkdm");
			omap2_clkdm_allow_idle(cpu1_clkdm);
			break;
		}
		pr_debug("CPU%u: spurious wakeup call\n", cpu);
	}
}

int platform_cpu_disable(unsigned int cpu)
{
	/*
	 * we don't allow CPU 0 to be shutdown (it is still too special
	 * e.g. clock tick interrupts)
	 */
	return cpu == 0 ? -EPERM : 0;
}
