/*
 *  gatemp.c
 *
 *  Gate wrapper implementation
 *
 *  Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */


/* Standard headers */
#include <linux/types.h>
#include <linux/kernel.h>

/* Module level headers */
#include <igateprovider.h>
#include <gate.h>


/* Structure defining internal object for the Gate Peterson.*/
struct gate_object {
	IGATEPROVIDER_SUPEROBJECT; /* For inheritance from IGateProvider */
};

/* Function to enter a Gate */
int *gate_enter_system(void)
{
	unsigned long flags;

	local_irq_save(flags);

	return (int *)flags;
}

/* Function to leave a gate */
void gate_leave_system(int *key)
{
	local_irq_restore((unsigned long) key);
}

/* Match with IGateProvider */
static inline int *_gate_enter_system(struct gate_object *obj)
{
	(void) obj;
	return gate_enter_system();
}

/* Match with IGateProvider */
static inline void _gate_leave_system(struct gate_object *obj, int *key)
{
	(void) obj;
	gate_leave_system(key);
}

static struct gate_object gate_system_object = {
	.enter = (int *(*)(void *))_gate_enter_system,
	.leave = (void (*)(void *, int *))_gate_leave_system,
};

struct igateprovider_object *gate_system_handle = \
			(struct igateprovider_object *)&gate_system_object;
