/*
 * OMAP Remote Processor driver
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 * Copyright (C) 2011 Google, Inc.
 *
 * Ohad Ben-Cohen <ohad@wizery.com>
 * Brian Swetland <swetland@google.com>
 * Fernando Guzman Lugo <fernando.lugo@ti.com>
 * Mark Grosen <mgrosen@ti.com>
 * Suman Anna <s-anna@ti.com>
 * Hari Kanigeri <h-kanigeri2@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/remoteproc.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>

#include <plat/mailbox.h>
#include <plat/remoteproc.h>
#include <plat/dmtimer.h>

#include "omap_remoteproc.h"
#include "remoteproc_internal.h"

/**
 * struct omap_rproc - omap remote processor state
 * @mbox: omap mailbox handle
 * @nb: notifier block that will be invoked on inbound mailbox messages
 * @rproc: rproc handle
 * @boot_reg: virtual address of the register where the bootaddr is stored
 * @qos_req: for requesting latency constraints for rproc
 */
struct omap_rproc {
	struct omap_mbox *mbox;
	struct notifier_block nb;
	struct rproc *rproc;
	void __iomem *boot_reg;
	struct dev_pm_qos_request qos_req;
	atomic_t thrd_cnt;
};

struct _thread_data {
	struct rproc *rproc;
	int msg;
};

static int _vq_interrupt_thread(struct _thread_data *d)
{
	struct omap_rproc *oproc = d->rproc->priv;

	/* msg contains the index of the triggered vring */
	if (rproc_vq_interrupt(d->rproc, d->msg) == IRQ_NONE)
		dev_dbg(d->rproc->dev, "no message was found in vqid 0x0\n");
	kfree(d);
	atomic_dec(&oproc->thrd_cnt);
	return 0;
}

/**
 * omap_rproc_mbox_callback() - inbound mailbox message handler
 * @this: notifier block
 * @index: unused
 * @data: mailbox payload
 *
 * This handler is invoked by omap's mailbox driver whenever a mailbox
 * message is received. Usually, the mailbox payload simply contains
 * the index of the virtqueue that is kicked by the remote processor,
 * and we let remoteproc core handle it.
 *
 * In addition to virtqueue indices, we also have some out-of-band values
 * that indicates different events. Those values are deliberately very
 * big so they don't coincide with virtqueue indices.
 */
static int omap_rproc_mbox_callback(struct notifier_block *this,
					unsigned long index, void *data)
{
	mbox_msg_t msg = (mbox_msg_t) data;
	struct omap_rproc *oproc = container_of(this, struct omap_rproc, nb);
	struct device *dev = oproc->rproc->dev;
	const char *name = oproc->rproc->name;
	struct _thread_data *d;

	dev_dbg(dev, "mbox msg: 0x%x\n", msg);

	switch (msg) {
	case RP_MBOX_CRASH:
		/* just log this for now. later, we'll also do recovery */
		dev_err(dev, "omap rproc %s crashed\n", name);
		break;
	case RP_MBOX_ECHO_REPLY:
		dev_info(dev, "received echo reply from %s\n", name);
		break;
	default:
		if (msg >= RP_MBOX_END_MSG) {
			dev_info(dev, "Dropping unknown message %x", msg);
			return NOTIFY_DONE;
		}
		d = kmalloc(sizeof(*d), GFP_KERNEL);
		if (!d)
			break;
		d->rproc = oproc->rproc;
		d->msg = msg;
		atomic_inc(&oproc->thrd_cnt);
		kthread_run((void *)_vq_interrupt_thread, d,
					"vp_interrupt_thread");
	}

	return NOTIFY_DONE;
}

/* kick a virtqueue */
static void omap_rproc_kick(struct rproc *rproc, int vqid)
{
	struct omap_rproc *oproc = rproc->priv;
	int ret;

	/* send the index of the triggered virtqueue in the mailbox payload */
	ret = omap_mbox_msg_send(oproc->mbox, vqid);
	if (ret)
		dev_err(rproc->dev, "omap_mbox_msg_send failed: %d\n", ret);
}

static int
omap_rproc_set_latency(struct device *dev, struct rproc *rproc, long val)
{
	struct platform_device *pdev = to_platform_device(rproc->dev);
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;
	struct omap_rproc *oproc = rproc->priv;
	int ret;

	/* Call device specific api if any */
	if (pdata->ops && pdata->ops->set_latency)
		return pdata->ops->set_latency(dev, rproc, val);

	ret = dev_pm_qos_update_request(&oproc->qos_req, val);
	/*
	 * dev_pm_qos_update_request returns 0 or 1 on success depending
	 * on if the constraint changed or not (same request). So, return
	 * 0 in both cases
	 */
	return ret - ret == 1;
}

static int
omap_rproc_set_bandwidth(struct device *dev, struct rproc *rproc, long val)
{
	struct platform_device *pdev = to_platform_device(rproc->dev);
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;

	/* Call device specific api if any */
	if (pdata->ops && pdata->ops->set_bandwidth)
		return pdata->ops->set_bandwidth(dev, rproc, val);

	/* TODO: call platform specific */

	return 0;
}

static int
omap_rproc_set_frequency(struct device *dev, struct rproc *rproc, long val)
{
	struct platform_device *pdev = to_platform_device(rproc->dev);
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;

	/* Call device specific api if any */
	if (pdata->ops && pdata->ops->set_frequency)
		return pdata->ops->set_frequency(dev, rproc, val);

	/* TODO: call platform specific */

	return 0;
}

/*
 * Power up the remote processor.
 *
 * This function will be invoked only after the firmware for this rproc
 * was loaded, parsed successfully, and all of its resource requirements
 * were met.
 */
static int omap_rproc_start(struct rproc *rproc)
{
	struct omap_rproc *oproc = rproc->priv;
	struct platform_device *pdev = to_platform_device(rproc->dev);
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;
	struct omap_rproc_timers_info *timers = pdata->timers;
	int ret, i;

	/* init thread counter for mbox messages */
	atomic_set(&oproc->thrd_cnt, 0);
	/* load remote processor boot address if needed. */
	if (oproc->boot_reg)
		writel(rproc->bootaddr, oproc->boot_reg);

	oproc->nb.notifier_call = omap_rproc_mbox_callback;

	/* every omap rproc is assigned a mailbox instance for messaging */
	oproc->mbox = omap_mbox_get(pdata->mbox_name, &oproc->nb);
	if (IS_ERR(oproc->mbox)) {
		ret = PTR_ERR(oproc->mbox);
		dev_err(rproc->dev, "omap_mbox_get failed: %d\n", ret);
		return ret;
	}

	/*
	 * Ping the remote processor. this is only for sanity-sake;
	 * there is no functional effect whatsoever.
	 *
	 * Note that the reply will _not_ arrive immediately: this message
	 * will wait in the mailbox fifo until the remote processor is booted.
	 */
	ret = omap_mbox_msg_send(oproc->mbox, RP_MBOX_ECHO_REQUEST);
	if (ret) {
		dev_err(rproc->dev, "omap_mbox_get failed: %d\n", ret);
		goto put_mbox;
	}

	for (i = 0; i < pdata->timers_cnt; i++) {
		timers[i].odt = omap_dm_timer_request_specific(timers[i].id);
		if (!timers[i].odt) {
			ret = -EBUSY;
			dev_err(rproc->dev,
				"omap_dm_timer_request failed: %d\n", ret);
			goto err_timers;
		}
		omap_dm_timer_set_source(timers[i].odt, OMAP_TIMER_SRC_SYS_CLK);
		omap_dm_timer_start(timers[i].odt);
	}

	ret = pdata->device_enable(pdev);
	if (ret) {
		dev_err(rproc->dev, "omap_device_enable failed: %d\n", ret);
		goto put_mbox;
	}

	return 0;

err_timers:
	while (i--) {
		omap_dm_timer_stop(timers[i].odt);
		omap_dm_timer_free(timers[i].odt);
		timers[i].odt = NULL;
	}

put_mbox:
	omap_mbox_put(oproc->mbox, &oproc->nb);
	return ret;
}

/* power off the remote processor */
static int omap_rproc_stop(struct rproc *rproc)
{
	struct platform_device *pdev = to_platform_device(rproc->dev);
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;
	struct omap_rproc *oproc = rproc->priv;
	struct omap_rproc_timers_info *timers = pdata->timers;
	int ret, i;

	ret = pdata->device_shutdown(pdev);
	if (ret)
		return ret;

	for (i = 0; i < pdata->timers_cnt; i++) {
		omap_dm_timer_stop(timers[i].odt);
		omap_dm_timer_free(timers[i].odt);
		timers[i].odt = NULL;
	}

	omap_mbox_put(oproc->mbox, &oproc->nb);

	/* wait untill all threads have finished */
	while (atomic_read(&oproc->thrd_cnt))
		schedule();

	return 0;
}

static struct rproc_ops omap_rproc_ops = {
	.start		= omap_rproc_start,
	.stop		= omap_rproc_stop,
	.kick		= omap_rproc_kick,
	.set_latency	= omap_rproc_set_latency,
	.set_bandwidth	= omap_rproc_set_bandwidth,
	.set_frequency	= omap_rproc_set_frequency,
};

static int __devinit omap_rproc_probe(struct platform_device *pdev)
{
	struct omap_rproc_pdata *pdata = pdev->dev.platform_data;
	struct omap_rproc *oproc;
	struct rproc *rproc;
	int ret;

	ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(pdev->dev.parent, "dma_set_coherent_mask: %d\n", ret);
		return ret;
	}

	rproc = rproc_alloc(&pdev->dev, pdata->name, &omap_rproc_ops,
				pdata->firmware, sizeof(*oproc));
	if (!rproc)
		return -ENOMEM;

	oproc = rproc->priv;
	oproc->rproc = rproc;

	if (pdata->boot_reg) {
		oproc->boot_reg = ioremap(pdata->boot_reg, sizeof(u32));
		if (!oproc->boot_reg)
			goto err_ioremap;
	}

	platform_set_drvdata(pdev, rproc);
	ret = dev_pm_qos_add_request(&pdev->dev, &oproc->qos_req,
			PM_QOS_DEFAULT_VALUE);
	if (ret)
		goto free_rproc;

	ret = rproc_register(rproc);
	if (ret)
		goto remove_req;

	return 0;

remove_req:
	dev_pm_qos_remove_request(&oproc->qos_req);
free_rproc:
	if (oproc->boot_reg)
		iounmap(oproc->boot_reg);
err_ioremap:
	rproc_free(rproc);
	return ret;
}

static int __devexit omap_rproc_remove(struct platform_device *pdev)
{
	struct rproc *rproc = platform_get_drvdata(pdev);
	struct omap_rproc *oproc = rproc->priv;

	if (oproc->boot_reg)
		iounmap(oproc->boot_reg);

	dev_pm_qos_remove_request(&oproc->qos_req);
	return rproc_unregister(rproc);
}

static struct platform_driver omap_rproc_driver = {
	.probe = omap_rproc_probe,
	.remove = __devexit_p(omap_rproc_remove),
	.driver = {
		.name = "omap-rproc",
		.owner = THIS_MODULE,
	},
};

module_platform_driver(omap_rproc_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("OMAP Remote Processor control driver");
