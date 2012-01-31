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
#include <plat/dma.h>
#include <plat/dmtimer.h>
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
};

int __init omap_rprm_init(void)
{
	int i;

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
}
module_init(omap_rprm_init);
module_exit(omap_rprm_fini);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Remote Processor Resource Manager OMAP resources");
