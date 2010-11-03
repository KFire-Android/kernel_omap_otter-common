/*
 *  sharedregion_ioctl.c
 *
 *  The sharedregion module is designed to be used in a
 *  multi-processor environment where there are memory regions
 *  that are shared and accessed across different processors
 *
 *  Copyright (C) 2008-2010 Texas Instruments, Inc.
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

#include <multiproc.h>
#include <sharedregion.h>
#include <sharedregion_ioctl.h>
#include <platform_mem.h>

static struct resource_info *find_sharedregion_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					struct sharedregion_cmd_args *cargs)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		struct sharedregion_cmd_args *args =
				(struct sharedregion_cmd_args *)info->data;
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_SHAREDREGION_CLEARENTRY:
			{
				u16 id = args->args.clear_entry.id;
				u16 temp = cargs->args.clear_entry.id;
				if (temp == id)
					found = true;
				break;
			}
			case CMD_SHAREDREGION_DESTROY:
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

/* This ioctl interface to sharedregion_get_config function */
static int sharedregion_ioctl_get_config(struct sharedregion_cmd_args *cargs)
{

	struct sharedregion_config config;
	s32 status = 0;
	s32 size;

	sharedregion_get_config(&config);
	size = copy_to_user((void __user *)cargs->args.get_config.config,
			&config, sizeof(struct sharedregion_config));
	if (size)
		status = -EFAULT;

	cargs->api_status = 0;
	return status;
}


/* This ioctl interface to sharedregion_setup function */
static int sharedregion_ioctl_setup(struct sharedregion_cmd_args *cargs)
{
	struct sharedregion_config config;
	struct sharedregion_region region;
	u16 i;
	s32 status = 0;
	s32 size;

	size = copy_from_user(&config, (void __user *)cargs->args.setup.config,
				sizeof(struct sharedregion_config));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->api_status = sharedregion_setup(&config);
	if (cargs->api_status < 0)
		goto exit;

	for (i = 0; i < config.num_entries; i++) {
		sharedregion_get_region_info(i, &region);
		if (region.entry.is_valid == true) {
			/* Convert kernel virtual address to physical
			 * addresses */
			/*region.entry.base = MemoryOS_translate(
					(Ptr) region.entry.base,
					Memory_XltFlags_Virt2Phys);*/
			region.entry.base = platform_mem_translate(
					region.entry.base,
					PLATFORM_MEM_XLT_FLAGS_VIRT2PHYS);
			if (region.entry.base == NULL) {
				pr_err("sharedregion_ioctl_setup: "
					"failed to translate region virtual "
					"address.\n");
				status = -ENOMEM;
				goto exit;
			}
			size = copy_to_user((void __user *)
					&(cargs->args.setup.regions[i]),
					&region,
					sizeof(struct sharedregion_region));
			if (size) {
				status = -EFAULT;
				goto exit;
			}
		}
	}

exit:
	return status;
}

/* This ioctl interface to sharedregion_destroy function */
static int sharedregion_ioctl_destroy(
				struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_destroy();
	return 0;
}

/* This ioctl interface to sharedregion_start function */
static int sharedregion_ioctl_start(struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_start();
	return 0;
}

/* This ioctl interface to sharedregion_stop function */
static int sharedregion_ioctl_stop(struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_stop();
	return 0;
}

/* This ioctl interface to sharedregion_attach function */
static int sharedregion_ioctl_attach(struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_attach(
					cargs->args.attach.remote_proc_id);
	return 0;
}

/* This ioctl interface to sharedregion_detach function */
static int sharedregion_ioctl_detach(struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_detach(
					cargs->args.detach.remote_proc_id);
	return 0;
}

/* This ioctl interface to sharedregion_get_heap function */
static int sharedregion_ioctl_get_heap(struct sharedregion_cmd_args *cargs)
{
	struct heap_object *heap_handle = NULL;
	s32 status = 0;

	heap_handle = (struct heap_object *) sharedregion_get_heap(
						cargs->args.get_heap.id);
	if (heap_handle != NULL)
		cargs->api_status = 0;
	else {
		pr_err("sharedregion_ioctl_get_heap failed: "
			"heap_handle is NULL!");
		cargs->api_status = -1;
	}

	cargs->args.get_heap.heap_handle = heap_handle;

	return status;
}

/* This ioctl interface to sharedregion_clear_entry function */
static int sharedregion_ioctl_clear_entry(struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_clear_entry(
					cargs->args.clear_entry.id);
	return 0;
}

/* This ioctl interface to sharedregion_set_entry function */
static int sharedregion_ioctl_set_entry(struct sharedregion_cmd_args *cargs)
{
	struct sharedregion_entry entry;
	s32 status = 0;

	entry = cargs->args.set_entry.entry;
	/* entry.base = Memory_translate ((Ptr)cargs->args.setEntry.entry.base,
				Memory_XltFlags_Phys2Virt); */
	entry.base = platform_mem_translate(
			(void *)cargs->args.set_entry.entry.base,
			PLATFORM_MEM_XLT_FLAGS_PHYS2VIRT);
	if (entry.base == NULL) {
		pr_err("sharedregion_ioctl_set_entry: failed to"
			"translate region virtual address.\n");
		status = -ENOMEM;
		goto exit;
	}

	cargs->api_status = sharedregion_set_entry(cargs->args.set_entry.id,
							&entry);

exit:
	return status;
}

/* This ioctl interface to sharedregion_reserve_memory function */
static int sharedregion_ioctl_reserve_memory
					(struct sharedregion_cmd_args *cargs)
{
	/* Ignore the return value. */
	sharedregion_reserve_memory(cargs->args.reserve_memory.id,
					cargs->args.reserve_memory.size);
	cargs->api_status = 0;
	return 0;
}

/* This ioctl interface to sharedregion_clear_reserved_memory function */
static int sharedregion_ioctl_clear_reserved_memory(
					struct sharedregion_cmd_args *cargs)
{
	/* Ignore the return value. */
	sharedregion_clear_reserved_memory();
	cargs->api_status = 0;
	return 0;
}

/* This ioctl interface to sharedregion_get_region_info function */
static int sharedregion_ioctl_get_region_info(
				struct sharedregion_cmd_args *cargs)
{
	struct sharedregion_config config;
	struct sharedregion_region region;
	u16 i;
	s32 status = 0;
	s32 size;

	sharedregion_get_config(&config);
	for (i = 0; i < config.num_entries; i++) {
		sharedregion_get_region_info(i, &region);
		if (region.entry.is_valid == true) {
			/* Convert the kernel virtual address to physical
			* addresses */
			/*region.entry.base = MemoryOS_translate (
					(Ptr)region.entry.base,
					Memory_XltFlags_Virt2Phys);*/
			region.entry.base = platform_mem_translate(
					region.entry.base,
					PLATFORM_MEM_XLT_FLAGS_VIRT2PHYS);
			if (region.entry.base == NULL) {
				pr_err(
					"sharedregion_ioctl_get_region_info: "
					"failed to translate region virtual "
					"address.\n");
				status = -ENOMEM;
				goto exit;
			}

			size = copy_to_user(
				(void __user *)&(cargs->args.setup.regions[i]),
				&region,
				sizeof(struct sharedregion_region));
			if (size) {
				status = -EFAULT;
				goto exit;
			}
		} else {
			region.entry.base = NULL;
			size = copy_to_user(
				(void __user *)&(cargs->args.setup.regions[i]),
				&region,
				sizeof(struct sharedregion_region));
			if (size) {
				status = -EFAULT;
				goto exit;
			}
		}
	}
	cargs->api_status = 0;

exit:
	if (status < 0)
		cargs->api_status = -1;
	return status;
}

#if 0
/*
 * ======== sharedregion_ioctl_add ========
 *  Purpose:
 *  This ioctl interface to sharedregion_add function
 */
static int sharedregion_ioctl_add(struct sharedregion_cmd_args *cargs)
{
	u32 base = (u32)platform_mem_translate(cargs->args.add.base,
					PLATFORM_MEM_XLT_FLAGS_PHYS2VIRT);
	cargs->api_status = sharedregion_add(cargs->args.add.index,
					(void *)base, cargs->args.add.len);
	return 0;
}

/*
 * ======== sharedregion_ioctl_get_index ========
 *  Purpose:
 *  This ioctl interface to sharedregion_get_index function
 */
static int sharedregion_ioctl_get_index(struct sharedregion_cmd_args *cargs)
{
	s32 index = 0;

	index = sharedregion_get_index(cargs->args.get_index.addr);
	cargs->args.get_index.index = index;
	cargs->api_status = 0;
	return 0;
}

/*
 * ======== sharedregion_ioctl_get_ptr ========
 *  Purpose:
 *  This ioctl interface to sharedregion_get_ptr function
 */
static int sharedregion_ioctl_get_ptr(struct sharedregion_cmd_args *cargs)
{
	void *addr = NULL;

	addr = sharedregion_get_ptr(cargs->args.get_ptr.srptr);
	/* We are not checking the return from the module, its user
	responsibilty to pass proper value to application
	*/
	cargs->args.get_ptr.addr = addr;
	cargs->api_status = 0;
	return 0;
}

/*
 * ======== sharedregion_ioctl_get_srptr ========
 *  Purpose:
 *  This ioctl interface to sharedregion_get_srptr function
 */
static int sharedregion_ioctl_get_srptr(struct sharedregion_cmd_args *cargs)
{
	u32 *srptr = NULL;

	srptr = sharedregion_get_srptr(cargs->args.get_srptr.addr,
					cargs->args.get_srptr.index);
	/* We are not checking the return from the module, its user
	responsibilty to pass proper value to application
	*/
	cargs->args.get_srptr.srptr = srptr;
	cargs->api_status = 0;
	return 0;
}

/*
 * ======== sharedregion_ioctl_remove ========
 *  Purpose:
 *  This ioctl interface to sharedregion_remove function
 */
static int sharedregion_ioctl_remove(struct sharedregion_cmd_args *cargs)
{
	cargs->api_status = sharedregion_remove(cargs->args.remove.index);
	return 0;
}

/*
 * ======== sharedregion_ioctl_set_table_info ========
 *  Purpose:
 *  This ioctl interface to sharedregion_set_table_info function
 */
static int sharedregion_ioctl_set_table_info(
			struct sharedregion_cmd_args *cargs)
{
	struct sharedregion_info info;
	s32 status = 0;
	s32 size;

	size = copy_from_user(&info, cargs->args.set_table_info.info,
				sizeof(struct sharedregion_info));
	if (size) {
		status = -EFAULT;
		goto exit;
	}

	cargs->api_status = sharedregion_set_table_info(
				cargs->args.set_table_info.index,
				cargs->args.set_table_info.proc_id, &info);

exit:
	return status;
}
#endif

/* This ioctl interface for sharedregion module */
int sharedregion_ioctl(struct inode *inode, struct file *filp,
			unsigned int cmd, unsigned long args, bool user)
{
	s32 status = 0;
	s32 size = 0;
	struct sharedregion_cmd_args __user *uarg =
				(struct sharedregion_cmd_args __user *)args;
	struct sharedregion_cmd_args cargs;
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
					sizeof(struct sharedregion_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		if (args != 0)
			memcpy(&cargs, (void *)args,
				sizeof(struct sharedregion_cmd_args));
	}

	switch (cmd) {
	case CMD_SHAREDREGION_GETCONFIG:
		status = sharedregion_ioctl_get_config(&cargs);
		break;

	case CMD_SHAREDREGION_SETUP:
		status = sharedregion_ioctl_setup(&cargs);
		if (status >= 0)
			add_pr_res(pr_ctxt, CMD_SHAREDREGION_DESTROY, NULL);
		break;

	case CMD_SHAREDREGION_DESTROY:
	{
		struct resource_info *info = NULL;
		info = find_sharedregion_resource(pr_ctxt,
						CMD_SHAREDREGION_DESTROY,
						&cargs);
		status = sharedregion_ioctl_destroy(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_SHAREDREGION_START:
		status = sharedregion_ioctl_start(&cargs);
		break;

	case CMD_SHAREDREGION_STOP:
		status = sharedregion_ioctl_stop(&cargs);
		break;

	case CMD_SHAREDREGION_ATTACH:
		status = sharedregion_ioctl_attach(&cargs);
		break;

	case CMD_SHAREDREGION_DETACH:
		status = sharedregion_ioctl_detach(&cargs);
		break;

	case CMD_SHAREDREGION_GETHEAP:
		status = sharedregion_ioctl_get_heap(&cargs);
		break;

	case CMD_SHAREDREGION_CLEARENTRY:
	{
		struct resource_info *info = NULL;
		info = find_sharedregion_resource(pr_ctxt,
						CMD_SHAREDREGION_CLEARENTRY,
						&cargs);
		status = sharedregion_ioctl_clear_entry(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_SHAREDREGION_SETENTRY:
		status = sharedregion_ioctl_set_entry(&cargs);
		if (status >= 0) {
			struct sharedregion_cmd_args *temp =
				kmalloc(sizeof(struct sharedregion_cmd_args),
					GFP_KERNEL);
			if (WARN_ON(!temp)) {
				status = -ENOMEM;
				goto exit;
			}
			temp->args.clear_entry.id = cargs.args.set_entry.id;
			add_pr_res(pr_ctxt, CMD_SHAREDREGION_CLEARENTRY, temp);
		}
		break;

	case CMD_SHAREDREGION_RESERVEMEMORY:
		status = sharedregion_ioctl_reserve_memory(&cargs);
		break;

	case CMD_SHAREDREGION_CLEARRESERVEDMEMORY:
		status = sharedregion_ioctl_clear_reserved_memory(&cargs);
		break;

	case CMD_SHAREDREGION_GETREGIONINFO:
		status = sharedregion_ioctl_get_region_info(&cargs);
		break;

	default:
		WARN_ON(cmd);
		status = -ENOTTY;
		break;
	}

	if (user == true) {
		/* Copy the full args to the user-side. */
		size = copy_to_user(uarg, &cargs,
					sizeof(struct sharedregion_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}

exit:
	if (status < 0)
		pr_err("sharedregion_ioctl failed! status = 0x%x", status);
	return status;
}
