/*
 * gcx.h
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef GCX_H
#define GCX_H

#include "semaphore.h"
#include "gcerror.h"
#include "gcreg.h"
#include "gcdbglog.h"
#include "gcdebug.h"

#ifndef countof
#define countof(a) \
( \
	sizeof(a) / sizeof(a[0]) \
)
#endif

#define GC_PTR2INT(p) \
( \
	(unsigned int) (p) \
)

#define GC_ALIGN(n, align) \
( \
	((n) + ((align) - 1)) & ~((align) - 1) \
)

#define GCLOCK_TIMEOUT_SEC 10
#define GCLOCK_TIMEOUT_JIF (msecs_to_jiffies(GCLOCK_TIMEOUT_SEC * 1000))

#define GCLOCK_TYPE \
	struct semaphore

#define GCDEFINE_LOCK(name) \
	DEFINE_SEMAPHORE(name)

#define GCLOCK_INIT(lock) \
	sema_init(lock, 1)

#define GCLOCK_DESTROY(lock) \
	do { } while (0)

#define GCLOCK(lock) { \
	unsigned long __result__; \
	while (true) { \
		__result__ = down_timeout(lock, GCLOCK_TIMEOUT_JIF); \
		if (__result__ == 0) \
			break; \
		printk(KERN_ERR "%s(%d) GCLOCK timeout (%lu).\n", \
		       __func__, __LINE__, __result__); \
		GCGPUSTATUS();					  \
		printk(KERN_INFO "%s(%d)\n", __func__, __LINE__); \
	} \
}

#define GCUNLOCK(lock) \
	up(lock)

#define GCWAIT_FOR_COMPLETION(completion) { \
	unsigned long __result__; \
	while (true) { \
		__result__ = wait_for_completion_timeout(completion, \
							GCLOCK_TIMEOUT_JIF); \
		if (__result__ != 0) \
			break; \
		printk(KERN_ERR \
		       "%s(%d) GCWAIT_FOR_COMPLETION timeout (%lu).\n", \
		       __func__, __LINE__, __result__); \
		GCGPUSTATUS();					  \
		printk(KERN_INFO "%s(%d)\n", __func__, __LINE__); \
	} \
}

#define GCWAIT_FOR_COMPLETION_TIMEOUT(result, completion, timeout) { \
	if (timeout == MAX_SCHEDULE_TIMEOUT) { \
		while (true) { \
			result = wait_for_completion_timeout(completion, \
							GCLOCK_TIMEOUT_JIF); \
			if (result != 0) \
				break; \
			printk(KERN_ERR \
			       "%s(%d) GCWAIT_FOR_COMPLETION_TIMEOUT" \
			       " timeout (%lu).\n", \
			       __func__, __LINE__, result); \
			GCGPUSTATUS();					\
			printk(KERN_INFO "%s(%d)\n", __func__, __LINE__); \
		} \
	} else { \
		result = wait_for_completion_timeout(completion, timeout); \
	} \
}

#endif
