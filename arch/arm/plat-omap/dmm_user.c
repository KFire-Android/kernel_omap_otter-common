/*
 * OMAP DMM (Dynamic memory mapping) to IOMMU module
 *
 * Copyright (C) 2010 Texas Instruments. All rights reserved.
 *
 * Authors: Hari Kanigeri <h-kanigeri2@ti.com>
 *          Ramesh Gupta <grgupta@ti.com>
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
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/swap.h>
#include <linux/genalloc.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>

#include <plat/iommu.h>
#include <plat/iovmm.h>
#include <plat/dmm_user.h>


#define OMAP_DMM_NAME "iovmm-omap"

static atomic_t		num_of_iovmmus;
static struct class	*omap_dmm_class;
static dev_t		omap_dmm_dev;

static int omap_dmm_ioctl(struct inode *inode, struct file *filp,
				unsigned int cmd, unsigned long args)
{
	struct iodmm_struct *obj;
	int ret = 0;
	obj = (struct iodmm_struct *)filp->private_data;

	if (!obj)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != DMM_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case DMM_IOCSETTLBENT:
		/* FIXME: re-visit this check to perform
		   proper permission checks */
		/* if (!capable(CAP_SYS_ADMIN))
		   return -EPERM; */
		ret = program_tlb_entry(obj, (const void __user *)args);
		break;
	case DMM_IOCCREATEPOOL:
		/* FIXME: re-visit this check to perform
		   proper permission checks */
		/* if (!capable(CAP_SYS_ADMIN))
		   return -EPERM; */
		ret = omap_create_dmm_pool(obj, (const void __user *)args);
		break;
	case DMM_IOCMEMMAP:
		ret = dmm_user(obj, (void __user *)args);
		break;
	case DMM_IOCMEMUNMAP:
		ret = user_un_map(obj, (const void __user *)args);
		break;
	case IOMMU_IOCEVENTREG:
		ret = register_mmufault(obj, (const void __user *)args);
		break;
	case IOMMU_IOCEVENTUNREG:
		ret = register_mmufault(obj, (const void __user *)args);
		break;
	case DMM_IOCMEMFLUSH:
		ret = proc_begin_dma(obj, (void __user *)args);
		break;
	case DMM_IOCMEMINV:
		ret = proc_end_dma(obj, (void __user *)args);
		break;
	/* This ioctl can be deprecated */
	case DMM_IOCDELETEPOOL:
		break;
	case DMM_IOCDATOPA:
	default:
		return -ENOTTY;
	}

	return ret;
}

static int omap_dmm_open(struct inode *inode, struct file *filp)
{
	struct iodmm_struct *iodmm;
	struct iovmm_device *obj;

	obj = container_of(inode->i_cdev, struct iovmm_device, cdev);
	obj->refcount++;

	iodmm = kzalloc(sizeof(struct iodmm_struct), GFP_KERNEL);
	INIT_LIST_HEAD(&iodmm->map_list);

	iodmm->iovmm = obj;
	obj->iommu = iommu_get(obj->name);
	filp->private_data = iodmm;

	return 0;
}

static int omap_dmm_release(struct inode *inode, struct file *filp)
{
	int status = 0;
	struct iodmm_struct *obj;

	if (!filp->private_data) {
		status = -EIO;
		goto err_out;
	}
	obj = filp->private_data;

	flush_signals(current);

	status = mutex_lock_interruptible(&obj->iovmm->dmm_map_lock);
	if (status == 0) {
		/*
		 * Report to remote Processor of the cleanup of these
		 * resources before cleaning in order to avoid MMU fault
		 * type of behavior
		 */
		if (!list_empty(&obj->map_list)) {
			iommu_notify_event(obj->iovmm->iommu, IOMMU_CLOSE,
								NULL);
		}
		mutex_unlock(&obj->iovmm->dmm_map_lock);
	} else {
		pr_err("%s mutex_lock_interruptible returned 0x%x\n",
						__func__, status);
	}

	user_remove_resources(obj);
	iommu_put(obj->iovmm->iommu);

	/* Delete all the DMM pools after the reference count goes to zero */
	if (--obj->iovmm->refcount == 0)
		omap_delete_dmm_pools(obj);

	kfree(obj);

	filp->private_data = NULL;

err_out:
	return status;
}

static const struct file_operations omap_dmm_fops = {
	.owner		=	THIS_MODULE,
	.open		=	omap_dmm_open,
	.release	=	omap_dmm_release,
	.ioctl		=	omap_dmm_ioctl,
};

static int __devinit omap_dmm_probe(struct platform_device *pdev)
{
	int err = -ENODEV;
	int major, minor;
	struct device *tmpdev;
	struct iommu_platform_data *pdata =
			(struct iommu_platform_data *)pdev->dev.platform_data;
	int ret = 0;
	struct iovmm_device *obj;

	obj = kzalloc(sizeof(struct iovmm_device), GFP_KERNEL);

	major = MAJOR(omap_dmm_dev);
	minor = atomic_read(&num_of_iovmmus);
	atomic_inc(&num_of_iovmmus);

	obj->minor = minor;
	obj->name = pdata->name;
	INIT_LIST_HEAD(&obj->mmap_pool);

	cdev_init(&obj->cdev, &omap_dmm_fops);
	obj->cdev.owner = THIS_MODULE;
	ret = cdev_add(&obj->cdev, MKDEV(major, minor), 1);

	if (ret) {
		dev_err(&pdev->dev, "%s: cdev_add failed: %d\n", __func__, ret);
		goto err_cdev;
	}
	tmpdev = device_create(omap_dmm_class, NULL,
				MKDEV(major, minor),
				NULL,
				OMAP_DMM_NAME "%d", minor);
	if (IS_ERR(tmpdev)) {
		ret = PTR_ERR(tmpdev);
		pr_err("%s: device_create failed: %d\n", __func__, ret);
		goto clean_cdev;
	}

	pr_info("%s initialized %s, major: %d, base-minor: %d\n",
			OMAP_DMM_NAME,
			pdata->name,
			MAJOR(omap_dmm_dev),
			minor);

	mutex_init(&obj->dmm_map_lock);
	platform_set_drvdata(pdev, obj);
	return 0;

clean_cdev:
	cdev_del(&obj->cdev);
err_cdev:
	return err;
}

static int __devexit omap_dmm_remove(struct platform_device *pdev)
{
	struct iovmm_device *obj = platform_get_drvdata(pdev);
	int major = MAJOR(omap_dmm_dev);

	device_destroy(omap_dmm_class, MKDEV(major, obj->minor));
	cdev_del(&obj->cdev);
	platform_set_drvdata(pdev, NULL);
	kfree(obj);

	return 0;

}

static struct platform_driver omap_dmm_driver = {
	.probe	= omap_dmm_probe,
	.remove	= __devexit_p(omap_dmm_remove),
	.driver	= {
		.name	= "omap-iovmm",
	},
};

static int __init dmm_user_init(void)
{
	int num, ret;

	num = iommu_get_plat_data_size();

	ret = alloc_chrdev_region(&omap_dmm_dev, 0, num, OMAP_DMM_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		goto out;
	}
	omap_dmm_class = class_create(THIS_MODULE, OMAP_DMM_NAME);
	if (IS_ERR(omap_dmm_class)) {
		ret = PTR_ERR(omap_dmm_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		goto unreg_region;
	}
	atomic_set(&num_of_iovmmus, 0);

	return platform_driver_register(&omap_dmm_driver);
unreg_region:
	unregister_chrdev_region(omap_dmm_dev, num);
out:
	return ret;
}
module_init(dmm_user_init);

static void __exit dmm_user_exit(void)
{
	int num = iommu_get_plat_data_size();

	class_destroy(omap_dmm_class);
	unregister_chrdev_region(omap_dmm_dev, num);
	platform_driver_unregister(&omap_dmm_driver);
}
module_exit(dmm_user_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Userspace DMM to IOMMU");
MODULE_AUTHOR("Hari Kanigeri");
MODULE_AUTHOR("Ramesh Gupta");
