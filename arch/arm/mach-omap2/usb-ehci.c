/*
 * linux/arch/arm/mach-omap2/usb-ehci.c
 *
 * This file will contain the board specific details for the
 * Synopsys EHCI host controller on OMAP3430
 *
 * Copyright (C) 2007 Texas Instruments
 * Author: Vikram Pandita <vikram.pandita@ti.com>
 *
 * Generalization by:
 * Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/semaphore.h>

#include <asm/io.h>
#include <plat/mux.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <plat/usb.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <linux/pm_runtime.h>

#include <plat/omap-pm.h>
#include "mux.h"
#include "pm.h"

/*
 * OMAP USBHOST Register addresses: VIRTUAL ADDRESSES
 */

/* TLL Register Set */
#define	OMAP_USBTLL_REVISION				(0x00)
#define	OMAP_USBTLL_SYSCONFIG				(0x10)
#define	OMAP_USBTLL_SYSCONFIG_CACTIVITY			(1 << 8)
#define	OMAP_USBTLL_SYSCONFIG_NIDLEMODE			(1 << 3)
#define	OMAP_USBTLL_SYSCONFIG_SIDLEMODE			(2 << 3)
#define	OMAP_USBTLL_SYSCONFIG_ENAWAKEUP			(1 << 2)
#define	OMAP_USBTLL_SYSCONFIG_SOFTRESET			(1 << 1)
#define	OMAP_USBTLL_SYSCONFIG_AUTOIDLE			(1 << 0)

#define	OMAP_USBTLL_SYSSTATUS				(0x14)
#define	OMAP_USBTLL_SYSSTATUS_RESETDONE			(1 << 0)

#define	OMAP_USBTLL_IRQSTATUS				(0x18)
#define	OMAP_USBTLL_IRQENABLE				(0x1C)

#define	OMAP_TLL_SHARED_CONF				(0x30)
#define	OMAP_TLL_SHARED_CONF_USB_90D_DDR_EN		(1 << 6)
#define	OMAP_TLL_SHARED_CONF_USB_180D_SDR_EN		(1 << 5)
#define	OMAP_TLL_SHARED_CONF_USB_DIVRATION		(1 << 2)
#define	OMAP_TLL_SHARED_CONF_FCLK_REQ			(1 << 1)
#define	OMAP_TLL_SHARED_CONF_FCLK_IS_ON			(1 << 0)

#define	OMAP_TLL_CHANNEL_CONF(num)			(0x040 + 0x004 * num)
#define OMAP_TLL_CHANNEL_CONF_FSLSMODE_SHIFT		24
#define	OMAP_TLL_CHANNEL_CONF_ULPINOBITSTUFF		(1 << 11)
#define	OMAP_TLL_CHANNEL_CONF_ULPI_ULPIAUTOIDLE		(1 << 10)
#define	OMAP_TLL_CHANNEL_CONF_UTMIAUTOIDLE		(1 << 9)
#define	OMAP_TLL_CHANNEL_CONF_ULPIDDRMODE		(1 << 8)
#define OMAP_TLL_CHANNEL_CONF_CHANMODE_FSLS		(1 << 1)
#define	OMAP_TLL_CHANNEL_CONF_CHANEN			(1 << 0)

#define	OMAP_TLL_ULPI_FUNCTION_CTRL(num)		(0x804 + 0x100 * num)
#define	OMAP_TLL_ULPI_INTERFACE_CTRL(num)		(0x807 + 0x100 * num)
#define	OMAP_TLL_ULPI_OTG_CTRL(num)			(0x80A + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_EN_RISE(num)			(0x80D + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_EN_FALL(num)			(0x810 + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_STATUS(num)			(0x813 + 0x100 * num)
#define	OMAP_TLL_ULPI_INT_LATCH(num)			(0x814 + 0x100 * num)
#define	OMAP_TLL_ULPI_DEBUG(num)			(0x815 + 0x100 * num)
#define	OMAP_TLL_ULPI_SCRATCH_REGISTER(num)		(0x816 + 0x100 * num)

#define OMAP_TLL_CHANNEL_COUNT				3
#define OMAP_TLL_CHANNEL_1_EN_MASK			(1 << 1)
#define OMAP_TLL_CHANNEL_2_EN_MASK			(1 << 2)
#define OMAP_TLL_CHANNEL_3_EN_MASK			(1 << 4)

/* UHH Register Set */
#define	OMAP_UHH_REVISION				(0x00)
#define	OMAP_UHH_SYSCONFIG				(0x10)
#define	OMAP_UHH_SYSCONFIG_MIDLEMODE			(1 << 12)
#define	OMAP_UHH_SYSCONFIG_CACTIVITY			(1 << 8)
#define	OMAP_UHH_SYSCONFIG_SIDLEMODE			(1 << 3)
#define	OMAP_UHH_SYSCONFIG_ENAWAKEUP			(1 << 2)
#define	OMAP_UHH_SYSCONFIG_SOFTRESET			(1 << 1)
#define	OMAP_UHH_SYSCONFIG_AUTOIDLE			(1 << 0)

#define	OMAP_UHH_SYSSTATUS				(0x14)
#define	OMAP_UHH_HOSTCONFIG				(0x40)
#define	OMAP_UHH_HOSTCONFIG_ULPI_BYPASS			(1 << 0)
#define	OMAP_UHH_HOSTCONFIG_ULPI_P1_BYPASS		(1 << 0)
#define	OMAP_UHH_HOSTCONFIG_ULPI_P2_BYPASS		(1 << 11)
#define	OMAP_UHH_HOSTCONFIG_ULPI_P3_BYPASS		(1 << 12)
#define OMAP_UHH_HOSTCONFIG_INCR4_BURST_EN		(1 << 2)
#define OMAP_UHH_HOSTCONFIG_INCR8_BURST_EN		(1 << 3)
#define OMAP_UHH_HOSTCONFIG_INCR16_BURST_EN		(1 << 4)
#define OMAP_UHH_HOSTCONFIG_INCRX_ALIGN_EN		(1 << 5)
#define OMAP_UHH_HOSTCONFIG_P1_CONNECT_STATUS		(1 << 8)
#define OMAP_UHH_HOSTCONFIG_P2_CONNECT_STATUS		(1 << 9)
#define OMAP_UHH_HOSTCONFIG_P3_CONNECT_STATUS		(1 << 10)

#define OMAP4_UHH_HOSTCONFIG_APP_START_CLK		(1 << 31)

/* OMAP4 specific */
#define OMAP_UHH_SYSCONFIG_IDLEMODE_RESET		(~(0xC))
#define OMAP_UHH_SYSCONFIG_FIDLEMODE_SET		(0 << 2)
#define OMAP_UHH_SYSCONFIG_NIDLEMODE_SET		(1 << 2)
#define OMAP_UHH_SYSCONFIG_SIDLEMODE_SET		(2 << 2)
#define OMAP_UHH_SYSCONFIG_SWIDLMODE_SET		(3 << 2)

#define OMAP_UHH_SYSCONFIG_STDYMODE_RESET		(~(3 << 4))
#define OMAP_UHH_SYSCONFIG_FSTDYMODE_SET		(0 << 4)
#define OMAP_UHH_SYSCONFIG_NSTDYMODE_SET		(1 << 4)
#define OMAP_UHH_SYSCONFIG_SSTDYMODE_SET		(2 << 4)
#define OMAP_UHH_SYSCONFIG_SWSTDMODE_SET		(3 << 4)

#define OMAP_UHH_HOST_PORT1_RESET			(~(0x3 << 16))
#define OMAP_UHH_HOST_PORT2_RESET			(~(0x3 << 18))

#define OMAP_UHH_HOST_P1_SET_ULPIPHY			(0 << 16)
#define OMAP_UHH_HOST_P1_SET_ULPITLL			(1 << 16)
#define OMAP_UHH_HOST_P1_SET_HSIC			(3 << 16)

#define OMAP_UHH_HOST_P2_SET_ULPIPHY			(0 << 18)
#define OMAP_UHH_HOST_P2_SET_ULPITLL			(1 << 18)
#define OMAP_UHH_HOST_P2_SET_HSIC			(3 << 18)
#define OMAP4_UHH_SYSCONFIG_SOFTRESET			(1 << 0)

#define OMAP4_TLL_CHANNEL_COUNT				2

#define	OMAP_UHH_DEBUG_CSR				(0x44)


/* OHCI Register Set */
#define OHCI_HCCONTROL			0x04
#define OHCI_HCCONTROL_RESET		(~(3<<6))
#define OHCI_HCCONTROL_SUSPEND		(3 << 6)
#define OHCI_HCCONTROL_RESUME		(1 << 6)
#define OHCI_HCCONTROL_OPERATIONAL	(2 << 6)



#define USBHS_IO_WAKEUPENABLE		(1 << 14)

#if defined(CONFIG_USB_EHCI_HCD) || defined(CONFIG_USB_EHCI_HCD_MODULE) || \
	defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)

static const char uhhtllname[] = "uhhtll-omap";
#define USB_UHH_HS_HWMODNAME				"usb_uhh_hs"
#define USB_TLL_HS_HWMODNAME				"usb_tll_hs"
#define USBHS_OHCI_HWMODNAME				"usbhs_ohci"
#define USBHS_EHCI_HWMODNAME				"usbhs_ehci"

struct uhhtll_hcd_omap {
	struct platform_device	*pdev;

	struct clk		*usbhost_ick;
	struct clk		*usbhost_hs_fck;
	struct clk		*usbhost_fs_fck;
	struct clk		*xclk60mhsp1_ck;
	struct clk		*xclk60mhsp2_ck;
	struct clk		*utmi_p1_fck;
	struct clk		*utmi_p2_fck;
	struct clk		*usbtll_fck;
	struct clk		*usbtll_ick;

	void __iomem		*uhh_base;
	void __iomem		*tll_base;


	struct semaphore	mutex;
	int			count;

	struct usbhs_omap_platform_data platdata;

	u32				ohci_addr_start;
	u32				ohci_addr_size;
	void __iomem			*ohci_base;
	struct omap_hwmod		*uhh_hwmod;
};

static struct uhhtll_hcd_omap uhhtll = {
	.pdev = NULL,
};

static int uhhtll_get_platform_data(struct usbhs_omap_platform_data *pdata);

static int uhhtll_drv_enable(enum driver_type drvtype);

static int uhhtll_drv_disable(enum driver_type drvtype);

static int uhhtll_drv_suspend(enum driver_type drvtype);

static int uhhtll_drv_resume(enum driver_type drvtype);


static struct omap_device_pm_latency omap_uhhtll_latency[] = {
	  {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	  },
};


static struct uhhtll_apis uhhtll_export = {
	.get_platform_data	= uhhtll_get_platform_data,
	.enable			= uhhtll_drv_enable,
	.disable		= uhhtll_drv_disable,
	.suspend		= uhhtll_drv_suspend,
	.resume			= uhhtll_drv_resume,
};

static void setup_ehci_io_mux(const enum usbhs_omap3_port_mode *port_mode);
static void setup_ohci_io_mux(const enum usbhs_omap3_port_mode *port_mode);
static void setup_4430ehci_io_mux(const enum usbhs_omap3_port_mode *port_mode);
static void setup_4430ohci_io_mux(const enum usbhs_omap3_port_mode *port_mode);


/*-------------------------------------------------------------------------*/

static inline void uhhtll_omap_write(void __iomem *base, u32 reg, u32 val)
{
	__raw_writel(val, base + reg);
}

static inline u32 uhhtll_omap_read(void __iomem *base, u32 reg)
{
	return __raw_readl(base + reg);
}

static inline void uhhtll_omap_writeb(void __iomem *base, u8 reg, u8 val)
{
	__raw_writeb(val, base + reg);
}

static inline u8 uhhtll_omap_readb(void __iomem *base, u8 reg)
{
	return __raw_readb(base + reg);
}

/*-------------------------------------------------------------------------*/


#ifdef	CONFIG_PM

/* get the throughput */
static int uhhtll_get_tput(struct device	*dev)
{

	return omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT,
					(200*1000*4));
}


/* release throughput */
static int uhhtll_put_tput(struct device	*dev)
{

	return omap_pm_set_min_bus_tput(dev, OCP_INITIATOR_AGENT, -1);
}


static void usbhs_4430ehci_io_enable(const enum usbhs_omap3_port_mode
					*port_mode)
{
	u16 reg;

	/*
	 * FIXME: This funtion should use mux framework functions;
	 * For now, we are hardcoding this.
	 */

	switch (port_mode[0]) {
	case OMAP_EHCI_PORT_MODE_PHY:

		/* HUSB1_DATA0 */
		reg = omap_readw(0x4A1000CA);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CA);

		break;

	case OMAP_EHCI_PORT_MODE_TLL:
		/* TODO */


		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_EHCI_PORT_MODE_PHY:

		/* HUSB2_DATA0 */
		reg = omap_readw(0x4A100168);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A100168);

		break;

	case OMAP_EHCI_PORT_MODE_TLL:
		/* TODO */

		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	return;
}



static void usbhs_4430ehci_io_disable(const enum usbhs_omap3_port_mode
					*port_mode)
{
	u16 reg;

	/*
	 * FIXME: This funtion should use mux framework functions;
	 * For now, we are hardcoding this.
	 */

	switch (port_mode[0]) {
	case OMAP_EHCI_PORT_MODE_PHY:

		/* HUSB1_DATA0 */
		reg = omap_readw(0x4A1000CA);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CA);

		break;


	case OMAP_EHCI_PORT_MODE_TLL:
		/* TODO */

		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_EHCI_PORT_MODE_PHY:

		/* HUSB2_DATA0 */
		reg = omap_readw(0x4A100168);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A100168);

		break;

	case OMAP_EHCI_PORT_MODE_TLL:
		/* TODO */

		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	return;
}


static void usbhs_ehci_io_wakeup(const enum usbhs_omap3_port_mode
					*port_mode, int enable)
{

	if (cpu_is_omap44xx()) {
		if (enable)
			usbhs_4430ehci_io_enable(port_mode);
		else
			usbhs_4430ehci_io_disable(port_mode);
	}
	omap4_trigger_ioctrl();
	/* TODO for OMAP3 */
}





static void usbhs_4430ohci_io_enable(const enum usbhs_omap3_port_mode
					*port_mode)
{
	u16 reg;

	/* FIXME: This funtion should use Mux frame work functions;
	 * for now, we are hardcodeing it
	 * This function will be later replaced by MUX framework API.
	 */
	switch (port_mode[0]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:

		/* usbb1_mm_rxdp */
		reg =  omap_readw(0x4A1000C4);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000C4);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:


		/* usbb1_mm_rxrcv */
		reg =  omap_readw(0x4A1000CA);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CA);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb1_mm_txen */
		reg =  omap_readw(0x4A1000D0);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000D0);
		break;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb1_mm_txdat */
		reg =  omap_readw(0x4A1000CE);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CE);

		/* usbb1_mm_txse0 */
		reg =  omap_readw(0x4A1000CC);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CC);

		break;

	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:


		reg =  omap_readw(0x4A1000F8);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000F8);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:

		/* usbb2_mm_rxrcv */
		reg =  omap_readw(0x4A1000FA);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000FA);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb2_mm_txen */
		reg =  omap_readw(0x4A1000FC);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000FC);
		break;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb2_mm_txdat */
		reg =  omap_readw(0x4A100112);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A100112);

		/* usbb2_mm_txse0 */
		reg =  omap_readw(0x4A100110);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A100110);
		break;

	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		break;
	}
}


static void usbhs_4430ohci_io_disable(const enum usbhs_omap3_port_mode
					*port_mode)
{
	u16 reg;

	/* FIXME: This funtion should use Mux frame work functions;
	 * for now, we are hardcodeing it
	 * This function will be later replaced by MUX framework API.
	 */
	switch (port_mode[0]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:

		/* usbb1_mm_rxdp */
		reg =  omap_readw(0x4A1000C4);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000C4);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:


		/* usbb1_mm_rxrcv */
		reg =  omap_readw(0x4A1000CA);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CA);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb1_mm_txen */
		reg =  omap_readw(0x4A1000D0);
		reg |= USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000D0);
		break;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb1_mm_txdat */
		reg =  omap_readw(0x4A1000CE);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CE);

		/* usbb1_mm_txse0 */
		reg =  omap_readw(0x4A1000CC);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000CC);

		break;

	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:


		reg =  omap_readw(0x4A1000F8);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000F8);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:

		/* usbb2_mm_rxrcv */
		reg =  omap_readw(0x4A1000FA);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000FA);
		break;

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb2_mm_txen */
		reg =  omap_readw(0x4A1000FC);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A1000FC);
		break;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb2_mm_txdat */
		reg =  omap_readw(0x4A100112);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A100112);

		/* usbb2_mm_txse0 */
		reg =  omap_readw(0x4A100110);
		reg &= ~USBHS_IO_WAKEUPENABLE;
		omap_writew(reg, 0x4A100110);
		break;

	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		break;
	}
}


static void usbhs_ohci_io_wakeup(const enum usbhs_omap3_port_mode
					*port_mode, int enable)
{

	if (cpu_is_omap44xx()) {
		if (enable)
			usbhs_4430ohci_io_enable(port_mode);
		else
			usbhs_4430ohci_io_disable(port_mode);
	}
	omap4_trigger_ioctrl();
	/* TODO for OMAP3 */
}


#else
static int uhhtll_get_tput(struct device	*dev)
{
	return 0;
}

static int uhhtll_put_tput(struct device	*dev)
{
	return 0;
}


static void usbhs_ehci_io_wakeup(const enum usbhs_omap3_port_mode
					*port_mode, int enable)
{
	return;
}


static void usbhs_ohci_io_wakeup(const enum usbhs_omap3_port_mode
					*port_mode, int enable)
{
	return;
}

#endif



/**
 * uhh_hcd_omap_probe - initialize TI-based HCDs
 *
 * Allocates basic resources for this USB host controller.
 */
static int uhhtll_hcd_omap_probe(struct platform_device *pdev)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;
	struct resource *res;


	if (!pdev) {
		pr_err("uhhtll_hcd_omap_probe : no pdev\n");
		return -EINVAL;
	}

	memcpy(&omap->platdata, pdev->dev.platform_data,
				sizeof(omap->platdata));



	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	omap->uhh_base = ioremap(res->start, resource_size(res));
	if (!omap->uhh_base) {
		dev_err(&pdev->dev, "UHH ioremap failed\n");
		kfree(omap);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	omap->tll_base = ioremap(res->start, resource_size(res));
	if (!omap->tll_base) {
		dev_err(&pdev->dev, "TLL ioremap failed\n");
		iounmap(omap->uhh_base);
		kfree(omap);
		return -ENOMEM;
	}

	omap->ohci_base = ioremap(omap->ohci_addr_start,
					omap->ohci_addr_size);
	if (!omap->ohci_base) {
		dev_err(&pdev->dev, "UHH OHCI ioremap failed\n");
		iounmap(omap->tll_base);
		iounmap(omap->uhh_base);
		kfree(omap);
		return -ENOMEM;
	}



	pm_runtime_enable(&pdev->dev);

	omap->pdev = pdev;

	/* TODO: USBTLL IRQ processing */
	return 0;
}


/**
 * uhhtll_hcd_omap_remove - shutdown processing for UHH & TLL HCDs
 * @pdev: USB Host Controller being removed
 *
 * Reverses the effect of uhhtll_hcd_omap_probe().
 */

static int uhhtll_hcd_omap_remove(struct platform_device *pdev)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;

	if (omap->count != 0) {
		dev_err(&pdev->dev,
			"Either EHCI or OHCI is still using UHH TLL\n");
		return -EBUSY;
	}

	pm_runtime_disable(&omap->pdev->dev);
	iounmap(omap->ohci_base);
	iounmap(omap->tll_base);
	iounmap(omap->uhh_base);
	omap->pdev = NULL;
	return 0;
}


#ifdef	CONFIG_PM


static int uhhtll_omap_suspend(struct device *dev)
{

	dev_err(dev, "uhhtll_omap_suspend\n");
	return 0;
}

static int uhhtll_omap_resume(struct device *dev)
{
	dev_err(dev, "uhhtll_omap_resume\n");
	return 0;
}


static const struct dev_pm_ops usbhs_omap_dev_pm_ops = {
	.runtime_suspend	= uhhtll_omap_suspend,
	.runtime_resume		= uhhtll_omap_resume,
};




#define USBHS_OMAP_DEV_PM_OPS (&usbhs_omap_dev_pm_ops)
#else
#define	USBHS_OMAP_DEV_PM_OPS	NULL
#endif



static struct platform_driver uhhtll_hcd_omap_driver = {
	.probe			= uhhtll_hcd_omap_probe,
	.remove			= uhhtll_hcd_omap_remove,
	.driver = {
		.name		= "uhhtll-omap",
		.pm		= USBHS_OMAP_DEV_PM_OPS,
	}
};



static int is_ohci_port(enum usbhs_omap3_port_mode pmode)
{
	switch (pmode) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		return 1;   /* YES */

	default:
		return 0; /* NO */
	}
}



/*-------------------------------------------------------------------------*/

/*
 * convert the port-mode enum to a value we can use in the FSLSMODE
 * field of USBTLL_CHANNEL_CONF
 */
static unsigned ohci_omap3_fslsmode(enum usbhs_omap3_port_mode mode)
{
	switch (mode) {
	case OMAP_USBHS_PORT_MODE_UNUSED:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
		return 0x0;

	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
		return 0x1;

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
		return 0x2;

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
		return 0x3;

	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
		return 0x4;

	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		return 0x5;

	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		return 0x6;

	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		return 0x7;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
		return 0xA;

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		return 0xB;
	default:
		pr_warning("Invalid port mode, using default\n");
		return 0x0;
	}
}


static void usbhs_omap_tll_init(struct uhhtll_hcd_omap *omap,
				 u8 tll_channel_count)
{
	struct usbhs_omap_platform_data *pdata = &omap->platdata;
	unsigned reg;
	int i;

	/* Program Common TLL register */
	reg = uhhtll_omap_read(omap->tll_base, OMAP_TLL_SHARED_CONF);
	reg |= (OMAP_TLL_SHARED_CONF_FCLK_IS_ON
		| OMAP_TLL_SHARED_CONF_USB_DIVRATION);
	reg &= ~OMAP_TLL_SHARED_CONF_USB_90D_DDR_EN;
	reg &= ~OMAP_TLL_SHARED_CONF_USB_180D_SDR_EN;

	uhhtll_omap_write(omap->tll_base, OMAP_TLL_SHARED_CONF, reg);

	/* Enable channels now */
	for (i = 0; i < tll_channel_count; i++) {
		reg = uhhtll_omap_read(omap->tll_base,
				OMAP_TLL_CHANNEL_CONF(i));

		if (pdata->port_mode[i] == OMAP_USBHS_PORT_MODE_UNUSED)
			continue;

		if (is_ohci_port(pdata->port_mode[i])) {
			reg |= ohci_omap3_fslsmode(pdata->port_mode[i])
				<< OMAP_TLL_CHANNEL_CONF_FSLSMODE_SHIFT;
			reg |= OMAP_TLL_CHANNEL_CONF_CHANMODE_FSLS;
		} else if (pdata->port_mode[i] == OMAP_EHCI_PORT_MODE_TLL) {

			/* Disable AutoIdle, BitStuffing and use SDR Mode */
			reg &= ~(OMAP_TLL_CHANNEL_CONF_UTMIAUTOIDLE
				| OMAP_TLL_CHANNEL_CONF_ULPINOBITSTUFF
				| OMAP_TLL_CHANNEL_CONF_ULPIDDRMODE);

			reg |= (1 << (i+1));
		}

		reg |= OMAP_TLL_CHANNEL_CONF_CHANEN;
		uhhtll_omap_write(omap->tll_base,
				OMAP_TLL_CHANNEL_CONF(i), reg);

		uhhtll_omap_writeb(omap->tll_base,
				OMAP_TLL_ULPI_SCRATCH_REGISTER(i), 0xbe);
		dev_dbg(&omap->pdev->dev, "ULPI_SCRATCH_REG[ch=%d]= 0x%02x\n",
				i+1, uhhtll_omap_readb(omap->tll_base,
				OMAP_TLL_ULPI_SCRATCH_REGISTER(i)));
	}
}



static int usbhs_enable(struct uhhtll_hcd_omap *omap, int do_init)
{
	struct usbhs_omap_platform_data *pdata = &omap->platdata;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	u32 reg = 0;
	char supply[7];
	int ret = 0;
	int i;

	dev_dbg(&omap->pdev->dev, "Enable USBHS\n");

	if (omap->count > 0)
		goto end_count;

	pm_runtime_get_sync(&omap->pdev->dev);

	if (!do_init)
		goto  end_clk;

	/* get ehci regulator and enable */
	for (i = 0 ; i < OMAP3_HS_USB_PORTS ; i++) {
		if (pdata->port_mode[i] != OMAP_EHCI_PORT_MODE_PHY) {
			pdata->regulator[i] = NULL;
			continue;
		}
		snprintf(supply, sizeof(supply), "hsusb%d", i);
		pdata->regulator[i] = regulator_get(&omap->pdev->dev, supply);
		if (IS_ERR(pdata->regulator[i])) {
			pdata->regulator[i] = NULL;
			dev_dbg(&omap->pdev->dev,
			"failed to get ehci port%d regulator\n", i);
		} else {
			regulator_enable(pdata->regulator[i]);
		}
	}

	if (pdata->phy_reset) {
		/* Refer: ISSUE1 */
		if (!gpio_request(pdata->reset_gpio_port[0],
					"USB1 PHY reset"))
			gpio_direction_output(pdata->reset_gpio_port[0], 0);

		if (!gpio_request(pdata->reset_gpio_port[1],
					"USB2 PHY reset"))
			gpio_direction_output(pdata->reset_gpio_port[1], 0);

		/* Hold the PHY in RESET for enough time till DIR is high */
		udelay(10);
	}


	/* set the hwmod to smart idle */
	omap->uhh_hwmod->flags &= ~(HWMOD_SWSUP_SIDLE |
					HWMOD_SWSUP_MSTANDBY);

	if (cpu_is_omap44xx()) {

		/* Put UHH in smart idle and smart standby mode */
		reg = uhhtll_omap_read(omap->uhh_base, OMAP_UHH_SYSCONFIG);
		reg &= OMAP_UHH_SYSCONFIG_IDLEMODE_RESET;
		reg |= OMAP_UHH_SYSCONFIG_SIDLEMODE_SET;

		reg &= OMAP_UHH_SYSCONFIG_STDYMODE_RESET;
		reg |= OMAP_UHH_SYSCONFIG_SSTDYMODE_SET;

		uhhtll_omap_write(omap->uhh_base, OMAP_UHH_SYSCONFIG, reg);

		reg = uhhtll_omap_read(omap->uhh_base,
				OMAP_UHH_HOSTCONFIG);

		/* setup ULPI bypass and burst configurations */
		reg |= (OMAP_UHH_HOSTCONFIG_INCR4_BURST_EN |
			OMAP_UHH_HOSTCONFIG_INCR8_BURST_EN |
			OMAP_UHH_HOSTCONFIG_INCR16_BURST_EN);
		reg &= ~OMAP_UHH_HOSTCONFIG_INCRX_ALIGN_EN;

		/*
		 * FIXME: This bit is currently undocumented.
		 * Update this commennt after the documentation
		 * is properly updated
		 */
		reg |= OMAP4_UHH_HOSTCONFIG_APP_START_CLK;

		uhhtll_omap_write(omap->uhh_base,
				OMAP_UHH_HOSTCONFIG, reg);


		/* configure OHCI to suspend; this will
		 * help if OHCI is not loaded
		 */
		reg = uhhtll_omap_read(omap->ohci_base, OHCI_HCCONTROL);

		reg &= OHCI_HCCONTROL_RESET;
		reg |= OHCI_HCCONTROL_SUSPEND;

		uhhtll_omap_write(omap->ohci_base,
				OHCI_HCCONTROL, reg);

		reg = uhhtll_omap_read(omap->uhh_base,
				OMAP_UHH_HOSTCONFIG);
		reg &= OMAP_UHH_HOST_PORT1_RESET;


		omap->utmi_p1_fck = clk_get(&omap->pdev->dev,
				"utmi_p1_gfclk");
		if (IS_ERR(omap->utmi_p1_fck)) {
			ret = PTR_ERR(omap->utmi_p1_fck);
			dev_err(&omap->pdev->dev,
				"utmi_p1_gfclk failed error:%d\n",
				ret);
				goto err_end;
		}

		if (pdata->port_mode[0] == OMAP_EHCI_PORT_MODE_PHY) {
			omap->xclk60mhsp1_ck = clk_get(&omap->pdev->dev,
					"xclk60mhsp1_ck");
			if (IS_ERR(omap->xclk60mhsp1_ck)) {
				ret = PTR_ERR(omap->xclk60mhsp1_ck);
				dev_err(&omap->pdev->dev,
					"xclk60mhsp1_ck failed"
					"error:%d\n", ret);
				goto err_xclk60mhsp1_ck;
			}

			/* Set the clock parent as External clock  */
			ret = clk_set_parent(omap->utmi_p1_fck,
						omap->xclk60mhsp1_ck);
			if (ret != 0) {
				dev_err(&omap->pdev->dev,
					"xclk60mhsp1_ck set parent"
					"failed error:%d\n", ret);
				goto err_xclk60mhsp1_ck1;
			}

			reg |= OMAP_UHH_HOST_P1_SET_ULPIPHY;

		} else {
				omap->xclk60mhsp1_ck = NULL;
				reg |= OMAP_UHH_HOST_P1_SET_ULPITLL;
			}


		reg &= OMAP_UHH_HOST_PORT2_RESET;

		omap->utmi_p2_fck = clk_get(&omap->pdev->dev,
				"utmi_p2_gfclk");
		if (IS_ERR(omap->utmi_p2_fck)) {
			ret = PTR_ERR(omap->utmi_p2_fck);
			dev_err(&omap->pdev->dev,
				"utmi_p2_gfclk failed error:%d\n", ret);
			goto err_xclk60mhsp1_ck1;
		}

		if (pdata->port_mode[1] == OMAP_EHCI_PORT_MODE_PHY) {
			omap->xclk60mhsp2_ck = clk_get(&omap->pdev->dev,
						"xclk60mhsp2_ck");

			if (IS_ERR(omap->xclk60mhsp2_ck)) {
				ret = PTR_ERR(omap->xclk60mhsp2_ck);
				dev_err(&omap->pdev->dev,
					"xclk60mhsp2_ck failed"
					"error:%d\n", ret);
				goto err_xclk60mhsp2_ck;
			}

			/* Set the clock parent as External clock  */
			ret = clk_set_parent(omap->utmi_p2_fck,
						omap->xclk60mhsp2_ck);

			if (ret != 0) {
				dev_err(&omap->pdev->dev,
					"xclk60mhsp1_ck set parent"
					"failed error:%d\n", ret);
				goto err_xclk60mhsp2_ck1;
			}

			reg |= OMAP_UHH_HOST_P2_SET_ULPIPHY;

		} else {
			omap->xclk60mhsp2_ck = NULL;
			reg |= OMAP_UHH_HOST_P2_SET_ULPITLL;
		}

		uhhtll_omap_write(omap->uhh_base,
				OMAP_UHH_HOSTCONFIG, reg);

		dev_dbg(&omap->pdev->dev,
			"UHH setup done, uhh_hostconfig=%x\n", reg);

		if ((pdata->port_mode[0] == OMAP_EHCI_PORT_MODE_TLL) ||
			(pdata->port_mode[1] == OMAP_EHCI_PORT_MODE_TLL) ||
			(is_ohci_port(pdata->port_mode[0])) ||
			(is_ohci_port(pdata->port_mode[1]))) {

			/* perform TLL soft reset, and wait
			 * until reset is complete */
			uhhtll_omap_write(omap->tll_base, OMAP_USBTLL_SYSCONFIG,
					OMAP_USBTLL_SYSCONFIG_SOFTRESET);

			/* Wait for TLL reset to complete */
			while (!(uhhtll_omap_read(omap->tll_base,
				OMAP_USBTLL_SYSSTATUS) &
				OMAP_USBTLL_SYSSTATUS_RESETDONE)) {
				cpu_relax();

				if (time_after(jiffies, timeout)) {
					dev_err(&omap->pdev->dev,
						"operation timed out\n");
					ret = -EINVAL;
					goto err_xclk60mhsp2_ck1;
				}
			}

			dev_dbg(&omap->pdev->dev, "TLL RESET DONE\n");

			uhhtll_omap_write(omap->tll_base, OMAP_USBTLL_SYSCONFIG,
					OMAP_USBTLL_SYSCONFIG_ENAWAKEUP |
					OMAP_USBTLL_SYSCONFIG_SIDLEMODE |
					OMAP_USBTLL_SYSCONFIG_CACTIVITY);


			/* Enable UTMI mode for required TLL channels */
			usbhs_omap_tll_init(omap, OMAP4_TLL_CHANNEL_COUNT);
		}

		goto ok_end;


err_xclk60mhsp2_ck1:
		if (omap->xclk60mhsp2_ck)
			clk_put(omap->xclk60mhsp2_ck);

err_xclk60mhsp2_ck:
		clk_put(omap->utmi_p2_fck);
		omap->utmi_p2_fck = NULL;


err_xclk60mhsp1_ck1:
		if (omap->xclk60mhsp1_ck)
			clk_put(omap->xclk60mhsp1_ck);

err_xclk60mhsp1_ck:
		clk_put(omap->utmi_p1_fck);
		omap->utmi_p2_fck = NULL;

		goto err_end;

	} else {

		/* OMAP3 clocks*/
		/* USB OMAP3 hardware mod not yet exist*/
		omap->usbhost_ick = clk_get(&omap->pdev->dev, "usbhost_ick");
		if (IS_ERR(omap->usbhost_ick)) {
			ret =  PTR_ERR(omap->usbhost_ick);
			goto err_end;
		}
		clk_enable(omap->usbhost_ick);

		omap->usbhost_fs_fck = clk_get(&omap->pdev->dev,
						"usbhost_48m_fck");
		if (IS_ERR(omap->usbhost_fs_fck)) {
			ret = PTR_ERR(omap->usbhost_fs_fck);
			goto err_host_fs_fck;
		}
		clk_enable(omap->usbhost_fs_fck);

		/* Configure TLL for 60Mhz clk for ULPI */
		omap->usbtll_fck = clk_get(&omap->pdev->dev, "usbtll_fck");
		if (IS_ERR(omap->usbtll_fck)) {
			ret = PTR_ERR(omap->usbtll_fck);
			goto err_tll_fck;
		}
		clk_enable(omap->usbtll_fck);


		/* no port clocks */
		omap->utmi_p1_fck = NULL;
		omap->utmi_p2_fck = NULL;


		/* perform TLL soft reset, and wait
			until reset is complete */
		uhhtll_omap_write(omap->tll_base,
			OMAP_USBTLL_SYSCONFIG,
			OMAP_USBTLL_SYSCONFIG_SOFTRESET);

		timeout = jiffies + msecs_to_jiffies(1000);

		/* Wait for TLL reset to complete */
		while (!(uhhtll_omap_read(omap->tll_base,
			OMAP_USBTLL_SYSSTATUS)
			& OMAP_USBTLL_SYSSTATUS_RESETDONE)) {
			cpu_relax();
			if (time_after(jiffies, timeout)) {
				dev_dbg(&omap->pdev->dev,
					"operation timed out\n");
				ret = -EINVAL;
				goto err_sys_status;
			}
		}

		dev_dbg(&omap->pdev->dev, "TLL RESET DONE\n");


		/* (1<<3) = no idle mode only for initial debugging */
		uhhtll_omap_write(omap->tll_base, OMAP_USBTLL_SYSCONFIG,
				OMAP_USBTLL_SYSCONFIG_ENAWAKEUP |
				OMAP_USBTLL_SYSCONFIG_NIDLEMODE |
				OMAP_USBTLL_SYSCONFIG_CACTIVITY);

		/* Put UHH in NoIdle/NoStandby mode */
		reg = uhhtll_omap_read(omap->uhh_base, OMAP_UHH_SYSCONFIG);
		reg |= (OMAP_UHH_SYSCONFIG_ENAWAKEUP
			| OMAP_UHH_SYSCONFIG_SIDLEMODE
			| OMAP_UHH_SYSCONFIG_CACTIVITY
			| OMAP_UHH_SYSCONFIG_MIDLEMODE);
		reg &= ~OMAP_UHH_SYSCONFIG_AUTOIDLE;

		uhhtll_omap_write(omap->uhh_base, OMAP_UHH_SYSCONFIG, reg);

		reg = uhhtll_omap_read(omap->uhh_base, OMAP_UHH_HOSTCONFIG);

		/* setup ULPI bypass and burst configurations */
		reg |= (OMAP_UHH_HOSTCONFIG_INCR4_BURST_EN
			| OMAP_UHH_HOSTCONFIG_INCR8_BURST_EN
			| OMAP_UHH_HOSTCONFIG_INCR16_BURST_EN);
		reg &= ~OMAP_UHH_HOSTCONFIG_INCRX_ALIGN_EN;

		if (pdata->port_mode[0] == OMAP_USBHS_PORT_MODE_UNUSED)
			reg &= ~OMAP_UHH_HOSTCONFIG_P1_CONNECT_STATUS;
		if (pdata->port_mode[1] == OMAP_USBHS_PORT_MODE_UNUSED)
			reg &= ~OMAP_UHH_HOSTCONFIG_P2_CONNECT_STATUS;
		if (pdata->port_mode[2] == OMAP_USBHS_PORT_MODE_UNUSED)
			reg &= ~OMAP_UHH_HOSTCONFIG_P3_CONNECT_STATUS;

		/* Bypass the TLL module for PHY mode operation */
		if (cpu_is_omap3430() && (omap_rev() <= OMAP3430_REV_ES2_1)) {
			dev_dbg(&omap->pdev->dev,
				"OMAP3 ES version <= ES2.1\n");
			if ((pdata->port_mode[0] == OMAP_EHCI_PORT_MODE_PHY) ||
			(pdata->port_mode[1] == OMAP_EHCI_PORT_MODE_PHY) ||
			(pdata->port_mode[2] == OMAP_EHCI_PORT_MODE_PHY))
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_BYPASS;

			if (pdata->es2_compatibility) {

				/*
				 * All OHCI modes need to go through the TLL,
				 * unlike in the EHCI case. So use UTMI mode
				 * for all ports for OHCI, on ES2.x silicon
				 */
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_BYPASS;
			}

		}  else {

			dev_dbg(&omap->pdev->dev,
				"OMAP3 ES version > ES2.1\n");
			if (pdata->port_mode[0] == OMAP_EHCI_PORT_MODE_PHY)
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_P1_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_P1_BYPASS;

			if (pdata->port_mode[1] == OMAP_EHCI_PORT_MODE_PHY)
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_P2_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_P2_BYPASS;

			if (pdata->port_mode[2] == OMAP_EHCI_PORT_MODE_PHY)
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_P3_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_P3_BYPASS;

		}

		if (pdata->es2_compatibility) {
			/*
			 * All OHCI modes need to go through the TLL,
			 * unlike in the EHCI case. So use UTMI mode
			 * for all ports for OHCI, on ES2.x silicon
			 */
			dev_dbg(&omap->pdev->dev,
				"OMAP3 ES version <= ES2.1\n");
			reg |= OMAP_UHH_HOSTCONFIG_ULPI_BYPASS;
		} else {
			dev_dbg(&omap->pdev->dev,
				"OMAP3 ES version > ES2.1\n");
			if (pdata->port_mode[0] == OMAP_USBHS_PORT_MODE_UNUSED)
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_P1_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_P1_BYPASS;

			if (pdata->port_mode[1] == OMAP_USBHS_PORT_MODE_UNUSED)
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_P2_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_P2_BYPASS;

			if (pdata->port_mode[2] == OMAP_USBHS_PORT_MODE_UNUSED)
				reg &= ~OMAP_UHH_HOSTCONFIG_ULPI_P3_BYPASS;
			else
				reg |= OMAP_UHH_HOSTCONFIG_ULPI_P3_BYPASS;

		}
		uhhtll_omap_write(omap->uhh_base, OMAP_UHH_HOSTCONFIG, reg);
		dev_dbg(&omap->pdev->dev,
			"UHH setup done, uhh_hostconfig=%x\n", reg);


		if ((pdata->port_mode[0] != OMAP_EHCI_PORT_MODE_PHY) ||
			(pdata->port_mode[1] != OMAP_EHCI_PORT_MODE_PHY) ||
			(pdata->port_mode[2] != OMAP_EHCI_PORT_MODE_PHY)) {

			/* Enable UTMI mode for required TLL channels */
			usbhs_omap_tll_init(omap, OMAP_TLL_CHANNEL_COUNT);
		}

		goto ok_end;

err_sys_status:
		clk_disable(omap->usbtll_fck);
		clk_put(omap->usbtll_fck);

err_tll_fck:
		clk_disable(omap->usbhost_fs_fck);
		clk_put(omap->usbhost_fs_fck);

err_host_fs_fck:
		clk_disable(omap->usbhost_ick);
		clk_put(omap->usbhost_ick);

	}

err_end:
	if (pdata->phy_reset) {
		if (gpio_is_valid(pdata->reset_gpio_port[0]))
			gpio_free(pdata->reset_gpio_port[0]);

		if (gpio_is_valid(pdata->reset_gpio_port[1]))
			gpio_free(pdata->reset_gpio_port[1]);
	}
	return ret;

ok_end:
	if (pdata->phy_reset) {
		/* Refer ISSUE1:
		 * Hold the PHY in RESET for enough time till
		 * PHY is settled and ready
		 */
		udelay(10);

		if (gpio_is_valid(pdata->reset_gpio_port[0]))
			gpio_set_value(pdata->reset_gpio_port[0], 1);

		if (gpio_is_valid(pdata->reset_gpio_port[1]))
			gpio_set_value(pdata->reset_gpio_port[1], 1);
	}

end_clk:
	if (omap->utmi_p1_fck != NULL)
		clk_enable(omap->utmi_p1_fck);

	if (omap->utmi_p2_fck != NULL)
		clk_enable(omap->utmi_p2_fck);

	uhhtll_get_tput(&omap->pdev->dev);

end_count:
	omap->count++;
	return 0;
}




static void usbhs_disable(struct uhhtll_hcd_omap *omap, int do_stop)
{
	struct usbhs_omap_platform_data *pdata = &omap->platdata;
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	dev_dbg(&omap->pdev->dev, "Disable USBHS\n");

	if (omap->count == 0)
		return;

	omap->count--;

	if (omap->count == 0) {

		if (omap->utmi_p1_fck != NULL)
			clk_disable(omap->utmi_p1_fck);

		if (omap->utmi_p2_fck != NULL)
			clk_disable(omap->utmi_p2_fck);

		if (!do_stop)
			goto ok_end;

		/* set the hwmod to force idle */
		omap->uhh_hwmod->flags = (HWMOD_SWSUP_SIDLE |
					HWMOD_SWSUP_MSTANDBY);

		if (cpu_is_omap44xx())
			uhhtll_omap_write(omap->uhh_base,
					OMAP_UHH_SYSCONFIG,
					OMAP4_UHH_SYSCONFIG_SOFTRESET);
		else if (cpu_is_omap34xx()) {
			uhhtll_omap_write(omap->uhh_base,
					OMAP_UHH_SYSCONFIG,
					OMAP_UHH_SYSCONFIG_SOFTRESET);

			while (!(uhhtll_omap_read(omap->uhh_base,
				OMAP_UHH_SYSSTATUS) & (1 << 0))) {
				cpu_relax();

				if (time_after(jiffies, timeout))
					dev_dbg(&omap->pdev->dev,
					"operation timed out\n");
			}
		}
		while (!(uhhtll_omap_read(omap->uhh_base,
			OMAP_UHH_SYSSTATUS) & (1 << 1))) {
			cpu_relax();

			if (time_after(jiffies, timeout))
				dev_dbg(&omap->pdev->dev,
				"operation timed out\n");
		}

		while (!(uhhtll_omap_read(omap->uhh_base,
			OMAP_UHH_SYSSTATUS) & (1 << 2))) {
			cpu_relax();

			if (time_after(jiffies, timeout))
				dev_dbg(&omap->pdev->dev,
				"operation timed out\n");
		}

		uhhtll_omap_write(omap->tll_base,
				OMAP_USBTLL_SYSCONFIG,
				(1 << 1));

		while (!(uhhtll_omap_read(omap->tll_base,
			OMAP_USBTLL_SYSSTATUS) & (1 << 0))) {
			cpu_relax();

		if (time_after(jiffies, timeout))
			dev_dbg(&omap->pdev->dev,
				"operation timed out\n");
		}

		if (omap->utmi_p1_fck != NULL) {
			clk_put(omap->utmi_p1_fck);
			omap->utmi_p1_fck = NULL;
		}

		if (omap->xclk60mhsp1_ck != NULL) {
			clk_put(omap->xclk60mhsp1_ck);
			omap->xclk60mhsp1_ck = NULL;
		}

		if (omap->utmi_p2_fck != NULL) {
			clk_put(omap->utmi_p2_fck);
			omap->utmi_p2_fck = NULL;
		}

		if (omap->xclk60mhsp2_ck != NULL) {
			clk_put(omap->xclk60mhsp2_ck);
			omap->xclk60mhsp2_ck = NULL;
		}

		if (cpu_is_omap34xx()) {
			if (omap->usbhost_fs_fck != NULL) {
				clk_disable(omap->usbhost_fs_fck);
				clk_put(omap->usbhost_fs_fck);
				omap->usbhost_fs_fck = NULL;
			}

			if (omap->usbhost_ick != NULL) {
				clk_disable(omap->usbhost_ick);
				clk_put(omap->usbhost_ick);
				omap->usbhost_ick = NULL;
			}

			if (omap->usbtll_fck != NULL) {
				clk_disable(omap->usbtll_fck);
				clk_put(omap->usbtll_fck);
				omap->usbtll_fck = NULL;
			}
		}

ok_end:
		uhhtll_put_tput(&omap->pdev->dev);
		pm_runtime_put_sync(&omap->pdev->dev);
	}

	if ((do_stop) && (pdata->phy_reset)) {
		if (gpio_is_valid(pdata->reset_gpio_port[0]))
			gpio_free(pdata->reset_gpio_port[0]);

		if (gpio_is_valid(pdata->reset_gpio_port[1]))
			gpio_free(pdata->reset_gpio_port[1]);
	}

	dev_dbg(&omap->pdev->dev, " USBHS controller disabled\n");

}


static int uhhtll_get_platform_data(struct usbhs_omap_platform_data *pdata)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;

	if (!omap->pdev) {
		pr_err("UHH not yet intialized\n");
		return -EBUSY;
	}

	down_interruptible(&omap->mutex);

	memcpy(pdata, &omap->platdata, sizeof(omap->platdata));

	up(&omap->mutex);

	return 0;

}


static int uhhtll_drv_enable(enum driver_type drvtype)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;
	int ret = -EBUSY;

	if (!omap->pdev) {
		pr_err("UHH not yet intialized\n");
		return ret;
	}

	down_interruptible(&omap->mutex);

	ret = usbhs_enable(omap, 1);

	up(&omap->mutex);


	return ret;
}


static int uhhtll_drv_disable(enum driver_type drvtype)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;
	struct usbhs_omap_platform_data *pdata = &omap->platdata;

	if (!omap->pdev) {
		pr_err("UHH not yet intialized\n");
		return -EBUSY;
	}

	down_interruptible(&omap->mutex);

	usbhs_disable(omap, 1);

	if (drvtype == OMAP_EHCI)
		usbhs_ehci_io_wakeup(pdata->port_mode, 0);
	else if (drvtype == OMAP_OHCI)
		usbhs_ohci_io_wakeup(pdata->port_mode, 0);


	up(&omap->mutex);

	return 0;

}


static int uhhtll_drv_suspend(enum driver_type drvtype)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;
	struct usbhs_omap_platform_data *pdata = &omap->platdata;

	if (!omap->pdev) {
		pr_err("UHH not yet intialized\n");
		return -EBUSY;
	}

	down_interruptible(&omap->mutex);

	usbhs_disable(omap, 0);

	if (drvtype == OMAP_EHCI)
		usbhs_ehci_io_wakeup(pdata->port_mode, 1);
	else if (drvtype == OMAP_OHCI)
		usbhs_ohci_io_wakeup(pdata->port_mode, 1);

	up(&omap->mutex);

	return 0;
}

static int uhhtll_drv_resume(enum driver_type drvtype)
{
	struct uhhtll_hcd_omap *omap = &uhhtll;
	struct usbhs_omap_platform_data *pdata = &omap->platdata;
	int ret = -EBUSY;

	if (!omap->pdev) {
		pr_err("UHH not yet intialized\n");
		return ret;
	}

	down_interruptible(&omap->mutex);


	ret = usbhs_enable(omap, 0);

	if (drvtype == OMAP_EHCI)
		usbhs_ehci_io_wakeup(pdata->port_mode, 0);
	else if (drvtype == OMAP_OHCI)
		usbhs_ohci_io_wakeup(pdata->port_mode, 0);

	up(&omap->mutex);


	return ret;

}

/* MUX settings for EHCI pins */
/*
 * setup_ehci_io_mux - initialize IO pad mux for USBHOST
 */
static void setup_ehci_io_mux(const enum usbhs_omap3_port_mode *port_mode)
{
	switch (port_mode[0]) {
	case OMAP_EHCI_PORT_MODE_PHY:
		omap_mux_init_signal("hsusb1_stp", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb1_clk", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb1_dir", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_nxt", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data0", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data1", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data2", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data3", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data4", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data5", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data6", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_data7", OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_EHCI_PORT_MODE_TLL:
		omap_mux_init_signal("hsusb1_tll_stp",
			OMAP_PIN_INPUT_PULLUP);
		omap_mux_init_signal("hsusb1_tll_clk",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_dir",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_nxt",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb1_tll_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_EHCI_PORT_MODE_PHY:
		omap_mux_init_signal("hsusb2_stp", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb2_clk", OMAP_PIN_OUTPUT);
		omap_mux_init_signal("hsusb2_dir", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_nxt", OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_EHCI_PORT_MODE_TLL:
		omap_mux_init_signal("hsusb2_tll_stp",
			OMAP_PIN_INPUT_PULLUP);
		omap_mux_init_signal("hsusb2_tll_clk",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_dir",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_nxt",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb2_tll_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[2]) {
	case OMAP_EHCI_PORT_MODE_PHY:
		printk(KERN_WARNING "Port3 can't be used in PHY mode\n");
		break;
	case OMAP_EHCI_PORT_MODE_TLL:
		omap_mux_init_signal("hsusb3_tll_stp",
			OMAP_PIN_INPUT_PULLUP);
		omap_mux_init_signal("hsusb3_tll_clk",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_dir",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_nxt",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data1",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data2",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data3",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data4",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data5",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data6",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("hsusb3_tll_data7",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	return;
}


static void setup_4430ehci_io_mux(const enum usbhs_omap3_port_mode *port_mode)
{
	/*
	 * FIXME: This funtion should use mux framework functions;
	 * For now, we are hardcoding this.
	 */

	switch (port_mode[0]) {
	case OMAP_EHCI_PORT_MODE_PHY:

		/* HUSB1_PHY CLK , INPUT ENABLED, PULLDOWN  */
		omap_writew(0x010C, 0x4A1000C2);

		/* HUSB1 STP */
		omap_writew(0x0004, 0x4A1000C4);

		/* HUSB1_DIR */
		omap_writew(0x010C, 0x4A1000C6);

		/* HUSB1_NXT */
		omap_writew(0x010C, 0x4A1000C8);

		/* HUSB1_DATA0 */
		omap_writew(0x010C, 0x4A1000CA);

		/* HUSB1_DATA1 */
		omap_writew(0x010C, 0x4A1000CC);

		/* HUSB1_DATA2 */
		omap_writew(0x010C, 0x4A1000CE);

		/* HUSB1_DATA3 */
		omap_writew(0x010C, 0x4A1000D0);

		/* HUSB1_DATA4 */
		omap_writew(0x010C, 0x4A1000D2);

		/* HUSB1_DATA5 */
		omap_writew(0x010C, 0x4A1000D4);

		/* HUSB1_DATA6 */
		omap_writew(0x010C, 0x4A1000D6);

		/* HUSB1_DATA7 */
		omap_writew(0x010C, 0x4A1000D8);

		break;


	case OMAP_EHCI_PORT_MODE_TLL:
		/* TODO */


		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_EHCI_PORT_MODE_PHY:
		/* HUSB2_PHY CLK , INPUT PULLDOWN ENABLED  */
		omap_writew(0x010C, 0x4A100160);

		/* HUSB2 STP */
		omap_writew(0x0002, 0x4A100162);

		/* HUSB2_DIR */
		omap_writew(0x010A, 0x4A100164);

		/* HUSB2_NXT */
		omap_writew(0x010A, 0x4A100166);

		/* HUSB2_DATA0 */
		omap_writew(0x010A, 0x4A100168);

		/* HUSB2_DATA1 */
		omap_writew(0x010A, 0x4A10016A);

		/* HUSB2_DATA2 */
		omap_writew(0x010A, 0x4A10016C);

		/* HUSB2_DATA3 */
		omap_writew(0x010A, 0x4A10016E);

		/* HUSB2_DATA4 */
		omap_writew(0x010A, 0x4A100170);

		/* HUSB2_DATA5 */
		omap_writew(0x010A, 0x4A100172);

		/* HUSB2_DATA6 */
		omap_writew(0x010A, 0x4A100174);

		/* HUSB2_DATA7 */
		omap_writew(0x010A, 0x4A100176);

		break;

	case OMAP_EHCI_PORT_MODE_TLL:
		/* TODO */

		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}

	return;
}

static void setup_ohci_io_mux(const enum usbhs_omap3_port_mode *port_mode)
{
	switch (port_mode[0]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		omap_mux_init_signal("mm1_rxdp",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm1_rxdm",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		omap_mux_init_signal("mm1_rxrcv",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		omap_mux_init_signal("mm1_txen_n", OMAP_PIN_OUTPUT);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		omap_mux_init_signal("mm1_txse0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm1_txdat",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}
	switch (port_mode[1]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		omap_mux_init_signal("mm2_rxdp",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm2_rxdm",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		omap_mux_init_signal("mm2_rxrcv",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		omap_mux_init_signal("mm2_txen_n", OMAP_PIN_OUTPUT);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		omap_mux_init_signal("mm2_txse0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm2_txdat",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}
	switch (port_mode[2]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		omap_mux_init_signal("mm3_rxdp",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm3_rxdm",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		omap_mux_init_signal("mm3_rxrcv",
			OMAP_PIN_INPUT_PULLDOWN);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		omap_mux_init_signal("mm3_txen_n", OMAP_PIN_OUTPUT);
		/* FALLTHROUGH */
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		omap_mux_init_signal("mm3_txse0",
			OMAP_PIN_INPUT_PULLDOWN);
		omap_mux_init_signal("mm3_txdat",
			OMAP_PIN_INPUT_PULLDOWN);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
		/* FALLTHROUGH */
	default:
		break;
	}
}

static void setup_4430ohci_io_mux(const enum usbhs_omap3_port_mode *port_mode)
{
	/* FIXME: This funtion should use Mux frame work functions;
	 * for now, we are hardcodeing it
	 * This function will be later replaced by MUX framework API.
	 */
	switch (port_mode[0]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:

		/* usbb1_mm_rxdp */
		omap_writew(0x001D, 0x4A1000C4);

		/* usbb1_mm_rxdm */
		omap_writew(0x001D, 0x4A1000C8);

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:

		/* usbb1_mm_rxrcv */
		omap_writew(0x001D, 0x4A1000CA);

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb1_mm_txen */
		omap_writew(0x001D, 0x4A1000D0);

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb1_mm_txdat */
		omap_writew(0x001D, 0x4A1000CE);

		/* usbb1_mm_txse0 */
		omap_writew(0x001D, 0x4A1000CC);
		break;

	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		break;
	}

	switch (port_mode[1]) {
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:

		/* usbb2_mm_rxdp */
		omap_writew(0x010C, 0x4A1000F8);

		/* usbb2_mm_rxdm */
		omap_writew(0x010C, 0x4A1000F6);

	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:

		/* usbb2_mm_rxrcv */
		omap_writew(0x010C, 0x4A1000FA);

	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:

		/* usbb2_mm_txen */
		omap_writew(0x080C, 0x4A1000FC);

	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:

		/* usbb2_mm_txdat */
		omap_writew(0x000C, 0x4A100112);

		/* usbb2_mm_txse0 */
		omap_writew(0x000C, 0x4A100110);
		break;

	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		break;
	}
}






void __init usb_uhhtll_init(const struct usbhs_omap_platform_data *pdata)
{
	struct omap_hwmod *oh[2], *ohci_oh;
	struct omap_device *od;
	struct omap_hwmod_addr_space *taddr;
	int  bus_id = -1;

	if (cpu_is_omap34xx()) {
		setup_ehci_io_mux(pdata->port_mode);
		setup_ohci_io_mux(pdata->port_mode);
	} else if (cpu_is_omap44xx()) {
		setup_4430ehci_io_mux(pdata->port_mode);
		setup_4430ohci_io_mux(pdata->port_mode);
	}

	oh[0] = omap_hwmod_lookup(USB_UHH_HS_HWMODNAME);

	if (!oh[0]) {
		pr_err("Could not look up %s\n", USB_UHH_HS_HWMODNAME);
		return;
	}

	oh[1] = omap_hwmod_lookup(USB_TLL_HS_HWMODNAME);
	if (!oh[1]) {
		pr_err("Could not look up %s\n", USB_TLL_HS_HWMODNAME);
		return;
	}

	ohci_oh = omap_hwmod_lookup(USBHS_OHCI_HWMODNAME);

	if (!ohci_oh) {
		pr_err("Could not look up %s\n", USBHS_OHCI_HWMODNAME);
		return;
	}

	od = omap_device_build_ss(uhhtllname, bus_id, oh, 2,
				(void *)pdata, sizeof(*pdata),
				omap_uhhtll_latency,
				ARRAY_SIZE(omap_uhhtll_latency), false);

	if (IS_ERR(od)) {
		pr_err("Could not build hwmod devices %s and %s\n",
			USB_UHH_HS_HWMODNAME, USB_TLL_HS_HWMODNAME);
		return;
	}

	sema_init(&uhhtll.mutex, 1);

	/*we know that first address is OHCI base address */
	taddr = &(ohci_oh->slaves[0]->addr[0]);
	uhhtll.ohci_addr_start = taddr->pa_start;
	uhhtll.ohci_addr_size = taddr->pa_end - taddr->pa_start + 1;

	uhhtll.uhh_hwmod = oh[0];

	if (platform_driver_register(&uhhtll_hcd_omap_driver) < 0) {
		pr_err("Unable to register HSUSB UHH TLL driver\n");
		return;
	}

}

#else

void __init usb_uhhtll_init(const struct usbhs_omap_platform_data *pdata)
{
}

#endif


#if defined(CONFIG_USB_EHCI_HCD) || defined(CONFIG_USB_EHCI_HCD_MODULE)


static const char ehciname[] = "ehci-omap";
static u64 ehci_dmamask = ~(u32)0;


void __init usb_ehci_init(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	int  bus_id = -1;
	struct platform_device	*pdev;
	struct device	*dev;

	oh = omap_hwmod_lookup(USBHS_EHCI_HWMODNAME);

	if (!oh) {
		pr_err("Could not look up %s\n", USBHS_EHCI_HWMODNAME);
		return;
	}

	od = omap_device_build(ehciname, bus_id, oh,
				(void *)&uhhtll_export, sizeof(uhhtll_export),
				omap_uhhtll_latency,
				ARRAY_SIZE(omap_uhhtll_latency), false);

	if (IS_ERR(od)) {
		pr_err("Could not build hwmod device %s\n",
			USBHS_EHCI_HWMODNAME);
		return;
	} else {
		pdev = &od->pdev;
		dev = &pdev->dev;
		get_device(dev);
		dev->dma_mask = &ehci_dmamask;
		dev->coherent_dma_mask = 0xffffffff;
		put_device(dev);

	}
}

#else

void __init usb_ehci_init(void)
{
}

#endif /* CONFIG_USB_EHCI_HCD */

#if defined(CONFIG_USB_OHCI_HCD) || defined(CONFIG_USB_OHCI_HCD_MODULE)

#ifndef USB_UHHTLL_HCD_MODULE
#define USB_UHHTLL_HCD_MODULE
#endif


static const char ohciname[] = "ohci-omap3";
static u64 ohci_dmamask = DMA_BIT_MASK(32);


void __init usb_ohci_init(void)
{
	struct omap_hwmod *oh;
	struct omap_device *od;
	int  bus_id = -1;
	struct platform_device	*pdev;
	struct device	*dev;

	oh = omap_hwmod_lookup(USBHS_OHCI_HWMODNAME);

	if (!oh) {
		pr_err("Could not look up %s\n", USBHS_OHCI_HWMODNAME);
		return;
	}

	od = omap_device_build(ohciname, bus_id, oh,
				(void *)&uhhtll_export, sizeof(uhhtll_export),
				omap_uhhtll_latency,
				ARRAY_SIZE(omap_uhhtll_latency), false);

	if (IS_ERR(od)) {
		pr_err("Could not build hwmod device %s\n",
			USBHS_OHCI_HWMODNAME);
		return;
	} else {
		pdev = &od->pdev;
		dev = &pdev->dev;
		get_device(dev);
		dev->dma_mask = &ohci_dmamask;
		dev->coherent_dma_mask = 0xffffffff;
		put_device(dev);

	}
}

#else

void __init usb_ohci_init(void)
{
}
#endif /* CONFIG_USB_OHCI_HCD */
