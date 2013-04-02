/*
 * ehci-omap.c - driver for USBHOST on OMAP3/4 processors
 *
 * Bus Glue for the EHCI controllers in OMAP3/4
 * Tested on several OMAP3 boards, and OMAP4 Pandaboard
 *
 * Copyright (C) 2007-2013 Texas Instruments, Inc.
 *	Author: Vikram Pandita <vikram.pandita@ti.com>
 *	Author: Anand Gadiyar <gadiyar@ti.com>
 *	Author: Keshava Munegowda <keshava_mgowda@ti.com>
 *	Author: Roger Quadros <rogerq@ti.com>
 *
 * Copyright (C) 2009 Nokia Corporation
 *	Contact: Felipe Balbi <felipe.balbi@nokia.com>
 *
 * Based on "ehci-fsl.c" and "ehci-au1xxx.c" ehci glue layers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * TODO (last updated Feb 27, 2010):
 *	- add kernel-doc
 *	- enable AUTOIDLE
 *	- add suspend/resume
 *	- add HSIC and TLL support
 *	- convert to use hwmod and runtime PM
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/ulpi.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/usb.h>
#include <linux/usb/hcd.h>
#include <linux/of.h>
#include <linux/dma-mapping.h>

#include "ehci.h"

#include <linux/platform_data/usb-omap.h>

/* EHCI Register Set */
#define EHCI_INSNREG04					(0xA0)
#define EHCI_INSNREG04_DISABLE_UNSUSPEND		(1 << 5)
#define	EHCI_INSNREG05_ULPI				(0xA4)
#define	EHCI_INSNREG05_ULPI_CONTROL_SHIFT		31
#define	EHCI_INSNREG05_ULPI_PORTSEL_SHIFT		24
#define	EHCI_INSNREG05_ULPI_OPSEL_SHIFT			22
#define	EHCI_INSNREG05_ULPI_REGADD_SHIFT		16
#define	EHCI_INSNREG05_ULPI_EXTREGADD_SHIFT		8
#define	EHCI_INSNREG05_ULPI_WRDATA_SHIFT		0

#define DRIVER_DESC "OMAP-EHCI Host Controller driver"

static const char hcd_name[] = "ehci-omap";

/*-------------------------------------------------------------------------*/

struct omap_hcd {
	struct usb_phy *phy[OMAP3_HS_USB_PORTS]; /* one PHY for each port */
	int nports;
};

static inline void ehci_write(void __iomem *base, u32 reg, u32 val)
{
	__raw_writel(val, base + reg);
}

static inline u32 ehci_read(void __iomem *base, u32 reg)
{
	return __raw_readl(base + reg);
}

static int omap_ehci_init(struct usb_hcd *hcd)
{
	struct ehci_hcd	*ehci = hcd_to_ehci(hcd);
	int rc;

	/* we know this is the memory we want, no need to ioremap again */
	ehci->caps = hcd->regs;

	rc = ehci_setup(hcd);

	return rc;
}

/* configure so an HC device and id are always provided */
/* always called with process context; sleeping is OK */

static struct hc_driver __read_mostly ehci_omap_hc_driver;

static const struct ehci_driver_overrides ehci_omap_overrides __initdata = {
	.reset = omap_ehci_init,
	.extra_priv_size = sizeof(struct omap_hcd),
};

static u64 omap_ehci_dma_mask = DMA_BIT_MASK(32);

/**
 * ehci_hcd_omap_probe - initialize TI-based HCDs
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 */
static int ehci_hcd_omap_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usbhs_omap_platform_data *pdata = dev->platform_data;
	struct resource	*res;
	struct usb_hcd	*hcd;
	void __iomem *regs;
	int ret = -ENODEV;
	int irq;
	int i;
	struct omap_hcd	*omap;

	if (usb_disabled())
		return -ENODEV;

	if (!dev->parent) {
		dev_err(dev, "Missing parent device\n");
		return -ENODEV;
	}

	/* For DT boot, get platform data from parent. i.e. usbhshost */
	if (dev->of_node) {
		pdata = dev->parent->platform_data;
		dev->platform_data = pdata;
	}

	if (!pdata) {
		dev_err(dev, "Missing platform data\n");
		return -ENODEV;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(dev, "EHCI irq failed\n");
		return -ENODEV;
	}

	res =  platform_get_resource(pdev, IORESOURCE_MEM, 0);
	regs = devm_request_and_ioremap(dev, res);
	if (!regs) {
		dev_err(dev, "Resource request/ioremap failed\n");
		return -EADDRNOTAVAIL;
	}

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we have dma capability bindings this can go away.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &omap_ehci_dma_mask;

	hcd = usb_create_hcd(&ehci_omap_hc_driver, dev,
			dev_name(dev));
	if (!hcd) {
		dev_err(dev, "Failed to create HCD\n");
		return -ENOMEM;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);
	hcd->regs = regs;

	omap = (struct omap_hcd *)hcd_to_ehci(hcd)->priv;
	omap->nports = pdata->nports;

	platform_set_drvdata(pdev, hcd);

	/* get the PHY devices if needed */
	for (i = 0 ; i < omap->nports ; i++) {
		struct usb_phy *phy;

		/* get the PHY device */
		if (dev->of_node)
			phy = devm_usb_get_phy_by_phandle(dev, "phys", i);
		else
			phy = devm_usb_get_phy_dev(dev, i);
		if (IS_ERR(phy) || !phy) {
			/* Don't bail out if PHY is not absolutely necessary */
			if (pdata->port_mode[i] != OMAP_EHCI_PORT_MODE_PHY)
				continue;

			ret = IS_ERR(phy) ? PTR_ERR(phy) : -ENODEV;
			dev_err(dev, "Can't get PHY device for port %d: %d\n",
					i, ret);
			goto err_phy;
		}

		omap->phy[i] = phy;
	}

	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);

	/*
	 * An undocumented "feature" in the OMAP3 EHCI controller,
	 * causes suspended ports to be taken out of suspend when
	 * the USBCMD.Run/Stop bit is cleared (for example when
	 * we do ehci_bus_suspend).
	 * This breaks suspend-resume if the root-hub is allowed
	 * to suspend. Writing 1 to this undocumented register bit
	 * disables this feature and restores normal behavior.
	 */
	ehci_write(regs, EHCI_INSNREG04,
				EHCI_INSNREG04_DISABLE_UNSUSPEND);

	ret = usb_add_hcd(hcd, irq, IRQF_SHARED);
	if (ret) {
		dev_err(dev, "failed to add hcd with err %d\n", ret);
		goto err_pm_runtime;
	}

	/* Bring PHYs out of reset */
	for (i = 0; i < omap->nports; i++) {
		if (!omap->phy[i])
			continue;

		usb_phy_init(omap->phy[i]);
		/* bring PHY out of suspend */
		usb_phy_set_suspend(omap->phy[i], 0);
	}

	return 0;

err_pm_runtime:
	pm_runtime_put_sync(dev);

err_phy:
	for (i = 0; i < omap->nports; i++) {
		if (omap->phy[i])
			usb_phy_shutdown(omap->phy[i]);
	}

	usb_put_hcd(hcd);

	return ret;
}


/**
 * ehci_hcd_omap_remove - shutdown processing for EHCI HCDs
 * @pdev: USB Host Controller being removed
 *
 * Reverses the effect of usb_ehci_hcd_omap_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 */
static int ehci_hcd_omap_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct usb_hcd *hcd = dev_get_drvdata(dev);
	struct omap_hcd *omap = (struct omap_hcd *)hcd_to_ehci(hcd)->priv;
	int i;

	usb_remove_hcd(hcd);

	for (i = 0; i < omap->nports; i++) {
		if (omap->phy[i])
			usb_phy_shutdown(omap->phy[i]);
	}

	usb_put_hcd(hcd);
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);

	return 0;
}

static void ehci_hcd_omap_shutdown(struct platform_device *pdev)
{
	struct usb_hcd *hcd = dev_get_drvdata(&pdev->dev);

	if (hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static const struct of_device_id omap_ehci_dt_ids[] = {
	{ .compatible = "ti,ehci-omap" },
	{ }
};

MODULE_DEVICE_TABLE(of, omap_ehci_dt_ids);

static struct platform_driver ehci_hcd_omap_driver = {
	.probe			= ehci_hcd_omap_probe,
	.remove			= ehci_hcd_omap_remove,
	.shutdown		= ehci_hcd_omap_shutdown,
	/*.suspend		= ehci_hcd_omap_suspend, */
	/*.resume		= ehci_hcd_omap_resume, */
	.driver = {
		.name		= hcd_name,
		.of_match_table = of_match_ptr(omap_ehci_dt_ids),
	}
};

/*-------------------------------------------------------------------------*/

static int __init ehci_omap_init(void)
{
	if (usb_disabled())
		return -ENODEV;

	pr_info("%s: " DRIVER_DESC "\n", hcd_name);

	ehci_init_driver(&ehci_omap_hc_driver, &ehci_omap_overrides);
	return platform_driver_register(&ehci_hcd_omap_driver);
}
module_init(ehci_omap_init);

static void __exit ehci_omap_cleanup(void)
{
	platform_driver_unregister(&ehci_hcd_omap_driver);
}
module_exit(ehci_omap_cleanup);

MODULE_ALIAS("platform:ehci-omap");
MODULE_AUTHOR("Texas Instruments, Inc.");
MODULE_AUTHOR("Felipe Balbi <felipe.balbi@nokia.com>");

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
