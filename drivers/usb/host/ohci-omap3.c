/*
 * ohci-omap3.c - driver for OHCI on OMAP3 and later processors
 *
 * Bus Glue for OMAP3 USBHOST 3 port OHCI controller
 * This controller is also used in later OMAPs and AM35x chips
 *
 * Copyright (C) 2007-2010 Texas Instruments, Inc.
 * Author: Vikram Pandita <vikram.pandita@ti.com>
 * Author: Anand Gadiyar <gadiyar@ti.com>
 *
 * Based on ehci-omap.c and some other ohci glue layers
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
 * TODO (last updated Mar 10th, 2010):
 *	- add kernel-doc
 *	- Factor out code common to EHCI to a separate file
 *	- Make EHCI and OHCI coexist together
 *	  - needs newer silicon versions to actually work
 *	  - the last one to be loaded currently steps on the other's toes
 *	- Add hooks for configuring transceivers, etc. at init/exit
 *	- Add aggressive clock-management code
 */

#include <linux/platform_device.h>
#include <linux/clk.h>

#include <plat/usb.h>

/*-------------------------------------------------------------------------*/

struct ohci_hcd_omap3 {
	struct ohci_hcd		*ohci;
	struct device		*dev;
	struct usbhs_omap_resource res;
	struct usbhs_omap_platform_data platdata;
};

/*-------------------------------------------------------------------------*/

static int ohci_omap3_init(struct usb_hcd *hcd)
{
	dev_dbg(hcd->self.controller, "starting OHCI controller\n");

	return ohci_init(hcd_to_ohci(hcd));
}

/*-------------------------------------------------------------------------*/

static int ohci_omap3_start(struct usb_hcd *hcd)
{
	struct ohci_hcd *ohci = hcd_to_ohci(hcd);
	int ret;

	/*
	 * RemoteWakeupConnected has to be set explicitly before
	 * calling ohci_run. The reset value of RWC is 0.
	 */
	ohci->hc_control = OHCI_CTRL_RWC;
	writel(OHCI_CTRL_RWC, &ohci->regs->control);

	ret = ohci_run(ohci);

	if (ret < 0) {
		dev_err(hcd->self.controller, "can't start\n");
		ohci_stop(hcd);
	}

	return ret;
}


static int ohci_omap3_bus_suspend(struct usb_hcd *hcd)
{
	struct device *dev = hcd->self.controller;
	struct uhhtll_apis *uhhtllp = dev->platform_data;

	int ret;

	ret = ohci_bus_suspend(hcd);

	/* Delay required so that after ohci suspend
	 * smart stand by can be set in the driver.
	 * required for power mangament
	 */
	msleep(5);

	if (ret != 0)
		dev_dbg(dev, "ohci_omap3_bus_suspend failed %d\n", ret);


	if (!ret && uhhtllp && uhhtllp->suspend)
		ret = uhhtllp->suspend(OMAP_OHCI);

	return ret;
}


static int ohci_omap3_bus_resume(struct usb_hcd *hcd)
{
	struct device *dev = hcd->self.controller;
	struct uhhtll_apis *uhhtllp = dev->platform_data;
	int ret = 0;

	if (uhhtllp && uhhtllp->resume)
		ret = uhhtllp->resume(OMAP_OHCI);

	if (!ret)
		ohci_bus_resume(hcd);

	return ret;
}


/*-------------------------------------------------------------------------*/

static const struct hc_driver ohci_omap3_hc_driver = {
	.description =		hcd_name,
	.product_desc =		"OMAP3 OHCI Host Controller",
	.hcd_priv_size =	sizeof(struct ohci_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq =			ohci_irq,
	.flags =		HCD_USB11 | HCD_MEMORY,

	/*
	 * basic lifecycle operations
	 */
	.reset =		ohci_omap3_init,
	.start =		ohci_omap3_start,
	.stop =			ohci_stop,
	.shutdown =		ohci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue =		ohci_urb_enqueue,
	.urb_dequeue =		ohci_urb_dequeue,
	.endpoint_disable =	ohci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number =	ohci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data =	ohci_hub_status_data,
	.hub_control =		ohci_hub_control,
#ifdef	CONFIG_PM
	.bus_suspend =		ohci_omap3_bus_suspend,
	.bus_resume =		ohci_omap3_bus_resume,
#endif
	.start_port_reset =	ohci_start_port_reset,
};

/*-------------------------------------------------------------------------*/

/*
 * configure so an HC device and id are always provided
 * always called with process context; sleeping is OK
 */

/**
 * ohci_hcd_omap3_probe - initialize OMAP-based HCDs
 *
 * Allocates basic resources for this USB host controller, and
 * then invokes the start() method for the HCD associated with it
 * through the hotplug entry's driver_data.
 */
static int __devinit ohci_hcd_omap3_probe(struct platform_device *pdev)
{
	struct uhhtll_apis *uhhtllp = pdev->dev.platform_data;
	struct ohci_hcd_omap3 *omap;
	struct usbhs_omap_platform_data *pdata;
	struct usbhs_omap_resource *omapresp;
	struct usb_hcd *hcd;
	int ret = -ENODEV;

	if (!uhhtllp) {
		dev_dbg(&pdev->dev, "missing platform_data\n");
		goto err_end;
	}

	if (usb_disabled())
		goto err_end;

	omap = kzalloc(sizeof(*omap), GFP_KERNEL);
	if (!omap) {
		ret = -ENOMEM;
		goto err_end;
	}

	pdata = &omap->platdata;

	if (uhhtllp->get_platform_data(pdata) != 0) {
		ret = -EINVAL;
		goto err_mem;
	}

	omapresp = &omap->res;
	if (uhhtllp->get_resource(OMAP_OHCI, omapresp) != 0) {
		ret = -EINVAL;
		goto err_mem;
	}

	if (!omapresp->regs) {
		dev_dbg(&pdev->dev, "failed to OHCI regs\n");
		goto err_mem;
	}

	hcd = usb_create_hcd(&ohci_omap3_hc_driver, &pdev->dev,
			dev_name(&pdev->dev));
	if (!hcd) {
		dev_err(&pdev->dev, "usb_create_hcd failed\n");
		ret = -ENOMEM;
		goto err_mem;
	}

	platform_set_drvdata(pdev, omap);
	omap->dev		= &pdev->dev;
	omap->ohci		= hcd_to_ohci(hcd);

	hcd->rsrc_start = omapresp->start;
	hcd->rsrc_len = omapresp->len;
	hcd->regs =  omapresp->regs;

	ret = uhhtllp->store(OMAP_OHCI, OMAP_USB_HCD, hcd);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to store hcd\n");
		goto err_regs;
	}

	ret = uhhtllp->enable(OMAP_OHCI);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to start ohci\n");
		goto err_regs;
	}

	ohci_hcd_init(omap->ohci);

	ret = usb_add_hcd(hcd, omapresp->irq, IRQF_DISABLED);
	if (ret) {
		dev_dbg(&pdev->dev, "failed to add hcd with err %d\n", ret);
		goto err_add_hcd;
	}

	return 0;

err_add_hcd:
	uhhtllp->disable(OMAP_OHCI);

err_regs:
	usb_put_hcd(hcd);

err_mem:
	kfree(omap);

err_end:
	return ret;
}

/*
 * may be called without controller electrically present
 * may be called with controller, bus, and devices active
 */

/**
 * ohci_hcd_omap3_remove - shutdown processing for OHCI HCDs
 * @pdev: USB Host Controller being removed
 *
 * Reverses the effect of ohci_hcd_omap3_probe(), first invoking
 * the HCD's stop() method.  It is always called from a thread
 * context, normally "rmmod", "apmd", or something similar.
 */
static int __devexit ohci_hcd_omap3_remove(struct platform_device *pdev)
{
	struct ohci_hcd_omap3 *omap = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ohci_to_hcd(omap->ohci);
	struct uhhtll_apis *uhhtllp = pdev->dev.platform_data;

	if (hcd->state == HC_STATE_SUSPENDED)
		uhhtllp->resume(OMAP_OHCI);
	usb_remove_hcd(hcd);
	uhhtllp->disable(OMAP_OHCI);
	usb_put_hcd(hcd);
	kfree(omap);

	return 0;
}

static void ohci_hcd_omap3_shutdown(struct platform_device *pdev)
{
	struct ohci_hcd_omap3 *omap = platform_get_drvdata(pdev);
	struct usb_hcd *hcd = ohci_to_hcd(omap->ohci);
	struct uhhtll_apis *uhhtllp = pdev->dev.platform_data;
	int ret = 0;

	if (uhhtllp && uhhtllp->resume)
		ret = uhhtllp->resume(OMAP_OHCI);

	if (!ret && hcd->driver->shutdown)
		hcd->driver->shutdown(hcd);
}

static struct platform_driver ohci_hcd_omap3_driver = {
	.probe		= ohci_hcd_omap3_probe,
	.remove		= __devexit_p(ohci_hcd_omap3_remove),
	.shutdown	= ohci_hcd_omap3_shutdown,
	.driver		= {
		.name	= "ohci-omap3",
	},
};

MODULE_ALIAS("platform:ohci-omap3");
MODULE_AUTHOR("Anand Gadiyar <gadiyar@ti.com>");
