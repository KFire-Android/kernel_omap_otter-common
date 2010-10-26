/*
 * hsi_driver.c
 *
 * Implements HSI module interface, initialization, and PM related functions.
 *
 * Copyright (C) 2007-2008 Nokia Corporation. All rights reserved.
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Author: Carlos Chinea <carlos.chinea@nokia.com>
 * Author: Sebastien JAN <s-jan@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include "hsi_driver.h"

#define	HSI_DRIVER_VERSION	"0.1"
#define HSI_RESETDONE_TIMEOUT	10	/* 10 ms */
#define HSI_RESETDONE_RETRIES	20	/* => max 200 ms waiting for reset */

/* NOTE: Function called in interrupt context */
int hsi_port_event_handler(struct hsi_port *p, unsigned int event, void *arg)
{
	struct hsi_channel *hsi_channel;
	int ch;

	if (event == HSI_EVENT_HSR_DATAAVAILABLE) {
		/* The data-available event is channel-specific and must not be
		 * broadcasted
		 */
		hsi_channel = p->hsi_channel + (int)arg;
		read_lock(&hsi_channel->rw_lock);
		if ((hsi_channel->dev) && (hsi_channel->port_event))
			hsi_channel->port_event(hsi_channel->dev, event, arg);
		read_unlock(&hsi_channel->rw_lock);
	} else {
		for (ch = 0; ch < p->max_ch; ch++) {
			hsi_channel = p->hsi_channel + ch;
			read_lock(&hsi_channel->rw_lock);
			if ((hsi_channel->dev) && (hsi_channel->port_event))
				hsi_channel->port_event(hsi_channel->dev,
								event, arg);
			read_unlock(&hsi_channel->rw_lock);
		}
	}
	return 0;
}

static int hsi_clk_event(struct notifier_block *nb, unsigned long event,
			 void *data)
{
/* TODO - implement support for clocks changes
	switch (event) {
	case CLK_PRE_RATE_CHANGE:
		break;
	case CLK_ABORT_RATE_CHANGE:
		break;
	case CLK_POST_RATE_CHANGE:
		break;
	default:
		break;
	}
*/

	/*
	 * TODO: At this point we may emit a port event warning about the
	 * clk frequency change to the upper layers.
	 */
	return NOTIFY_DONE;
}

static void hsi_dev_release(struct device *dev)
{
	/* struct device kfree is already made in unregister_hsi_devices().
	 * Registering this function is necessary to avoid an error from
	 * the device_release() function.
	 */
}

static int __init reg_hsi_dev_ch(struct hsi_dev *hsi_ctrl, unsigned int p,
				 unsigned int ch)
{
	struct hsi_device *dev;
	struct hsi_port *port = &hsi_ctrl->hsi_port[p];
	int err;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->n_ctrl = hsi_ctrl->id;
	dev->n_p = p;
	dev->n_ch = ch;
	dev->ch = &port->hsi_channel[ch];
	dev->device.bus = &hsi_bus_type;
	dev->device.parent = hsi_ctrl->dev;
	dev->device.release = hsi_dev_release;
	if (dev->n_ctrl < 0)
		dev_set_name(&dev->device, "omap_hsi-p%u.c%u", p, ch);
	else
		dev_set_name(&dev->device, "omap_hsi%d-p%u.c%u", dev->n_ctrl, p,
			     ch);

	err = device_register(&dev->device);
	if (err >= 0) {
		write_lock_bh(&port->hsi_channel[ch].rw_lock);
		port->hsi_channel[ch].dev = dev;
		write_unlock_bh(&port->hsi_channel[ch].rw_lock);
	} else {
		kfree(dev);
	}
	return err;
}

static int __init register_hsi_devices(struct hsi_dev *hsi_ctrl)
{
	int port;
	int ch;
	int err;

	for (port = 0; port < hsi_ctrl->max_p; port++)
		for (ch = 0; ch < hsi_ctrl->hsi_port[port].max_ch; ch++) {
			err = reg_hsi_dev_ch(hsi_ctrl, port, ch);
			if (err < 0)
				return err;
		}

	return 0;
}

static int __init hsi_softreset(struct hsi_dev *hsi_ctrl)
{
	int ind = 0;
	void __iomem *base = hsi_ctrl->base;
	u32 status;

	hsi_outl_or(HSI_SOFTRESET, base, HSI_SYS_SYSCONFIG_REG);

	do {
		set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_timeout(msecs_to_jiffies(HSI_RESETDONE_TIMEOUT));
		status = hsi_inl(base, HSI_SYS_SYSSTATUS_REG);
		ind++;
	} while ((!(status & HSI_RESETDONE)) && (ind < HSI_RESETDONE_RETRIES));

	if (ind >= HSI_RESETDONE_RETRIES)
		return -EIO;

	/* Reseting GDD */
	hsi_outl_or(HSI_SWRESET, base, HSI_GDD_GRST_REG);

	return 0;
}

static void __init set_hsi_ports_default(struct hsi_dev *hsi_ctrl,
					 struct platform_device *pd)
{
	struct port_ctx *cfg;
	struct hsi_platform_data *pdata = pd->dev.platform_data;
	unsigned int port = 0;
	void __iomem *base = hsi_ctrl->base;
	struct platform_device *pdev = to_platform_device(hsi_ctrl->dev);

	for (port = 1; port <= pdata->num_ports; port++) {
		cfg = &pdata->ctx.pctx[port - 1];
		hsi_outl(cfg->hst.mode | cfg->hst.flow | HSI_MODE_WAKE_CTRL_SW,
			 base, HSI_HST_MODE_REG(port));
		if (!hsi_driver_device_is_hsi(pdev))
			hsi_outl(cfg->hst.frame_size, base,
						 HSI_HST_FRAMESIZE_REG(port));
		hsi_outl(cfg->hst.divisor, base, HSI_HST_DIVISOR_REG(port));
		hsi_outl(cfg->hst.channels, base, HSI_HST_CHANNELS_REG(port));
		hsi_outl(cfg->hst.arb_mode, base, HSI_HST_ARBMODE_REG(port));

		hsi_outl(cfg->hsr.mode | cfg->hsr.flow, base,
			 HSI_HSR_MODE_REG(port));
		hsi_outl(cfg->hsr.frame_size, base,
			 HSI_HSR_FRAMESIZE_REG(port));
		hsi_outl(cfg->hsr.channels, base, HSI_HSR_CHANNELS_REG(port));
		if (hsi_driver_device_is_hsi(pdev))
			hsi_outl(cfg->hsr.divisor, base,
						HSI_HSR_DIVISOR_REG(port));
		hsi_outl(cfg->hsr.timeout, base, HSI_HSR_COUNTERS_REG(port));
	}

	if (hsi_driver_device_is_hsi(pdev)) {
		/* SW strategy for HSI fifo management can be changed here */
		hsi_fifo_mapping(hsi_ctrl, HSI_FIFO_MAPPING_DEFAULT);
	}
}

static int __init hsi_port_channels_init(struct hsi_port *port)
{
	struct hsi_channel *ch;
	unsigned int ch_i;

	for (ch_i = 0; ch_i < port->max_ch; ch_i++) {
		ch = &port->hsi_channel[ch_i];
		ch->channel_number = ch_i;
		rwlock_init(&ch->rw_lock);
		ch->flags = 0;
		ch->hsi_port = port;
		ch->read_data.addr = NULL;
		ch->read_data.size = 0;
		ch->read_data.lch = -1;
		ch->write_data.addr = NULL;
		ch->write_data.size = 0;
		ch->write_data.lch = -1;
		ch->dev = NULL;
		ch->read_done = NULL;
		ch->write_done = NULL;
		ch->port_event = NULL;
	}

	return 0;
}

static void hsi_ports_exit(struct hsi_dev *hsi_ctrl, unsigned int max_ports)
{
	struct hsi_port *hsi_p;
	unsigned int port;

	for (port = 0; port < max_ports; port++) {
		hsi_p = &hsi_ctrl->hsi_port[port];
		hsi_mpu_exit(hsi_p);
		hsi_cawake_exit(hsi_p);
	}
}

static int __init hsi_request_mpu_irq(struct hsi_port *hsi_p)
{
	struct hsi_dev *hsi_ctrl = hsi_p->hsi_controller;
	struct platform_device *pd = to_platform_device(hsi_ctrl->dev);
	struct resource *mpu_irq;

	if (hsi_driver_device_is_hsi(pd))
		mpu_irq = platform_get_resource(pd, IORESOURCE_IRQ,
					hsi_p->port_number - 1);
	else /* SSI support 2 IRQs per port */
		mpu_irq = platform_get_resource(pd, IORESOURCE_IRQ,
					(hsi_p->port_number - 1) * 2);

	if (!mpu_irq) {
		dev_err(hsi_ctrl->dev, "HSI misses info for MPU IRQ on"
			" port %d\n", hsi_p->port_number);
		return -ENXIO;
	}
	hsi_p->n_irq = 0;	/* We only use one irq line */
	hsi_p->irq = mpu_irq->start;
	return hsi_mpu_init(hsi_p, mpu_irq->name);
}

static int __init hsi_request_cawake_irq(struct hsi_port *hsi_p)
{
	struct hsi_dev *hsi_ctrl = hsi_p->hsi_controller;
	struct platform_device *pd = to_platform_device(hsi_ctrl->dev);
	struct resource *cawake_irq;

	if (hsi_driver_device_is_hsi(pd)) {
		hsi_p->cawake_gpio = -1;
		return 0;
	} else {
		cawake_irq = platform_get_resource(pd, IORESOURCE_IRQ,
					   4 + hsi_p->port_number);
	}

	if (!cawake_irq) {
		dev_err(hsi_ctrl->dev, "SSI device misses info for CAWAKE"
			"IRQ on port %d\n", hsi_p->port_number);
		return -ENXIO;
	}

	if (cawake_irq->flags & IORESOURCE_UNSET) {
		dev_info(hsi_ctrl->dev, "No CAWAKE GPIO support\n");
		hsi_p->cawake_gpio = -1;
		return 0;
	}

	hsi_p->cawake_gpio_irq = cawake_irq->start;
	hsi_p->cawake_gpio = irq_to_gpio(cawake_irq->start);
	return hsi_cawake_init(hsi_p, cawake_irq->name);
}

static int __init hsi_ports_init(struct hsi_dev *hsi_ctrl)
{
	struct platform_device *pd = to_platform_device(hsi_ctrl->dev);
	struct hsi_platform_data *pdata = pd->dev.platform_data;
	struct hsi_port *hsi_p;
	unsigned int port;
	int err;

	for (port = 0; port < hsi_ctrl->max_p; port++) {
		hsi_p = &hsi_ctrl->hsi_port[port];
		hsi_p->port_number = port + 1;
		hsi_p->hsi_controller = hsi_ctrl;
		hsi_p->max_ch = hsi_driver_device_is_hsi(pd) ?
				HSI_CHANNELS_MAX : HSI_SSI_CHANNELS_MAX;
		hsi_p->max_ch = min(hsi_p->max_ch, (u8) HSI_PORT_MAX_CH);
		hsi_p->irq = 0;
		hsi_p->counters_on = 1;
		hsi_p->reg_counters = pdata->ctx.pctx[port].hsr.timeout;
		spin_lock_init(&hsi_p->lock);
		err = hsi_port_channels_init(&hsi_ctrl->hsi_port[port]);
		if (err < 0)
			goto rback1;
		err = hsi_request_mpu_irq(hsi_p);
		if (err < 0)
			goto rback2;
		err = hsi_request_cawake_irq(hsi_p);
		if (err < 0)
			goto rback3;
	}
	return 0;
rback3:
	hsi_mpu_exit(hsi_p);
rback2:
	hsi_ports_exit(hsi_ctrl, port + 1);
rback1:
	return err;
}

static int __init hsi_request_gdd_irq(struct hsi_dev *hsi_ctrl)
{
	struct platform_device *pd = to_platform_device(hsi_ctrl->dev);
	struct resource *gdd_irq;

	if (hsi_driver_device_is_hsi(pd))
		gdd_irq = platform_get_resource(pd, IORESOURCE_IRQ, 2);
	else
		gdd_irq = platform_get_resource(pd, IORESOURCE_IRQ, 4);

	if (!gdd_irq) {
		dev_err(hsi_ctrl->dev, "HSI has no GDD IRQ resource\n");
		return -ENXIO;
	}

	hsi_ctrl->gdd_irq = gdd_irq->start;
	return hsi_gdd_init(hsi_ctrl, gdd_irq->name);
}

static void __init hsi_init_gdd_chan_count(struct hsi_dev *hsi_ctrl)
{
	struct platform_device *pd = to_platform_device(hsi_ctrl->dev);
	struct resource *gdd_chan_count;
	int i;

	gdd_chan_count = platform_get_resource(pd, IORESOURCE_DMA, 0);

	if (!gdd_chan_count) {
		dev_warn(hsi_ctrl->dev, "HSI device has no GDD channel count "
					"resource (use 8 as default)\n");
		hsi_ctrl->gdd_chan_count = 8;
	} else {
		hsi_ctrl->gdd_chan_count = gdd_chan_count->start;
		/* Check that the number of channels is power of 2 */
		for (i = 0; i < 16; i++) {
			if (hsi_ctrl->gdd_chan_count == (1 << i))
				break;
		}
		if (i >= 16)
			dev_err(hsi_ctrl->dev, "The Number of DMA channels "
					"shall be a power of 2! (=%d)\n",
					hsi_ctrl->gdd_chan_count);
	}
}

static int __init hsi_controller_init(struct hsi_dev *hsi_ctrl,
				      struct platform_device *pd)
{
	struct hsi_platform_data *pdata = pd->dev.platform_data;
	struct resource *mem, *ioarea;
	int err;

	mem = platform_get_resource(pd, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pd->dev, "HSI device does not have "
			"HSI IO memory region information\n");
		return -ENXIO;
	}

	ioarea = devm_request_mem_region(&pd->dev, mem->start,
					 (mem->end - mem->start) + 1,
					 dev_name(&pd->dev));
	if (!ioarea) {
		dev_err(&pd->dev, "Unable to request HSI IO mem region\n");
		return -EBUSY;
	}

	hsi_ctrl->phy_base = mem->start;
	hsi_ctrl->base = devm_ioremap(&pd->dev, mem->start,
				      (mem->end - mem->start) + 1);
	if (!hsi_ctrl->base) {
		dev_err(&pd->dev, "Unable to ioremap HSI base IO address\n");
		return -ENXIO;
	}

	hsi_ctrl->id = pd->id;
	if (pdata->num_ports > HSI_MAX_PORTS) {
		dev_err(&pd->dev, "The HSI driver does not support enough "
		"ports!\n");
		return -ENXIO;
	}
	hsi_ctrl->max_p = pdata->num_ports;
	hsi_ctrl->dev = &pd->dev;
	spin_lock_init(&hsi_ctrl->lock);
	hsi_ctrl->hsi_clk = clk_get(&pd->dev, "hsi_fck");
	hsi_init_gdd_chan_count(hsi_ctrl);

	if (IS_ERR(hsi_ctrl->hsi_clk)) {
		dev_err(hsi_ctrl->dev, "Unable to get HSI clocks\n");
		return PTR_ERR(hsi_ctrl->hsi_clk);
	}

	if (pdata->clk_notifier_register) {
		hsi_ctrl->hsi_nb.notifier_call = hsi_clk_event;
		hsi_ctrl->hsi_nb.priority = INT_MAX; /* Let's try to be first */
		err = pdata->clk_notifier_register(hsi_ctrl->hsi_clk,
						   &hsi_ctrl->hsi_nb);
		if (err < 0)
			goto rback1;
	}

	err = hsi_ports_init(hsi_ctrl);
	if (err < 0)
		goto rback2;

	err = hsi_request_gdd_irq(hsi_ctrl);
	if (err < 0)
		goto rback3;

	return 0;
rback3:
	hsi_ports_exit(hsi_ctrl, hsi_ctrl->max_p);
rback2:
	if (pdata->clk_notifier_unregister)
		pdata->clk_notifier_unregister(hsi_ctrl->hsi_clk,
					       &hsi_ctrl->hsi_nb);
rback1:
	clk_put(hsi_ctrl->hsi_clk);
	dev_err(&pd->dev, "Error on hsi_controller initialization\n");
	return err;
}

static void hsi_controller_exit(struct hsi_dev *hsi_ctrl)
{
	struct hsi_platform_data *pdata = hsi_ctrl->dev->platform_data;

	hsi_gdd_exit(hsi_ctrl);
	hsi_ports_exit(hsi_ctrl, hsi_ctrl->max_p);
	if (pdata->clk_notifier_unregister)
		pdata->clk_notifier_unregister(hsi_ctrl->hsi_clk,
					       &hsi_ctrl->hsi_nb);
	clk_put(hsi_ctrl->hsi_clk);
}

static int __init hsi_probe(struct platform_device *pd)
{
	struct hsi_platform_data *pdata = pd->dev.platform_data;
	struct hsi_dev *hsi_ctrl;
	u32 revision;
	int err;

	dev_dbg(&pd->dev, "The platform device probed is an %s\n",
			hsi_driver_device_is_hsi(pd) ? "HSI" : "SSI");

	if (!pdata) {
		pr_err(LOG_NAME "No platform_data found on hsi device\n");
		return -ENXIO;
	}

	hsi_ctrl = kzalloc(sizeof(*hsi_ctrl), GFP_KERNEL);
	if (hsi_ctrl == NULL) {
		dev_err(&pd->dev, "Could not allocate memory for"
			" struct hsi_dev\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pd, hsi_ctrl);
	err = hsi_controller_init(hsi_ctrl, pd);
	if (err < 0) {
		dev_err(&pd->dev, "Could not initialize hsi controller:"
			" %d\n", err);
		goto rollback1;
	}

	clk_enable(hsi_ctrl->hsi_clk);

	err = hsi_softreset(hsi_ctrl);
	if (err < 0)
		goto rollback2;

	/* Set default PM settings */
	hsi_outl((HSI_AUTOIDLE | HSI_SIDLEMODE_SMART | HSI_MIDLEMODE_SMART),
		 hsi_ctrl->base, HSI_SYS_SYSCONFIG_REG);
	hsi_outl(HSI_CLK_AUTOGATING_ON, hsi_ctrl->base, HSI_GDD_GCR_REG);

	/* Configure HSI ports */
	set_hsi_ports_default(hsi_ctrl, pd);

	/* Gather info from registers for the driver.(REVISION) */
	revision = hsi_inl(hsi_ctrl->base, HSI_SYS_REVISION_REG);
	if (hsi_driver_device_is_hsi(pd))
		dev_info(hsi_ctrl->dev, "HSI Hardware REVISION 0x%x\n",
								revision);
	else
		dev_info(hsi_ctrl->dev, "SSI Hardware REVISION %d.%d\n",
					(revision & HSI_SSI_REV_MAJOR) >> 4,
					(revision & HSI_SSI_REV_MINOR));


	err = hsi_debug_add_ctrl(hsi_ctrl);
	if (err < 0)
		goto rollback2;

	err = register_hsi_devices(hsi_ctrl);
	if (err < 0)
		goto rollback3;

	return err;

rollback3:
	hsi_debug_remove_ctrl(hsi_ctrl);
rollback2:
	hsi_controller_exit(hsi_ctrl);
rollback1:
	kfree(hsi_ctrl);
	return err;
}

static void __exit unregister_hsi_devices(struct hsi_dev *hsi_ctrl)
{
	struct hsi_port *hsi_p;
	struct hsi_device *device;
	unsigned int port;
	unsigned int ch;

	for (port = 0; port < hsi_ctrl->max_p; port++) {
		hsi_p = &hsi_ctrl->hsi_port[port];
		for (ch = 0; ch < hsi_p->max_ch; ch++) {
			device = hsi_p->hsi_channel[ch].dev;
			hsi_close(device);
			device_unregister(&device->device);
			kfree(device);
		}
	}
}

static int __exit hsi_remove(struct platform_device *pd)
{
	struct hsi_dev *hsi_ctrl = platform_get_drvdata(pd);

	if (!hsi_ctrl)
		return 0;

	unregister_hsi_devices(hsi_ctrl);
	hsi_debug_remove_ctrl(hsi_ctrl);
	hsi_controller_exit(hsi_ctrl);
	kfree(hsi_ctrl);

	return 0;
}

int hsi_driver_device_is_hsi(struct platform_device *dev)
{
	const struct platform_device_id *id = platform_get_device_id(dev);
	return (id->driver_data == HSI_DRV_DEVICE_HSI);
}

/* List of devices supported by this driver */
static struct platform_device_id hsi_id_table[] = {
	{ "omap_hsi",	HSI_DRV_DEVICE_HSI },
	{ "omap_ssi",	HSI_DRV_DEVICE_SSI },
	{ },
};
MODULE_DEVICE_TABLE(platform, hsi_id_table);

static struct platform_driver hsi_pdriver = {
	.probe = hsi_probe,
	.remove = __exit_p(hsi_remove),
	.driver = {
		   .name = "omap_hsi",
		   .owner = THIS_MODULE,
		   },
	.id_table = hsi_id_table
};

static int __init hsi_driver_init(void)
{
	int err = 0;

	pr_info("HSI DRIVER Version " HSI_DRIVER_VERSION "\n");

	hsi_bus_init();
	err = hsi_debug_init();
	if (err < 0) {
		pr_err(LOG_NAME "HSI Debugfs failed %d\n", err);
		goto rback1;
	}
	err = platform_driver_probe(&hsi_pdriver, hsi_probe);
	if (err < 0) {
		pr_err(LOG_NAME "Platform DRIVER register FAILED: %d\n", err);
		goto rback2;
	}

	/* Set the CM_L3INIT_HSI_CLKCTRL to divide HSI_FCLK @192MHz by 2
	 *  to make the HSI works even if the OPP50 is set
	 * TODO : omap2_clk_set_parent() should be used */
	omap_writel(0x01000001, 0X4A009338);

	return 0;
rback2:
	hsi_debug_exit();
rback1:
	hsi_bus_exit();
	return err;
}

static void __exit hsi_driver_exit(void)
{
	platform_driver_unregister(&hsi_pdriver);
	hsi_debug_exit();
	hsi_bus_exit();

	pr_info("HSI DRIVER removed\n");
}

module_init(hsi_driver_init);
module_exit(hsi_driver_exit);

MODULE_ALIAS("platform:omap_hsi");
MODULE_AUTHOR("Carlos Chinea / Nokia");
MODULE_AUTHOR("Sebastien JAN / Texas Instruments");
MODULE_DESCRIPTION("MIPI High-speed Synchronous Serial Interface (HSI) Driver");
MODULE_LICENSE("GPL");
