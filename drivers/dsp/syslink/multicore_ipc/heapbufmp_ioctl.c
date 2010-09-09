/*
 *  heapbufmp_ioctl.c
 *
 *  Heap module manages fixed size buffers that can be used
 *  in a multiprocessor system with shared memory.
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

#include <sharedregion.h>
#include <heap.h>
#include <heapbufmp.h>
#include <heapbufmp_ioctl.h>


static struct resource_info *find_heapbufmp_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					struct heapbufmp_cmd_args *cargs)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		struct heapbufmp_cmd_args *args =
					(struct heapbufmp_cmd_args *)info->data;
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_HEAPBUFMP_DELETE:
			{
				void *handle =
					args->args.delete.handle;
				void *temp =
					cargs->args.delete.handle;
				if (temp == handle)
					found = true;
				break;
			}
			case CMD_HEAPBUFMP_DESTROY:
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
 * ======== heapbufmp_ioctl_alloc ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_alloc function
 */
static int heapbufmp_ioctl_alloc(struct heapbufmp_cmd_args *cargs)
{
	u32 *block_srptr = SHAREDREGION_INVALIDSRPTR;
	void *block;
	s32 index = SHAREDREGION_INVALIDREGIONID;
	s32 status = 0;

	block = heapbufmp_alloc(cargs->args.alloc.handle,
				cargs->args.alloc.size,
				cargs->args.alloc.align);
	if (block != NULL) {
		index = sharedregion_get_id(block);
		block_srptr = sharedregion_get_srptr(block, index);
	}
	/* The error on above fn will be a null ptr. We are not
	checking that condition here. We are passing whatever
	we are getting from the heapbuf module. So IOCTL will succed,
	but the actual fn might be failed inside heapbuf
	*/
	BUG_ON(index == SHAREDREGION_INVALIDREGIONID);
	cargs->args.alloc.block_srptr = block_srptr;
	BUG_ON(cargs->args.alloc.block_srptr == SHAREDREGION_INVALIDSRPTR);
	cargs->api_status = 0;
	return status;
}

/*
 * ======== heapbufmp_ioctl_free ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_free function
 */
static int heapbufmp_ioctl_free(struct heapbufmp_cmd_args *cargs)
{
	char *block;

	block = sharedregion_get_ptr(cargs->args.free.block_srptr);
	BUG_ON(block == NULL);
	cargs->api_status  = heapbufmp_free(cargs->args.free.handle, block,
					cargs->args.free.size);
	return 0;
}

/*
 * ======== heapbufmp_ioctl_params_init ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_params_init function
 */
static int heapbufmp_ioctl_params_init(struct heapbufmp_cmd_args *cargs)
{
	struct heapbufmp_params params;
	s32 status = 0;
	u32 size;

	heapbufmp_params_init(&params);
	cargs->api_status = 0;
	size = copy_to_user((void __user *)cargs->args.params_init.params,
				&params, sizeof(struct heapbufmp_params));
	if (size)
		status = -EFAULT;

	return status;
}

/*
 * ======== heapbufmp_ioctl_create ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_create function
 */
static int heapbufmp_ioctl_create(struct heapbufmp_cmd_args *cargs)
{
	struct heapbufmp_params params;
	s32 status = 0;
	u32 size;
	void *handle = NULL;

	size = copy_from_user(&params, (void __user *)cargs->args.create.params,
				sizeof(struct heapbufmp_params));
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

	params.shared_addr = sharedregion_get_ptr((u32 *)
				cargs->args.create.shared_addr_srptr);
	params.gate = cargs->args.create.knl_gate;
	handle = heapbufmp_create(&params);
	cargs->args.create.handle = handle;
	cargs->api_status  = 0;

name_from_usr_error:
	if (cargs->args.create.name_len > 0)
		kfree(params.name);

exit:
	return status;
}


/*
 * ======== heapbufmp_ioctl_delete ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_delete function
 */
static int heapbufmp_ioctl_delete(struct heapbufmp_cmd_args *cargs)
{
	cargs->api_status = heapbufmp_delete(&cargs->args.delete.handle);
	return 0;
}

/*
 * ======== heapbufmp_ioctl_open ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_open function
 */
static int heapbufmp_ioctl_open(struct heapbufmp_cmd_args *cargs)
{
	s32 status = 0;
	u32 size = 0;
	void *handle = NULL;
	char *name = NULL;

	if (cargs->args.open.name_len > 0) {
		name = kmalloc(cargs->args.open.name_len, GFP_KERNEL);
		if (name == NULL) {
			status = -ENOMEM;
			goto exit;
		}

		size = copy_from_user(name,
					(void __user *)cargs->args.open.name,
					cargs->args.open.name_len);
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}

	cargs->api_status = heapbufmp_open(cargs->args.open.name, &handle);
	cargs->args.open.handle = handle;

	if (cargs->args.open.name_len > 0)
		kfree(name);
exit:
	return status;
}

/*
 * ======== heapbufmp_ioctl_open_by_addr ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_open_by_addr function
 */
static int heapbufmp_ioctl_open_by_addr(struct heapbufmp_cmd_args *cargs)
{
	void *handle = NULL;
	void *shared_addr;

	shared_addr = sharedregion_get_ptr(cargs->args.
				open_by_addr.shared_addr_srptr);
	cargs->api_status = heapbufmp_open_by_addr(
				shared_addr, &handle);
	cargs->args.open_by_addr.handle = handle;

	return 0;
}


/*
 * ======== heapbufmp_ioctl_close ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_close function
 */
static int heapbufmp_ioctl_close(struct heapbufmp_cmd_args *cargs)
{
	cargs->api_status = heapbufmp_close(&cargs->args.close.handle);
	return 0;
}

/*
 * ======== heapbufmp_ioctl_shared_mem_req ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_shared_mem_req function
 */
static int heapbufmp_ioctl_shared_mem_req(struct heapbufmp_cmd_args *cargs)
{
	struct heapbufmp_params params;
	s32 status = 0;
	ulong size;

	size = copy_from_user(&params,
			(void __user *)cargs->args.shared_mem_req.params,
			sizeof(struct heapbufmp_params));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->args.shared_mem_req.bytes = heapbufmp_shared_mem_req(&params);
	cargs->api_status = 0;

exit:
	return status;
}


/*
 * ======== heapbufmp_ioctl_get_config ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_get_config function
 */
static int heapbufmp_ioctl_get_config(struct heapbufmp_cmd_args *cargs)
{
	struct heapbufmp_config config;
	s32 status = 0;
	ulong size;

	cargs->api_status = heapbufmp_get_config(&config);
	size = copy_to_user((void __user *)cargs->args.get_config.config,
				&config, sizeof(struct heapbufmp_config));
	if (size)
		status = -EFAULT;

	return status;
}

/*
 * ======== heapbufmp_ioctl_setup ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_setup function
 */
static int heapbufmp_ioctl_setup(struct heapbufmp_cmd_args *cargs)
{
	struct heapbufmp_config config;
	s32 status = 0;
	ulong size;

	size = copy_from_user(&config, (void __user *)cargs->args.setup.config,
					sizeof(struct heapbufmp_config));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->api_status = heapbufmp_setup(&config);

exit:
	return status;
}
/*
 * ======== heapbufmp_ioctl_destroy ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_destroy function
 */
static int heapbufmp_ioctl_destroy(struct heapbufmp_cmd_args *cargs)
{
	cargs->api_status = heapbufmp_destroy();
	return 0;
}


/*
 * ======== heapbufmp_ioctl_get_stats ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_get_stats function
 */
static int heapbufmp_ioctl_get_stats(struct heapbufmp_cmd_args *cargs)
{
	struct memory_stats stats;
	s32 status = 0;
	ulong size;

	heapbufmp_get_stats(cargs->args.get_stats.handle, &stats);
	cargs->api_status = 0;

	size = copy_to_user((void __user *)cargs->args.get_stats.stats, &stats,
					sizeof(struct memory_stats));
	if (size)
		status = -EFAULT;

	return status;
}

/*
 * ======== heapbufmp_ioctl_get_extended_stats ========
 *  Purpose:
 *  This ioctl interface to heapbufmp_get_extended_stats function
 */
static int heapbufmp_ioctl_get_extended_stats(struct heapbufmp_cmd_args *cargs)
{
	struct heapbufmp_extended_stats stats;
	s32 status = 0;
	ulong size;
	heapbufmp_get_extended_stats(cargs->args.get_extended_stats.
							handle, &stats);
	cargs->api_status = 0;

	size = copy_to_user((void __user *)cargs->args.get_extended_stats.stats,
				&stats,
				sizeof(struct heapbufmp_extended_stats));
	if (size)
		status = -EFAULT;

	return status;
}

/*
 * ======== heapbufmp_ioctl ========
 *  Purpose:
 *  This ioctl interface for heapbuf module
 */
int heapbufmp_ioctl(struct inode *pinode, struct file *filp,
			unsigned int cmd, unsigned long  args, bool user)
{
	s32 status = 0;
	s32 size = 0;
	struct heapbufmp_cmd_args __user *uarg =
				(struct heapbufmp_cmd_args __user *)args;
	struct heapbufmp_cmd_args cargs;
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
					sizeof(struct heapbufmp_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		if (args != 0)
			memcpy(&cargs, (void *)args,
				sizeof(struct heapbufmp_cmd_args));
	}

	switch (cmd) {
	case CMD_HEAPBUFMP_ALLOC:
		status = heapbufmp_ioctl_alloc(&cargs);
		break;

	case CMD_HEAPBUFMP_FREE:
		status = heapbufmp_ioctl_free(&cargs);
		break;

	case CMD_HEAPBUFMP_PARAMS_INIT:
		status = heapbufmp_ioctl_params_init(&cargs);
		break;

	case CMD_HEAPBUFMP_CREATE:
		status = heapbufmp_ioctl_create(&cargs);
		if (status >= 0) {
			struct heapbufmp_cmd_args *temp =
				kmalloc(sizeof(struct heapbufmp_cmd_args),
						GFP_KERNEL);
			if (WARN_ON(!temp)) {
				status = -ENOMEM;
				goto exit;
			}
			temp->args.delete.handle = cargs.args.create.handle;
			add_pr_res(pr_ctxt, CMD_HEAPBUFMP_DELETE, temp);
		}
		break;

	case CMD_HEAPBUFMP_DELETE:
	{
		struct resource_info *info = NULL;
		info = find_heapbufmp_resource(pr_ctxt,
						CMD_HEAPBUFMP_DELETE,
						&cargs);
		status  = heapbufmp_ioctl_delete(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_HEAPBUFMP_OPEN:
		status  = heapbufmp_ioctl_open(&cargs);
		break;

	case CMD_HEAPBUFMP_OPENBYADDR:
		status  = heapbufmp_ioctl_open_by_addr(&cargs);
		break;

	case CMD_HEAPBUFMP_CLOSE:
		status = heapbufmp_ioctl_close(&cargs);
		break;

	case CMD_HEAPBUFMP_SHAREDMEMREQ:
		status = heapbufmp_ioctl_shared_mem_req(&cargs);
		break;

	case CMD_HEAPBUFMP_GETCONFIG:
		status = heapbufmp_ioctl_get_config(&cargs);
		break;

	case CMD_HEAPBUFMP_SETUP:
		status = heapbufmp_ioctl_setup(&cargs);
		if (status >= 0)
			add_pr_res(pr_ctxt, CMD_HEAPBUFMP_DESTROY, NULL);
		break;

	case CMD_HEAPBUFMP_DESTROY:
	{
		struct resource_info *info = NULL;
		info = find_heapbufmp_resource(pr_ctxt,
						CMD_HEAPBUFMP_DESTROY,
						&cargs);
		status = heapbufmp_ioctl_destroy(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_HEAPBUFMP_GETSTATS:
		status = heapbufmp_ioctl_get_stats(&cargs);
		break;

	case CMD_HEAPBUFMP_GETEXTENDEDSTATS:
		status = heapbufmp_ioctl_get_extended_stats(&cargs);
		break;

	default:
		WARN_ON(cmd);
		status = -ENOTTY;
		break;
	}

	if (user == true) {
		/* Copy the full args to the user-side. */
		size = copy_to_user(uarg, &cargs,
				sizeof(struct heapbufmp_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}

exit:
	return status;
}
