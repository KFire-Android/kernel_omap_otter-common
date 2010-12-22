/*
 * Remote Processor machine-specific module for OMAP3
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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <plat/remoteproc.h>
#include <plat/dmtimer.h>

#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>


static inline int proc44x_start(struct device *dev, u32 start_addr)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_rproc *obj = (struct omap_rproc *)platform_get_drvdata(
						to_platform_device(dev));
	int ret = 0;

	/* Enable the Timer that would be used by co-processor */
	if (obj->timer_id >= 0) {
		obj->dmtimer =
			omap_dm_timer_request_specific(obj->timer_id);
		if (!obj->dmtimer) {
			ret = -EBUSY;
			goto err_start;
		}
		omap_dm_timer_set_int_enable(obj->dmtimer,
						OMAP_TIMER_INT_OVERFLOW);
		omap_dm_timer_set_source(obj->dmtimer, OMAP_TIMER_SRC_SYS_CLK);
	}

	ret = omap_device_enable(pdev);
	if (ret)
		goto err_start;

	obj->state = OMAP_RPROC_RUNNING;
	return 0;

err_start:
	dev_err(dev, "%s error 0x%x\n", __func__, ret);
	return ret;
}

static inline int proc44x_stop(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_rproc *obj = (struct omap_rproc *)platform_get_drvdata(
						to_platform_device(dev));
	int ret = 0;

	if (obj->state == OMAP_RPROC_RUNNING) {
		ret = omap_device_shutdown(pdev);
		if (ret)
			dev_err(dev, "%s err 0x%x\n", __func__, ret);
	}

	if (obj->dmtimer) {
		omap_dm_timer_free(obj->dmtimer);
		obj->dmtimer = NULL;
	}

	obj->state = OMAP_RPROC_STOPPED;
	return ret;
}

static inline int proc44x_sleep(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_rproc *obj = (struct omap_rproc *)platform_get_drvdata(
						to_platform_device(dev));
	int ret = 0;

	if (obj->state == OMAP_RPROC_RUNNING) {
		ret = omap_device_shutdown(pdev);
		if (ret)
			dev_err(dev, "%s err 0x%x\n", __func__, ret);

		if (obj->dmtimer)
			omap_dm_timer_stop(obj->dmtimer);
	}

	obj->state = OMAP_RPROC_HIBERNATING;
	return ret;
}

static inline int proc44x_wakeup(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_rproc *obj = (struct omap_rproc *)platform_get_drvdata(
						to_platform_device(dev));
	int ret = 0;

	if (obj->dmtimer)
		omap_dm_timer_start(obj->dmtimer);

	ret = omap_device_enable(pdev);
	if (ret)
		goto err_start;

	obj->state = OMAP_RPROC_RUNNING;
	return 0;

err_start:
	dev_err(dev, "%s error 0x%x\n", __func__, ret);
	return ret;
}


static inline int omap4_rproc_get_state(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct omap_device *odev = to_omap_device(pdev);

	return odev->_state;
}

static struct omap_rproc_ops omap4_ducati0_ops = {
	.start = proc44x_start,
	.stop = proc44x_stop,
	.sleep = proc44x_sleep,
	.wakeup = proc44x_wakeup,
	.get_state = omap4_rproc_get_state,
};

static struct omap_rproc_ops omap4_ducati1_ops = {
	.start = proc44x_start,
	.stop = proc44x_stop,
	.sleep = proc44x_sleep,
	.wakeup = proc44x_wakeup,
	.get_state = omap4_rproc_get_state,
};

static struct omap_rproc_ops omap4_tesla_ops = {
	.start = proc44x_start,
	.stop = proc44x_stop,
	.sleep = proc44x_sleep,
	.wakeup = proc44x_wakeup,
	.get_state = omap4_rproc_get_state,
};

static struct omap_rproc_platform_data omap4_rproc_data[] = {
	{
		.name = "tesla",
		.ops = &omap4_tesla_ops,
		.oh_name = "dsp_c0",
		.timer_id = 5,
	},
	{
		.name = "ducati-proc0",
		.ops = &omap4_ducati0_ops,
		.oh_name = "ipu_c0",
#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
		.timer_id = 3,
#else
		.timer_id = -1,
#endif
	},
	{
		.name = "ducati-proc1",
		.ops = &omap4_ducati1_ops,
		.oh_name = "ipu_c1",
#ifdef CONFIG_SYSLINK_IPU_SELF_HIBERNATION
		.timer_id = 4,
#else
		.timer_id = -1,
#endif

	},
};

static struct omap_device_pm_latency omap_rproc_latency[] = {
	{
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.deactivate_lat = 1,
		.activate_lat = 1,
	},
};

struct omap_rproc_platform_data *omap4_get_rproc_data(void)
{
	return omap4_rproc_data;
}


#define NR_RPROC_DEVICES ARRAY_SIZE(omap4_rproc_data)

static struct omap_device *omap4_rproc_pdev[NR_RPROC_OMAP4_DEVICES];

static int __init omap4_rproc_init(void)
{
	struct omap_hwmod *oh;
	struct omap_device_pm_latency *ohl;
	char *oh_name, *pdev_name;
	int ohl_cnt = 0, i;
	int rproc_data_size;
	struct omap_rproc_platform_data *rproc_data;

	pdev_name = "omap-remoteproc";
	ohl = omap_rproc_latency;
	ohl_cnt = ARRAY_SIZE(omap_rproc_latency);


	rproc_data = omap4_get_rproc_data();
	rproc_data_size = NR_RPROC_OMAP4_DEVICES;

	for (i = 0; i < rproc_data_size; i++) {
		oh_name = rproc_data[i].oh_name;
		oh = omap_hwmod_lookup(oh_name);
		if (!oh) {
			pr_err("%s: could not look up %s\n", __func__, oh_name);
			continue;
		}
		omap4_rproc_pdev[i] = omap_device_build(pdev_name, i, oh,
					&rproc_data[i],
					sizeof(struct omap_rproc_platform_data),
					ohl, ohl_cnt, false);
		WARN(IS_ERR(omap4_rproc_pdev[i]), "Could not build omap_device"
				"for %s %s\n", pdev_name, oh_name);
	}
	return 0;
}
module_init(omap4_rproc_init);

static void __exit omap4_rproc_exit(void)
{

}
module_exit(omap4_rproc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OMAP4 Remote Processor module");
MODULE_AUTHOR("Ohad Ben-Cohen <ohad@wizery.com>");
MODULE_AUTHOR("Hari Kanigeri <h-kanigeri2@ti.com>");
