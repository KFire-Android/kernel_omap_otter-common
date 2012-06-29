/*
 * linux/drivers/video/omap2/dss/wb.c
 * Copyright (C) 2009 Texas Instruments
 * Author: mythripk <mythripk@ti.com>
 * Some code and ideas taken from drivers/video/omap/ driver
 * by Imre Deak.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define DSS_SUBSYS_NAME "WRITEBACK"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <video/omapdss.h>
#include <plat/cpu.h>

#include "dss.h"

static struct list_head wb_list;

static struct attribute *writeback_sysfs_attrs[] = {
	NULL
};

static ssize_t writeback_attr_show(struct kobject *kobj, struct attribute *attr,
		char *buf)
{
return 0;
}

static ssize_t writeback_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t size)
{
return 0;
}

static const struct sysfs_ops writeback_sysfs_ops = {
	.show = writeback_attr_show,
	.store = writeback_attr_store,
};

static struct kobj_type writeback_ktype = {
	.sysfs_ops = &writeback_sysfs_ops,
	.default_attrs = writeback_sysfs_attrs,
};

bool omap_dss_check_wb(struct writeback_cache_data *wb, int overlayId,
			int managerId)
{
	bool result = false;
	DSSDBG("ovl=%d,mgr=%d,mode=%s(%s),src=%d\n",
	   overlayId, managerId,
	   wb->mode == OMAP_WB_MEM2MEM_MODE ? "mem2mem" : "capture",
	   wb->source >= OMAP_WB_GFX ? "overlay" : "manager",
	   wb->source);

	if ((wb->mode == OMAP_WB_MEM2MEM_MODE) &&
				((wb->source - 3) == overlayId))
		result = true;
	else if (wb->mode == OMAP_WB_CAPTURE_MODE) {
		switch (wb->source) {
		case OMAP_WB_LCD1:
			if (managerId == OMAP_DSS_CHANNEL_LCD)
				result = true;
			break;
		case OMAP_WB_LCD2:
			if (managerId == OMAP_DSS_CHANNEL_LCD2)
				result = true;
			break;
		case OMAP_WB_TV:
			if (managerId == OMAP_DSS_CHANNEL_DIGIT)
				result = true;
			break;
		case OMAP_WB_GFX:
		case OMAP_WB_VID1:
		case OMAP_WB_VID2:
		case OMAP_WB_VID3:
			break;
		}
	}

	return result;

}

static bool dss_check_wb(struct omap_writeback *wb)
{
	DSSDBG("srctype=%s,src=%d\n",
	   wb->info.source >= OMAP_WB_GFX ? "overlay" : "manager",
	   wb->info.source);

	return 0;
}

static int omap_dss_wb_set_info(struct omap_writeback *wb,
		struct omap_writeback_info *info)
{
	int r;
	struct omap_writeback_info old_info;
	old_info = wb->info;
	wb->info = *info;

	r = dss_check_wb(wb);
	if (r) {
		wb->info = old_info;
		return r;
	}

	wb->info_dirty = true;

	return 0;
}

static void wb_irq_handler(void *data, u32 mask)
{
	complete((struct completion *)data);
	omap_dispc_unregister_isr(wb_irq_handler,
		(struct completion *)data, DISPC_IRQ_FRAMEDONE_WB);
}

static int omap_dss_wb_wait_framedone(struct omap_writeback *wb)
{
	int timeout = wait_for_completion_timeout(&wb->wb_completion,
					msecs_to_jiffies(50));
	if (timeout == 0)
		return -ETIMEDOUT;
	if (timeout == -ERESTARTSYS)
		return -ERESTARTSYS;
	return 0;
}

static int omap_dss_wb_register_framedone(struct omap_writeback *wb)
{
	INIT_COMPLETION(wb->wb_completion);
	return omap_dispc_register_isr(wb_irq_handler,
			&wb->wb_completion, DISPC_IRQ_FRAMEDONE_WB);
}

static void omap_dss_wb_get_info(struct omap_writeback *wb,
		struct omap_writeback_info *info)
{
	*info = wb->info;
}

struct omap_writeback *omap_dss_get_wb(int num)
{
	int i = 0;
	struct omap_writeback *wb;

	list_for_each_entry(wb, &wb_list, list) {
		if (i++ == num)
			return wb;
	}

	return NULL;
}
EXPORT_SYMBOL(omap_dss_get_wb);

static __attribute__ ((unused)) void omap_dss_add_wb(struct omap_writeback *wb)
{
	list_add_tail(&wb->list, &wb_list);
}


void dss_init_writeback(struct platform_device *pdev)
{
	int r;
	struct omap_writeback *wb;

	INIT_LIST_HEAD(&wb_list);

	wb = kzalloc(sizeof(*wb), GFP_KERNEL);

	BUG_ON(wb == NULL);

	wb->check_wb = &dss_check_wb;
	wb->set_wb_info = &omap_dss_wb_set_info;
	wb->get_wb_info = &omap_dss_wb_get_info;
	wb->register_framedone = &omap_dss_wb_register_framedone;
	wb->wait_framedone = &omap_dss_wb_wait_framedone;
	mutex_init(&wb->lock);

	init_completion(&wb->wb_completion);

	omap_dss_add_wb(wb);

	r = kobject_init_and_add(&wb->kobj, &writeback_ktype,
			&pdev->dev.kobj, "writeback%d", 0);

	if (r)
		DSSERR("failed to create sysfs file\n");
}

void dss_uninit_writeback(struct platform_device *pdev)
{
	struct omap_writeback *wb;

	while (!list_empty(&wb_list)) {
		wb = list_first_entry(&wb_list,
				struct omap_writeback, list);
		list_del(&wb->list);
		kobject_del(&wb->kobj);
		kobject_put(&wb->kobj);
		kfree(wb);
	}
}
