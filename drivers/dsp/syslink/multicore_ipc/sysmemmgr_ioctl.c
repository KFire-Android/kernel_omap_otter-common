/*
 *  sysmemmgr_ioctl.c
 *
 *  This file implements all the ioctl operations required on the sysmemmgr
 *  module.
 *
 *  Copyright (C) 2008-2009 Texas Instruments, Inc.
 *
 *  This package is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *  WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE.
 */

/* Standard headers */
#include <linux/types.h>

/* Linux headers */
#include <linux/uaccess.h>
#include <linux/bug.h>
#include <linux/fs.h>
#include <linux/mm.h>

/* Module Headers */
#include <sysmemmgr.h>
#include <sysmemmgr_ioctl.h>


/*
 * ======== sysmemmgr_ioctl_get_config ========
 *  Purpose:
 *  This ioctl interface to sysmemmgr_get_config function
 */
static inline int sysmemmgr_ioctl_get_config(struct sysmemmgr_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	struct sysmemmgr_config config;

	sysmemmgr_get_config(&config);
	size = copy_to_user(cargs->args.get_config.config, &config,
				sizeof(struct sysmemmgr_config));
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status = 0;
exit:
	return retval;
}

/*
 * ======== sysmemmgr_ioctl_setup ========
 *  Purpose:
 *  This ioctl interface to sysmemmgr_setup function
 */
static inline int sysmemmgr_ioctl_setup(struct sysmemmgr_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	struct sysmemmgr_config config;

	if (cargs->args.setup.config == NULL) {
		cargs->api_status = sysmemmgr_setup(NULL);
		goto exit;
	}

	size = copy_from_user(&config, cargs->args.setup.config,
					sizeof(struct sysmemmgr_config));
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status = sysmemmgr_setup(&config);

exit:
	return retval;
}

/*
 * ======== sysmemmgr_ioctl_destroy ========
 *  Purpose:
 *  This ioctl interface to sysmemmgr_destroy function
 */
static inline int sysmemmgr_ioctl_destroy(struct sysmemmgr_cmd_args *cargs)
{
	cargs->api_status = sysmemmgr_destroy();
	return 0;
}

/*
 * ======== sysmemmgr_ioctl_alloc ========
 *  Purpose:
 *  This ioctl interface to sysmemmgr_alloc function
 */
static inline int sysmemmgr_ioctl_alloc(struct sysmemmgr_cmd_args *cargs)
{
	void *kbuf = NULL;
	void *phys = NULL;

	kbuf = sysmemmgr_alloc(cargs->args.alloc.size,
				cargs->args.alloc.flags);
	if (unlikely(kbuf == NULL))
		goto exit;

	/* If the flag is not virtually contiguous */
	if (cargs->args.alloc.flags != sysmemmgr_allocflag_virtual)
		phys = sysmemmgr_translate(kbuf, sysmemmgr_xltflag_kvirt2phys);
	cargs->api_status = 0;

exit:
	cargs->args.alloc.kbuf = kbuf;
	cargs->args.alloc.kbuf = phys;
	return 0;
}

/*
 * ======== sysmemmgr_ioctl_free ========
 *  Purpose:
 *  This ioctl interface to sysmemmgr_free function
 */
static inline int sysmemmgr_ioctl_free(struct sysmemmgr_cmd_args *cargs)
{
	cargs->api_status = sysmemmgr_free(cargs->args.free.kbuf,
						cargs->args.free.size,
						cargs->args.alloc.flags);
	return 0;
}

/*
 * ======== sysmemmgr_ioctl_translate ========
 *  Purpose:
 *  This ioctl interface to sysmemmgr_translate function
 */
static inline int sysmemmgr_ioctl_translate(struct sysmemmgr_cmd_args *cargs)
{
	cargs->args.translate.ret_ptr = sysmemmgr_translate(
						cargs->args.translate.buf,
						cargs->args.translate.flags);
	WARN_ON(cargs->args.translate.ret_ptr == NULL);
	cargs->api_status = 0;
	return 0;
}

/*
 * ======== sysmemmgr_ioctl ========
 *  Purpose:
 *  ioctl interface function for sysmemmgr module
 */
int sysmemmgr_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long args)
{
	int os_status = 0;
	struct sysmemmgr_cmd_args __user *uarg =
				(struct sysmemmgr_cmd_args __user *)args;
	struct sysmemmgr_cmd_args cargs;
	unsigned long size;

	if (_IOC_DIR(cmd) & _IOC_READ)
		os_status = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		os_status = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));
	if (os_status) {
		os_status = -EFAULT;
		goto exit;
	}

	/* Copy the full args from user-side */
	size = copy_from_user(&cargs, uarg, sizeof(struct sysmemmgr_cmd_args));
	if (size) {
		os_status = -EFAULT;
		goto exit;
	}

	switch (cmd) {
	case CMD_SYSMEMMGR_GETCONFIG:
		os_status = sysmemmgr_ioctl_get_config(&cargs);
		break;

	case CMD_SYSMEMMGR_SETUP:
		os_status = sysmemmgr_ioctl_setup(&cargs);
		break;

	case CMD_SYSMEMMGR_DESTROY:
		os_status = sysmemmgr_ioctl_destroy(&cargs);
		break;

	case CMD_SYSMEMMGR_ALLOC:
		os_status = sysmemmgr_ioctl_alloc(&cargs);
		break;

	case CMD_SYSMEMMGR_FREE:
		os_status = sysmemmgr_ioctl_free(&cargs);
		break;

	case CMD_SYSMEMMGR_TRANSLATE:
		os_status = sysmemmgr_ioctl_translate(&cargs);
		break;

	default:
		WARN_ON(cmd);
		os_status = -ENOTTY;
		break;
	}

	if ((cargs.api_status == -ERESTARTSYS) || (cargs.api_status == -EINTR))
		os_status = -ERESTARTSYS;

	if (os_status < 0)
		goto exit;

	/* Copy the full args to the user-side. */
	size = copy_to_user(uarg, &cargs, sizeof(struct sysmemmgr_cmd_args));
	if (size) {
		os_status = -EFAULT;
		goto exit;
	}

exit:
	return os_status;
}
