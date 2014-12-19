/*
 * gcqueue.c
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

#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>
#include <asm/cacheflush.h>
#include "gcmain.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_ISR		(1 << 1)
#define GCZONE_THREAD		(1 << 2)
#define GCZONE_INTERRUPT	(1 << 3)
#define GCZONE_BUFFER		(1 << 4)
#define GCZONE_EVENT		(1 << 5)
#define GCZONE_QUEUE		(1 << 6)
#define GCZONE_MAPPING		(1 << 7)
#define GCZONE_ALLOC		(1 << 8)
#define GCZONE_EXEC		(1 << 9)

GCDBG_FILTERDEF(queue, GCZONE_NONE,
		"init",
		"isr",
		"thread",
		"interrupt",
		"buffer",
		"event",
		"queue",
		"mapping",
		"alloc",
		"exec")

#define GCDBG_QUEUE(zone, message, gccmdbuf) { \
	GCDBG(zone, message "queue entry @ 0x%08X:\n", \
	      (unsigned int) gccmdbuf); \
	GCDBG(zone, "  logical = 0x%08X\n", \
	      (unsigned int) gccmdbuf->logical); \
	GCDBG(zone, "  physical = 0x%08X\n", \
	      gccmdbuf->physical); \
	GCDBG(zone, "  size = %d, count = %d\n", \
	      gccmdbuf->size, gccmdbuf->count); \
	GCDBG(zone, "  terminator = 0x%08X\n", \
	      (unsigned int) gccmdbuf->gcmoterminator); \
	GCDBG(zone, "  int = %d\n", \
	      gccmdbuf->interrupt); \
	GCDBG(zone, "  %s\n", \
	      list_empty(&gccmdbuf->events) ? "no events" : "has events"); \
}


/*******************************************************************************
 * Internal definitions.
 */

/* GPU timeout in milliseconds. The timeout value controls when the power
 * on the GPU is pulled if there is no activity in progress or scheduled. */
#define GC_THREAD_TIMEOUT	20

/* Time in milliseconds to wait for GPU to become idle. */
#define GC_IDLE_TIMEOUT		100

/* The size of storage buffer. */
#define GC_STORAGE_SIZE		((GC_BUFFER_SIZE * GC_CMDBUF_FACTOR + \
					(PAGE_SIZE - 1)) & PAGE_MASK)

/* Number of pages per one kernel storage buffer. */
#define GC_STORAGE_PAGES	(GC_STORAGE_SIZE / PAGE_SIZE)

/* Reserved number of bytes at the end of storage buffer. */
#define GC_TAIL_RESERVE		sizeof(struct gcmoterminator)

/* Minimum available space requirement. */
#define GC_MIN_THRESHOLD	((int) (GC_TAIL_RESERVE + 200))

/* Event assignment. */
#define GC_SIG_BUS_ERROR	31
#define GC_SIG_MMU_ERROR	30
#define GC_SIG_DMA_DONE_BITS	30

#define GC_SIG_MASK_BUS_ERROR	(1 << GC_SIG_BUS_ERROR)
#define GC_SIG_MASK_MMU_ERROR	(1 << GC_SIG_MMU_ERROR)
#define GC_SIG_MASK_DMA_DONE	((1 << GC_SIG_DMA_DONE_BITS) - 1)


/*******************************************************************************
 * ISR.
 */

static irqreturn_t gcisr(int irq, void *_gccorecontext)
{
	struct gccorecontext *gccorecontext;
	struct gcqueue *gcqueue;
	unsigned int triggered;

	/* Read gcregIntrAcknowledge register. */
	triggered = gc_read_reg(GCREG_INTR_ACKNOWLEDGE_Address);

	/* Return if not our interrupt. */
	if (triggered == 0)
		return IRQ_NONE;

	/* Cast the context. */
	gccorecontext = (struct gccorecontext *) _gccorecontext;

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Log interrupt data. */
	gc_debug_cache_gpu_status_from_irq(triggered);

	/* Bus error? */
	if ((triggered & GC_SIG_MASK_BUS_ERROR) != 0) {
		triggered &= ~GC_SIG_MASK_BUS_ERROR;
		complete(&gcqueue->buserror);
	}

	/* MMU error? */
	if ((triggered & GC_SIG_MASK_MMU_ERROR) != 0) {
		triggered &= ~GC_SIG_MASK_MMU_ERROR;
		complete(&gcqueue->mmuerror);
	}

	/* Command buffer event? */
	if (triggered != 0)
		atomic_add(triggered, &gcqueue->triggered);

	/* Release the command buffer thread. */
	complete(&gcqueue->ready);

	/* IRQ handled. */
	return IRQ_HANDLED;
}


/*******************************************************************************
 * Command buffer event and queue management.
 */

enum gcerror gcqueue_alloc_event(struct gcqueue *gcqueue,
				 struct gcevent **gcevent)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcevent *temp;

	GCENTER(GCZONE_EVENT);

	GCLOCK(&gcqueue->vaceventlock);

	if (list_empty(&gcqueue->vacevents)) {
		GCDBG(GCZONE_EVENT, "allocating event entry.\n");
		temp = kmalloc(sizeof(struct gcevent), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("event entry allocation failed.\n");
			gcerror = GCERR_CMD_EVENT_ALLOC;
			goto exit;
		}
	} else {
		struct list_head *head;
		head = gcqueue->vacevents.next;
		temp = list_entry(head, struct gcevent, link);
		list_del(head);
	}

	GCDBG(GCZONE_EVENT, "event entry allocated @ 0x%08X.\n",
	      (unsigned int) temp);

	*gcevent = temp;

exit:
	GCUNLOCK(&gcqueue->vaceventlock);

	GCEXITARG(GCZONE_EVENT, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

enum gcerror gcqueue_free_event(struct gcqueue *gcqueue,
				struct gcevent *gcevent)
{
	GCENTER(GCZONE_EVENT);

	GCLOCK(&gcqueue->vaceventlock);
	list_move(&gcevent->link, &gcqueue->vacevents);
	GCUNLOCK(&gcqueue->vaceventlock);

	GCEXIT(GCZONE_EVENT);
	return GCERR_NONE;
}

enum gcerror gcqueue_alloc_cmdbuf(struct gcqueue *gcqueue,
				  struct gccmdbuf **gccmdbuf)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gccmdbuf *temp;

	GCENTER(GCZONE_QUEUE);

	GCLOCK(&gcqueue->vacqueuelock);

	if (list_empty(&gcqueue->vacqueue)) {
		GCDBG(GCZONE_QUEUE, "allocating queue entry.\n");
		temp = kmalloc(sizeof(struct gccmdbuf), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("queue entry allocation failed.\n");
			gcerror = GCERR_CMD_QUEUE_ALLOC;
			goto exit;
		}
	} else {
		struct list_head *head;
		head = gcqueue->vacqueue.next;
		temp = list_entry(head, struct gccmdbuf, link);
		list_del(head);
	}

	INIT_LIST_HEAD(&temp->events);

	GCDBG(GCZONE_EVENT, "queue entry allocated @ 0x%08X.\n",
	      (unsigned int) temp);

	*gccmdbuf = temp;

exit:
	GCUNLOCK(&gcqueue->vacqueuelock);

	GCEXITARG(GCZONE_QUEUE, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

void gcqueue_free_cmdbuf(struct gcqueue *gcqueue,
			 struct gccmdbuf *gccmdbuf,
			 unsigned int *flags)
{
	struct list_head *head;
	struct gcevent *gcevent;

	GCENTERARG(GCZONE_QUEUE, "queue entry = 0x%08X\n",
		   (unsigned int) gccmdbuf);

	/* Have events? */
	if (!list_empty(&gccmdbuf->events)) {
		/* Events not expected? */
		if (flags == NULL)
			GCERR("buffer has events.\n");

		/* Execute and free all events. */
		while (!list_empty(&gccmdbuf->events)) {
			head = gccmdbuf->events.next;
			gcevent = list_entry(head, struct gcevent, link);
			gcevent->handler(gcevent, flags);
			gcqueue_free_event(gcqueue, gcevent);
		}
	}

	/* Move the queue entry to the vacant list. */
	GCLOCK(&gcqueue->vacqueuelock);
	list_move(&gccmdbuf->link, &gcqueue->vacqueue);
	GCUNLOCK(&gcqueue->vacqueuelock);

	GCEXIT(GCZONE_QUEUE);
}


/*******************************************************************************
 * Event handlers.
 */

/* Completion event. */
static void event_completion(struct gcevent *gcevent, unsigned int *flags)
{
	struct completion *completion;

	GCENTER(GCZONE_EVENT);

	completion = gcevent->event.completion.completion;
	GCDBG(GCZONE_EVENT, "completion = 0x%08X\n",
		   (unsigned int) completion);

	complete(completion);

	GCEXIT(GCZONE_EVENT);
}

/* Callback event. */
static void event_callback(struct gcevent *gcevent, unsigned int *flags)
{
	void (*callback) (void *callbackparam);
	void *callbackparam;

	GCENTER(GCZONE_EVENT);

	callback = gcevent->event.callback.callback;
	callbackparam = gcevent->event.callback.callbackparam;
	GCDBG(GCZONE_EVENT, "callback      = 0x%08X\n",
		(unsigned int) callback);
	GCDBG(GCZONE_EVENT, "callbackparam = 0x%08X\n",
		(unsigned int) callbackparam);

	callback(callbackparam);

	GCEXIT(GCZONE_EVENT);
}

/* Unmap event. */
static void event_unmap(struct gcevent *gcevent, unsigned int *flags)
{
	enum gcerror gcerror;
	struct gccorecontext *gccorecontext;
	struct gcmmucontext *gcmmucontext;
	struct gcmmuarena *gcmmuarena;

	GCENTER(GCZONE_EVENT);

	gccorecontext = gcevent->event.unmap.gccorecontext;
	gcmmucontext = gcevent->event.unmap.gcmmucontext;
	gcmmuarena = gcevent->event.unmap.gcmmuarena;

	GCDBG(GCZONE_EVENT, "arena = 0x%08X\n",
		(unsigned int) gcmmuarena);

	gcerror = gcmmu_unmap(gccorecontext, gcmmucontext, gcmmuarena);
	if (gcerror != GCERR_NONE)
		GCERR("failed to unmap 0x%08X (gcerror = 0x%08X).\n",
		      gcmmuarena, gcerror);

	GCEXIT(GCZONE_EVENT);
}


/*******************************************************************************
 * Command buffer thread.
 */

enum gcerror gcqueue_alloc_int(struct gcqueue *gcqueue,
			       unsigned int *interrupt)
{
	unsigned int i;

	GCENTER(GCZONE_INTERRUPT);

	/* Wait until there are available interrupts. */
	GCWAIT_FOR_COMPLETION(&gcqueue->freeint);

	/* Get acceess to the interrupt tracker. */
	GCLOCK(&gcqueue->intusedlock);

	/* Find the first available interrupt. */
	for (i = 0; i < countof(gcqueue->intused); i += 1)
		if (!gcqueue->intused[i]) {
			gcqueue->intused[i] = true;

			/* Release access to the interrupt tracker. */
			GCUNLOCK(&gcqueue->intusedlock);
			*interrupt = i;

			GCEXITARG(GCZONE_INTERRUPT, "interrupt = %d.\n", i);
			return GCERR_NONE;
		}

	/* Release access to the interrupt tracker. */
	GCUNLOCK(&gcqueue->intusedlock);

	/* This is unexpected. */
	GCERR("unexpected condition.");

	GCEXIT(GCZONE_INTERRUPT);
	return GCERR_CMD_INT_ALLOC;
}

static void free_interrupt(struct gcqueue *gcqueue, unsigned int interrupt)
{
	GCENTERARG(GCZONE_INTERRUPT, "interrupt = %d.\n", interrupt);

	GCLOCK(&gcqueue->intusedlock);

	if (!gcqueue->intused[interrupt]) {
		GCERR("interrupt %d is already free.\n", interrupt);
		GCUNLOCK(&gcqueue->intusedlock);
	} else {
		gcqueue->intused[interrupt] = false;
		GCUNLOCK(&gcqueue->intusedlock);

		complete(&gcqueue->freeint);
	}

	GCEXIT(GCZONE_INTERRUPT);
}

static void link_queue(struct list_head *newlist,
		       struct list_head *queue,
		       struct gcmoterminator *gcmoterminator)
{
	struct list_head *head;
	struct gccmdbuf *gccmdbuf;
	struct gccmdlink gccmdlink;

	GCENTER(GCZONE_QUEUE);

	if (!list_empty(queue)) {
		GCDBG(GCZONE_QUEUE, "main queue is not empty.\n");

		/* Get the tail of the queue. */
		head = queue->prev;
		gccmdbuf = list_entry(head, struct gccmdbuf, link);
		gcmoterminator = gccmdbuf->gcmoterminator;

		/* Link the tail command buffer from the specified queue to
		 * the first command buffer in the specified list. If the tail
		 * command buffer has no events associated with it, the
		 * interrupt is not needed. */
		if (list_empty(&gccmdbuf->events)) {
			/* Replace EVENT with a NOP. */
			GCDBG_QUEUE(GCZONE_QUEUE, "removing interrupt from ",
				    gccmdbuf);
			gcmoterminator->u1.nop.cmd.raw = gccmdnop_const.cmd.raw;
			mb();
		}
	}

	if (gcmoterminator != NULL) {
		GCDBG(GCZONE_QUEUE, "modifying the terminator @ 0x%08X.\n",
		      gcmoterminator);

		/* Get the head of the list. */
		head = newlist->next;
		gccmdbuf = list_entry(head, struct gccmdbuf, link);

		/* Configure new command word. */
		gccmdlink.cmd.raw = 0;
		gccmdlink.cmd.fld.count = gccmdbuf->count;
		gccmdlink.cmd.fld.opcode = GCREG_COMMAND_OPCODE_LINK;

		/* Change WAIT into a LINK command; write the address first. */
		gcmoterminator->u2.linknext.address = gccmdbuf->physical;
		mb();
		gcmoterminator->u2.linknext.cmd.raw = gccmdlink.cmd.raw;
		mb();
	}

	/* Merge the list to the end of the queue. */
	list_splice_tail_init(newlist, queue);

	GCEXIT(GCZONE_QUEUE);
}

static enum gcerror append_cmdbuf(
	struct gccorecontext *gccorecontext,
	struct gcqueue *gcqueue)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gccmdbuf *headcmdbuf, *tailcmdbuf;
	struct list_head *head, *tail;

	/* Dump scheduled command buffers. */
#if GCDEBUG_ENABLE
	list_for_each(head, &gcqueue->cmdbufhead) {
		headcmdbuf = list_entry(head, struct gccmdbuf, link);

		GCDUMPBUFFER(GCZONE_BUFFER,
			     headcmdbuf->logical,
			     headcmdbuf->physical,
			     headcmdbuf->size);
	}
#endif

	GCLOCK(&gcqueue->queuelock);

	/* Link the tail of the active command buffer to
	 * the first scheduled command buffer. */
	link_queue(&gcqueue->cmdbufhead, &gcqueue->queue,
		   gcqueue->gcmoterminator);

	/* Get the last entry from the active queue. */
	tail = gcqueue->queue.prev;
	tailcmdbuf = list_entry(tail, struct gccmdbuf, link);

	/* Update the tail pointer. */
	gcqueue->gcmoterminator = tailcmdbuf->gcmoterminator;

	/* Start the GPU if not already running. */
	if (try_wait_for_completion(&gcqueue->stopped)) {
		GCDBG(GCZONE_THREAD, "GPU is currently stopped - starting.\n");

		/* Enable power to the chip. */
		gcpwr_set(gccorecontext, GCPWR_ON);

		/* Eable MMU. */
		gcerror = gcmmu_enable(gccorecontext, gcqueue);
		if (gcerror != GCERR_NONE) {
			GCERR("failed to enable MMU.\n");
			goto exit;
		}

		/* Get the first entry from the active queue. */
		head = gcqueue->queue.next;
		headcmdbuf = list_entry(head, struct gccmdbuf, link);

		GCDBG_QUEUE(GCZONE_THREAD, "starting ", headcmdbuf);

		/* Enable all events. */
		gc_write_reg(GCREG_INTR_ENBL_Address, ~0U);

		/* Write address register. */
		gc_write_reg(GCREG_CMD_BUFFER_ADDR_Address,
			     headcmdbuf->physical);

		/* Write control register. */
		gc_write_reg(GCREG_CMD_BUFFER_CTRL_Address,
			     GCSETFIELDVAL(0, GCREG_CMD_BUFFER_CTRL,
					   ENABLE, ENABLE) |
			     GCSETFIELD(0, GCREG_CMD_BUFFER_CTRL,
					PREFETCH, headcmdbuf->count));

		/* Release the command buffer thread. */
		complete(&gcqueue->ready);
	}

exit:
	/* Unlock queue acccess. */
	GCUNLOCK(&gcqueue->queuelock);

	return gcerror;
}

static int gccmdthread(void *_gccorecontext)
{
	struct gccorecontext *gccorecontext;
	struct gcqueue *gcqueue;
	unsigned long timeout, signaled;
	unsigned int i, triggered, ints2process, intmask;
	struct list_head *head;
	struct gccmdbuf *headcmdbuf;
	unsigned int flags;
	unsigned int dmapc, pc1, pc2;

	GCDBG(GCZONE_THREAD, "context = 0x%08X\n",
		   (unsigned int) _gccorecontext);

	/* Cast the context. */
	gccorecontext = (struct gccorecontext *) _gccorecontext;

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	GCDBG(GCZONE_THREAD, "starting command queue thread.\n");

	/* Set initial timeout to infinity since at this point GPU is
	 * considered to be powered off and there is no work pending or
	 * in progress yet. */
	timeout = MAX_SCHEDULE_TIMEOUT;

	/* Main thread loop. */
	while (true) {
		/* Wait for ready signal. If 'ready' is signaled before the
		 * call times out, signaled is set to a value greater then
		 * zero. If the call times out, signaled is set to zero. */
		signaled = wait_for_completion_interruptible_timeout(
			&gcqueue->ready, timeout);
		GCDBG(GCZONE_THREAD, "wait(ready) = %d.\n", signaled);

		if (signaled < 0)
			continue;

		/* Get triggered interrupts. */
		ints2process = triggered = atomic_read(&gcqueue->triggered);
		GCDBG(GCZONE_THREAD, "int = 0x%08X.\n", triggered);

		GCLOCK(&gcqueue->queuelock);

		/* Reset execution control flags. */
		flags = 0;

		/* Process triggered interrupts. */
		while (ints2process != 0) {
			GCDBG(GCZONE_INTERRUPT, "ints2process = 0x%08X.\n",
			      ints2process);

			/* Queue empty? */
			if (list_empty(&gcqueue->queue)) {
				GCERR("no match for triggered ints 0x%08X.\n",
				      ints2process);

				/* Reset triggered interrupts. */
				atomic_sub(triggered, &gcqueue->triggered);
				GCDBG(GCZONE_INTERRUPT, "triggered = 0x%08X.\n",
				      atomic_read(&gcqueue->triggered));

				/* Free triggered interrupts. */
				for (i = 0, intmask = 1; ints2process != 0;
					i += 1, intmask <<= 1) {
					GCDBG(GCZONE_INTERRUPT,
					      "ints2process = 0x%08X.\n",
					      ints2process);
					GCDBG(GCZONE_INTERRUPT,
					      "intmask = 0x%08X (%d).\n",
					      intmask, i);

					if ((ints2process & intmask) != 0) {
						free_interrupt(gcqueue, i);
						ints2process &= ~intmask;
					}
				}

				break;
			}

			/* Get the head entry from the queue. */
			head = gcqueue->queue.next;
			headcmdbuf = list_entry(head, struct gccmdbuf, link);

			GCDBG_QUEUE(GCZONE_THREAD, "processing ", headcmdbuf);

			/* Buffer with no events? */
			if (headcmdbuf->interrupt == ~0U) {
				GCDBG(GCZONE_THREAD,
				      "buffer has no interrupt.\n");

				/* Free the entry. */
				gcqueue_free_cmdbuf(gcqueue, headcmdbuf, NULL);
				continue;
			}

			/* Compute interrupt mask for the buffer. */
			intmask = 1 << headcmdbuf->interrupt;
			GCDBG(GCZONE_INTERRUPT, "intmask = 0x%08X.\n", intmask);

			/* Did interrupt happen for the head entry? */
			if ((ints2process & intmask) != 0) {
				ints2process &= ~intmask;
				GCDBG(GCZONE_INTERRUPT,
				      "ints2process = 0x%08X\n", ints2process);

				atomic_sub(intmask, &gcqueue->triggered);
				GCDBG(GCZONE_INTERRUPT, "triggered = 0x%08X.\n",
				      atomic_read(&gcqueue->triggered));
			} else {
				if (list_empty(&headcmdbuf->events)) {
					GCDBG(GCZONE_THREAD,
					      "possibility of a lost "
					      "interrupt %d.\n",
					      headcmdbuf->interrupt);
				} else {
					GCERR("lost interrupt %d.\n",
					      headcmdbuf->interrupt);
				}
			}

			/* Free the interrupt. */
			free_interrupt(gcqueue, headcmdbuf->interrupt);

			/* Execute events if any and free the entry. */
			gcqueue_free_cmdbuf(gcqueue, headcmdbuf, &flags);
		}

		GCUNLOCK(&gcqueue->queuelock);

		/* Bus error? */
		if (try_wait_for_completion(&gcqueue->buserror)) {
			GCERR("bus error detected.\n");
			GCGPUSTATUS();

			GCLOCK(&gcqueue->queuelock);

			/* Execute all pending events. */
			while (!list_empty(&gcqueue->queue)) {
				head = gcqueue->queue.next;
				headcmdbuf = list_entry(head,
							struct gccmdbuf,
							link);

				/* Execute events if any and free the entry. */
				gcqueue_free_cmdbuf(gcqueue, headcmdbuf,
						    &flags);
			}

			GCUNLOCK(&gcqueue->queuelock);

			/* Reset GPU. */
			gcpwr_reset(gccorecontext);
			continue;
		}

		/* MMU error? */
		if (try_wait_for_completion(&gcqueue->mmuerror)) {
			static char *mmuerror[] = {
				"  no error",
				"  slave not present",
				"  page not present",
				"  write violation"
			};

			unsigned int mmustatus, mmucode;
			unsigned int mmuaddress;
			unsigned int mtlb, stlb, offset;

			mmustatus = gc_read_reg(GCREG_MMU_STATUS_Address);
			GCERR("MMU error detected; status = 0x%08X.\n",
			      mmustatus);

			for (i = 0; i < 4; i += 1) {
				mmucode = mmustatus & 0xF;
				mmustatus >>= 4;

				if (mmucode == 0)
					continue;

				GCERR("MMU%d is at fault:\n", i);
				if (mmucode >= countof(mmuerror))
					GCERR("  unknown state %d.\n", mmucode);
				else
					GCERR("%s.\n", mmuerror[mmucode]);

				mmuaddress = gc_read_reg(
					GCREG_MMU_EXCEPTION_Address + i);

				mtlb = (mmuaddress & GCMMU_MTLB_MASK)
				     >> GCMMU_MTLB_SHIFT;
				stlb = (mmuaddress & GCMMU_STLB_MASK)
				     >> GCMMU_STLB_SHIFT;
				offset =  mmuaddress & GCMMU_OFFSET_MASK;

				GCERR("  address = 0x%08X\n", mmuaddress);
				GCERR("	 mtlb    = %d\n", mtlb);
				GCERR("	 stlb    = %d\n", stlb);
				GCERR("	 offset  = 0x%08X (%d)\n",
				      offset, offset);

				/* Route the invalid access to the safe zone. */
				gc_write_reg(GCREG_MMU_EXCEPTION_Address + i,
					gccorecontext->gcmmu.safezonephys);

				GCERR("	 rcovery address = 0x%08X\n",
				      gccorecontext->gcmmu.safezonephys);
			}

			continue;
		}

		/* Need to start FE? */
		if ((flags & GC_CMDBUF_START_FE) != 0) {
			GCLOCK(&gcqueue->queuelock);

			/* Get the first entry from the active queue. */
			head = gcqueue->queue.next;
			headcmdbuf = list_entry(head,
						struct gccmdbuf,
						link);

			GCDBG_QUEUE(GCZONE_THREAD, "restarting ", headcmdbuf);

			/* Write address register. */
			gc_write_reg(GCREG_CMD_BUFFER_ADDR_Address,
					headcmdbuf->physical);

			/* Write control register. */
			gc_write_reg(GCREG_CMD_BUFFER_CTRL_Address,
				GCSETFIELDVAL(0, GCREG_CMD_BUFFER_CTRL,
					ENABLE, ENABLE) |
				GCSETFIELD(0, GCREG_CMD_BUFFER_CTRL,
					PREFETCH, headcmdbuf->count));

			GCUNLOCK(&gcqueue->queuelock);
			continue;
		}

		if (signaled && !gcqueue->suspend) {
			/* The timeout value controls when the power
			 * on the GPU is pulled if there is no activity
			 * in progress or scheduled. */
			timeout = msecs_to_jiffies(GC_THREAD_TIMEOUT);
		} else {
			GCLOCK(&gcqueue->queuelock);

			if (gcqueue->suspend)
				GCDBG(GCZONE_THREAD, "suspend requested\n");

			if (!signaled)
				GCDBG(GCZONE_THREAD, "thread timedout.\n");

			if (gcqueue->gcmoterminator == NULL) {
				GCDBG(GCZONE_THREAD, "no work scheduled.\n");

				if (!list_empty(&gcqueue->queue))
					GCERR("queue is not empty.\n");

				gcqueue->suspend = false;

				/* Set timeout to infinity. */
				timeout = MAX_SCHEDULE_TIMEOUT;

				GCUNLOCK(&gcqueue->queuelock);
				continue;
			}

			/* Determine PC range for the terminatior. */
			pc1 = gcqueue->gcmoterminator->u3.linkwait.address;
			pc2 = pc1
			    + sizeof(struct gccmdwait)
			    + sizeof(struct gccmdlink);

			/* Check to see if the FE reached the terminatior. */
			dmapc = gc_read_reg(GCREG_FE_DEBUG_CUR_CMD_ADR_Address);
			if ((dmapc < pc1) || (dmapc > pc2)) {
				/* Haven't reached yet. */
				GCDBG(GCZONE_THREAD,
				      "execution hasn't reached the end; "
				      "large amount of work?\n");
				GCDBG(GCZONE_THREAD,
				      "current location @ 0x%08X.\n",
				      dmapc);
				GCUNLOCK(&gcqueue->queuelock);
				continue;
			}

			/* Free the queue. */
			while (!list_empty(&gcqueue->queue)) {
				head = gcqueue->queue.next;
				headcmdbuf = list_entry(head,
							struct gccmdbuf,
							link);

				if (!list_empty(&headcmdbuf->events)) {
					/* Found events, there must be
					 * pending interrupts. */
					break;
				}

				/* Free the entry. */
				gcqueue_free_cmdbuf(gcqueue, headcmdbuf,
						    NULL);
			}

			if (!list_empty(&gcqueue->queue)) {
				GCDBG(GCZONE_THREAD,
				      "aborting shutdown to process events\n");
				GCUNLOCK(&gcqueue->queuelock);
				continue;
			}

			GCDBG(GCZONE_THREAD,
			      "execution finished, shutting down.\n");

			/* Convert WAIT to END. */
			gcqueue->gcmoterminator->u2.end.cmd.raw
				= gccmdend_const.cmd.raw;
			mb();

			gcqueue->gcmoterminator = NULL;

			/* Go to suspend. */
			gcpwr_set(gccorecontext, GCPWR_OFF);

			/* Set idle state. */
			complete(&gcqueue->stopped);

			gcqueue->suspend = false;

			/* Set timeout to infinity. */
			timeout = MAX_SCHEDULE_TIMEOUT;

			GCUNLOCK(&gcqueue->queuelock);

			/* Is termination requested? */
			if (try_wait_for_completion(&gcqueue->stop)) {
				GCDBG(GCZONE_THREAD,
				      "terminating command queue thread.\n");
				break;
			}
		}
	}

	GCDBG(GCZONE_THREAD, "thread exiting.\n");
	return 0;
}

/*******************************************************************************
 * Command buffer API.
 */

enum gcerror gcqueue_start(struct gccorecontext *gccorecontext)
{
	enum gcerror gcerror;
	struct gcqueue *gcqueue;
	struct gccmdstorage *storage;
	unsigned int i;

	GCENTERARG(GCZONE_INIT, "context = 0x%08X\n",
		   (unsigned int) gccorecontext);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Reset the queue object. */
	memset(gcqueue, 0, sizeof(struct gcqueue));

	/* ISR not installed yet. */
	gcqueue->isrroutine = -1;

	/* Initialize all storage buffers. */
	for (i = 0; i < GC_STORAGE_COUNT; i += 1) {
		/* Get a shortcut to the current storage buffer. */
		storage = &gcqueue->storagearray[i];

		/* Allocate storage buffer. */
		gcerror = gc_alloc_noncached(&storage->page, GC_STORAGE_SIZE);
		if (gcerror != GCERR_NONE) {
			gcerror = GCERR_SETGRP(gcerror, GCERR_CMD_ALLOC);
			goto fail;
		}

		/* Reset buffer info. */
		storage->physical = storage->page.physical;
		storage->mapped = 0;

		init_completion(&storage->ready);
		complete(&storage->ready);

		GCDBG(GCZONE_INIT, "storage buffer [%d] allocated:\n", i);
		GCDBG(GCZONE_INIT, "  physical = 0x%08X\n", storage->physical);
		GCDBG(GCZONE_INIT, "  logical = 0x%08X\n",
		      (unsigned int) storage->page.logical);
		GCDBG(GCZONE_INIT, "  size = %d\n", storage->page.size);
	}

	/* Set the current storage buffer. */
	gcqueue->dirtystorage = true;
	gcqueue->curstorageidx = 0;
	gcqueue->curstorage = &gcqueue->storagearray[gcqueue->curstorageidx];

	/* Initialize current allocation info. */
	gcqueue->logical = (unsigned char *) gcqueue->curstorage->page.logical;
	gcqueue->physical = gcqueue->curstorage->physical;
	gcqueue->available = gcqueue->curstorage->page.size - GC_TAIL_RESERVE;

	/* Initialize interrupt tracking. */
	GCLOCK_INIT(&gcqueue->intusedlock);
	atomic_set(&gcqueue->triggered, 0);

	/* Mark all interrupts as available. */
	init_completion(&gcqueue->freeint);
	for (i = 0; i < countof(gcqueue->intused); i += 1)
		complete(&gcqueue->freeint);

	/* Set GPU running state. */
	init_completion(&gcqueue->stopped);
	complete(&gcqueue->stopped);

	/* Initialize sleep completion. */
	init_completion(&gcqueue->sleep);

	/* Initialize thread control completions. */
	init_completion(&gcqueue->ready);
	init_completion(&gcqueue->stop);
	init_completion(&gcqueue->stall);

	/* Initialize error signals. */
	init_completion(&gcqueue->mmuerror);
	init_completion(&gcqueue->buserror);

	/* Initialize the current command buffer entry. */
	INIT_LIST_HEAD(&gcqueue->cmdbufhead);

	/* Initialize the schedule and active queue. */
	INIT_LIST_HEAD(&gcqueue->queue);
	GCLOCK_INIT(&gcqueue->queuelock);

	/* Initialize entry cache. */
	INIT_LIST_HEAD(&gcqueue->vacevents);
	INIT_LIST_HEAD(&gcqueue->vacqueue);
	GCLOCK_INIT(&gcqueue->vaceventlock);
	GCLOCK_INIT(&gcqueue->vacqueuelock);

	/* Reset MMU flush state. */
	gcqueue->flushlogical = NULL;

	/* Install ISR. */
	gcqueue->isrroutine = request_irq(gccorecontext->irqline, gcisr,
					  IRQF_SHARED, GC_DEV_NAME,
					  gccorecontext);
	if (gcqueue->isrroutine < 0) {
		GCERR("failed to install ISR (%d).\n", gcqueue->isrroutine);
		gcerror = GCERR_CMD_ISR;
		goto fail;
	}

	GCDBG(GCZONE_INIT, "starting the command queue thread.\n");
	gcqueue->cmdthread = kthread_create(gccmdthread, gccorecontext,
						"gccmdthread");
	if (IS_ERR(gcqueue->cmdthread)) {
		GCERR("failed to start command thread.\n");
		gcqueue->cmdthread = NULL;
		gcerror = GCERR_CMD_THREAD;
		goto fail;
	}

	wake_up_process(gcqueue->cmdthread);

	GCEXIT(GCZONE_INIT);
	return GCERR_NONE;

fail:
	gcqueue_stop(gccorecontext);
	GCEXITARG(GCZONE_INIT, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

enum gcerror gcqueue_stop(struct gccorecontext *gccorecontext)
{
	struct gcqueue *gcqueue;
	struct gccmdstorage *storage;
	struct list_head *head;
	struct gccmdbuf *gccmdbuf;
	struct gcevent *gcevent;
	unsigned int i;

	GCENTERARG(GCZONE_INIT, "context = 0x%08X\n",
		   (unsigned int) gccorecontext);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Stop the command buffer thread. */
	if (gcqueue->cmdthread != NULL) {
		GCDBG(GCZONE_INIT, "stopping the command queue thread.\n");
		complete(&gcqueue->stop);
		complete(&gcqueue->ready);
		kthread_stop(gcqueue->cmdthread);
		gcqueue->cmdthread = NULL;
		GCDBG(GCZONE_INIT, "command queue thread stopped.\n");
	}

	/* Remove ISR routine. */
	if (gcqueue->isrroutine == 0) {
		GCDBG(GCZONE_ISR, "removing ISR.\n");
		free_irq(gccorecontext->irqline, gccorecontext);
		gcqueue->isrroutine = -1;
	}

	/* Free all storage buffers. */
	for (i = 0; i < GC_STORAGE_COUNT; i += 1) {
		/* Get a shortcut to the current storage buffer. */
		storage = &gcqueue->storagearray[i];

		/* Verify that the storage buffer is not mapped. */
		if (storage->mapped != 0)
			GCERR("storage buffer is mapped.\n");

		/* Free the buffer. */
		if (storage->page.logical != NULL) {
			gc_free_noncached(&storage->page);
			storage->physical = ~0U;
		}
	}

	/* Reset current buffer info. */
	gcqueue->logical = NULL;
	gcqueue->physical = ~0U;
	gcqueue->available = 0;

	/* Free the current command buffer. */
	while (!list_empty(&gcqueue->cmdbufhead)) {
		head = gcqueue->cmdbufhead.next;
		gccmdbuf = list_entry(head, struct gccmdbuf, link);
		gcqueue_free_cmdbuf(gcqueue, gccmdbuf, NULL);
	}

	/* Free command queue entries. */
	while (!list_empty(&gcqueue->queue)) {
		head = gcqueue->queue.next;
		gccmdbuf = list_entry(head, struct gccmdbuf, link);
		gcqueue_free_cmdbuf(gcqueue, gccmdbuf, NULL);
	}

	/* Free vacant entry lists. */
	while (!list_empty(&gcqueue->vacevents)) {
		head = gcqueue->vacevents.next;
		gcevent = list_entry(head, struct gcevent, link);
		list_del(head);
		kfree(gcevent);
	}

	while (!list_empty(&gcqueue->vacqueue)) {
		head = gcqueue->vacqueue.next;
		gccmdbuf = list_entry(head, struct gccmdbuf, link);
		list_del(head);
		kfree(gccmdbuf);
	}

	GCEXIT(GCZONE_INIT);
	return GCERR_NONE;
}

enum gcerror gcqueue_map(struct gccorecontext *gccorecontext,
				struct gcmmucontext *gcmmucontext)
{
	enum gcerror gcerror;
	struct gcqueue *gcqueue;
	struct gccmdstorage *gccmdstorage;
	struct gcmmuphysmem mem;
	pte_t physpages[GC_STORAGE_PAGES];
	unsigned int i, j, physical;

	GCENTERARG(GCZONE_MAPPING, "context = 0x%08X\n",
		   (unsigned int) gccorecontext);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Map all command buffers. */
	for (i = 0; i < GC_STORAGE_COUNT; i += 1) {
		/* Get a shortcut to the current storage buffer. */
		gccmdstorage = &gcqueue->storagearray[i];

		/* Make sure command buffer hasn't been mapped yet. */
		if (gcmmucontext->storagearray[i] != NULL) {
			GCERR("storage %d is already mapped.\n", i);
			gcerror = GCERR_CMD_MAPPED;
			goto fail;
		}

		GCDBG(GCZONE_MAPPING, "maping storage %d.\n", i);
		GCDBG(GCZONE_MAPPING, "  physical = 0x%08X\n",
		      gccmdstorage->page.physical);
		GCDBG(GCZONE_MAPPING, "  logical = 0x%08X\n",
		      (unsigned int) gccmdstorage->page.logical);
		GCDBG(GCZONE_MAPPING, "  size = %d\n",
		      gccmdstorage->page.size);

		/* Get physical pages. */
		physical = gccmdstorage->page.physical;
		for (j = 0; j < GC_STORAGE_PAGES; j += 1) {
			physpages[j] = physical;
			physical += PAGE_SIZE;
		}

		/* Map the buffer. */
		mem.base = 0;
		mem.offset = 0;
		mem.count = GC_STORAGE_PAGES;
		mem.pages = physpages;
		mem.pagesize = PAGE_SIZE;

		gcerror = gcmmu_map(gccorecontext, gcmmucontext, &mem,
				    &gcmmucontext->storagearray[i]);
		if (gcerror != 0) {
			GCERR("failed to map storage %d.\n", i);
			goto fail;
		}

		physical = gcmmucontext->storagearray[i]->address;
		if (gccmdstorage->mapped) {
			GCDBG(GCZONE_MAPPING,
			      "  storage %d is already mapped.\n", i);

			if (gccmdstorage->physical != physical) {
				GCERR("storage %d, inconsistent mapping!\n", i);
				gcerror = GCERR_CMD_CONSISTENCY;
				goto fail;
			}
		} else {
			GCDBG(GCZONE_MAPPING,
			      "  mapped storage %d @ 0x%08X.\n",
			      i, physical);

			/* Not mapped yet, replace the physical address with
			 * the mapped one. */
			gccmdstorage->physical = physical;

			/* Update the allocatable address. */
			if (gccmdstorage == gcqueue->curstorage) {
				gcqueue->physical = gccmdstorage->physical;
				GCDBG(GCZONE_MAPPING,
				      "  setting current physical address.\n");
			}
		}

		gccmdstorage->mapped += 1;
	}

	GCEXIT(GCZONE_MAPPING);
	return GCERR_NONE;

fail:
	gcqueue_unmap(gccorecontext, gcmmucontext);
	GCEXITARG(GCZONE_MAPPING, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

enum gcerror gcqueue_unmap(struct gccorecontext *gccorecontext,
				struct gcmmucontext *gcmmucontext)
{
	enum gcerror gcerror;
	struct gcqueue *gcqueue;
	struct gccmdstorage *gccmdstorage;
	unsigned int i;

	GCENTERARG(GCZONE_MAPPING, "context = 0x%08X\n",
		   (unsigned int) gccorecontext);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Map all command buffers. */
	for (i = 0; i < GC_STORAGE_COUNT; i += 1) {
		/* Get a shortcut to the current command buffer structure. */
		gccmdstorage = &gcqueue->storagearray[i];

		/* Make sure command buffer is mapped. */
		if (gcmmucontext->storagearray[i] == NULL)
			continue;

		gcerror = gcmmu_unmap(gccorecontext, gcmmucontext,
				      gcmmucontext->storagearray[i]);
		if (gcerror != 0) {
			GCERR("failed to unmap command buffer %d.\n", i);
			goto fail;
		}

		/* Reset mapping. */
		gcmmucontext->storagearray[i] = NULL;
		gccmdstorage->mapped -= 1;
	}

	GCEXIT(GCZONE_MAPPING);
	return GCERR_NONE;

fail:
	GCEXITARG(GCZONE_MAPPING, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

enum gcerror gcqueue_callback(struct gccorecontext *gccorecontext,
			      struct gcmmucontext *gcmmucontext,
			      void (*callback) (void *callbackparam),
			      void *callbackparam)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcqueue *gcqueue;
	struct gccmdbuf *gccmdbuf;
	struct list_head *head;
	struct gcevent *gcevent;

	GCENTER(GCZONE_EVENT);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Allocate command buffer. */
	if (list_empty(&gcqueue->cmdbufhead)) {
		gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
					0, NULL, NULL);
		if (gcerror != GCERR_NONE)
			goto exit;
	}

	/* Get the current command buffer. */
	head = gcqueue->cmdbufhead.prev;
	gccmdbuf = list_entry(head, struct gccmdbuf, link);

	/* Add callback event. */
	gcerror = gcqueue_alloc_event(gcqueue, &gcevent);
	if (gcerror != GCERR_NONE)
		goto exit;

	/* Initialize the event and add to the list. */
	gcevent->handler = event_callback;
	gcevent->event.callback.callback = callback;
	gcevent->event.callback.callbackparam = callbackparam;
	list_add_tail(&gcevent->link, &gccmdbuf->events);

	GCDBG(GCZONE_EVENT, "callback      = 0x%08X\n",
		(unsigned int) callback);
	GCDBG(GCZONE_EVENT, "callbackparam = 0x%08X\n",
		(unsigned int) callbackparam);

exit:
	GCEXIT(GCZONE_EVENT);
	return gcerror;
}

enum gcerror gcqueue_schedunmap(struct gccorecontext *gccorecontext,
				struct gcmmucontext *gcmmucontext,
				unsigned long handle)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcqueue *gcqueue;
	struct gccmdbuf *gccmdbuf;
	struct list_head *head;
	struct gcevent *gcevent;

	GCENTER(GCZONE_EVENT);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Allocate command buffer. */
	if (list_empty(&gcqueue->cmdbufhead)) {
		gcerror = gcqueue_alloc(gccorecontext, gcmmucontext,
					0, NULL, NULL);
		if (gcerror != GCERR_NONE)
			goto exit;
	}

	/* Get the current command buffer. */
	head = gcqueue->cmdbufhead.prev;
	gccmdbuf = list_entry(head, struct gccmdbuf, link);

	/* Add callback event. */
	gcerror = gcqueue_alloc_event(gcqueue, &gcevent);
	if (gcerror != GCERR_NONE)
		goto exit;

	/* Initialize the event and add to the list. */
	gcevent->handler = event_unmap;
	gcevent->event.unmap.gccorecontext = gccorecontext;
	gcevent->event.unmap.gcmmucontext = gcmmucontext;
	gcevent->event.unmap.gcmmuarena = (struct gcmmuarena *) handle;
	list_add_tail(&gcevent->link, &gccmdbuf->events);

	GCDBG(GCZONE_EVENT, "handle = 0x%08X\n", handle);

exit:
	GCEXIT(GCZONE_EVENT);
	return gcerror;
}

enum gcerror gcqueue_alloc(struct gccorecontext *gccorecontext,
			   struct gcmmucontext *gcmmucontext,
			   unsigned int size,
			   void **logical,
			   unsigned int *physical)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcqueue *gcqueue;
	struct gccmdbuf *gccmdbuf;
	struct list_head *head;

	GCENTERARG(GCZONE_ALLOC, "context = 0x%08X, size = %d\n",
		   (unsigned int) gccorecontext, size);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	while (true) {
		/* Wait until the storage buffer becomes available. */
		if (gcqueue->dirtystorage) {
			if (!try_wait_for_completion(
					&gcqueue->curstorage->ready)) {
				GCDBG(GCZONE_ALLOC,
					"waiting for the storage buffer"
					" to become available.\n");

				GCWAIT_FOR_COMPLETION(
					&gcqueue->curstorage->ready);
			}

			GCDBG(GCZONE_ALLOC, "using storage buffer #%d.\n",
				gcqueue->curstorageidx);

			gcqueue->dirtystorage = false;
		}

		GCDBG(GCZONE_ALLOC, "queue logical = 0x%08X\n",
		      (unsigned int) gcqueue->logical);
		GCDBG(GCZONE_ALLOC, "queue physical = 0x%08X\n",
		      gcqueue->physical);
		GCDBG(GCZONE_ALLOC, "queue available = %d\n",
			gcqueue->available);

		/* Create a new command buffer entry if not created yet. */
		if (list_empty(&gcqueue->cmdbufhead)) {
			GCDBG(GCZONE_ALLOC, "allocating new queue entry.\n");

			gcerror = gcqueue_alloc_cmdbuf(gcqueue, &gccmdbuf);
			if (gcerror != GCERR_NONE)
				goto exit;

			list_add_tail(&gccmdbuf->link, &gcqueue->cmdbufhead);

			gccmdbuf->gcmmucontext = gcmmucontext;
			gccmdbuf->logical = gcqueue->logical;
			gccmdbuf->physical = gcqueue->physical;
			gccmdbuf->size = 0;
			gccmdbuf->count = 0;
			gccmdbuf->gcmoterminator = NULL;
			gccmdbuf->interrupt = ~0U;
		} else {
			head = gcqueue->cmdbufhead.next;
			gccmdbuf = list_entry(head, struct gccmdbuf, link);
		}

		/* Do we have enough room in the current buffer? */
		if ((int) size <= gcqueue->available) {
			/* Set the pointers. */
			if (logical != NULL)
				*logical = gcqueue->logical;

			if (physical != NULL)
				*physical = gcqueue->physical;

			/* Update the info. */
			gcqueue->logical += size;
			gcqueue->physical += size;
			gcqueue->available -= size;
			gccmdbuf->size += size;

			GCDBG_QUEUE(GCZONE_ALLOC, "updated ", gccmdbuf);
			goto exit;
		}

		/* Execute the current command buffer. */
		GCDBG_QUEUE(GCZONE_ALLOC, "current ", gccmdbuf);
		GCDBG(GCZONE_ALLOC, "out of available space.\n");
		gcerror = gcqueue_execute(gccorecontext, true, true);
		if (gcerror != GCERR_NONE)
			goto exit;
	}

exit:
	GCEXITARG(GCZONE_ALLOC, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);

	return gcerror;
}

enum gcerror gcqueue_free(struct gccorecontext *gccorecontext,
			  unsigned int size)
{
	struct list_head *head;
	struct gcqueue *gcqueue;
	struct gccmdbuf *gccmdbuf;

	GCENTERARG(GCZONE_ALLOC, "context = 0x%08X, size = %d\n",
		   (unsigned int) gccorecontext, size);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	if (list_empty(&gcqueue->cmdbufhead)) {
		GCERR("no current command buffer\n");
	} else {
		/* Get a pointer to the open entry. */
		head = gcqueue->cmdbufhead.next;
		gccmdbuf = list_entry(head, struct gccmdbuf, link);

		/* Roll back the previous allocation. */
		gcqueue->logical -= size;
		gcqueue->physical -= size;
		gcqueue->available += size;
		gccmdbuf->size -= size;
	}

	GCEXIT(GCZONE_ALLOC);
	return GCERR_NONE;
}

enum gcerror gcqueue_execute(struct gccorecontext *gccorecontext,
			     bool switchtonext, bool asynchronous)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcqueue *gcqueue;
	struct gcmoterminator *gcmoterminator;
	struct gcevent *gcevent;
	struct gccmdbuf *gccmdbuf;
	struct list_head *head;

	GCENTERARG(GCZONE_EXEC, "context = 0x%08X, asynchronous = %d\n",
		   (unsigned int) gccorecontext, asynchronous);

	/* Get a shortcut to the queue object. */
	gcqueue = &gccorecontext->gcqueue;

	/* Nothing to execute? */
	if (list_empty(&gcqueue->cmdbufhead))
		goto exit;

	/* Get the current command buffer. */
	head = gcqueue->cmdbufhead.prev;
	gccmdbuf = list_entry(head, struct gccmdbuf, link);

	/* Determine the location of the terminator. */
	gccmdbuf->gcmoterminator = gcmoterminator
		= (struct gcmoterminator *) gcqueue->logical;

	/* Configure the second entry as a wait. */
	gcmoterminator->u2.wait.cmd.fld = gcfldwait200;

	/* Configure the third entry as a link back to the wait. */
	gcmoterminator->u3.linkwait.cmd.fld = gcfldlink2;
	gcmoterminator->u3.linkwait.address = gcqueue->physical
				+ offsetof(struct gcmoterminator, u2);

	/* Update the info. */
	gcqueue->logical += GC_TAIL_RESERVE;
	gcqueue->physical += GC_TAIL_RESERVE;
	gcqueue->available -= GC_TAIL_RESERVE;
	gccmdbuf->size += GC_TAIL_RESERVE;

	GCDBG(GCZONE_EXEC, "queue logical = 0x%08X\n",
		(unsigned int) gcqueue->logical);
	GCDBG(GCZONE_EXEC, "queue physical = 0x%08X\n",
		gcqueue->physical);
	GCDBG(GCZONE_EXEC, "queue available = %d\n",
		gcqueue->available);

	/* Determine data count. */
	gccmdbuf->count = (gccmdbuf->size + 7) >> 3;

	/* Is there an MMU flush in the buffer? */
	if (gcqueue->flushlogical != NULL) {
		GCDBG(GCZONE_EXEC, "finalizing MMU flush.\n");

		/* Finalize the flush. */
		gcmmu_flush_finalize(gccmdbuf,
					gcqueue->flushlogical,
					gcqueue->flushaddress);

		/* Reset flush pointer. */
		gcqueue->flushlogical = NULL;
	}

	GCDBG_QUEUE(GCZONE_EXEC, "current ", gccmdbuf);

	/* Check the remaining space minimum threshold. */
	if (switchtonext || (gcqueue->available < GC_MIN_THRESHOLD)) {
		GCDBG(GCZONE_EXEC, "switching to the next storage.\n");

		/* Add event for the current command buffer. */
		gcerror = gcqueue_alloc_event(gcqueue, &gcevent);
		if (gcerror != GCERR_NONE)
			goto exit;

		/* Initialize the event and add to the list. */
		gcevent->handler = event_completion;
		gcevent->event.completion.completion
			= &gcqueue->curstorage->ready;
		list_add_tail(&gcevent->link, &gccmdbuf->events);
		GCDBG(GCZONE_EXEC, "buffer switch completion 0x%08X.\n",
		      (unsigned int) gcevent->event.completion.completion);

		/* Switch to the next storage buffer. */
		gcqueue->curstorageidx = (gcqueue->curstorageidx + 1)
					% GC_STORAGE_COUNT;
		gcqueue->curstorage = &gcqueue->storagearray
					[gcqueue->curstorageidx];

		/* Initialize current allocation info. */
		gcqueue->logical = (unsigned char *)
				gcqueue->curstorage->page.logical;
		gcqueue->physical = gcqueue->curstorage->physical;
		gcqueue->available = gcqueue->curstorage->page.size
					- GC_TAIL_RESERVE;

		/* Invalidate the storage. */
		gcqueue->dirtystorage = true;

		GCDBG(GCZONE_EXEC, "switching to storage %d.\n",
		      gcqueue->curstorageidx);
	}

	/* Add stall event for synchronous operation. */
	if (!asynchronous) {
		GCDBG(GCZONE_EXEC, "appending stall event.\n");

		/* Add stall event. */
		gcerror = gcqueue_alloc_event(gcqueue, &gcevent);
		if (gcerror != GCERR_NONE)
			goto exit;

		/* Initialize the event and add to the list. */
		gcevent->handler = event_completion;
		gcevent->event.completion.completion = &gcqueue->stall;
		list_add_tail(&gcevent->link, &gccmdbuf->events);
		GCDBG(GCZONE_EXEC, "stall completion 0x%08X.\n",
		      (unsigned int) gcevent->event.completion.completion);
	}

	/* If the buffer has no events, don't allocate an interrupt for it. */
	if (list_empty(&gccmdbuf->events)) {
		gcmoterminator->u1.nop.cmd.raw = gccmdnop_const.cmd.raw;
	} else {
		gcerror = gcqueue_alloc_int(gcqueue, &gccmdbuf->interrupt);
		if (gcerror != GCERR_NONE)
			goto exit;

		gcmoterminator->u1.done.signal_ldst = gcmosignal_signal_ldst;
		gcmoterminator->u1.done.signal.raw = 0;
		gcmoterminator->u1.done.signal.reg.id = gccmdbuf->interrupt;
		gcmoterminator->u1.done.signal.reg.pe
						= GCREG_EVENT_PE_SRC_ENABLE;
	}

	/* Append the current command buffer to the queue. */
	append_cmdbuf(gccorecontext, gcqueue);

	/* Wait for completion. */
	if (!asynchronous) {
		GCDBG(GCZONE_EXEC, "waiting until execution is complete.\n");
		GCWAIT_FOR_COMPLETION(&gcqueue->stall);
		GCDBG(GCZONE_EXEC, "execution complete.\n");
	}

exit:
	GCEXITARG(GCZONE_EXEC, "gc%s = 0x%08X\n",
		(gcerror == GCERR_NONE) ? "result" : "error", gcerror);
	return gcerror;
}

enum gcerror gcqueue_wait_idle(struct gccorecontext *gccorecontext)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcqueue *gcqueue = &gccorecontext->gcqueue;
	unsigned long timeout;
	unsigned int count, limit;

	GCENTER(GCZONE_THREAD);

	/* Indicate shutdown immediately. */
	gcqueue->suspend = true;
	complete(&gcqueue->ready);

	/* Convert timeout to jiffies. */
	timeout = msecs_to_jiffies(GC_IDLE_TIMEOUT);

	/* Compute the maximum number of attempts. */
	limit = 5000 / GC_IDLE_TIMEOUT;

	/* Wait for GPU to stop. */
	count = 0;
	while (gcqueue->suspend) {
		wait_for_completion_timeout(&gcqueue->sleep, timeout);

		/* Waiting too long? */
		if (++count == limit) {
			GCERR("wait for idle takes too long.\n");
			gcerror = GCERR_TIMEOUT;
			break;
		}
	}

	GCEXIT(GCZONE_THREAD);
	return gcerror;
}
