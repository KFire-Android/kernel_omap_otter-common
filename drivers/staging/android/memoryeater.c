/*
 * drivers/misc/memoryeater.c
 *
 * writing a value in /sys/..../memoryeater will attempt to allocate XMB of GFP_KERNEL RAM
 * in 1MB chunks. New values will free/alloc only the difference, and "0" frees it all.
 *
 * reading it back will report how much has been sucessfully allocated.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 * Copyright (C) 2011 Lab 126
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/sysfs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/* only 512MB in system, and no swap, slot 0 unused */
#define BIGGEST_ALLOC 512

static unsigned asked_for = 0;
static unsigned so_far = 0;
static int in_progress;
static char *mem_hunk[BIGGEST_ALLOC]; /* slot 0 unused */

static void gobble_mem(void)
{
	int i;
	unsigned old_so_far = so_far;

	in_progress = 1;

	if(so_far > asked_for) {
		/* free slots */
		for(i = so_far; i > asked_for; i--)
			if(mem_hunk[i]) {
				kfree(mem_hunk[i]);
				mem_hunk[i] = NULL;
			};
		so_far = asked_for;
		in_progress = 0;
		printk(KERN_INFO "%s: allocated down from %uMB to %uMB\n", __func__, old_so_far, asked_for);
		return;
	};

	for(i = so_far + 1; i <= asked_for; i++) {
		mem_hunk[i] = kmalloc(1024 * 1024, GFP_KERNEL);
		if(!mem_hunk[i]) {
			so_far = i - 1;
			printk(KERN_ERR "%s: could only allocate %uMB (total) of %uMB requested\n", __func__, so_far, asked_for);
			in_progress = 0;
			return;
		};
	};
	so_far = asked_for;
	in_progress = 0;
	printk(KERN_INFO "%s: allocated up from %uMB to %uMB\n", __func__, old_so_far, asked_for);
};

static ssize_t memeater_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "last asked %uMB last alloced %uMB\n", asked_for, so_far);
}

static ssize_t memeater_store(struct device *dev, struct device_attribute *attr, const char * buf, size_t count)
{
	unsigned long temp_val = 0;
	int ret = count;

	if (in_progress)
		return ret;

	if (strict_strtoul(buf, 0, &temp_val) < 0) {
		printk(KERN_ERR "%s: bad value: %s\n", __func__, buf);
		return -EINVAL;
	};

	if(temp_val <= BIGGEST_ALLOC)
		asked_for = temp_val;

	if(asked_for != so_far)
		gobble_mem();

	return ret;
}

static DEVICE_ATTR(howmuch, S_IRUGO | S_IWUSR, memeater_show, memeater_store);

static int memeater_probe(struct platform_device *pdev)
{
	int ret = 0;
	ret = device_create_file(&pdev->dev, &dev_attr_howmuch);
	if(ret)
		printk(KERN_ERR "%s: can't create sysfs file: %d\n", __func__, ret);
	return ret;
}

static int memeater_remove(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev, &dev_attr_howmuch);
	return 0;
}

static struct platform_device memeater_device = {
	.name = "memeater",
	.id = -1,
};

static struct platform_driver memeater_driver = {
	.probe = memeater_probe,
	.remove = memeater_remove,
	.driver = {
		.name = "memeater",
		.owner = THIS_MODULE,
	},
};

static int __init memeater_init(void)
{
	int ret;
	ret = platform_driver_register(&memeater_driver);
	if(ret)
		printk(KERN_ERR "%s: can't register platform device: %d\n", __func__, ret);
	ret = platform_device_register(&memeater_device);
	if(ret)
		printk(KERN_ERR "%s: can't register platform device: %d\n", __func__, ret);
	return ret;
}

static void __exit memeater_exit(void)
{
	platform_device_unregister(&memeater_device);
	platform_driver_unregister(&memeater_driver);
}

module_init(memeater_init);
module_exit(memeater_exit);

MODULE_LICENSE("GPL");
