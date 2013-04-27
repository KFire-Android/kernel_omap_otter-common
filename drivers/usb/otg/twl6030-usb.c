/*
 * twl6030_usb - TWL6030 USB transceiver, talking to OMAP OTG driver.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <linux/usb/otg.h>
#include <linux/usb/phy_companion.h>
#include <linux/usb/omap_usb.h>
#include <linux/usb/musb-omap.h>
#include <linux/i2c/twl.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/power_supply.h>

/* usb register definitions */
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

#define USB_ID_INT_MASK			0x1F

/* to be moved to LDO */
#define TWL6030_MISC2			0xE5
#define TWL6030_CFG_LDO_PD2		0xF5
#define TWL6030_BACKUP_REG		0xFA

/* bits in MISC2 Register */
#define VUSB_IN_PMID			BIT(3)
#define VUSB_IN_VBAT			BIT(4)

#define STS_HW_CONDITIONS		0x21

/* In module TWL6030_MODULE_PM_MASTER */
#define STS_HW_CONDITIONS		0x21
#define STS_USB_ID			BIT(2)

/* In module TWL6030_MODULE_PM_RECEIVER */
#define VUSB_CFG_TRANS			0x71
#define VUSB_CFG_STATE			0x72
#define VUSB_CFG_VOLTAGE		0x73

/* in module TWL6030_MODULE_MAIN_CHARGE */

#define CHARGERUSB_CTRL1		0x8

#define BOOST_MODE_OFF_MASK		0x00
#define BOOST_MODE_ON_MASK		0x40
#define BOOST_MODE_STATE_MASK		0x60

#define CONTROLLER_STAT1		0x03
#define	VBUS_DET			BIT(2)

struct twl6030_usb {
	struct phy_companion	comparator;
	struct device		*dev;

	/* for vbus reporting with irqs disabled */
	spinlock_t		lock;

	struct regulator		*usb3v3;

	/* used to set vbus, in atomic path */
	struct work_struct	set_vbus_work;

	int			irq1;
	int			irq2;
	u8			asleep;
	bool			irq_enabled;
	bool			vbus_enable;
	unsigned long		features;
	u32			errata;

	enum omap_musb_vbus_id_status prev_status;

	struct wake_lock	charger_det_lock;
};

static BLOCKING_NOTIFIER_HEAD(notifier_list);

#define	comparator_to_twl(x) container_of((x), struct twl6030_usb, comparator)
/*-------------------------------------------------------------------------*/

int twl6030_usb_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(twl6030_usb_register_notifier);

int twl6030_usb_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&notifier_list, nb);
}
EXPORT_SYMBOL_GPL(twl6030_usb_unregister_notifier);

static inline int twl6030_writeb(struct twl6030_usb *twl, u8 module,
						u8 data, u8 address)
{
	int ret = 0;

	ret = twl_i2c_write_u8(module, data, address);
	if (ret < 0)
		dev_err(twl->dev,
			"Write[0x%x] Error %d\n", address, ret);
	return ret;
}

static inline u8 twl6030_readb(struct twl6030_usb *twl, u8 module, u8 address)
{
	u8 data, ret = 0;

	ret = twl_i2c_read_u8(module, &data, address);
	if (ret >= 0)
		ret = data;
	else
		dev_err(twl->dev,
			"readb[0x%x,0x%x] Error %d\n",
					module, address, ret);
	return ret;
}

static int twl6030_start_srp(struct phy_companion *comparator)
{
	struct twl6030_usb *twl = comparator_to_twl(comparator);

	twl6030_writeb(twl, TWL_MODULE_USB, 0x24, USB_VBUS_CTRL_SET);
	twl6030_writeb(twl, TWL_MODULE_USB, 0x84, USB_VBUS_CTRL_SET);

	mdelay(100);
	twl6030_writeb(twl, TWL_MODULE_USB, 0xa0, USB_VBUS_CTRL_CLR);

	return 0;
}

static void twl6030_enable_ldo_input_supply(struct twl6030_usb *twl,
		bool enable)
{
	u8 misc2_data = 0;

	misc2_data = twl6030_readb(twl, TWL6030_MODULE_ID0, TWL6030_MISC2);
	misc2_data &= ~(VUSB_IN_PMID | VUSB_IN_VBAT);
	if (enable)
		misc2_data |= VUSB_IN_VBAT;
	twl6030_writeb(twl, TWL6030_MODULE_ID0, misc2_data, TWL6030_MISC2);
}

static int twl6030_usb_ldo_init(struct twl6030_usb *twl)
{
	/* Set to OTG_REV 1.3 and turn on the ID_WAKEUP_COMP */
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x1, TWL6030_BACKUP_REG);

	/* Program CFG_LDO_PD2 register and set VUSB bit */
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x1, TWL6030_CFG_LDO_PD2);

	twl->usb3v3 = regulator_get(twl->dev, "vusb");
	if (IS_ERR(twl->usb3v3))
		return -ENODEV;

	/* Program the USB_VBUS_CTRL_SET and set VBUS_ACT_COMP bit */
	twl6030_writeb(twl, TWL_MODULE_USB, 0x4, USB_VBUS_CTRL_SET);

	/*
	 * Program the USB_ID_CTRL_SET register to enable GND drive
	 * and the ID comparators
	 */
	twl6030_writeb(twl, TWL_MODULE_USB, 0x14, USB_ID_CTRL_SET);

	/* Disable LDO before disabling his input supply */
	regulator_force_disable(twl->usb3v3);
	twl6030_enable_ldo_input_supply(twl, false);

	return 0;
}

static ssize_t twl6030_usb_vbus_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct twl6030_usb *twl = dev_get_drvdata(dev);
	unsigned long flags;
	int ret = -EINVAL;

	spin_lock_irqsave(&twl->lock, flags);

	switch (twl->prev_status) {
	case OMAP_MUSB_VBUS_VALID:
	       ret = snprintf(buf, PAGE_SIZE, "vbus\n");
	       break;
	case OMAP_MUSB_ID_GROUND:
	       ret = snprintf(buf, PAGE_SIZE, "id\n");
	       break;
	case OMAP_MUSB_VBUS_OFF:
	case OMAP_MUSB_ID_FLOAT:
	       ret = snprintf(buf, PAGE_SIZE, "none\n");
	       break;
	default:
	       ret = snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
	}
	spin_unlock_irqrestore(&twl->lock, flags);

	return ret;
}
static DEVICE_ATTR(vbus, 0444, twl6030_usb_vbus_show, NULL);

static irqreturn_t twl6030_usb_irq(int irq, void *_twl)
{
	struct twl6030_usb *twl = _twl;
	int status = OMAP_MUSB_UNKNOWN;
	u8 vbus_state, hw_state;
	unsigned long charger_type = POWER_SUPPLY_TYPE_UNKNOWN;
	int event;

	hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	vbus_state = twl6030_readb(twl, TWL_MODULE_MAIN_CHARGE,
						CONTROLLER_STAT1);
	if (!(hw_state & STS_USB_ID)) {
		if (vbus_state & VBUS_DET) {
			if (twl->prev_status == OMAP_MUSB_VBUS_VALID)
				return IRQ_HANDLED;
			wake_lock(&twl->charger_det_lock);
			twl6030_enable_ldo_input_supply(twl, true);
			regulator_enable(twl->usb3v3);
			charger_type = omap_usb2_charger_detect(
					&twl->comparator);
			if (charger_type == POWER_SUPPLY_TYPE_USB_DCP)
				event = USB_EVENT_CHARGER;
			else
				event = USB_EVENT_VBUS;
			twl->asleep = 1;
			status = OMAP_MUSB_VBUS_VALID;
			omap_musb_mailbox(status);
			blocking_notifier_call_chain(&notifier_list,
						     event, &charger_type);
			wake_unlock(&twl->charger_det_lock);
		} else {
			if (twl->prev_status != OMAP_MUSB_UNKNOWN) {
				if (twl->prev_status == OMAP_MUSB_VBUS_OFF)
					return IRQ_HANDLED;
				status = OMAP_MUSB_VBUS_OFF;
				event = USB_EVENT_NONE;
				omap_musb_mailbox(status);
				blocking_notifier_call_chain(&notifier_list,
							     event,
							     &charger_type);
				if (twl->asleep) {
					regulator_disable(twl->usb3v3);
					twl6030_enable_ldo_input_supply(twl,
									false);
					twl->asleep = 0;
				}
			}
		}
		twl->prev_status = status;
		sysfs_notify(&twl->dev->kobj, NULL, "vbus");
	}

	return IRQ_HANDLED;
}

static irqreturn_t twl6030_usbotg_irq(int irq, void *_twl)
{
	struct twl6030_usb *twl = _twl;
	u8 hw_state;

	hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	if (hw_state & STS_USB_ID) {
		if (twl->prev_status == OMAP_MUSB_ID_GROUND)
			goto exit;
		twl->prev_status = OMAP_MUSB_ID_GROUND;
		twl6030_enable_ldo_input_supply(twl, true);
		regulator_enable(twl->usb3v3);
		twl->asleep = 1;
		twl6030_writeb(twl, TWL_MODULE_USB, 0x1, USB_ID_INT_EN_HI_CLR);
		twl6030_writeb(twl, TWL_MODULE_USB, 0x10, USB_ID_INT_EN_HI_SET);
		omap_musb_mailbox(OMAP_MUSB_ID_GROUND);
		/*
		 * NOTE:
		 * This code is needed because if ID pin
		 * is grounded then no operations are performed in
		 * VBUS detection interrupt.
		 * Just do sysfs_notify.
		 */
		sysfs_notify(&twl->dev->kobj, NULL, "vbus");
	} else  {
		if (twl->prev_status != OMAP_MUSB_ID_GROUND)
			goto exit;
		twl->prev_status = OMAP_MUSB_ID_FLOAT;
		/*
		 * NOTE:
		 * This is workaround for the TWL6032 that miss VBUS
		 * detection interrupt while OPA_MODE is set to 1 in
		 * the CHARGERUSB_CTRL1 register.
		 * Just set BOOST mode for OTG to off and VBUS interrupt
		 * will start to work.
		 */
		if (twl->errata & TWL6032_ERRATA_VBUS_IRQ_LOST_OPA_MODE) {
			twl->prev_status = OMAP_MUSB_VBUS_VALID;
			twl->vbus_enable = 0;
			twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE,
				       BOOST_MODE_OFF_MASK, CHARGERUSB_CTRL1);
		}
		twl6030_writeb(twl, TWL_MODULE_USB, 0x10, USB_ID_INT_EN_HI_CLR);
		twl6030_writeb(twl, TWL_MODULE_USB, 0x1, USB_ID_INT_EN_HI_SET);
	}
exit:
	twl6030_writeb(twl, TWL_MODULE_USB, USB_ID_INT_MASK,
		       USB_ID_INT_LATCH_CLR);

	return IRQ_HANDLED;
}

static int twl6030_enable_irq(struct twl6030_usb *twl)
{
	twl6030_writeb(twl, TWL_MODULE_USB, 0x1, USB_ID_INT_EN_HI_SET);
	twl6030_interrupt_unmask(0x05, REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(0x05, REG_INT_MSK_STS_C);

	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
				REG_INT_MSK_LINE_C);
	twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
				REG_INT_MSK_STS_C);
	twl6030_usb_irq(twl->irq2, twl);
	twl6030_usbotg_irq(twl->irq1, twl);

	return 0;
}

static void otg_set_vbus_work(struct work_struct *data)
{
	struct twl6030_usb *twl = container_of(data, struct twl6030_usb,
								set_vbus_work);

	/*
	 * Start driving VBUS. Set OPA_MODE bit and clear HZ_MODE bit
	 * in CHARGERUSB_CTRL1 register. This enables Boost mode for OTG
	 * purpose.
	 * Since in OTG operating mode internal USB charger used as VBUS supply
	 * then it should not be in high-impedance mode on VBUS.
	 */
	if (twl->vbus_enable)
		twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE,
			       BOOST_MODE_ON_MASK, CHARGERUSB_CTRL1);
	else
		twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE,
			       BOOST_MODE_OFF_MASK, CHARGERUSB_CTRL1);
}

static int twl6030_set_vbus(struct phy_companion *comparator, bool enabled)
{
	struct twl6030_usb *twl = comparator_to_twl(comparator);

	if (twl->vbus_enable != enabled) {
		twl->vbus_enable = enabled;
		schedule_work(&twl->set_vbus_work);
	}

	return 0;
}

static void twl6030_get_vbus(struct twl6030_usb *twl)
{
	u8 ctrl1_data;

	/*
	 * Read CHARGERUSB_CTRL1 register and set vbus_enable flag
	 * if BOOST mode for OTG purpose is enabled and USB charger
	 * is not in high-impedance mode on VBUS.
	 */
	ctrl1_data = twl6030_readb(twl, TWL_MODULE_MAIN_CHARGE,
				   CHARGERUSB_CTRL1);
	if ((ctrl1_data & BOOST_MODE_STATE_MASK) == BOOST_MODE_ON_MASK)
		twl->vbus_enable = true;
	else
		twl->vbus_enable = false;
}

static int __devinit twl6030_usb_probe(struct platform_device *pdev)
{
	struct twl6030_usb	*twl;
	int			status, err;
	struct twl4030_usb_data *pdata;
	struct device *dev = &pdev->dev;
	pdata = dev->platform_data;

	twl = kzalloc(sizeof *twl, GFP_KERNEL);
	if (!twl)
		return -ENOMEM;

	twl->dev		= &pdev->dev;
	twl->irq1		= platform_get_irq(pdev, 0);
	twl->irq2		= platform_get_irq(pdev, 1);
	twl->features		= pdata->features;
	twl->errata		= pdata->errata;
	twl->prev_status	= OMAP_MUSB_UNKNOWN;

	twl->comparator.set_vbus	= twl6030_set_vbus;
	twl->comparator.start_srp	= twl6030_start_srp;

	omap_usb2_set_comparator(&twl->comparator);

	/* init spinlock for workqueue */
	spin_lock_init(&twl->lock);

	err = twl6030_usb_ldo_init(twl);
	if (err) {
		dev_err(&pdev->dev, "ldo init failed\n");
		kfree(twl);
		return err;
	}

	twl6030_get_vbus(twl);

	platform_set_drvdata(pdev, twl);
	if (device_create_file(&pdev->dev, &dev_attr_vbus))
		dev_warn(&pdev->dev, "could not create sysfs file\n");

	INIT_WORK(&twl->set_vbus_work, otg_set_vbus_work);
	wake_lock_init(&twl->charger_det_lock,
		       WAKE_LOCK_SUSPEND, "charger_detector");
	twl->irq_enabled = true;
	status = request_threaded_irq(twl->irq1, NULL, twl6030_usbotg_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_usb", twl);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq1, status);
		wake_lock_destroy(&twl->charger_det_lock);
		device_remove_file(twl->dev, &dev_attr_vbus);
		kfree(twl);
		return status;
	}

	status = request_threaded_irq(twl->irq2, NULL, twl6030_usb_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_usb", twl);
	if (status < 0) {
		dev_err(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq2, status);
		free_irq(twl->irq1, twl);
		wake_lock_destroy(&twl->charger_det_lock);
		device_remove_file(twl->dev, &dev_attr_vbus);
		kfree(twl);
		return status;
	}

	twl->asleep = 0;
	twl6030_enable_irq(twl);
	dev_info(&pdev->dev, "Initialized TWL6030 USB module\n");

	return 0;
}

static int __exit twl6030_usb_remove(struct platform_device *pdev)
{
	struct twl6030_usb *twl = platform_get_drvdata(pdev);

	twl6030_interrupt_mask(TWL6030_USBOTG_INT_MASK,
		REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_USBOTG_INT_MASK,
			REG_INT_MSK_STS_C);
	free_irq(twl->irq1, twl);
	free_irq(twl->irq2, twl);
	regulator_put(twl->usb3v3);
	device_remove_file(twl->dev, &dev_attr_vbus);
	cancel_work_sync(&twl->set_vbus_work);
	wake_lock_destroy(&twl->charger_det_lock);
	kfree(twl);

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
MODULE_AUTHOR("Hema HK <hemahk@ti.com>");
MODULE_DESCRIPTION("TWL6030 USB transceiver driver");
MODULE_LICENSE("GPL");
