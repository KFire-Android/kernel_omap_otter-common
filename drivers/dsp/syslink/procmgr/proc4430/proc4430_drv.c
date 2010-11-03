/*
 * proc4430_drv.c
 *
 * Syslink driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>


/* Module headers */
#include "proc4430.h"
#include "proc4430_drvdefs.h"



/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
#define PROC4430_NAME "syslink-proc4430"

static char *driver_name = PROC4430_NAME;

static s32 driver_major;

static s32 driver_minor;

struct proc_4430_dev {
	struct cdev cdev;
};

static struct proc_4430_dev *proc_4430_device;

static struct class *proc_4430_class;



/** ============================================================================
 *  Forward declarations of internal functions
 *  ============================================================================
 */
/* Linux driver function to open the driver object. */
static int proc4430_drv_open(struct inode *inode, struct file *filp);

/* Linux driver function to close the driver object. */
static int proc4430_drv_release(struct inode *inode, struct file *filp);

/* Linux driver function to invoke the APIs through ioctl. */
static int proc4430_drv_ioctl(struct inode *inode,
			struct file *filp, unsigned int cmd,
			unsigned long args);

/* Linux driver function to map memory regions to user space. */
static int proc4430_drv_mmap(struct file *filp,
			struct vm_area_struct *vma);

/* Module initialization function for Linux driver. */
static int __init proc4430_drv_initializeModule(void);

/* Module finalization  function for Linux driver. */
static void  __exit proc4430_drv_finalizeModule(void);


/* Process Context */
struct proc4430_process_context {
	u32 setup_count;
};

/** ============================================================================
 *  Globals
 *  ============================================================================
 */

/*
  File operations table for PROC4430.
 */
static const struct file_operations proc_4430_fops = {
	.open = proc4430_drv_open,
	.release = proc4430_drv_release,
	.ioctl = proc4430_drv_ioctl,
	.mmap = proc4430_drv_mmap,
};

static int proc4430_drv_open(struct inode *inode, struct file *filp)
{
	s32 retval = 0;
	struct proc4430_process_context *pr_ctxt = NULL;

	pr_ctxt = kzalloc(sizeof(struct proc4430_process_context), GFP_KERNEL);
	if (!pr_ctxt)
		retval = -ENOMEM;

	filp->private_data = pr_ctxt;
	return retval;
}

static int proc4430_drv_release(struct inode *inode, struct file *filp)
{
	s32 retval = 0;
	struct proc4430_process_context *pr_ctxt;

	if (!filp->private_data) {
		retval = -EIO;
		goto err;
	}

	pr_ctxt = filp->private_data;
	while (pr_ctxt->setup_count-- > 0) {
		/* Destroy has not been called.  Call destroy now. */
		proc4430_destroy();
	}
	kfree(pr_ctxt);

	filp->private_data = NULL;
err:
	return retval;
}


/*
  Linux driver function to invoke the APIs through ioctl.
 *
 */
static int proc4430_drv_ioctl(struct inode *inode, struct file *filp,
		unsigned int cmd, unsigned long args)
{
	int retval = 0;
	struct proc_mgr_cmd_args *cmd_args = (struct proc_mgr_cmd_args *)args;
	struct proc_mgr_cmd_args command_args;
	struct proc4430_process_context *pr_ctxt =
		(struct proc4430_process_context *)filp->private_data;

	switch (cmd) {
	case CMD_PROC4430_GETCONFIG:
	{
		struct proc4430_cmd_args_get_config *src_args =
			(struct proc4430_cmd_args_get_config *)args;
		struct proc4430_config cfg;

		/* copy_from_useris not needed for
		* proc4430_get_config, since the
		* user's config is not used.
		*/
		proc4430_get_config(&cfg);

		retval = copy_to_user((void __user *)(src_args->cfg),
				(const void *)&cfg,
				sizeof(struct proc4430_config));
		if (WARN_ON(retval < 0))
			goto func_exit;
	}
	break;

	case CMD_PROC4430_SETUP:
	{
		struct proc4430_cmd_args_setup *src_args =
			(struct proc4430_cmd_args_setup *)args;
		struct proc4430_config cfg;

		retval = copy_from_user((void *)&cfg,
			(const void __user *)(src_args->cfg),
			sizeof(struct proc4430_config));
		if (WARN_ON(retval < 0))
			goto func_exit;
		proc4430_setup(&cfg);
		pr_ctxt->setup_count++;
	}
	break;

	case CMD_PROC4430_DESTROY:
	{
		proc4430_destroy();
		pr_ctxt->setup_count--;
	}
	break;

	case CMD_PROC4430_PARAMS_INIT:
	{
		struct proc4430_cmd_args_params_init src_args;
		struct proc4430_params params;

		/* Copy the full args from user-side. */
		retval = copy_from_user((void *)&src_args,
			(const void __user *)(args),
			sizeof(struct proc4430_cmd_args_params_init));
		if (WARN_ON(retval < 0))
			goto func_exit;
		proc4430_params_init(src_args.handle, &params);
		retval = copy_to_user((void __user *)(src_args.params),
			(const void *) &params,
			sizeof(struct proc4430_params));
		if (WARN_ON(retval < 0))
			goto func_exit;
	}
	break;

	case CMD_PROC4430_CREATE:
	{
		struct proc4430_cmd_args_create   src_args;
		struct proc4430_params params;
		struct proc4430_mem_entry *entries = NULL;

		/* Copy the full args from user-side. */
		retval = copy_from_user((void *)&src_args,
				(const void __user *)(args),
				sizeof(struct proc4430_cmd_args_create));
		if (WARN_ON(retval < 0))
			goto func_exit;
		retval = copy_from_user((void *) &params,
			(const void __user *)(src_args.params),
			sizeof(struct proc4430_params));
		if (WARN_ON(retval < 0))
			goto func_exit;
		/* Copy the contents of mem_entries from user-side */
		if (params.num_mem_entries) {
			entries = vmalloc(params.num_mem_entries * \
				sizeof(struct proc4430_mem_entry));
			if (WARN_ON(!entries))
				goto func_exit;
			retval = copy_from_user((void *) (entries),
				(const void __user *)(params.mem_entries),
				params.num_mem_entries * \
				sizeof(struct proc4430_mem_entry));
			if (WARN_ON(retval < 0)) {
				vfree(entries);
				goto func_exit;
			}
			params.mem_entries = entries;
		}
		src_args.handle = proc4430_create(src_args.proc_id,
							&params);
		if (WARN_ON(src_args.handle == NULL))
			goto func_exit;
		retval = copy_to_user((void __user *)(args),
				(const void *)&src_args,
				sizeof(struct proc4430_cmd_args_create));
		/* Free the memory created */
		if (params.num_mem_entries)
			vfree(entries);
	}
	break;

	case CMD_PROC4430_DELETE:
	{
		struct proc4430_cmd_args_delete src_args;

		/* Copy the full args from user-side. */
		retval = copy_from_user((void *)&src_args,
			(const void __user *)(args),
			sizeof(struct proc4430_cmd_args_delete));
		if (WARN_ON(retval < 0))
			goto func_exit;
		retval = proc4430_delete(&(src_args.handle));
		WARN_ON(retval < 0);
	}
	break;

	case CMD_PROC4430_OPEN:
	{
		struct proc4430_cmd_args_open src_args;

		/*Copy the full args from user-side. */
		retval = copy_from_user((void *)&src_args,
				(const void __user *)(args),
				sizeof(struct proc4430_cmd_args_open));
		if (WARN_ON(retval < 0))
			goto func_exit;
		retval = proc4430_open(&(src_args.handle),
					src_args.proc_id);
		retval = copy_to_user((void __user *)(args),
				(const void *)&src_args,
			sizeof(struct proc4430_cmd_args_open));
		WARN_ON(retval < 0);
	}
	break;

	case CMD_PROC4430_CLOSE:
	{
		struct proc4430_cmd_args_close src_args;

		/*Copy the full args from user-side. */
		retval = copy_from_user((void *)&src_args,
			(const void __user *)(args),
			sizeof(struct proc4430_cmd_args_close));
		if (WARN_ON(retval < 0))
			goto func_exit;
		retval = proc4430_close(src_args.handle);
		WARN_ON(retval < 0);
	}
	break;

	default:
	{
		pr_err("unsupported ioctl\n");
	}
	break;
	}
func_exit:
	/* Set the status and copy the common args to user-side. */
	command_args.api_status = retval;
	retval = copy_to_user((void __user *) cmd_args,
		(const void *) &command_args,
		 sizeof(struct proc_mgr_cmd_args));
	WARN_ON(retval < 0);
	return retval;
}


/*
  Linux driver function to map memory regions to user space.
 *
 */
static int proc4430_drv_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);

	if (remap_pfn_range(vma,
			 vma->vm_start,
			 vma->vm_pgoff,
			 vma->vm_end - vma->vm_start,
			 vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}


/** ==================================================================
 *  Functions required for multiple .ko modules configuration
 *  ==================================================================
 */
/*
  Module initialization  function for Linux driver.
 */
static int __init proc4430_drv_initializeModule(void)
{
	dev_t dev = 0 ;
	int retval;

	/* Display the version info and created date/time */
	pr_info("proc4430_drv_initializeModule\n");

	if (driver_major) {
		dev = MKDEV(driver_major, driver_minor);
		retval = register_chrdev_region(dev, 1, driver_name);
	} else {
		retval = alloc_chrdev_region(&dev, driver_minor, 1,
				 driver_name);
		driver_major = MAJOR(dev);
	}

	proc_4430_device = kmalloc(sizeof(struct proc_4430_dev), GFP_KERNEL);
	if (!proc_4430_device) {
		retval = -ENOMEM;
		unregister_chrdev_region(dev, 1);
		goto exit;
	}
	memset(proc_4430_device, 0, sizeof(struct proc_4430_dev));
	cdev_init(&proc_4430_device->cdev, &proc_4430_fops);
	proc_4430_device->cdev.owner = THIS_MODULE;
	proc_4430_device->cdev.ops = &proc_4430_fops;

	retval = cdev_add(&proc_4430_device->cdev, dev, 1);

	if (retval) {
		pr_err("Failed to add the syslink proc_4430 device\n");
		goto exit;
	}

	/* udev support */
	proc_4430_class = class_create(THIS_MODULE, "syslink-proc4430");

	if (IS_ERR(proc_4430_class)) {
		pr_err("Error creating bridge class\n");
		goto exit;
	}
	device_create(proc_4430_class, NULL, MKDEV(driver_major, driver_minor),
			NULL, PROC4430_NAME);
exit:
	return 0;
}

/*
 function to finalize the driver module.
 */
static void __exit proc4430_drv_finalizeModule(void)
{
	dev_t devno = 0;

	/* FIX ME: THIS MIGHT NOT BE THE RIGHT PLACE TO CALL THE SETUP */
	proc4430_destroy();

	devno = MKDEV(driver_major, driver_minor);
	if (proc_4430_device) {
		cdev_del(&proc_4430_device->cdev);
		kfree(proc_4430_device);
	}
	unregister_chrdev_region(devno, 1);
	if (proc_4430_class) {
		/* remove the device from sysfs */
		device_destroy(proc_4430_class, MKDEV(driver_major,
						driver_minor));
		class_destroy(proc_4430_class);
	}
	return;
}

/*
  Macro calls that indicate initialization and finalization functions
 *		  to the kernel.
 */
MODULE_LICENSE("GPL v2");
module_init(proc4430_drv_initializeModule);
module_exit(proc4430_drv_finalizeModule);
