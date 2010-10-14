/*
 * Device Handler machine-specific module for OMAP4
 *
 * Copyright (C) 2010 Texas Instruments Inc.
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
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <mach/irqs.h>
#include <plat/omap_device.h>
#include <plat/iommu.h>
#include <plat/remoteproc.h>
#if defined(CONFIG_TILER_OMAP)
#include <mach/tiler.h>
#endif

#include <syslink/ipc.h>

#include "../devh.h"
#include "../../ipu_pm/ipu_pm.h"

static struct mutex local_gate;

struct omap_devh_runtime_info {
	int brd_state;
	struct iommu *iommu;
	struct omap_rproc *rproc;
	pid_t mgr_pid;
};

enum {
	DEVH_BRDST_RUNNING,
	DEVH_BRDST_STOPPED,
	DEVH_BRDST_ERROR,
};

static struct omap_devh_platform_data *devh_get_plat_data_by_name(char *name)
{
	int i, j = devh_get_plat_data_size();
	struct omap_devh_platform_data *pdata = devh_get_plat_data();

	if (name) {
		for (i = 0; i < j; i++) {
			if (!(strcmp(name, pdata[i].name)))
				return &pdata[i];
		}
	}
	return NULL;
}

static int devh44xx_notifier_call(struct notifier_block *nb,
					unsigned long val, void *v,
					struct omap_devh_platform_data *pdata)
{
	int err = 0;
	pid_t my_pid = current->tgid;
	struct omap_devh_runtime_info *pinfo = NULL;
	struct omap_devh_platform_data *pdata2 = NULL;

	if (pdata)
		pinfo = (struct omap_devh_runtime_info *)pdata->private_data;
	else
		return -EINVAL;

	if (pinfo->brd_state == DEVH_BRDST_RUNNING) {
		err = mutex_lock_interruptible(&local_gate);
		if (err)
			goto exit;
		err = ipu_pm_notifications(PM_PID_DEATH, (void *)my_pid);
		if (err) {
			pinfo->brd_state = DEVH_BRDST_ERROR;
			if (!strcmp(pdata->name, "SysM3")) {
				pdata2 = devh_get_plat_data_by_name("AppM3");
				if (pdata2) {
					pinfo =
					(struct omap_devh_runtime_info *)
							pdata2->private_data;
					pinfo->brd_state = DEVH_BRDST_ERROR;
				}
			}
		}
		mutex_unlock(&local_gate);
	}

exit:
	return err;
}

static int devh44xx_sysm3_iommu_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("SysM3");
	struct omap_devh_runtime_info *pinfo = NULL;

	if (!pdata)
		return 0;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	switch ((int)val) {
	case IOMMU_CLOSE:
		if (pinfo->rproc->state != OMAP_RPROC_STOPPED
			&& pinfo->mgr_pid == current->tgid)
			rproc_stop(pinfo->rproc);
		return devh44xx_notifier_call(nb, val, v, pdata);
	case IOMMU_FAULT:
		pinfo->brd_state = DEVH_BRDST_ERROR;
		return 0;
	default:
		return 0;
	}
}

static int devh44xx_appm3_iommu_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("AppM3");
	struct omap_devh_runtime_info *pinfo = NULL;

	if (!pdata)
		return 0;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	switch ((int)val) {
	case IOMMU_CLOSE:
		if (pinfo->rproc->state != OMAP_RPROC_STOPPED
			&& pinfo->mgr_pid == current->tgid)
			rproc_stop(pinfo->rproc);
		return devh44xx_notifier_call(nb, val, v, pdata);
	case IOMMU_FAULT:
		pinfo->brd_state = DEVH_BRDST_ERROR;
		return 0;
	default:
		return 0;
	}
}

static int devh44xx_tesla_iommu_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("Tesla");
	struct omap_devh_runtime_info *pinfo = NULL;

	if (!pdata)
		return 0;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	switch ((int)val) {
	case IOMMU_CLOSE:
		if (pinfo->rproc->state != OMAP_RPROC_STOPPED
			&& pinfo->mgr_pid == current->tgid)
			rproc_stop(pinfo->rproc);
		return devh44xx_notifier_call(nb, val, v, pdata);
	case IOMMU_FAULT:
		pinfo->brd_state = DEVH_BRDST_ERROR;
		return 0;
	default:
		return 0;
	}
}

static struct notifier_block devh_notify_nb_iommu_tesla = {
	.notifier_call = devh44xx_tesla_iommu_notifier_call,
};
static struct notifier_block devh_notify_nb_iommu_ducati0 = {
	.notifier_call = devh44xx_sysm3_iommu_notifier_call,
};
static struct notifier_block devh_notify_nb_iommu_ducati1 = {
	.notifier_call = devh44xx_appm3_iommu_notifier_call,
};

static int devh44xx_sysm3_ipc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("SysM3");

	switch ((int)val) {
	case IPC_CLOSE:
		return devh44xx_notifier_call(nb, val, v, pdata);
	default:
		return 0;
	}
}

static int devh44xx_appm3_ipc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("AppM3");

	switch ((int)val) {
	case IPC_CLOSE:
		return devh44xx_notifier_call(nb, val, v, pdata);
	default:
		return 0;
	}
}

static int devh44xx_tesla_ipc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("Tesla");

	switch ((int)val) {
	case IPC_CLOSE:
		return devh44xx_notifier_call(nb, val, v, pdata);
	default:
		return 0;
	}
}

static struct notifier_block devh_notify_nb_ipc_tesla = {
	.notifier_call = devh44xx_tesla_ipc_notifier_call,
};
static struct notifier_block devh_notify_nb_ipc_ducati1 = {
	.notifier_call = devh44xx_appm3_ipc_notifier_call,
};
static struct notifier_block devh_notify_nb_ipc_ducati0 = {
	.notifier_call = devh44xx_sysm3_ipc_notifier_call,
};

static int devh44xx_sysm3_rproc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("SysM3");
	struct omap_devh_runtime_info *pinfo = NULL;

	if (pdata)
		pinfo = (struct omap_devh_runtime_info *)pdata->private_data;
	else
		return -EINVAL;

	switch ((int)val) {
	case OMAP_RPROC_START:
		pinfo->brd_state = DEVH_BRDST_RUNNING;
		pinfo->mgr_pid = current->tgid;
		pinfo->iommu = iommu_get("ducati");
		if (pinfo->iommu != ERR_PTR(-ENODEV) &&
			pinfo->iommu != ERR_PTR(-EINVAL))
				iommu_register_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_ducati0);
		else
			pinfo->iommu = NULL;
		return 0;
	case OMAP_RPROC_STOP:
		pinfo->brd_state = DEVH_BRDST_STOPPED;
		if (pinfo->iommu != NULL) {
			iommu_unregister_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_ducati0);
			iommu_put(pinfo->iommu);
			pinfo->iommu = NULL;
		}
		return 0;
	default:
		return 0;
	}
}

static int devh44xx_appm3_rproc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("AppM3");
	struct omap_devh_runtime_info *pinfo = NULL;

	if (pdata)
		pinfo = (struct omap_devh_runtime_info *)pdata->private_data;
	else
		return -EINVAL;

	switch ((int)val) {
	case OMAP_RPROC_START:
		pinfo->brd_state = DEVH_BRDST_RUNNING;
		pinfo->mgr_pid = current->tgid;
		pinfo->iommu = iommu_get("ducati");
		if (pinfo->iommu != ERR_PTR(-ENODEV) &&
			pinfo->iommu != ERR_PTR(-EINVAL))
				iommu_register_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_ducati1);
		else
			pinfo->iommu = NULL;
		return 0;
	case OMAP_RPROC_STOP:
		pinfo->brd_state = DEVH_BRDST_STOPPED;
		if (pinfo->iommu != NULL) {
			iommu_unregister_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_ducati1);
			iommu_put(pinfo->iommu);
			pinfo->iommu = NULL;
		}
		return 0;
	default:
		return 0;
	}
}

static int devh44xx_tesla_rproc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("Tesla");
	struct omap_devh_runtime_info *pinfo = NULL;

	if (pdata)
		pinfo = (struct omap_devh_runtime_info *)pdata->private_data;
	else
		return -EINVAL;

	switch ((int)val) {
	case OMAP_RPROC_START:
		pinfo->brd_state = DEVH_BRDST_RUNNING;
		pinfo->mgr_pid = current->tgid;
		pinfo->iommu = iommu_get("tesla");
		if (pinfo->iommu != ERR_PTR(-ENODEV) &&
			pinfo->iommu != ERR_PTR(-EINVAL))
				iommu_register_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_tesla);
		else
			pinfo->iommu = NULL;
		return 0;
	case OMAP_RPROC_STOP:
		pinfo->brd_state = DEVH_BRDST_STOPPED;
		if (pinfo->iommu != NULL) {
			iommu_unregister_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_tesla);
			iommu_put(pinfo->iommu);
			pinfo->iommu = NULL;
		}
		return 0;
	default:
		return 0;
	}
}

static struct notifier_block devh_notify_nb_rproc_tesla = {
	.notifier_call = devh44xx_tesla_rproc_notifier_call,
};
static struct notifier_block devh_notify_nb_rproc_ducati0 = {
	.notifier_call = devh44xx_sysm3_rproc_notifier_call,
};
static struct notifier_block devh_notify_nb_rproc_ducati1 = {
	.notifier_call = devh44xx_appm3_rproc_notifier_call,
};

#if defined(CONFIG_TILER_OMAP)
static int devh44xx_sysm3_tiler_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("SysM3");

	switch ((int)val) {
	case TILER_DEVICE_CLOSE:
#if defined(CONFIG_TILER_PID_KILL_NOTIFICATIONS)
		return devh44xx_notifier_call(nb, val, v, pdata);
#else
		return 0;
#endif
	default:
		return 0;
	}
}

static int devh44xx_appm3_tiler_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("AppM3");

	switch ((int)val) {
	case TILER_DEVICE_CLOSE:
#if defined(CONFIG_TILER_PID_KILL_NOTIFICATIONS)
		return devh44xx_notifier_call(nb, val, v, pdata);
#else
		return 0;
#endif
	default:
		return 0;
	}
}

static int devh44xx_tesla_tiler_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("Tesla");

	switch ((int)val) {
	case TILER_DEVICE_CLOSE:
#if defined(CONFIG_TILER_PID_KILL_NOTIFICATIONS)
		return devh44xx_notifier_call(nb, val, v, pdata);
#else
		return 0;
#endif
	default:
		return 0;
	}
}

static struct notifier_block devh_notify_nb_tiler_tesla = {
	.notifier_call = devh44xx_tesla_tiler_notifier_call,
};
static struct notifier_block devh_notify_nb_tiler_ducati0 = {
	.notifier_call = devh44xx_sysm3_tiler_notifier_call,
};
static struct notifier_block devh_notify_nb_tiler_ducati1 = {
	.notifier_call = devh44xx_appm3_tiler_notifier_call,
};
#endif

static inline int devh44xx_sysm3_register(struct omap_devh *devh)
{
	int retval = 0;
	struct omap_devh_platform_data *pdata = NULL;
	struct omap_devh_runtime_info *pinfo = NULL;
	if (!devh->dev)
		return -EINVAL;

	pdata = (struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!pdata)
		return -EINVAL;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	/* register will kernel modules for event notifications. */
	ipc_register_notifier(&devh_notify_nb_ipc_ducati0);
	pinfo->rproc = omap_rproc_get("ducati-proc0");
	if (pinfo->rproc != ERR_PTR(-ENODEV))
		omap_rproc_register_notifier(pinfo->rproc,
						&devh_notify_nb_rproc_ducati0);
	else
		pinfo->rproc = NULL;
#if defined(CONFIG_TILER_OMAP)
	tiler_reg_notifier(&devh_notify_nb_tiler_ducati0);
#endif

	return retval;
}

static inline int devh44xx_appm3_register(struct omap_devh *devh)
{
	int retval = 0;
	struct omap_devh_platform_data *pdata = NULL;
	struct omap_devh_runtime_info *pinfo = NULL;
	if (!devh->dev)
		return -EINVAL;

	pdata = (struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!pdata)
		return -EINVAL;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	/* register will kernel modules for event notifications. */
	ipc_register_notifier(&devh_notify_nb_ipc_ducati1);
	pinfo->rproc = omap_rproc_get("ducati-proc1");
	if (pinfo->rproc != ERR_PTR(-ENODEV))
		omap_rproc_register_notifier(pinfo->rproc,
						&devh_notify_nb_rproc_ducati1);
	else
		pinfo->rproc = NULL;
#if defined(CONFIG_TILER_OMAP)
	tiler_reg_notifier(&devh_notify_nb_tiler_ducati1);
#endif

	return retval;
}

static inline int devh44xx_tesla_register(struct omap_devh *devh)
{
	int retval = 0;
	struct omap_devh_platform_data *pdata = NULL;
	struct omap_devh_runtime_info *pinfo = NULL;
	if (!devh->dev)
		return -EINVAL;

	pdata = (struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!pdata)
		return -EINVAL;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	/* register will kernel modules for event notifications. */
	ipc_register_notifier(&devh_notify_nb_ipc_tesla);
	pinfo->rproc = omap_rproc_get("tesla");
	if (pinfo->rproc != ERR_PTR(-ENODEV))
		omap_rproc_register_notifier(pinfo->rproc,
						&devh_notify_nb_rproc_tesla);
	else
		pinfo->rproc = NULL;
#if defined(CONFIG_TILER_OMAP)
	tiler_reg_notifier(&devh_notify_nb_tiler_tesla);
#endif

	return retval;
}

static inline int devh44xx_sysm3_unregister(struct omap_devh *devh)
{
	int retval = 0;
	struct omap_devh_platform_data *pdata = NULL;
	struct omap_devh_runtime_info *pinfo = NULL;

	if (!devh->dev)
		return -EINVAL;

	pdata = (struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!pdata)
		return -EINVAL;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	/* un-register will kernel modules for event notifications. */
	ipc_unregister_notifier(&devh_notify_nb_ipc_ducati0);
	if (pinfo->rproc) {
		omap_rproc_unregister_notifier(pinfo->rproc,
						&devh_notify_nb_rproc_ducati0);
		omap_rproc_put(pinfo->rproc);
	}
	if (pinfo->iommu) {
		iommu_unregister_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_ducati0);
		iommu_put(pinfo->iommu);
	}
#if defined(CONFIG_TILER_OMAP)
	tiler_unreg_notifier(&devh_notify_nb_tiler_ducati0);
#endif

	return retval;
}

static inline int devh44xx_appm3_unregister(struct omap_devh *devh)
{
	int retval = 0;
	struct omap_devh_platform_data *pdata = NULL;
	struct omap_devh_runtime_info *pinfo = NULL;

	if (!devh->dev)
		return -EINVAL;

	pdata = (struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!pdata)
		return -EINVAL;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	/* un-register will kernel modules for event notifications. */
	ipc_unregister_notifier(&devh_notify_nb_ipc_ducati1);
	if (pinfo->rproc) {
		omap_rproc_unregister_notifier(pinfo->rproc,
						&devh_notify_nb_rproc_ducati1);
		omap_rproc_put(pinfo->rproc);
	}
	if (pinfo->iommu) {
		iommu_unregister_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_ducati1);
		iommu_put(pinfo->iommu);
	}
#if defined(CONFIG_TILER_OMAP)
	tiler_unreg_notifier(&devh_notify_nb_tiler_ducati1);
#endif

	return retval;
}

static inline int devh44xx_tesla_unregister(struct omap_devh *devh)
{
	int retval = 0;
	struct omap_devh_platform_data *pdata = NULL;
	struct omap_devh_runtime_info *pinfo = NULL;

	if (!devh->dev)
		return -EINVAL;

	pdata = (struct omap_devh_platform_data *)devh->dev->platform_data;

	if (!pdata)
		return -EINVAL;

	pinfo = (struct omap_devh_runtime_info *)pdata->private_data;

	/* un-register will kernel modules for event notifications. */
	ipc_unregister_notifier(&devh_notify_nb_ipc_tesla);
	if (pinfo->rproc) {
		omap_rproc_unregister_notifier(pinfo->rproc,
						&devh_notify_nb_rproc_tesla);
		omap_rproc_put(pinfo->rproc);
	}
	if (pinfo->iommu) {
		iommu_unregister_notifier(pinfo->iommu,
						&devh_notify_nb_iommu_tesla);
		iommu_put(pinfo->iommu);
	}
#if defined(CONFIG_TILER_OMAP)
	tiler_unreg_notifier(&devh_notify_nb_tiler_tesla);
#endif
	return retval;
}

static struct omap_devh_runtime_info omap4_sysm3_runtime_info = {
	.brd_state = DEVH_BRDST_STOPPED,
	.iommu = NULL,
	.rproc = NULL,
};

static struct omap_devh_runtime_info omap4_appm3_runtime_info = {
	.brd_state = DEVH_BRDST_STOPPED,
	.iommu = NULL,
	.rproc = NULL,
};

static struct omap_devh_runtime_info omap4_tesla_runtime_info = {
	.brd_state = DEVH_BRDST_STOPPED,
	.iommu = NULL,
	.rproc = NULL,
};

static struct omap_devh_ops omap4_sysm3_ops = {
	.register_notifiers = devh44xx_sysm3_register,
	.unregister_notifiers = devh44xx_sysm3_unregister,
};

static struct omap_devh_ops omap4_appm3_ops = {
	.register_notifiers = devh44xx_appm3_register,
	.unregister_notifiers = devh44xx_appm3_unregister,
};

static struct omap_devh_ops omap4_tesla_ops = {
	.register_notifiers = devh44xx_tesla_register,
	.unregister_notifiers = devh44xx_tesla_unregister,
};

static struct omap_devh_platform_data omap4_devh_data[] = {
	{
		.name = "Tesla",
		.ops = &omap4_tesla_ops,
		.proc_id = 0,
		.private_data = &omap4_tesla_runtime_info,
	},
	{
		.name = "SysM3",
		.ops = &omap4_sysm3_ops,
		.proc_id = 2,
		.private_data = &omap4_sysm3_runtime_info,
	},
	{
		.name = "AppM3",
		.ops = &omap4_appm3_ops,
		.proc_id = 1,
		.private_data = &omap4_appm3_runtime_info,
	},
};

int devh_get_plat_data_size(void)
{
	return ARRAY_SIZE(omap4_devh_data);
}
EXPORT_SYMBOL(devh_get_plat_data_size);


#define NR_DEVH_DEVICES ARRAY_SIZE(omap4_devh_data)

static struct platform_device *omap4_devh_pdev[NR_DEVH_DEVICES];

struct omap_devh_platform_data *devh_get_plat_data(void)
{
	return omap4_devh_data;
}

static int __init omap4_devh_init(void)
{
	int i, err;

	mutex_init(&local_gate);
	for (i = 0; i < NR_DEVH_DEVICES; i++) {
		struct platform_device *pdev;

		pdev = platform_device_alloc("omap-devicehandler", i);
		if (!pdev) {
			err = -ENOMEM;
			goto err_out;
		}

		err = platform_device_add_data(pdev, &omap4_devh_data[i],
					       sizeof(omap4_devh_data[0]));
		sema_init(&(omap4_devh_data[i].sem_handle), 0);
		err = platform_device_add(pdev);
		if (err)
			goto err_out;
		omap4_devh_pdev[i] = pdev;
	}
	return 0;

err_out:
	while (i--)
		platform_device_put(omap4_devh_pdev[i]);
	return err;
}
module_init(omap4_devh_init);

static void __exit omap4_devh_exit(void)
{
	int i;

	for (i = 0; i < NR_DEVH_DEVICES; i++)
		platform_device_unregister(omap4_devh_pdev[i]);
}
module_exit(omap4_devh_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OMAP4 Device Handler module");
MODULE_AUTHOR("Angela Stegmaier <angelabaker@ti.com>");
