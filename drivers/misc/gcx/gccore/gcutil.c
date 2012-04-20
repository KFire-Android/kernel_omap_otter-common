/*
 * gcutil.c
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

#include <linux/module.h>
#include <linux/gcx.h>
#include <linux/gccore.h>
#include "gcmmu.h"

#define GC_DETECT_TIMEOUT	0
#define GC_DIAG_SLEEP_SEC	10
#define GC_DIAG_SLEEP_MSEC	(GC_DIAG_SLEEP_SEC * 1000)
#define GC_DIAG_SLEEP_JIF	(GC_DIAG_SLEEP_SEC * HZ)

/*******************************************************************************
** Synchronization functions.
*/

void gc_delay(unsigned int milliseconds)
{
	if (milliseconds > 0) {
		unsigned long jiffies;

		/* Convert timeval to jiffies. */
		jiffies = msecs_to_jiffies(milliseconds);

		/* Schedule timeout. */
		schedule_timeout_interruptible(jiffies);
	}
}
EXPORT_SYMBOL(gc_delay);

enum gcerror gc_acquire_mutex(struct mutex *mutex,
				unsigned int milliseconds)
{
	enum gcerror gcerror;

#if GC_DETECT_TIMEOUT
	unsigned long timeout = 0;

	for (;;) {
		/* Try to acquire the mutex (no waiting). */
		if (mutex_trylock(mutex)) {
			gcerror = GCERR_NONE;
			break;
		}

		/* Advance the timeout. */
		timeout += 1;

		/* Timeout? */
		if ((milliseconds != GC_INFINITE) &&
			(timeout >= milliseconds)) {
			gcerror = GCERR_TIMEOUT;
			break;
		}

		/* Wait for 1 millisecond. */
		gc_delay(1);
	}
#else
	if (milliseconds == GC_INFINITE) {
		mutex_lock(mutex);
		gcerror = GCERR_NONE;
	} else {
		while (true) {
			/* Try to acquire the mutex. */
			if (mutex_trylock(mutex)) {
				gcerror = GCERR_NONE;
				break;
			}

			if (milliseconds-- == 0) {
				gcerror = GCERR_TIMEOUT;
				break;
			}

			/* Wait for 1 millisecond. */
			gc_delay(1);
		}
	}
#endif

	return gcerror;
}
EXPORT_SYMBOL(gc_acquire_mutex);
