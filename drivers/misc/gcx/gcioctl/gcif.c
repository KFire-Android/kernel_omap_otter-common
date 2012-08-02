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
#define GCZONE_MAPPING		(1 << 1)
#define GCZONE_CACHE		(1 << 2)
#define GCZONE_COMMIT		(1 << 3)
#define GCZONE_IOCTL		(1 << 4)
#define GCZONE_CALLBACK		(1 << 5)

GCDBG_FILTERDEF(ioctl, GCZONE_NONE,
		"init",
		"mapping",
		"cache",
		"commit",
		"ioctl",
		"callback")


/*******************************************************************************
 * Command buffer copy management.
 */

static GCLOCK_TYPE g_lock;
struct gcbuffer *g_buffervacant;
struct gcfixup *g_fixupvacant;

static enum gcerror alloc_buffer(struct gcbuffer **gcbuffer)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcbuffer *temp;

	if (g_buffervacant == NULL) {
		temp = kmalloc(sizeof(struct gcbuffer), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("out of memory.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
					       GCERR_IOCTL_BUF_ALLOC);
			goto exit;
		}
	} else {
		temp = g_buffervacant;
		g_buffervacant = g_buffervacant->next;
	}

	temp->fixuphead = NULL;
	temp->next = NULL;

	*gcbuffer = temp;

exit:
	return gcerror;
}

static enum gcerror alloc_fixup(struct gcfixup **gcfixup)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcfixup *temp;

	if (g_fixupvacant == NULL) {
		temp = kmalloc(sizeof(struct gcfixup), GFP_KERNEL);
		if (temp == NULL) {
			GCERR("out of memory.\n");
			gcerror = GCERR_SETGRP(GCERR_OODM,
					       GCERR_IOCTL_FIXUP_ALLOC);
			goto exit;
		}
	} else {
		temp = g_fixupvacant;
		g_fixupvacant = g_fixupvacant->next;
	}

	*gcfixup = temp;

exit:
	return gcerror;
}

static void free_buffer_tree(struct gcbuffer *gcbuffer)
{
	struct gcbuffer *prev;
	struct gcbuffer *curr;

	prev = NULL;
	curr = gcbuffer;
	while (curr != NULL) {
		if (curr->fixuphead != NULL) {
			curr->fixuptail->next = g_fixupvacant;
			g_fixupvacant = curr->fixuphead;
		}

		prev = curr;
		curr = curr->next;
	}

	prev->next = g_buffervacant;
	g_buffervacant = gcbuffer;
}

static void destroy_buffer(void)
{
	struct gcbuffer *currbuffer, *tempbuffer;
	struct gcfixup *currfixup, *tempfixup;

	currbuffer = g_buffervacant;
	while (currbuffer != NULL) {
		tempbuffer = currbuffer;
		currbuffer = currbuffer->next;
		kfree(tempbuffer);
	}

	currfixup = g_fixupvacant;
	while (currfixup != NULL) {
		tempfixup = currfixup;
		currfixup = currfixup->next;
		kfree(tempfixup);
	}
}


/*******************************************************************************
 * Unmap list management.
 */

static LIST_HEAD(g_unmapvac);

static enum gcerror alloc_schedunmap(struct gcschedunmap **gcschedunmap)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcschedunmap *temp;

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

	*gcschedunmap = temp;

exit:
	return gcerror;
}

static void destroy_unmap(void)
{
	struct list_head *head;
	struct gcschedunmap *gcschedunmap;

	while (!list_empty(&g_unmapvac)) {
		head = g_unmapvac.next;
		gcschedunmap = list_entry(head, struct gcschedunmap, link);
		list_del(head);
		kfree(gcschedunmap);
	}
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
		list_add(&temp->link, &gccallbackhandle->scheduled);
	} else {
		struct list_head *head;
		head = g_vacinfo.next;
		temp = list_entry(head, struct gccallbackinfo, link);
		list_move(head, &gccallbackhandle->scheduled);
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

static int gc_commit_wrapper(struct gccommit *gccommit)
{
	int ret = 0;
	bool locked = false;
	bool unmapcopied = false;
	struct gccommit cpcommit;
	struct gcbuffer *cpbuffer = NULL, *gcbuffer;
	struct gcbuffer *headbuffer = NULL, *tailbuffer;
	struct gcfixup *cpfixup = NULL, *gcfixup;
	struct gcfixupentry *gcfixupentry;
	struct gcschedunmap *cpschedunmap, *gcschedunmap;
	struct gccallbackinfo *gccallbackinfo;
	struct list_head *head;
	LIST_HEAD(cpunmap);
	int tablesize;

	GCENTER(GCZONE_COMMIT);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcommit, gccommit, sizeof(struct gccommit))) {
		GCERR("failed to read data.\n");
		cpcommit.gcerror = GCERR_USER_READ;
		goto exit;
	}

	GCLOCK(&g_lock);
	locked = true;

	/* Make a copy of the user buffer structures. */
	gcbuffer = cpcommit.buffer;
	while (gcbuffer != NULL) {
		GCDBG(GCZONE_COMMIT, "copying buffer 0x%08X.\n",
		      (unsigned int) gcbuffer);

		/* Allocate a buffer structure. */
		cpcommit.gcerror = alloc_buffer(&cpbuffer);
		if (cpcommit.gcerror != GCERR_NONE)
			goto exit;

		/* Add to the list. */
		if (headbuffer == NULL)
			headbuffer = cpbuffer;
		else
			tailbuffer->next = cpbuffer;
		tailbuffer = cpbuffer;

		/* Get the data from the user. */
		if (copy_from_user(cpbuffer, gcbuffer,
				   sizeof(struct gcbuffer))) {
			GCERR("failed to read data.\n");
			cpcommit.gcerror = GCERR_USER_READ;
			goto exit;
		}

		/* Get the next user buffer. */
		gcbuffer = cpbuffer->next;
		cpbuffer->next = NULL;

		/* Get fixups and reset them in the kernel copy. */
		gcfixup = cpbuffer->fixuphead;
		cpbuffer->fixuphead = NULL;

		/* Copy all fixups. */
		while (gcfixup != NULL) {
			GCDBG(GCZONE_COMMIT, "copying fixup 0x%08X.\n",
			      (unsigned int) gcfixup);

			/* Allocare a fixup structure. */
			cpcommit.gcerror = alloc_fixup(&cpfixup);
			if (cpcommit.gcerror != GCERR_NONE)
				goto exit;

			/* Add to the list. */
			if (cpbuffer->fixuphead == NULL)
				cpbuffer->fixuphead = cpfixup;
			else
				cpbuffer->fixuptail->next = cpfixup;
			cpbuffer->fixuptail = cpfixup;

			/* Get the size of the fixup array. */
			if (copy_from_user(cpfixup, gcfixup,
					   offsetof(struct gcfixup, fixup))) {
				GCERR("failed to read data.\n");
				cpcommit.gcerror = GCERR_USER_READ;
				goto exit;
			}

			/* Get the fixup array. */
			gcfixupentry = gcfixup->fixup;

			/* Get the next fixup. */
			gcfixup = cpfixup->next;
			cpfixup->next = NULL;

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

	/* Copy scheduled unmappings to the local list. */
	GCDBG(GCZONE_COMMIT, "copying unmaps.\n");
	head = cpcommit.unmap.next;
	while (head != &gccommit->unmap) {
		/* Get a pointer to the user structure. */
		gcschedunmap = list_entry(head, struct gcschedunmap, link);
		GCDBG(GCZONE_COMMIT, "unmap 0x%08X.\n",
		      (unsigned int) gcschedunmap);

		/* Allocate unmap record. */
		cpcommit.gcerror = alloc_schedunmap(&cpschedunmap);
		if (cpcommit.gcerror != GCERR_NONE)
			goto exit;

		/* Copy unmap record from user. */
		if (copy_from_user(cpschedunmap, gcschedunmap,
				   sizeof(struct gcschedunmap))) {
			list_add(&cpschedunmap->link, &cpunmap);
			GCERR("failed to read data.\n");
			cpcommit.gcerror = GCERR_USER_READ;
			goto exit;
		}

		/* Get the next record. */
		head = cpschedunmap->link.next;

		/* Append to the list. */
		list_add_tail(&cpschedunmap->link, &cpunmap);
	}

	/* Move the local list to the commit structure. */
	INIT_LIST_HEAD(&cpcommit.unmap);
	list_splice_init(&cpunmap, &cpcommit.unmap);
	unmapcopied = true;

	GCUNLOCK(&g_lock);
	locked = false;

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
	cpcommit.buffer = headbuffer;
	gc_commit(&cpcommit, true);

exit:
	if (copy_to_user(&gccommit->gcerror, &cpcommit.gcerror,
			 sizeof(enum gcerror))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	/* Free temporary resources. */
	if (!locked)
		GCLOCK(&g_lock);
	if (headbuffer != NULL)
		free_buffer_tree(headbuffer);
	list_splice(unmapcopied ? &cpcommit.unmap : &cpunmap, &g_unmapvac);
	GCUNLOCK(&g_lock);

	GCEXIT(GCZONE_COMMIT);
	return ret;
}

static int gc_map_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	int mapped = 0;
	struct gcmap cpmap;

	GCENTER(GCZONE_MAPPING);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpmap, gcmap, sizeof(struct gcmap))) {
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
	if (copy_to_user(gcmap, &cpmap, offsetof(struct gcmap, buf))) {
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

static int gc_unmap_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	struct gcmap cpmap;

	GCENTER(GCZONE_MAPPING);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpmap, gcmap, sizeof(struct gcmap))) {
		GCERR("failed to read data.\n");
		cpmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Call the core driver. */
	gc_unmap(&cpmap, true);

exit:
	if (copy_to_user(gcmap, &cpmap, offsetof(struct gcmap, buf))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_MAPPING);
	return ret;
}

static int gc_cache_wrapper(struct gccachexfer *gccachexfer)
{
	int ret = 0;
	struct gccachexfer cpcache;

	GCENTER(GCZONE_CACHE);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcache, gccachexfer, sizeof(struct gccachexfer))) {
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

static int gc_callback_alloc(struct gccmdcallback *gccmdcallback)
{
	int ret = 0;
	struct gccmdcallback cpcmdcallback;
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
	if (copy_to_user(gccmdcallback, &cpcmdcallback,
			 sizeof(struct gccmdcallback))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CALLBACK);
	return ret;
}

static int gc_callback_free(struct gccmdcallback *gccmdcallback)
{
	int ret = 0;
	struct gccmdcallback cpcmdcallback;
	struct gccallbackhandle *gccallbackhandle;

	GCENTER(GCZONE_CALLBACK);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcmdcallback, gccmdcallback,
			   sizeof(struct gccmdcallback))) {
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
	if (copy_to_user(gccmdcallback, &cpcmdcallback,
			 sizeof(struct gccmdcallback))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	GCEXIT(GCZONE_CALLBACK);
	return ret;
}

static int gc_callback_wait(struct gccmdcallbackwait *gccmdcallbackwait)
{
	int ret = 0;
	struct gccmdcallbackwait cpcmdcallbackwait;
	struct gccallbackhandle *gccallbackhandle;
	struct gccallbackinfo *gccallbackinfo;
	struct list_head *head;
	unsigned long timeout;

	GCENTER(GCZONE_CALLBACK);

	/* Get IOCTL parameters. */
	if (copy_from_user(&cpcmdcallbackwait, gccmdcallbackwait,
			   sizeof(struct gccmdcallbackwait))) {
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

	if (timeout < 0)
		ret = timeout;

	else if (timeout == 0) {
		cpcmdcallbackwait.gcerror = GCERR_TIMEOUT;
		cpcmdcallbackwait.callback = NULL;
		cpcmdcallbackwait.callbackparam = NULL;
	} else {
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
	if (copy_to_user(gccmdcallbackwait, &cpcmdcallbackwait,
			 sizeof(struct gccmdcallbackwait))) {
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
	case GCIOCTL_COMMIT:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_COMMIT\n");
		ret = gc_commit_wrapper((struct gccommit *) arg);
		break;

	case GCIOCTL_MAP:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_MAP\n");
		ret = gc_map_wrapper((struct gcmap *) arg);
		break;

	case GCIOCTL_UNMAP:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_UNMAP\n");
		ret = gc_unmap_wrapper((struct gcmap *) arg);
		break;

	case GCIOCTL_CACHE:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CACHE\n");
		ret = gc_cache_wrapper((struct gccachexfer *) arg);
		break;

	case GCIOCTL_CALLBACK_ALLOC:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_ALLOC\n");
		ret = gc_callback_alloc((struct gccmdcallback *) arg);
		break;

	case GCIOCTL_CALLBACK_FREE:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_FREE\n");
		ret = gc_callback_free((struct gccmdcallback *) arg);
		break;

	case GCIOCTL_CALLBACK_WAIT:
		GCDBG(GCZONE_IOCTL, "GCIOCTL_CALLBACK_WAIT\n");
		ret = gc_callback_wait((struct gccmdcallbackwait *) arg);
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

	GCLOCK_INIT(&g_lock);
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

	destroy_buffer();
	destroy_unmap();
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
