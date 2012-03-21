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

enum gcerror gc_wait_completion(struct completion *completion,
				unsigned int milliseconds)
{
	enum gcerror gcerror;
	int timeout;

	might_sleep();

	spin_lock_irq(&completion->wait.lock);

	if (completion->done) {
		completion->done = 0;
		gcerror = GCERR_NONE;
	}  else if (milliseconds == 0) {
		gcerror = GCERR_TIMEOUT;
	} else {
		DECLARE_WAITQUEUE(wait, current);

		if (milliseconds == GC_INFINITE)
#if GC_DETECT_TIMEOUT
			timeout = GC_DIAG_SLEEP_JIF;
#else
			timeout = MAX_SCHEDULE_TIMEOUT;
#endif
		else
			timeout = milliseconds * HZ / 1000;

		wait.flags |= WQ_FLAG_EXCLUSIVE;
		__add_wait_queue_tail(&completion->wait, &wait);

		while (1) {
			if (signal_pending(current)) {
				/* Interrupt received. */
				gcerror = GCERR_INTERRUPTED;
				break;
			}

			__set_current_state(TASK_INTERRUPTIBLE);
			spin_unlock_irq(&completion->wait.lock);
			timeout = schedule_timeout(timeout);
			spin_lock_irq(&completion->wait.lock);

			if (completion->done) {
				completion->done = 0;
				gcerror = GCERR_NONE;
				break;
			}

			timeout = GC_DIAG_SLEEP_JIF;
		}

		__remove_wait_queue(&completion->wait, &wait);
	}

	spin_unlock_irq(&completion->wait.lock);

	return gcerror;
}
EXPORT_SYMBOL(gc_wait_completion);

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
