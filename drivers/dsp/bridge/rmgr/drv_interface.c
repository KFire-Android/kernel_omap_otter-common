/*
 * drv_interface.c
 *
 * DSP-BIOS Bridge driver support functions for TI OMAP processors.
 *
 * DSP/BIOS Bridge driver interface.
 *
 * Copyright (C) 2005-2006 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*  ----------------------------------- Host OS */

#include <dspbridge/host_os.h>
#include <linux/platform_device.h>
#include <linux/pm.h>

#ifdef MODULE
#include <linux/module.h>
#endif

#include <linux/device.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>

/*  ----------------------------------- DSP/BIOS Bridge */
#include <dspbridge/std.h>
#include <dspbridge/dbdefs.h>

/*  ----------------------------------- Trace & Debug */
#include <dspbridge/dbc.h>

/*  ----------------------------------- OS Adaptation Layer */
#include <dspbridge/services.h>
#include <dspbridge/sync.h>

/*  ----------------------------------- Platform Manager */
#include <dspbridge/wcdioctl.h>
#include <dspbridge/_dcd.h>
#include <dspbridge/dspdrv.h>
#include <_tiomap.h>
#ifdef CONFIG_BRIDGE_WDT3
#include <dspbridge/clk.h>
#include <dspbridge/io_sm.h>
#endif

/*  ----------------------------------- Resource Manager */
#include <dspbridge/pwr.h>

/*  ----------------------------------- This */
#include <drv_interface.h>

#include <dspbridge/cfg.h>
#include <dspbridge/resourcecleanup.h>
#include <dspbridge/chnl.h>
#include <dspbridge/proc.h>
#include <dspbridge/dev.h>
#include <dspbridge/drvdefs.h>
#include <dspbridge/drv.h>
#include <dspbridge/brddefs.h>

#ifdef CONFIG_BRIDGE_DVFS
#include <linux/cpufreq.h>
#endif

#define BRIDGE_NAME "C6410"
/*  ----------------------------------- Globals */
#define DRIVER_NAME  "DspBridge"
#define DSPBRIDGE_VERSION	"0.2"
s32 dsp_debug;

struct platform_device *omap_dspbridge_dev;
struct device *bridge;

/* This is a test variable used by Bridge to test different sleep states */
s32 dsp_test_sleepstate;

static struct cdev bridge_cdev;

static struct class *bridge_class;

static u32 driver_context;
static s32 driver_major;
static char *base_img;
char *iva_img;
static s32 shm_size = 0x500000;	/* 5 MB */
static int tc_wordswapon;	/* Default value is always false */
#ifdef CONFIG_BRIDGE_RECOVERY
#define REC_TIMEOUT 5000       /* recovery timeout in msecs */
static atomic_t bridge_cref;	/* number of bridge open handles */
static struct workqueue_struct *bridge_rec_queue;
static struct work_struct bridge_recovery_work;
static DECLARE_COMPLETION(bridge_comp);
static DECLARE_COMPLETION(bridge_open_comp);
static bool recover;
#endif

#ifdef CONFIG_PM
struct omap34_xx_bridge_suspend_data {
	int suspended;
	wait_queue_head_t suspend_wq;
};

static struct omap34_xx_bridge_suspend_data bridge_suspend_data;

static void bridge_create_sysfs(void);
static void bridge_destroy_sysfs(void);

static int omap34_xxbridge_suspend_lockout(struct omap34_xx_bridge_suspend_data
					   *s, struct file *f)
{
	if ((s)->suspended) {
		if ((f)->f_flags & O_NONBLOCK)
			return -EPERM;
		wait_event_interruptible((s)->suspend_wq, (s)->suspended == 0);
	}
	return 0;
}
#endif

module_param(dsp_debug, int, 0);
MODULE_PARM_DESC(dsp_debug, "Wait after loading DSP image. default = false");

module_param(dsp_test_sleepstate, int, 0);
MODULE_PARM_DESC(dsp_test_sleepstate, "DSP Sleep state = 0");

module_param(base_img, charp, 0);
MODULE_PARM_DESC(base_img, "DSP base image, default = NULL");

module_param(shm_size, int, 0);
MODULE_PARM_DESC(shm_size, "shm size, default = 4 MB, minimum = 64 KB");

module_param(tc_wordswapon, int, 0);
MODULE_PARM_DESC(tc_wordswapon, "TC Word Swap Option. default = 0");

MODULE_AUTHOR("Texas Instruments");
MODULE_LICENSE("GPL");
MODULE_VERSION(DSPBRIDGE_VERSION);

static char *driver_name = DRIVER_NAME;

static const struct file_operations bridge_fops = {
	.open = bridge_open,
	.release = bridge_release,
	.unlocked_ioctl = bridge_ioctl,
	.mmap = bridge_mmap,
};

#ifdef CONFIG_PM
static u32 time_out = 1000;
#ifdef CONFIG_BRIDGE_DVFS
static struct clk *clk_handle;
#endif
#endif

struct dspbridge_platform_data *omap_dspbridge_pdata;

#ifdef CONFIG_BRIDGE_RECOVERY
static void bridge_recover(struct work_struct *work)
{
	struct dev_object *dev;
	struct cfg_devnode *dev_node;
	if (atomic_read(&bridge_cref)) {
		INIT_COMPLETION(bridge_comp);
		while (!wait_for_completion_timeout(&bridge_comp,
						msecs_to_jiffies(REC_TIMEOUT)))
			pr_info("%s:%d handle(s) still opened\n",
					__func__, atomic_read(&bridge_cref));
	}
	dev = dev_get_first();
	dev_get_dev_node(dev, &dev_node);
	if (!dev_node || DSP_FAILED(proc_auto_start(dev_node, dev)))
		pr_err("DSP could not be restarted\n");
	recover = false;
	complete_all(&bridge_open_comp);
}

void bridge_recover_schedule(void)
{
	INIT_COMPLETION(bridge_open_comp);
	recover = true;
	queue_work(bridge_rec_queue, &bridge_recovery_work);
}
#endif

#ifdef CONFIG_BRIDGE_DVFS
static int dspbridge_scale_notification(struct notifier_block *op,
					unsigned long val, void *ptr)
{
	struct dspbridge_platform_data *pdata =
	    omap_dspbridge_dev->dev.platform_data;
	if (CPUFREQ_POSTCHANGE == val && pdata->dsp_get_opp)
		pwr_pm_post_scale(PRCM_VDD1, pdata->dsp_get_opp());
	return 0;
}

static struct notifier_block iva_clk_notifier = {
	.notifier_call = dspbridge_scale_notification,
	NULL,
};
#endif

/**
 * omap3_bridge_startup() - perform low lever initializations
 * @pdev:      pointer to platform device
 *
 * Initializes recovery, PM and DVFS required data, before calling
 * clk and memory init routines.
 */
static int omap3_bridge_startup(struct platform_device *pdev)
{
	struct dspbridge_platform_data *pdata = pdev->dev.platform_data;
	struct drv_data *drv_datap = NULL;
	u32 phys_membase, phys_memsize;
	int err;

#ifdef CONFIG_BRIDGE_RECOVERY
	bridge_rec_queue = create_workqueue("bridge_rec_queue");
	INIT_WORK(&bridge_recovery_work, bridge_recover);
	INIT_COMPLETION(bridge_comp);
#endif

#ifdef CONFIG_PM
	/* Initialize the wait queue */
	bridge_suspend_data.suspended = 0;
	init_waitqueue_head(&bridge_suspend_data.suspend_wq);

#ifdef CONFIG_BRIDGE_DVFS
	clk_handle = clk_get(NULL, "iva2_ck");
	if (!clk_handle) {
		pr_err("%s: clk_get failed to get iva2_ck\n", __func__);
		return -EPERM;
	}

	if (cpufreq_register_notifier(&iva_clk_notifier,
				      CPUFREQ_TRANSITION_NOTIFIER)) {
		pr_err("%s: cpufreq_register_notifier failed for "
		       "iva2_ck\n", __func__);
		return -EPERM;
	}
#endif
#endif

	services_init();

	drv_datap = kzalloc(sizeof(struct drv_data), GFP_KERNEL);
	if (!drv_datap) {
		err = -ENOMEM;
		goto err1;
	}

	drv_datap->shm_size = shm_size;
	drv_datap->tc_wordswapon = tc_wordswapon;

	if (base_img) {
		drv_datap->base_img = kmalloc(strlen(base_img) + 1, GFP_KERNEL);
		if (!drv_datap->base_img) {
			err = -ENOMEM;
			goto err2;
		}
		strncpy(drv_datap->base_img, base_img, strlen(base_img) + 1);
	}

	dev_set_drvdata(bridge, drv_datap);

	if (shm_size < 0x10000) {	/* 64 KB */
		err = -EINVAL;
		pr_err("%s: shm size must be at least 64 KB\n", __func__);
		goto err3;
	}
	dev_dbg(bridge, "%s: requested shm_size = 0x%x\n", __func__, shm_size);

	phys_membase = pdata->phys_mempool_base;
	phys_memsize = pdata->phys_mempool_size;
	if (phys_membase > 0 && phys_memsize > 0)
		mem_ext_phys_pool_init(phys_membase, phys_memsize);

	if (tc_wordswapon)
		dev_dbg(bridge, "%s: TC Word Swap is enabled\n", __func__);

	driver_context = dsp_init(&err);
	if (err) {
		pr_err("DSP Bridge driver initialization failed\n");
		goto err4;
	}

	return 0;

err4:
	mem_ext_phys_pool_release();
err3:
	kfree(drv_datap->base_img);
err2:
	kfree(drv_datap);
err1:
#ifdef CONFIG_BRIDGE_DVFS
	cpufreq_unregister_notifier(&iva_clk_notifier,
					CPUFREQ_TRANSITION_NOTIFIER);
#endif
	services_exit();

	return err;
}

static int __devinit omap34_xx_bridge_probe(struct platform_device *pdev)
{
	int status;
	dev_t dev = 0;

	omap_dspbridge_dev = pdev;

	/* Global bridge device */
	bridge = &omap_dspbridge_dev->dev;

	/* Bridge low level initializations */
	status = omap3_bridge_startup(pdev);
	if (status)
		goto err1;

	/* use 2.6 device model */
	status = alloc_chrdev_region(&dev, 0, 1, driver_name);
	if (status < 0) {
		pr_err("%s: Can't get major %d\n", __func__, driver_major);
		goto err1;
	}

	driver_major = MAJOR(dev);

	cdev_init(&bridge_cdev, &bridge_fops);
	bridge_cdev.owner = THIS_MODULE;

	status = cdev_add(&bridge_cdev, dev, 1);
	if (status) {
		pr_err("%s: Failed to add bridge device\n", __func__);
		goto err2;
	}

	/* udev support */
	bridge_class = class_create(THIS_MODULE, "ti_bridge");

	if (IS_ERR(bridge_class))
		pr_err("%s: Error creating bridge class\n", __func__);

	bridge_create_sysfs();

	DBC_ASSERT(status == 0);

	device_create(bridge_class, NULL, MKDEV(driver_major, 0),
			NULL, "DspBridge");

	return 0;

err2:
	unregister_chrdev_region(dev, 1);
err1:
	return status;
}

static int __devexit omap34_xx_bridge_remove(struct platform_device *pdev)
{
	dev_t devno;
	bool ret;
	int status = 0;
	bhandle hdrv_obj = NULL;

	status = cfg_get_object((u32 *) &hdrv_obj, REG_DRV_OBJECT);
	if (DSP_FAILED(status))
		goto func_cont;

#ifdef CONFIG_BRIDGE_DVFS
	if (cpufreq_unregister_notifier(&iva_clk_notifier,
					CPUFREQ_TRANSITION_NOTIFIER))
		pr_err("%s: clk_notifier_unregister failed for iva2_ck\n",
		       __func__);
#endif /* #ifdef CONFIG_BRIDGE_DVFS */

	if (driver_context) {
		/* Put the DSP in reset state */
		ret = dsp_deinit(driver_context);
		driver_context = 0;
		DBC_ASSERT(ret == true);
	}
#ifdef CONFIG_BRIDGE_DVFS
	clk_put(clk_handle);
	clk_handle = NULL;
#endif /* #ifdef CONFIG_BRIDGE_DVFS */

func_cont:
	mem_ext_phys_pool_release();

	services_exit();

	bridge_destroy_sysfs();

	devno = MKDEV(driver_major, 0);
	cdev_del(&bridge_cdev);
	unregister_chrdev_region(devno, 1);
	if (bridge_class) {
		/* remove the device from sysfs */
		device_destroy(bridge_class, MKDEV(driver_major, 0));
		class_destroy(bridge_class);

	}
	return 0;
}

#ifdef CONFIG_PM
static int BRIDGE_SUSPEND(struct platform_device *pdev, pm_message_t state)
{
	u32 status;
	u32 command = PWR_EMERGENCYDEEPSLEEP;

	status = pwr_sleep_dsp(command, time_out);
	if (DSP_FAILED(status))
		return -1;

	bridge_suspend_data.suspended = 1;
	return 0;
}

static int BRIDGE_RESUME(struct platform_device *pdev)
{
	u32 status = 0;
	struct wmd_dev_context *dev_ctxt;

	dev_get_wmd_context(dev_get_first(), &dev_ctxt);
	if (!dev_ctxt)
		return -EFAULT;

	/*
	 * only wake up the DSP if it was not in Hibernation before the
	 * suspend transition
	 */
	if (dev_ctxt->dw_brd_state != BRD_DSP_HIBERNATION)
		status = pwr_wake_dsp(time_out);

	if (DSP_FAILED(status))
		return status;

	bridge_suspend_data.suspended = 0;
	wake_up(&bridge_suspend_data.suspend_wq);
	return 0;
}
#else
#define BRIDGE_SUSPEND NULL
#define BRIDGE_RESUME NULL
#endif

static struct platform_driver bridge_driver = {
	.driver = {
		   .name = BRIDGE_NAME,
		   },
	.probe = omap34_xx_bridge_probe,
	.remove = __devexit_p(omap34_xx_bridge_remove),
	.suspend = BRIDGE_SUSPEND,
	.resume = BRIDGE_RESUME,
};

static int __init bridge_init(void)
{
	return platform_driver_register(&bridge_driver);
}

static void __exit bridge_exit(void)
{
	platform_driver_unregister(&bridge_driver);
}

/*
 * This function is called when an application opens handle to the
 * bridge driver.
 */
static int bridge_open(struct inode *ip, struct file *filp)
{
	int status = 0;
	struct process_context *pr_ctxt = NULL;

	/*
	 * Allocate a new process context and insert it into global
	 * process context list.
	 */

#ifdef CONFIG_BRIDGE_RECOVERY
	if (recover) {
		if (filp->f_flags & O_NONBLOCK ||
			wait_for_completion_killable(&bridge_open_comp))
			return -EBUSY;
	}
#endif

	pr_ctxt = kzalloc(sizeof(struct process_context), GFP_KERNEL);
	if (pr_ctxt) {
		pr_ctxt->res_state = PROC_RES_ALLOCATED;
		spin_lock_init(&pr_ctxt->dmm_map_lock);
		INIT_LIST_HEAD(&pr_ctxt->dmm_map_list);
		spin_lock_init(&pr_ctxt->dmm_rsv_lock);
		INIT_LIST_HEAD(&pr_ctxt->dmm_rsv_list);

		pr_ctxt->node_idp = kzalloc(sizeof(struct idr), GFP_KERNEL);
		if (pr_ctxt->node_idp) {
			idr_init(pr_ctxt->node_idp);
		} else {
			status = -ENOMEM;
			goto err;
		}

		pr_ctxt->strm_idp = kzalloc(sizeof(struct idr), GFP_KERNEL);
		if (pr_ctxt->strm_idp)
			idr_init(pr_ctxt->strm_idp);
		else
			status = -ENOMEM;
	} else {
		status = -ENOMEM;
	}
err:
	filp->private_data = pr_ctxt;

#ifdef CONFIG_BRIDGE_RECOVERY
	if (!status)
		atomic_inc(&bridge_cref);
#endif

	return status;
}

/*
 * This function is called when an application closes handle to the bridge
 * driver.
 */
static int bridge_release(struct inode *ip, struct file *filp)
{
	int status = 0;
	struct process_context *pr_ctxt;

	if (!filp->private_data) {
		status = -EIO;
		goto err;
	}

	pr_ctxt = filp->private_data;
	flush_signals(current);
	drv_remove_all_resources(pr_ctxt);
	proc_detach(pr_ctxt);
	kfree(pr_ctxt);

	filp->private_data = NULL;

err:
#ifdef CONFIG_BRIDGE_RECOVERY
	if (!atomic_dec_return(&bridge_cref))
		complete(&bridge_comp);
#endif
	return status;
}

/* This function provides IO interface to the bridge driver. */
static long bridge_ioctl(struct file *filp, unsigned int code,
			 unsigned long args)
{
	int status;
	u32 retval = 0;
	union Trapped_Args buf_in;

	DBC_REQUIRE(filp != NULL);
#ifdef CONFIG_BRIDGE_RECOVERY
	if (recover) {
		status = -EIO;
		goto err;
	}
#endif

#ifdef CONFIG_PM
	status = omap34_xxbridge_suspend_lockout(&bridge_suspend_data, filp);
	if (status != 0)
		return status;
#endif

	if (!filp->private_data) {
		status = -EIO;
		goto err;
	}

	status = copy_from_user(&buf_in, (union Trapped_Args *)args,
				sizeof(union Trapped_Args));

	if (!status) {
		status = wcd_call_dev_io_ctl(code, &buf_in, &retval,
					     filp->private_data);

		if (DSP_SUCCEEDED(status)) {
			status = retval;
		} else {
			dev_dbg(bridge, "%s: IOCTL Failed, code: 0x%x "
				"status %i\n", __func__, code, status);
			status = -1;
		}

	}

err:
	return status;
}

/* This function maps kernel space memory to user space memory. */
static int bridge_mmap(struct file *filp, struct vm_area_struct *vma)
{
	u32 offset = vma->vm_pgoff << PAGE_SHIFT;
	u32 status;

	DBC_ASSERT(vma->vm_start < vma->vm_end);

	vma->vm_flags |= VM_RESERVED | VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	dev_dbg(bridge, "%s: vm filp %p offset %x start %lx end %lx page_prot "
		"%lx flags %lx\n", __func__, filp, offset,
		vma->vm_start, vma->vm_end, vma->vm_page_prot, vma->vm_flags);

	status = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
				 vma->vm_end - vma->vm_start,
				 vma->vm_page_prot);
	if (status != 0)
		status = -EAGAIN;

	return status;
}

/* To remove all process resources before removing the process from the
 * process context list */
int drv_remove_all_resources(bhandle hPCtxt)
{
	int status = 0;
	struct process_context *ctxt = (struct process_context *)hPCtxt;
	drv_remove_all_strm_res_elements(ctxt);
	drv_remove_all_node_res_elements(ctxt);
	drv_remove_all_dmm_res_elements(ctxt);
	ctxt->res_state = PROC_RES_FREED;
	return status;
}

#ifdef CONFIG_BRIDGE_WDT3
static ssize_t wdt3_show(struct device *dev, struct device_attribute *attr,
			 char *buf)
{
	return sprintf(buf, "%d\n", (dsp_wdt_get_enable())? 1 : 0);
}

static ssize_t wdt3_store(struct device *dev, struct device_attribute *attr,
			  const char *buf, size_t n)
{
	u32 wdt3;
	struct dev_object *dev_object;
	struct wmd_dev_context *dev_ctxt;

	if (sscanf(buf, "%d", &wdt3) != 1)
		return -EINVAL;

	dev_object = dev_get_first();
	if (dev_object == NULL)
		goto func_end;
	dev_get_wmd_context(dev_object, &dev_ctxt);
	if (dev_ctxt == NULL)
		goto func_end;

	/* enable WDT */
	if (wdt3 == 1) {
		if (dsp_wdt_get_enable())
			goto func_end;
		dsp_wdt_set_enable(true);
		if (!clk_get_use_cnt(SERVICESCLK_WDT3_FCK) &&
		    dev_ctxt->dw_brd_state != BRD_DSP_HIBERNATION)
			dsp_wdt_enable(true);
	} else if (wdt3 == 0) {
		if (!dsp_wdt_get_enable())
			goto func_end;
		if (clk_get_use_cnt(SERVICESCLK_WDT3_FCK))
			dsp_wdt_enable(false);
		dsp_wdt_set_enable(false);
	}
func_end:
	return n;
}

static DEVICE_ATTR(dsp_wdt, S_IWUSR | S_IRUGO, wdt3_show, wdt3_store);

static ssize_t wdt3_timeout_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", dsp_wdt_get_timeout());
}

static ssize_t wdt3_timeout_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t n)
{
	u32 wdt3_to;

	if (sscanf(buf, "%d", &wdt3_to) != 1)
		return -EINVAL;

	dsp_wdt_set_timeout(wdt3_to);
	return n;
}

static DEVICE_ATTR(dsp_wdt_timeout, S_IWUSR | S_IRUGO, wdt3_timeout_show,
		   wdt3_timeout_store);

#endif

/*
 * this sysfs is intended to retrieve two MPU addresses
 * needed for the INST2 utility.
 * the inst_log script will run this sysfs
 */
static ssize_t mpu_address_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct wmd_dev_context *dw_context = NULL;
	struct dev_object *hdev_obj = NULL;
	u32 mem_poolsize = 0;
	u32 GppPa = 0, DspVa = 0;
	u32 armPhyMemOffUncached = 0;
	struct dspbridge_platform_data *pdata = bridge->platform_data;
	hdev_obj = (struct dev_object *)drv_get_first_dev_object();
	dev_get_wmd_context(hdev_obj, &dw_context);
	if (!dw_context) {
		pr_err("%s: failed to get the dev context handle\n", __func__);
		return 0;
	}
	GppPa = dw_context->atlb_entry[0].ul_gpp_pa;
	DspVa = dw_context->atlb_entry[0].ul_dsp_va;

	/*
	 * the physical address offset, this offset is a
	 * fixed value for a given platform.
	 */
	armPhyMemOffUncached = GppPa - DspVa;

	/*
	 * the offset value for cached address region
	 * on DSP address space
	 */
	mem_poolsize = pdata->phys_mempool_base - 0x20000000;

	/* Retrive the above calculated addresses */
	return sprintf(buf, "mempoolsizeOffset 0x%x GppPaOffset 0x%x\n",
		       mem_poolsize, armPhyMemOffUncached);
}

static DEVICE_ATTR(mpu_address, S_IRUGO, mpu_address_show, NULL);

static struct attribute *attrs[] = {
#ifdef CONFIG_BRIDGE_WDT3
	&dev_attr_dsp_wdt.attr,
	&dev_attr_dsp_wdt_timeout.attr,
#endif
	&dev_attr_mpu_address.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static void bridge_create_sysfs(void)
{
	int error;

	error = sysfs_create_group(&omap_dspbridge_dev->dev.kobj, &attr_group);

	if (error)
		kobject_put(&omap_dspbridge_dev->dev.kobj);
}

static void bridge_destroy_sysfs(void)
{
	sysfs_remove_group(&omap_dspbridge_dev->dev.kobj, &attr_group);
}

/* Bridge driver initialization and de-initialization functions */
module_init(bridge_init);
module_exit(bridge_exit);
