/*
 * OMAP Remote processor resource manager resources
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * Fernando Guzman Lugo <fernando.lugo@ti.com>
 * Miguel Vadillo <vadillo@ti.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/rpmsg_resmgr.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <plat/dma.h>
#include <plat/dmtimer.h>
#include <plat/omap-pm.h>
#include <plat/rpmsg_resmgr.h>
#include "omap_rpmsg_resmgr.h"

#define GPTIMERS_MAX	11

struct rprm_gpt_depot {
	struct rprm_gpt args;
	struct omap_dm_timer *gpt;
};

struct rprm_auxclk_depot {
	struct rprm_auxclk args;
	struct clk *clk;
};

/* device handle for resources which use the generic apis */
struct rprm_gen_device_handle {
	struct device *dev;
	struct dev_pm_qos_request req;
};

/* pointer to the constraint ops exported by omap mach module */
static struct omap_rprm_ops *cnstrnt_ops;

static inline struct device *__find_device_by_name(const char *name)
{
	return bus_find_device_by_name(&platform_bus_type, NULL, name);
}

static int rprm_gptimer_request(void **handle, void *data, size_t len)
{
	int ret;
	struct rprm_gpt_depot *gptd;
	struct rprm_gpt *gpt = data;

	if (len != sizeof *gpt)
		return -EINVAL;

	if (gpt->id > GPTIMERS_MAX) {
		pr_err("invalid gptimer %u\n", gpt->id);
		return -EINVAL;
	}

	gptd = kmalloc(sizeof *gptd, GFP_KERNEL);
	if (!gptd)
		return -ENOMEM;

	gptd->gpt = omap_dm_timer_request_specific(gpt->id);
	if (!gptd->gpt) {
		ret = -EBUSY;
		goto free_handle;
	}

	ret = omap_dm_timer_set_source(gptd->gpt, gpt->src_clk);
	if (ret) {
		pr_err("invalid source %i\n", gpt->src_clk);
		goto free_gpt;
	}

	omap_dm_timer_enable(gptd->gpt);
	memcpy(&gptd->args, gpt, sizeof *gpt);
	*handle = gptd;
	return 0;

free_gpt:
	omap_dm_timer_free(gptd->gpt);
free_handle:
	kfree(gptd);
	return ret;
}

static int rprm_gptimer_release(void *handle)
{
	struct rprm_gpt_depot *gptd = handle;

	omap_dm_timer_disable(gptd->gpt);
	omap_dm_timer_free(gptd->gpt);
	kfree(gptd);

	return 0;
}

static int rprm_gptimer_get_info(void *handle, char *buf, size_t len)
{
	struct rprm_gpt_depot *gptd = handle;
	struct rprm_gpt *gpt = &gptd->args;

	return snprintf(buf, len,
		"Id:%d\n"
		"Source:%d\n",
		gpt->id, gpt->src_clk);
}

static int rprm_sdma_request(void **handle, void *data, size_t len)
{
	int ret;
	int ch;
	int i;
	struct rprm_sdma *sd;
	struct rprm_sdma *sdma = data;

	if (len != sizeof *sdma)
		return -EINVAL;

	if (sdma->num_chs > MAX_NUM_SDMA_CHANNELS) {
		pr_err("not able to provide %u channels\n", sdma->num_chs);
		return -EINVAL;
	}

	/* Create sdma depot */
	sd = kmalloc(sizeof *sd, GFP_KERNEL);
	if (!sd)
		return -ENOMEM;

	for (i = 0; i < sdma->num_chs; i++) {
		ret = omap_request_dma(0, "rpmsg_resmgr", NULL, NULL, &ch);
		if (ret) {
			pr_err("error %d providing sdma channel %d\n", ret, ch);
			goto err;
		}
		sdma->channels[i] = ch;
		pr_debug("providing sdma ch %d\n", ch);
	}

	*handle = memcpy(sd, sdma, sizeof *sdma);

	return 0;
err:
	while (i--)
		omap_free_dma(sdma->channels[i]);
	kfree(sd);
	return ret;
}

static int rprm_sdma_release(void *handle)
{
	struct rprm_sdma *sd = handle;
	int i = sd->num_chs;

	while (i--) {
		omap_free_dma(sd->channels[i]);
		pr_debug("releasing sdma ch %d\n", sd->channels[i]);
	}
	kfree(sd);

	return 0;
}

static int rprm_sdma_get_info(void *handle, char *buf, size_t len)
{
	struct rprm_sdma *sd = handle;
	int i, ret = 0;

	ret += snprintf(buf, len, "NumChannels:%d\n", sd->num_chs);
	for (i = 0 ; i < sd->num_chs; i++)
		ret += snprintf(buf + ret, len - ret, "Channel[%d]:%d\n", i,
							sd->channels[i]);
	return ret;
}

static int rprm_auxclk_request(void **handle, void *args, size_t len)
{
	int ret;
	struct rprm_auxclk *auxck = args;
	struct rprm_auxclk_depot *acd;
	struct clk *src;
	struct clk *parent;

	if (len != sizeof *auxck)
		return -EINVAL;

	/* Create auxclks depot */
	acd = kmalloc(sizeof *acd, GFP_KERNEL);
	if (!acd)
		return -ENOMEM;

	/* make sure make is NULL terminated */
	auxck->name[sizeof auxck->name - 1] = '\0';
	acd->clk = clk_get(NULL, auxck->name);
	if (!acd->clk) {
		pr_err("unable to get clock %s\n", auxck->name);
		ret = -EIO;
		goto error;
	}
	/*
	 * The parent for an auxiliar clock is set to the auxclkX_ck_src
	 * clock which is the parent of auxclkX_ck
	 */
	src = clk_get_parent(acd->clk);
	if (!src) {
		pr_err("unable to get %s source clock\n", auxck->name);
		ret = -EIO;
		goto error_aux;
	}

	/* make sure pname is NULL terminated */
	auxck->pname[sizeof auxck->pname - 1] = '\0';
	parent = clk_get(NULL, auxck->pname);
	if (!parent) {
		pr_err("unable to get parent clock %s\n", auxck->pname);
		ret = -EIO;
		goto error_aux;
	}

	ret = clk_set_parent(src, parent);
	if (ret) {
		pr_err("unable to set clk %s as parent of %s\n",
						auxck->pname, auxck->name);
		goto error_aux_parent;
	}

	ret = clk_set_rate(parent, auxck->pclk_rate);
	if (ret) {
		pr_err("rate %u not supported by %s\n", auxck->pclk_rate,
								auxck->pname);
		goto error_aux_parent;
	}

	ret = clk_set_rate(acd->clk, auxck->clk_rate);
	if (ret) {
		pr_err("rate %u not supported by %s\n", auxck->clk_rate,
								auxck->name);
		goto error_aux_parent;
	}

	ret = clk_enable(acd->clk);
	if (ret) {
		pr_err("error enabling %s\n", auxck->name);
		goto error_aux_parent;
	}
	clk_put(parent);

	memcpy(&acd->args, auxck, sizeof *auxck);

	*handle = acd;

	return 0;
error_aux_parent:
	clk_put(parent);
error_aux:
	clk_put(acd->clk);
error:
	kfree(acd);

	return ret;
}

static int rprm_auxclk_release(void *handle)
{
	struct rprm_auxclk_depot *acd = handle;

	clk_disable(acd->clk);
	clk_put(acd->clk);

	kfree(acd);

	return 0;
}

static int rprm_auxclk_get_info(void *handle, char *buf, size_t len)
{
	struct rprm_auxclk_depot *acd = handle;
	struct rprm_auxclk *auxck = &acd->args;

	return snprintf(buf, len,
		"name:%s\n"
		"rate:%u\n"
		"parent:%s\n"
		"parent rate:%u\n",
		auxck->name, auxck->clk_rate, auxck->pname, auxck->pclk_rate);
}

static int
_enable_device_exclusive(void **handle, struct device **pdev, const char *name)
{
	struct device *dev = *pdev;
	struct rprm_gen_device_handle *rprm_handle;
	int ret;

	/* if we already have dev do not search it again */
	if (!dev) {
		dev = __find_device_by_name(name);
		if (!dev)
			return -ENOENT;
		if (dev && !pm_runtime_enabled(dev))
			pm_runtime_enable(dev);
		/* update pdev that works as cache to avoid searing again */
		*pdev = dev;
	}

	rprm_handle = kzalloc(sizeof *rprm_handle, GFP_KERNEL);
	if (!rprm_handle)
		return -ENOMEM;

	ret = dev_pm_qos_add_request(dev, &rprm_handle->req,
						PM_QOS_DEFAULT_VALUE);
	if (ret < 0)
		goto err_handle_free;

	ret = pm_runtime_get_sync(dev);
	if (ret) {
		/*
		 * we want exclusive accesss to the device, so if it is already
		 * enabled (ret == 1), call pm_runtime_put because somebody
		 * else is using it and return error.
		 */
		if (ret == 1) {
			pm_runtime_put_sync(dev);
			ret = -EBUSY;
		}

		pr_err("error %d get sync for %s\n", ret, name);
		goto err_qos_free;
	}

	rprm_handle->dev = dev;
	*handle = rprm_handle;

	return 0;

err_qos_free:
	dev_pm_qos_remove_request(&rprm_handle->req);
err_handle_free:
	kfree(rprm_handle);
	return ret;
}

static int _device_release(void *handle)
{
	struct rprm_gen_device_handle *obj = handle;
	struct device *dev = obj->dev;

	dev_pm_qos_remove_request(&obj->req);
	kfree(obj);

	return pm_runtime_put_sync(dev);
}

static int _device_scale(struct device *rdev, void *handle, unsigned long val)
{
	struct rprm_gen_device_handle *obj = handle;

	return 0;

	if (!cnstrnt_ops || !cnstrnt_ops->device_scale)
			return -ENOSYS;

	return cnstrnt_ops->device_scale(rdev, obj->dev, val);
}

static int _device_latency(struct device *rdev, void *handle, unsigned long val)
{
	struct rprm_gen_device_handle *obj = handle;
	int ret = dev_pm_qos_update_request(&obj->req, val);

	return ret -= ret == 1;
}

static int _device_bandwidth(struct device *rdev, void *handle,
							 unsigned long val)
{
	struct rprm_gen_device_handle *obj = handle;

	if (!cnstrnt_ops || !cnstrnt_ops->set_min_bus_tput)
		return -ENOSYS;

	return cnstrnt_ops->set_min_bus_tput(rdev, obj->dev, val);
}

static int rprm_iva_request(void **handle, void *data, size_t len)
{
	static struct device *dev;

	return  _enable_device_exclusive(handle, &dev, "iva.0");
}

static int rprm_iva_seq0_request(void **handle, void *data, size_t len)
{
	static struct device *dev;

	return _enable_device_exclusive(handle, &dev, "iva_seq0.0");
}

static int rprm_iva_seq1_request(void **handle, void *data, size_t len)
{
	static struct device *dev;

	return _enable_device_exclusive(handle, &dev, "iva_seq1.0");
}

static int rprm_fdif_request(void **handle, void *data, size_t len)
{
	static struct device *dev;

	return _enable_device_exclusive(handle, &dev, "fdif");
}

static int rprm_sl2if_request(void **handle, void *data, size_t len)
{
	static struct device *dev;

	return _enable_device_exclusive(handle, &dev, "sl2if");
}

static int rprm_iss_request(void **handle, void *data, size_t len)
{
	static struct device *dev;

	return _enable_device_exclusive(handle, &dev, "iss");
}

static struct rprm_res_ops gptimer_ops = {
	.request = rprm_gptimer_request,
	.release = rprm_gptimer_release,
	.get_info = rprm_gptimer_get_info,
};

static struct rprm_res_ops sdma_ops = {
	.request = rprm_sdma_request,
	.release = rprm_sdma_release,
	.get_info = rprm_sdma_get_info,
};

static struct rprm_res_ops auxclk_ops = {
	.request = rprm_auxclk_request,
	.release = rprm_auxclk_release,
	.get_info = rprm_auxclk_get_info,
};

static struct rprm_res_ops iva_ops = {
	.request = rprm_iva_request,
	.release = _device_release,
	.latency = _device_latency,
	.bandwidth = _device_bandwidth,
	.scale = _device_scale,
};

static struct rprm_res_ops iva_seq0_ops = {
	.request = rprm_iva_seq0_request,
	.release = _device_release,
};

static struct rprm_res_ops iva_seq1_ops = {
	.request = rprm_iva_seq1_request,
	.release = _device_release,
};

static struct rprm_res_ops fdif_ops = {
	.request = rprm_fdif_request,
	.release = _device_release,
	.latency = _device_latency,
	.bandwidth = _device_bandwidth,
	.scale = _device_scale,
};

static struct rprm_res_ops sl2if_ops = {
	.request = rprm_sl2if_request,
	.release = _device_release,
};

static struct rprm_res_ops iss_ops = {
	.request = rprm_iss_request,
	.release = _device_release,
	.latency = _device_latency,
	.bandwidth = _device_bandwidth,
};

static struct rprm_res omap_res[] = {
	{
		.name = "omap-gptimer",
		.ops = &gptimer_ops,
	},
	{
		.name = "omap-sdma",
		.ops = &sdma_ops,
	},
	{
		.name = "omap-auxclk",
		.ops = &auxclk_ops,
	},
	{
		.name = "iva",
		.ops = &iva_ops,
	},
	{
		.name = "iva_seq0",
		.ops = &iva_seq0_ops,
	},
	{
		.name = "iva_seq1",
		.ops = &iva_seq1_ops,
	},
	{
		.name = "omap-fdif",
		.ops = &fdif_ops,
	},
	{
		.name = "omap-sl2if",
		.ops = &sl2if_ops,
	},
	{
		.name = "omap-iss",
		.ops = &iss_ops,
	},
};

static int omap_rprm_probe(struct platform_device *pdev)
{
	struct omap_rprm_pdata *pdata = pdev->dev.platform_data;

	cnstrnt_ops = pdata->ops;
	return 0;
}

static int omap_rprm_remove(struct platform_device *pdev)
{
	cnstrnt_ops = NULL;

	return 0;
}

static struct platform_driver omap_rprm_driver = {
	.probe = omap_rprm_probe,
	.remove = __devexit_p(omap_rprm_remove),
	.driver = {
		.name = "omap-rprm",
		.owner = THIS_MODULE,
	},
};

int __init omap_rprm_init(void)
{
	int i, ret;

	ret = platform_driver_register(&omap_rprm_driver);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(omap_res); i++) {
		omap_res[i].owner = THIS_MODULE;
		rprm_resource_register(&omap_res[i]);
	}

	return 0;
}

static void __exit omap_rprm_fini(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(omap_res); i++)
		rprm_resource_unregister(&omap_res[i]);

	platform_driver_unregister(&omap_rprm_driver);
}
module_init(omap_rprm_init);
module_exit(omap_rprm_fini);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Remote Processor Resource Manager OMAP resources");
