/*
 * OMAP Remote Processor driver
 *
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Written by Ohad Ben-Cohen <ohad@wizery.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/eventfd.h>

#include <plat/remoteproc.h>

#define OMAP_RPROC_NAME "omap-rproc"

static struct class *omap_rproc_class;
static dev_t omap_rproc_dev;
static atomic_t num_of_rprocs;


static void rproc_eventfd_ntfy(struct omap_rproc *obj, int event)
{
	struct omap_rproc_ntfy *fd_reg;

	spin_lock_irq(&obj->event_lock);
	list_for_each_entry(fd_reg, &obj->event_list, list)
		if (fd_reg->event == event)
			eventfd_signal(fd_reg->evt_ctx, 1);
	spin_unlock_irq(&obj->event_lock);
}

int rproc_start(struct omap_rproc *rproc, const void __user *arg)
{
	int ret;
	struct omap_rproc_platform_data *pdata;
	struct omap_rproc_start_args start_args;

	start_args.start_addr = 0;

	if (!rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;
	if (!pdata->ops)
		return -EINVAL;

#if 0
	if (copy_from_user(&start_args, arg, sizeof(start_args)))
		return -EFAULT;
#endif
	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret)
		return ret;

	ret = pdata->ops->start(rproc->dev, start_args.start_addr);

	if (!ret) {
		omap_rproc_notify_event(rproc, OMAP_RPROC_START, NULL);
		rproc_eventfd_ntfy(rproc, PROC_START);
	}

	mutex_unlock(&rproc->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_start);

int rproc_stop(struct omap_rproc *rproc)
{
	int ret;
	struct omap_rproc_platform_data *pdata;
	if (!rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;
	if (!pdata->ops)
		return -EINVAL;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret)
		return ret;

	ret = pdata->ops->stop(rproc->dev);

	if (!ret) {
		omap_rproc_notify_event(rproc, OMAP_RPROC_STOP, NULL);
		rproc_eventfd_ntfy(rproc, PROC_STOP);
	}

	mutex_unlock(&rproc->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_stop);

int rproc_sleep(struct omap_rproc *rproc)
{
	int ret;
	struct omap_rproc_platform_data *pdata;
	if (!rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;
	if (!pdata->ops)
		return -EINVAL;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret)
		return ret;

	ret = pdata->ops->sleep(rproc->dev);

	mutex_unlock(&rproc->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_sleep);

int rproc_wakeup(struct omap_rproc *rproc)
{
	int ret;
	struct omap_rproc_platform_data *pdata;

	if (!rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;
	if (!pdata->ops)
		return -EINVAL;

	ret = mutex_lock_interruptible(&rproc->lock);
	if (ret)
		return ret;

	ret = pdata->ops->wakeup(rproc->dev);

	mutex_unlock(&rproc->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_wakeup);

static inline int rproc_get_state(struct omap_rproc *rproc)
{
	struct omap_rproc_platform_data *pdata;
	if (!rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;
	if (!pdata->ops)
		return -EINVAL;

	return pdata->ops->get_state(rproc->dev);
}

static int rproc_reg_user_event(struct omap_rproc *rproc,
				const void __user *arg)
{
	struct omap_rproc_ntfy *fd_reg;
	int state;
	struct omap_rproc_reg_event_args args;
	int size;

	size = copy_from_user(&args, arg,
			sizeof(struct omap_rproc_reg_event_args));
	if (size)
		return -EINVAL;

	fd_reg = kzalloc(sizeof(struct omap_rproc_ntfy), GFP_KERNEL);
	if (!fd_reg)
		return -ENOMEM;

	fd_reg->fd = args.fd;
	fd_reg->evt_ctx = eventfd_ctx_fdget(args.fd);
	fd_reg->event = args.event;

	INIT_LIST_HEAD(&fd_reg->list);

	spin_lock_irq(&rproc->event_lock);
	list_add_tail(&fd_reg->list, &rproc->event_list);
	spin_unlock_irq(&rproc->event_lock);

	/*
	 * Check the state of remote proc and send the notification
	 * if it is already in the desired state.
	 */
	state = rproc_get_state(rproc);

	switch (state) {
	case OMAP_RPROC_RUNNING:
	case OMAP_RPROC_HIBERNATING:
		if (args.event == PROC_START)
			rproc_eventfd_ntfy(rproc, args.event);
		break;
	case OMAP_RPROC_STOPPED:
		if (args.event == PROC_STOP)
			rproc_eventfd_ntfy(rproc, args.event);
		break;
	}

	return 0;
}

static int rproc_unreg_user_event(struct omap_rproc *rproc,
				const void __user *arg)
{
	struct omap_rproc_ntfy *fd_reg, *temp_reg;
	struct omap_rproc_reg_event_args args;
	int size;
	int ret = -ENOENT;

	size = copy_from_user(&args, arg,
			sizeof(struct omap_rproc_reg_event_args));
	if (size)
		return -EINVAL;

	spin_lock_irq(&rproc->event_lock);
	list_for_each_entry_safe(fd_reg, temp_reg,
			&rproc->event_list, list) {
		if (fd_reg->fd == args.fd) {
			list_del(&fd_reg->list);
			kfree(fd_reg);
			ret = 0;
			break;
		}
	}
	spin_unlock_irq(&rproc->event_lock);
	return ret;
}

int omap_rproc_notify_event(struct omap_rproc *rproc, int event, void *data)
{
	return blocking_notifier_call_chain(&rproc->notifier, event, data);
}

int omap_rproc_register_notifier(struct omap_rproc *rproc,
						struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_register(&rproc->notifier, nb);
}
EXPORT_SYMBOL_GPL(omap_rproc_register_notifier);

int omap_rproc_unregister_notifier(struct omap_rproc *rproc,
						struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_unregister(&rproc->notifier, nb);
}
EXPORT_SYMBOL_GPL(omap_rproc_unregister_notifier);

static int omap_rproc_open(struct inode *inode, struct file *filp)
{
	unsigned int count, dev_num = iminor(inode);
	struct omap_rproc *rproc;
	struct omap_rproc_platform_data *pdata;

	rproc = container_of(inode->i_cdev, struct omap_rproc, cdev);
	if (!rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;

	count = atomic_inc_return(&rproc->count);
	dev_info(rproc->dev, "%s: dev num %d, name %s, count %d\n", __func__,
							dev_num,
							pdata->name,
							count);
	filp->private_data = rproc;

	return 0;
}

static int omap_rproc_release(struct inode *inode, struct file *filp)
{
	unsigned int count;
	struct omap_rproc_platform_data *pdata;
	struct omap_rproc *rproc = filp->private_data;
	if (!rproc || !rproc->dev)
		return -EINVAL;

	pdata = rproc->dev->platform_data;

	count = atomic_dec_return(&rproc->count);
	if (!count && (rproc->state != OMAP_RPROC_STOPPED))
		rproc_stop(rproc);

	return 0;
}

static int omap_rproc_ioctl(struct inode *inode, struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct omap_rproc *rproc = filp->private_data;

	dev_info(rproc->dev, "%s\n", __func__);

	if (!rproc)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != RPROC_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > RPROC_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case RPROC_IOCSTART:
		/* FIXME: re-visit this check to perform
		   proper permission checks */
		/* if (!capable(CAP_SYS_ADMIN))
		   return -EPERM; */
		rc = rproc_start(rproc, (const void __user *) arg);
		break;
	case RPROC_IOCSTOP:
		/* FIXME: re-visit this check to perform
		   proper permission checks */
		/* if (!capable(CAP_SYS_ADMIN))
		   return -EPERM; */
		rc = rproc_stop(rproc);
		break;
	case RPROC_IOCGETSTATE:
		rc = rproc_get_state(rproc);
		break;
	case RPROC_IOCREGEVENT:
		rc = rproc_reg_user_event(rproc, (const void __user *) arg);
		break;
	case RPROC_IOCUNREGEVENT:
		rc = rproc_unreg_user_event(rproc, (const void __user *) arg);
		break;
	default:
		return -ENOTTY;
	}

	/* First element of arg is the status */
	copy_to_user((void __user *)arg, &rc, sizeof(rc));

	return 0;
}

static int omap_rproc_mmap(struct file *filp, struct vm_area_struct *vma)
{

	vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
	vma->vm_flags |= VM_RESERVED;

	if (remap_pfn_range(vma,
			 vma->vm_start,
			 vma->vm_pgoff,
			 vma->vm_end - vma->vm_start,
			 vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;
}

static const struct file_operations omap_rproc_fops = {
	.open		=	omap_rproc_open,
	.release	=	omap_rproc_release,
	.ioctl		=	omap_rproc_ioctl,
	.mmap		=	omap_rproc_mmap,
	.owner		=	THIS_MODULE,
};

static int omap_rproc_probe(struct platform_device *pdev)
{
	int ret = 0, major, minor;
	struct device *tmpdev;
	struct device *dev = &pdev->dev;
	struct omap_rproc_platform_data *pdata = dev->platform_data;
	struct omap_rproc *rproc;

	if (!pdata || !pdata->name || !pdata->oh_name || !pdata->ops)
		return -EINVAL;

	dev_info(dev, "%s: adding rproc %s\n", __func__, pdata->name);

	rproc = kzalloc(sizeof(struct omap_rproc), GFP_KERNEL);
	if (!rproc) {
		dev_err(dev, "%s: kzalloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, rproc);
	major = MAJOR(omap_rproc_dev);
	minor = atomic_read(&num_of_rprocs);
	atomic_inc(&num_of_rprocs);

	rproc->dev = dev;
	rproc->minor = minor;
	atomic_set(&rproc->count, 0);
	rproc->name = pdata->name;

	rproc->timer_id = pdata->timer_id;

	mutex_init(&rproc->lock);
	BLOCKING_INIT_NOTIFIER_HEAD(&rproc->notifier);

	spin_lock_init(&rproc->event_lock);
	INIT_LIST_HEAD(&rproc->event_list);

	cdev_init(&rproc->cdev, &omap_rproc_fops);
	rproc->cdev.owner = THIS_MODULE;
	ret = cdev_add(&rproc->cdev, MKDEV(major, minor), 1);
	if (ret) {
		dev_err(dev, "%s: cdev_add failed: %d\n", __func__, ret);
		goto free_rproc;
	}

	tmpdev = device_create(omap_rproc_class, NULL,
				MKDEV(major, minor),
				NULL,
				OMAP_RPROC_NAME "%d", minor);
	if (IS_ERR(tmpdev)) {
		ret = PTR_ERR(tmpdev);
		dev_err(dev, "%s: device_create failed: %d\n", __func__, ret);
		goto clean_cdev;
	}

	dev_info(dev, "%s initialized %s, major: %d, base-minor: %d\n",
			OMAP_RPROC_NAME,
			pdata->name,
			MAJOR(omap_rproc_dev),
			minor);
	return 0;

clean_cdev:
	cdev_del(&rproc->cdev);
free_rproc:
	kfree(rproc);
out:
	return ret;
}

static int __devexit omap_rproc_remove(struct platform_device *pdev)
{
	int major = MAJOR(omap_rproc_dev);
	struct device *dev = &pdev->dev;
	struct omap_rproc_platform_data *pdata = dev->platform_data;
	struct omap_rproc *rproc = platform_get_drvdata(pdev);

	if (!pdata || !rproc)
		return -EINVAL;

	dev_info(dev, "%s removing %s, major: %d, base-minor: %d\n",
			OMAP_RPROC_NAME,
			pdata->name,
			major,
			rproc->minor);

	device_destroy(omap_rproc_class, MKDEV(major, rproc->minor));
	cdev_del(&rproc->cdev);

	return 0;
}

static struct platform_driver omap_rproc_driver = {
	.probe = omap_rproc_probe,
	.remove = __devexit_p(omap_rproc_remove),
	.driver = {
		.name = "omap-remoteproc",
		.owner = THIS_MODULE,
	},
};

static int device_match_by_alias(struct device *dev, void *data)
{
	struct omap_rproc *obj = (struct omap_rproc *)platform_get_drvdata(
						to_platform_device(dev));
	const char *name = data;

	pr_debug("%s: %s %s\n", __func__, obj->name, name);

	return strcmp(obj->name, name) == 0;
}

struct omap_rproc *omap_rproc_get(const char *name)
{
	struct device *dev;
	struct omap_rproc *rproc;
	dev = driver_find_device(&omap_rproc_driver.driver, NULL, (void *)name,
				 device_match_by_alias);
	if (!dev)
		return ERR_PTR(-ENODEV);
	rproc = (struct omap_rproc *)platform_get_drvdata(
						to_platform_device(dev));

	dev_dbg(rproc->dev, "%s: %s\n", __func__, rproc->name);
	return rproc;
}
EXPORT_SYMBOL_GPL(omap_rproc_get);

void omap_rproc_put(struct omap_rproc *rproc)
{
	if (!rproc || IS_ERR(rproc))
		return;

	dev_dbg(rproc->dev, "%s: %s\n", __func__, rproc->name);
}
EXPORT_SYMBOL_GPL(omap_rproc_put);

static int __init omap_rproc_init(void)
{
	int num;
	int ret;

	if (cpu_is_omap44xx())
		num = NR_RPROC_OMAP4_DEVICES;
	else
		return 0;

	ret = alloc_chrdev_region(&omap_rproc_dev, 0, num, OMAP_RPROC_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		goto out;
	}

	omap_rproc_class = class_create(THIS_MODULE, OMAP_RPROC_NAME);
	if (IS_ERR(omap_rproc_class)) {
		ret = PTR_ERR(omap_rproc_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		goto unreg_region;
	}

	atomic_set(&num_of_rprocs, 0);

	ret = platform_driver_register(&omap_rproc_driver);
	if (ret) {
		pr_err("%s: platform_driver_register failed: %d\n", __func__,
									ret);
		goto out;
	}
	return 0;

unreg_region:
	unregister_chrdev_region(omap_rproc_dev, num);
out:
	return ret;
}
module_init(omap_rproc_init);

static void __exit omap_rproc_exit(void)
{
	int num;

	if (cpu_is_omap44xx())
		num = NR_RPROC_OMAP4_DEVICES;
	else
		return;

	class_destroy(omap_rproc_class);
	unregister_chrdev_region(omap_rproc_dev, num);
}
module_exit(omap_rproc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OMAP Remote Processor driver");
MODULE_AUTHOR("Ohad Ben-Cohen <ohad@wizery.com>");
MODULE_AUTHOR("Hari Kanigeri <h-kanigeri2@ti.com>");
