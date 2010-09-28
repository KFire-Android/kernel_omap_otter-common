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
#include <linux/eventfd.h>

#include <plat/iommu.h>
#include <plat/iovmm.h>
#include <plat/dmm_user.h>
#include "iopgtable.h"


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
	{
		struct iotlb_entry e;
		int size;
		size = copy_from_user(&e, (void __user *)args,
					sizeof(struct iotlb_entry));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		load_iotlb_entry(obj->iovmm->iommu, &e);
		break;
	}
	case DMM_IOCCREATEPOOL:
	{
		struct iovmm_pool_info pool_info;
		int size;

		size = copy_from_user(&pool_info, (void __user *)args,
						sizeof(struct iovmm_pool_info));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		omap_create_dmm_pool(obj, pool_info.pool_id, pool_info.size,
							pool_info.da_begin);
		break;
	}
	case DMM_IOCDELETEPOOL:
	{
		int pool_id;
		int size;

		size = copy_from_user(&pool_id, (void __user *)args,
							sizeof(int));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		ret = omap_delete_dmm_pool(obj, pool_id);
		break;
	}
	case DMM_IOCMEMMAP:
	{
		struct  dmm_map_info map_info;
		int size;
		int status;

		size = copy_from_user(&map_info, (void __user *)args,
						sizeof(struct dmm_map_info));

		status = dmm_user(obj, map_info.mem_pool_id,
					map_info.da, map_info.mpu_addr,
					map_info.size, map_info.flags);
		ret = copy_to_user((void __user *)args, &map_info,
					sizeof(struct dmm_map_info));
		break;
	}
	case DMM_IOCMEMUNMAP:
	{
		u32 da;
		int size;
		int status = 0;

		size = copy_from_user(&da, (void __user *)args, sizeof(u32));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		status = user_un_map(obj, da);
		ret = status;
		break;
	}
	case IOMMU_IOCEVENTREG:
	{
		int fd;
		int size;
		struct iommu_event_ntfy *fd_reg;

		size = copy_from_user(&fd, (void __user *)args, sizeof(int));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}

		fd_reg = kzalloc(sizeof(struct iommu_event_ntfy), GFP_KERNEL);
		fd_reg->fd = fd;
		fd_reg->evt_ctx = eventfd_ctx_fdget(fd);
		INIT_LIST_HEAD(&fd_reg->list);
		spin_lock_irq(&obj->iovmm->iommu->event_lock);
		list_add_tail(&fd_reg->list, &obj->iovmm->iommu->event_list);
		spin_unlock_irq(&obj->iovmm->iommu->event_lock);
		break;
	}
	case IOMMU_IOCEVENTUNREG:
	{
		int fd;
		int size;
		struct iommu_event_ntfy *fd_reg, *temp_reg;

		size = copy_from_user(&fd, (void __user *)args, sizeof(int));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		/* Free DMM mapped memory resources */
		spin_lock_irq(&obj->iovmm->iommu->event_lock);
		list_for_each_entry_safe(fd_reg, temp_reg,
				&obj->iovmm->iommu->event_list, list) {
			if (fd_reg->fd == fd) {
				list_del(&fd_reg->list);
				kfree(fd_reg);
			}
		}
		spin_unlock_irq(&obj->iovmm->iommu->event_lock);
		break;
	}
	case DMM_IOCMEMFLUSH:
	{
		int size;
		int status;
		struct dmm_dma_info dma_info;
		size = copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		status = proc_begin_dma(obj, dma_info.pva, dma_info.ul_size,
								dma_info.dir);
		ret = status;
		break;
	}
	case DMM_IOCMEMINV:
	{
		int size;
		int status;
		struct dmm_dma_info dma_info;
		size = copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info));
		if (size) {
			ret = -EINVAL;
			goto err_user_buf;
		}
		status = proc_end_dma(obj, dma_info.pva, dma_info.ul_size,
								dma_info.dir);
		ret = status;
		break;
	}
	case DMM_IOCDATOPA:
	default:
		return -ENOTTY;
	}
err_user_buf:
	return ret;

}

static int omap_dmm_open(struct inode *inode, struct file *filp)
{
	struct iodmm_struct *iodmm;
	struct iovmm_device *obj;

	obj = container_of(inode->i_cdev, struct iovmm_device, cdev);

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
		goto err;
	}
	obj = filp->private_data;
	flush_signals(current);
	iommu_notify_event(obj->iovmm->iommu, IOMMU_CLOSE, NULL);
	user_remove_resources(obj);
	iommu_put(obj->iovmm->iommu);
	kfree(obj);
	filp->private_data = NULL;

err:
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
	iopgtable_clear_entry_all(obj->iommu);
	iommu_put(obj->iommu);
	free_pages((unsigned long)obj->iommu->iopgd,
				get_order(IOPGD_TABLE_SIZE));
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
