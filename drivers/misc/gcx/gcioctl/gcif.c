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

#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_IOCTL		(1 << 1)

#include <linux/gcx.h>
#include <linux/gccore.h>
#include <linux/gcbv.h>
#include "gcif.h"

static struct mutex g_maplock;

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
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to acquire mutex (0x%08X).\n",
			__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_BUF_ALLOC);
		goto exit;
	}
	bufferlocked = 1;

	if (g_buffervacant == NULL) {
		temp = kmalloc(sizeof(struct gcbuffer), GFP_KERNEL);
		if (temp == NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"out of memory.\n",
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
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to acquire mutex (0x%08X).\n",
			__func__, __LINE__, gcerror);
		gcerror = GCERR_SETGRP(gcerror, GCERR_IOCTL_FIXUP_ALLOC);
		goto exit;
	}
	bufferlocked = 1;

	if (g_fixupvacant == NULL) {
		temp = kmalloc(sizeof(struct gcfixup), GFP_KERNEL);
		if (temp == NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"out of memory.\n",
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
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to acquire mutex (0x%08X).\n",
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
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to read data.\n",
			__func__, __LINE__);
		kgccommit.gcerror = GCERR_USER_READ;
		goto exit;
	}

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
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"failed to read data.\n",
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
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					" failed to read data.\n",
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
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"failed to read data.\n",
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
	if (copy_to_user(&gccommit->gcerror, &kgccommit.gcerror,
				sizeof(enum gcerror))) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

	if (head != NULL)
		put_buffer_tree(head);

	return ret;
}

static int gc_map_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	int mapped = 0;
	struct gcmap kgcmap;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcmap, gcmap, sizeof(struct gcmap))) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to read data.\n",
			__func__, __LINE__);
		kgcmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	kgcmap.pagecount = 0;
	kgcmap.pagearray = NULL;

	/* Call the core driver. */
	gc_map(&kgcmap);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	mapped = 1;

exit:
	if (copy_to_user(gcmap, &kgcmap, offsetof(struct gcmap, logical))) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to write data.\n",
			__func__, __LINE__);
		kgcmap.gcerror = GCERR_USER_WRITE;
		ret = -EFAULT;
	}

	if (kgcmap.gcerror != GCERR_NONE) {
		if (mapped)
			gc_unmap(&kgcmap);
	}

	return ret;
}

static int gc_unmap_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	struct gcmap kgcmap;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcmap, gcmap, sizeof(struct gcmap))) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			" failed to read data.\n",
			__func__, __LINE__);
		kgcmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Call the core driver. */
	gc_unmap(&kgcmap);

exit:
	if (copy_to_user(gcmap, &kgcmap, offsetof(struct gcmap, logical))) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to write data.\n",
			__func__, __LINE__);
		ret = -EFAULT;
	}

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
	return 0;
}

static long dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch (cmd) {
	case GCIOCTL_COMMIT:
		GCPRINT(GCDBGFILTER, GCZONE_IOCTL, GC_MOD_PREFIX
			"GCIOCTL_COMMIT\n", __func__, __LINE__);
		ret = gc_commit_wrapper((struct gccommit *) arg);
		break;

	case GCIOCTL_MAP:
		GCPRINT(GCDBGFILTER, GCZONE_IOCTL, GC_MOD_PREFIX
			"GCIOCTL_MAP\n", __func__, __LINE__);
		ret = gc_map_wrapper((struct gcmap *) arg);
		break;

	case GCIOCTL_UNMAP:
		GCPRINT(GCDBGFILTER, GCZONE_IOCTL, GC_MOD_PREFIX
			"GCIOCTL_UNMAP\n", __func__, __LINE__);
		ret = gc_unmap_wrapper((struct gcmap *) arg);
		break;

	default:
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"invalid command (%d)\n", __func__, __LINE__, cmd);
		ret = -EINVAL;
	}

	GCPRINT(GCDBGFILTER, GCZONE_IOCTL, GC_MOD_PREFIX
		"ret = %d\n", __func__, __LINE__, ret);

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

	GCPRINT(GCDBGFILTER, GCZONE_INIT, GC_MOD_PREFIX
		"initializing device.\n", __func__, __LINE__);

	/* Register the character device. */
	dev_major = register_chrdev(0, GC_DEV_NAME, &dev_operations);
	if (dev_major < 0) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to allocate device (%d).\n",
			__func__, __LINE__, ret = dev_major);
		goto failed;
	}

	/* Create the device class. */
	dev_class = class_create(THIS_MODULE, GC_DEV_NAME);
	if (IS_ERR(dev_class)) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to create class (%d).\n",
			__func__, __LINE__, ret = PTR_ERR(dev_class));
		goto failed;
	}

	/* Create device. */
	dev_object = device_create(dev_class, NULL, MKDEV(dev_major, 0),
					NULL, GC_DEV_NAME);
	if (IS_ERR(dev_object)) {
		GCPRINT(NULL, 0, GC_MOD_PREFIX
			"failed to create device (%d).\n",
			__func__, __LINE__, ret = PTR_ERR(dev_object));
		goto failed;
	}

	/* Initialize mutexes. */
	mutex_init(&g_maplock);
	mutex_init(&g_bufferlock);

	GCPRINT(GCDBGFILTER, GCZONE_INIT, GC_MOD_PREFIX
		"device number = %d\n",
		__func__, __LINE__, dev_major);

	GCPRINT(GCDBGFILTER, GCZONE_INIT, GC_MOD_PREFIX
		"device class = 0x%08X\n",
		__func__, __LINE__, (unsigned int) dev_class);

	GCPRINT(GCDBGFILTER, GCZONE_INIT, GC_MOD_PREFIX
		"device object = 0x%08X\n",
		__func__, __LINE__, (unsigned int) dev_object);

	return 0;

failed:
	mod_exit();
	return ret;
}

static void mod_exit(void)
{
	GCPRINT(GCDBGFILTER, GCZONE_INIT, GC_MOD_PREFIX
		"cleaning up resources.\n", __func__, __LINE__);

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
