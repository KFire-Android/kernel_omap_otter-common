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
	ldr	\base, =0x104
	ldr	\value, =0x118
	smc	#0
	mov	r12, r13
	ldmia	r12, {r1-r11, r13-r14}
	.endm
