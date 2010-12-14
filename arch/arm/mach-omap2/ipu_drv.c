/*
 * linux/arch/arm/mach-omap2/ipu_drv.c
 *
 * OMAP Image Processing Unit Power Management Driver
 *
 * Copyright (C) 2010 Texas Instruments
 * Paul Hunt <hunt@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <plat/ipu_dev.h>

#define IPU_CLASS_NAME "ipu-power"
#define IPU_DRIVER_NAME "omap-ipu-pm"

static struct class *omap_ipu_pm_class;
static dev_t omap_ipu_pm_dev;

int ipu_pm_first_dev;
int need_resume_notifications;

static struct proc_dir_entry *ipu_pm_proc_entry;
/* we could iterate over something much more
 * complicated than a set of lines of text
 * Just debugging.
 */
static char *lines[] = {
	"This is the first line from ipu_pm_seq",
	"This is the second line from ipu_pm_seq",
};

static int ipu_pm_num_procs = ARRAY_SIZE(lines);

static void *ipu_pm_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= ipu_pm_num_procs)
		return NULL;/* no more to read */
	return lines[*pos];
}
static void *ipu_pm_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= ipu_pm_num_procs)
		return NULL;/* no more to read */
	return lines[*pos];
}
static void ipu_pm_seq_stop(struct seq_file *s, void *v)
{
	/* nothing to do */
}
static int ipu_pm_seq_show(struct seq_file *s, void *v)
{
	seq_printf(s, "IPU_PM: %s\n", (char *) v);
	return 0;
}
static const struct seq_operations ipu_pm_seq_ops = {
	.start = ipu_pm_seq_start,
	.next = ipu_pm_seq_next,
	.stop = ipu_pm_seq_stop,
	.show = ipu_pm_seq_show,
};
static int ipu_pm_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ipu_pm_seq_ops);
}

static const struct file_operations ipu_pm_proc_ops = {
		.owner	= THIS_MODULE,
		.open	= ipu_pm_proc_open,
		.read	= seq_read,
		.llseek	= seq_lseek,
		.release = seq_release,
};

static void _suspend_stub(void)
{
	pr_info("Suspend IOCTL received\n");
}

static void _resume_stub(void)
{
	pr_info("Resume IOCTL received!\n");
}

/* arg should encode the IPU-managed HWMOD
 * to be suspended, 0 for system wide
 */
static int ipu_pm_ioctl(struct inode *inode, struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	/* struct omap_ipu_pm *ipu_pm = filp->private_data; */

	/* FIXME: check for ipu_pm when requested */

	if (_IOC_TYPE(cmd) != IPU_PM_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd)   >  IPU_PM_IOC_MAXNR)
		return -ENOTTY;

	/* FIXME:not using the next two yet, since no args */
	if (_IOC_DIR(cmd) & _IOC_READ) {
		if (!access_ok(VERIFY_WRITE,
			      (void __user *)arg,
			      _IOC_SIZE(cmd)))
			return -EFAULT;
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		if (!access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd)))
			return -EFAULT;
	}

	switch (cmd) {
	case IPU_PM_IOC_SUSPEND:
		_suspend_stub();
		break;
	case IPU_PM_IOC_RESUME:
		_resume_stub();
		break;

	default:
		return -ENOTTY;
	}

	return ret;
}


#ifdef ZERO
static const struct sysfs_ops {
	ssize_t	(*show)(struct kobject *, struct attribute *, char *);
	ssize_t	(*store)(struct kobject *, struct attribute *, const char *,
									size_t);
};
#endif

static int is_driver_init;

static const struct file_operations ipm_pm_fops = {
		.owner = THIS_MODULE,
		.ioctl = ipu_pm_ioctl,
		.open  = NULL,
};

static struct ipu_pm_dev ipu_pm_dev;

static int __devinit ipu_pm_probe(struct platform_device *pdev)
{
	/* FIXME: get pdata */
	/* struct ipu_pm_platform_data *pdata = pdev->dev.platform_data; */
	/* int id = pdev->id; */
	/* ivahd.pdev = pdev; */

	need_resume_notifications = 0;

	if (!is_driver_init) {
		is_driver_init = 1;
		/* FIXME: maybe needed for multiple dev */
		/* spin_lock_init(&ipu_pm_lock); */
	}
	/* FIXME: add kobjects to kset */

	return 0;
}

static int ipu_pm_drv_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int retval = 0;

	if (pdev->id == ipu_pm_first_dev) {
		pr_debug("%s.%d ASKED TO SUSPEND", pdev->name, pdev->id);
		/* save any local context,
		 * BIOS timers could be saved locally or on Ducati
		 */

		/* FIXME: Currently sending SUSPEND is enough to send
		 * Ducati to hibernate, save ctx can be called at this
		 * point to save ctx and reset remote procs
		 * Currently the save ctx process can be called using
		 * which ever proc_id, maybe this will change when
		 * Tesla support is added.
		 */
		/* call notification function */
		if (ipu_pm_get_handle(APP_M3)) {
			if (!(ipu_pm_get_state(APP_M3) & APP_PROC_DOWN)) {
				retval = ipu_pm_notifications(APP_M3,
							      PM_SUSPEND, NULL);
				if (retval)
					goto error;
			}
		}
		if (ipu_pm_get_handle(SYS_M3)) {
			if (!(ipu_pm_get_state(SYS_M3) & SYS_PROC_DOWN)) {
				retval = ipu_pm_notifications(SYS_M3,
							      PM_SUSPEND, NULL);
				if (retval)
					goto error;
				/* sysm3 is handling hibernation of ducati
				 * currently
				 */
				ipu_pm_save_ctx(SYS_M3);
				need_resume_notifications = 1;
			}
		}
		/* return result, should be zero if all Ducati clients
		 * returned zero else fail code
		 */
	}
error:
	return retval;
}

static int ipu_pm_drv_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	int retval = 0;

	if (pdev->id == ipu_pm_first_dev) {
		pr_debug("%s.%d ASKED TO RESUME", pdev->name, pdev->id);
		/* restore any local context,
		 * BIOS timers could be restored locally or on Ducati
		 */

		/* call our notification function */
		if (need_resume_notifications) {
			if (ipu_pm_get_handle(APP_M3)) {
				retval = ipu_pm_notifications(APP_M3,
							       PM_RESUME, NULL);
				if (retval)
					goto error;
			}
			if (ipu_pm_get_handle(SYS_M3)) {
				retval = ipu_pm_notifications(SYS_M3,
							       PM_RESUME, NULL);
				if (retval)
					goto error;
			}
			need_resume_notifications = 0;
		}

		/* return result, should be zero if all Ducati clients
		 * returned zero else fail code
		 */
	}
error:
	return retval;
}

static const struct dev_pm_ops ipu_pm_ops = {
	.suspend = ipu_pm_drv_suspend,
	.resume  = ipu_pm_drv_resume,
};

static struct platform_driver ipu_pm_driver = {
	.probe          = ipu_pm_probe,
	/*.remove			= ipu_pm_remove, */
	.driver         = {
		.name   = IPU_DRIVER_NAME,
		.owner = THIS_MODULE,
		.pm = &ipu_pm_ops,
	},
};

static int __init ipu_pm_init(void)
{
	int ret;
	struct device *tmpdev;

	if (!is_driver_init) {
		is_driver_init = 1;
		/* FIXME: maybe needed for multiple dev */
		/* spin_lock_init(&ipu_pm_lock); */
	}

	ret = alloc_chrdev_region(&omap_ipu_pm_dev, 0, 2, IPU_CLASS_NAME);
	if (ret) {
		pr_err("%s: alloc_chrdev_region failed: %d\n", __func__, ret);
		return ret;
	}

	/* create device class */
	omap_ipu_pm_class = class_create(THIS_MODULE, IPU_CLASS_NAME);
	if (IS_ERR(omap_ipu_pm_class)) {
		ret = PTR_ERR(omap_ipu_pm_class);
		pr_err("%s: class_create failed: %d\n", __func__, ret);
		return ret;
	}

	/* create an instance of this device class */
	cdev_init(&ipu_pm_dev.cdev, &ipm_pm_fops);
	ipu_pm_dev.cdev.owner = THIS_MODULE;
	ipu_pm_dev.cdev.ops = &ipm_pm_fops;
	ret = cdev_add(&ipu_pm_dev.cdev, omap_ipu_pm_dev, 1);
	if (ret)
		pr_err("%s: cdev_add failed: %d\n", __func__, omap_ipu_pm_dev);

	tmpdev = device_create(omap_ipu_pm_class, NULL,
				omap_ipu_pm_dev,
				NULL,
				"ipu%d",
				MINOR(omap_ipu_pm_dev));

	if (IS_ERR(tmpdev)) {
		ret = PTR_ERR(tmpdev);
		pr_err("%s: device_create failed: %d\n", __func__, ret);
		/* FIXME: add clean_cdev when error */
		/*goto clean_cdev;*/
	}
	dev_info(tmpdev, "Test of writing to the device message log,"
			 "done from %s\n", __func__);

	pr_info("%s initialized %s, major: %d, minor: %d\n",
			IPU_CLASS_NAME,
			/*pdata->name,*/
			"ipu",
			MAJOR(omap_ipu_pm_dev),
			MINOR(omap_ipu_pm_dev));

	/* FIXME:add a kset pointing to this new class */
	/* omap_ipu_pm_class->dev_kobj */

	/* add proc interface */
	ipu_pm_proc_entry = create_proc_entry("ipu_proc_loads", 0, NULL);
	if (ipu_pm_proc_entry)
		ipu_pm_proc_entry->proc_fops = &ipu_pm_proc_ops;
	else
		pr_err("%s: proc entry create failed for: %s\n",
						__func__, IPU_CLASS_NAME);

	return platform_driver_register(&ipu_pm_driver);
}

static void __exit ipu_pm_exit(void)
{
	platform_driver_unregister(&ipu_pm_driver);
}

/* early_platform_init("earlytimer", &ipu_pm_driver); */
module_init(ipu_pm_init);
module_exit(ipu_pm_exit);

MODULE_DESCRIPTION("OMAP IMAGE PROCESSING UNIT POWER MGMT DRIVER");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" IPU_CLASS_NAME);
MODULE_AUTHOR("Texas Instruments Inc");
