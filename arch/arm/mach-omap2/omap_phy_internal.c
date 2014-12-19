/*
  * This file configures the internal USB PHY in OMAP4430. Used
  * with TWL6030 transceiver and MUSB on OMAP4430.
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

#include <linux/types.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/usb.h>
#include <linux/power_supply.h>

#include <plat/usb.h>
#include "control.h"

/* OMAP control module register for UTMI PHY */
#define CONTROL_DEV_CONF		0x300
#define PHY_PD				0x1

#define USBOTGHS_CONTROL		0x33c
#define	AVALID				BIT(0)
#define	BVALID				BIT(1)
#define	VBUSVALID			BIT(2)
#define	SESSEND				BIT(3)
#define	IDDIG				BIT(4)

#define CONTROL_USB2PHYCORE		0x620
#define CHARGER_TYPE_PS2		0x2
#define CHARGER_TYPE_DEDICATED		0x4
#define CHARGER_TYPE_HOST		0x5
#define CHARGER_TYPE_PC			0x6
#define USB2PHY_CHGDETECTED		BIT(13)
#define USB2PHY_RESTARTCHGDET		BIT(15)
#define USB2PHY_DISCHGDET		BIT(30)

static struct clk *phyclk, *clk48m, *clk32k;
static void __iomem *ctrl_base;
static int usbotghs_control;

#ifdef CONFIG_OMAP4_HSOTG_ED_CORRECTION
#define OMAP4_HSOTG_SWTRIM_MASK		0xFFFF00FF
#define OMAP4_HSOTG_REF_GEN_TEST_MASK	0xF8FFFFFF
static void __iomem *hsotg_base;
#endif

int omap4430_phy_init(struct device *dev)
{
	u32 usb2phycore;
	ctrl_base = ioremap(OMAP443X_SCM_BASE, SZ_1K);
#ifdef CONFIG_OMAP4_HSOTG_ED_CORRECTION
	hsotg_base = ioremap(OMAP44XX_HSUSB_OTG_BASE, SZ_16K);
	if (!hsotg_base) {
		dev_err(dev, "hsotg memory ioremap failed\n");
		return -ENOMEM;
	}

#endif
	if (!ctrl_base) {
		pr_err("control module ioremap failed\n");
		return -ENOMEM;
	}
	/* Power down the phy */
	__raw_writel(PHY_PD, ctrl_base + CONTROL_DEV_CONF);

	/* Disable charger detection by default */
	usb2phycore = omap4_ctrl_pad_readl(CONTROL_USB2PHYCORE);
	usb2phycore |= USB2PHY_DISCHGDET;
	omap4_ctrl_pad_writel(usb2phycore, CONTROL_USB2PHYCORE);

	if (!dev) {
		iounmap(ctrl_base);
		return 0;
	}

	phyclk = clk_get(dev, "ocp2scp_usb_phy_ick");
	if (IS_ERR(phyclk)) {
		dev_err(dev, "cannot clk_get ocp2scp_usb_phy_ick\n");
		iounmap(ctrl_base);
		return PTR_ERR(phyclk);
	}

	clk48m = clk_get(dev, "ocp2scp_usb_phy_phy_48m");
	if (IS_ERR(clk48m)) {
		dev_err(dev, "cannot clk_get ocp2scp_usb_phy_phy_48m\n");
		clk_put(phyclk);
		iounmap(ctrl_base);
		return PTR_ERR(clk48m);
	}

	clk32k = clk_get(dev, "usb_phy_cm_clk32k");
	if (IS_ERR(clk32k)) {
		dev_err(dev, "cannot clk_get usb_phy_cm_clk32k\n");
		clk_put(phyclk);
		clk_put(clk48m);
		iounmap(ctrl_base);
		return PTR_ERR(clk32k);
	}
	return 0;
}

#ifdef CONFIG_OMAP4_HSOTG_ED_CORRECTION
static void omap44xx_hsotg_ed_correction(void)
{
	u32 val;

	/*
	 * Software workaround #1
	 * By this way we improve HS OTG
	 * eye diagramm by 2-3%
	 * Allow this change for all OMAP4 family
	 */

	/*
	 * For prevent 4-bit shift issue
	 * bit field SYNC2 of OCP2SCP_TIMING
	 * should be set to value >6
	 */

	val = __raw_readl(hsotg_base + 0x2018);
	val |= 0x0F;
	__raw_writel(val, hsotg_base + 0x2018);

	/*
	 * USBPHY_ANA_CONFIG2[16:15] = RTERM_TEST = 11b
	 */
	val = __raw_readl(hsotg_base + 0x20D4);
	val |= (3<<15);
	__raw_writel(val, hsotg_base + 0x20D4);

	/*
	 * USBPHY_TERMINATION_CONTROL[13:11] = HS_CODE_SEL = 011b
	 */
	val = __raw_readl(hsotg_base + 0x2080);
	val &= ~(7<<11);
	val |= (3<<11);
	__raw_writel(val, hsotg_base + 0x2080);

	/*
	 * Software workaround #2
	 * Reducing interface output impedance
	 * By this way we improve HS OTG eye diagramm by 8%
	 * This change needed only for 4430 CPUs
	 * because this change can impact Rx performance
	 */

	/*
	 * Increase SWCAP trim code by 0x24
	 * NOTE: Value should be between 0 and 0x24
	 */
	val = __raw_readl(hsotg_base + 0x20B8);
	if (is_omap443x() && !(val & 0x8000)) {
		val = min((val + (0x24<<8)), (val | (0x7F<<8))) | 0x8000;
		__raw_writel(val, hsotg_base + 0x20B8);
	}

	/*
	 * For 4460 and 4470 CPUs there is 10-15mV adjustable
	 * improvement available via REF_GEN_TEST[26:24]=110
	 */
	if (is_omap446x() || is_omap447x()) {
		val = __raw_readl(hsotg_base + 0x20D4);
		val &= OMAP4_HSOTG_REF_GEN_TEST_MASK;
		val |= (0x6<<24);
		__raw_writel(val, hsotg_base + 0x20D4);
	}
}
#endif

static int omap4430_phy_set_clk(struct device *dev, int on)
{
	static int state;

	if (on && !state) {
		/* Enable the phy clocks */
		clk_enable(phyclk);
		clk_enable(clk48m);
		clk_enable(clk32k);
		state = 1;
	} else if (!on && state) {
		/* Disable the phy clocks */
		clk_disable(phyclk);
		clk_disable(clk48m);
		clk_disable(clk32k);
		state = 0;
	}
	return 0;
}

int omap4_charger_detect(void)
{
	unsigned long timeout;
	int charger = POWER_SUPPLY_TYPE_USB;
	u32 usb2phycore = 0;
	u32 chargertype = 0;

	/* enable charger detection and restart it */
	usb2phycore = omap4_ctrl_pad_readl(CONTROL_USB2PHYCORE);
	usb2phycore &= ~USB2PHY_DISCHGDET;
	usb2phycore |= USB2PHY_RESTARTCHGDET;
	omap4_ctrl_pad_writel(usb2phycore, CONTROL_USB2PHYCORE);
	mdelay(2);
	usb2phycore = omap4_ctrl_pad_readl(CONTROL_USB2PHYCORE);
	usb2phycore &= ~USB2PHY_RESTARTCHGDET;
	omap4_ctrl_pad_writel(usb2phycore, CONTROL_USB2PHYCORE);

	timeout = jiffies + msecs_to_jiffies(500);
	do {
		usb2phycore = omap4_ctrl_pad_readl(CONTROL_USB2PHYCORE);
		chargertype = ((usb2phycore >> 21) & 0x7);
		if (usb2phycore & USB2PHY_CHGDETECTED)
			break;
		msleep_interruptible(10);
	} while (!time_after(jiffies, timeout));

	switch (chargertype) {
	case CHARGER_TYPE_DEDICATED:
		charger = POWER_SUPPLY_TYPE_USB_DCP;
		pr_info("DCP detected\n");
		break;
	case CHARGER_TYPE_HOST:
		charger = POWER_SUPPLY_TYPE_USB_CDP;
		pr_info("CDP detected\n");
		break;
	case CHARGER_TYPE_PC:
		charger = POWER_SUPPLY_TYPE_USB;
		pr_info("PC detected\n");
		break;
	case CHARGER_TYPE_PS2:
		pr_info("PS/2 detected!\n");
		break;
	default:
		pr_err("Unknown charger detected! %d\n", chargertype);
	}

	usb2phycore = omap4_ctrl_pad_readl(CONTROL_USB2PHYCORE);
	usb2phycore |= USB2PHY_DISCHGDET;
	omap4_ctrl_pad_writel(usb2phycore, CONTROL_USB2PHYCORE);

	return charger;
}

int omap4430_phy_power(struct device *dev, int ID, int on)
{
	if (on) {

#ifdef CONFIG_OMAP4_HSOTG_ED_CORRECTION
		/* apply eye diagram improvement settings */
		omap44xx_hsotg_ed_correction();
#endif

		if (ID)
			/* enable VBUS valid, IDDIG groung */
			__raw_writel(AVALID | VBUSVALID, ctrl_base +
							USBOTGHS_CONTROL);
		else
			/*
			 * Enable VBUS Valid, AValid and IDDIG
			 * high impedance
			 */
			__raw_writel(IDDIG | AVALID | VBUSVALID,
						ctrl_base + USBOTGHS_CONTROL);
	} else {
		/* Enable session END and IDIG to high impedance. */
		__raw_writel(SESSEND | IDDIG, ctrl_base +
					USBOTGHS_CONTROL);
	}
	return 0;
}

int omap4430_phy_suspend(struct device *dev, int suspend)
{
	if (suspend) {
		/* Disable the clocks */
		omap4430_phy_set_clk(dev, 0);
		/* Power down the phy */
		__raw_writel(PHY_PD, ctrl_base + CONTROL_DEV_CONF);

		/* save the context */
		usbotghs_control = __raw_readl(ctrl_base + USBOTGHS_CONTROL);
	} else {
		/* Enable the internel phy clcoks */
		omap4430_phy_set_clk(dev, 1);
		/* power on the phy */
		if (__raw_readl(ctrl_base + CONTROL_DEV_CONF) & PHY_PD)
			__raw_writel(~PHY_PD, ctrl_base + CONTROL_DEV_CONF);

		/* restore the context */
		__raw_writel(usbotghs_control, ctrl_base + USBOTGHS_CONTROL);
	}

	return 0;
}

int omap4430_phy_exit(struct device *dev)
{
	if (ctrl_base)
		iounmap(ctrl_base);
	if (phyclk)
		clk_put(phyclk);
	if (clk48m)
		clk_put(clk48m);
	if (clk32k)
		clk_put(clk32k);

	return 0;
}

void am35x_musb_reset(void)
{
	u32	regval;

	/* Reset the musb interface */
	regval = omap_ctrl_readl(AM35XX_CONTROL_IP_SW_RESET);

	regval |= AM35XX_USBOTGSS_SW_RST;
	omap_ctrl_writel(regval, AM35XX_CONTROL_IP_SW_RESET);

	regval &= ~AM35XX_USBOTGSS_SW_RST;
	omap_ctrl_writel(regval, AM35XX_CONTROL_IP_SW_RESET);

	regval = omap_ctrl_readl(AM35XX_CONTROL_IP_SW_RESET);
}

void am35x_musb_phy_power(u8 on)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(100);
	u32 devconf2;

	if (on) {
		/*
		 * Start the on-chip PHY and its PLL.
		 */
		devconf2 = omap_ctrl_readl(AM35XX_CONTROL_DEVCONF2);

		devconf2 &= ~(CONF2_RESET | CONF2_PHYPWRDN | CONF2_OTGPWRDN);
		devconf2 |= CONF2_PHY_PLLON;

		omap_ctrl_writel(devconf2, AM35XX_CONTROL_DEVCONF2);

		pr_info(KERN_INFO "Waiting for PHY clock good...\n");
		while (!(omap_ctrl_readl(AM35XX_CONTROL_DEVCONF2)
				& CONF2_PHYCLKGD)) {
			cpu_relax();

			if (time_after(jiffies, timeout)) {
				pr_err(KERN_ERR "musb PHY clock good timed out\n");
				break;
			}
		}
	} else {
		/*
		 * Power down the on-chip PHY.
		 */
		devconf2 = omap_ctrl_readl(AM35XX_CONTROL_DEVCONF2);

		devconf2 &= ~CONF2_PHY_PLLON;
		devconf2 |=  CONF2_PHYPWRDN | CONF2_OTGPWRDN;
		omap_ctrl_writel(devconf2, AM35XX_CONTROL_DEVCONF2);
	}
}

void am35x_musb_clear_irq(void)
{
	u32 regval;

	regval = omap_ctrl_readl(AM35XX_CONTROL_LVL_INTR_CLEAR);
	regval |= AM35XX_USBOTGSS_INT_CLR;
	omap_ctrl_writel(regval, AM35XX_CONTROL_LVL_INTR_CLEAR);
	regval = omap_ctrl_readl(AM35XX_CONTROL_LVL_INTR_CLEAR);
}

void am35x_set_mode(u8 musb_mode)
{
	u32 devconf2 = omap_ctrl_readl(AM35XX_CONTROL_DEVCONF2);

	devconf2 &= ~CONF2_OTGMODE;
	switch (musb_mode) {
#ifdef	CONFIG_USB_MUSB_HDRC_HCD
	case MUSB_HOST:		/* Force VBUS valid, ID = 0 */
		devconf2 |= CONF2_FORCE_HOST;
		break;
#endif
#ifdef	CONFIG_USB_GADGET_MUSB_HDRC
	case MUSB_PERIPHERAL:	/* Force VBUS valid, ID = 1 */
		devconf2 |= CONF2_FORCE_DEVICE;
		break;
#endif
#ifdef	CONFIG_USB_MUSB_OTG
	case MUSB_OTG:		/* Don't override the VBUS/ID comparators */
		devconf2 |= CONF2_NO_OVERRIDE;
		break;
#endif
	default:
		pr_info(KERN_INFO "Unsupported mode %u\n", musb_mode);
	}

	omap_ctrl_writel(devconf2, AM35XX_CONTROL_DEVCONF2);
}
