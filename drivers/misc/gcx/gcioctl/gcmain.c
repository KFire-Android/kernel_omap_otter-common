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

#include <linux/gcx.h>
#include <linux/gccore.h>
#include <linux/gcbv.h>
#include "gcmain.h"

#ifndef GC_DUMP
#	define GC_DUMP 0
#endif

#if GC_DUMP
#	define GC_PRINT printk
#else
#	define GC_PRINT(...)
#endif

/*******************************************************************************
 * User context mapping.
 */

struct gccontextmap {
	pid_t pid;
	struct gccontext *context;
	struct gccontextmap *prev;
	struct gccontextmap *next;
};

static struct mutex g_maplock;
static struct gccontextmap *g_map;
static struct gccontextmap *g_mapvacant;

static enum gcerror find_context(struct gccontextmap **context, int create)
{
	enum gcerror gcerror;
	struct gccontextmap *prev;
	struct gccontextmap *curr;
	int maplocked = 0;
	pid_t pid;

	GC_PRINT(GC_INFO_MSG " getting lock mutex.\n", __func__, __LINE__);

	/* Acquire map access mutex. */
	gcerror = gc_acquire_mutex(&g_maplock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_CTX_ALLOC);
		goto exit;
	}
	maplocked = 1;

	/* Get current PID. */
	pid = current->tgid;

	/* Search the list. */
	prev = NULL;
	curr = g_map;

	GC_PRINT(GC_INFO_MSG " scanning existing records for pid %d.\n",
		__func__, __LINE__, pid);

	/* Try to locate the record. */
	while (curr != NULL) {
		/* Found the record? */
		if (curr->pid == pid) {
			/* Move to the top of the list. */
			if (prev != NULL) {
				prev->next = curr->next;
				curr->next = g_map;
				g_map = curr;
			}

			/* Success. */
			GC_PRINT(GC_INFO_MSG " record is found @ 0x%08X\n",
				__func__, __LINE__, (unsigned int) curr);

			*context = curr;
			goto exit;
		}

		/* Get the next record. */
		prev = curr;
		curr = curr->next;
	}

	/* Not found, do we need to create a new one? */
	if (!create) {
		GC_PRINT(GC_INFO_MSG " not found, exiting.\n",
			__func__, __LINE__);
		gcerror = GCERR_NOT_FOUND;
		goto exit;
	}

	/* Get new record. */
	if (g_mapvacant == NULL) {
		GC_PRINT(GC_INFO_MSG " not found, allocating.\n",
			__func__, __LINE__);

		curr = kmalloc(sizeof(struct gccontextmap), GFP_KERNEL);
		if (curr == NULL) {
			GC_PRINT(GC_ERR_MSG " out of memory.\n",
					__func__, __LINE__);
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_IOCTL_CTX_ALLOC);
			goto exit;
		}

		GC_PRINT(GC_INFO_MSG " allocated @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) curr);
	} else {
		GC_PRINT(GC_INFO_MSG " not found, reusing record @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) g_mapvacant);

		curr = g_mapvacant;
		g_mapvacant = g_mapvacant->next;
	}

	GC_PRINT(GC_INFO_MSG " creating new context.\n",
		__func__, __LINE__);

	/* Create the context. */
	gcerror = gc_attach(&curr->context);

	/* Success? */
	if (gcerror == GCERR_NONE) {
		GC_PRINT(GC_INFO_MSG " new context created @ 0x%08X\n",
			__func__, __LINE__, (unsigned int) curr->context);

		/* Set the PID. */
		curr->pid = pid;

		/* Add to the list. */
		curr->prev = NULL;
		curr->next = g_map;
		if (g_map != NULL)
			g_map->prev = curr;
		g_map = curr;

		/* Set return value. */
		*context = curr;
	} else {
		GC_PRINT(GC_INFO_MSG " failed to create a context.\n",
			__func__, __LINE__);

		/* Add the record to the vacant list. */
		curr->next = g_mapvacant;
		g_mapvacant = curr;
	}

exit:
	if (maplocked)
		mutex_unlock(&g_maplock);

	return gcerror;
}

static enum gcerror release_context(struct gccontextmap *context)
{
	enum gcerror gcerror;

	/* Remove from the list. */
	if (context->prev == NULL) {
		if (context != g_map) {
			gcerror = GCERR_NOT_FOUND;
			goto exit;
		}

		g_map = context->next;
		g_map->prev = NULL;
	} else {
		context->prev->next = context->next;
		context->next->prev = context->prev;
	}

	if (context->context != NULL) {
		gcerror = gc_detach(&context->context);
		if (gcerror != GCERR_NONE)
			goto exit;
	}

	kfree(context);

exit:
	return gcerror;
}

static void delete_context_map(void)
{
	struct gccontextmap *curr;

	while (g_map != NULL)
		release_context(g_map);

	while (g_mapvacant != NULL) {
		curr = g_mapvacant;
		g_mapvacant = g_mapvacant->next;
		kfree(curr);
	}
}

/*******************************************************************************
 * Command buffer copy management.
 */

static struct mutex g_bufferlock;
struct gcbuffer *g_buffervacant;
struct gcfixup *g_fixupvacant;

static enum gcerror get_buffer(struct gcbuffer **gcbuffer)
{
	enum gcerror gcerror;
	int bufferlocked = 0;
	struct gcbuffer *temp;

	/* Acquire buffer access mutex. */
	gcerror = gc_acquire_mutex(&g_bufferlock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_BUF_ALLOC);
		goto exit;
	}
	bufferlocked = 1;

	if (g_buffervacant == NULL) {
		temp = kmalloc(sizeof(struct gcbuffer), GFP_KERNEL);
		if (temp == NULL) {
			GC_PRINT(GC_ERR_MSG " out of memory.\n",
					__func__, __LINE__);
			gcerror = GCERR_SETGRP(GCERR_OODM,
						GCERR_IOCTL_BUF_ALLOC);
			goto exit;
		}
	} else {
		temp = g_buffervacant;
		g_buffervacant = g_buffervacant->next;
	}

	*gcbuffer = temp;

exit:
	if (bufferlocked)
		mutex_unlock(&g_bufferlock);

	return gcerror;
}

static enum gcerror get_fixup(struct gcfixup **gcfixup)
{
	enum gcerror gcerror;
	int bufferlocked = 0;
	struct gcfixup *temp;

	/* Acquire fixup access mutex. */
	gcerror = gc_acquire_mutex(&g_bufferlock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_FIXUP_ALLOC);
		goto exit;
	}
	bufferlocked = 1;

	if (g_fixupvacant == NULL) {
		temp = kmalloc(sizeof(struct gcfixup), GFP_KERNEL);
		if (temp == NULL) {
			GC_PRINT(GC_ERR_MSG " out of memory.\n",
					__func__, __LINE__);
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
	if (bufferlocked)
		mutex_unlock(&g_bufferlock);

	return gcerror;
}

static enum gcerror put_buffer_tree(struct gcbuffer *gcbuffer)
{
	enum gcerror gcerror;
	int bufferlocked = 0;
	struct gcbuffer *prev;
	struct gcbuffer *curr;

	/* Acquire buffer access mutex. */
	gcerror = gc_acquire_mutex(&g_bufferlock, GC_INFINITE);
	if (gcerror != GCERR_NONE) {
		GC_PRINT(GC_ERR_MSG " failed to acquire mutex (0x%08X).\n",
				__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_BUF_ALLOC);
		goto exit;
	}
	bufferlocked = 1;

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

exit:
	if (bufferlocked)
		mutex_unlock(&g_bufferlock);

	return gcerror;
}

/*******************************************************************************
 * API user wrappers.
 */

static int gc_commit_wrapper(struct gccommit *gccommit)
{
	int ret = 0;
	int locked = 0;
	struct gccontextmap *context;
	struct gccommit kgccommit;
	struct gcbuffer *head = NULL;
	struct gcbuffer *tail;
	struct gcbuffer *ubuffer;
	struct gcbuffer *kbuffer = NULL;
	struct gcfixup *ufixup;
	struct gcfixup *nfixup;
	struct gcfixup *kfixup = NULL;
	int tablesize;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgccommit, gccommit, sizeof(struct gccommit))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		kgccommit.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Locate the client entry. */
	kgccommit.gcerror = find_context(&context, true);
	if (kgccommit.gcerror != GCERR_NONE)
		goto exit;

	/* Set context. */
	kgccommit.gcerror = gc_lock(context->context);
	if (kgccommit.gcerror != GCERR_NONE)
		goto exit;
	locked = 1;

	/* Make a copy of the user buffer structures. */
	ubuffer = kgccommit.buffer;
	while (ubuffer != NULL) {
		/* Allocate a buffer structure. */
		kgccommit.gcerror = get_buffer(&kbuffer);
		if (kgccommit.gcerror != GCERR_NONE)
			goto exit;

		/* Add to the list. */
		if (head == NULL)
			head = kbuffer;
		else
			tail->next = kbuffer;
		tail = kbuffer;

		/* Reset user pointers in the kernel structure. */
		kbuffer->fixuphead = NULL;
		kbuffer->next = NULL;

		/* Get the data from the user. */
		if (copy_from_user(kbuffer, ubuffer, sizeof(struct gcbuffer))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			kgccommit.gcerror = GCERR_USER_READ;
			goto exit;
		}

		/* Get the next user buffer. */
		ubuffer = kbuffer->next;
		kbuffer->next = NULL;

		/* Get fixups and reset them in the kernel copy. */
		ufixup = kbuffer->fixuphead;
		kbuffer->fixuphead = NULL;

		/* Copy all fixups. */
		while (ufixup != NULL) {
			/* Allocare a fixup structure. */
			kgccommit.gcerror = get_fixup(&kfixup);
			if (kgccommit.gcerror != GCERR_NONE)
				goto exit;

			/* Add to the list. */
			if (kbuffer->fixuphead == NULL)
				kbuffer->fixuphead = kfixup;
			else
				kbuffer->fixuptail->next = kfixup;
			kbuffer->fixuptail = kfixup;

			/* Get the data from the user. */
			if (copy_from_user(kfixup, ufixup,
					offsetof(struct gcfixup, fixup))) {
				GC_PRINT(GC_ERR_MSG " failed to read data.\n",
						__func__, __LINE__);
				kgccommit.gcerror = GCERR_USER_READ;
				goto exit;
			}

			/* Get the next fixup. */
			nfixup = kfixup->next;
			kfixup->next = NULL;

			/* Compute the size of the fixup table. */
			tablesize = kfixup->count * sizeof(struct gcfixupentry);

			/* Get the fixup table. */
			if (copy_from_user(kfixup->fixup, ufixup->fixup,
						tablesize)) {
				GC_PRINT(GC_ERR_MSG " failed to read data.\n",
						__func__, __LINE__);
				kgccommit.gcerror = GCERR_USER_READ;
				goto exit;
			}

			/* Advance to the next. */
			ufixup = nfixup;
		}
	}

	/* Call the core driver. */
	kgccommit.buffer = head;
	gc_commit(&kgccommit, true);

exit:
	GC_PRINT(GC_INFO_MSG " gcerror = 0x%08X\n",
		__func__, __LINE__, kgccommit.gcerror);

	if (copy_to_user(&gccommit->gcerror, &kgccommit.gcerror,
				sizeof(enum gcerror))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

	if (locked)
		gc_unlock(context->context);

	if (head != NULL)
		put_buffer_tree(head);

	return ret;
}

static int gc_map_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	int mapped = 0;
	int locked = 0;
	struct gccontextmap *context;
	struct gcmap kgcmap;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcmap, gcmap, sizeof(struct gcmap))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		kgcmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Locate the client entry. */
	kgcmap.gcerror = find_context(&context, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;

	/* Set context. */
	kgcmap.gcerror = gc_lock(context->context);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	locked = 1;

	/* Call the core driver. */
	gc_map(&kgcmap);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	mapped = 1;

exit:
	GC_PRINT(GC_INFO_MSG " gcerror = 0x%08X\n",
		__func__, __LINE__, kgcmap.gcerror);

	if (copy_to_user(gcmap, &kgcmap, offsetof(struct gcmap, logical))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		kgcmap.gcerror = GCERR_USER_WRITE;
		ret = -EFAULT;
	}

	if (kgcmap.gcerror != GCERR_NONE) {
		if (mapped)
			gc_unmap(&kgcmap);
	}

	if (locked)
		gc_unlock(context->context);

	return ret;
}

static int gc_unmap_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	int locked = 0;
	struct gccontextmap *context;
	struct gcmap kgcmap;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcmap, gcmap, sizeof(struct gcmap))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		kgcmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Locate the client entry. */
	kgcmap.gcerror = find_context(&context, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;

	/* Set context. */
	kgcmap.gcerror = gc_lock(context->context);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	locked = 1;

	/* Call the core driver. */
	gc_unmap(&kgcmap);

exit:
	GC_PRINT(GC_INFO_MSG " gcerror = 0x%08X\n",
		__func__, __LINE__, kgcmap.gcerror);

	if (copy_to_user(gcmap, &kgcmap, offsetof(struct gcmap, logical))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

	if (locked)
		gc_unlock(context->context);

	return ret;
}

int gc_bvblt_wrapper(struct gcbvblt *gcbvblt)
{
	int ret = 0;
	int locked = 0;
	enum gcerror gcerror;
	struct gccontextmap *context;
	struct gcbvblt kgcbvblt;
	struct bvbltparams kbvbltparams;
	struct bvbuffdesc kdestbuffdesc;
	struct bvsurfgeom kdestsurfgeom;
	struct bvbuffdesc ksrc1buffdesc;
	struct bvsurfgeom ksrc1surfgeom;
	struct bvbuffdesc ksrc2buffdesc;
	struct bvsurfgeom ksrc2surfgeom;
	struct bvbuffdesc kmaskbuffdesc;
	struct bvsurfgeom kmasksurfgeom;
	int errdesclen;
	int batchinfosize;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcbvblt, gcbvblt, sizeof(struct gcbvblt))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Get blit parameters. */
	if (copy_from_user(&kbvbltparams, kgcbvblt.bltparams,
				sizeof(struct bvbltparams))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Reset the error message */
	kbvbltparams.errdesc = NULL;

	/* Get destination parameters. */
	if (kbvbltparams.dstdesc != NULL) {
		if (copy_from_user(&kdestbuffdesc, kbvbltparams.dstdesc,
					sizeof(struct bvbuffdesc))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.dstdesc = &kdestbuffdesc;
	}

	if (kbvbltparams.dstgeom != NULL) {
		if (copy_from_user(&kdestsurfgeom, kbvbltparams.dstgeom,
					sizeof(struct bvsurfgeom))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.dstgeom = &kdestsurfgeom;
	}

	/* Get src1 parameters. */
	if (kbvbltparams.src1.desc != NULL) {
		if (copy_from_user(&ksrc1buffdesc, kbvbltparams.src1.desc,
					sizeof(struct bvbuffdesc))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.src1.desc = &ksrc1buffdesc;
	}

	if (kbvbltparams.src1geom != NULL) {
		if (copy_from_user(&ksrc1surfgeom, kbvbltparams.src1geom,
					sizeof(struct bvsurfgeom))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.src1geom = &ksrc1surfgeom;
	}

	/* Get src2 parameters. */
	if (kbvbltparams.src2.desc != NULL) {
		if (copy_from_user(&ksrc2buffdesc, kbvbltparams.src2.desc,
					sizeof(struct bvbuffdesc))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.src2.desc = &ksrc2buffdesc;
	}

	if (kbvbltparams.src2geom != NULL) {
		if (copy_from_user(&ksrc2surfgeom, kbvbltparams.src2geom,
					sizeof(struct bvsurfgeom))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.src2geom = &ksrc2surfgeom;
	}

	/* Get mask parameters. */
	if (kbvbltparams.mask.desc != NULL) {
		if (copy_from_user(&kmaskbuffdesc, kbvbltparams.mask.desc,
					sizeof(struct bvbuffdesc))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.mask.desc = &kmaskbuffdesc;
	}

	if (kbvbltparams.maskgeom != NULL) {
		if (copy_from_user(&kmasksurfgeom, kbvbltparams.maskgeom,
					sizeof(struct bvsurfgeom))) {
			GC_PRINT(GC_ERR_MSG " failed to read data.\n",
					__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
		kbvbltparams.maskgeom = &kmasksurfgeom;
	}

	/* Locate the client entry. */
	gcerror = find_context(&context, true);
	if (gcerror != GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}

	/* Set context. */
	gcerror = gc_lock(context->context);
	if (gcerror != GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}
	locked = 1;

	/* Call the core driver. */
	kgcbvblt.bverror = bv_blt(&kbvbltparams);
	if (kgcbvblt.bverror != BVERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}

	/* Copy batch info back to the user. */
	batchinfosize
		= offsetof(struct bvbltparams, callbackfn)
		- offsetof(struct bvbltparams, batchflags);

	if (copy_to_user(&gcbvblt->bltparams->batchflags,
				&kbvbltparams.batchflags, batchinfosize)) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Copy error message if any. */
	if ((kbvbltparams.errdesc != NULL) && (kgcbvblt.errdesc != NULL)) {
		errdesclen = strlen(kgcbvblt.errdesc);
		if (kgcbvblt.errdesclen < errdesclen)
			errdesclen = kgcbvblt.errdesclen;

		if (copy_to_user(gcbvblt->errdesc, kbvbltparams.errdesc,
					errdesclen)) {
			GC_PRINT(GC_ERR_MSG " failed to write data.\n",
				__func__, __LINE__);
			ret = -EFAULT;
			goto exit;
		}
	}

exit:
	if (copy_to_user(&gcbvblt->bverror, &kgcbvblt.bverror,
				sizeof(enum bverror))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

	if (locked)
		gc_unlock(context->context);

	return ret;
}

int gc_bvmap_wrapper(struct gcbvmap *gcbvmap)
{
	int ret = 0;
	int mapped = 0;
	int locked = 0;
	enum gcerror gcerror;
	struct gccontextmap *context;
	struct gcbvmap kgcbvmap;
	struct bvbuffdesc kbvbuffdesc;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcbvmap, gcbvmap, sizeof(struct gcbvmap))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Get buffer descriptor. */
	if (copy_from_user(&kbvbuffdesc, kgcbvmap.buffdesc,
				sizeof(struct bvbuffdesc))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Locate the client entry. */
	gcerror = find_context(&context, true);
	if (gcerror != GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}

	/* Set context. */
	gcerror = gc_lock(context->context);
	if (gcerror != GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}
	locked = 1;

	/* Call the core driver. */
	kgcbvmap.bverror = bv_map(&kbvbuffdesc);
	if (kgcbvmap.bverror != BVERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}
	mapped = 1;

	if (copy_to_user(gcbvmap->buffdesc, &kbvbuffdesc,
				sizeof(struct bvbuffdesc))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

exit:
	if (copy_to_user(&gcbvmap->bverror, &kgcbvmap.bverror,
				sizeof(enum bverror))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

	if (ret != 0) {
		if (mapped)
			bv_unmap(&kbvbuffdesc);
	}

	if (locked)
		gc_unlock(context->context);

	return ret;
}

int gc_bvunmap_wrapper(struct gcbvmap *gcbvmap)
{
	int ret = 0;
	int locked = 0;
	enum gcerror gcerror;
	struct gccontextmap *context;
	struct gcbvmap kgcbvmap;
	struct bvbuffdesc kbvbuffdesc;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcbvmap, gcbvmap, sizeof(struct gcbvmap))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Get buffer descriptor. */
	if (copy_from_user(&kbvbuffdesc, kgcbvmap.buffdesc,
				sizeof(struct bvbuffdesc))) {
		GC_PRINT(GC_ERR_MSG " failed to read data.\n",
				__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

	/* Locate the client entry. */
	gcerror = find_context(&context, true);
	if (gcerror != GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}

	/* Set context. */
	gcerror = gc_lock(context->context);
	if (gcerror != GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}
	locked = 1;

	/* Call the core driver. */
	kgcbvmap.bverror = bv_unmap(&kbvbuffdesc);
	if (kgcbvmap.bverror != BVERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}

	if (copy_to_user(gcbvmap->buffdesc, &kbvbuffdesc,
				sizeof(struct bvbuffdesc))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
		goto exit;
	}

exit:
	if (copy_to_user(&gcbvmap->bverror, &kgcbvmap.bverror,
				sizeof(enum bverror))) {
		GC_PRINT(GC_ERR_MSG " failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

	if (locked)
		gc_unlock(context->context);

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
	int ret = 0;
	enum gcerror gcerror;
	struct gccontextmap *context;

	GC_PRINT(GC_INFO_MSG "\n", __func__, __LINE__);

	/* Locate the client entry. */
	gcerror = find_context(&context, true);
	if (gcerror != GCERR_NONE)
		ret = -EFAULT;

	return ret;
}

static int dev_release(struct inode *inode, struct file *file)
{
	int ret = 0;
	enum gcerror gcerror;
	struct gccontextmap *context;

	GC_PRINT(GC_INFO_MSG "\n", __func__, __LINE__);

	/* Locate the client entry. */
	gcerror = find_context(&context, false);
	if (gcerror == GCERR_NONE) {
		ret = -EFAULT;
		goto exit;
	}

	/* Destroy the context. */
	gcerror = release_context(context);
	if (gcerror == GCERR_NONE)
		ret = -EFAULT;

exit:
	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case GCIOCTL_COMMIT:
		GC_PRINT(GC_INFO_MSG " GCIOCTL_COMMIT\n", __func__, __LINE__);
		ret = gc_commit_wrapper((struct gccommit *) arg);
		break;

	case GCIOCTL_MAP:
		GC_PRINT(GC_INFO_MSG " GCIOCTL_MAP\n", __func__, __LINE__);
		ret = gc_map_wrapper((struct gcmap *) arg);
		break;

	case GCIOCTL_UNMAP:
		GC_PRINT(GC_INFO_MSG " GCIOCTL_UNMAP\n", __func__, __LINE__);
		ret = gc_unmap_wrapper((struct gcmap *) arg);
		break;

	case GCIOCTL_BVBLT:
		GC_PRINT(GC_INFO_MSG " GCIOCTL_BVBLT\n", __func__, __LINE__);
		ret = gc_bvblt_wrapper((struct gcbvblt *) arg);
		break;

	case GCIOCTL_BVMAP:
		GC_PRINT(GC_INFO_MSG " GCIOCTL_BVMAP\n", __func__, __LINE__);
		ret = gc_bvmap_wrapper((struct gcbvmap *) arg);
		break;

	case GCIOCTL_BVUNMAP:
		GC_PRINT(GC_INFO_MSG " GCIOCTL_BVUNMAP\n", __func__, __LINE__);
		ret = gc_bvunmap_wrapper((struct gcbvmap *) arg);
		break;

	default:
		GC_PRINT(GC_INFO_MSG " invalid command\n", __func__, __LINE__);
		ret = -EINVAL;
	}

	GC_PRINT(GC_INFO_MSG " ret = %d\n", __func__, __LINE__, ret);

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

static int mod_init(void)
{
	int ret;

	GC_PRINT(GC_INFO_MSG " initializing device.\n", __func__, __LINE__);

	/* Register the character device. */
	dev_major = register_chrdev(0, DEV_NAME, &dev_operations);
	if (dev_major < 0) {
		GC_PRINT(GC_ERR_MSG " failed to allocate device (%d).\n",
			__func__, __LINE__, ret = dev_major);
		goto failed;
	}

	/* Create the device class. */
	dev_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(dev_class)) {
		GC_PRINT(GC_ERR_MSG " failed to create class (%d).\n",
			__func__, __LINE__, ret = PTR_ERR(dev_class));
		goto failed;
	}

	/* Create device. */
	dev_object = device_create(dev_class, NULL, MKDEV(dev_major, 0),
					NULL, DEV_NAME);
	if (IS_ERR(dev_object)) {
		GC_PRINT(GC_ERR_MSG " failed to create device (%d).\n",
			__func__, __LINE__, ret = PTR_ERR(dev_object));
		goto failed;
	}

	/* Initialize mutexes. */
	mutex_init(&g_maplock);
	mutex_init(&g_bufferlock);

	GC_PRINT(GC_INFO_MSG " device number = %d\n",
		__func__, __LINE__, dev_major);

	GC_PRINT(GC_INFO_MSG " device class = 0x%08X\n",
		__func__, __LINE__, (unsigned int) dev_class);

	GC_PRINT(GC_INFO_MSG " device object = 0x%08X\n",
		__func__, __LINE__, (unsigned int) dev_object);

	return 0;

failed:
	mod_exit();
	return ret;
}

static void mod_exit(void)
{
	GC_PRINT(GC_INFO_MSG " cleaning up resources.\n", __func__, __LINE__);

	delete_context_map();

	if ((dev_object != NULL) && !IS_ERR(dev_object)) {
		device_destroy(dev_class, MKDEV(dev_major, 0));
		dev_object = NULL;
	}

	if ((dev_class != NULL) && !IS_ERR(dev_class)) {
		class_destroy(dev_class);
		dev_class = NULL;
	}

	if (dev_major > 0) {
		unregister_chrdev(dev_major, DEV_NAME);
		dev_major = 0;
	}
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
