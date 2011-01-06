/*
 * OMAP Device Handler driver
 *
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Written by Angela Stegmaier <angelabaker@ti.com>
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

#include "devh.h"

#define OMAP_DEVH_NAME "omap-devh"
#define DRV_NAME "omap-devicehandler"

static struct class *omap_devh_class;
static dev_t omap_devh_dev;
static atomic_t num_of_devhs;
static struct platform_driver omap_devh_driver;

static int omap_devh_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct omap_devh *devh;

	devh = container_of(inode->i_cdev, struct omap_devh, cdev);
	if (!devh->dev)
		return -EINVAL;

	filp->private_data = devh;

	return ret;
}

static int omap_devh_release(struct inode *inode, struct file *filp)
{
	struct omap_devh *devh = filp->private_data;
	if (!devh || !devh->dev)
		return -EINVAL;

	return 0;
}

static int omap_devh_ioctl(struct inode *inode, struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	struct omap_devh *devh = filp->private_data;
	struct omap_devh_platform_data *pdata =
		(struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!devh)
		return -EINVAL;

	if (_IOC_TYPE(cmd) != DEVH_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEVH_IOC_MAXNR)
		return -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (!access_ok(VERIFY_WRITE, (void __user *)arg,
				       _IOC_SIZE(cmd)))
			return -EFAULT;
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (!access_ok(VERIFY_READ, (void __user *)arg,
				       _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {

	/*
	* this will be updated to support user registering for event
	* notifications.
	*/
	case DEVH_IOCWAITONEVENTS:
		/*rc = omap_devh_wait_on_events(devh);*/
		break;
	case DEVH_IOCEVENTREG:
		rc = pdata->ops->register_event_notification(devh,
						(const void __user *)arg);
		break;
	case DEVH_IOCEVENTUNREG:
		rc = pdata->ops->unregister_event_notification(devh,
						(const void __user *)arg);
		break;
	default:
		return -ENOTTY;
	}

	return rc;
}

static const struct file_operations omap_devh_fops = {
	.open		=	omap_devh_open,
	.release	=	omap_devh_release,
	.ioctl		=	omap_devh_ioctl,
	.owner		=	THIS_MODULE,
};

static int omap_devh_probe(struct platform_device *pdev)
{
	int ret = 0, major, minor;
	struct device *tmpdev;
	struct device *dev = &pdev->dev;
	struct omap_devh_platform_data *pdata = dev->platform_data;
	struct omap_devh *devh;

	if (!pdata || !pdata->name || !pdata->ops)
		return -EINVAL;

	dev_info(dev, "%s: adding devh %s\n", __func__, pdata->name);

	devh = kzalloc(sizeof(struct omap_devh), GFP_KERNEL);
	if (!devh) {
		dev_err(dev, "%s: kzalloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	platform_set_drvdata(pdev, devh);
	major = MAJOR(omap_devh_dev);
	minor = atomic_read(&num_of_devhs);
	atomic_inc(&num_of_devhs);

	devh->dev = dev;
	devh->minor = minor;
	devh->name = pdata->name;

	cdev_init(&devh->cdev, &omap_devh_fops);
	devh->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devh->cdev, MKDEV(major, minor), 1);
	if (ret) {
		dev_err(dev, "%s: cdev_add failed: %d\n", __func__, ret);
		goto free_devh;
	}

	tmpdev = device_create(omap_devh_class, NULL,
				MKDEV(major, minor),
				NULL,
				OMAP_DEVH_NAME "%d", minor);
	if (IS_ERR(tmpdev)) {
		ret = PTR_ERR(tmpdev);
		pr_err("%s: device_create failed: %d\n", __func__, ret);
		goto clean_cdev;
	}

	pr_info("%s initialized %s, major: %d, base-minor: %d\n",
			OMAP_DEVH_NAME,
			pdata->name,
			MAJOR(omap_devh_dev),
			minor);

	INIT_LIST_HEAD(&(devh->event_list));
	spin_lock_init(&(devh->event_lock));
	if (pdata->ops->register_notifiers)
		pdata->ops->register_notifiers(devh);

	return 0;

clean_cdev:
	cdev_del(&devh->cdev);
free_devh:
	kfree(devh);
out:
	return ret;
}

static int omap_devh_remove(struct platform_device *pdev)
{
	int major = MAJOR(omap_devh_dev);
	struct device *dev = &pdev->dev;
	struct omap_devh_platform_data *pdata = dev->platform_data;
	struct omap_devh *devh = platform_get_drvdata(pdev);

	if (!pdata || !devh)
		return -EINVAL;

	if (pdata->ops->unregister_notifiers)
		pdata->ops->unregister_notifiers(devh);

	dev_info(dev, "%s removing %s, major: %d, base-minor: %d\n",
			OMAP_DEVH_NAME,
			pdata->name,
			major,
			devh->minor);

	device_destroy(omap_devh_class, MKDEV(major, devh->minor));
	cdev_del(&devh->cdev);
	kfree(devh);

	return 0;
}

static struct platform_driver omap_devh_driver = {
	.probe = omap_devh_probe,
	.remove = omap_devh_remove,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init omap_devh_init(void)
{
	int num = devh_get_plat_data_size();
	int ret;

	ret = alloc_chrdev_region(&omap_devh_dev, 0, num, OMAP_DEVH_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		goto out;
	}

	omap_devh_class = class_create(THIS_MODULE, OMAP_DEVH_NAME);
	if (IS_ERR(omap_devh_class)) {
		ret = PTR_ERR(omap_devh_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		goto unreg_region;
	}

	atomic_set(&num_of_devhs, 0);

	ret = platform_driver_register(&omap_devh_driver);
	if (ret) {
		pr_err("%s: platform_driver_register failed: %d\n",
							__func__, ret);
		goto out;
	}
	return 0;
unreg_region:
	unregister_chrdev_region(omap_devh_dev, num);
out:
	return ret;
}
module_init(omap_devh_init);

static void __exit omap_devh_exit(void)
{
	int num = devh_get_plat_data_size();
	pr_info("%s\n", __func__);
	platform_driver_unregister(&omap_devh_driver);
	class_destroy(omap_devh_class);
	unregister_chrdev_region(omap_devh_dev, num);
}
module_exit(omap_devh_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OMAP Device Handler driver");
MODULE_AUTHOR("Angela Stegmaier <angelabaker@ti.com>");
