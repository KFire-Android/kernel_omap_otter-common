/*
 * arch/arm/mach-omap2/hsi.c
 *
 * HSI device definition
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
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
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/notifier.h>
#include <mach/omap_hsi.h>
#include <plat/omap_hwmod.h>
#include <plat/omap_device.h>
#include <linux/hsi_driver_if.h>
#include "clock.h"
#include <asm/clkdev.h>

#define OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME  "omap_hsi"
#define OMAP_HSI_HWMOD_NAME  "hsi"
#define OMAP_HSI_HWMOD_CLASSNAME  "hsi"

#define	hsi_inl(p)	inl((unsigned long) p)
#define	hsi_outl(v, p)	outl(v, (unsigned long) p)

#ifdef HSI_CONTEXT_SAVE_RESTORE
static void hsi_set_mode(struct platform_device *pdev, u32 mode)
{
	struct hsi_platform_data *pdata = pdev->dev.platform_data;
	void __iomem *base = OMAP2_IO_ADDRESS(pdev->resource[0].start);
	int port;

	for (port = 1; port <= pdata->num_ports; port++) {
		/* FIXME - to update: need read/modify/write or something else:
		 * this register now also contains flow and wake ctrl
		 */
		hsi_outl(mode, base + HSI_HST_MODE_REG(port));
		hsi_outl(mode, base + HSI_HSR_MODE_REG(port));
	}
}

static void hsi_save_mode(struct platform_device *pdev)
{
	struct hsi_platform_data *pdata = pdev->dev.platform_data;
	void __iomem *base = OMAP2_IO_ADDRESS(pdev->resource[0].start);
	struct port_ctx *p;
	int port;

	for (port = 1; port <= pdata->num_ports; port++) {
		p = &pdata->ctx.pctx[port - 1];
		p->hst.mode = hsi_inl(base + HSI_HST_MODE_REG(port));
		p->hsr.mode = hsi_inl(base + HSI_HSR_MODE_REG(port));
	}
}

static void hsi_restore_mode(struct platform_device *pdev)
{
	struct hsi_platform_data *pdata = pdev->dev.platform_data;
	void __iomem *base = OMAP2_IO_ADDRESS(pdev->resource[0].start);
	struct port_ctx *p;
	int port;

	for (port = 1; port <= pdata->num_ports; port++) {
		p = &pdata->ctx.pctx[port - 1];
		hsi_outl(p->hst.mode, base + HSI_HST_MODE_REG(port));
		hsi_outl(p->hsr.mode, base + HSI_HSR_MODE_REG(port));
	}
}
#endif /* HSI_CONTEXT_SAVE_RESTORE */

#ifdef HSI_CONTEXT_SAVE_RESTORE
static void hsi_save_ctx(struct platform_device *pdev)
{
	struct hsi_platform_data *pdata = pdev->dev.platform_data;
	void __iomem *base = OMAP2_IO_ADDRESS(pdev->resource[0].start);
	struct port_ctx *p;
	int port;

	pdata->ctx.loss_count = omap_pm_get_dev_context_loss_count(&pdev->dev);
	pdata->ctx.sysconfig = hsi_inl(base + HSI_SYS_SYSCONFIG_REG);
	pdata->ctx.gdd_gcr = hsi_inl(base + HSI_GDD_GCR_REG);
	for (port = 1; port <= pdata->num_ports; port++) {
		p = &pdata->ctx.pctx[port - 1];
		p->sys_mpu_enable[0] = hsi_inl(base +
					       HSI_SYS_MPU_ENABLE_REG(port, 0));
		p->sys_mpu_enable[1] = hsi_inl(base +
					       HSI_SYS_MPU_U_ENABLE_REG(port,
									0));
		p->hst.frame_size = hsi_inl(base + HSI_HST_FRAMESIZE_REG(port));
		p->hst.divisor = hsi_inl(base + HSI_HST_DIVISOR_REG(port));
		p->hst.channels = hsi_inl(base + HSI_HST_CHANNELS_REG(port));
		p->hst.arb_mode = hsi_inl(base + HSI_HST_ARBMODE_REG(port));
		p->hsr.frame_size = hsi_inl(base + HSI_HSR_FRAMESIZE_REG(port));
/*FIXME - check this register*/
		p->hsr.timeout = hsi_inl(base + HSI_HSR_COUNTERS_REG(port));
		p->hsr.channels = hsi_inl(base + HSI_HSR_CHANNELS_REG(port));
	}
}

static void hsi_restore_ctx(struct platform_device *pdev)
{
	struct hsi_platform_data *pdata = pdev->dev.platform_data;
	void __iomem *base = OMAP2_IO_ADDRESS(pdev->resource[0].start);
	struct port_ctx *p;
	int port;
	int loss_count;

	loss_count = omap_pm_get_dev_context_loss_count(&pdev->dev);

	if (loss_count == pdata->ctx.loss_count)
		return;

	hsi_outl(pdata->ctx.sysconfig, base + HSI_SYS_SYSCONFIG_REG);
	hsi_outl(pdata->ctx.gdd_gcr, base + HSI_GDD_GCR_REG);
	for (port = 1; port <= pdata->num_ports; port++) {
		p = &pdata->ctx.pctx[port - 1];
		hsi_outl(p->sys_mpu_enable[0], base +
			 HSI_SYS_MPU_ENABLE_REG(port, 0));
		hsi_outl(p->sys_mpu_enable[1], base +
			 HSI_SYS_MPU_U_ENABLE_REG(port, 0));
		hsi_outl(p->hst.frame_size, base + HSI_HST_FRAMESIZE_REG(port));
		hsi_outl(p->hst.divisor, base + HSI_HST_DIVISOR_REG(port));
		hsi_outl(p->hst.channels, base + HSI_HST_CHANNELS_REG(port));
		hsi_outl(p->hst.arb_mode, base + HSI_HST_ARBMODE_REG(port));
		hsi_outl(p->hsr.frame_size, base + HSI_HSR_FRAMESIZE_REG(port));
/* FIXME - check this register */
		hsi_outl(p->hsr.timeout, base + HSI_HSR_COUNTERS_REG(port));
		hsi_outl(p->hsr.channels, base + HSI_HSR_CHANNELS_REG(port));
	}
}
#endif /* HSI_CONTEXT_SAVE_RESTORE */

/*
 * NOTE: We abuse a little bit the struct port_ctx to use it also for
 * initialization.
 */
static struct port_ctx hsi_port_ctx[] = {
	[0] = {
	       .hst.mode = HSI_MODE_FRAME,
	       .hst.flow = HSI_FLOW_SYNCHRONIZED,
	       .hst.frame_size = HSI_FRAMESIZE_DEFAULT,
	       .hst.divisor = 1,
	       .hst.channels = HSI_CHANNELS_DEFAULT,
	       .hst.arb_mode = HSI_ARBMODE_ROUNDROBIN,
	       .hsr.mode = HSI_MODE_FRAME,
	       .hsr.flow = HSI_FLOW_SYNCHRONIZED,
	       .hsr.frame_size = HSI_FRAMESIZE_DEFAULT,
	       .hsr.channels = HSI_CHANNELS_DEFAULT,
	       .hsr.divisor = 1,
	       .hsr.timeout = HSI_COUNTERS_FT_DEFAULT |
	       HSI_COUNTERS_TB_DEFAULT | HSI_COUNTERS_FB_DEFAULT,
	       },
};

static struct hsi_platform_data omap_hsi_platform_data = {
	.num_ports = ARRAY_SIZE(hsi_port_ctx),
	.ctx.pctx = hsi_port_ctx,
	.device_enable = omap_device_enable,
	.device_idle = omap_device_idle,
	.device_shutdown = omap_device_shutdown,
};

/* HSI_TODO : This requires some fine tuning & completion of
 * activate/deactivate latency values
 */
static struct omap_device_pm_latency omap_hsi_latency[] = {
	[0] = {
	       .deactivate_func = omap_device_idle_hwmods,
	       .activate_func = omap_device_enable_hwmods,
	       .flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	       },
};

/* HSI device registration */
static int __init omap_hsi_init(struct omap_hwmod *oh, void *user)
{
	struct omap_device *od;
	struct hsi_platform_data *pdata = &omap_hsi_platform_data;

	if (!oh) {
		pr_err("Could not look up %s omap_hwmod\n",
		       OMAP_HSI_HWMOD_NAME);
		return -EEXIST;
	}

	od = omap_device_build(OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME, 0, oh,
			       pdata, sizeof(*pdata), omap_hsi_latency,
			       ARRAY_SIZE(omap_hsi_latency), false);
	WARN(IS_ERR(od), "Cant build omap_device for %s:%s.\n",
	     OMAP_HSI_PLATFORM_DEVICE_DRIVER_NAME, oh->name);

	pr_info("HSI: device registered\n");
	return 0;
}

/* HSI devices registration */
static int __init omap_init_hsi(void)
{
	/* Keep this for genericity, although there is only one hwmod for HSI */
	return omap_hwmod_for_each_by_class(OMAP_HSI_HWMOD_CLASSNAME,
					    omap_hsi_init, NULL);
}

/* HSI_TODO : maybe change the prio, eg. use arch_initcall() */
subsys_initcall(omap_init_hsi);
