/*
*  atomic_linux.h
*
*  Atomic operations functions
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

#ifndef _ATOMIC_LINUX_H
#define _ATOMIC_LINUX_H

#include <linux/types.h>
#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/atomic.h>

/*
 * ======== atomic_cmpmask_and_set ========
 *  Purpose:
 *  This will compare a mask and set if not equal
 */
static inline void atomic_cmpmask_and_set(atomic_t *v, u32 mask, u32 val)
{
	s32	ret;
	unsigned long flags;
	atomic_t *atm = v;

	raw_local_irq_save(flags);
	ret = atm->counter;
	if (likely(((ret & mask) != mask)))
		atm->counter = val;
	raw_local_irq_restore(flags);
}

/*
 * ======== atomic_cmpmask_and_set ========
 *  Purpose:
 *  This will compare a mask and then check current value less than
 *  provided value.
 */
static inline bool atomic_cmpmask_and_lt(atomic_t *v, u32 mask, u32 val)
{
	bool ret = true;
	atomic_t *atm = v;
	s32 cur;
	unsigned long flags;

	raw_local_irq_save(flags);
	cur = atm->counter;
	/* Compare mask, if matches then compare val */
	if (likely(((cur & mask) == mask))) {
		if (likely(cur >= val))
			ret = false;
	}
	raw_local_irq_restore(flags);

	/* retval = true  if mask matches and current value is less than given
	*  value */
	/* retval = false either mask doesnot matches or current value is not
	*  less than given value */
	return ret;
}


/*
 * ======== atomic_cmpmask_and_set ========
 *  Purpose:
 *  This will compare a mask and then check current value greater than
 *  provided value.
 */
static inline bool atomic_cmpmask_and_gt(atomic_t *v, u32 mask, u32 val)
{
	bool ret = false;
	atomic_t *atm = v;
	s32	cur;
	unsigned long flags;

	raw_local_irq_save(flags);
	cur = atm->counter;
	/* Compare mask, if matches then compare val */
	if (likely(((cur & mask) == mask))) {
		if (likely(cur > val))
			ret = true;
	}

	raw_local_irq_restore(flags);
	/* retval = true  if mask matches and current value is less than given
	 *  value */
	/* etval =false either mask doesnot matches or current value is not
	 *  greater than given value */
	return ret;
}

#endif /* if !defined(_ATOMIC_LINUX_H) */
