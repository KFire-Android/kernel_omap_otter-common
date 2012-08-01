/*
 * Palmas USB transceiver driver
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
 *
 * Based on twl6030_usb.c
 *
 * Author: Hema HK <hemahk@ti.com>
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
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/usb/omap_usb.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/notifier.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include <linux/mfd/palmas.h>

/*
 * This is an Empirical value that works, need to confirm the actual
 * value required for PALMAS_INT3_LINE_STATE to get stabilized.
 */

#define PALMAS_INT3_LINE_STATE_TIME	20000

static int palmas_usb_read(struct palmas *palmas, unsigned int reg,
		unsigned int *dest)
{
	unsigned int addr;
	int slave;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_USB_OTG_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_USB_OTG_BASE, reg);

	return regmap_read(palmas->regmap[slave], addr, dest);
}

static int palmas_usb_write(struct palmas *palmas, unsigned int reg,
		unsigned int data)
{
	unsigned int addr;
	int slave;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_USB_OTG_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_USB_OTG_BASE, reg);

	return regmap_write(palmas->regmap[slave], addr, data);
}

static void palmas_usb_wakeup(struct palmas *palmas, int enable)
{
	if (enable)
		palmas_usb_write(palmas, PALMAS_USB_WAKEUP,
					USB_WAKEUP_ID_WK_UP_COMP);
	else
		palmas_usb_write(palmas, PALMAS_USB_WAKEUP, 0);
}

static ssize_t palmas_usb_vbus_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	unsigned long		flags;
	int			ret = -EINVAL;
	struct palmas_usb	*palmas_usb = dev_get_drvdata(dev);

	spin_lock_irqsave(&palmas_usb->lock, flags);

	switch (palmas_usb->linkstat) {
	case OMAP_DWC3_VBUS_VALID:
	       ret = snprintf(buf, PAGE_SIZE, "vbus\n");
	       break;
	case OMAP_DWC3_ID_GROUND:
	       ret = snprintf(buf, PAGE_SIZE, "id\n");
	       break;
	case OMAP_DWC3_ID_FLOAT:
	case OMAP_DWC3_VBUS_OFF:
	       ret = snprintf(buf, PAGE_SIZE, "none\n");
	       break;
	default:
	       ret = snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
	}
	spin_unlock_irqrestore(&palmas_usb->lock, flags);

	return ret;
}
static DEVICE_ATTR(vbus, 0444, palmas_usb_vbus_show, NULL);

static irqreturn_t palmas_vbus_irq(int irq, void *_palmas_usb)
{
	/* TODO: Do we need to do any work here? */

	return IRQ_HANDLED;
}

static irqreturn_t palmas_vbus_wakeup_irq(int irq, void *_palmas_usb)
{
	enum omap_dwc3_vbus_id_status status = OMAP_DWC3_UNKNOWN;
	unsigned int vbus_line_state;
	int slave;
	unsigned int addr;
	int timeout = PALMAS_INT3_LINE_STATE_TIME;
	struct palmas_usb *palmas_usb = _palmas_usb;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_INTERRUPT_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_INTERRUPT_BASE,
						PALMAS_INT3_LINE_STATE);

	do {
		regmap_read(palmas_usb->palmas->regmap[slave], addr, &vbus_line_state);

		if (vbus_line_state == INT3_LINE_STATE_VBUS) {
			if (palmas_usb->linkstat != OMAP_DWC3_VBUS_VALID) {
				regulator_enable(palmas_usb->vbus_reg);
				status = OMAP_DWC3_VBUS_VALID;
				palmas_usb->linkstat = status;
			} else {
				dev_err(palmas_usb->dev,
					"Spurious connect event detected\n");
			}
			break;
		} else if (vbus_line_state == INT3_LINE_STATE_NONE) {
			if (palmas_usb->linkstat == OMAP_DWC3_VBUS_VALID) {
				regulator_disable(palmas_usb->vbus_reg);
				status = OMAP_DWC3_VBUS_OFF;
				palmas_usb->linkstat = status;
			} else {
				dev_err(palmas_usb->dev,
					"Spurious disconnect event detected\n");
			}
			break;
		}
		udelay(1);
	} while (--timeout);

	WARN_ON(!timeout);

	if (status != OMAP_DWC3_UNKNOWN) {
		omap_dwc3_mailbox(status);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static irqreturn_t palmas_id_irq(int irq, void *_palmas_usb)
{
	/* TODO: Do we need to do any work here? */

	return IRQ_HANDLED;
}

static irqreturn_t palmas_id_wakeup_irq(int irq, void *_palmas_usb)
{
	enum omap_dwc3_vbus_id_status status = OMAP_DWC3_UNKNOWN;
	unsigned int			set;
	struct palmas_usb	*palmas_usb = _palmas_usb;

	palmas_usb_read(palmas_usb->palmas, PALMAS_USB_ID_INT_LATCH_SET, &set);

	if (set & USB_ID_INT_SRC_ID_GND) {
		if (palmas_usb->linkstat != OMAP_DWC3_ID_GROUND) {
			regulator_enable(palmas_usb->vbus_reg);
			palmas_usb_write(palmas_usb->palmas,
						PALMAS_USB_ID_INT_EN_HI_SET,
					    USB_ID_INT_EN_HI_SET_ID_FLOAT);
			palmas_usb_write(palmas_usb->palmas,
						PALMAS_USB_ID_INT_EN_HI_CLR,
						USB_ID_INT_EN_HI_CLR_ID_GND);
			status = OMAP_DWC3_ID_GROUND;
			palmas_usb->linkstat = status;
		} else {
			dev_dbg(palmas_usb->dev, "Spurious ID GND detected\n");
		}
	} else if (set & USB_ID_INT_SRC_ID_FLOAT) {
		if (palmas_usb->linkstat == OMAP_DWC3_ID_GROUND) {
			palmas_usb_write(palmas_usb->palmas,
						PALMAS_USB_ID_INT_EN_HI_SET,
						USB_ID_INT_EN_HI_SET_ID_GND);
			palmas_usb_write(palmas_usb->palmas,
						PALMAS_USB_ID_INT_EN_HI_CLR,
						USB_ID_INT_EN_HI_CLR_ID_FLOAT);
			regulator_disable(palmas_usb->vbus_reg);
			status = OMAP_DWC3_ID_FLOAT;
			palmas_usb->linkstat = status;
		} else {
			dev_dbg(palmas_usb->dev, "Spurious ID FLOAT detected\n");
		}
	}

	if (status != OMAP_DWC3_UNKNOWN) {
		omap_dwc3_mailbox(status);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int palmas_enable_irq(struct palmas_usb *palmas_usb)
{
	palmas_usb_write(palmas_usb->palmas, PALMAS_USB_VBUS_CTRL_SET,
					USB_VBUS_CTRL_SET_VBUS_ACT_COMP);

	palmas_usb_write(palmas_usb->palmas, PALMAS_USB_ID_CTRL_SET,
					USB_ID_CTRL_SET_ID_ACT_COMP);

	palmas_usb_write(palmas_usb->palmas, PALMAS_USB_ID_INT_EN_HI_SET,
						USB_ID_INT_EN_HI_SET_ID_GND);

	palmas_vbus_wakeup_irq(palmas_usb->irq4, palmas_usb);

	if (palmas_usb->linkstat == OMAP_DWC3_UNKNOWN)
		palmas_id_wakeup_irq(palmas_usb->irq2, palmas_usb);

	return 0;
}

static void palmas_set_vbus_work(struct work_struct *data)
{
	struct palmas_usb *palmas_usb = container_of(data, struct palmas_usb,
								set_vbus_work);

	/*
	 * Start driving VBUS. Set OPA_MODE bit in CHARGERUSB_CTRL1
	 * register. This enables boost mode.
	 */

	if (palmas_usb->vbus_enable)
		regulator_enable(palmas_usb->vbus_reg);
	else
		regulator_disable(palmas_usb->vbus_reg);
}

static int palmas_set_vbus(struct phy_companion *comparator, bool enabled)
{
	struct palmas_usb *palmas_usb = comparator_to_palmas(comparator);

	palmas_usb->vbus_enable = enabled;
	schedule_work(&palmas_usb->set_vbus_work);

	return 0;
}

static int palmas_start_srp(struct phy_companion *comparator)
{
	struct palmas_usb *palmas_usb = comparator_to_palmas(comparator);

	palmas_usb_write(palmas_usb->palmas, PALMAS_USB_VBUS_CTRL_SET,
			USB_VBUS_CTRL_SET_VBUS_DISCHRG |
			USB_VBUS_CTRL_SET_VBUS_IADP_SINK);
	palmas_usb_write(palmas_usb->palmas, PALMAS_USB_VBUS_CTRL_SET,
			USB_VBUS_CTRL_SET_VBUS_CHRG_VSYS |
			USB_VBUS_CTRL_SET_VBUS_IADP_SINK);

	mdelay(100);

	palmas_usb_write(palmas_usb->palmas, PALMAS_USB_VBUS_CTRL_CLR,
			USB_VBUS_CTRL_SET_VBUS_CHRG_VSYS |
			USB_VBUS_CTRL_SET_VBUS_CHRG_VSYS);

	return 0;
}

static int __devinit palmas_usb_probe(struct platform_device *pdev)
{
	struct palmas_usb		*palmas_usb;
	int				status;
	struct palmas_platform_data	*pdata;
	struct palmas			*palmas;

	palmas	= dev_get_drvdata(pdev->dev.parent);
	pdata	= palmas->dev->platform_data;

	palmas_usb = kzalloc(sizeof(*palmas_usb), GFP_KERNEL);
	if (!palmas_usb)
		return -ENOMEM;

	palmas->usb		= palmas_usb;
	palmas_usb->palmas	= palmas;

	palmas_usb->dev		= &pdev->dev;
	palmas_usb->irq1	= platform_get_irq(pdev, 0);
	palmas_usb->irq2	= platform_get_irq(pdev, 1);
	palmas_usb->irq3	= platform_get_irq(pdev, 2);
	palmas_usb->irq4	= platform_get_irq(pdev, 3);

	palmas_usb_wakeup(palmas, pdata->usb_pdata->wakeup);

	palmas_usb->comparator.set_vbus	= palmas_set_vbus;
	palmas_usb->comparator.start_srp = palmas_start_srp;

	omap_usb2_set_comparator(&palmas_usb->comparator);

	/* init spinlock for workqueue */
	spin_lock_init(&palmas_usb->lock);

	if (!pdata->usb_pdata->no_control_vbus) {
		palmas_usb->vbus_reg = regulator_get(palmas->dev,
								"vbus");
		if (IS_ERR(palmas_usb->vbus_reg)) {
			dev_err(&pdev->dev, "vbus init failed\n");
			kfree(palmas_usb);
			return PTR_ERR(palmas_usb->vbus_reg);
		}
	}

	platform_set_drvdata(pdev, palmas_usb);

	if (device_create_file(&pdev->dev, &dev_attr_vbus))
		dev_warn(&pdev->dev, "could not create sysfs file\n");

	/* init spinlock for workqueue */
	spin_lock_init(&palmas_usb->lock);

	INIT_WORK(&palmas_usb->set_vbus_work, palmas_set_vbus_work);

	status = request_threaded_irq(palmas_usb->irq1, NULL,
				palmas_id_irq,
				IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"palmas_usb", palmas_usb);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->irq1, status);
		goto fail_irq1;
	}

	status = request_threaded_irq(palmas_usb->irq2, NULL,
			palmas_id_wakeup_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"palmas_usb", palmas_usb);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->irq2, status);
		goto fail_irq2;
	}

	status = request_threaded_irq(palmas_usb->irq3, NULL,
			palmas_vbus_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"palmas_usb", palmas_usb);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->irq3, status);
		goto fail_irq3;
	}

	status = request_threaded_irq(palmas_usb->irq4, NULL,
			palmas_vbus_wakeup_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"palmas_usb", palmas_usb);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
					palmas_usb->irq4, status);
		goto fail_irq4;
	}

	dev_info(&pdev->dev, "Initialized Palmas USB module\n");

	palmas_enable_irq(palmas_usb);

	return 0;

fail_irq4:
	free_irq(palmas_usb->irq3, palmas_usb);

fail_irq3:
	free_irq(palmas_usb->irq2, palmas_usb);

fail_irq2:
	free_irq(palmas_usb->irq1, palmas_usb);

fail_irq1:
	cancel_work_sync(&palmas_usb->set_vbus_work);
	device_remove_file(palmas_usb->dev, &dev_attr_vbus);
	kfree(palmas_usb);

	return status;
}

static int __devexit palmas_usb_remove(struct platform_device *pdev)
{
	struct palmas_usb *palmas_usb = platform_get_drvdata(pdev);

	free_irq(palmas_usb->irq1, palmas_usb);
	free_irq(palmas_usb->irq2, palmas_usb);
	free_irq(palmas_usb->irq3, palmas_usb);
	free_irq(palmas_usb->irq4, palmas_usb);
	regulator_put(palmas_usb->vbus_reg);
	device_remove_file(palmas_usb->dev, &dev_attr_vbus);
	cancel_work_sync(&palmas_usb->set_vbus_work);
	kfree(palmas_usb);

	return 0;
}

static struct platform_driver palmas_usb_driver = {
	.probe		= palmas_usb_probe,
	.remove		= __devexit_p(palmas_usb_remove),
	.driver		= {
		.name	= "palmas-usb",
		.owner	= THIS_MODULE,
	},
};

static int __init palmas_usb_init(void)
{
	return platform_driver_register(&palmas_usb_driver);
}
module_init(palmas_usb_init);

static void __exit palmas_usb_exit(void)
{
	platform_driver_unregister(&palmas_usb_driver);
}
module_exit(palmas_usb_exit);

MODULE_ALIAS("platform:palmas-usb");
MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas USB transceiver driver");
MODULE_LICENSE("GPL");
