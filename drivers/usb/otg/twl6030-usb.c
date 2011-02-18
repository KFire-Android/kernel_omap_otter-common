/*
 * twl6030_usb - TWL6030 USB transceiver, talking to OMAP OTG controller
 *
 * Copyright (C) 2004-2007 Texas Instruments
 * Copyright (C) 2008 Nokia Corporation
 * Contact: Felipe Balbi <felipe.balbi@nokia.com>
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Current status:
 *	- HS USB ULPI mode works.
 *	- 3-pin mode support may be added in future.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/usb/otg.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/clk.h>

/* usb register definitions*/
#define USB_VENDOR_ID_LSB		0x00
#define USB_VENDOR_ID_MSB		0x01
#define USB_PRODUCT_ID_LSB		0x02
#define USB_PRODUCT_ID_MSB		0x03
#define USB_VBUS_CTRL_SET		0x04
#define USB_VBUS_CTRL_CLR		0x05
#define USB_ID_CTRL_SET			0x06
#define USB_ID_CTRL_CLR			0x07
#define USB_VBUS_INT_SRC		0x08
#define USB_VBUS_INT_LATCH_SET		0x09
#define USB_VBUS_INT_LATCH_CLR		0x0A
#define USB_VBUS_INT_EN_LO_SET		0x0B
#define USB_VBUS_INT_EN_LO_CLR		0x0C
#define USB_VBUS_INT_EN_HI_SET		0x0D
#define USB_VBUS_INT_EN_HI_CLR		0x0E
#define USB_ID_INT_SRC			0x0F
#define ID_GND				(1 << 0)
#define ID_FLOAT			(1 << 4)

#define USB_ID_INT_LATCH_SET		0x10
#define USB_ID_INT_LATCH_CLR		0x11


#define USB_ID_INT_EN_LO_SET		0x12
#define USB_ID_INT_EN_LO_CLR		0x13
#define USB_ID_INT_EN_HI_SET		0x14
#define USB_ID_INT_EN_HI_CLR		0x15
#define USB_OTG_ADP_CTRL		0x16
#define USB_OTG_ADP_HIGH		0x17
#define USB_OTG_ADP_LOW			0x18
#define USB_OTG_ADP_RISE		0x19
#define USB_OTG_REVISION		0x1A

/* to be moved to LDO*/
#define MISC2				0xE5
#define CFG_LDO_PD2			0xF5
#define USB_BACKUP_REG			0xFA

#define STS_HW_CONDITIONS		0x21
#define STS_USB_ID			(1 << 2)


/* In module TWL6030_MODULE_PM_MASTER */
#define PROTECT_KEY			0x0E
#define STS_HW_CONDITIONS		0x21

/* In module TWL6030_MODULE_PM_RECEIVER */
#define VUSB_DEDICATED1			0x7D
#define VUSB_DEDICATED2			0x7E
#define VUSB1V5_DEV_GRP			0x71
#define VUSB1V5_TYPE			0x72
#define VUSB1V5_REMAP			0x73
#define VUSB1V8_DEV_GRP			0x74
#define VUSB1V8_TYPE			0x75
#define VUSB1V8_REMAP			0x76
#define VUSB3V1_DEV_GRP			0x77
#define VUSB3V1_TYPE			0x78
#define VUSB3V1_REMAP			0x79

/* In module TWL6030_MODULE_PM_RECEIVER */
#define VUSB_CFG_TRANS			0x71
#define VUSB_CFG_STATE			0x72
#define VUSB_CFG_VOLTAGE		0x73

/* in module TWL6030_MODULE_MAIN_CHARGE*/

#define CHARGERUSB_CTRL1		0x8

#define CONTROLLER_STAT1		0x03
#define	VBUS_DET			(1 << 2)


/* OMAP control module register for UTMI PHY*/
#define CONTROL_DEV_CONF		0x300
#define PHY_PD				0x1

struct twl6030_usb {
	struct otg_transceiver	otg;
	struct device		*dev;

	/* TWL6030 internal USB regulator supplies */
	struct regulator	*usb1v5;
	struct regulator	*usb1v8;
	struct regulator	*usb3v1;

	/* for vbus reporting with irqs disabled */
	spinlock_t		lock;

	int			irq;
	u8			linkstat;
	u8			asleep;
	bool			irq_enabled;
	int			prev_vbus;
};

/* internal define on top of container_of */
#define xceiv_to_twl(x)		container_of((x), struct twl6030_usb, otg);

static void __iomem *ctrl_base;

/*-------------------------------------------------------------------------*/

static inline int twl6030_writeb(struct twl6030_usb *twl, u8 module,
						u8 data, u8 address)
{
	int ret = 0;

	ret = twl_i2c_write_u8(module, data, address);
	if (ret < 0)
		dev_dbg(twl->dev,
			"TWL6030:USB:Write[0x%x] Error %d\n", address, ret);
	return ret;
}

static inline int twl6030_readb(struct twl6030_usb *twl, u8 module, u8 address)
{
	u8 data = 0;
	int ret = 0;

	ret = twl_i2c_read_u8(module, &data, address);
	if (ret >= 0)
		ret = data;
	else
		dev_dbg(twl->dev,
			"TWL6030:readb[0x%x,0x%x] Error %d\n",
					module, address, ret);

	return ret;
}

/*-------------------------------------------------------------------------*/

static int twl6030_usb_ldo_init(struct twl6030_usb *twl)
{

	/* Set to OTG_REV 1.3 and turn on the ID_WAKEUP_COMP*/
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x1, USB_BACKUP_REG);

	/* Program CFG_LDO_PD2 register and set VUSB bit */
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x1, CFG_LDO_PD2);

	/* Program MISC2 register and set bit VUSB_IN_VBAT */
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x10, MISC2);
	/*
	 * Program the VUSB_CFG_VOLTAGE register to set the VUSB
	 * voltage to 3.3V
	 */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0x18,
						VUSB_CFG_VOLTAGE);

	/* Program the VUSB_CFG_TRANS for ACTIVE state. */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0x3F,
						VUSB_CFG_TRANS);

	/* Program the VUSB_CFG_STATE register to ON on all groups. */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0xE1,
						VUSB_CFG_STATE);

	/* Program the USB_VBUS_CTRL_SET and set VBUS_ACT_COMP bit */
	twl6030_writeb(twl, TWL_MODULE_USB, 0x4, USB_VBUS_CTRL_SET);

	/*
	 * Program the USB_ID_CTRL_SET register to enable GND drive
	 * and the ID comparators
	 */
	twl6030_writeb(twl, TWL_MODULE_USB, 0x4, USB_ID_CTRL_SET);

	return 0;

}

static ssize_t twl6030_usb_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct twl6030_usb *twl = dev_get_drvdata(dev);
	unsigned long flags;
	int ret = -EINVAL;

	spin_lock_irqsave(&twl->lock, flags);
	ret = sprintf(buf, "%s\n",
			(twl->linkstat == USB_EVENT_VBUS) ? "on" : "off");
	spin_unlock_irqrestore(&twl->lock, flags);

	return ret;
}
static DEVICE_ATTR(vbus, 0444, twl6030_usb_vbus_show, NULL);

static irqreturn_t twl6030_usb_irq(int irq, void *_twl)
{
	struct twl6030_usb *twl = _twl;
	int status  = USB_EVENT_NONE;
	int vbus_state, hw_state;

	hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	vbus_state = twl6030_readb(twl, TWL6030_MODULE_CHARGER,
						CONTROLLER_STAT1);
	/* AC unplugg can also generate this IRQ
	 * we only call the notifier in case of VBUS change
	 */
	if (twl->prev_vbus != (vbus_state & VBUS_DET)) {
		if (!(hw_state & STS_USB_ID)) {
			if (vbus_state & VBUS_DET) {
				status = USB_EVENT_VBUS;
				twl->otg.default_a = false;
				twl->otg.state = OTG_STATE_B_IDLE;
			}
			else
				status = USB_EVENT_NONE;

			if (status >= 0) {
				twl->linkstat = status;
				blocking_notifier_call_chain(&twl->otg.notifier,
						status, twl->otg.gadget);
			}
		}
		sysfs_notify(&twl->dev->kobj, NULL, "vbus");
	}
	twl->prev_vbus = vbus_state & VBUS_DET;

	return IRQ_HANDLED;
}

static irqreturn_t twl6030_usbotg_irq(int irq, void *_twl)
{
	struct twl6030_usb *twl = _twl;
	int status = USB_EVENT_NONE;
	u8 hw_state;

	hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	if (hw_state & STS_USB_ID) {

		twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_EN_HI_CLR, 0x1);
		twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_EN_HI_SET,
									0x10);
		status = USB_EVENT_ID;
		twl->otg.default_a = true;
		twl->otg.state = OTG_STATE_A_IDLE;
		blocking_notifier_call_chain(&twl->otg.notifier, status,
							twl->otg.gadget);
	} else {
		twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_EN_HI_CLR,
									0x10);
		twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_EN_HI_SET,
									0x1);
	}
	twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_LATCH_CLR, status);
	twl->linkstat = status;

	return IRQ_HANDLED;
}

static int twl6030_set_suspend(struct otg_transceiver *x, int suspend)
{
	return 0;
}

static int twl6030_set_peripheral(struct otg_transceiver *x,
		struct usb_gadget *gadget)
{
	struct twl6030_usb *twl;

	if (!x)
		return -ENODEV;

	twl = xceiv_to_twl(x);
	twl->otg.gadget = gadget;
	if (!gadget)
		twl->otg.state = OTG_STATE_UNDEFINED;

	return 0;
}

static int twl6030_enable_irq(struct otg_transceiver *x)
{
	struct twl6030_usb *twl = xceiv_to_twl(x);

	twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_EN_HI_SET, 0x1);
	twl6030_interrupt_unmask(0x05, REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(0x05, REG_INT_MSK_STS_C);

	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
				REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
				REG_INT_MSK_STS_C);
	twl6030_usb_irq(twl->irq, twl);
	twl6030_usbotg_irq(twl->irq, twl);

	return 0;
}

static int twl6030_set_vbus(struct otg_transceiver *x, bool enabled)
{
	struct twl6030_usb *twl = xceiv_to_twl(x);

	/*
	 * Start driving VBUS. Set OPA_MODE bit in CHARGERUSB_CTRL1
	 * register. This enables boost mode.
	 */
	if (enabled) {

		twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE , 0x40,
						CHARGERUSB_CTRL1);
	} else {
		twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE , 0x00,
						CHARGERUSB_CTRL1);
	}

	return 0;
}

static int twl6030_set_host(struct otg_transceiver *x, struct usb_bus *host)
{
	struct twl6030_usb *twl;

	if (!x)
		return -ENODEV;

	twl = xceiv_to_twl(x);
	twl->otg.host = host;
	if (!host)
		twl->otg.state = OTG_STATE_UNDEFINED;
	return 0;
}

static int set_phy_clk(struct otg_transceiver *x, int on)
{
	static int state;
	struct clk *phyclk;
	struct clk *clk48m;
	struct clk *clk32k;

	phyclk = clk_get(NULL, "ocp2scp_usb_phy_ick");
	if (IS_ERR(phyclk)) {
		pr_warning("cannot clk_get ocp2scp_usb_phy_ick\n");
		return PTR_ERR(phyclk);
	}

	clk48m = clk_get(NULL, "ocp2scp_usb_phy_phy_48m");
	if (IS_ERR(clk48m)) {
		pr_warning("cannot clk_get ocp2scp_usb_phy_phy_48m\n");
		clk_put(phyclk);
		return PTR_ERR(clk48m);
	}

	clk32k = clk_get(NULL, "usb_phy_cm_clk32k");
	if (IS_ERR(clk32k)) {
		pr_warning("cannot clk_get usb_phy_cm_clk32k\n");
		clk_put(phyclk);
		clk_put(clk48m);
		return PTR_ERR(clk32k);
	}

	if (on) {
		if (!state) {
			/* Enable the phy clocks*/
			clk_enable(phyclk);
			clk_enable(clk48m);
			clk_enable(clk32k);
			state = 1;
		}
	} else if (state) {
		/* Disable the phy clocks*/
		clk_disable(phyclk);
		clk_disable(clk48m);
		clk_disable(clk32k);
		state = 0;
	}
	return 0;
}

static int phy_init(struct otg_transceiver *x)
{
	set_phy_clk(x, 1);

	if (__raw_readl(ctrl_base + CONTROL_DEV_CONF) & PHY_PD) {
		__raw_writel(~PHY_PD, ctrl_base + CONTROL_DEV_CONF);
		msleep(200);
	}
	return 0;
}

static void phy_shutdown(struct otg_transceiver *x)
{
	set_phy_clk(x, 0);
	__raw_writel(PHY_PD, ctrl_base + CONTROL_DEV_CONF);
}

static int __devinit twl6030_usb_probe(struct platform_device *pdev)
{
	struct twl6030_usb	*twl;
	int			status, err;
	twl = kzalloc(sizeof *twl, GFP_KERNEL);
	if (!twl)
		return -ENOMEM;

	twl->dev		= &pdev->dev;
	twl->irq		= platform_get_irq(pdev, 0);
	twl->otg.dev		= twl->dev;
	twl->otg.label		= "twl6030";
	twl->otg.set_host	= twl6030_set_host;
	twl->otg.set_peripheral	= twl6030_set_peripheral;
	twl->otg.set_suspend	= twl6030_set_suspend;
	twl->asleep		= 1;
	twl->otg.set_vbus	= twl6030_set_vbus;
	twl->otg.init		= phy_init;
	twl->otg.enable_irq	= twl6030_enable_irq;
	twl->otg.set_clk	= set_phy_clk;
	twl->otg.shutdown	= phy_shutdown;
	twl->prev_vbus		= 0;

	/* init spinlock for workqueue */
	spin_lock_init(&twl->lock);

	err = twl6030_usb_ldo_init(twl);
	if (err) {
		dev_err(&pdev->dev, "ldo init failed\n");
		kfree(twl);
		return err;
	}
	otg_set_transceiver(&twl->otg);

	platform_set_drvdata(pdev, twl);
	if (device_create_file(&pdev->dev, &dev_attr_vbus))
		dev_warn(&pdev->dev, "could not create sysfs file\n");

	BLOCKING_INIT_NOTIFIER_HEAD(&twl->otg.notifier);

	/* Our job is to use irqs and status from the power module
	 * to keep the transceiver disabled when nothing's connected.
	 *
	 * FIXME we actually shouldn't start enabling it until the
	 * USB controller drivers have said they're ready, by calling
	 * set_host() and/or set_peripheral() ... OTG_capable boards
	 * need both handles, otherwise just one suffices.
	 */
	twl->irq_enabled = true;
	status = request_threaded_irq(twl->irq, NULL, twl6030_usbotg_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_usb", twl);
	if (status < 0) {
		dev_dbg(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq, status);
		kfree(twl);
		return status;
	}
	twl->irq = platform_get_irq(pdev, 1);

	status = request_threaded_irq(twl->irq, NULL, twl6030_usb_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_usb", twl);
	if (status < 0) {
		dev_dbg(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq, status);
		kfree(twl);
		return status;
	}

	ctrl_base = ioremap(0x4A002000, SZ_1K);
	/* power down the phy by default can be enabled on connect */
	__raw_writel(PHY_PD, ctrl_base + CONTROL_DEV_CONF);

	dev_info(&pdev->dev, "Initialized TWL6030 USB module\n");
	return 0;
}

static int __exit twl6030_usb_remove(struct platform_device *pdev)
{
	struct twl6030_usb *twl = platform_get_drvdata(pdev);

	free_irq(twl->irq, twl);
	device_remove_file(twl->dev, &dev_attr_vbus);

	twl6030_interrupt_mask(TWL6030_USBOTG_INT_MASK,
		REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_USBOTG_INT_MASK,
			REG_INT_MSK_STS_C);
	kfree(twl);
	iounmap(ctrl_base);

	return 0;
}

static struct platform_driver twl6030_usb_driver = {
	.probe		= twl6030_usb_probe,
	.remove		= __exit_p(twl6030_usb_remove),
	.driver		= {
		.name	= "twl6030_usb",
		.owner	= THIS_MODULE,
	},
};

static int __init twl6030_usb_init(void)
{
	return platform_driver_register(&twl6030_usb_driver);
}
subsys_initcall(twl6030_usb_init);

static void __exit twl6030_usb_exit(void)
{
	platform_driver_unregister(&twl6030_usb_driver);
}
module_exit(twl6030_usb_exit);

MODULE_ALIAS("platform:twl6030_usb");
MODULE_AUTHOR("Texas Instruments, Inc, Nokia Corporation");
MODULE_DESCRIPTION("TWL6030 USB transceiver driver");
MODULE_LICENSE("GPL");
