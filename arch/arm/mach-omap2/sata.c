/**
 * sata.c - The ahci sata device init functions
 *
 * Copyright (C) 2011 Texas Instruments Incorporated - http://www.ti.com
 * Author: Keshava Munegowda <keshava_mgowda@ti.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2  of
 * the License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/platform_data/omap_ocp2scp.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/ahci_platform.h>
#include <linux/clk.h>
#include <plat/sata.h>

#include "omap_device.h"
#include "soc.h"

#define OMAP_SATA_HWMODNAME	"sata"
#define OMAP_OCP2SCP3_HWMODNAME	"ocp2scp3"
#define AHCI_PLAT_DEVNAME	"ahci"

#define OMAP_SATA_PLL_CONTROL					0x00
#define OMAP_SATA_PLL_STATUS					0x04
#define OMAP_SATA_PLL_STATUS_LOCK_SHIFT				1
#define OMAP_SATA_PLL_STATUS_LOCK				1
#define OMAP_SATA_PLL_STATUS_RESET_DONE				1
#define OMAP_SATA_PLL_STATUS_LDOPWDN_SHIFT			15
#define OMAP_SATA_PLL_STATUS_TICOPWDN_SHIFT			16

#define OMAP_SATA_PLL_GO					0x08

#define OMAP_SATA_PLL_CFG1					0x0c
#define OMAP_SATA_PLL_CFG1_M					625
/* M value for 20Mhz DRA7XX */
#define OMAP_SATA_PLL_CFG1_M_DRA7XX				600
#define OMAP_SATA_PLL_CFG1_M_MASK				0xfff
#define OMAP_SATA_PLL_CFG1_M_SHIFT				9

#define OMAP_SATA_PLL_CFG1_N					7
#define OMAP_SATA_PLL_CFG1_N_MASK				0xff
#define OMAP_SATA_PLL_CFG1_N_SHIFT				1

#define OMAP_SATA_PLL_CFG2					0x10
#define OMAP_SATA_PLL_CFG2_SELFREQDCO				4
#define OMAP_SATA_PLL_CFG2_SELFREQDCO_MASK			7
#define OMAP_SATA_PLL_CFG2_SELFREQDCO_SHIFT			1
#define OMAP_SATA_PLL_CFG2_REFSEL				0
#define OMAP_SATA_PLL_CFG2_REFSEL_MASK				3
#define OMAP_SATA_PLL_CFG2_REFSEL_SHIFT				21
#define OMAP_SATA_PLL_CFG2_LOCKSEL				1
#define OMAP_SATA_PLL_CFG2_LOCKSEL_MASK				3
#define OMAP_SATA_PLL_CFG2_LOCKSEL_SHIFT			9
#define OMAP_SATA_PLL_CFG2_IDLE					1
#define OMAP_SATA_PLL_CFG2_IDLE_MASK				1

#define OMAP_SATA_PLL_CFG3					0x14

#define OMAP_SATA_PLL_CFG3_SD					6
#define OMAP_SATA_PLL_CFG3_SD_MASK				0xff
#define OMAP_SATA_PLL_CFG3_SD_SHIF				10

#define OMAP_SATA_PLL_CFG4					0x20
#define OMAP_SATA_PLL_CFG4_REGM_F				0
#define OMAP_SATA_PLL_CFG4_REGM_F_MASK				0x3ffff
#define OMAP_SATA_PLL_CFG4_REGM2				1
#define OMAP_SATA_PLL_CFG4_REGM2_MASK				0x1fc
#define OMAP_SATA_PLL_CFG4_REGM2_SHIFT				18

#define OMAP_OCP2SCP3_SYSCONFIG					0x10
#define OMAP_OCP2SCP3_SYSCONFIG_RESET_MASK			1
#define OMAP_OCP2SCP3_SYSCONFIG_RESET_SHIFT			1
#define OMAP_OCP2SCP3_SYSCONFIG_RESET				1

#define OMAP_OCP2SCP3_SYSSTATUS					0x14
#define OMAP_OCP2SCP3_SYSSTATUS_RESETDONE			1

#define OMAP_OCP2SCP3_TIMING					0x18
#define OMAP_OCP2SCP3_TIMING_DIV_MASK				0x7
#define OMAP_OCP2SCP3_TIMING_DIV_SHIFT				7
#define OMAP_OCP2SCP3_TIMING_DIV				1
#define OMAP_OCP2SCP3_TIMING_SYNC1_MASK				0x7
#define OMAP_OCP2SCP3_TIMING_SYNC1_SHIFT			4
#define OMAP_OCP2SCP3_TIMING_SYNC1				0
#define OMAP_OCP2SCP3_TIMING_SYNC2_MASK				0xF
#define OMAP_OCP2SCP3_TIMING_SYNC2				0xF

#define SATAPHYRX_IO_A2D_OVRIDES_REG1				0x44
#define SATAPHYRX_IO_A2D_OVRIDES_REG1_MEMCDR_LOS_SRC_MASK	3
#define SATAPHYRX_IO_A2D_OVRIDES_REG1_MEMCDR_LOS_SRC_SHIFT	9

#define OMAP_CTRL_MODULE_CORE					0x4a002000
#define OMAP_CTRL_MODULE_CORE_SIZE				2048

#define OMAP_CTRL_SATA_PHY_POWER				0x374
#define SATA_PWRCTL_CLK_FREQ_SHIFT				22
#define SATA_PWRCTL_CLK_CMD_SHIFT				14

#define OMAP_CTRL_SATA_EXT_MODE					0x3ac

/* Enable the 19.2 Mhz frequency */
#define SATA_PWRCTL_CLK_FREQ					19
/* Enable the 20 Mhz frequency (specific only to DRA7XX) */
#define SATA_PWRCTL_CLK_FREQ_DRA7XX				20

/* Enable Tx and Rx phys */
#define SATA_PWRCTL_CLK_CMD					3

struct omap_sata_platdata {
	void __iomem		*ocp2scp3, *phyrx, *pll;
	struct omap_ocp2scp_dev *dev_attr;
	struct clk	*ref_clk;
};

static struct omap_sata_platdata	omap_sata_data;
static struct ahci_platform_data	sata_pdata;
static u64				sata_dmamask = DMA_BIT_MASK(32);


static void __iomem			*sataphy_pwr;

static struct omap_device_pm_latency omap_sata_latency[] = {
	  {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	  },
};

static inline void omap_sata_writel(void __iomem *base, u32 reg, u32 val)
{
	__raw_writel(val, base + reg);
}

static inline u32 omap_sata_readl(void __iomem *base, u32 reg)
{
	return __raw_readl(base + reg);
}

static void sataphy_pwr_init(void)
{
	sataphy_pwr = ioremap(OMAP_CTRL_MODULE_CORE,
			      OMAP_CTRL_MODULE_CORE_SIZE);
	if (!sataphy_pwr)
		pr_err("%s: ioremap of 0x%X failed\n", __func__,
		       OMAP_CTRL_MODULE_CORE);
}

static void  sataphy_pwr_on(void)
{
	unsigned long sata_pwrctl_clk_freq_value = 0;

	if (!sataphy_pwr) {
		pr_err("%s: 0x%X not accessible\n", __func__,
		       OMAP_CTRL_MODULE_CORE);
		return;
	}

	/* For DRA7xx, system clock is changed from 19.2M to 20M
	   Setting clock value depending on the m/c type */
	if (soc_is_dra7xx())
		sata_pwrctl_clk_freq_value = SATA_PWRCTL_CLK_FREQ_DRA7XX;
	else
		sata_pwrctl_clk_freq_value = SATA_PWRCTL_CLK_FREQ;
	omap_sata_writel(sataphy_pwr, OMAP_CTRL_SATA_PHY_POWER,
		((sata_pwrctl_clk_freq_value << SATA_PWRCTL_CLK_FREQ_SHIFT)|
		(SATA_PWRCTL_CLK_CMD << SATA_PWRCTL_CLK_CMD_SHIFT)));
	omap_sata_writel(sataphy_pwr, OMAP_CTRL_SATA_EXT_MODE, 1);
}

static void sataphy_pwr_off(void)
{
	u32 reg;

	if (!sataphy_pwr) {
		pr_err("%s: 0x%X not accessible\n", __func__,
		       OMAP_CTRL_MODULE_CORE);
		return;
	}

	reg = omap_sata_readl(sataphy_pwr, OMAP_CTRL_SATA_PHY_POWER);
	reg &= ~(SATA_PWRCTL_CLK_CMD << SATA_PWRCTL_CLK_CMD_SHIFT);
	omap_sata_writel(sataphy_pwr, OMAP_CTRL_SATA_PHY_POWER, reg);
}

static void sataphy_pwr_deinit(void)
{
	iounmap(sataphy_pwr);
}


static void omap_sataphyrx_init(struct device *dev, void __iomem *base)
{
	u32 reg;

	reg = omap_sata_readl(base, SATAPHYRX_IO_A2D_OVRIDES_REG1);
	reg &= ~(SATAPHYRX_IO_A2D_OVRIDES_REG1_MEMCDR_LOS_SRC_MASK <<
		 SATAPHYRX_IO_A2D_OVRIDES_REG1_MEMCDR_LOS_SRC_SHIFT);
	omap_sata_writel(base, SATAPHYRX_IO_A2D_OVRIDES_REG1, reg);
}

static void omap_ocp2scp_init(struct device *dev, void __iomem *base)
{
	unsigned long	timeout;
	u32		reg;

	/* config the bridge timing */
	reg  = omap_sata_readl(base, OMAP_OCP2SCP3_TIMING);
	reg &= (~(OMAP_OCP2SCP3_TIMING_DIV_MASK <<
			OMAP_OCP2SCP3_TIMING_DIV_SHIFT) |
		~(OMAP_OCP2SCP3_TIMING_SYNC1_MASK <<
			OMAP_OCP2SCP3_TIMING_SYNC1_SHIFT) |
		~(OMAP_OCP2SCP3_TIMING_SYNC2_MASK));

	reg |= ((OMAP_OCP2SCP3_TIMING_DIV <<
			OMAP_OCP2SCP3_TIMING_DIV_SHIFT) |
		(OMAP_OCP2SCP3_TIMING_SYNC1 <<
			OMAP_OCP2SCP3_TIMING_SYNC1_SHIFT) |
		(OMAP_OCP2SCP3_TIMING_SYNC2));
	omap_sata_writel(base, OMAP_OCP2SCP3_TIMING, reg);

	/* do the soft reset of ocp2scp3 */
	reg  = omap_sata_readl(base, OMAP_OCP2SCP3_SYSCONFIG);
	reg &= ~(OMAP_OCP2SCP3_SYSCONFIG_RESET_MASK <<
		OMAP_OCP2SCP3_SYSCONFIG_RESET_SHIFT);
	reg |= (OMAP_OCP2SCP3_SYSCONFIG_RESET <<
		OMAP_OCP2SCP3_SYSCONFIG_RESET_SHIFT);
	omap_sata_writel(base, OMAP_OCP2SCP3_SYSCONFIG, reg);

	/* wait for the reset to complete */
	timeout = jiffies + msecs_to_jiffies(10000);
	do {
		cpu_relax();
		reg = omap_sata_readl(base, OMAP_OCP2SCP3_SYSSTATUS) &
			OMAP_OCP2SCP3_SYSSTATUS_RESETDONE;
		if (time_after(jiffies, timeout) && !reg) {
			dev_err(dev, "OCP2SCP RESET[0x%08x] timed out\n", reg);
			break;
		}
	} while (!reg);
}

static int sata_dpll_wait_lock(struct device *dev, void __iomem *base)
{
	unsigned long timeout;

	/* Poll for the PLL lock */
	timeout = jiffies + msecs_to_jiffies(10000);
	while (!(omap_sata_readl(base, OMAP_SATA_PLL_STATUS) &
		(OMAP_SATA_PLL_STATUS_LOCK <<
		OMAP_SATA_PLL_STATUS_LOCK_SHIFT))) {
		cpu_relax();

		if (time_after(jiffies, timeout)) {
			dev_err(dev, "sata phy pll lock timed out\n");
			return -1;
		}
	}
	return 0;
}

static int sata_dpll_config(struct device *dev, void __iomem *base)
{
	u32		reg;
	unsigned long	timeout;
	unsigned long	omap_sata_cfg1_m_value = 0;

	/* Make sure ADPLLLJM is out of reset before configuring it */
	timeout = jiffies + msecs_to_jiffies(10000);
	while (!(omap_sata_readl(base, OMAP_SATA_PLL_STATUS) &
		OMAP_SATA_PLL_STATUS_RESET_DONE)) {
		cpu_relax();

		if (time_after(jiffies, timeout)) {
			dev_err(dev, "SATA PLL RESET - timed out\n");
			return -1;
		}
	}
	reg  = omap_sata_readl(base, OMAP_SATA_PLL_CFG1);
	reg &= ~((OMAP_SATA_PLL_CFG1_M_MASK << OMAP_SATA_PLL_CFG1_M_SHIFT) |
		 (OMAP_SATA_PLL_CFG1_N_MASK << OMAP_SATA_PLL_CFG1_N_SHIFT));

	/* For DRA7xx, system clock is changed from 19.2M to 20M
	   Due to this change, M value for SATA PLL is different from others
	   Setting SATA PLL M valus depending on the m/c type */
	if (soc_is_dra7xx())
		omap_sata_cfg1_m_value = OMAP_SATA_PLL_CFG1_M_DRA7XX;
	else
		omap_sata_cfg1_m_value = OMAP_SATA_PLL_CFG1_M;

	reg |= ((omap_sata_cfg1_m_value << OMAP_SATA_PLL_CFG1_M_SHIFT) |
		(OMAP_SATA_PLL_CFG1_N << OMAP_SATA_PLL_CFG1_N_SHIFT));
	omap_sata_writel(base, OMAP_SATA_PLL_CFG1, reg);

	reg  = omap_sata_readl(base, OMAP_SATA_PLL_CFG2);
	reg &= (~(OMAP_SATA_PLL_CFG2_SELFREQDCO_MASK <<
		  OMAP_SATA_PLL_CFG2_SELFREQDCO_SHIFT) |
		~(OMAP_SATA_PLL_CFG2_REFSEL_MASK <<
		  OMAP_SATA_PLL_CFG2_REFSEL_SHIFT) |
		~(OMAP_SATA_PLL_CFG2_LOCKSEL_MASK <<
		  OMAP_SATA_PLL_CFG2_LOCKSEL_SHIFT));

	reg |= ((OMAP_SATA_PLL_CFG2_SELFREQDCO <<
		 OMAP_SATA_PLL_CFG2_SELFREQDCO_SHIFT) |
		(OMAP_SATA_PLL_CFG2_REFSEL <<
		 OMAP_SATA_PLL_CFG2_REFSEL_SHIFT) |
		(OMAP_SATA_PLL_CFG2_LOCKSEL <<
		 OMAP_SATA_PLL_CFG2_LOCKSEL_SHIFT));
	omap_sata_writel(base, OMAP_SATA_PLL_CFG2, reg);

	reg  = omap_sata_readl(base, OMAP_SATA_PLL_CFG3);
	reg &= ~(OMAP_SATA_PLL_CFG3_SD_MASK << OMAP_SATA_PLL_CFG3_SD_SHIF);
	reg |= (OMAP_SATA_PLL_CFG3_SD << OMAP_SATA_PLL_CFG3_SD_SHIF);
	omap_sata_writel(base, OMAP_SATA_PLL_CFG3, reg);

	reg  = omap_sata_readl(base, OMAP_SATA_PLL_CFG4);
	reg &= (~(OMAP_SATA_PLL_CFG4_REGM_F_MASK) |
		~(OMAP_SATA_PLL_CFG4_REGM2_MASK <<
		  OMAP_SATA_PLL_CFG4_REGM2_SHIFT));
	reg |= ((OMAP_SATA_PLL_CFG4_REGM_F) |
		(OMAP_SATA_PLL_CFG4_REGM2   <<
		 OMAP_SATA_PLL_CFG4_REGM2_SHIFT));
	omap_sata_writel(base, OMAP_SATA_PLL_CFG4, reg);
	omap_sata_writel(base, OMAP_SATA_PLL_GO, 1);

	return sata_dpll_wait_lock(dev, base);
}


static struct resource *ocp2scp_get_resource_bynme(struct omap_ocp2scp_dev *dev,
						   unsigned int      type,
						   const char        *name)
{
	struct resource *res = dev->res;

	while (dev && res && res->start != res->end) {
		if (type == resource_type(res) && !strcmp(res->name, name))
			return res;
		res++;
	}
	return NULL;
}

static int sata_phy_init(struct device *dev)
{
	struct resource			*res;
	struct platform_device		*pdev;
	struct ahci_platform_data	*pdata;
	struct omap_sata_platdata	*spdata;
	struct omap_ocp2scp_dev		*ocp2scpdev;
	int				ret;

	pdev =	container_of(dev, struct platform_device, dev);
	pdata = dev->platform_data;

	if (!pdata || !pdata->priv) {
		dev_err(dev, "No platform data\n");
		return -ENODEV;
	}
	spdata = pdata->priv;
	ocp2scpdev = spdata->dev_attr;
	spdata->ref_clk = clk_get(dev, "sata_ref_clk");
	if (IS_ERR(spdata->ref_clk)) {
		ret = PTR_ERR(spdata->ref_clk);
		dev_err(dev, "sata_ref_clk failed:%d\n", ret);
		return ret;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ocp2scp3");
	if (!res) {
		dev_err(dev, "ocp2scp3 get resource failed\n");
		return -ENXIO;
	}

	spdata->ocp2scp3 = ioremap(res->start, resource_size(res));
	if (!spdata->ocp2scp3) {
		dev_err(dev, "can't map ocp2scp3 0x%X\n", res->start);
		return -ENOMEM;
	}

	res = ocp2scp_get_resource_bynme(ocp2scpdev, IORESOURCE_MEM,
					  "sata_pll");
	if (!res) {
		dev_err(dev, "sata_pll get resource failed\n");
		ret =  -ENXIO;
		goto pll_err_end;
	}

	spdata->pll = ioremap(res->start, resource_size(res));
	if (!spdata->pll) {
		dev_err(dev, "can't map sata_pll 0x%X\n", res->start);
		ret =  -ENOMEM;
		goto pll_err_end;
	}

	res = ocp2scp_get_resource_bynme(ocp2scpdev, IORESOURCE_MEM,
					  "sata_phy_rx");
	if (!res) {
		dev_err(dev, "sata_phy_rx get resource failed\n");
		ret =  -ENXIO;
		goto rx_err_end;
	}

	spdata->phyrx = ioremap(res->start, resource_size(res));
	if (!spdata->phyrx) {
		dev_err(dev, "can't map sata_phy_rx 0x%X\n", res->start);
		ret =  -ENOMEM;
		goto rx_err_end;
	}

	clk_enable(spdata->ref_clk);

	omap_ocp2scp_init(dev, spdata->ocp2scp3);
	sata_dpll_config(dev, spdata->pll);
	sata_dpll_wait_lock(dev, spdata->pll);
	omap_sataphyrx_init(dev, spdata->phyrx);
	sataphy_pwr_init();
	sataphy_pwr_on();

	return 0;

rx_err_end:
	iounmap(spdata->pll);

pll_err_end:
	iounmap(spdata->ocp2scp3);

	return ret;
}

static void sata_phy_exit(struct device *dev)
{
	struct platform_device		*pdev;
	struct ahci_platform_data	*pdata;
	struct omap_sata_platdata	*spdata;

	pdev =	container_of(dev, struct platform_device, dev);
	pdata = dev->platform_data;

	if (!pdata || !pdata->priv) {
		dev_err(dev, "No platform data\n");
		return;
	}
	spdata = pdata->priv;

	clk_disable(spdata->ref_clk);
	clk_put(spdata->ref_clk);

	iounmap(spdata->phyrx);
	iounmap(spdata->pll);
	iounmap(spdata->ocp2scp3);

	sataphy_pwr_off();
	sataphy_pwr_deinit();
}

static int omap_ahci_plat_init(struct device *dev, void __iomem *base)
{
	pm_runtime_enable(dev);
	pm_runtime_get_sync(dev);
	sata_phy_init(dev);
	return 0;
}

static void omap_ahci_plat_exit(struct device *dev)
{
	pm_runtime_put_sync(dev);
	pm_runtime_disable(dev);
	sata_phy_exit(dev);
}

#ifdef CONFIG_PM

static int omap_sata_suspend(struct device *dev)
{
	struct ahci_platform_data	*pdata;
	struct omap_sata_platdata	*spdata;
	unsigned long			timeout;
	u32				reg;

	pdata = dev->platform_data;
	if (!pdata || !pdata->priv) {
		dev_err(dev, "No platform data\n");
		return -ENXIO;
	}
	spdata = pdata->priv;
	if (!spdata->ref_clk || !spdata->pll) {
		dev_err(dev, "No refclk and pll\n");
		return -EPERM;
	}

	reg  = omap_sata_readl(spdata->pll, OMAP_SATA_PLL_CFG2);
	reg &= ~OMAP_SATA_PLL_CFG2_IDLE_MASK;
	reg |= OMAP_SATA_PLL_CFG2_IDLE;
	omap_sata_writel(spdata->pll, OMAP_SATA_PLL_CFG2, reg);

	/* wait for internal LDO and internal oscilator power down */
	timeout = jiffies + msecs_to_jiffies(10000);
	do {
		cpu_relax();
		reg = omap_sata_readl(spdata->pll, OMAP_SATA_PLL_STATUS);
		reg = (reg >> OMAP_SATA_PLL_STATUS_LDOPWDN_SHIFT) &
		      (reg >> OMAP_SATA_PLL_STATUS_TICOPWDN_SHIFT);
		if (time_after(jiffies, timeout) & !reg) {
			dev_err(dev, "ADPLLLJM IDLE[0x%08x] - timed out\n",
				reg);
			return -EIO;
		}
	} while (!reg);

#ifdef OMAP_SATA_PHY_PWR
	sataphy_pwr_off();
#endif

	clk_disable(spdata->ref_clk);
	return 0;
}


static int omap_sata_resume(struct device *dev)
{
	struct ahci_platform_data	*pdata;
	struct omap_sata_platdata	*spdata;
	u32				reg;

	pdata = dev->platform_data;
	if (!pdata || !pdata->priv) {
		dev_err(dev, "No platform data\n");
		return -ENXIO;
	}
	spdata = pdata->priv;
	if (!spdata->ref_clk || !spdata->pll ||
	    !spdata->ocp2scp3 || !spdata->phyrx) {
		dev_err(dev, "No refclk , pll and ocp2scp3\n");
		return -EPERM;
	}

	clk_enable(spdata->ref_clk);
	omap_ocp2scp_init(dev, spdata->ocp2scp3);
	reg  = omap_sata_readl(spdata->pll, OMAP_SATA_PLL_CFG2);
	if (reg & OMAP_SATA_PLL_CFG2_IDLE_MASK) {
		dev_err(dev, "sata pll in idle\n");
		reg &= ~OMAP_SATA_PLL_CFG2_IDLE_MASK;
		omap_sata_writel(spdata->pll, OMAP_SATA_PLL_CFG2, reg);
	} else {
		dev_err(dev, "sata pll not in idle, so reconfigure pll\n");
		omap_sataphyrx_init(dev, spdata->phyrx);
		sata_dpll_config(dev, spdata->pll);
	}
	sata_dpll_wait_lock(dev, spdata->pll);

#ifdef OMAP_SATA_PHY_PWR
	sataphy_pwr_on();
#endif

	return 0;
}

#else
static int omap_sata_suspend(struct device *dev)
{
	return -EPERM;
}

static int omap_sata_resume(struct device *dev)
{
	return -EPERM;
}

#endif

void __init omap_sata_init(void)
{
	struct omap_hwmod	*hwmod[2];
	struct platform_device	*pdev;
	struct device		*dev;
	int			oh_cnt = 1;

	/* For now sata init works only for omap5 */
	if (!soc_is_omap54xx() && !soc_is_dra7xx())
		return;

	sata_pdata.init		= omap_ahci_plat_init;
	sata_pdata.exit		= omap_ahci_plat_exit;
	sata_pdata.suspend	= omap_sata_suspend;
	sata_pdata.resume	= omap_sata_resume;

	hwmod[0] = omap_hwmod_lookup(OMAP_SATA_HWMODNAME);
	if (!hwmod[0]) {
		pr_err("Could not look up %s\n", OMAP_SATA_HWMODNAME);
		return;
	}

	hwmod[1] = omap_hwmod_lookup(OMAP_OCP2SCP3_HWMODNAME);
	if (hwmod[1]) {
		oh_cnt++;
	} else {
		pr_err("Could not look up %s\n", OMAP_OCP2SCP3_HWMODNAME);
		return;
	}

	omap_sata_data.dev_attr = (struct omap_ocp2scp_dev *)hwmod[1]->dev_attr;
	sata_pdata.priv = &omap_sata_data;

	pdev = omap_device_build_ss(AHCI_PLAT_DEVNAME, PLATFORM_DEVID_AUTO, hwmod, oh_cnt,
				(void *)&sata_pdata, sizeof(sata_pdata),
				omap_sata_latency,
				ARRAY_SIZE(omap_sata_latency), false);
	if (IS_ERR(pdev)) {
		pr_err("Could not build hwmod device %s\n",
		       OMAP_SATA_HWMODNAME);
		return;
	}
	dev = &pdev->dev;
	get_device(dev);
	dev->dma_mask = &sata_dmamask;
	dev->coherent_dma_mask = DMA_BIT_MASK(32);
	put_device(dev);
}
