/*
 * arch/arm/mach-omap2/include/mach/secure-regs.S
 *
 * Low-level IRQ helper macros for OMAP-based platforms
 *
 * Copyright (C) 2009 Texas Instruments
 *
 *
 * This file is licensed under  the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

/*
 * Macro to set the secure registers on OMAP5.
 *
 */
	.macro  set_secure_regs, base, value
	stmia	r12, {r1-r11, r13-r14}
	mov	r13, r12
	/*
	 * Work around for errata for 761171.
	 * Streaming write that will not allocate in L2
	 * could lead to data corruption.
	 */
#if defined(CONFIG_ARM_ERRATA_761171) || defined(CONFIG_ARM_ERRATA_763722) \
	|| defined(CONFIG_ARM_ERRATA_775619) || defined(CONFIG_ARM_ERRATA_773022)

	cmp	r6, #0x03		@ Present before r0p3, fixed later
	bgt	1f
	mrc p15, 0, r0, c1, c0, 1	@ Read Auxiliary Control Register
#ifdef CONFIG_ARM_ERRATA_763722
	orr r0, r0, #0x3 << 25
#endif
	orr r0, r0, #0x3 << 27		@ set bits[28:27], L1_mode3_threshold to 0x3.

#ifdef CONFIG_ARM_ERRATA_775619
	orr r0, r0, #0x1 << 23		@ Forces in-order requests to the same set and way
#endif

#ifdef CONFIG_ARM_ERRATA_773022
	orr r0, r0, #0x1 << 1		@ Disable Loop Buffer
#endif

	ldr	\base,	=0x107
	smc	#0
#endif
#ifdef CONFIG_ARM_ERRATA_774769
	cmp	r6, #0x4			@ present in r0p4
	bgt	1f
	mrc p15, 0, r0, c1, c0, 1		@ read aux control register
	bic r0, r0, #7 << 26		@ clear bits #28-26
	orr r0, r0, #1 << 25		@ set bit #25
	ldr	\base, =0x107
	smc 	#0
#endif
#ifdef CONFIG_ARM_ERRATA_773022
	cmp	r6, #0x4			@ present in r0p4
	bgt	1f
	mrc p15, 0, r0, c1, c0, 1		@ read aux control register
	orr r0, r0, #1 << 1			@ set bit #1
	ldr	\base, =0x107
	smc	#0
#endif

#ifdef CONFIG_ARM_ERRATA_762974
	cmp	r6, #0x03		@ Present before r0p3, fixed later
	bgt	1f
	mov	r0, #0x400		@ L2 prefetch completely
	ldr	\base, =0x106
	smc	#0
#endif

1:
	/*
	 * Setting the L2 ACTLR register for recommended value to get the optimum
	 * performance.
	 */
	ldr	\base, =0x104
	ldr	\value, =0x118
	smc	#0
	mov	r12, r13
	ldmia	r12, {r1-r11, r13-r14}
	.endm
