/*
 *  sysipc_ioctl.c
 *
 *  This file implements all the ioctl operations required on the sysmgr
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
#include <linux/slab.h>

/* Module Headers */
#include <ipc.h>
#include <sysipc_ioctl.h>
/*#include <platform.h>*/


static struct resource_info *find_sysipc_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					struct sysipc_cmd_args *cargs)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		struct sysipc_cmd_args *args =
					(struct sysipc_cmd_args *)info->data;
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_IPC_CONTROL:
			{
				s32 cmd_id = args->args.control.cmd_id;
				s32 t_cmd_id = cargs->args.control.cmd_id;
				u16 proc_id = args->args.control.proc_id;
				u16 t_proc_id = cargs->args.control.proc_id;
				if (cmd_id == t_cmd_id && proc_id == t_proc_id)
					found = true;
				break;
			}
			case CMD_IPC_DESTROY:
			{
				found = true;
				break;
			}
			}
			if (found == true)
				break;
		}
	}

	spin_unlock(&pr_ctxt->res_lock);

	if (found == false)
		info = NULL;

	return info;
}

/*
 * ioctl interface to ipc_setup function
 */
static inline int sysipc_ioctl_setup(struct sysipc_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	struct ipc_config config;

	size = copy_from_user(&config, (void __user *)cargs->args.setup.config,
					sizeof(struct ipc_config));
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status = ipc_setup(&config);

exit:
	return retval;
}

/*
 * ioctl interface to ipc_control function
 */
static inline int sysipc_ioctl_control(struct sysipc_cmd_args *cargs)
{
	cargs->api_status = ipc_control(cargs->args.control.proc_id,
					cargs->args.control.cmd_id,
					cargs->args.control.arg);
	return 0;
}

/*
 * ioctl interface to ipc_read_config function
 */
static inline int sysipc_ioctl_read_config(struct sysipc_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	void *cfg = NULL;

	cfg = kzalloc(cargs->args.read_config.size, GFP_KERNEL);
	if (cfg == NULL) {
		retval = -ENOMEM;
		goto exit;
	}

	cargs->api_status = ipc_read_config(
					cargs->args.read_config.remote_proc_id,
					cargs->args.read_config.tag, cfg,
					cargs->args.read_config.size);

	size = copy_to_user((void __user *)cargs->args.read_config.cfg, cfg,
					cargs->args.read_config.size);
	if (size)
		retval = -EFAULT;

	kfree(cfg);

exit:
	return retval;
}

/*
 * ioctl interface to ipc_write_config function
 */
static inline int sysipc_ioctl_write_config(struct sysipc_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	void *cfg = NULL;

	cfg = kzalloc(cargs->args.write_config.size, GFP_KERNEL);
	if (cfg == NULL) {
		retval = -ENOMEM;
		goto exit;
	}

	size = copy_from_user(cfg, (void __user *)cargs->args.write_config.cfg,
					cargs->args.write_config.size);
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status = ipc_write_config(
					cargs->args.write_config.remote_proc_id,
					cargs->args.write_config.tag, cfg,
					cargs->args.write_config.size);

	kfree(cfg);

exit:
	return retval;
}

/*
 * ioctl interface to sysmgr_destroy function
 */
static inline int sysipc_ioctl_destroy(struct sysipc_cmd_args *cargs)
{
	cargs->api_status = ipc_destroy();
	return 0;
}

/*
 * ioctl interface function for sysmgr module
 */
int sysipc_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long args, bool user)
{
	int status = 0;
	struct sysipc_cmd_args __user *uarg =
				(struct sysipc_cmd_args __user *)args;
	struct sysipc_cmd_args cargs;
	unsigned long size;
	struct ipc_process_context *pr_ctxt =
			(struct ipc_process_context *)filp->private_data;

	if (user == true) {
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
					sizeof(struct sysipc_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		if (args != 0)
			memcpy(&cargs, (void *)args,
				sizeof(struct sysipc_cmd_args));
	}

	switch (cmd) {
	case CMD_IPC_SETUP:
		status = sysipc_ioctl_setup(&cargs);
		if (status >= 0)
			add_pr_res(pr_ctxt, CMD_IPC_DESTROY, NULL);
		break;

	case CMD_IPC_DESTROY:
	{
		struct resource_info *info = NULL;
		info = find_sysipc_resource(pr_ctxt, CMD_IPC_DESTROY,
							&cargs);
		status = sysipc_ioctl_destroy(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_IPC_CONTROL:
	{
		u32 id = cargs.args.control.proc_id;
		u32 ctrl_cmd = cargs.args.control.cmd_id;
		struct resource_info *info = NULL;

		info = find_sysipc_resource(pr_ctxt, CMD_IPC_CONTROL,
							&cargs);

		status = sysipc_ioctl_control(&cargs);
		if (ctrl_cmd == IPC_CONTROLCMD_STARTCALLBACK) {
			if (status >= 0) {
				struct sysipc_cmd_args *temp = kmalloc(
						sizeof(struct sysipc_cmd_args),
						GFP_KERNEL);
				if (WARN_ON(!temp)) {
					status = -ENOMEM;
					goto exit;
				}
				temp->args.control.cmd_id =
						IPC_CONTROLCMD_STOPCALLBACK;
				temp->args.control.proc_id = id;
				temp->args.control.arg = NULL;
				add_pr_res(pr_ctxt, CMD_IPC_CONTROL,
						(void *)temp);
			}
		} else if (ctrl_cmd == IPC_CONTROLCMD_STOPCALLBACK) {
			remove_pr_res(pr_ctxt, info);
		}
		break;
	}

	case CMD_IPC_READCONFIG:
		status = sysipc_ioctl_read_config(&cargs);
		break;

	case CMD_IPC_WRITECONFIG:
		status = sysipc_ioctl_write_config(&cargs);
		break;

	default:
		WARN_ON(cmd);
		status = -ENOTTY;
		break;
	}
	if (status < 0)
		goto exit;

	if (user == true) {
		/* Copy the full args to the user-side. */
		size = copy_to_user(uarg, &cargs,
					sizeof(struct sysipc_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}

exit:
	return status;
}
