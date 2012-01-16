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
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
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
#define VUSB_CFG_GRP			0x70
#define VUSB_CFG_TRANS			0x71
#define VUSB_CFG_STATE			0x72
#define VUSB_CFG_VOLTAGE		0x73

/* in module TWL6030_MODULE_MAIN_CHARGE*/

#define CHARGERUSB_CTRL1		0x8
#define SUSPEND_BOOT    (1 << 7)
#define OPA_MODE        (1 << 6)
#define HZ_MODE         (1 << 5)
#define TERM            (1 << 4)


#define CONTROLLER_STAT1		0x03
#define	VBUS_DET			(1 << 2)


/* OMAP control module register for UTMI PHY*/
#define CONTROL_DEV_CONF		0x300
#define PHY_PD				0x1
static struct wake_lock twlusb_lock;
struct twl6030_usb {
	struct otg_transceiver	otg;
	struct device		*dev;

	/* TWL4030 internal USB regulator supplies */
	struct regulator	*usb1v5;
	struct regulator	*usb1v8;
	struct regulator	*usb3v1;
	/* TWL6030 internal USB regulator supplies */
	struct regulator	*vusb;
	/* for vbus reporting with irqs disabled */
	spinlock_t		lock;
   struct work_struct	        vbus_work;
   struct work_struct	        id_work;
	int			irq1;
	int			irq2;
	u8			linkstat;
	u8			asleep;
	bool			irq_enabled;
    int prev_vbus;
    int                     mbid;
};
struct workqueue_struct    *vbusid_work_queue;
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

/*-------------------------------------------------------------------------*/

static int twl6030_usb_ldo_init(struct twl6030_usb *twl)
{
#ifdef CONFIG_USB_MUSB_HDRC_HCD 
	/* Set to OTG_REV 1.3 and turn on the ID_WAKEUP_COMP*/
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x1, USB_BACKUP_REG);
# else
	/* Turn off the ID_WAKEUP_COMP*/
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x0, USB_BACKUP_REG);    
#endif
	/* Set Pull-down is enabled when VUSB and VUSIM is in OFF state */
	//twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x3, CFG_LDO_PD2);

	/* Program MISC2 register and set bit VUSB_IN_VBAT */
	twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x10, MISC2);

	/*
	 * Program the VUSB_CFG_VOLTAGE register to set the VUSB
	 * voltage to 3.3V
	 */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0x0,
						VUSB_CFG_GRP);
	/* Program the VUSB_CFG_TRANS for ACTIVE state. */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0x0,
						VUSB_CFG_TRANS);
	/* Program the VUSB_CFG_TRANS for ACTIVE state. */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0x0,
						VUSB_CFG_TRANS);

	/* Program the VUSB_CFG_STATE register to ON on all groups. */
	twl6030_writeb(twl, TWL_MODULE_PM_RECEIVER, 0x0,
						VUSB_CFG_STATE);
#if 1
	u8 data2;
	twl_i2c_read_u8(TWL6030_MODULE_ID0, &data2, 0xE6);
	printk("!!!!!BBSPOR_CFG = %2x\n",data2);
	twl_i2c_write_u8(TWL6030_MODULE_ID0, data2 | 0x20, 0xE6);
	printk("!!!!!BBSPOR_CFG = %2x\n",data2 | 0x20);
#endif
	/* Program the USB_VBUS_CTRL_SET and set VBUS_ACT_COMP bit */
	twl6030_writeb(twl, TWL_MODULE_USB, 0x4, USB_VBUS_CTRL_SET);
#ifdef CONFIG_USB_MUSB_HDRC_HCD
	/*
	 * Program the USB_ID_CTRL_SET register to enable GND drive
	 * and the ID comparators
	 */
	twl6030_writeb(twl, TWL_MODULE_USB, 0x4, USB_ID_CTRL_SET);
#endif
	return 0;

}

static ssize_t twl6030_usb_vbus_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct twl6030_usb *twl = dev_get_drvdata(dev);
	unsigned long flags;
	int ret = -EINVAL;

	spin_lock_irqsave(&twl->lock, flags);

	switch (twl->linkstat) {
	case USB_EVENT_VBUS:
	       ret = snprintf(buf, PAGE_SIZE, "vbus\n");
	       break;
	case USB_EVENT_ID:
	       ret = snprintf(buf, PAGE_SIZE, "id\n");
	       break;
	case USB_EVENT_NONE:
	       ret = snprintf(buf, PAGE_SIZE, "none\n");
	       break;
	default:
	       ret = snprintf(buf, PAGE_SIZE, "UNKNOWN\n");
	}
	spin_unlock_irqrestore(&twl->lock, flags);

	return ret;
}
static DEVICE_ATTR(vbus, 0444, twl6030_usb_vbus_show, NULL);
/*
Read 0x0: Wait state
Read 0x1: No contact
Read 0x2: PS/2
Read 0x3: Unknown error
Read 0x4: Dedicated charger
Read 0x5: HOST charger
Read 0x6: PC
Read 0x7: Interrupt
*/
#define PHY_DETECT_WAIT 0
#define PHY_DETECT_NO_CONTACT 1
#define PHY_DETECT_PS2 2
#define PHY_DETECT_UNKNOW_ERROE 3
#define PHY_DETECT_DEDICATED_CHARGER 4
#define PHY_DETECT_HOST_CHARGER 5
#define PHY_DETECT_PC 6
#define PHY_DETECT_INTERRUPT 7
int detect_usb()
{
    u32 value;
    int i=0;
    u32 usb2phy;
    //OTG_INTERFSEL PHY interface is 12-pin, 8-bit SDR ULPI
    __raw_writel(0x1,OMAP2_L4_IO_ADDRESS(0x4a0a0000 + (0xb40c)));
    __raw_writel(~PHY_PD, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
    usb2phy=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));
    if(usb2phy&0x40000000 ){            //ROM code disable detectcharger function
        __raw_writel(0,OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));     //enable the detect charger fuction
    }
    while(1){
        value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));
        usb2phy=value;
        value&=0xE00000;value=value>>21;
        i++;
        if(value!=0)
            break;
               
        if(i>=20000000 || i<0)
            break;
    }
    //printk("%s : usb2phy =0x%x value=0x%x i=%d\n",__func__,usb2phy,value,i);
    switch(value){
        case PHY_DETECT_PC:
            printk("%s : Source is PC\n",__func__);
        break;
        case PHY_DETECT_DEDICATED_CHARGER:
            printk("%s : Source is Dedicated charger\n",__func__);
        break;
        case PHY_DETECT_HOST_CHARGER:
            printk("%s : USB   HOST charger\n",__func__);
        break;
        case PHY_DETECT_INTERRUPT:
            printk("%s : INTERRUPT \n",__func__);
        break;
        case PHY_DETECT_UNKNOW_ERROE:
            printk("%s : Unknown error \n",__func__);
        break;
        case PHY_DETECT_NO_CONTACT:
            printk("%s : No contact \n",__func__);
        break;
        default:
            value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));  
  
            //work around,need to fixed
            //This will happen when boot by usb apapter without d+/d- shorter
            if(value==0x80260)
                value=PHY_DETECT_UNKNOW_ERROE;
            printk("%s Detect Error:",__func__);
        break;
    }
    //OTG_INTERFSEL PHY Embedded PHY interface is 8-bit, UTMI+
    __raw_writel(0x0,OMAP2_L4_IO_ADDRESS(0x4a0a0000 + (0xb40c)));
    __raw_writel(PHY_PD, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
    return value;
}

void vbus_monitor_work_func(struct work_struct *work)
{
    struct twl6030_usb *twl = container_of(work,
                struct twl6030_usb, vbus_work);
    int event  = -1;    unsigned long flags;
    int vbus_state, hw_state;
    int detect_result=0;u32 value=0;
    //spin_lock_irqsave(&twl->lock, flags);
#ifdef CONFIG_USB_MUSB_HDRC_HCD
    hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);
#endif 
    vbus_state = twl6030_readb(twl, TWL6030_MODULE_CHARGER,
						CONTROLLER_STAT1);
    /* AC unplugg can also generate this IRQ
     * we only call the notifier in case of VBUS change
     */
    if (twl->prev_vbus != (vbus_state & VBUS_DET)) {
        //printk("\n%s twl->prev_vbus=%d vbus_state=%d \n",__func__,twl->prev_vbus,vbus_state & VBUS_DET);
        twl->prev_vbus = vbus_state & VBUS_DET;
#ifdef CONFIG_USB_MUSB_HDRC_HCD    
        if (!(hw_state & STS_USB_ID)) {
#endif        
            if (vbus_state & VBUS_DET) {
                wake_lock(&twlusb_lock);
                regulator_enable(twl->vusb);
                if(twl->mbid==0){//detect by omap
                    //printk("%s DETECT_BY_PHY\n",__func__);
                    
                    blocking_notifier_call_chain(&twl->otg.notifier,USB_EVENT_DETECT_SOURCE, twl->otg.gadget);
                    detect_result=detect_usb();
                    if(detect_result==PHY_DETECT_PC)
                        event = USB_EVENT_VBUS;
                    else if(detect_result==PHY_DETECT_DEDICATED_CHARGER)
                        event = USB_EVENT_CHARGER;
                    else if(detect_result==PHY_DETECT_HOST_CHARGER)
                        event = USB_EVENT_HOST_CHARGER;
                    else 
                        event = USB_EVENT_CHARGER;
                    twl->otg.default_a = false;
                    twl->otg.state = OTG_STATE_B_IDLE;
                }else{//detect by charger
                    //printk("%s DETECT_BY_CHARGER\n",__func__);
                    __raw_writel(0x40000000,OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));
                    event = USB_EVENT_DETECT_SOURCE;
                    twl->otg.default_a = false;
                    twl->otg.state = OTG_STATE_B_IDLE;
                }
            }else{
                //printk("!!%s USB_EVENT_NONE\n\n",__func__);
                event = USB_EVENT_NONE;
                //regulator_disable(twl->vusb);
            }
            if (event >= 0) {
                twl->linkstat = event;
                blocking_notifier_call_chain(&twl->otg.notifier,event, twl->otg.gadget);
            }
#ifdef CONFIG_USB_MUSB_HDRC_HCD            
        }
#endif        
        sysfs_notify(&twl->dev->kobj, NULL, "vbus");
    }
    if(event == USB_EVENT_NONE) {
        wake_unlock(&twlusb_lock);
    }
}
static irqreturn_t twl6030_usb_irq(int irq, void *_twl)
{
    struct twl6030_usb *twl = _twl;
    queue_work(vbusid_work_queue, &twl->vbus_work);
    //spin_unlock_irqrestore(&twl->lock, flags);
    return IRQ_HANDLED;
}
void id_monitor_work_func(struct work_struct *work)
{
    struct twl6030_usb *twl = container_of(work,
                struct twl6030_usb, vbus_work);
	int status = USB_EVENT_NONE;
	u8 hw_state;
#ifdef CONFIG_USB_MUSB_HDRC_HCD
	hw_state = twl6030_readb(twl, TWL6030_MODULE_ID0, STS_HW_CONDITIONS);

	if (hw_state & STS_USB_ID) {

		twl6030_writeb(twl, TWL_MODULE_USB, 0x1, USB_ID_INT_EN_HI_CLR);
		twl6030_writeb(twl, TWL_MODULE_USB, 0x10, USB_ID_INT_EN_HI_SET);
		status = USB_EVENT_ID;
		twl->otg.default_a = true;
		twl->otg.state = OTG_STATE_A_IDLE;
		blocking_notifier_call_chain(&twl->otg.notifier, status,
							twl->otg.gadget);
	} else  {
		twl6030_writeb(twl, TWL_MODULE_USB, 0x10, USB_ID_INT_EN_HI_CLR);
		twl6030_writeb(twl, TWL_MODULE_USB, 0x1, USB_ID_INT_EN_HI_SET);
	}
	twl6030_writeb(twl, TWL_MODULE_USB, status, USB_ID_INT_LATCH_CLR);
	twl->linkstat = status;
#endif
    
}
void twl6030_set_input_current_limit(struct otg_transceiver *x,unsigned mA)
{
    struct twl6030_usb *twl = xceiv_to_twl(x);
    int event=0;
    switch(mA){
        case 0:
            event = USB_EVENT_LIMIT_0;
        break;
        case 100:
            event = USB_EVENT_LIMIT_100;
        break;
        case 500:
            event = USB_EVENT_LIMIT_500;
        break;
    }
    blocking_notifier_call_chain(&twl->otg.notifier,event, twl->otg.gadget);
}

static irqreturn_t twl6030_usbotg_irq(int irq, void *_twl)
{
	struct twl6030_usb *twl = _twl;
	
        queue_work(vbusid_work_queue, &twl->id_work);
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

static int twl6030_set_hz_mode(struct otg_transceiver *x, bool enabled)
{
	u8 val;
	struct twl6030_usb *twl;

	if (!x)
		return -ENODEV;

	twl = xceiv_to_twl(x);

	/* set/reset USB charger in High impedence mode on VBUS */
	val = twl6030_readb(twl, TWL_MODULE_MAIN_CHARGE,
						CHARGERUSB_CTRL1);

	if (enabled)
		val |= HZ_MODE;
	else
		val &= ~HZ_MODE;

	twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE , val,
						CHARGERUSB_CTRL1);

	return 0;
}

static int twl6030_set_vbus(struct otg_transceiver *x, bool enabled)
{
	struct twl6030_usb *twl = xceiv_to_twl(x);

	/*
	 * Start driving VBUS. Set OPA_MODE bit in CHARGERUSB_CTRL1
	 * register. This enables boost mode.
	 */
	if (enabled)
		twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE , 0x40,
						CHARGERUSB_CTRL1);
	 else
		twl6030_writeb(twl, TWL_MODULE_MAIN_CHARGE , 0x00,
						CHARGERUSB_CTRL1);
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
     printk("%s\n",__func__);
     struct twl6030_usb *twl = xceiv_to_twl(x);
	set_phy_clk(x, 0);
	__raw_writel(PHY_PD, ctrl_base + CONTROL_DEV_CONF);
     regulator_disable(twl->vusb);
}
extern u8 quanta_get_mbid(void);
static int __devinit twl6030_usb_probe(struct platform_device *pdev)
{
	struct twl6030_usb	*twl;
	int			status,err,vbus_state;
	struct twl4030_usb_data *pdata;
	struct device *dev = &pdev->dev;
	pdata = dev->platform_data;

	twl = kzalloc(sizeof *twl, GFP_KERNEL);
	if (!twl)
		return -ENOMEM;

	twl->dev		= &pdev->dev;
	twl->irq1		= platform_get_irq(pdev, 0);
	twl->irq2		= platform_get_irq(pdev, 1);
	twl->otg.dev		= twl->dev;
	twl->otg.label		= "twl6030";
	twl->otg.set_host	= twl6030_set_host;
	twl->otg.set_peripheral	= twl6030_set_peripheral;
	twl->otg.set_suspend	= twl6030_set_suspend;
	twl->asleep		= 1;
	#ifndef CONFIG_USB_ETH_RNDIS
	twl->otg.set_power	= twl6030_set_input_current_limit;
	twl->otg.set_vbus	= twl6030_set_vbus;
	#endif
	twl->otg.set_hz_mode	= twl6030_set_hz_mode;
	twl->otg.init		= phy_init;
	twl->otg.shutdown	= phy_shutdown;
	twl->otg.enable_irq	= twl6030_enable_irq;
	twl->otg.set_clk	= set_phy_clk;
	twl->otg.shutdown	= phy_shutdown;
	twl->prev_vbus		= 0;
	twl->vusb = regulator_get(NULL, "usb-phy");
        if (IS_ERR(twl->vusb)) {
            pr_err("Unable to get usb-phy regulator\n");
        }
        
        twl->mbid=quanta_get_mbid();
        printk(KERN_INFO "%s mbid=%d\n",__func__,twl->mbid);
        err = regulator_set_voltage(twl->vusb, 3300000,
                    3300000);
        //regulator_disable(twl->vusb);

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
        INIT_WORK(&twl->vbus_work, vbus_monitor_work_func);
        INIT_WORK(&twl->id_work, id_monitor_work_func);
	/* Our job is to use irqs and status from the power module
	 * to keep the transceiver disabled when nothing's connected.
	 *
	 * FIXME we actually shouldn't start enabling it until the
	 * USB controller drivers have said they're ready, by calling
	 * set_host() and/or set_peripheral() ... OTG_capable boards
	 * need both handles, otherwise just one suffices.
	 */
	twl->irq_enabled = true;
	status = request_threaded_irq(twl->irq1, NULL, twl6030_usbotg_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_usb", twl);
	if (status < 0) {
		dev_dbg(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq1, status);
		kfree(twl);
		return status;
	}

	status = request_threaded_irq(twl->irq2, NULL, twl6030_usb_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"twl6030_usb", twl);
	if (status < 0) {
		dev_dbg(&pdev->dev, "can't get IRQ %d, err %d\n",
			twl->irq2, status);
		kfree(twl);
		return status;
	}

        vbus_state = twl6030_readb(twl, TWL6030_MODULE_CHARGER,
						CONTROLLER_STAT1);
        wake_lock_init(&twlusb_lock, WAKE_LOCK_SUSPEND, "usb_wake_lock");

	ctrl_base = ioremap(0x4A002000, SZ_1K);
	/* power down the phy by default can be enabled on connect */
	__raw_writel(PHY_PD, ctrl_base + CONTROL_DEV_CONF);

	dev_info(&pdev->dev, "Initialized TWL6030 USB module\n");
	return 0;
}

static int __exit twl6030_usb_remove(struct platform_device *pdev)
{
	struct twl6030_usb *twl = platform_get_drvdata(pdev);

	free_irq(twl->irq1, twl);
    free_irq(twl->irq2, twl);
	device_remove_file(twl->dev, &dev_attr_vbus);
        regulator_put(twl->vusb);
	twl6030_interrupt_mask(TWL6030_USBOTG_INT_MASK,
		REG_INT_MSK_LINE_C);
	twl6030_interrupt_mask(TWL6030_USBOTG_INT_MASK,
			REG_INT_MSK_STS_C);
	kfree(twl);
	iounmap(ctrl_base);

	return 0;
}

static int twl6030_usb_suspend(struct platform_device *pdev){
    struct twl6030_usb *twl = platform_get_drvdata(pdev);	
    return 0;
}

static int twl6030_usb_resume(struct platform_device *pdev){
    struct twl6030_usb *twl = platform_get_drvdata(pdev);
    //printk("%s \n",__func__);
    int vbus_state = twl6030_readb(twl, TWL6030_MODULE_CHARGER,
						CONTROLLER_STAT1);
    /* AC unplugg can also generate this IRQ
     * we only call the notifier in case of VBUS change
     */
    if (twl->prev_vbus != (vbus_state & VBUS_DET)) {	 
                wake_lock(&twlusb_lock);
		queue_work(vbusid_work_queue, &twl->vbus_work);
    }
    /* Program MISC2 register and set bit VUSB_IN_VBAT */
    twl6030_writeb(twl, TWL6030_MODULE_ID0 , 0x10, MISC2);
    return 0;
}

static struct platform_driver twl6030_usb_driver = {
	.probe		= twl6030_usb_probe,
	.remove		= __exit_p(twl6030_usb_remove),
	.driver		= {
		.name	= "twl6030_usb",
		.owner	= THIS_MODULE,
	},
	.suspend	= twl6030_usb_suspend,
	.resume		= twl6030_usb_resume,
};

static int __init twl6030_usb_init(void)
{
    vbusid_work_queue = create_singlethread_workqueue("vbus-id");
    if (vbusid_work_queue == NULL) {
        //ret = -ENOMEM;
    }
    return platform_driver_register(&twl6030_usb_driver);
}
subsys_initcall(twl6030_usb_init);

static void __exit twl6030_usb_exit(void)
{
    destroy_workqueue(vbusid_work_queue);
    platform_driver_unregister(&twl6030_usb_driver);
}
module_exit(twl6030_usb_exit);

MODULE_ALIAS("platform:twl6030_usb");
MODULE_AUTHOR("Texas Instruments, Inc, Nokia Corporation");
MODULE_DESCRIPTION("TWL6030 USB transceiver driver");
MODULE_LICENSE("GPL");
