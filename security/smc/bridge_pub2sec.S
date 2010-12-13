/**
 * Copyright (c) 2006-2010 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

.text

#define SMICODEPUB_IRQ_END   0xFE
#define SMICODEPUB_FIQ_END   0xFD
#define SMICODEPUB_RPC_END   0xFC

#define PUB2SEC_NOCST        0xFF
#define SMICODEPUB_NEWTASK   0x00

/*
 * RPC status:
 *  - 0: the secure world yielded due to an interrupt
 *  - 1: the secure world yielded on an RPC (no public thread is handling it)
 *  - 2: the secure world yielded on an RPC and the response is ready
 */
#define RPC_ADVANCEMENT_NONE		0
#define RPC_ADVANCEMENT_PENDING		1
#define RPC_ADVANCEMENT_FINISHED	2

#ifdef CONFIG_ARM_ERRATA_430973
#define INVALIDATE_BTB       MCR p15, 0, R0, c7, c5, 6
#else
#define INVALIDATE_BTB
#endif

schedule_secure_world:
	.global schedule_secure_world

	/* Save registers and disable interrupts */
	push	{r4-r12, lr}
	cpsid	i

	/* Copy the Secure Service ID in r12 */
	mov     r12, r0

	cmp	r0, #SMICODEPUB_IRQ_END
	beq	check_rpc

	mov	r6, #PUB2SEC_NOCST
	mov	r12, #SMICODEPUB_NEWTASK

	b	label_smc

check_rpc:
	ldr	r8, =g_RPC_advancement
	ldr	r9, [r8]
	cmp	r9, #RPC_ADVANCEMENT_PENDING
	beq	schedule_secure_world_exit

	ldr	r10, =g_secure_task_id
	ldr	r6, [r10]

	cmp	r9, #RPC_ADVANCEMENT_NONE
	beq	label_smc

	ldr	r9, =g_RPC_parameters
	ldm	r9, {r0-r3}
	mov	r10, #RPC_ADVANCEMENT_NONE
	str	r10, [r8]
	mov 	r12, #SMICODEPUB_RPC_END

	b	label_smc

label_smc:
	INVALIDATE_BTB
	smc	1
	b	service_end
	nop

	INVALIDATE_BTB
	mov	r0, #0x00
	ldr	r8, =g_secure_task_id
	str	r6, [r8]
	b	schedule_secure_world_exit

service_end:
   ldr	r8, =g_service_end
   mov	r6, #0x1
   str	r6, [r8]

schedule_secure_world_exit:
	INVALIDATE_BTB

	/* Enable interrupts and restore registers */
	cpsie	i
	pop	{r4-r12, lr}

	bx	lr

rpc_handler:
	.global rpc_handler

	INVALIDATE_BTB

	/* g_RPC_advancement = RPC_ADVANCEMENT_PENDING */
	ldr	r8, =g_RPC_advancement
	mov	r9, #RPC_ADVANCEMENT_PENDING
	str	r9, [r8]

	ldr	r8, =g_RPC_parameters
	stm	r8, {r0-r3}

	ldr	r8, =g_secure_task_id
	str	r6, [r8]

	/* Enable interrupts and restore registers */
	cpsie	i
	pop	{r4-r12, lr}
	mov r0, #0x00

	bx	lr
