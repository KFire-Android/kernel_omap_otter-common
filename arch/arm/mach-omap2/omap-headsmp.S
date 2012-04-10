/*
 * Secondary CPU startup routine source file.
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author:
 *      Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Interface functions needed for the SMP. This file is based on arm
 * realview smp platform.
 * Copyright (c) 2003 ARM Limited.
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <linux/init.h>

#include <plat/omap44xx.h>
#include <mach/omap-secure.h>
#include <asm/smp_scu.h>

#include "prcm_mpu44xx.h"

#define CPU1_PWRSTCTRL (OMAP4430_PRCM_MPU_BASE + OMAP4430_PRCM_MPU_CPU1_INST + \
			OMAP4_PM_CPU1_PWRSTCTRL_OFFSET)

	__CPUINIT

/* Physical address needed since MMU not enabled yet on secondary core */
#define AUX_CORE_BOOT0_PA			0x48281800

/*
 * OMAP5 specific entry point for secondary CPU to jump from ROM
 * code.  This routine also provides a holding flag into which
 * secondary core is held until we're ready for it to initialise.
 * The primary core will update this flag using a hardware
+ * register AuxCoreBoot0.
 */
ENTRY(omap5_secondary_startup)
wait:	ldr	r2, =AUX_CORE_BOOT0_PA	@ read from AuxCoreBoot0
	ldr	r0, [r2]
	mov	r0, r0, lsr #5
	mrc	p15, 0, r4, c0, c0, 5
	and	r4, r4, #0x0f
	cmp	r0, r4
	bne	wait
	b	secondary_startup
END(omap5_secondary_startup)

/*
 * Put CPU1 to off mode.
 * Programs CPU1_PWRSTCTRL to OFF via secure API, and enters WFI which
 * triggers the HW transition. omap_do_wfi() can't be used as we don't
 * have MMU enabled and neither do we have stack at this point.
 */
ENTRY(omap_cpu1_off)
	/*
	 * If we are running from a cold boot, we can't enter idle
	 * yet as PRCM does not contain valid programming for it.
	 * Thus, we check the CPU1_PWRSTCTRL and see whether it contains
	 * reset value (POWERSTATE=2), and if this is the case, exit early
	 */
	ldr	r12, =CPU1_PWRSTCTRL
	ldr	r0, [r12]
	and	r0, #3
	cmp	r0, #2
	beq	exit_cpu1_off		@ can't idle yet, exit

	/*
	 * Program SCU power state for this CPU to POWEROFF. Secure API
	 * is used for this.
	 */
	mov	r0, #SCU_PM_POWEROFF
	mov	r1, #0x0
	ldr	r12, =OMAP4_MON_SCU_PWR_INDEX
	dsb
	smc     #0
	dsb

	isb
	dsb
	dmb
	/*
	 * Execute a WFI instruction to enter OFF.
	 * Sixteen NOPs added after WFI to prevent speculative
	 * pipeline execution.
	 */
	wfi
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
exit_cpu1_off:
	mov	pc, lr
ENDPROC(omap_cpu1_off)

/*
 * OMAP4 specific entry point for secondary CPU to jump from ROM
 * code.  This routine also provides a holding flag into which
 * secondary core is held until we're ready for it to initialise.
 * The primary core will update this flag using a hardware
 * register AuxCoreBoot0.
 */
ENTRY(omap_secondary_startup)
	ldr	r12,=0x103
	dsb
	smc	#0			@ read from AuxCoreBoot0
	mov	r0, r0, lsr #9
	mrc	p15, 0, r4, c0, c0, 5
	and	r4, r4, #0x0f
	cmp	r0, r4
	beq	omap_cont_boot

	bl	omap_cpu1_off
	b	omap_secondary_startup

	/*
	 * we've been released from the wait loop,secondary_stack
	 * should now contain the SVC stack for this core
	 */
omap_cont_boot:
	b	secondary_startup
ENDPROC(omap_secondary_startup)

ENTRY(omap_secondary_startup_4460)
	ldr	r12,=0x103
	dsb
	smc	#0			@ read from AuxCoreBoot0
	mov	r0, r0, lsr #9
	mrc	p15, 0, r4, c0, c0, 5
	and	r4, r4, #0x0f
	cmp	r0, r4
	beq	omap4460_cont_boot

	bl	omap_cpu1_off
	b	omap_secondary_startup_4460

omap4460_cont_boot:
	/*
	 * GIC distributor control register has changed between
	 * CortexA9 r1pX and r2pX. The Control Register secure
	 * banked version is now composed of 2 bits:
	 * bit 0 == Secure Enable
	 * bit 1 == Non-Secure Enable
	 * The Non-Secure banked register has not changed
	 * Because the ROM Code is based on the r1pX GIC, the CPU1
	 * GIC restoration will cause a problem to CPU0 Non-Secure SW.
	 * The workaround must be:
	 * 1) Before doing the CPU1 wakeup, CPU0 must disable
	 * the GIC distributor
	 * 2) CPU1 must re-enable the GIC distributor on
	 * it's wakeup path.
	 */
	ldr	r1, =OMAP44XX_GIC_DIST_BASE
	ldr	r0, [r1]
	orr	r0, #1
	str	r0, [r1]

	/*
	 * we've been released from the wait loop,secondary_stack
	 * should now contain the SVC stack for this core
	 */
	b	secondary_startup
ENDPROC(omap_secondary_startup_4460)
