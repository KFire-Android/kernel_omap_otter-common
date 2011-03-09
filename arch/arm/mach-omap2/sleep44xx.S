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
#include <asm/memory.h>

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
 * Macro to call PPA svc when MMU is OFF
 * Caller must setup r0 and r3 before calling this macro
 * @r0: PPA service ID
 * @r3: Pointer to params
*/
.macro LM_CALL_PPA_SERVICE_PA
	mov	r1, #0x0        @ Process ID
	mov	r2, #0x4	@ Flag
	mov	r6, #0xff
	mov	r12, #0x00      @ Secure Service ID
	dsb
	smc     #0
.endm

/*
 * Macro to check CpuID
 */
.macro LM_CHECK_IF_CPU0
	mrc	p15, 0, r0, c0, c0, 5		@ Get cpuID
	ands	r0, r0, #0x0f
.endm

.equ LOCAL_VA2PA_OFFSET,		(PHYS_OFFSET-PAGE_OFFSET)

zero_params:
.word 0

POR_params:
.word 1, 5

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
	 * Available for HS devices only:
	 * Enable NS access to SMP for CPU1
	 * CPU0 is setup in PPA
	 */
	ldr	r4, =OMAP44XX_SAR_RAM_BASE
	ldr	r9, [r4, #OMAP_TYPE_OFFSET]	@ Get DEVICE type
	cmp	r9, #0x1			@ Check for HS device
	bne	skip_enable_smp_bit			@ if GP device

	LM_CHECK_IF_CPU0
	beq	enable_smp_bit				@ if CPU0

	/*
	 *     Check the WakeUp CPU
	 * CPU1 must check if CPU0 is alive/awaken;
	 * if PL310 is OFF, MPUSS was OFF and CPU0 is still off,
	 * CPU1 must go to sleep and wait for CPU0
	 */
	ldr	r2, =OMAP44XX_L2CACHE_BASE
	ldr	r0, [r2, #L2X0_CTRL]
	and	r0, #0x0f
	cmp	r0, #1
	beq	set_ns_smp_access
	mov     r0, #0x03		@ CPU1 to OFF state
	mov     r1, #0x00		@ Secure L1 is already clean
	ldr	r12, =0x108		@ SCU power state secure API
	dsb
	smc	#0
	dsb				@ Necessary barriers before wfi
	dmb
	isb
	wfi				@ wait for interrupt
	nop
	nop
set_ns_smp_access:
	mcr	p15, 0, r7, c7, c5, 6	@ invalidate BPIALL
	isb
	/* Setup PPA svc call */
	mov     r0, #PPA_SERVICE_NS_SMP
	adr	r3, zero_params		@ r3 must contain a pointer to params
	LM_CALL_PPA_SERVICE_PA
enable_smp_bit:
	/* CPUx must join coherency */
        mrc     p15, 0, r0, c1, c0, 1   @ Read SCTRL
	orr	r0, r0, #0x40		@ Enable SMP bit
        mcr     p15, 0, r0, c1, c0, 1
	isb
skip_enable_smp_bit:			@ GP device should execute from here
#ifdef CONFIG_CACHE_L2X0
	/* FIXME : Add POR register restore as well */
	ldr	r2, =OMAP44XX_L2CACHE_BASE
	ldr	r0, [r2, #L2X0_CTRL]
	and	r0, #0x0f
	cmp	r0, #1
	beq	skip_l2en
	/* Check ES revision */
	mrc	p15, 0, r0, c0, c0, 0
	ldr	r1, =A9_ES1_REV
	cmp	r0, r1
	bne	check_por
	ldr	r0, =OMAP4_L2X0_AUXCTL_VALUE_ES1
	ldr     r12, =0x109		@ Setup L2 AUXCTL value
	dsb
	smc     #0
	b	skip_l2en
check_por:
	ldr	r0, =OMAP44XX_SAR_RAM_BASE  @ Check DEVICE type
	ldr	r1, [r0, #OMAP_TYPE_OFFSET]
	cmp	r1, #0x1                   @ Check for HS device
	bne	skip_por
	ldr	r0, =PPA_SERVICE_PL310_POR @ Setup PPA HAL call
	adr	r3, POR_params
	LM_CALL_PPA_SERVICE_PA
skip_por:
	ldr	r0, =OMAP4_L2X0_AUXCTL_VALUE
	ldr     r12, =0x109		@ Setup L2 AUXCTL value
	dsb
	smc     #0
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
	LM_CHECK_IF_CPU0
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
#ifdef CONFIG_CACHE_L2X0
	ldr	r5, =OMAP44XX_L2CACHE_BASE
	str	r2, [r5, #0x7f0]	@ Clean and invalidate L2 cache line
restorewait_l2:
	ldr	r0, [r5, #0x7f0]
	ands	r0, #1
	bne	restorewait_l2
#endif
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
	dsb
	isb
	ldr	r0, =mmu_on
	bx	r0
mmu_on:
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

	mrc	p15, 0, r0, c1, c0, 0
	bic	r0, r0, #(1 << 2)	/* Disable the C bit */
	mcr	p15, 0, r0, c1, c0, 0
	isb

	ldr	r4, =sar_ram_base	@ Check DEVICE type
	ldr	r4, [r4]
	ldr     r9, [r4, #OMAP_TYPE_OFFSET]
	cmp	r9, #0x1		@ Check for HS device
	beq	clean_inv_l1_hs
clean_inv_l1:				@ cache-v7.S routine used here
	bl	v7_flush_dcache_all
	ldr	r4, =sar_ram_base
	ldr	r4, [r4]
	ldr	r3, [r4, #SCU_OFFSET]
	ldr	r2, =scu_base		@ Take CPUx out of coherency
	ldr	r2, [r2]
	str	r3, [r2, #0x08]
	b	l2x_clean_inv
clean_inv_l1_hs:
	stmfd   r13!, {r4-r12, r14}
	mov     r0, #0x00		@ Keep CPU in ON state
	mov     r1, #0xFF		@ Clean Secure L1
	ldr	r12, =0x108		@ SCU power state secure API
	dsb
	smc	#0
	ldmfd   r13!, {r4-r12, r14}

	bl	v7_flush_dcache_all	@ Clean and Invalidate L1

	stmfd   r13!, {r4-r12, r14}
	mov     r0, #0x03		@ CPU to OFF state
	mov     r1, #0x00		@ Secure L1 is already clean
	ldr	r12, =0x108		@ SCU power state secure API
	dsb
	smc	#0
	ldmfd   r13!, {r4-r12, r14}

	LM_CHECK_IF_CPU0
	beq	disable_smp_bit		@ skip if CPU0
	mrc     p15, 0, r0, c1, c1, 2	@ read NACR
	ands    r0, r0, #(1<<18)	@ check if NS can access SMP bit
	bne     disable_smp_bit		@ skip if access is already there
	stmfd   r13!, {r4-r12, r14}

	/*
	 * Cannot use LM_CALL_PPA_SVC_PA here because MMU is ON
	 * @r0 PPA service
	 * @r3 Pointer's PA of parameters, since MMU is on, it is
	 * needed to calculate the PA
	 */
	mov     r0, #PPA_SERVICE_NS_SMP	@ Call svc to set NS access to SMP
	mov	r1, #0x0	      	@ Process ID
	mov	r2, #0x4		@ Flag
	ldr	r3, =zero_params
	ldr	r4, =LOCAL_VA2PA_OFFSET
	add     r3, r3, r4		@ Pointer to params
	mov	r6, #0xff
	mov	r12, #0x00     		@ Secure Service ID
	dsb
	smc     #0
	ldmfd   r13!, {r4-r12, r14}
disable_smp_bit:
	mrc     p15, 0, r0, c1, c0, 1
	bic     r0, r0, #0x40		@ clear SMP
	mcr     p15, 0, r0, c1, c0, 1
	isb
l2x_clean_inv:				@ Only for MPU OFF and last CPU
#ifdef CONFIG_CACHE_L2X0
	ldr	r8, =sar_ram_base
	ldr	r8, [r8]
	ldr	r0, [r8, #L2X0_OFFSET]
	cmp	r0, #3
	bne	do_WFI
	ldr	r2, =l2cache_base
	ldr	r2, [r2]
	ldr	r0, =0xffff
	str	r0, [r2, #L2X0_CLEAN_INV_WAY]
loop4:
	ldr	r0, [r2, #L2X0_CLEAN_INV_WAY]
	cmp	r0, #0
	bne	loop4
l2x_sync:
	mov	r0, #0x0
	str	r0, [r2, #L2X0_CACHE_SYNC]
loop5:
	ldr	r0, [r2, #L2X0_CACHE_SYNC]
	ands	r0, r0, #0x1
	bne	loop5
#endif
do_WFI:
	dsb				@ Necessary barriers before wfi
	dmb
	isb
	wfi				@ wait for interrupt

	/*
	 * CPUx didn't hit targeted low power state.
	 * Clear SCU power status. Both CPU bits needs
	 * to be cleared o.w CPUs may deadlock because
	 * of coherency
	 */
	mrc	p15, 0, r0, c1, c0, 0
	tst	r0, #(1 << 2)        @ Check C bit enabled?
	orreq	r0, r0, #(1 << 2)    @ Enable the C bit if cleared
	mcreq	p15, 0, r0, c1, c0, 0
	isb

	ldr	r4, =sar_ram_base
	ldr	r4, [r4]
	ldr     r9, [r4, #OMAP_TYPE_OFFSET]
	cmp	r9, #0x1
	bne	scu_gp_clear

	mov     r0, #0x00
	mov     r1, #0x00
	stmfd   r13!, {r4-r12, r14}
	ldr	r12, =0x108		@ SCU power state secure API
	dsb
	smc	#0
	ldmfd   r13!, {r4-r12, r14}
	isb
	mrc	p15, 0, r0, c1, c0, 1
	orr	r0, r0, #0x40		@ Enable SMP bit
	mcr	p15, 0, r0, c1, c0, 1
	isb
	b	skip_gp_clear
scu_gp_clear:
	ldr	r2, =scu_base		@ Take CPUx out of coherency
	ldr	r2, [r2]
	ldr	r3, =SCU_CLEAR_STATE
	str	r3, [r2, #0x08]
	dsb				@ Issue a write memory barrier
	ldr	r3, [r2, #0x08]		@ read-back
skip_gp_clear:
	ldmfd	sp!, {r0-r12, pc}	@ restore regs and return

END(__omap4_cpu_suspend)

#endif
