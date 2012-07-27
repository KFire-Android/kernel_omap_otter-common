/*
 * gcmain.c
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <plat/cpu.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/gcx.h>
#include <linux/gccore.h>
#include <linux/cache-2dmanager.h>
#include "gcif.h"
#include "version.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_CAPS		(1 << 1)
#define GCZONE_MAPPING		(1 << 2)
#define GCZONE_CACHE		(1 << 3)
#define GCZONE_COMMIT		(1 << 4)
#define GCZONE_IOCTL		(1 << 5)
#define GCZONE_CALLBACK		(1 << 6)

GCDBG_FILTERDEF(ioctl, GCZONE_NONE,
		"init",
		"caps",
		"mapping",
		"cache",
		"commit",
		"ioctl",
		"callback")

static GCDEFINE_LOCK(g_fixuplock);
static GCDEFINE_LOCK(g_bufferlock);
static GCDEFINE_LOCK(g_unmaplock);

static LIST_HEAD(g_buffervac);		/* gcbuffer */
static LIST_HEAD(g_fixupvac);		/* gcfixup */
static LIST_HEAD(g_unmapvac);		/* gcschedunmap */


/*******************************************************************************
 * Command buffer copy management.
 */

static enum gcerror alloc_fixup(struct gcfixup **gcfixup)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcfixup *temp;

	GCLOCK(&g_fixuplock);

	if (list_empty(&g_fixupvac)) {
		temp = kmalloc(sizeof(struct gcfixup), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("out of memory.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
					       GCERR_IOCTL_FIXUP_ALLOC);
			goto exit;
		}
	} else {
		struct list_head *head;
		head = g_fixupvac.next;
		temp = list_entry(head, struct gcfixup, link);
		list_del(head);
	}

	GCUNLOCK(&g_fixuplock);

	INIT_LIST_HEAD(&temp->link);
	*gcfixup = temp;

exit:
	return gcerror;
}

static void free_fixup(struct gcfixup *gcfixup)
{
	GCLOCK(&g_fixuplock);
	list_move(&gcfixup->link, &g_fixupvac);
	GCUNLOCK(&g_fixuplock);
}

static void free_fixup_list(struct list_head *fixuplist)
{
	GCLOCK(&g_fixuplock);
	list_splice_init(fixuplist, &g_fixupvac);
	GCUNLOCK(&g_fixuplock);
}

static void free_vacant_fixups(void)
{
	struct list_head *head;
	struct gcfixup *gcfixup;

	GCLOCK(&g_fixuplock);
	while (!list_empty(&g_fixupvac)) {
		head = g_fixupvac.next;
		gcfixup = list_entry(head, struct gcfixup, link);
		list_del(head);
		kfree(gcfixup);
	}
	GCUNLOCK(&g_fixuplock);
}

static enum gcerror alloc_buffer(struct gcbuffer **gcbuffer)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcbuffer *temp;

	GCLOCK(&g_bufferlock);

	if (list_empty(&g_buffervac)) {
		temp = kmalloc(sizeof(struct gcbuffer), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("out of memory.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
					       GCERR_IOCTL_BUF_ALLOC);
			goto exit;
		}
	} else {
		struct list_head *head;
		head = g_buffervac.next;
		temp = list_entry(head, struct gcbuffer, link);
		list_del(head);
	}

	GCUNLOCK(&g_bufferlock);

	INIT_LIST_HEAD(&temp->fixup);
	INIT_LIST_HEAD(&temp->link);
	*gcbuffer = temp;

exit:
	return gcerror;
}

static void free_buffer(struct gcbuffer *gcbuffer)
{
	/* Free fixups. */
	free_fixup_list(&gcbuffer->fixup);

	/* Free the buffer. */
	GCLOCK(&g_bufferlock);
	list_move(&gcbuffer->link, &g_buffervac);
	GCUNLOCK(&g_bufferlock);
}

static void free_buffer_list(struct list_head *bufferlist)
{
	struct list_head *head;
	struct gcbuffer *gcbuffer;

	while (!list_empty(bufferlist)) {
		head = bufferlist->next;
		gcbuffer = list_entry(head, struct gcbuffer, link);
		free_buffer(gcbuffer);
	}
}

static void free_vacant_buffers(void)
{
	struct list_head *head;
	struct gcbuffer *gcbuffer;

	GCLOCK(&g_bufferlock);
	while (!list_empty(&g_buffervac)) {
		head = g_buffervac.next;
		gcbuffer = list_entry(head, struct gcbuffer, link);
		list_del(head);
		kfree(gcbuffer);
	}
	GCUNLOCK(&g_bufferlock);
}


/*******************************************************************************
 * Unmap list management.
 */

static enum gcerror alloc_schedunmap(struct gcschedunmap **gcschedunmap)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcschedunmap *temp;

	GCLOCK(&g_unmaplock);

	if (list_empty(&g_unmapvac)) {
		temp = kmalloc(sizeof(struct gcschedunmap), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("out of memory.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
					       GCERR_IOCTL_BUF_ALLOC);
			goto exit;
		}
	} else {
		struct list_head *head;
		head = g_unmapvac.next;
		temp = list_entry(head, struct gcschedunmap, link);
		list_del(head);
	}

	GCUNLOCK(&g_unmaplock);

	INIT_LIST_HEAD(&temp->link);
	*gcschedunmap = temp;

exit:
	return gcerror;
}

static void free_schedunmap(struct gcschedunmap *gcschedunmap)
{
	GCLOCK(&g_unmaplock);
	list_move(&gcschedunmap->link, &g_unmapvac);
	GCUNLOCK(&g_unmaplock);
}

static void free_schedunmap_list(struct list_head *schedunmaplist)
{
	GCLOCK(&g_unmaplock);
	list_splice_init(schedunmaplist, &g_unmapvac);
	GCUNLOCK(&g_unmaplock);
}

static void free_vacant_unmap(void)
{
	struct list_head *head;
	struct gcschedunmap *gcschedunmap;

	GCLOCK(&g_unmaplock);
	while (!list_empty(&g_unmapvac)) {
		head = g_unmapvac.next;
		gcschedunmap = list_entry(head, struct gcschedunmap, link);
		list_del(head);
		kfree(gcschedunmap);
	}
	GCUNLOCK(&g_unmaplock);
}


/*******************************************************************************
 * Callback managemnent.
 */

/* Per-client callback handle. */
struct gccallbackhandle {
	/* Ready signal. */
	struct completion ready;

	/* List of triggered callbacks (gccallbackinfo). */
	struct list_head triggered;

	/* List of scheduled callbacks (gccallbackinfo). */
	struct list_head scheduled;

	/* Previous/next callback handle. */
	struct list_head link;
};

/* Callback information. */
struct gccallbackinfo {
	/* Callback handle. */
	unsigned long handle;

	/* User callback. */
	void (*callback) (void *callbackparam);
	void *callbackparam;

	/* Previous/next callback information. */
	struct list_head link;
};

/* Callback lists. */
static LIST_HEAD(g_handlelist);
static LIST_HEAD(g_vachandle);
static LIST_HEAD(g_vacinfo);
static GCDEFINE_LOCK(g_callbacklock);

static enum gcerror alloc_callbackhandle(struct gccallbackhandle **handle)
{
	enum gcerror gcerror;
	struct gccallbackhandle *temp;

	GCLOCK(&g_callbacklock);

	if (list_empty(&g_vachandle)) {
		temp = kmalloc(sizeof(struct gccallbackhandle), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("failed to allocate callback handle.\n");
			gcerror = GCERR_OODM;
			goto exit;
		}
		list_add(&temp->link, &g_handlelist);
	} else {
		struct list_head *head;
		head = g_vachandle.next;
		temp = list_entry(head, struct gccallbackhandle, link);
		list_move(head, &g_handlelist);
	}

	init_completion(&temp->ready);
	INIT_LIST_HEAD(&temp->triggered);
	INIT_LIST_HEAD(&temp->scheduled);

	*handle = temp;
	gcerror = GCERR_NONE;

exit:
	GCUNLOCK(&g_callbacklock);
	return gcerror;
}

static void free_callbackhandle(struct gccallbackhandle *handle)
{
	GCLOCK(&g_callbacklock);
	list_move(&handle->link, &g_vachandle);
	GCUNLOCK(&g_callbacklock);
}

static enum gcerror alloc_callbackinfo(unsigned long handle,
				       struct gccallbackinfo **info)
{
	enum gcerror gcerror;
	struct gccallbackinfo *temp;
	struct gccallbackhandle *gccallbackhandle;

	GCLOCK(&g_callbacklock);

	/* Get the callback handle. */
	gccallbackhandle = (struct gccallbackhandle *) handle;

	if (list_empty(&g_vacinfo)) {
		temp = kmalloc(sizeof(struct gccallbackinfo), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("failed to allocate callback info.\n");
			gcerror = GCERR_OODM;
			goto exit;
		}
		list_add_tail(&temp->link, &gccallbackhandle->scheduled);
	} else {
		struct list_head *head;
		head = g_vacinfo.next;
		temp = list_entry(head, struct gccallbackinfo, link);
		list_move_tail(head, &gccallbackhandle->scheduled);
	}

	temp->handle = handle;

	*info = temp;
	gcerror = GCERR_NONE;

exit:
	GCUNLOCK(&g_callbacklock);
	return gcerror;
}

static void gccallback(void *callbackparam)
{
	struct gccallbackinfo *gccallbackinfo;
	struct gccallbackhandle *gccallbackhandle;

	GCENTER(GCZONE_CALLBACK);

	/* Cast callback info. */
	gccallbackinfo = (struct gccallbackinfo *) callbackparam;
	GCDBG(GCZONE_CALLBACK, "callback info received 0x%08X.\n",
	      (unsigned int) gccallbackinfo);

	/* Get the callback handle. */
	gccallbackhandle = (struct gccallbackhandle *) gccallbackinfo->handle;
	GCDBG(GCZONE_CALLBACK, "callback handle 0x%08X.\n",
	      (unsigned int) gccallbackhandle);

	/* Move to the triggered list. */
	GCLOCK(&g_callbacklock);
	list_move_tail(&gccallbackinfo->link, &gccallbackhandle->triggered);
	GCUNLOCK(&g_callbacklock);

	/* Complete ready signal. */
	complete(&gccallbackhandle->ready);
	GCDBG(GCZONE_CALLBACK, "user released.\n");

	GCEXIT(GCZONE_CALLBACK);
}

static void destroy_callback(void)
{
	struct list_head *head;
	struct gccallbackhandle *gccallbackhandle;
	struct gccallbackinfo *gccallbackinfo;

	while (!list_empty(&g_handlelist)) {
		head = g_handlelist.next;
		gccallbackhandle = list_entry(head,
					      struct gccallbackhandle,
					      link);

		list_splice_init(&gccallbackhandle->triggered, &g_vacinfo);
		list_splice_init(&gccallbackhandle->scheduled, &g_vacinfo);

		list_del(head);
		kfree(gccallbackhandle);
	}

	while (!list_empty(&g_vachandle)) {
		head = g_vachandle.next;
		gccallbackhandle = list_entry(head,
					      struct gccallbackhandle,
					      link);
		list_del(head);
		kfree(gccallbackhandle);
	}

	while (!list_empty(&g_vacinfo)) {
		head = g_vacinfo.next;
		gccallbackinfo = list_entry(head,
					    struct gccallbackinfo,
					    link);
		list_del(head);
		kfree(gccallbackinfo);
	}
}


/*******************************************************************************
 * API user wrappers.
 */

static int gc_getcaps_wrapper(struct gcicaps *gcicaps)
{
	int ret = 0;
	struct gcicaps cpcaps;

	GCENTER(GCZONE_CAPS);

	/* Call the core driver. */
	gc_caps(&cpcaps);

	if (copy_to_user(gcicaps, &cpcaps, sizeof(struct gcicaps))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CAPS);
	return ret;
}

static int gc_commit_wrapper(struct gcicommit *gcicommit)
{
	int ret = 0;
	bool buffercopied = false;
	bool unmapcopied = false;
	struct gcicommit cpcommit;
	struct gccallbackinfo *gccallbackinfo;

	struct list_head *gcbufferhead;
	struct gcbuffer *cpbuffer, *gcbuffer;
	LIST_HEAD(cpbufferlist);

	struct list_head *gcfixuphead;
	struct gcfixup *cpfixup, *gcfixup;
	struct gcfixupentry *gcfixupentry;
	int tablesize;

	struct list_head *gcschedunmaphead;
	struct gcschedunmap *cpschedunmap, *gcschedunmap;
	LIST_HEAD(cpunmaplist);

	GCENTER(GCZONE_COMMIT);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcommit, gcicommit, sizeof(struct gcicommit))) {
		GCERR("failed to read data.\n");
		cpcommit.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Make a copy of the user buffer list. */
	gcbufferhead = cpcommit.buffer.next;
	while (gcbufferhead != &gcicommit->buffer) {
		gcbuffer = list_entry(gcbufferhead, struct gcbuffer, link);
		GCDBG(GCZONE_COMMIT, "copying buffer 0x%08X.\n",
		      (unsigned int) gcbuffer);

		/* Allocate a buffer structure. */
		cpcommit.gcerror = alloc_buffer(&cpbuffer);
		if (cpcommit.gcerror != GCERR_NONE)
			goto exit;

		/* Get the data from the user. */
		if (copy_from_user(cpbuffer, gcbuffer,
				   sizeof(struct gcbuffer))) {
			free_buffer(cpbuffer);
			GCERR("failed to read data.\n");
			cpcommit.gcerror = GCERR_USER_READ;
			goto exit;
		}

		/* Get the next user buffer. */
		gcbufferhead = cpbuffer->link.next;

		/* Add the buffer to the list. */
		list_add_tail(&cpbuffer->link, &cpbufferlist);

		/* Copy all fixups. */
		gcfixuphead = cpbuffer->fixup.next;
		INIT_LIST_HEAD(&cpbuffer->fixup);
		while (gcfixuphead != &gcbuffer->fixup) {
			gcfixup = list_entry(gcfixuphead, struct gcfixup, link);
			GCDBG(GCZONE_COMMIT, "copying fixup 0x%08X.\n",
			      (unsigned int) gcfixup);

			/* Allocare a fixup structure. */
			cpcommit.gcerror = alloc_fixup(&cpfixup);
			if (cpcommit.gcerror != GCERR_NONE)
				goto exit;

			/* Get the size of the fixup array. */
			if (copy_from_user(cpfixup, gcfixup,
					   offsetof(struct gcfixup, fixup))) {
				free_fixup(cpfixup);
				GCERR("failed to read data.\n");
				cpcommit.gcerror = GCERR_USER_READ;
				goto exit;
			}

			/* Get the next fixup. */
			gcfixuphead = cpfixup->link.next;

			/* Add to the list. */
			list_add_tail(&cpfixup->link, &cpbuffer->fixup);

			/* Get the fixup array. */
			gcfixupentry = gcfixup->fixup;

			/* Compute the size of the fixup table. */
			tablesize = cpfixup->count
				  * sizeof(struct gcfixupentry);

			/* Get the fixup table. */
			if (copy_from_user(cpfixup->fixup, gcfixupentry,
					   tablesize)) {
				GCERR("failed to read data.\n");
				cpcommit.gcerror = GCERR_USER_READ;
				goto exit;
			}
		}
	}

	/* Move the local list to the commit structure. */
	INIT_LIST_HEAD(&cpcommit.buffer);
	list_splice_init(&cpbufferlist, &cpcommit.buffer);
	buffercopied = true;

	/* Copy scheduled unmappings to the local list. */
	GCDBG(GCZONE_COMMIT, "copying unmaps.\n");
	gcschedunmaphead = cpcommit.unmap.next;
	while (gcschedunmaphead != &gcicommit->unmap) {
		/* Get a pointer to the user structure. */
		gcschedunmap = list_entry(gcschedunmaphead,
					  struct gcschedunmap,
					  link);
		GCDBG(GCZONE_COMMIT, "unmap 0x%08X.\n",
		      (unsigned int) gcschedunmap);

		/* Allocate unmap record. */
		cpcommit.gcerror = alloc_schedunmap(&cpschedunmap);
		if (cpcommit.gcerror != GCERR_NONE)
			goto exit;

		/* Copy unmap record from user. */
		if (copy_from_user(cpschedunmap, gcschedunmap,
				   sizeof(struct gcschedunmap))) {
			free_schedunmap(cpschedunmap);
			GCERR("failed to read data.\n");
			cpcommit.gcerror = GCERR_USER_READ;
			goto exit;
		}

		/* Get the next record. */
		gcschedunmaphead = cpschedunmap->link.next;

		/* Append to the list. */
		list_add_tail(&cpschedunmap->link, &cpunmaplist);
	}

	/* Move the local list to the commit structure. */
	INIT_LIST_HEAD(&cpcommit.unmap);
	list_splice_init(&cpunmaplist, &cpcommit.unmap);
	unmapcopied = true;

	/* Setup callback. */
	if (cpcommit.callback != NULL) {
		GCDBG(GCZONE_COMMIT, "setting up callback.\n");

		/* Allocate callback info. */
		cpcommit.gcerror = alloc_callbackinfo(cpcommit.handle,
						      &gccallbackinfo);
		if (cpcommit.gcerror != GCERR_NONE)
			goto exit;

		/* Initialize callback info. */
		gccallbackinfo->callback = cpcommit.callback;
		gccallbackinfo->callbackparam = cpcommit.callbackparam;

		/* Substiture the callback. */
		cpcommit.callback = gccallback;
		cpcommit.callbackparam = gccallbackinfo;
	} else {
		GCDBG(GCZONE_COMMIT, "no callback provided.\n");
	}

	/* Call the core driver. */
	gc_commit(&cpcommit, true);

exit:
	if (copy_to_user(&gcicommit->gcerror, &cpcommit.gcerror,
			 sizeof(enum gcerror))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	/* Free temporary resources. */
	free_buffer_list(buffercopied ? &cpcommit.buffer : &cpbufferlist);
	free_schedunmap_list(unmapcopied ? &cpcommit.unmap : &cpunmaplist);

	GCEXIT(GCZONE_COMMIT);
	return ret;
}

static int gc_map_wrapper(struct gcimap *gcimap)
{
	int ret = 0;
	int mapped = 0;
	struct gcimap cpmap;

	GCENTER(GCZONE_MAPPING);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpmap, gcimap, sizeof(struct gcimap))) {
		GCERR("failed to read data.\n");
		cpmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	cpmap.pagearray = NULL;

	/* Call the core driver. */
	gc_map(&cpmap, true);
	if (cpmap.gcerror != GCERR_NONE)
		goto exit;
	mapped = 1;

exit:
	if (copy_to_user(gcimap, &cpmap, offsetof(struct gcimap, buf))) {
		GCERR("failed to write data.\n");
		cpmap.gcerror = GCERR_USER_WRITE;
		ret = -EFAULT;
	}

	if (cpmap.gcerror != GCERR_NONE) {
		if (mapped)
			gc_unmap(&cpmap, true);
	}

	GCEXIT(GCZONE_MAPPING);
	return ret;
}

static int gc_unmap_wrapper(struct gcimap *gcimap)
{
	int ret = 0;
	struct gcimap cpmap;

	GCENTER(GCZONE_MAPPING);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpmap, gcimap, sizeof(struct gcimap))) {
		GCERR("failed to read data.\n");
		cpmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Call the core driver. */
	gc_unmap(&cpmap, true);

exit:
	if (copy_to_user(gcimap, &cpmap, offsetof(struct gcimap, buf))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_MAPPING);
	return ret;
}

static int gc_cache_wrapper(struct gcicache *gcicache)
{
	int ret = 0;
	struct gcicache cpcache;

	GCENTER(GCZONE_CACHE);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcache, gcicache, sizeof(struct gcicache))) {
		GCERR("failed to read data.\n");
		goto exit;
	}

	switch (cpcache.dir) {
	case DMA_FROM_DEVICE:
		c2dm_l2cache(cpcache.count, cpcache.rgn, cpcache.dir);
		c2dm_l1cache(cpcache.count, cpcache.rgn, cpcache.dir);
		break;

	case DMA_TO_DEVICE:
		c2dm_l1cache(cpcache.count, cpcache.rgn, cpcache.dir);
		c2dm_l2cache(cpcache.count, cpcache.rgn, cpcache.dir);
		break;

	case DMA_BIDIRECTIONAL:
		c2dm_l1cache(cpcache.count, cpcache.rgn, cpcache.dir);
		c2dm_l2cache(cpcache.count, cpcache.rgn, cpcache.dir);
		break;
	}

exit:
	GCEXIT(GCZONE_CACHE);
	return ret;
}

static int gc_callback_alloc(struct gcicallback *gcicallback)
{
	int ret = 0;
	struct gcicallback cpcmdcallback;
	struct gccallbackhandle *gccallbackhandle;

	GCENTER(GCZONE_CALLBACK);

	/* Reset the handle. */
	cpcmdcallback.handle = 0;

	/* Allocate callback handle struct. */
	cpcmdcallback.gcerror = alloc_callbackhandle(&gccallbackhandle);
	if (cpcmdcallback.gcerror != GCERR_NONE)
		goto exit;

	/* Set the handle. */
	cpcmdcallback.handle = (unsigned long) gccallbackhandle;

exit:
	if (copy_to_user(gcicallback, &cpcmdcallback,
			 sizeof(struct gcicallback))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CALLBACK);
	return ret;
}

static int gc_callback_free(struct gcicallback *gcicallback)
{
	int ret = 0;
	struct gcicallback cpcmdcallback;
	struct gccallbackhandle *gccallbackhandle;

	GCENTER(GCZONE_CALLBACK);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcmdcallback, gcicallback,
			   sizeof(struct gcicallback))) {
		GCERR("failed to read data.\n");
		cpcmdcallback.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Free the handle struct. */
	gccallbackhandle = (struct gccallbackhandle *) cpcmdcallback.handle;
	free_callbackhandle(gccallbackhandle);

	/* Reset the handle. */
	cpcmdcallback.handle = 0;
	cpcmdcallback.gcerror = GCERR_NONE;

exit:
	if (copy_to_user(gcicallback, &cpcmdcallback,
			 sizeof(struct gcicallback))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CALLBACK);
	return ret;
}

static int gc_callback_wait(struct gcicallbackwait *gcicallbackwait)
{
	int ret = 0;
	struct gcicallbackwait cpcmdcallbackwait;
	struct gccallbackhandle *gccallbackhandle;
	struct gccallbackinfo *gccallbackinfo;
	struct list_head *head;
	unsigned long timeout;

	GCENTER(GCZONE_CALLBACK);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcmdcallbackwait, gcicallbackwait,
			   sizeof(struct gcicallbackwait))) {
		GCERR("failed to read data.\n");
		cpcmdcallbackwait.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Cast the handle. */
	gccallbackhandle = (struct gccallbackhandle *) cpcmdcallbackwait.handle;

	/* Convert milliseconds to jiffies. */
	timeout = (cpcmdcallbackwait.timeoutms == ~0U)
		? MAX_SCHEDULE_TIMEOUT
		: msecs_to_jiffies(cpcmdcallbackwait.timeoutms);

	/* Wait until a callback is triggered. */
	timeout = wait_for_completion_interruptible_timeout(
		&gccallbackhandle->ready, timeout);

	if (timeout < 0) {
		/* Error occurred. */
		ret = timeout;
	} else if (timeout == 0) {
		/* Timeout. */
		cpcmdcallbackwait.gcerror = GCERR_TIMEOUT;
		cpcmdcallbackwait.callback = NULL;
		cpcmdcallbackwait.callbackparam = NULL;
	} else {
		/* Callback event triggered. */
		GCLOCK(&g_callbacklock);

		if (list_empty(&gccallbackhandle->triggered)) {
			GCERR("triggered list is empty.\n");
		} else {
			/* Get the head of the triggered list. */
			head = gccallbackhandle->triggered.next;
			gccallbackinfo = list_entry(head,
						    struct gccallbackinfo,
						    link);

			/* Set callback info. */
			cpcmdcallbackwait.callback
				= gccallbackinfo->callback;
			cpcmdcallbackwait.callbackparam
				= gccallbackinfo->callbackparam;

			/* Free the entry. */
			list_move(head, &g_vacinfo);
		}

		GCUNLOCK(&g_callbacklock);
		cpcmdcallbackwait.gcerror = GCERR_NONE;
	}

exit:
	if (copy_to_user(gcicallbackwait, &cpcmdcallbackwait,
			 sizeof(struct gcicallbackwait))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CALLBACK);
	return ret;
}

static int gc_callback_arm(struct gcicallbackarm *gcicallbackarm)
{
	int ret = 0;
	struct gcicallbackarm cpcallbackarm;
	struct gccallbackinfo *gccallbackinfo;

	GCENTER(GCZONE_CALLBACK);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcallbackarm, gcicallbackarm,
			   sizeof(struct gcicallbackarm))) {
		GCERR("failed to read data.\n");
		cpcallbackarm.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Allocate callback info. */
	cpcallbackarm.gcerror = alloc_callbackinfo(cpcallbackarm.handle,
						   &gccallbackinfo);
	if (cpcallbackarm.gcerror != GCERR_NONE)
		goto exit;

	/* Initialize callback info. */
	gccallbackinfo->callback = cpcallbackarm.callback;
	gccallbackinfo->callbackparam = cpcallbackarm.callbackparam;

	/* Substiture the callback. */
	cpcallbackarm.callback = gccallback;
	cpcallbackarm.callbackparam = gccallbackinfo;

	/* Call the core driver. */
	gc_callback(&cpcallbackarm, true);

exit:
	if (copy_to_user(&gcicallbackarm->gcerror, &cpcallbackarm.gcerror,
			 sizeof(enum gcerror))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CALLBACK);
	return ret;
}


/*******************************************************************************
 * Device definitions/operations.
 */

static int dev_major;
static struct class *dev_class;
static struct device *dev_object;

static int dev_open(struct inode *inode, struct file *file)
{
	if (cpu_is_omap447x())
		return 0;
	else
		return -1;
}

static int dev_release(struct inode *inode, struct file *file)
{
	gc_release();
	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case GCIOCTL_GETCAPS:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_GETCAPS\n");
		ret = gc_getcaps_wrapper((struct gcicaps *) arg);
		break;

	case GCIOCTL_COMMIT:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_COMMIT\n");
		ret = gc_commit_wrapper((struct gcicommit *) arg);
		break;

	case GCIOCTL_MAP:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_MAP\n");
		ret = gc_map_wrapper((struct gcimap *) arg);
		break;

	case GCIOCTL_UNMAP:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_UNMAP\n");
		ret = gc_unmap_wrapper((struct gcimap *) arg);
		break;

	case GCIOCTL_CACHE:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CACHE\n");
		ret = gc_cache_wrapper((struct gcicache *) arg);
		break;

	case GCIOCTL_CALLBACK_ALLOC:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_ALLOC\n");
		ret = gc_callback_alloc((struct gcicallback *) arg);
		break;

	case GCIOCTL_CALLBACK_FREE:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_FREE\n");
		ret = gc_callback_free((struct gcicallback *) arg);
		break;

	case GCIOCTL_CALLBACK_WAIT:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_WAIT\n");
		ret = gc_callback_wait((struct gcicallbackwait *) arg);
		break;

	case GCIOCTL_CALLBACK_ARM:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_ARM\n");
		ret = gc_callback_arm((struct gcicallbackarm *) arg);
		break;

	default:
		GCERR("invalid command (%d)\n", cmd);
		ret = -EINVAL;
	}

	GCDBG(GCZONE_IOCTL, "ret = %d\n", ret);

	return ret;
}

static const struct file_operations dev_operations = {
	.open		= dev_open,
	.release	= dev_release,
	.unlocked_ioctl	= dev_ioctl,
};


/*******************************************************************************
 * Device init/cleanup.
 */

static int mod_init(void);
static void mod_exit(void);

static struct platform_driver gcx_drv = {
	.probe = 0,
	.driver = {
		.owner = THIS_MODULE,
		.name = "gcx",
	},
};

static const char *gcx_version = VER_FILEVERSION_STR;

static ssize_t show_version(struct device_driver *driver, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", gcx_version);
}

static DRIVER_ATTR(version, 0444, show_version, NULL);

static int mod_init(void)
{
	int ret = 0;

	GCDBG(GCZONE_INIT, "initializing device.\n");

	/* Register the character device. */
	dev_major = register_chrdev(0, GC_DEV_NAME, &dev_operations);
	if (dev_major < 0) {
		GCERR("failed to allocate device (%d).\n", ret = dev_major);
		goto failed;
	}

	/* Create the device class. */
	dev_class = class_create(THIS_MODULE, GC_DEV_NAME);
	if (IS_ERR(dev_class)) {
		GCERR("failed to create class (%d).\n",
			ret = PTR_ERR(dev_class));
		goto failed;
	}

	/* Create device. */
	dev_object = device_create(dev_class, NULL, MKDEV(dev_major, 0),
					NULL, GC_DEV_NAME);
	if (IS_ERR(dev_object)) {
		GCERR("failed to create device (%d).\n",
			ret = PTR_ERR(dev_object));
		goto failed;
	}

	ret = platform_driver_register(&gcx_drv);
	if (ret) {
		GCERR("failed to create gcx driver (%d).\n", ret);
		goto failed;
	}

	ret = driver_create_file(&gcx_drv.driver, &driver_attr_version);
	if (ret) {
		GCERR("failed to create gcx driver version (%d).\n", ret);
		goto failed;
	}

	GCDBG_REGISTER(ioctl);

	GCDBG(GCZONE_INIT, "device number = %d\n", dev_major);
	GCDBG(GCZONE_INIT, "device class = 0x%08X\n",
		(unsigned int) dev_class);
	GCDBG(GCZONE_INIT, "device object = 0x%08X\n",
		(unsigned int) dev_object);

	return 0;

failed:
	mod_exit();
	return ret;
}

static void mod_exit(void)
{
	GCDBG(GCZONE_INIT, "cleaning up resources.\n");

	if ((dev_object != NULL) && !IS_ERR(dev_object)) {
		device_destroy(dev_class, MKDEV(dev_major, 0));
		dev_object = NULL;
	}

	if ((dev_class != NULL) && !IS_ERR(dev_class)) {
		class_destroy(dev_class);
		dev_class = NULL;
	}

	if (dev_major > 0) {
		unregister_chrdev(dev_major, GC_DEV_NAME);
		dev_major = 0;
	}

	platform_driver_unregister(&gcx_drv);
	driver_remove_file(&gcx_drv.driver, &driver_attr_version);

	free_vacant_buffers();
	free_vacant_fixups();
	free_vacant_unmap();
	destroy_callback();
}

static int __init mod_init_wrapper(void)
{
	return mod_init();
}

static void __exit mod_exit_wrapper(void)
{
	mod_exit();
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(mod_init_wrapper);
module_exit(mod_exit_wrapper);
