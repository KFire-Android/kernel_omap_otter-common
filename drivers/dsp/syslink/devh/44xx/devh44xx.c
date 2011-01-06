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
#include <linux/eventfd.h>
#include <mach/irqs.h>
#include <plat/omap_device.h>
#include <plat/iommu.h>
#include <plat/remoteproc.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
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

struct deh_event_ntfy {
	u32 fd;
	u32 event;
	struct eventfd_ctx *evt_ctx;
	struct list_head list;
};

struct omap_devh *devh_get_obj(int dev_index);

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

static int devh_notify_event(struct omap_devh *devh , u32 event)
{
	struct deh_event_ntfy *fd_reg;

	spin_lock_irq(&(devh->event_lock));
	list_for_each_entry(fd_reg, &(devh->event_list), list)
		if (fd_reg->event == event)
			eventfd_signal(fd_reg->evt_ctx, 1);
	spin_unlock_irq(&(devh->event_lock));

	return 0;
}

static void devh_notification_handler(u16 proc_id, u16 line_id, u32 event_id,
					uint *arg, u32 payload)
{
	pr_warning("Sys Error occured in Ducati for proc_id = %d\n",
		proc_id);

	/* schedule the recovery */
	ipc_recover_schedule();

	devh_notify_event((struct omap_devh *)arg, DEV_SYS_ERROR);
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
		err = ipu_pm_notifications(APP_M3, PM_PID_DEATH,
								(void *)my_pid);
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

static int devh44xx_wdt_ipc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh *obj;
	int i = devh_get_plat_data_size();

	pr_warning("Ducati Watch Dog fired\n");

	/* schedule the recovery */
	ipc_recover_schedule();

	while (i--) {
		obj = devh_get_obj(i);
		devh_notify_event(obj, DEV_WATCHDOG_ERROR);
	}

	return 0;
}

static struct notifier_block devh_notify_nb_ipc_wdt = {
	.notifier_call = devh44xx_wdt_ipc_notifier_call,
};

static int devh44xx_sysm3_ipc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("SysM3");
	u16 *proc_id = (u16 *)v;
	struct omap_devh *obj;
	int status;

	switch ((int)val) {
	case IPC_CLOSE:
		return devh44xx_notifier_call(nb, val, v, pdata);
	case IPC_START:
		/*
		 * TODO - hack hack - clean this up to use a define proc id
		 * and use this to get the devh object
		 */
		if (*proc_id == multiproc_get_id("SysM3")) {
			obj = devh_get_obj(1);
			if (WARN_ON(obj == NULL)) {
				status = -1;
				pr_err("devh_44xx_notifier_call: cannot grab "
					"device devh1 instance\n");
			} else {
				status = notify_register_event(pdata->proc_id,
					pdata->line_id,
					pdata->err_event_id,
					(notify_fn_notify_cbck) \
					devh_notification_handler,
					(void *)obj);
			}
			if (status == 0)
				status = ipu_pm_register_notifier(
						&devh_notify_nb_ipc_wdt);
		} else {
			status = 0;
		}
		return status;
	case IPC_STOP:
		if (*proc_id == multiproc_get_id("SysM3")) {
			ipu_pm_unregister_notifier(&devh_notify_nb_ipc_wdt);
			obj = devh_get_obj(1);
			if (WARN_ON(obj == NULL)) {
				status = -1;
				pr_err("devh_44xx_notifier_call: cannot grab "
					"device devh1 instance\n");
			} else {
				status = notify_unregister_event(pdata->proc_id,
					pdata->line_id,
					pdata->err_event_id,
					(notify_fn_notify_cbck) \
					devh_notification_handler,
					(void *)obj);
			}
		} else {
			status = 0;
		}
		return status;
	default:
		return 0;
	}
}

static int devh44xx_appm3_ipc_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	struct omap_devh_platform_data *pdata =
				devh_get_plat_data_by_name("AppM3");
	struct omap_devh *obj;

	u16* proc_id = (u16 *)v;
	int status;
	switch ((int)val) {
	case IPC_CLOSE:
		return devh44xx_notifier_call(nb, val, v, pdata);
	case IPC_START:
		/*
		 * TODO - hack hack - clean this up to use a define proc id
		 * and use this to get the devh object
		 */
		if (*proc_id == multiproc_get_id("AppM3")) {
			obj = devh_get_obj(2);
			if (WARN_ON(obj == NULL)) {
				status = -1;
				pr_err("devh_44xx_notifier_call: cannot grab "
					"device devh2 instance\n");
			} else {
				status = notify_register_event(pdata->proc_id,
					pdata->line_id,
					pdata->err_event_id,
					(notify_fn_notify_cbck) \
					devh_notification_handler,
					(void *)obj);
			}
		} else {
			status = 0;
		}
		return status;
	case IPC_STOP:
		if (*proc_id == multiproc_get_id("AppM3")) {
			obj = devh_get_obj(2);
			if (WARN_ON(obj == NULL)) {
				status = -1;
				pr_err("devh_44xx_notifier_call: cannot grab "
					"device devh2 instance\n");
			} else {
				status = notify_unregister_event(pdata->proc_id,
					pdata->line_id,
					pdata->err_event_id,
					(notify_fn_notify_cbck) \
					devh_notification_handler,
					(void *)obj);
			}
		} else {
			status = 0;
		}
		return status;
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
		return devh44xx_notifier_call(nb, val, v, pdata);
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
		return devh44xx_notifier_call(nb, val, v, pdata);
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
		return devh44xx_notifier_call(nb, val, v, pdata);
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

static inline int devh44xx_register_event
	(struct omap_devh *devh, const void __user *args)
{
	struct deh_event_ntfy *fd_reg;
	struct deh_reg_event_args re_args;

	if (copy_from_user(&re_args, args, sizeof(re_args)))
		return -EFAULT;

	fd_reg = kzalloc(sizeof(struct deh_event_ntfy), GFP_KERNEL);
	if (!fd_reg) {
		dev_err(devh->dev, "%s: kzalloc failed\n", __func__);
		return -ENOMEM;
	}

	fd_reg->fd = re_args.fd;
	fd_reg->event = re_args.event;
	fd_reg->evt_ctx = eventfd_ctx_fdget(re_args.fd);

	INIT_LIST_HEAD(&fd_reg->list);

	spin_lock_irq(&(devh->event_lock));
	list_add_tail(&fd_reg->list, &(devh->event_list));
	spin_unlock_irq(&(devh->event_lock));

	pr_info("Registered user-space process for %s event in %s\n",
		(re_args.event == 1 ? "DEV_SYS_ERROR" :
		(re_args.event == 2 ? "DEV_WATCHDOG_ERROR" : "UNKNOWN EVENT")),
		devh->name);
	return 0;
}

static inline int devh44xx_unregister_event(struct omap_devh *devh,
						const void __user *args)
{
	struct deh_event_ntfy *fd_reg, *tmp_reg;
	struct deh_reg_event_args re_args;
	if (copy_from_user(&re_args, args, sizeof(re_args)))
		return -EFAULT;

	spin_lock_irq(&(devh->event_lock));
	list_for_each_entry_safe(fd_reg, tmp_reg, &(devh->event_list), list)
	{
		if (fd_reg->fd == re_args.fd) {
			list_del(&fd_reg->list);
			kfree(fd_reg);
		}
	}
	spin_unlock_irq(&(devh->event_lock));
	return 0;
}

static struct omap_devh_ops omap4_sysm3_ops = {
	.register_notifiers = devh44xx_sysm3_register,
	.unregister_notifiers = devh44xx_sysm3_unregister,
	.register_event_notification = devh44xx_register_event,
	.unregister_event_notification = devh44xx_unregister_event,
};

static struct omap_devh_ops omap4_appm3_ops = {
	.register_notifiers = devh44xx_appm3_register,
	.unregister_notifiers = devh44xx_appm3_unregister,
	.register_event_notification = devh44xx_register_event,
	.unregister_event_notification = devh44xx_unregister_event,
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
		.err_event_id = -1,
		.line_id = -1,
		.private_data = &omap4_tesla_runtime_info,
	},
	{
		.name = "SysM3",
		.ops = &omap4_sysm3_ops,
		.proc_id = 2,
		.err_event_id = (4 | (NOTIFY_SYSTEMKEY << 16)),
		.line_id = 0,
		.private_data = &omap4_sysm3_runtime_info,
	},
	{
		.name = "AppM3",
		.ops = &omap4_appm3_ops,
		.proc_id = 1,
		.err_event_id = (4 | (NOTIFY_SYSTEMKEY << 16)),
		.line_id = 0,
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

/* returns devh object based on the device index */
struct omap_devh *devh_get_obj(int dev_index)
{
	struct platform_device *pdev;
	struct omap_devh *obj = NULL;
	int max_num_devices;

	max_num_devices = devh_get_plat_data_size();

	if (WARN_ON((dev_index > (max_num_devices - 1))))
		return NULL;

	pdev = omap4_devh_pdev[dev_index];

	if (pdev)
		obj = (struct omap_devh *)platform_get_drvdata(pdev);

	return obj;
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
