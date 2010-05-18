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
#include <linux/hsi_driver_if.h>
#include "clock.h"
#include <asm/clkdev.h>

#define	hsi_inl(p)	inl((unsigned long) p)
#define	hsi_outl(v, p)	outl(v, (unsigned long) p)

/**
 *	struct hsi_internal_clk - Generic virtual hsi clock
 *	@clk: clock data
 *	@nb: notfier block for the DVFS notification chain
 *	@childs: Array of HSI FCK and ICK clocks
 *	@n_childs: Number of clocks in childs array
 *	@rate_change: Tracks if we are in the middle of a clock rate change
 *	@pdev: Reference to the HSI platform device associated to the clock
 *	@drv_nb: Reference to driver nb, use to propagate the DVFS notification
 */
struct hsi_internal_clk {
	struct clk clk;
	struct notifier_block nb;

	struct clk **childs;
	int n_childs;

	unsigned int rate_change:1;

	struct platform_device *pdev;
	struct notifier_block *drv_nb;
};

/* OMAP_HSI_EXAMPLE_PWR_CODE flag used to surround code cloned from ssi driver
 * but not reworked for HSI/4430.
 * Kept here for reference and support for implementation
 */
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
#undef OMAP_HSI_EXAMPLE_PWR_CODE
#endif

#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
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
#endif

static int hsi_clk_event(struct notifier_block *nb, unsigned long event,
								void *data)
{
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	struct hsi_internal_clk *hsi_clk =
				container_of(nb, struct hsi_internal_clk, nb);
	switch (event) {
	case CLK_PRE_RATE_CHANGE:
		hsi_clk->drv_nb->notifier_call(hsi_clk->drv_nb, event, data);
		hsi_clk->rate_change = 1;
		if (hsi_clk->clk.usecount > 0) {
			hsi_save_mode(hsi_clk->pdev);
			hsi_set_mode(hsi_clk->pdev, HSI_MODE_SLEEP);
		}
		break;
	case CLK_ABORT_RATE_CHANGE:
	case CLK_POST_RATE_CHANGE:
		if ((hsi_clk->clk.usecount > 0) && (hsi_clk->rate_change))
			hsi_restore_mode(hsi_clk->pdev);

		hsi_clk->rate_change = 0;
		hsi_clk->drv_nb->notifier_call(hsi_clk->drv_nb, event, data);
		break;
	default:
		break;
	}
#endif
	return NOTIFY_DONE;
}

static int hsi_clk_notifier_register(struct clk *clk, struct notifier_block *nb)
{
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	struct hsi_internal_clk *hsi_clk;
	if (!clk || !nb)
		return -EINVAL;
	hsi_clk = container_of(clk, struct hsi_internal_clk, clk);
	hsi_clk->drv_nb = nb;
	hsi_clk->nb.priority = nb->priority;

	/* NOTE: We only want notifications from the functional clock */
	return clk_notifier_register(hsi_clk->childs[1], &hsi_clk->nb);

	pr_debug("%s called\n", __func__);
#endif
	return 0;
}

static int hsi_clk_notifier_unregister(struct clk *clk,
						struct notifier_block *nb)
{
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	struct hsi_internal_clk *hsi_clk;

	if (!clk || !nb)
		return -EINVAL;

	hsi_clk = container_of(clk, struct hsi_internal_clk, clk);
	hsi_clk->drv_nb = NULL;

	return clk_notifier_unregister(hsi_clk->childs[1], &hsi_clk->nb);

#endif
	pr_debug(KERN_DEBUG "%s called", __func__);
	return 0;
}

#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
static void hsi_save_ctx(struct platform_device *pdev)
{
	struct hsi_platform_data *pdata = pdev->dev.platform_data;
	void __iomem *base = OMAP2_IO_ADDRESS(pdev->resource[0].start);
	struct port_ctx *p;
	int port;

	pdata->ctx.loss_count =
			omap_pm_get_dev_context_loss_count(&pdev->dev);
	pdata->ctx.sysconfig = hsi_inl(base + HSI_SYS_SYSCONFIG_REG);
	pdata->ctx.gdd_gcr = hsi_inl(base + HSI_GDD_GCR_REG);
	for (port = 1; port <= pdata->num_ports; port++) {
		p = &pdata->ctx.pctx[port - 1];
		p->sys_mpu_enable[0] = hsi_inl(base +
					HSI_SYS_MPU_ENABLE_REG(port, 0));
		p->sys_mpu_enable[1] = hsi_inl(base +
					HSI_SYS_MPU_U_ENABLE_REG(port, 0));
		p->hst.frame_size = hsi_inl(base +
						HSI_HST_FRAMESIZE_REG(port));
		p->hst.divisor = hsi_inl(base + HSI_HST_DIVISOR_REG(port));
		p->hst.channels = hsi_inl(base + HSI_HST_CHANNELS_REG(port));
		p->hst.arb_mode = hsi_inl(base + HSI_HST_ARBMODE_REG(port));
		p->hsr.frame_size = hsi_inl(base +
						HSI_HSR_FRAMESIZE_REG(port));
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
		hsi_outl(p->hst.frame_size, base +
						HSI_HST_FRAMESIZE_REG(port));
		hsi_outl(p->hst.divisor, base + HSI_HST_DIVISOR_REG(port));
		hsi_outl(p->hst.channels, base + HSI_HST_CHANNELS_REG(port));
		hsi_outl(p->hst.arb_mode, base + HSI_HST_ARBMODE_REG(port));
		hsi_outl(p->hsr.frame_size, base +
						HSI_HSR_FRAMESIZE_REG(port));
/* FIXME - check this register */
		hsi_outl(p->hsr.timeout, base + HSI_HSR_COUNTERS_REG(port));
		hsi_outl(p->hsr.channels, base + HSI_HSR_CHANNELS_REG(port));
	}
}
#endif

static void hsi_pdev_release(struct device *dev)
{
}

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
				HSI_COUNTERS_TB_DEFAULT |
				HSI_COUNTERS_FB_DEFAULT,
		},
};

static struct hsi_platform_data hsi_pdata = {
	.num_ports = ARRAY_SIZE(hsi_port_ctx),
	.ctx.pctx = hsi_port_ctx,
	.clk_notifier_register = hsi_clk_notifier_register,
	.clk_notifier_unregister = hsi_clk_notifier_unregister,
};

static struct resource hsi_resources[] = {
	[0] =	{
		.start = 0x4A058000,
		.end = 0x4A05b950,
		.name = "omap_hsi_iomem",
		.flags = IORESOURCE_MEM,
		},
	[1] =	{
		.start = OMAP44XX_IRQ_HSI_P1,
		.end = OMAP44XX_IRQ_HSI_P1,
		.name = "hsi_p1_mpu_irq0",
		.flags = IORESOURCE_IRQ,
		},
	[2] =	{
		.start = OMAP44XX_IRQ_HSI_P2,
		.end = OMAP44XX_IRQ_HSI_P2,
		.name = "hsi_p2_mpu_irq0",
		.flags = IORESOURCE_IRQ,
		},
	[3] =	{
		.start = OMAP44XX_IRQ_HSI_DMA,
		.end = OMAP44XX_IRQ_HSI_DMA,
		.name = "hsi_gdd",
		.flags = IORESOURCE_IRQ,
		},
	[4] =	{
		.start = 16, /* DMA channels available */
		.end = 16,
		.name = "hsi_gdd_chan_count",
		.flags = IORESOURCE_DMA,
		},
};

static struct platform_device hsi_pdev = {
	.name = "omap_hsi",
	.id = -1,
	.num_resources = ARRAY_SIZE(hsi_resources),
	.resource = hsi_resources,
	.dev =	{
		.release = hsi_pdev_release,
		.platform_data = &hsi_pdata,
		},
};

#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
#define __HSI_CLK_FIX__
#ifdef __HSI_CLK_FIX__
/*
 * FIXME: TO BE REMOVED.
 * This hack allows us to ensure that clocks are stable before accehsing
 * HSI controller registers. To be removed when PM functionalty is in place.
 */
static int check_hsi_active(void)
{
	u32 reg;
	unsigned long dl = jiffies + msecs_to_jiffies(500);
	void __iomem *cm_idlest1 = OMAP2_IO_ADDRESS(0x48004a20);

	reg = inl(cm_idlest1);
	while ((!(reg & 0x01)) && (time_before(jiffies, dl)))
		reg = inl(cm_idlest1);

	if (!(reg & 0x01)) { /* HST */
		pr_err("HSI is still in STANDBY ! (BUG !?)\n");
		return -1;
	}

	return 0;
}
#endif /* __HSI_CLK_FIX__ */
#endif

static int hsi_clk_init(struct hsi_internal_clk *hsi_clk)
{
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
/* FIXME - update clock names on OMAP4*/
	const char *clk_names[] = {};

	int i;
	int j;

	hsi_clk->n_childs = ARRAY_SIZE(clk_names);
	hsi_clk->childs = kzalloc(hsi_clk->n_childs * sizeof(*hsi_clk->childs),
								GFP_KERNEL);
	if (!hsi_clk->childs)
		return -ENOMEM;

	for (i = 0; i < hsi_clk->n_childs; i++) {
		hsi_clk->childs[i] = clk_get(&hsi_clk->pdev->dev, clk_names[i]);
		if (IS_ERR(hsi_clk->childs[i])) {
			pr_err("Unable to get HSI clock: %s", clk_names[i]);
			for (j = i - 1; j >= 0; j--)
				clk_put(hsi_clk->childs[j]);
			return -ENODEV;
		}
	}

#endif
	return 0;
}



static int hsi_clk_enable(struct clk *clk)
{
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	struct hsi_internal_clk *hsi_clk =
				container_of(clk, struct hsi_internal_clk, clk);
	int err;
	int i;

	for (i = 0; i < hsi_clk->n_childs; i++) {
		err = omap2_clk_enable(hsi_clk->childs[i]);
		if (unlikely(err < 0))
			goto rollback;
	}

#ifdef __HSI_CLK_FIX__
	/*
	 * FIXME: To be removed
	 * Wait until the HSI controller has the clocks stable
	 */
	check_hsi_active();
#endif
	hsi_restore_ctx(hsi_clk->pdev);
	if (!hsi_clk->rate_change)
		hsi_restore_mode(hsi_clk->pdev);

#endif
	return 0;
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
rollback:
	pr_err("Error on HSI clk child %d\n", i);
	for (i = i - 1; i >= 0; i--)
		omap2_clk_disable(hsi_clk->childs[i]);
	return err;
#endif
}

static void hsi_clk_disable(struct clk *clk)
{
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	struct hsi_internal_clk *hsi_clk =
				container_of(clk, struct hsi_internal_clk, clk);
	int i;

	if (!hsi_clk->rate_change) {
		hsi_save_mode(hsi_clk->pdev);
		hsi_set_mode(hsi_clk->pdev, HSI_MODE_SLEEP);
	}
	/* Save ctx in all ports */
	hsi_save_ctx(hsi_clk->pdev);

	for (i = 0; i < hsi_clk->n_childs; i++)
		omap2_clk_disable(hsi_clk->childs[i]);

#endif
}

/*
static const int omap44xx_hsi_pins[] = {
	AE18_4430_HSI1_CAWAKE,
	AG19_4430_HSI1_CADATA,
	AF19_4430_HSI1_CAFLAG,
	AE19_4430_HSI1_ACREADY,
	AF18_4430_HSI1_ACWAKE,
	AG18_4430_HSI1_ACDATA,
	AE17_4430_HSI1_ACFLAG,
	AF17_4430_HSI1_CAREADY,
};
*/
/* Mux settings for OMAP4430 */
void omap_hsi_mux_setup(void)
{
/*	int i;
	for (i = 0; i < ARRAY_SIZE(omap44xx_hsi_pins); i++)
		omap_cfg_reg(omap44xx_hsi_pins[i]);
	pr_debug("Pin muxing for HSI support done\n");
*/}

static const struct clkops clkops_hsi = {
	.enable		= hsi_clk_enable,
	.disable	= hsi_clk_disable,
};

static struct hsi_internal_clk hsi_clock = {
	.clk = {
		.name = "hsi_clk",
		.clkdm_name = "core_l4_clkdm",
		.ops = &clkops_hsi,
	},
	.nb = {
		.notifier_call = hsi_clk_event,
		.priority = INT_MAX,
	},
	.pdev = &hsi_pdev,
};

#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
static struct clk_lookup hsi_lk = {
	.dev_id = NULL,
	.con_id = "hsi_clk",
	.clk = &hsi_clock.clk,
};
#endif

static int __init omap_hsi_init(void)
{
	int err;
	struct clk *hsi_clk = &hsi_clock.clk;

	hsi_clk_init(&hsi_clock);
	clk_preinit(hsi_clk);
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	clkdev_add(&hsi_lk);
#endif
	clk_register(hsi_clk);
#ifdef OMAP_HSI_EXAMPLE_PWR_CODE
	omap2_init_clk_clkdm(hsi_clk);
#endif
	err = platform_device_register(&hsi_pdev);
	if (err < 0) {
		pr_err("Unable to register HSI platform device: %d\n", err);
		return err;
	}

	omap_hsi_mux_setup();

	pr_info("HSI: device registered\n");
	return 0;
}
subsys_initcall(omap_hsi_init);
