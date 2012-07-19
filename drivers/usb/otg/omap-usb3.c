/*
 * omap-usb3 - USB PHY, talking to dwc3 controller in OMAP.
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Author: Kishon Vijay Abraham I <kishon@ti.com>
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/usb/omap_usb.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/mfd/omap_control.h>
#include <linux/usb/omap4_usb_phy.h>

static struct usb_dpll_params omap_usb3_dpll_params[NUM_SYS_CLKS] = {
	{1250, 5, 4, 20, 0},		/* 12 MHz */
	{3125, 20, 4, 20, 0},		/* 16.8 MHz */
	{1172, 8, 4, 20, 65537},	/* 19.2 MHz */
	{1250, 12, 4, 20, 0},		/* 26 MHz */
	{3125, 47, 4, 20, 92843},	/* 38.4 MHz */
};

static int omap_usb3_suspend(struct usb_phy *x, int suspend)
{
	struct omap_usb *phy = phy_to_omapusb(x);
	u32	val, ret;
	int timeout = PLL_IDLE_TIME;

	if (suspend && !phy->is_suspended) {
		pm_runtime_put_sync(phy->dev);
		clk_disable(phy->wkupclk);
		clk_disable(phy->optclk);

		val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
		val |= PLL_IDLE;
		omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

		do {
			val = omap_usb_readl(phy->pll_ctrl_base, PLL_STATUS);
			if (val & PLL_TICOPWDN)
				break;
			udelay(1);
		} while (--timeout);

		omap5_usb_phy_power(phy->control_dev, 0);

		phy->is_suspended	= 1;
	} else if (!suspend && phy->is_suspended) {
		phy->is_suspended	= 0;
		ret = clk_enable(phy->optclk);
		if (ret) {
			dev_err(phy->dev, "Failed to enable optclk %d\n", ret);
			goto err3;
		}
		ret = clk_enable(phy->wkupclk);
		if (ret) {
			dev_err(phy->dev, "Failed to enable wkupclk %d\n", ret);
			goto err2;
		}
		ret = pm_runtime_get_sync(phy->dev);
		if (ret < 0) {
			dev_err(phy->dev, "get_sync failed with err %d\n",
									ret);
			goto err1;
		}

		val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
		val &= ~PLL_IDLE;
		omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

		do {
			val = omap_usb_readl(phy->pll_ctrl_base, PLL_STATUS);
			if (!(val & PLL_TICOPWDN))
				break;
			udelay(1);
		} while (--timeout);
	}

	return 0;

err1:
	clk_disable(phy->wkupclk);
err2:
	clk_disable(phy->optclk);
err3:
	return ret;
}

static inline enum sys_clk_rate __get_sys_clk_index(unsigned long rate)
{
	switch (rate) {
	case 12000000:
		return CLK_RATE_12MHZ;
	case 16800000:
		return CLK_RATE_16MHZ;
	case 19200000:
		return CLK_RATE_19MHZ;
	case 26000000:
		return CLK_RATE_26MHZ;
	case 38400000:
		return CLK_RATE_38MHZ;
	default:
		return CLK_RATE_UNDEFINED;
	}
}

static void omap_usb_dpll_relock(struct omap_usb *phy)
{
	u32		val;
	unsigned long	timeout;

	omap_usb_writel(phy->pll_ctrl_base, PLL_GO, SET_PLL_GO);

	timeout = jiffies + msecs_to_jiffies(20);
	do {
		val = omap_usb_readl(phy->pll_ctrl_base, PLL_STATUS);
		if (val & PLL_LOCK)
			break;
	} while (!time_after(jiffies, timeout));
}

static int omap_usb_dpll_lock(struct omap_usb *phy)
{
	u32			val;
	struct clk		*sys_clk;
	unsigned long		rate;
	enum sys_clk_rate	clk_index;

	sys_clk	= clk_get(NULL, "sys_clkin");
	if (IS_ERR(sys_clk)) {
		pr_err("unable to get sys_clkin\n");
		return PTR_ERR(sys_clk);
	}

	rate		= clk_get_rate(sys_clk);
	clk_index	= __get_sys_clk_index(rate);

	if (clk_index == CLK_RATE_UNDEFINED) {
		pr_err("dpll cannot be locked for the sys clk frequency:%luHz\n", rate);
		return -EINVAL;
	}

	val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION1);
	val &= ~PLL_REGN_MASK;
	val |= omap_usb3_dpll_params[clk_index].n << PLL_REGN_SHIFT;
	omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION1, val);

	val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION2);
	val &= ~PLL_SELFREQDCO_MASK;
	val |= omap_usb3_dpll_params[clk_index].freq << PLL_SELFREQDCO_SHIFT;
	omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION2, val);

	val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION1);
	val &= ~PLL_REGM_MASK;
	val |= omap_usb3_dpll_params[clk_index].m << PLL_REGM_SHIFT;
	omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION1, val);

	val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION4);
	val &= ~PLL_REGM_F_MASK;
	val |= omap_usb3_dpll_params[clk_index].mf << PLL_REGM_F_SHIFT;
	omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION4, val);

	val = omap_usb_readl(phy->pll_ctrl_base, PLL_CONFIGURATION3);
	val &= ~PLL_SD_MASK;
	val |= omap_usb3_dpll_params[clk_index].sd << PLL_SD_SHIFT;
	omap_usb_writel(phy->pll_ctrl_base, PLL_CONFIGURATION3, val);

	omap_usb_dpll_relock(phy);

	return 0;
}

static int omap_usb3_init(struct usb_phy *x)
{
	struct omap_usb	*phy = phy_to_omapusb(x);

	omap_usb_dpll_lock(phy);
	omap5_usb_phy_partial_powerup(phy->control_dev);
	/*
	 * Give enough time for the PHY to partially power-up before
	 * powering it up completely. delay value suggested by the HW
	 * team.
	 */
	mdelay(100);
	omap5_usb_phy_power(phy->control_dev, 1);

	return 0;
}

static void omap_usb3_poweron(struct usb_phy *x)
{
	struct omap_usb *phy = phy_to_omapusb(x);

	omap5_usb_phy_power(phy->control_dev, 1);
}

static void omap_usb3_shutdown(struct usb_phy *x)
{
	struct omap_usb *phy = phy_to_omapusb(x);

	omap5_usb_phy_power(phy->control_dev, 0);
}

static int __devinit omap_usb3_probe(struct platform_device *pdev)
{
	struct omap_usb			*phy;
	struct resource			*res;

	phy = devm_kzalloc(&pdev->dev, sizeof *phy, GFP_KERNEL);
	if (!phy) {
		dev_err(&pdev->dev, "unable to allocate memory for OMAP USB3 PHY\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pll_ctrl");
	if (!res) {
		dev_err(&pdev->dev, "unable to get base address of pll_ctrl\n");
		return -ENODEV;
	}

	phy->pll_ctrl_base = devm_ioremap(&pdev->dev, res->start,
							resource_size(res));
	if (!phy->pll_ctrl_base) {
		dev_err(&pdev->dev, "ioremap of pll_ctrl failed\n");
		return -ENOMEM;
	}

	phy->dev		= &pdev->dev;

	phy->phy.dev		= phy->dev;
	phy->phy.label		= "omap-usb3";
	phy->phy.init		= omap_usb3_init;
	phy->phy.poweron	= omap_usb3_poweron;
	phy->phy.shutdown	= omap_usb3_shutdown;
	phy->phy.set_suspend	= omap_usb3_suspend;

	phy->control_dev	= omap_control_get();

	phy->is_suspended	= 1;
	omap5_usb_phy_power(phy->control_dev, 0);

	phy->wkupclk = clk_get(phy->dev, "usb_phy_cm_clk32k");
	phy->optclk = clk_get(phy->dev, "usb_otg_ss_refclk960m");

	usb_add_phy(&phy->phy, USB_PHY_TYPE_USB3);

	platform_set_drvdata(pdev, phy);

	pm_runtime_enable(phy->dev);

	return 0;
}

static int __devexit omap_usb3_remove(struct platform_device *pdev)
{
	struct omap_usb *phy = platform_get_drvdata(pdev);

	usb_remove_phy(&phy->phy);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver omap_usb3_driver = {
	.probe		= omap_usb3_probe,
	.remove		= __devexit_p(omap_usb3_remove),
	.driver		= {
		.name	= "omap-usb3",
		.owner	= THIS_MODULE,
	},
};

static int __init usb3_omap_init(void)
{
	return platform_driver_register(&omap_usb3_driver);
}
arch_initcall(usb3_omap_init);

static void __exit usb3_omap_exit(void)
{
	platform_driver_unregister(&omap_usb3_driver);
}
module_exit(usb3_omap_exit);

MODULE_ALIAS("platform: omap_usb3");
MODULE_AUTHOR("Kishon Vijay Abraham I <kishon@ti.com>");
MODULE_DESCRIPTION("OMAP USB3 PHY DRIVER");
MODULE_LICENSE("GPL");
