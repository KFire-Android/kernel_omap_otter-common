/*
 * OMAP44xx Low level save/restore file.
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Written by Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 *
 * This program is free software,you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/linkage.h>
#include <asm/system.h>
#include <asm/hardware/cache-l2x0.h>
#include <mach/omap4-common.h>
#include <plat/omap44xx.h>

#ifdef CONFIG_SMP

/*
 * Masks used for MMU manipulation
 */
#define TTRBIT_MASK				0xFFFFC000
#define TABLE_INDEX_MASK			0xFFF00000
#define TABLE_ENTRY				0x00000C02
#define CACHE_DISABLE_MASK			0xFFFFE7FB
#define SCU_CLEAR_STATE				0xFCFC

/*
 * CPUx Wakeup Non-Secure Physical Address for
 * resume from OSWR/OFF
 */
ENTRY(omap4_cpu_wakeup_addr)
	stmfd   sp!, {lr}		@ save registers on stack
	adr	r0, restore_context
	ldmfd   sp!, {pc}		@ restore regs and return
END(omap4_cpu_wakeup_addr)


/*
 * void __omap4_cpu_suspend(unsigned int cpu, unsigned int save_state)
 * r0 contains cpu id
 * r1 contains information about context save state
 */
ENTRY(__omap4_cpu_suspend)
	stmfd	sp!, {r0-r12, lr}	@ save registers on stack
	cmp	r1, #0x0
	beq	do_WFI
	bne	context_save		@ Save context if needed

restore_context:
        /*
	 * Check  the wakeup CPU
	 */
#ifdef CONFIG_CACHE_L2X0
	ldr	r2, =OMAP44XX_L2CACHE_BASE
	ldr	r0, [r2, #L2X0_CTRL]
	and	r0, #0x0f
	cmp	r0, #1
	beq	skip_l2en
	mov     r0, #0x1
	ldr     r12, =0x102		@ Enable L2 Cache controller
	dsb
	smc     #0
	dsb
skip_l2en:
#endif
	ldr	r3, =OMAP44XX_SAR_RAM_BASE
	mov	r1, #0
	mcr	p15, 0, r1, c7, c5, 0	@ Invalidate $I to PoU
	mrc	p15, 0, r0, c0, c0, 5	@ MPIDR
	ands	r0, r0, #0x0f
	orreq	r3, r3, #CPU0_SAVE_OFFSET
	orrne	r3, r3, #CPU1_SAVE_OFFSET

	ldmia	r3!, {r4-r6}
	mov	sp, r4			@ Restore sp
	msr	spsr_cxsf, r5		@ Restore spsr
	mov	lr, r6			@ Restore lr

	ldmia	r3!, {r4-r7}
	mcr	p15, 0, r4, c1, c0, 2	@ Coprocessor access Control Register
	mcr	p15, 0, r5, c2, c0, 0	@ TTBR0
	mcr	p15, 0, r6, c2, c0, 1	@ TTBR1
	mcr	p15, 0, r7, c2, c0, 2	@ TTBCR

	ldmia	r3!,{r4-r6}
	mcr	p15, 0, r4, c3, c0, 0	@ Domain access Control Register
	mcr	p15, 0, r5, c10, c2, 0	@ PRRR
	mcr	p15, 0, r6, c10, c2, 1	@ NMRR

	ldmia	r3!,{r4-r7}
	mcr	p15, 0, r4, c13, c0, 1	@ Context ID
	mcr	p15, 0, r5, c13, c0, 2	@ User r/w thread and process ID
	mcr	p15, 0, r6, c13, c0, 3	@ User ro thread and process ID
	mcr	p15, 0, r7, c13, c0, 4	@ Privilege only thread and process ID

	ldmia	r3!,{r4,r5}
	mrc	p15, 0, r4, c12, c0, 0	@ Secure or NS vector base address
	msr	cpsr, r5		@ store cpsr

	/*
	 * Enabling MMU here. Page entry needs to be altered to create
	 * temprary one is to one map and then resore the entry ones
	 * MMU is enabled
	 */
	mrc	p15, 0, r7, c2, c0, 2	@ Read TTBRControl
	and	r7, #0x7		@ Extract N (0:2) to decide TTBR0/TTBR1
	cmp	r7, #0x0
	beq	use_ttbr0
ttbr_error:
	b	ttbr_error		@ Only N = 0 supported for now
use_ttbr0:
	mrc	p15, 0, r2, c2, c0, 0	@ Read TTBR0
	ldr	r5, =TTRBIT_MASK
	and	r2, r5
	mov	r4, pc
	ldr	r5, =TABLE_INDEX_MASK
	and	r4, r5			@ r4 = 31 to 20 bits of pc
	ldr	r1, =TABLE_ENTRY
	add	r1, r1, r4 		@ r1 has value of table entry
	lsr	r4, #18			@ Address of table entry
	add	r2, r4			@ r2 - location needs to be modified
	/*
	 * Storing previous entry of location being modified
	 */
	ldr     r5, =OMAP44XX_SAR_RAM_BASE
	ldr	r4, [r2]
	str	r4, [r5, #MMU_OFFSET]
	str	r1, [r2]		@ Modify the table entry
	/*
	 * Storing address of entry being modified
	 * It will be restored after enabling MMU
	 */
	ldr     r5, =OMAP44XX_SAR_RAM_BASE
	orr	r5, r5, #MMU_OFFSET
	str	r2, [r5, #0x04]
	mov	r0, #0
	mcr	p15, 0, r0, c7, c5, 4	@ Flush prefetch buffer
	mcr	p15, 0, r0, c7, c5, 6	@ Invalidate branch predictor array
	mcr	p15, 0, r0, c8, c5, 0	@ Invalidate instruction TLB
	mcr	p15, 0, r0, c8, c6, 0	@ Invalidate data TLB

	/*
	 * Restore control register  but dont enable caches here
	 * Caches will be enabled after restoring MMU table entry
	 */
	ldmia	r3!, {r4}
	str	r4, [r5, #0x8]		@ Store previous value of CR
	ldr	r2, =CACHE_DISABLE_MASK
	and	r4, r2
	mcr	p15, 0, r4, c1, c0, 0

	ldmfd	sp!, {r0-r12, pc}	@ restore regs and return

context_save:
	/*
	 * Check the targeted CPU and MPUSS
	 * state to derive L2 state
	 * 1 - CPUx L1 and logic lost: MPUSS CSWR
	 * 2 - CPUx L1 and logic lost + GIC lost: MPUSS OSWR
	 * 3 - CPUx L1 and logic lost + GIC + L2 lost: MPUSS OFF
	 */
	ldr	r8, =sar_ram_base
	ldr	r8, [r8]
	str	r1, [r8, #L2X0_OFFSET]
	ands	r0, r0, #0x0f
	orreq	r8, r8, #CPU0_SAVE_OFFSET
	orrne	r8, r8, #CPU1_SAVE_OFFSET

	mov	r4, sp			@ Store sp
	mrs	r5, spsr		@ Store spsr
	mov	r6, lr			@ Store lr
	stmia	r8!, {r4-r6}

	mrc	p15, 0, r4, c1, c0, 2	@ Coprocessor access control register
	mrc	p15, 0, r5, c2, c0, 0	@ TTBR0
	mrc	p15, 0, r6, c2, c0, 1	@ TTBR1
	mrc	p15, 0, r7, c2, c0, 2	@ TTBCR
	stmia	r8!, {r4-r7}

	mrc	p15, 0, r4, c3, c0, 0	@ Domain access Control Register
	mrc	p15, 0, r5, c10, c2, 0	@ PRRR
	mrc	p15, 0, r6, c10, c2, 1	@ NMRR
	stmia	r8!,{r4-r6}

	mrc	p15, 0, r4, c13, c0, 1	@ Context ID
	mrc	p15, 0, r5, c13, c0, 2	@ User r/w thread and process ID
	mrc	p15, 0, r6, c13, c0, 3	@ User ro thread and process ID
	mrc	p15, 0, r7, c13, c0, 4	@ Privilege only thread and process ID
	stmia	r8!, {r4-r7}

	mrc	p15, 0, r4, c12, c0, 0	@ Secure or NS vector base address
	mrs	r5, cpsr		@ Store current cpsr
	stmia	r8!, {r4,r5}

	mrc	p15, 0, r4, c1, c0, 0	@ save control register
	stmia	r8!, {r4}

clean_inv_l1:				@ cache-v7.S routine used here
	dmb				@ ensure ordering with previous accesses
	mrc	p15, 1, r0, c0, c0, 1	@ read clidr
	ands	r3, r0, #0x7000000	@ extract loc from clidr
	mov	r3, r3, lsr #23		@ left align loc bit field
	beq	finished		@ if loc is 0, then no need to clean
	mov	r10, #0			@ start clean at cache level 0
loop1:
	add	r2, r10, r10, lsr #1	@ work out 3x current cache level
	mov	r1, r0, lsr r2		@ extract cache type bits from clidr
	and	r1, r1, #7		@ mask bits for current cache only
	cmp	r1, #2			@ see what cache we have at this level
	blt	skip			@ skip if no cache, or just i-cache
	mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
	isb				@ isb to sych the new cssr&csidr
	mrc	p15, 1, r1, c0, c0, 0	@ read the new csidr
	and	r2, r1, #7		@ extract the length of the cache lines
	add	r2, r2, #4		@ add 4 (line length offset)
	ldr	r4, =0x3ff
	ands	r4, r4, r1, lsr #3	@ find maximum number on the way size
	clz	r5, r4			@ find bit position way size increment
	ldr	r7, =0x7fff
	ands	r7, r7, r1, lsr #13	@ extract max number of the index size
loop2:
	mov	r9, r4			@ create working copy of max way size
loop3:
	orr	r11, r10, r9, lsl r5	@ factor way and cache number into r11
	orr	r11, r11, r7, lsl r2	@ factor index number into r11
	mcr	p15, 0, r11, c7, c14, 2	@ clean & invalidate by set/way
	subs	r9, r9, #1		@ decrement the way
	bge	loop3
	subs	r7, r7, #1		@ decrement the index
	bge	loop2
skip:
	add	r10, r10, #2		@ increment cache number
	cmp	r3, r10
	bgt	loop1
finished:
	mov	r10, #0			@ swith back to cache level 0
	mcr	p15, 2, r10, c0, c0, 0	@ select current cache level in cssr
	dsb
	isb
	mov	r0, #0
	mcr	p15, 0, r0, c7, c1, 0	@ ICIALLUIS
	mcr	p15, 0, r0, c7, c1, 6	@ BPIALLIS
	mcr	p15, 0, r0, c8, c3, 0
#ifdef CONFIG_CACHE_L2X0
	mrc	p15, 0, r0, c0, c0, 5	@ MPIDR
	ands	r0, r0, #0x0f
	bne	do_WFI
l2x_clean:				@ Only for MPU OFF and last CPU
	ldr	r8, =sar_ram_base
	ldr	r8, [r8]
	ldr	r0, [r8, #L2X0_OFFSET]
	cmp	r0, #3
	bne	do_WFI
	ldr	r2, =l2cache_base
	ldr	r2, [r2]
	mov	r0, #0xff
	str	r0, [r2, #L2X0_CLEAN_WAY]
loop4:
	ldr	r0, [r2, #L2X0_CLEAN_WAY]
	ands	r0, r0, #0xff
	bne	loop4
#endif
do_WFI:
	ldr	r4, =sar_ram_base
	ldr	r4, [r4]
	ldr     r9, [r4, #OMAP_TYPE_OFFSET]
	cmp	r9, #0x1		@ Check for HS device
	bne	scu_gp_set
	orr	r4, r4, #SCU_OFFSET
	ldr     r0, [r4, #0x04]
	ldr     r1, [r4, #0x08]
	stmfd   r13!, {r4-r12, r14}
	ldr	r12, =0x108		@ SCU power state secure API
	smc	#0
	ldmfd   r13!, {r4-r12, r14}
	b	skip_gp_set
scu_gp_set:
	ldr     r3, [r4, #SCU_OFFSET]
	ldr	r2, =scu_base		@ Take CPUx out of coherency
	ldr	r2, [r2]
	str	r3, [r2, #0x08]
skip_gp_set:
	isb
	dsb				@ Necessary barriers before wfi
	dmb
#ifdef CONFIG_CACHE_L2X0
l2x_sync:
	ldr	r1, =l2cache_base
	ldr	r1, [r1]
	mov	r0, #0x0
	str	r0, [r1, #L2X0_CACHE_SYNC]
loop5:
	ldr	r0, [r1, #L2X0_CACHE_SYNC]
	ands	r0, r0, #0x1
	bne	loop5
#endif
	wfi				@ wait for interrupt

	/*
	 * CPUx didn't hit targeted low power state.
	 * Clear SCU power status. Both CPU bits needs
	 * to be cleared o.w CPUs may deadlock because
	 * of coherency
	 */
	cmp	r9, #0x1
	bne	scu_gp_clear
	mov     r0, #0x00
	mov     r1, #0xff
	stmfd   r13!, {r4-r12, r14}
	ldr	r12, =0x108		@ SCU power state secure API
	smc	#0
	ldmfd   r13!, {r4-r12, r14}
	b	skip_gp
scu_gp_clear:
	ldr	r3, =SCU_CLEAR_STATE
	str	r3, [r2, #0x08]
skip_gp:
	dsb				@ Issue a write memory barrier
#ifdef CONFIG_CACHE_L2X0
	mov	r0, #0x0
	str	r0, [r1, #L2X0_CACHE_SYNC]
loop6:
	ldr	r0, [r1, #L2X0_CACHE_SYNC]
	ands	r0, r0, #0x1
	bne	loop6
#endif
	ldmfd	sp!, {r0-r12, pc}	@ restore regs and return

END(__omap4_cpu_suspend)

#endif
