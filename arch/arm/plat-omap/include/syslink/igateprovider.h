/*
 *  igateprovider.h
 *
 *   Interface implemented by all gate providers.
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
 *
 */
/** ============================================================================
 *  Gates are used serialize access to data structures that are used by more
 *  than one thread.
 *
 *  Gates are responsible for ensuring that only one out of multiple threads
 *  can access a data structure at a time.  There
 *  are important scheduling latency and performance considerations that
 *  affect the "type" of gate used to protect each data structure.  For
 *  example, the best way to protect a shared counter is to simply disable
 *  all interrupts before the update and restore the interrupt state after
 *  the update; disabling all interrupts prevents all thread switching, so
 *  the update is guaranteed to be "atomic".  Although highly efficient, this
 *  method of creating atomic sections causes serious system latencies when
 *  the time required to update the data structure can't be bounded.
 *
 *  For example, a memory manager's list of free blocks can grow indefinitely
 *  long during periods of high fragmentation.  Searching such a list with
 *  interrupts disabled would cause system latencies to also become unbounded.
 *  In this case, the best solution is to provide a gate that suspends the
 *  execution of  threads that try to enter a gate that has already been
 *  entered; i.e., the gate "blocks" the thread until the thread
 *  already in the gate leaves.  The time required to enter and leave the
 *  gate is greater than simply enabling and restoring interrupts, but since
 *  the time spent within the gate is relatively large, the overhead caused by
 *  entering and leaving gates will not become a significant percentage of
 *  overall system time.  More importantly, threads that do not need to
 *  access the shared data structure are completely unaffected by threads
 *  that do access it.
 *  ============================================================================
 */


#ifndef _IGATEPROVIDER_H_
#define _IGATEPROVIDER_H_


/* Invalid Igate */
#define IGATEPROVIDER_NULL	(struct igateprovider_object *)0xFFFFFFFF

/* Gates with this "quality" may cause the calling thread to block;
 *  i.e., suspend execution until another thread leaves the gate. */
#define IGateProvider_Q_BLOCKING	1

/* Gates with this "quality" allow other threads to preempt the thread
 * that has already entered the gate. */
#define IGateProvider_Q_PREEMPTING	2

/* Object embedded in other Gate modules. (Inheritance) */
#define IGATEPROVIDER_SUPEROBJECT	\
	int *(*enter)(void *);		\
	void (*leave)(void *, int *)

#define IGATEPROVIDER_OBJECTINITIALIZER(x, y)	\
	do {					\
		((struct igateprovider_object *)(x))->enter = y##_enter; \
		((struct igateprovider_object *)(x))->leave = y##_leave; \
	} while (0)


/* Structure for generic gate instance */
struct igateprovider_object {
	IGATEPROVIDER_SUPEROBJECT;
};


/*
 * Enter this gate
 *
 * Each gate provider can implement mutual exclusion using different
 * algorithms; e.g., disabling all scheduling, disabling the scheduling
 * of all threads below a specified "priority level", suspending the
 * caller when the gate has been entered by another thread and
 * re-enabling it when the the other thread leaves the gate.  However,
 * in all cases, after this method returns that caller has exclusive
 * access to the data protected by this gate.
 *
 * A thread may reenter a gate without blocking or failing.
 */
static inline int *igateprovider_enter(struct igateprovider_object *handle)
{
	int *key = NULL;

	if (handle != IGATEPROVIDER_NULL)
		key = (handle->enter)((void *)handle);
	return key;
}


/*
 * Leave this gate
 *
 * This method is only called by threads that have previously entered
 * this gate via `{@link #enter}`.  After this method returns, the
 * caller must not access the data structure protected by this gate
 * (unless the caller has entered the gate more than once and other
 * calls to `leave` remain to balance the number of previous
 * calls to `enter`).
 */
static inline void igateprovider_leave(struct igateprovider_object *handle,
						int *key)
{
	if (handle != IGATEPROVIDER_NULL)
		(handle->leave)((void *)handle, key);
}

#endif /* ifndef _IGATEPROVIDER_H_ */
