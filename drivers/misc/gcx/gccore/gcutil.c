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

#ifndef GC_DUMP
#	define GC_DUMP 0
#endif

#if GC_DUMP
#	define GC_PRINT printk
#else
#	define GC_PRINT(...)
#endif

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

			gc_gpu_status((char *) __func__, __LINE__, NULL);
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

		/* Time to print diag info? */
		if ((timeout % GC_DIAG_SLEEP_MSEC) == 0) {
			gc_gpu_status((char *) __func__,
					__LINE__, NULL);
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

/*******************************************************************************
** Debugging tools.
*/

void gc_gpu_id(void)
{
	unsigned int chipModel;
	unsigned int chipRevision;
	unsigned int chipDate;
	unsigned int chipTime;
	unsigned int chipFeatures;
	unsigned int chipMinorFeatures;

	chipModel = gc_read_reg(GC_CHIP_ID_Address);
	chipRevision = gc_read_reg(GC_CHIP_REV_Address);
	chipDate = gc_read_reg(GC_CHIP_DATE_Address);
	chipTime = gc_read_reg(GC_CHIP_TIME_Address);
	chipFeatures = gc_read_reg(GC_FEATURES_Address);
	chipMinorFeatures = gc_read_reg(GC_MINOR_FEATURES0_Address);

	GC_PRINT(KERN_INFO "CHIP IDENTITY\n");
	GC_PRINT(KERN_INFO "  model=%X\n", chipModel);
	GC_PRINT(KERN_INFO "  revision=%X\n", chipRevision);
	GC_PRINT(KERN_INFO "  date=%X\n", chipDate);
	GC_PRINT(KERN_INFO "  time=%X\n", chipTime);
	GC_PRINT(KERN_INFO "  chipFeatures=0x%08X\n", chipFeatures);
}
EXPORT_SYMBOL(gc_gpu_id);

void gc_gpu_status(char *function, int line, unsigned int *acknowledge)
{
	int i;
	unsigned int idle;
	unsigned int dma_state, dma_addr;
	unsigned int dma_low_data, dma_high_data;
	unsigned int status;
	unsigned int mmu;
	unsigned int address;
	unsigned int total_reads;
	unsigned int total_writes;
	unsigned int total_read_bursts;
	unsigned int total_write_bursts;
	unsigned int total_read_reqs;
	unsigned int total_write_reqs;

	GC_PRINT(KERN_INFO "%s(%d): Current GPU status.\n",
		function, line);

	idle = gc_read_reg(GCREG_HI_IDLE_Address);
	GC_PRINT(KERN_INFO "%s(%d):   idle = 0x%08X\n",
		function, line, idle);

	dma_state = gc_read_reg(GCREG_FE_DEBUG_STATE_Address);
	GC_PRINT(KERN_INFO "%s(%d):   DMA state = 0x%08X\n",
		function, line, dma_state);

	dma_addr = gc_read_reg(GCREG_FE_DEBUG_CUR_CMD_ADR_Address);
	GC_PRINT(KERN_INFO "%s(%d):   DMA address = 0x%08X\n",
		function, line, dma_addr);

	dma_low_data = gc_read_reg(GCREG_FE_DEBUG_CMD_LOW_REG_Address);
	GC_PRINT(KERN_INFO "%s(%d):   DMA low data = 0x%08X\n",
		function, line, dma_low_data);

	dma_high_data = gc_read_reg(GCREG_FE_DEBUG_CMD_HI_REG_Address);
	GC_PRINT(KERN_INFO "%s(%d):   DMA high data = 0x%08X\n",
		function, line, dma_high_data);

	total_reads = gc_read_reg(GC_TOTAL_READS_Address);
	GC_PRINT(KERN_INFO "%s(%d):   Total memory reads = %d\n",
		function, line, total_reads);

	total_writes = gc_read_reg(GC_TOTAL_WRITES_Address);
	GC_PRINT(KERN_INFO "%s(%d):   Total memory writes = %d\n",
		function, line, total_writes);

	total_read_bursts = gc_read_reg(GC_TOTAL_READ_BURSTS_Address);
	GC_PRINT(KERN_INFO "%s(%d):   Total memory read 64-bit bursts = %d\n",
		function, line, total_read_bursts);

	total_write_bursts = gc_read_reg(GC_TOTAL_WRITE_BURSTS_Address);
	GC_PRINT(KERN_INFO "%s(%d):   Total memory write 64-bit bursts = %d\n",
		function, line, total_write_bursts);

	total_read_reqs = gc_read_reg(GC_TOTAL_READ_REQS_Address);
	GC_PRINT(KERN_INFO "%s(%d):   Total memory read requests = %d\n",
		function, line, total_read_reqs);

	total_write_reqs = gc_read_reg(GC_TOTAL_WRITE_REQS_Address);
	GC_PRINT(KERN_INFO "%s(%d):   Total memory write requests = %d\n",
		function, line, total_write_reqs);

	if (acknowledge != NULL)
		GC_PRINT(KERN_INFO "%s(%d):   interrupt acknowledge = 0x%08X\n",
			function, line, *acknowledge);

	if (*acknowledge & 0x80000000) {
		GC_PRINT(KERN_INFO "%s(%d):   *** BUS ERROR ***\n",
			function, line);
	}

	if (*acknowledge & 0x40000000) {
		unsigned int mtlb, stlb, offset;

		GC_PRINT(KERN_INFO "%s(%d):   *** MMU ERROR ***\n",
			function, line);

		status = gc_read_reg(GCREG_MMU_STATUS_Address);
		GC_PRINT(KERN_INFO "%s(%d):   MMU status = 0x%08X\n",
			function, line, status);

		for (i = 0; i < 4; i += 1) {
			mmu = status & 0xF;
			status >>= 4;

			if (mmu == 0)
				continue;

			switch (mmu) {
			case 1:
				GC_PRINT(KERN_INFO
					"%s(%d):   MMU%d: slave not present\n",
					function, line, i);
				break;

			case 2:
				GC_PRINT(KERN_INFO
					"%s(%d):   MMU%d: page not present\n",
					function, line, i);
				break;

			case 3:
				GC_PRINT(KERN_INFO
					"%s(%d):   MMU%d: write violation\n",
					function, line, i);
				break;

			default:
				GC_PRINT(KERN_INFO
					"%s(%d):   MMU%d: unknown state\n",
					function, line, i);
			}

			address = gc_read_reg(GCREG_MMU_EXCEPTION_Address + i);

			mtlb   = (address & MMU_MTLB_MASK) >> MMU_MTLB_SHIFT;
			stlb   = (address & MMU_STLB_MASK) >> MMU_STLB_SHIFT;
			offset =  address & MMU_OFFSET_MASK;

			GC_PRINT(KERN_INFO
				"%s(%d):   MMU%d: exception address = 0x%08X\n",
				function, line, i, address);

			GC_PRINT(KERN_INFO
				"%s(%d):            MTLB entry = %d\n",
				function, line, mtlb);

			GC_PRINT(KERN_INFO
				"%s(%d):            STLB entry = %d\n",
				function, line, stlb);

			GC_PRINT(KERN_INFO
				"%s(%d):            Offset = 0x%08X (%d)\n",
				function, line, offset, offset);
		}
	}
}
EXPORT_SYMBOL(gc_gpu_status);
