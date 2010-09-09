/*
 * drv_notify.h
 *
 * Notify driver support for OMAP Processors.
 *
 * Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#if !defined _DRV_NOTIFY_H_
#define _DRV_NOTIFY_H_


/* Module includes */
#include <syslink/notify_driverdefs.h>
#include <syslink/_notify.h>


/* read function for of Notify driver.*/
int notify_drv_read(struct file *filp, char __user *dst, size_t size,
				loff_t *offset);

/* Linux driver function to map memory regions to user space. */
int notify_drv_mmap(struct file *filp, struct vm_area_struct *vma);

/* ioctl function for of Linux Notify driver.*/
int notify_drv_ioctl(struct inode *inode, struct file *filp, u32 cmd,
						unsigned long args, bool user);

void _notify_drv_setup(void);

void _notify_drv_destroy(void);


#endif  /* !defined (_DRV_NOTIFY_H_) */
