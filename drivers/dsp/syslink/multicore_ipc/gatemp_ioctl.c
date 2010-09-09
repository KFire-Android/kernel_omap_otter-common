/*
 *  gatemp_ioctl.c
 *
 *  The Gate Peterson Algorithm for mutual exclusion of shared memory.
 *  Current implementation works for 2 processors.
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
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <gatemp.h>
#include <gatemp_ioctl.h>
#include <sharedregion.h>

static struct resource_info *find_gatemp_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					struct gatemp_cmd_args *cargs)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		struct gatemp_cmd_args *args =
					(struct gatemp_cmd_args *)info->data;
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_GATEMP_DELETE:
			{
				void *handle =
					args->args.delete_instance.handle;
				void *temp =
					cargs->args.delete_instance.handle;
				if (temp == handle)
					found = true;
				break;
			}
			case CMD_GATEMP_DESTROY:
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

/* ioctl interface to gatemp_get_config function*/
static int gatemp_ioctl_get_config(struct gatemp_cmd_args *cargs)
{
	struct gatemp_config config;
	s32 status = 0;
	s32 size;

	gatemp_get_config(&config);
	size = copy_to_user((void __user *)cargs->args.get_config.config,
				&config, sizeof(struct gatemp_config));
	if (size)
		status = -EFAULT;

	cargs->api_status = 0;
	return status;
}

/* ioctl interface to gatemp_setup function */
static int gatemp_ioctl_setup(struct gatemp_cmd_args *cargs)
{
	struct gatemp_config config;
	s32 status = 0;
	s32 size;

	size = copy_from_user(&config, (void __user *)cargs->args.setup.config,
				sizeof(struct gatemp_config));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->api_status = gatemp_setup(&config);

exit:
	return status;
}

/* ioctl interface to gatemp_destroy function */
static int gatemp_ioctl_destroy(struct gatemp_cmd_args *cargs)
{
	cargs->api_status = gatemp_destroy();
	return 0;
}

/* ioctl interface to gatemp_params_init function */
static int gatemp_ioctl_params_init(struct gatemp_cmd_args *cargs)
{
	struct gatemp_params params;
	s32 status = 0;
	s32 size;

	gatemp_params_init(&params);
	size = copy_to_user((void __user *)cargs->args.params_init.params,
				&params, sizeof(struct gatemp_params));
	if (size)
		status = -EFAULT;

	cargs->api_status = 0;
	return status;
}

/* ioctl interface to gatemp_create function */
static int gatemp_ioctl_create(struct gatemp_cmd_args *cargs)
{
	struct gatemp_params params;
	s32 status = 0;
	s32 size;

	cargs->api_status = -1;
	size = copy_from_user(&params, (void __user *)cargs->args.create.params,
					sizeof(struct gatemp_params));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	if (cargs->args.create.name_len > 0) {
		params.name = kmalloc(cargs->args.create.name_len, GFP_KERNEL);
		if (params.name == NULL) {
			status = -ENOMEM;
			goto exit;
		}

		params.name[cargs->args.create.name_len] = '\0';
		size = copy_from_user(params.name,
				(void __user *)cargs->args.create.params->name,
				cargs->args.create.name_len);
		if (size) {
			status = -EFAULT;
			goto name_from_usr_error;
		}

	}

	params.shared_addr = sharedregion_get_ptr(
				(u32 *)cargs->args.create.shared_addr_srptr);
	cargs->args.create.handle = gatemp_create(&params);
	if (cargs->args.create.handle != NULL)
		cargs->api_status = 0;

name_from_usr_error:
	if (cargs->args.open.name_len > 0)
		kfree(params.name);

exit:
	return status;
}

/* ioctl interface to gatemp_ioctl_delete function */
static int gatemp_ioctl_delete(struct gatemp_cmd_args *cargs)

{
	cargs->api_status = gatemp_delete(&cargs->args.delete_instance.handle);
	return 0;
}

/* ioctl interface to gatemp_open function */
static int gatemp_ioctl_open(struct gatemp_cmd_args *cargs)
{
	struct gatemp_params params;
	void *handle = NULL;
	s32 status = 0;
	s32 size;

	gatemp_params_init(&params);
	if (cargs->args.open.name_len > 0) {
		params.name = kmalloc(cargs->args.open.name_len, GFP_KERNEL);
		if (params.name == NULL) {
			status = -ENOMEM;
			goto exit;
		}

		params.name[cargs->args.open.name_len] = '\0';
		size = copy_from_user(params.name,
					(void __user *)cargs->args.open.name,
					cargs->args.open.name_len);
		if (size) {
			status = -EFAULT;
			goto name_from_usr_error;
		}
	}

	cargs->api_status = gatemp_open(params.name, &handle);
	cargs->args.open.handle = handle;

name_from_usr_error:
	if (cargs->args.open.name_len > 0)
		kfree(params.name);

exit:
	return status;
}

/* ioctl interface to gatemp_close function */
static int gatemp_ioctl_close(struct gatemp_cmd_args *cargs)
{
	cargs->api_status = gatemp_close(&cargs->args.close.handle);
	return 0;
}

/* ioctl interface to gatemp_enter function */
static int gatemp_ioctl_enter(struct gatemp_cmd_args *cargs)
{
	cargs->args.enter.flags = gatemp_enter(cargs->args.enter.handle);
	cargs->api_status = 0;
	return 0;
}

/* ioctl interface to gatemp_leave function */
static int gatemp_ioctl_leave(struct gatemp_cmd_args *cargs)
{
	gatemp_leave(cargs->args.leave.handle, cargs->args.leave.flags);
	cargs->api_status = 0;
	return 0;
}

/* ioctl interface to gatemp_shared_mem_req function */
static int gatemp_ioctl_shared_mem_req(struct gatemp_cmd_args *cargs)
{
	struct gatemp_params params;
	s32 status = 0;
	s32 size;

	size = copy_from_user(&params,
			(void __user *)cargs->args.shared_mem_req.params,
			sizeof(struct gatemp_params));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->args.shared_mem_req.ret_val =
		gatemp_shared_mem_req(cargs->args.shared_mem_req.params);
	cargs->api_status = 0;

exit:
	return status;
}

/* ioctl interface to gatemp_open_by_addr function */
static int gatemp_ioctl_open_by_addr(struct gatemp_cmd_args *cargs)
{
	void *shared_addr = NULL;
	void *handle = NULL;
	s32 status = 0;

	/* For open by name, the shared_add_srptr may be invalid */
	if (cargs->args.open_by_addr.shared_addr_srptr != \
					SHAREDREGION_INVALIDSRPTR) {
		shared_addr = sharedregion_get_ptr((u32 *)cargs->args.
						open_by_addr.shared_addr_srptr);
	}
	cargs->api_status = gatemp_open_by_addr(shared_addr, &handle);
	cargs->args.open.handle = handle;

	return status;
}

/* ioctl interface to gatemp_ioctl_get_default_remote function */
static int gatemp_ioctl_get_default_remote(struct gatemp_cmd_args *cargs)
{
	void *handle = NULL;

	handle = gatemp_get_default_remote();
	cargs->args.get_default_remote.handle = handle;
	cargs->api_status = 0;

	return 0;
}

/* ioctl interface for gatemp module */
int gatemp_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user)
{
	s32 status = 0;
	s32 size = 0;
	struct gatemp_cmd_args __user *uarg =
				(struct gatemp_cmd_args __user *)args;
	struct gatemp_cmd_args cargs;
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
						sizeof(struct gatemp_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		if (args != 0)
			memcpy(&cargs, (void *)args,
				sizeof(struct gatemp_cmd_args));
	}

	switch (cmd) {
	case CMD_GATEMP_GETCONFIG:
		status = gatemp_ioctl_get_config(&cargs);
		break;

	case CMD_GATEMP_SETUP:
		status = gatemp_ioctl_setup(&cargs);
		if (status >= 0)
			add_pr_res(pr_ctxt, CMD_GATEMP_DESTROY, NULL);
		break;

	case CMD_GATEMP_DESTROY:
	{
		struct resource_info *info = NULL;
		info = find_gatemp_resource(pr_ctxt, CMD_GATEMP_DESTROY,
							&cargs);
		status = gatemp_ioctl_destroy(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_GATEMP_PARAMS_INIT:
		status  = gatemp_ioctl_params_init(&cargs);
		break;

	case CMD_GATEMP_CREATE:
		status = gatemp_ioctl_create(&cargs);
		if (status >= 0) {
			struct gatemp_cmd_args *temp = kmalloc(
					sizeof(struct gatemp_cmd_args),
					GFP_KERNEL);
			if (WARN_ON(!temp)) {
				status = -ENOMEM;
				goto exit;
			}
			temp->args.delete_instance.handle =
						cargs.args.create.handle;
			add_pr_res(pr_ctxt, CMD_GATEMP_DELETE, (void *)temp);
		}
		break;

	case CMD_GATEMP_DELETE:
	{
		struct resource_info *info = NULL;
		info = find_gatemp_resource(pr_ctxt, CMD_GATEMP_DELETE,
							&cargs);
		status = gatemp_ioctl_delete(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_GATEMP_OPEN:
		status = gatemp_ioctl_open(&cargs);
		break;

	case CMD_GATEMP_CLOSE:
		status = gatemp_ioctl_close(&cargs);
		break;

	case CMD_GATEMP_ENTER:
		status = gatemp_ioctl_enter(&cargs);
		break;

	case CMD_GATEMP_LEAVE:
		status = gatemp_ioctl_leave(&cargs);
		break;

	case CMD_GATEMP_SHAREDMEMREQ:
		status = gatemp_ioctl_shared_mem_req(&cargs);
		break;

	case CMD_GATEMP_OPENBYADDR:
		status = gatemp_ioctl_open_by_addr(&cargs);
		break;

	case CMD_GATEMP_GETDEFAULTREMOTE:
		status = gatemp_ioctl_get_default_remote(&cargs);
		break;

	default:
		WARN_ON(cmd);
		status = -ENOTTY;
		break;
	}

	if (user == true) {
		/* Copy the full args to the user-side. */
		size = copy_to_user(uarg, &cargs,
					sizeof(struct gatemp_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}

exit:
	return status;
}
