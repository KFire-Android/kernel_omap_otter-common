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
#include <linux/platform_device.h>

#include <linux/gcx.h>
#include <linux/gccore.h>
#include <linux/gcbv.h>
#include <linux/cache-2dmanager.h>

#include "gcif.h"
#include "version.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_INIT		(1 << 0)
#define GCZONE_IOCTL		(1 << 1)

GCDBG_FILTERDEF(ioctl, GCZONE_NONE,
		"init",
		"ioctl")

static struct mutex g_maplock;

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

/*******************************************************************************
 * Command buffer copy management.
 */

static struct mutex g_bufferlock;
struct gcbuffer *g_buffervacant;
struct gcfixup *g_fixupvacant;

static enum gcerror get_buffer(struct gcbuffer **gcbuffer)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcbuffer *temp;

	/* Acquire buffer access mutex. */
	mutex_lock(&g_bufferlock);

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

	*gcbuffer = temp;

exit:
	mutex_unlock(&g_bufferlock);
	return gcerror;
}

static enum gcerror get_fixup(struct gcfixup **gcfixup)
{
	enum gcerror gcerror = GCERR_NONE;
	struct gcfixup *temp;

	/* Acquire fixup access mutex. */
	mutex_lock(&g_bufferlock);

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
	mutex_unlock(&g_bufferlock);
	return gcerror;
}

static void put_buffer_tree(struct gcbuffer *gcbuffer)
{
	struct gcbuffer *prev;
	struct gcbuffer *curr;

	/* Acquire buffer access mutex. */
	mutex_lock(&g_bufferlock);

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

	mutex_unlock(&g_bufferlock);
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
		GCERR("failed to read data.\n");
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
			GCERR("failed to read data.\n");
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
				GCERR("failed to read data.\n");
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
				GCERR("failed to read data.\n");
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
		GCERR("failed to write data.\n");
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
		GCERR("failed to read data.\n");
		kgcmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	kgcmap.pagearray = NULL;

	/* Call the core driver. */
	gc_map(&kgcmap, true);
	if (kgcmap.gcerror != GCERR_NONE)
		goto exit;
	mapped = 1;

exit:
	if (copy_to_user(gcmap, &kgcmap, offsetof(struct gcmap, buf))) {
		GCERR("failed to write data.\n");
		kgcmap.gcerror = GCERR_USER_WRITE;
		ret = -EFAULT;
	}

	if (kgcmap.gcerror != GCERR_NONE) {
		if (mapped)
			gc_unmap(&kgcmap, true);
	}

	return ret;
}

static int gc_unmap_wrapper(struct gcmap *gcmap)
{
	int ret = 0;
	struct gcmap kgcmap;

	/* Get IOCTL parameters. */
	if (copy_from_user(&kgcmap, gcmap, sizeof(struct gcmap))) {
		GCERR("failed to read data.\n");
		kgcmap.gcerror = GCERR_USER_READ;
		goto exit;
	}

	/* Call the core driver. */
	gc_unmap(&kgcmap, true);

exit:
	if (copy_to_user(gcmap, &kgcmap, offsetof(struct gcmap, buf))) {
		GCERR("failed to write data.\n");
		ret = -EFAULT;
	}

	return ret;
}

static int gc_cache_wrapper(struct bvcachexfer *bvcachexfer)
{
	int ret = 0;
	struct bvcachexfer xfer;

	/* Get IOCTL parameters. */
	if (copy_from_user(&xfer, bvcachexfer,
			sizeof(struct bvcachexfer))) {
		GCERR("failed to read data.\n");
		goto exit;
	}

	switch (xfer.dir) {

	case DMA_FROM_DEVICE:
		c2dm_l2cache(xfer.count, (struct c2dmrgn *)&xfer.rgn,
				xfer.dir);
		c2dm_l1cache(xfer.count, (struct c2dmrgn *)&xfer.rgn,
				xfer.dir);
		break;
	case DMA_TO_DEVICE:
		c2dm_l1cache(xfer.count, (struct c2dmrgn *)&xfer.rgn,
				xfer.dir);
		c2dm_l2cache(xfer.count, (struct c2dmrgn *)&xfer.rgn,
				xfer.dir);
		break;
	case DMA_BIDIRECTIONAL:
		c2dm_l1cache(xfer.count, (struct c2dmrgn *)&xfer.rgn,
				xfer.dir);
		c2dm_l2cache(xfer.count, (struct c2dmrgn *)&xfer.rgn,
				xfer.dir);
		break;
	}

exit:
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
		ret = gc_cache_wrapper((struct bvcachexfer *) arg);
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

	/* Initialize mutexes. */
	mutex_init(&g_maplock);
	mutex_init(&g_bufferlock);

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
