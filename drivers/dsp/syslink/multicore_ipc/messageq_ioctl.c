/*
 *  messageq_ioctl.c
 *
 *  This file implements all the ioctl operations required on the messageq
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
#include <messageq.h>
#include <messageq_ioctl.h>
#include <sharedregion.h>

static struct resource_info *find_messageq_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					struct messageq_cmd_args *cargs)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		struct messageq_cmd_args *args =
					(struct messageq_cmd_args *)info->data;
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_MESSAGEQ_DELETE:
			{
				void *handle = NULL;
				void *t_handle = NULL;
				if (cargs != NULL && args != NULL) {
					handle = args->args.delete_messageq.
							messageq_handle;
					t_handle = cargs->args.delete_messageq.
							messageq_handle;
					if (t_handle == handle)
						found = true;
				}
				break;
			}
			case CMD_MESSAGEQ_CLOSE:
			{
				if (cargs != NULL && args != NULL) {
					u16 q_id = args->args.close.queue_id;
					u16 temp = cargs->args.close.queue_id;
					if (temp == q_id)
						found = true;
				}
				break;
			}
			case CMD_MESSAGEQ_DESTROY:
			{
				found = true;
				break;
			}
			case CMD_MESSAGEQ_UNREGISTERHEAP:
			{
				if (cargs != NULL && args != NULL) {
					u16 h_id = args->args.unregister_heap.
									heap_id;
					u16 temp = cargs->args.unregister_heap.
									heap_id;
					if (temp == h_id)
						found = true;
				}
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
 * ======== messageq_ioctl_put ========
 *  Purpose:
 *  This ioctl interface to messageq_put function
 */
static inline int messageq_ioctl_put(struct messageq_cmd_args *cargs)
{
	int status = 0;
	messageq_msg msg;

	msg = (messageq_msg) sharedregion_get_ptr(cargs->args.put.msg_srptr);
	if (unlikely(msg == NULL))
		goto exit;

	status = messageq_put(cargs->args.put.queue_id, msg);

	cargs->api_status = status;
exit:
	return 0;
}

/*
 * ======== messageq_ioctl_get ========
 *  Purpose:
 *  This ioctl interface to messageq_get function
 */
static inline int messageq_ioctl_get(struct messageq_cmd_args *cargs)
{
	messageq_msg msg = NULL;
	u32 *msg_srptr = SHAREDREGION_INVALIDSRPTR;
	u16 index;

	cargs->api_status = messageq_get(cargs->args.get.messageq_handle,
					&msg,
					cargs->args.get.timeout);
	if (unlikely(cargs->api_status < 0))
		goto exit;

	index = sharedregion_get_id(msg);
	if (unlikely(index < 0)) {
		cargs->api_status = index;
		goto exit;
	}

	msg_srptr = sharedregion_get_srptr(msg, index);

exit:
	cargs->args.get.msg_srptr = msg_srptr;
	return 0;
}

/*
 * ======== messageq_ioctl_count ========
 *  Purpose:
 *  This ioctl interface to messageq_count function
 */
static inline int messageq_ioctl_count(struct messageq_cmd_args *cargs)
{
	int result = messageq_count(cargs->args.count.messageq_handle);
	if (result < 0)
		cargs->api_status = result;
	else
		cargs->args.count.count = result;

	return 0;
}

/*
 * ======== messageq_ioctl_alloc ========
 *  Purpose:
 *  This ioctl interface to messageq_alloc function
 */
static inline int messageq_ioctl_alloc(struct messageq_cmd_args *cargs)
{
	messageq_msg msg;
	u32 *msg_srptr = SHAREDREGION_INVALIDSRPTR;
	u16 index;

	msg = messageq_alloc(cargs->args.alloc.heap_id, cargs->args.alloc.size);
	if (unlikely(msg == NULL))
		goto exit;

	index = sharedregion_get_id(msg);
	if (unlikely(index < 0))
		goto exit;

	msg_srptr = sharedregion_get_srptr(msg, index);

	cargs->api_status = 0;
exit:
	cargs->args.alloc.msg_srptr = msg_srptr;
	return 0;
}

/*
 * ======== messageq_ioctl_free ========
 *  Purpose:
 *  This ioctl interface to messageq_free function
 */
static inline int messageq_ioctl_free(struct messageq_cmd_args *cargs)
{
	int status = 0;
	messageq_msg msg;

	msg = sharedregion_get_ptr(cargs->args.free.msg_srptr);
	if (unlikely(msg == NULL))
		goto exit;
	status = messageq_free(msg);

	cargs->api_status = status;
exit:
	return 0;
}

/*
 * ======== messageq_ioctl_params_init ========
 *  Purpose:
 *  This ioctl interface to messageq_params_init function
 */
static inline int messageq_ioctl_params_init(struct messageq_cmd_args *cargs)
{
	s32 retval = 0;
	int status = 0;
	unsigned long size;
	struct messageq_params params;

	messageq_params_init(&params);
	size = copy_to_user((void __user *)cargs->args.params_init.params,
				&params, sizeof(struct messageq_params));
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status = status;
exit:
	return retval;
}

/*
 * ======== messageq_ioctl_create ========
 *  Purpose:
 *  This ioctl interface to messageq_create function
 */
static inline int messageq_ioctl_create(struct messageq_cmd_args *cargs)
{
	s32 retval = 0;
	int status = 0;
	unsigned long size;
	struct messageq_params params;
	char *name = NULL;

	if (cargs->args.create.params != NULL) {
		size = copy_from_user(&params,
				(void __user *)cargs->args.create.params,
				sizeof(struct messageq_params));
		if (size) {
			retval = -EFAULT;
			goto exit;
		}
	}

	/* Allocate memory for the name */
	if (cargs->args.create.name_len > 0) {
		name = kmalloc(cargs->args.create.name_len, GFP_KERNEL);
		if (name == NULL) {
			retval = -ENOMEM;
			goto exit;
		}
		size = copy_from_user(name,
					(void __user *)cargs->args.create.name,
					cargs->args.create.name_len);
		if (size) {
			retval = -EFAULT;
			goto free_name;
		}
	}

	if (cargs->args.create.params != NULL) {
		cargs->args.create.messageq_handle = \
			messageq_create(name, &params);
	} else {
		cargs->args.create.messageq_handle = \
			messageq_create(name, NULL);
	}

	if (cargs->args.create.messageq_handle != NULL) {
		cargs->args.create.queue_id = messageq_get_queue_id(
					 cargs->args.create.messageq_handle);
	}

free_name:
	if (cargs->args.create.name_len > 0)
		kfree(name);

	cargs->api_status = status;
exit:
	return retval;
}

/*
 * ======== messageq_ioctl_delete ========
 *  Purpose:
 *  This ioctl interface to messageq_delete function
 */
static inline int messageq_ioctl_delete(struct messageq_cmd_args *cargs)
{
	cargs->api_status =
		messageq_delete(&(cargs->args.delete_messageq.messageq_handle));
	return 0;
}

/*
 * ======== messageq_ioctl_open ========
 *  Purpose:
 *  This ioctl interface to messageq_open function
 */
static inline int messageq_ioctl_open(struct messageq_cmd_args *cargs)
{
	s32 retval = 0;
	int status = 0;
	unsigned long size;
	char *name = NULL;
	u32 queue_id = MESSAGEQ_INVALIDMESSAGEQ;

	/* Allocate memory for the name */
	if (cargs->args.open.name_len > 0) {
		name = kmalloc(cargs->args.open.name_len, GFP_KERNEL);
		if (name == NULL) {
			retval = -ENOMEM;
			goto exit;
		}
		size = copy_from_user(name,
					(void __user *)cargs->args.open.name,
					cargs->args.open.name_len);
		if (size) {
			retval = -EFAULT;
			goto free_name;
		}
	}

	status = messageq_open(name, &queue_id);
	cargs->args.open.queue_id = queue_id;

free_name:
	if (cargs->args.open.name_len > 0)
		kfree(name);

	cargs->api_status = status;
exit:
	return retval;
}

/*
 * ======== messageq_ioctl_close ========
 *  Purpose:
 *  This ioctl interface to messageq_close function
 */
static inline int messageq_ioctl_close(struct messageq_cmd_args *cargs)
{
	u32 queue_id = cargs->args.close.queue_id;
	messageq_close(&queue_id);
	cargs->args.close.queue_id = queue_id;

	cargs->api_status = 0;
	return 0;
}

/*
 * ======== messageq_ioctl_get_config ========
 *  Purpose:
 *  This ioctl interface to messageq_get_config function
 */
static inline int messageq_ioctl_get_config(struct messageq_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	struct messageq_config config;

	messageq_get_config(&config);
	size = copy_to_user((void __user *)cargs->args.get_config.config,
				&config, sizeof(struct messageq_config));
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status = 0;
exit:
	return retval;
}

/*
 * ======== messageq_ioctl_unblock ========
 *  Purpose:
 *  This ioctl interface to messageq_unblock function
 */
static inline int messageq_ioctl_unblock(struct messageq_cmd_args *cargs)
{
	cargs->api_status = messageq_unblock(cargs->args.unblock.messageq_handle);

	return 0;
}

/*
 * ======== messageq_ioctl_setup ========
 *  Purpose:
 *  This ioctl interface to messageq_setup function
 */
static inline int messageq_ioctl_setup(struct messageq_cmd_args *cargs)
{
	s32 retval = 0;
	unsigned long size;
	struct messageq_config config;

	size = copy_from_user(&config, (void __user *)cargs->args.setup.config,
					sizeof(struct messageq_config));
	if (size) {
		retval = -EFAULT;
		goto exit;
	}

	cargs->api_status =  messageq_setup(&config);

exit:
	return retval;
}

/*
 * ======== messageq_ioctl_destroy ========
 *  Purpose:
 *  This ioctl interface to messageq_destroy function
 */
static inline int messageq_ioctl_destroy(struct messageq_cmd_args *cargs)
{
	cargs->api_status = messageq_destroy();
	return 0;
}

/*
 * ======== messageq_ioctl_register_heap ========
 *  Purpose:
 *  This ioctl interface to messageq_register_heap function
 */
static inline int messageq_ioctl_register_heap(struct messageq_cmd_args *cargs)
{
	cargs->api_status = \
		messageq_register_heap(cargs->args.register_heap.heap_handle,
				cargs->args.register_heap.heap_id);
	return 0;
}

/*
 * ======== messageq_ioctl_unregister_heap ========
 *  Purpose:
 *  This ioctl interface to messageq_unregister_heap function
 */
static inline int messageq_ioctl_unregister_heap(
					struct messageq_cmd_args *cargs)
{
	cargs->api_status =  messageq_unregister_heap(
				cargs->args.unregister_heap.heap_id);
	return 0;
}

/*
 * ======== messageq_ioctl_attach ========
 *  Purpose:
 *  This ioctl interface to messageq_ioctl_attach function
 */
static inline int messageq_ioctl_attach(struct messageq_cmd_args *cargs)
{
	void *shared_addr;

	shared_addr = sharedregion_get_ptr(
				cargs->args.attach.shared_addr_srptr);
	if (unlikely(shared_addr == NULL)) {
		cargs->api_status = -1;
		goto exit;
	}
	cargs->api_status = messageq_attach(cargs->args.attach.remote_proc_id,
				shared_addr);

exit:
	return 0;
}

/*
 * ======== messageq_ioctl_detach ========
 *  Purpose:
 *  This ioctl interface to messageq_ioctl_detach function
 */
static inline int messageq_ioctl_detach(struct messageq_cmd_args *cargs)
{
	cargs->api_status = messageq_detach(cargs->args.detach.remote_proc_id);
	return 0;
}

/*
 * ======== messageq_ioctl_sharedmem_req ========
 *  Purpose:
 *  This ioctl interface to messageq_ioctl_sharedmem_req function
 */
static inline int messageq_ioctl_shared_mem_req(struct messageq_cmd_args *cargs)
{
	void *shared_addr;

	shared_addr = sharedregion_get_ptr(
				cargs->args.shared_mem_req.shared_addr_srptr);
	if (unlikely(shared_addr == NULL)) {
		cargs->api_status = -1;
		goto exit;
	}
	cargs->args.shared_mem_req.mem_req = \
			messageq_shared_mem_req(shared_addr);
	cargs->api_status = 0;

exit:
	return 0;
}

/*
 * ======== messageq_ioctl ========
 *  Purpose:
 *  ioctl interface function for messageq module
 */
int messageq_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long args, bool user)
{
	int status = 0;
	struct messageq_cmd_args __user *uarg =
				(struct messageq_cmd_args __user *)args;
	struct messageq_cmd_args cargs;
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
					sizeof(struct messageq_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	} else {
		if (args != 0)
			memcpy(&cargs, (void *)args,
				sizeof(struct messageq_cmd_args));
	}

	switch (cmd) {
	case CMD_MESSAGEQ_PUT:
		status = messageq_ioctl_put(&cargs);
		break;

	case CMD_MESSAGEQ_GET:
		status = messageq_ioctl_get(&cargs);
		break;

	case CMD_MESSAGEQ_COUNT:
		status = messageq_ioctl_count(&cargs);
		break;

	case CMD_MESSAGEQ_ALLOC:
		status = messageq_ioctl_alloc(&cargs);
		break;

	case CMD_MESSAGEQ_FREE:
		status = messageq_ioctl_free(&cargs);
		break;

	case CMD_MESSAGEQ_PARAMS_INIT:
		status = messageq_ioctl_params_init(&cargs);
		break;

	case CMD_MESSAGEQ_CREATE:
		status = messageq_ioctl_create(&cargs);
		if (status >= 0 && cargs.api_status >= 0) {
			struct messageq_cmd_args *temp = kmalloc(
					sizeof(struct messageq_cmd_args),
					GFP_KERNEL);
			if (WARN_ON(!temp)) {
				status = -ENOMEM;
				goto exit;
			}
			temp->args.delete_messageq.messageq_handle =
					cargs.args.create.messageq_handle;
			add_pr_res(pr_ctxt, CMD_MESSAGEQ_DELETE, (void *)temp);
		}
		break;

	case CMD_MESSAGEQ_DELETE:
	{
		struct resource_info *info = NULL;
		info = find_messageq_resource(pr_ctxt,
						CMD_MESSAGEQ_DELETE,
						&cargs);
		status = messageq_ioctl_delete(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_MESSAGEQ_OPEN:
		status = messageq_ioctl_open(&cargs);
		if (status >= 0 && cargs.api_status >= 0) {
			struct messageq_cmd_args *temp = kmalloc(
					sizeof(struct messageq_cmd_args),
					GFP_KERNEL);
			if (WARN_ON(!temp)) {
				status = -ENOMEM;
				goto exit;
			}
			temp->args.close.queue_id = cargs.args.open.queue_id;
			add_pr_res(pr_ctxt, CMD_MESSAGEQ_CLOSE, (void *)temp);
		}
		break;

	case CMD_MESSAGEQ_CLOSE:
	{
		struct resource_info *info = NULL;
		info = find_messageq_resource(pr_ctxt,
						CMD_MESSAGEQ_CLOSE,
						&cargs);
		status = messageq_ioctl_close(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_MESSAGEQ_GETCONFIG:
		status = messageq_ioctl_get_config(&cargs);
		break;

	case CMD_MESSAGEQ_UNBLOCK:
		status = messageq_ioctl_unblock(&cargs);
		break;

	case CMD_MESSAGEQ_SETUP:
		status = messageq_ioctl_setup(&cargs);
		if (status >= 0)
			add_pr_res(pr_ctxt, CMD_MESSAGEQ_DESTROY, NULL);
		break;

	case CMD_MESSAGEQ_DESTROY:
	{
		struct resource_info *info = NULL;
		info = find_messageq_resource(pr_ctxt,
						CMD_MESSAGEQ_DESTROY,
						&cargs);
		status = messageq_ioctl_destroy(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_MESSAGEQ_REGISTERHEAP:
		status = messageq_ioctl_register_heap(&cargs);
		if (cargs.api_status >= 0) {
			struct messageq_cmd_args *temp = kmalloc(
					sizeof(struct messageq_cmd_args),
					GFP_KERNEL);
			if (WARN_ON(!temp)) {
				status = -ENOMEM;
				goto exit;
			}

			temp->args.unregister_heap.heap_id =
					cargs.args.register_heap.heap_id;
			add_pr_res(pr_ctxt, CMD_MESSAGEQ_UNREGISTERHEAP,
					(void *)temp);
		}
		break;

	case CMD_MESSAGEQ_UNREGISTERHEAP:
	{
		struct resource_info *info = NULL;
		info = find_messageq_resource(pr_ctxt,
						CMD_MESSAGEQ_UNREGISTERHEAP,
						&cargs);
		status = messageq_ioctl_unregister_heap(&cargs);
		remove_pr_res(pr_ctxt, info);
		break;
	}

	case CMD_MESSAGEQ_ATTACH:
		status = messageq_ioctl_attach(&cargs);
		break;

	case CMD_MESSAGEQ_DETACH:
		status = messageq_ioctl_detach(&cargs);
		break;

	case CMD_MESSAGEQ_SHAREDMEMREQ:
		status = messageq_ioctl_shared_mem_req(&cargs);
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
					sizeof(struct messageq_cmd_args));
		if (size) {
			status = -EFAULT;
			goto exit;
		}
	}
	return status;

exit:
	printk(KERN_ERR "messageq_ioctl failed: status = 0x%x\n", status);
	return status;
}
