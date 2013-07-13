/*
 * gcqueue.h
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

#ifndef GCQUEUE_H
#define GCQUEUE_H

#include <linux/gccore.h>


/*******************************************************************************
 * Command queue defines.
 */

/* Number of the storage buffers that the driver will switch between. */
#define GC_STORAGE_COUNT	2

/* Number of command buffers that fit in one storage buffer. */
#define GC_CMDBUF_FACTOR	2


/*******************************************************************************
 * Command queue structures.
 */

/* Execution control flags. */
#define GC_CMDBUF_START_FE	(1 << 0)

/* Event record. */
struct gcevent {
	/* Event handler function. */
	void (*handler) (struct gcevent *gcevent, unsigned int *flags);

	/* Event specific data. */
	union {
		struct {
			struct completion *completion;
		} completion;

		struct {
			void (*callback) (void *callbackparam);
			void *callbackparam;
		} callback;

		struct {
			struct gccorecontext *gccorecontext;
			struct gcmmucontext *gcmmucontext;
			struct gcmmuarena *gcmmuarena;
		} unmap;
	} event;

	/* Previous/next event link. */
	struct list_head link;
};

/* Command buffer storage descriptor. This represents a container within
 * which smaller allocations are made, filled with commands and executed.
 * There should be at least two of these storage buffers to allow for seamless
 * execution. When there is no more room in the current storage buffer, the
 * buffer is sent for execution while allocation can continue from the next
 * storage buffer. */
struct gccmdstorage {
	/* Storage buffer allocation descriptor. */
	struct gcpage page;

	/* Physical (GPU mapped) address of the storage buffer. */
	unsigned int physical;

	/* Number of clients that have the storage buffer mapped.*/
	unsigned int mapped;

	/* Completion used for switching to this storage buffer. */
	struct completion ready;
};

/* Command queue entry. */
struct gccmdbuf {
	/* Associated MMU context. */
	struct gcmmucontext *gcmmucontext;

	/* Pointers to the beginning of the command buffer and the size
	 * of the command buffer in bytes. */
	unsigned char *logical;
	unsigned int physical;
	unsigned int size;

	/* The size of the command buffer in the number if 64-bit chunks. */
	unsigned int count;

	/* Command buffer terminator structure. */
	struct gcmoterminator *gcmoterminator;

	/* Interrupt number assigned to the command buffer. */
	unsigned int interrupt;

	/* Event list associated with the command buffer. */
	struct list_head events;

	/* Previous/next command queue entry. */
	struct list_head link;
};

/* Command queue object. */
struct gcqueue {
	/* ISR installed flag. */
	int isrroutine;

	/* Storage buffer array. */
	bool dirtystorage;
	struct gccmdstorage storagearray[GC_STORAGE_COUNT];
	struct gccmdstorage *curstorage;
	unsigned int curstorageidx;

	/* Pointers to the area of the current storage buffer available for
	 * allocation and the size of the available space in bytes. */
	unsigned char *logical;
	unsigned int physical;
	int available;

	/* Array to keep track of which interrupts are in use. */
	bool intused[30];
	GCLOCK_TYPE intusedlock;

	/* The completion to track the number of available interrupts. */
	struct completion freeint;

	/* Bit mask containing triggered interrupts. */
	atomic_t triggered;

	/* The tail of the last command buffer. */
	struct gcmoterminator *gcmoterminator;

	/* GPU running state. */
	struct completion stopped;

	/* Command buffer thread and thread control completions. */
	struct task_struct *cmdthread;
	struct completion ready;
	struct completion stop;
	struct completion sleep;

	/* Suspend request flag. */
	bool suspend;

	/* Stall completion; used to imitate synchronous behaviour. */
	struct completion stall;

	/* Error signals. */
	struct completion mmuerror;
	struct completion buserror;

	/* Command buffer currently being worked on. */
	struct list_head cmdbufhead;

	/* Queue of entries being executed (gccmdbuf). */
	struct list_head queue;
	GCLOCK_TYPE queuelock;

	/* Cache of vacant event entries (gcevent). */
	struct list_head vacevents;
	GCLOCK_TYPE vaceventlock;

	/* Cache of vacant queue entries (gccmdbuf). */
	struct list_head vacqueue;
	GCLOCK_TYPE vacqueuelock;

	/* MMU flush pointers. */
	struct gcmommuflush *flushlogical;
	unsigned int flushaddress;
};


/*******************************************************************************
 * Command queue management API.
 */

struct gccorecontext;
struct gcmmucontext;

enum gcerror gcqueue_start(struct gccorecontext *gccorecontext);
enum gcerror gcqueue_stop(struct gccorecontext *gccorecontext);

enum gcerror gcqueue_map(struct gccorecontext *gccorecontext,
			 struct gcmmucontext *gcmmucontext);
enum gcerror gcqueue_unmap(struct gccorecontext *gccorecontext,
			   struct gcmmucontext *gcmmucontext);

enum gcerror gcqueue_callback(struct gccorecontext *gccorecontext,
			      struct gcmmucontext *gcmmucontext,
			      void (*callback) (void *callbackparam),
			      void *callbackparam);
enum gcerror gcqueue_schedunmap(struct gccorecontext *gccorecontext,
				struct gcmmucontext *gcmmucontext,
				unsigned long handle);
enum gcerror gcqueue_alloc(struct gccorecontext *gccorecontext,
			   struct gcmmucontext *gcmmucontext,
			   unsigned int size,
			   void **logical,
			   unsigned int *physical);
enum gcerror gcqueue_free(struct gccorecontext *gccorecontext,
			  unsigned int size);
enum gcerror gcqueue_execute(struct gccorecontext *gccorecontext,
			     bool switchtonext, bool asynchronous);

enum gcerror gcqueue_alloc_event(struct gcqueue *gcqueue,
				 struct gcevent **gcevent);
enum gcerror gcqueue_free_event(struct gcqueue *gcqueue,
				struct gcevent *gcevent);

enum gcerror gcqueue_alloc_cmdbuf(struct gcqueue *gcqueue,
				  struct gccmdbuf **gccmdbuf);
void gcqueue_free_cmdbuf(struct gcqueue *gcqueue,
			 struct gccmdbuf *gccmdbuf,
			 unsigned int *flags);

enum gcerror gcqueue_alloc_int(struct gcqueue *gcqueue,
			       unsigned int *interrupt);

enum gcerror gcqueue_wait_idle(struct gccorecontext *gccorecontext);

#endif
