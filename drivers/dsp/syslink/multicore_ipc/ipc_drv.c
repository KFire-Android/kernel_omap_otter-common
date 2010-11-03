/*
 *  ipc_drv.c
 *
 *  IPC driver module.
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/notifier.h>
#include <ipc_ioctl.h>
#include <ipc.h>
#include <drv_notify.h>
#include <notify_ioctl.h>
#include <nameserver.h>
#ifdef CONFIG_SYSLINK_RECOVERY
#include <plat/iommu.h>
#include <plat/remoteproc.h>
#endif

#define IPC_NAME		"syslink_ipc"
#define IPC_MAJOR		0
#define IPC_MINOR		0
#define IPC_DEVICES		1

#define ipc_release_resource(x, y, z) (ipc_ioc_router(x, y, z, false))

struct ipc_device {
	struct cdev cdev;

	struct blocking_notifier_head	notifier;
};

static struct ipc_device *ipc_device;
static struct class *ipc_class;

static s32 ipc_major = IPC_MAJOR;
static s32 ipc_minor = IPC_MINOR;
static char *ipc_name = IPC_NAME;

module_param(ipc_name, charp, 0);
MODULE_PARM_DESC(ipc_name, "Device name, default = syslink_ipc");

module_param(ipc_major, int, 0);	/* Driver's major number */
MODULE_PARM_DESC(ipc_major, "Major device number, default = 0 (auto)");

module_param(ipc_minor, int, 0);	/* Driver's minor number */
MODULE_PARM_DESC(ipc_minor, "Minor device number, default = 0 (auto)");

MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL v2");

#ifdef CONFIG_SYSLINK_RECOVERY
#define REC_TIMEOUT 5000       /* recovery timeout in msecs */
static atomic_t ipc_cref;	/* number of ipc open handles */
static struct workqueue_struct *ipc_rec_queue;
static struct work_struct ipc_recovery_work;
static DECLARE_COMPLETION(ipc_comp);
static DECLARE_COMPLETION(ipc_open_comp);
static bool recover;
static struct iommu *piommu;
static struct omap_rproc *prproc_sysm3;

static int ipc_ducati_iommu_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v);

static int ipc_sysm3_rproc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v);

static struct notifier_block ipc_notify_nb_iommu_ducati = {
	.notifier_call = ipc_ducati_iommu_notifier_call,
};

static struct notifier_block ipc_notify_nb_rproc_ducati0 = {
	.notifier_call = ipc_sysm3_rproc_notifier_call,
};

static void ipc_recover(struct work_struct *work)
{
	if (atomic_read(&ipc_cref)) {
		INIT_COMPLETION(ipc_comp);
		while (!wait_for_completion_timeout(&ipc_comp,
						msecs_to_jiffies(REC_TIMEOUT)))
			pr_info("%s:%d handle(s) still opened\n",
					__func__, atomic_read(&ipc_cref));
	}
	recover = false;
	complete_all(&ipc_open_comp);
}

static void ipc_recover_schedule(void)
{
	INIT_COMPLETION(ipc_open_comp);
	recover = true;
	queue_work(ipc_rec_queue, &ipc_recovery_work);
}

static int ipc_ducati_iommu_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	switch ((int)val) {
	case IOMMU_FAULT:
		ipc_recover_schedule();
		return 0;
	default:
		return 0;
	}
}

static int ipc_sysm3_rproc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	switch ((int)val) {
	case OMAP_RPROC_START:
		piommu = iommu_get("ducati");
		if (piommu != ERR_PTR(-ENODEV) &&
			piommu != ERR_PTR(-EINVAL))
				iommu_register_notifier(piommu,
						&ipc_notify_nb_iommu_ducati);
		else
			piommu = NULL;
		return 0;
	case OMAP_RPROC_STOP:
		if (piommu != NULL) {
			iommu_unregister_notifier(piommu,
						&ipc_notify_nb_iommu_ducati);
			iommu_put(piommu);
			piommu = NULL;
		}
		return 0;
	default:
		return 0;
	}
}
#endif

/*
 * ======== ipc_open ========
 *  This function is invoked when an application
 *  opens handle to the ipc driver
 */
static int ipc_open(struct inode *inode, struct file *filp)
{
	s32 retval = 0;
	struct ipc_device *dev;
	struct ipc_process_context *pr_ctxt = NULL;

#ifdef CONFIG_SYSLINK_RECOVERY
	if (recover) {
		if (filp->f_flags & O_NONBLOCK ||
			wait_for_completion_killable(&ipc_open_comp))
			return -EBUSY;
	}
#endif

	pr_ctxt = kzalloc(sizeof(struct ipc_process_context), GFP_KERNEL);
	if (!pr_ctxt)
		retval = -ENOMEM;
	else {
		INIT_LIST_HEAD(&pr_ctxt->resources);
		spin_lock_init(&pr_ctxt->res_lock);
		dev = container_of(inode->i_cdev, struct ipc_device,
					cdev);
		pr_ctxt->dev = dev;
		filp->private_data = pr_ctxt;
	}

#ifdef CONFIG_SYSLINK_RECOVERY
	if (!retval)
		atomic_inc(&ipc_cref);
#endif

	return retval;
}

/*
 * ======== ipc_release ========
 *  This function is invoked when an application
 *  closes handle to the ipc driver
 */
static int ipc_release(struct inode *inode, struct file *filp)
{
	s32 retval = 0;
	struct ipc_process_context *pr_ctxt;
	struct resource_info *info = NULL, *temp = NULL;

	if (!filp->private_data) {
		retval = -EIO;
		goto err;
	}

	ipc_notify_event(IPC_CLOSE, (void *)NULL);

	pr_ctxt = filp->private_data;

	list_for_each_entry_safe(info, temp, &pr_ctxt->resources, res) {
		retval = ipc_release_resource(info->cmd, (ulong)info->data,
					filp);
	}

	list_del(&pr_ctxt->resources);
	kfree(pr_ctxt);

	filp->private_data = NULL;
err:
#ifdef CONFIG_SYSLINK_RECOVERY
	if (!atomic_dec_return(&ipc_cref))
		complete(&ipc_comp);
#endif
	return retval;
}

/*
 * ======== ipc_ioctl ========
 *  This function  provides IO interface  to the
 *  ipc driver
 */
static int ipc_ioctl(struct inode *ip, struct file *filp, u32 cmd, ulong arg)
{
	s32 retval = 0;
	void __user *argp = (void __user *)arg;

#ifdef CONFIG_SYSLINK_RECOVERY
	if (recover) {
		if (cmd != CMD_NOTIFY_THREADDETACH) {
			retval = -EIO;
			goto exit;
		}
	}
#endif
	/* Verify the memory and ensure that it is not is kernel
	     address space
	*/
	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok(VERIFY_WRITE, argp, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		retval = !access_ok(VERIFY_READ, argp, _IOC_SIZE(cmd));

	if (retval) {
		retval = -EFAULT;
		goto exit;
	}

	retval = ipc_ioc_router(cmd, (ulong)argp, filp, true);
	return retval;

exit:
	return retval;
}

static const struct file_operations ipc_fops = {
	.open = ipc_open,
	.release = ipc_release,
	.ioctl = ipc_ioctl,
	.read = notify_drv_read,
	.mmap = notify_drv_mmap,
};

/*
 * ======== ipc_notify_event ========
 *  IPC event notifications.
 */
int ipc_notify_event(int event, void *data)
{
	return blocking_notifier_call_chain(&ipc_device->notifier, event, data);
}

/*
 * ======== ipc_register_notifier ========
 *  Register for IPC events.
 */
int ipc_register_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_register(&ipc_device->notifier, nb);
}
EXPORT_SYMBOL_GPL(ipc_register_notifier);

/*
 * ======== ipc_unregister_notifier ========
 *  Un-register for IPC events.
 */
int ipc_unregister_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_unregister(&ipc_device->notifier, nb);
}
EXPORT_SYMBOL_GPL(ipc_unregister_notifier);

/*
 * ======== ipc_modules_init ========
 *  IPC Initialization routine. will initialize various
 *  sub components (modules) of IPC.
 */
static int ipc_modules_init(void)
{
	/* Setup the notify_drv module */
	_notify_drv_setup();

#ifdef CONFIG_SYSLINK_RECOVERY
	ipc_rec_queue = create_workqueue("ipc_rec_queue");
	INIT_WORK(&ipc_recovery_work, ipc_recover);
	INIT_COMPLETION(ipc_comp);

	prproc_sysm3 = omap_rproc_get("ducati-proc0");
	if (prproc_sysm3 != ERR_PTR(-ENODEV))
		omap_rproc_register_notifier(prproc_sysm3,
						&ipc_notify_nb_rproc_ducati0);
	else
		prproc_sysm3 = NULL;
#endif

	return 0;
}

/*
 * ======== ipc_modules_exit ========
 *  IPC cleanup routine. will cleanup of various
 *  sub components (modules) of IPC.
 */
static void ipc_modules_exit(void)
{
#ifdef CONFIG_SYSLINK_RECOVERY
	if (prproc_sysm3) {
		omap_rproc_unregister_notifier(prproc_sysm3,
						&ipc_notify_nb_rproc_ducati0);
		omap_rproc_put(prproc_sysm3);
	}
	destroy_workqueue(ipc_rec_queue);
#endif
	/* Destroy the notify_drv module */
	_notify_drv_destroy();
}

/*
 * ======== ipc_init ========
 *  Initialization routine. Executed when the driver is
 *  loaded (as a kernel module), or when the system
 *  is booted (when included as part of the kernel
 *  image).
 */
static int __init ipc_init(void)
{
	dev_t dev ;
	s32 retval = 0;

	retval = alloc_chrdev_region(&dev, ipc_minor, IPC_DEVICES,
							ipc_name);
	ipc_major = MAJOR(dev);

	if (retval < 0) {
		pr_err("ipc_init: can't get major %x\n", ipc_major);
		goto exit;
	}

	ipc_device = kmalloc(sizeof(struct ipc_device), GFP_KERNEL);
	if (!ipc_device) {
		pr_err("ipc_init: memory allocation failed for ipc_device\n");
		retval = -ENOMEM;
		goto unreg_exit;
	}

	memset(ipc_device, 0, sizeof(struct ipc_device));

	BLOCKING_INIT_NOTIFIER_HEAD(&ipc_device->notifier);

	retval = ipc_modules_init();
	if (retval) {
		pr_err("ipc_init: ipc initialization failed\n");
		goto unreg_exit;

	}
	ipc_class = class_create(THIS_MODULE, "syslink_ipc");
	if (IS_ERR(ipc_class)) {
		pr_err("ipc_init: error creating ipc class\n");
		retval = PTR_ERR(ipc_class);
		goto unreg_exit;
	}

	device_create(ipc_class, NULL, MKDEV(ipc_major, ipc_minor), NULL,
								ipc_name);
	cdev_init(&ipc_device->cdev, &ipc_fops);
	ipc_device->cdev.owner = THIS_MODULE;
	retval = cdev_add(&ipc_device->cdev, dev, IPC_DEVICES);
	if (retval) {
		pr_err("ipc_init: failed to add the ipc device\n");
		goto class_exit;
	}
	return retval;

class_exit:
	class_destroy(ipc_class);

unreg_exit:
	unregister_chrdev_region(dev, IPC_DEVICES);
	kfree(ipc_device);

exit:
	return retval;
}

/*
 * ======== ipc_exit ========
 *  This function is invoked during unlinking of ipc
 *  module from the kernel. ipc resources are
 *  freed in this function.
 */
static void __exit ipc_exit(void)
{
	dev_t devno;

	ipc_modules_exit();
	devno = MKDEV(ipc_major, ipc_minor);
	if (ipc_device) {
		cdev_del(&ipc_device->cdev);
		kfree(ipc_device);
	}
	unregister_chrdev_region(devno, IPC_DEVICES);
	if (ipc_class) {
		/* remove the device from sysfs */
		device_destroy(ipc_class, MKDEV(ipc_major, ipc_minor));
		class_destroy(ipc_class);
	}
}

/*
 *  ipc driver initialization and de-initialization functions
 */
module_init(ipc_init);
module_exit(ipc_exit);
