/*
 * drv_notify.c
 *
 * Syslink support functions for TI OMAP processors.
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

#include <generated/autoconf.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/pgtable.h>
#include <linux/types.h>
#include <linux/cdev.h>


#include <syslink/platform_mem.h>
#include <syslink/ipc_ioctl.h>
#include <syslink/drv_notify.h>
#include <syslink/notify_driver.h>
#include <syslink/notify.h>
#include <syslink/notify_ioctl.h>


/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
/* Maximum number of user supported. */
#define MAX_PROCESSES 32

/*Structure of Event callback argument passed to register fucntion*/
struct notify_drv_event_cbck {
	struct list_head element; /* List element header */
	u16 proc_id; /* Processor identifier */
	u16 line_id; /* line identifier */
	u32 event_id; /* Event identifier */
	notify_fn_notify_cbck func; /* Callback function for the event. */
	void *param; /* User callback argument. */
	u32 pid; /* Process Identifier for user process. */
};

/* Keeps the information related to Event.*/
struct notify_drv_event_state {
	struct list_head buf_list;
	/* Head of received event list. */
	u32 pid;
	/* User process ID. */
	u32 ref_count;
	/*Reference count, used when multiple Notify_registerEvent are called
	from same process space(multi threads/processes). */
	struct semaphore *semhandle;
	/* Semaphore for waiting on event. */
	struct semaphore *tersemhandle;
	/* Termination synchronization semaphore. */
};

/* NotifyDrv module state object */
struct notify_drv_module_object {
	bool is_setup;
	/* Indicates whether the module has been already setup */
	bool open_ref_count;
	/* Open reference count. */
	struct mutex *gate_handle;
	/* Handle of gate to be used for local thread safety */
	struct list_head event_cbck_list;
	/* List containg callback arguments for all registered handlers from
	 * user mode. */
	struct list_head single_event_cbck_list;
	/* List containg callback arguments for all registered handlers from
	 user mode for 'single' registrations. */
	struct notify_drv_event_state event_state[MAX_PROCESSES];
	/* List for all user processes registered. */
};

static struct notify_drv_module_object notifydrv_state = {
	.is_setup = false,
	.open_ref_count = 0,
	.gate_handle = NULL
	/*.event_cbck_list = NULL,
	.single_event_cbck_list = NULL*/
};


/* Attach a process to notify user support framework. */
static int notify_drv_attach(u32 pid);

/* Detach a process from notify user support framework. */
static int notify_drv_detach(u32 pid, bool force);

/* This function implements the callback registered with IPS. Here to pass
 * event no. back to user function (so that it can do another level of
 * demultiplexing of callbacks) */
static void _notify_drv_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload);

/* This function adds a data to a registered process. */
static int _notify_drv_add_buf_by_pid(u16 proc_id, u16 line_id, u32 pid,
			u32 event_id, u32 data, notify_fn_notify_cbck cb_fxn,
			void *param);


static struct resource_info *find_notify_drv_resource(
					struct ipc_process_context *pr_ctxt,
					unsigned int cmd,
					void *args)
{
	struct resource_info *info = NULL;
	bool found = false;

	spin_lock(&pr_ctxt->res_lock);

	list_for_each_entry(info, &pr_ctxt->resources, res) {
		if (info->cmd == cmd) {
			switch (cmd) {
			case CMD_NOTIFY_DESTROY:
				found = true;
				break;
			case CMD_NOTIFY_THREADDETACH:
				if ((u32)args == *(u32 *)info->data)
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
 * read data from the driver
 */
int notify_drv_read(struct file *filp, char __user *dst, size_t size,
		loff_t *offset)
{

	bool flag = false;
	struct notify_drv_event_packet *u_buf = NULL;
	int ret_val = 0;
	u32 i;
	struct list_head *elem;
	struct notify_drv_event_packet t_buf;

	if (WARN_ON(notifydrv_state.is_setup == false)) {
		ret_val = -EFAULT;
		goto func_end;
	}

	ret_val = copy_from_user((void *)&t_buf,
				(void __user *)dst,
				sizeof(struct notify_drv_event_packet));
	if (WARN_ON(ret_val != 0))
		ret_val = -EFAULT;

	for (i = 0; i < MAX_PROCESSES; i++) {
		if (notifydrv_state.event_state[i].pid == t_buf.pid) {
			flag = true;
			break;
		}
	}
	if (flag == false) {
		ret_val = -EFAULT;
		goto func_end;
	}

	/* Wait for the event */
	ret_val = down_interruptible(notifydrv_state.event_state[i].semhandle);
	if (ret_val < 0) {
		ret_val = -ERESTARTSYS;
		goto func_end;
	}
	WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
	elem = ((struct list_head *)
			&(notifydrv_state.event_state[i].buf_list))->next;
	u_buf = container_of(elem, struct notify_drv_event_packet, element);
	list_del(elem);
	mutex_unlock(notifydrv_state.gate_handle);
	if (u_buf == NULL) {
		ret_val = -EFAULT;
		goto func_end;
	}
	ret_val = copy_to_user((void __user *)dst, u_buf,
			sizeof(struct notify_drv_event_packet));
	if (WARN_ON(ret_val != 0))
		ret_val = -EFAULT;
	ret_val = sizeof(struct notify_drv_event_packet);

	if (u_buf->is_exit == true)
		up(notifydrv_state.event_state[i].tersemhandle);

	kfree(u_buf);
	u_buf = NULL;

func_end:
	return ret_val;
}

int notify_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
		vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

/* ioctl function for of Linux Notify driver. */
int notify_drv_ioctl(struct inode *inode, struct file *filp, u32 cmd,
					unsigned long args, bool user)
{
	int status = NOTIFY_S_SUCCESS;
	int os_status = 0;
	unsigned long size;
	struct notify_cmd_args *cmd_args = (struct notify_cmd_args *)args;
	struct notify_cmd_args common_args;
	struct ipc_process_context *pr_ctxt =
		(struct ipc_process_context *)filp->private_data;

	switch (cmd) {
	case CMD_NOTIFY_GETCONFIG:
	{
		struct notify_cmd_args_get_config *src_args =
				(struct notify_cmd_args_get_config *)args;
		struct notify_config cfg;

		notify_get_config(&cfg);
		size = copy_to_user((void __user *) (src_args->cfg),
			(const void *) &cfg, sizeof(struct notify_config));
		if (WARN_ON(size != 0))
			os_status = -EFAULT;
	}
	break;

	case CMD_NOTIFY_SETUP:
	{
		struct notify_cmd_args_setup *src_args =
					(struct notify_cmd_args_setup *) args;
		struct notify_config cfg;

		size = copy_from_user((void *) &cfg,
					(const void __user *) (src_args->cfg),
					sizeof(struct notify_config));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}
		status = notify_setup(&cfg);
		if (status >= 0)
			add_pr_res(pr_ctxt, CMD_NOTIFY_DESTROY, NULL);
	}
	break;

	case CMD_NOTIFY_DESTROY:
	{
		struct resource_info *info = NULL;
		/* copy_from_user is not needed for Notify_getConfig, since the
		 * user's config is not used.
		 */
		info = find_notify_drv_resource(pr_ctxt,
						CMD_NOTIFY_DESTROY,
						NULL);
		status = notify_destroy();
		remove_pr_res(pr_ctxt, info);
	}
	break;

	case CMD_NOTIFY_REGISTEREVENTSINGLE:
	{
		struct notify_cmd_args_register_event src_args;
		struct notify_drv_event_cbck *cbck = NULL;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *) &src_args,
				(const void __user *) (args),
				sizeof(struct notify_cmd_args_register_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}

		cbck = kmalloc(sizeof(struct notify_drv_event_cbck),
					GFP_ATOMIC);
		WARN_ON(cbck == NULL);
		cbck->proc_id = src_args.proc_id;
		cbck->line_id = src_args.line_id;
		cbck->event_id = src_args.event_id;
		cbck->pid = src_args.pid;
		cbck->func = src_args.fn_notify_cbck;
		cbck->param = src_args.cbck_arg;
		status = notify_register_event_single(src_args.proc_id,
					src_args.line_id, src_args.event_id,
					_notify_drv_callback, (void *)cbck);
		if (status < 0) {
			/* This does not impact return status of this function,
			 * so retval comment is not used. */
			kfree(cbck);
		} else {
			WARN_ON(mutex_lock_interruptible
					(notifydrv_state.gate_handle));
			INIT_LIST_HEAD((struct list_head *)&(cbck->element));
			list_add_tail(&(cbck->element),
				&(notifydrv_state.single_event_cbck_list));
			mutex_unlock(notifydrv_state.gate_handle);
		}
	}
	break;

	case CMD_NOTIFY_REGISTEREVENT:
	{
		struct notify_cmd_args_register_event src_args;
		struct notify_drv_event_cbck *cbck = NULL;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *) &src_args,
				(const void __user *) (args),
				sizeof(struct notify_cmd_args_register_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}

		cbck = kmalloc(sizeof(struct notify_drv_event_cbck),
					GFP_ATOMIC);
		WARN_ON(cbck == NULL);
		cbck->proc_id = src_args.proc_id;
		cbck->line_id = src_args.line_id;
		cbck->event_id = src_args.event_id;
		cbck->func = src_args.fn_notify_cbck;
		cbck->param = src_args.cbck_arg;
		cbck->pid = src_args.pid;
		status = notify_register_event(src_args.proc_id,
					src_args.line_id, src_args.event_id,
					_notify_drv_callback, (void *)cbck);
		if (status < 0) {
			/* This does not impact return status of this function,
			 * so retval comment is not used. */
			kfree(cbck);
		} else {
			WARN_ON(mutex_lock_interruptible
					(notifydrv_state.gate_handle));
			INIT_LIST_HEAD((struct list_head *)&(cbck->element));
			list_add_tail(&(cbck->element),
				&(notifydrv_state.event_cbck_list));
			mutex_unlock(notifydrv_state.gate_handle);
		}
	}
	break;

	case CMD_NOTIFY_UNREGISTEREVENTSINGLE:
	{
		bool found = false;
		u32 pid;
		struct notify_drv_event_cbck *cbck = NULL;
		struct list_head *entry = NULL;
		struct notify_cmd_args_unregister_event src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *)&src_args,
			(const void __user *)(args),
			sizeof(struct notify_cmd_args_unregister_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}

		WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
		pid = src_args.pid;
		list_for_each(entry, (struct list_head *)
			&(notifydrv_state.single_event_cbck_list)) {
			cbck = (struct notify_drv_event_cbck *)(entry);
			if ((cbck->proc_id == src_args.proc_id) &&
				(cbck->line_id == src_args.line_id) &&
				(cbck->event_id == src_args.event_id) &&
				(cbck->pid == pid)) {
					found = true;
					break;
			}
		}
		mutex_unlock(notifydrv_state.gate_handle);
		if (found == false) {
			status = NOTIFY_E_NOTFOUND;
			goto func_end;
		}
		status = notify_unregister_event_single(src_args.proc_id,
				src_args.line_id, src_args.event_id);
		/* This check is needed at run-time also to propagate the
		 * status to user-side. This must not be optimized out. */
		if (status >= 0) {
			WARN_ON(mutex_lock_interruptible
						(notifydrv_state.gate_handle));
			list_del((struct list_head *)cbck);
			mutex_unlock(notifydrv_state.gate_handle);
			kfree(cbck);
		}
	}
	break;

	case CMD_NOTIFY_UNREGISTEREVENT:
	{
		bool found = false;
		u32 pid;
		struct notify_drv_event_cbck *cbck = NULL;
		struct list_head *entry = NULL;
		struct notify_cmd_args_unregister_event src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *)&src_args,
			(const void __user *)(args),
			sizeof(struct notify_cmd_args_unregister_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}

		WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
		pid = src_args.pid;
		list_for_each(entry, (struct list_head *)
			&(notifydrv_state.event_cbck_list)) {
			cbck = (struct notify_drv_event_cbck *)(entry);
			if ((cbck->func == src_args.fn_notify_cbck) &&
				(cbck->param == src_args.cbck_arg) &&
				(cbck->proc_id == src_args.proc_id) &&
				(cbck->line_id == src_args.line_id) &&
				(cbck->event_id == src_args.event_id) &&
				(cbck->pid == pid)) {
					found = true;
					break;
			}
		}
		mutex_unlock(notifydrv_state.gate_handle);
		if (found == false) {
			status = NOTIFY_E_NOTFOUND;
			goto func_end;
		}
		status = notify_unregister_event(src_args.proc_id,
				src_args.line_id, src_args.event_id,
				_notify_drv_callback, (void *) cbck);
		/* This check is needed at run-time also to propagate the
		 * status to user-side. This must not be optimized out. */
		if (status >= 0) {
			WARN_ON(mutex_lock_interruptible
						(notifydrv_state.gate_handle));
			list_del((struct list_head *)cbck);
			mutex_unlock(notifydrv_state.gate_handle);
			kfree(cbck);
		}
	}
	break;

	case CMD_NOTIFY_SENDEVENT:
	{
		struct notify_cmd_args_send_event src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *) &src_args,
				(const void __user *) (args),
				sizeof(struct notify_cmd_args_send_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}
		status = notify_send_event(src_args.proc_id, src_args.line_id,
				src_args.event_id, src_args.payload,
				src_args.wait_clear);
	}
	break;

	case CMD_NOTIFY_DISABLE:
	{
		struct notify_cmd_args_disable src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *) &src_args,
				(const void __user *) (args),
				sizeof(struct notify_cmd_args_disable));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}
		src_args.flags = notify_disable(src_args.proc_id,
							src_args.line_id);

		/* Copy the full args to user-side */
		size = copy_to_user((void __user *) (args),
					(const void *) &src_args,
					sizeof(struct notify_cmd_args_disable));
		/* This check is needed at run-time also since it depends on
		 * run environment. It must not be optimized out. */
		if (WARN_ON(size != 0))
			os_status = -EFAULT;
	}
	break;

	case CMD_NOTIFY_RESTORE:
	{
		struct notify_cmd_args_restore src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *) &src_args,
				(const void __user *)(args),
				sizeof(struct notify_cmd_args_restore));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}
		notify_restore(src_args.proc_id, src_args.line_id,
			src_args.key);
	}
	break;

	case CMD_NOTIFY_DISABLEEVENT:
	{
		struct notify_cmd_args_disable_event src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *) &src_args,
				(const void __user *)(args),
				sizeof(struct notify_cmd_args_disable_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}
		notify_disable_event(src_args.proc_id, src_args.line_id,
						src_args.event_id);
	}
	break;

	case CMD_NOTIFY_ENABLEEVENT:
	{
		struct notify_cmd_args_enable_event src_args;

		/* Copy the full args from user-side. */
		size = copy_from_user((void *)&src_args,
				(const void __user *)(args),
				sizeof(struct notify_cmd_args_enable_event));
		if (WARN_ON(size != 0)) {
			os_status = -EFAULT;
			goto func_end;
		}
		notify_enable_event(src_args.proc_id, src_args.line_id,
						src_args.event_id);
	}
	break;

	case CMD_NOTIFY_THREADATTACH:
	{
		u32 pid = *((u32 *)args);
		status = notify_drv_attach(pid);
		if (status < 0)
			pr_err("NOTIFY_ATTACH FAILED\n");
		else {
			u32 *data = kmalloc(sizeof(u32), GFP_KERNEL);
			if (WARN_ON(!data)) {
				notify_drv_detach(pid, true);
				os_status = -ENOMEM;
				goto func_end;
			}
			*data = pid;
			add_pr_res(pr_ctxt, CMD_NOTIFY_THREADDETACH,
								(void *)data);
		}
	}
	break;

	case CMD_NOTIFY_THREADDETACH:
	{
		struct resource_info *info = NULL;
		u32 pid = *((u32 *)args);
		info = find_notify_drv_resource(pr_ctxt,
						CMD_NOTIFY_THREADDETACH,
						(void *)pid);
		if (user == true)
			status = notify_drv_detach(pid, false);
		else
			status = notify_drv_detach(pid, true);
		if (status < 0)
			pr_err("NOTIFY_DETACH FAILED\n");
		else
			remove_pr_res(pr_ctxt, info);
	}
	break;

	case CMD_NOTIFY_ATTACH:
	{
		struct notify_cmd_args_attach src_args;
		void *knl_shared_addr;

		size = copy_from_user((void *) &src_args,
					(const void __user *)(args),
					sizeof(struct notify_cmd_args_attach));
		if (size != 0) {
			os_status = -EFAULT;
			goto func_end;
		}

		/* knl_shared_addr = Memory_translate(src_args.shared_addr,
						Memory_XltFlags_Phys2Virt); */
		knl_shared_addr = platform_mem_translate(
					(void *)src_args.shared_addr,
					PLATFORM_MEM_XLT_FLAGS_PHYS2VIRT);
		status = notify_attach(src_args.proc_id, knl_shared_addr);
	}
	break;

	case CMD_NOTIFY_DETACH:
	{
		struct notify_cmd_args_detach src_args;

		size = copy_from_user((void *) &src_args,
					(const void __user *)(args),
					sizeof(struct notify_cmd_args_detach));
		if (size != 0) {
			os_status = -EFAULT;
			goto func_end;
		}

		status = notify_detach(src_args.proc_id);
	}
	break;

	case CMD_NOTIFY_SHAREDMEMREQ:
	{
		struct notify_cmd_args_shared_mem_req src_args;
		void *knl_shared_addr;

		size = copy_from_user((void *) &src_args,
				(const void __user *)(args),
				sizeof(struct notify_cmd_args_shared_mem_req));
		if (size != 0) {
			os_status = -EFAULT;
			goto func_end;
		}

		/* knl_shared_addr = Memory_translate(src_args.shared_addr,
						Memory_XltFlags_Phys2Virt); */
		knl_shared_addr = platform_mem_translate(
					(void *)src_args.shared_addr,
					PLATFORM_MEM_XLT_FLAGS_PHYS2VIRT);
		status = notify_shared_mem_req(src_args.proc_id,
						knl_shared_addr);
	}
	break;

	case CMD_NOTIFY_ISREGISTERED:
	{
		struct notify_cmd_args_is_registered src_args;

		size = copy_from_user((void *) &src_args,
				(const void __user *)(args),
				sizeof(struct notify_cmd_args_is_registered));
		if (size != 0) {
			os_status = -EFAULT;
			goto func_end;
		}

		src_args.is_registered = notify_is_registered(src_args.proc_id,
						src_args.line_id);
		size = copy_to_user((void __user *) (args),
				(const void *)&src_args,
				sizeof(struct notify_cmd_args_is_registered));
		if (size != 0) {
			os_status = -EFAULT;
			goto func_end;
		}
	}
	break;

	default:
	{
		/* This does not impact return status of this function,so retval
		 * comment is not used. */
		status = NOTIFY_E_INVALIDARG;
		pr_err("not valid command\n");
	}
	break;
	}

func_end:
	/* Set the status and copy the common args to user-side. */
	common_args.api_status = status;
	if (user == true) {
		size = copy_to_user((void __user *) cmd_args,
					(const void *) &common_args,
					sizeof(struct notify_cmd_args));
		if (size)
			os_status = -EFAULT;
	}
	return os_status;
}

/* This function implements the callback registered with IPS. Here
 * to pass event no. back to user function(so that it can do another
 * level of demultiplexing of callbacks) */
static void _notify_drv_callback(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload)
{
	struct notify_drv_event_cbck *cbck;
	int status = 0;

	if (WARN_ON(notifydrv_state.is_setup == false)) {
		status = -EFAULT;
		goto func_end;
	}

	if (WARN_ON(arg == NULL)) {
		status = -EINVAL;
		goto func_end;
	}

	cbck = (struct notify_drv_event_cbck *)arg;
	status = _notify_drv_add_buf_by_pid(proc_id, line_id, cbck->pid,
				event_id, payload, cbck->func, cbck->param);

func_end:
	if (status < 0)
		pr_err("_notify_drv_callback failed! status = 0x%x", status);
	return;
}

/* This function adds a data to a registered process. */
static int _notify_drv_add_buf_by_pid(u16 proc_id, u16 line_id, u32 pid,
					u32 event_id, u32 data,
					notify_fn_notify_cbck cb_fxn,
					void *param)
{
	s32 status = 0;
	bool flag = false;
	bool is_exit = false;
	struct notify_drv_event_packet *u_buf = NULL;
	u32 i;

	WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
	for (i = 0; (i < MAX_PROCESSES) && (flag != true); i++) {
		if (notifydrv_state.event_state[i].pid == pid) {
			flag = true;
			break;
		}
	}
	mutex_unlock(notifydrv_state.gate_handle);

	if (WARN_ON(flag == false)) {
		status = -EFAULT;
		goto func_end;
	}

	u_buf = kmalloc(sizeof(struct notify_drv_event_packet), GFP_ATOMIC);
	if (u_buf == NULL) {
		status = -ENOMEM;
		goto func_end;
	}

	INIT_LIST_HEAD((struct list_head *)&u_buf->element);
	u_buf->proc_id = proc_id;
	u_buf->line_id = line_id;
	u_buf->data = data;
	u_buf->event_id = event_id;
	u_buf->func = cb_fxn;
	u_buf->param = param;
	u_buf->is_exit = false;
	if (u_buf->event_id == (u32) -1) {
		u_buf->is_exit = true;
		is_exit = true;
	}
	WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
	list_add_tail((struct list_head *)&(u_buf->element),
		(struct list_head *)&(notifydrv_state.event_state[i].buf_list));
	mutex_unlock(notifydrv_state.gate_handle);
	up(notifydrv_state.event_state[i].semhandle);

	/* Termination packet */
	if (is_exit == true) {
		if (down_interruptible(
			notifydrv_state.event_state[i].tersemhandle))
			status = NOTIFY_E_OSFAILURE;
	}

func_end:
	if (status < 0) {
		pr_err("_notify_drv_add_buf_by_pid failed! "
			"status = 0x%x", status);
	}
	return status;
}

/* Module setup function.*/
void _notify_drv_setup(void)
{
	int i;

	INIT_LIST_HEAD((struct list_head *)&(notifydrv_state.event_cbck_list));
	INIT_LIST_HEAD(
		(struct list_head *)&(notifydrv_state.single_event_cbck_list));
	notifydrv_state.gate_handle = kmalloc(sizeof(struct mutex), GFP_KERNEL);
	mutex_init(notifydrv_state.gate_handle);
	for (i = 0; i < MAX_PROCESSES; i++) {
		notifydrv_state.event_state[i].pid = -1;
		notifydrv_state.event_state[i].ref_count = 0;
		INIT_LIST_HEAD((struct list_head *)
			&(notifydrv_state.event_state[i].buf_list));
	}
	notifydrv_state.is_setup = true;
}

/* Module destroy function.*/
void _notify_drv_destroy(void)
{
	int i;
	struct notify_drv_event_packet *packet;
	struct list_head *entry, *n;
	struct notify_drv_event_cbck *cbck;

	for (i = 0; i < MAX_PROCESSES; i++) {
		notifydrv_state.event_state[i].pid = -1;
		notifydrv_state.event_state[i].ref_count = 0;
		/* Free event packets for any received but unprocessed events.*/
		list_for_each_safe(entry, n, (struct list_head *)
				&(notifydrv_state.event_state[i].buf_list)) {
			packet = (struct notify_drv_event_packet *)entry;
			kfree(packet);
		}
		INIT_LIST_HEAD(&notifydrv_state.event_state[i].buf_list);
	}

	/* Clear any event registrations that were not unregistered. */
	list_for_each_safe(entry, n, (struct list_head *)
			&(notifydrv_state.event_cbck_list)) {
		cbck = (struct notify_drv_event_cbck *)(entry);
		kfree(cbck);
	}
	INIT_LIST_HEAD(&notifydrv_state.event_cbck_list);

	/* Clear any event registrations that were not unregistered from single
	* list. */
	list_for_each_safe(entry, n,
		(struct list_head *)&(notifydrv_state.single_event_cbck_list)) {
		cbck = (struct notify_drv_event_cbck *)(entry);
		kfree(cbck);
	}
	INIT_LIST_HEAD(&notifydrv_state.single_event_cbck_list);

	mutex_destroy(notifydrv_state.gate_handle);
	kfree(notifydrv_state.gate_handle);
	notifydrv_state.is_setup = false;
	return;
}

/* Attach a process to notify user support framework. */
static int notify_drv_attach(u32 pid)
{
	bool flag = false;
	bool is_init = false;
	u32 i;
	struct semaphore *sem_handle = NULL;
	struct semaphore *ter_sem_handle = NULL;
	int ret_val = 0;

	if (WARN_ON(notifydrv_state.is_setup == false)) {
		ret_val = NOTIFY_E_FAIL;
		goto exit;
	}

	WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
	for (i = 0; (i < MAX_PROCESSES); i++) {
		if (notifydrv_state.event_state[i].pid == pid) {
			notifydrv_state.event_state[i].ref_count++;
			is_init = true;
			break;
		}
	}
	if (is_init == true) {
		mutex_unlock(notifydrv_state.gate_handle);
		return 0;
	}

	sem_handle = kmalloc(sizeof(struct semaphore), GFP_ATOMIC);
	ter_sem_handle = kmalloc(sizeof(struct semaphore), GFP_ATOMIC);
	if (sem_handle == NULL || ter_sem_handle == NULL) {
		ret_val = -ENOMEM;
		goto sem_fail;
	}
	sema_init(sem_handle, 0);
	/* Create the termination semaphore */
	sema_init(ter_sem_handle, 0);

	/* Search for an available slot for user process. */
	for (i = 0; i < MAX_PROCESSES; i++) {
		if (notifydrv_state.event_state[i].pid == -1) {
			notifydrv_state.event_state[i].semhandle = \
						sem_handle;
			notifydrv_state.event_state[i].tersemhandle = \
						ter_sem_handle;
			notifydrv_state.event_state[i].pid = pid;
			notifydrv_state.event_state[i].ref_count = 1;
			INIT_LIST_HEAD(&(notifydrv_state.event_state[i].
							buf_list));
			flag = true;
			break;
		}
	}
	mutex_unlock(notifydrv_state.gate_handle);

	if (WARN_ON(flag != true)) {
		/* Max users have registered. No more clients
		 * can be supported */
		ret_val = NOTIFY_E_RESOURCE;
		goto sem_fail;
	}

	return 0;

sem_fail:
	kfree(ter_sem_handle);
	kfree(sem_handle);
exit:
	return ret_val;
}


/* Detach a process from notify user support framework. */
static int notify_drv_detach(u32 pid, bool force)
{
	s32 status = NOTIFY_S_SUCCESS;
	bool flag = false;
	u32 i;
	struct semaphore *sem_handle = NULL;
	struct semaphore *ter_sem_handle = NULL;

	if (WARN_ON(notifydrv_state.is_setup == false)) {
		status = NOTIFY_E_FAIL;
		goto func_end;
	}

	/* Send termination packet only if this is not a forced detach */
	if (force == false) {
		/* Send the termination packet to notify thread */
		status = _notify_drv_add_buf_by_pid(0, 0, pid, (u32)-1, (u32)0,
							NULL, NULL);
	}

	WARN_ON(mutex_lock_interruptible(notifydrv_state.gate_handle));
	for (i = 0; i < MAX_PROCESSES; i++) {
		if (notifydrv_state.event_state[i].pid == pid) {
			if (notifydrv_state.event_state[i].ref_count == 1) {
				/* Last client being unregistered for this
				* process*/
				notifydrv_state.event_state[i].pid = -1;
				notifydrv_state.event_state[i].ref_count = 0;
				sem_handle =
				notifydrv_state.event_state[i].semhandle;
				ter_sem_handle =
				notifydrv_state.event_state[i].tersemhandle;
				INIT_LIST_HEAD((struct list_head *)
				&(notifydrv_state.event_state[i].buf_list));
				notifydrv_state.event_state[i].semhandle =
								NULL;
				notifydrv_state.event_state[i].tersemhandle =
								NULL;
				flag = true;
				break;
			} else
				notifydrv_state.event_state[i].ref_count--;
		}
	}
	mutex_unlock(notifydrv_state.gate_handle);

	if ((flag == false) && (i == MAX_PROCESSES)) {
		/* The specified user process was not found registered with
		 * Notify Driver module. */
		status = NOTIFY_E_NOTFOUND;
	} else {
		kfree(sem_handle);
		kfree(ter_sem_handle);
	}

func_end:
	return status;
}
