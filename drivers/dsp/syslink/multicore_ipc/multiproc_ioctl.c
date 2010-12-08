/*
*  multiproc_ioctl.c
*
*  This provides the ioctl interface for multiproc module
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
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <ipc.h>
#include <multiproc.h>
#include <multiproc_ioctl.h>

static struct resource_info *find_mproc_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					struct multiproc_cmd_args *cargs)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_MULTIPROC_DESTROY:
				found = true;
				break;
			}
		}
		if (found == true)
			break;
	}

	spin_unlock(&pr_ctxt->res_lock);

	if (found == false)
		info = NULL;

	return info;
}

/*
 * ======== mproc_ioctl_setup ========
 *  Purpose:
 *  This wrapper function will call the multproc function
 *  to setup the module
 */
static int mproc_ioctl_setup(struct multiproc_cmd_args *cargs)
{
	struct multiproc_config config;
	s32 status = 0;
	ulong size;

	size = copy_from_user(&config,
				(void __user *)cargs->args.setup.config,
				sizeof(struct multiproc_config));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->api_status = multiproc_setup(&config);

exit:
	return status;
}

/*
 * ======== mproc_ioctl_destroy ========
 *	Purpose:
 *	This wrapper function will call the multproc function
 *	to destroy the module
 */
static int mproc_ioctl_destroy(struct multiproc_cmd_args *cargs)
{
	cargs->api_status = multiproc_destroy();
	return 0;
}

/*
 * ======== mproc_ioctl_get_config ========
 *	Purpose:
 *	This wrapper function will call the multproc function
 *	to get the default configuration the module
 */
static int mproc_ioctl_get_config(struct multiproc_cmd_args *cargs)
{
	struct multiproc_config config;
	u32 size;

	multiproc_get_config(&config);
	size = copy_to_user((void __user *)cargs->args.get_config.config,
				&config, sizeof(struct multiproc_config));
	if (size) {
		cargs->api_status = -EFAULT;
		return 0;
	}
	cargs->api_status = 0;
	return 0;
}

/*
 * ======== mproc_ioctl_setup ========
 *	Purpose:
 *	This wrapper function will call the multproc function
 *	to setup the module
 */
static int multiproc_ioctl_set_local_id(struct multiproc_cmd_args *cargs)
{
	cargs->api_status = multiproc_set_local_id(cargs->args.set_local_id.id);
	return 0;
}

/*
 * ======== multiproc_ioctl ========
 *  Purpose:
 *  This ioctl interface for multiproc module
 */
int multiproc_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user)
{
	s32 status = 0;
	s32 size = 0;
	struct multiproc_cmd_args __user *uarg =
				(struct multiproc_cmd_args __user *)args;
	struct multiproc_cmd_args cargs;
	struct ipc_process_context *pr_ctxt =
			(struct ipc_process_context *)filp->private_data;


	if (user == true) {
#ifdef CONFIG_SYSLINK_RECOVERY
		if (ipc_recovering()) {
			status = -EIO;
			goto exit;
		}
#endif
		if (_IOC_DIR(cmd) & _IOC_READ)
			status = !access_ok(VERIFY_WRITE, uarg, _IOC_SIZE(cmd));
		else if (_IOC_DIR(cmd) & _IOC_WRITE)
			status = !access_ok(VERIFY_READ, uarg, _IOC_SIZE(cmd));

		if (status) {
			status = -EFAULT;
			goto exit;
		}

		/* Copy the full args from user-side */
		size = copy_from_user(&cargs, uarg,
					sizeof(struct multiproc_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		if (args != 0)
			memcpy(&cargs, (void *)args,
				sizeof(struct multiproc_cmd_args));
	}

	switch (cmd) {
	case CMD_MULTIPROC_SETUP:
		status = mproc_ioctl_setup(&cargs);
		if (status == 0 && cargs.api_status >= 0)
			add_pr_res(pr_ctxt, CMD_MULTIPROC_DESTROY, NULL);
		break;

	case CMD_MULTIPROC_DESTROY:
	{
		struct resource_info *info = NULL;

		info = find_mproc_resource(pr_ctxt,
						CMD_MULTIPROC_DESTROY,
						&cargs);
		status = mproc_ioctl_destroy(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_MULTIPROC_GETCONFIG:
		status = mproc_ioctl_get_config(&cargs);
		break;

	case CMD_MULTIPROC_SETLOCALID:
		status = multiproc_ioctl_set_local_id(&cargs);
		break;

	default:
		WARN_ON(cmd);
		cargs.api_status = -EFAULT;
		status = -ENOTTY;
		break;
	}

	if ((cargs.api_status == -ERESTARTSYS) || (cargs.api_status == -EINTR))
		status = -ERESTARTSYS;

	if (status < 0)
		goto exit;


	if (user == true) {
		/* Copy the full args to the user-side. */
		size = copy_to_user(uarg, &cargs,
					sizeof(struct multiproc_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}

exit:
	return status;

}
