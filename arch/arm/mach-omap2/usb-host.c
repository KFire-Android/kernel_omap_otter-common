/*
 * usb-host.c - OMAP USB Host
 *
 * This file will contain the board specific details for the
 * Synopsys EHCI/OHCI host controller on OMAP3430 and onwards
 *
 * Copyright (C) 2007-2011 Texas Instruments
 * Author: Vikram Pandita <vikram.pandita@ti.com>
 * Author: Keshava Munegowda <keshava_mgowda@ti.com>
 *
 * Generalization by:
 * Felipe Balbi <balbi@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>

#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <plat/usb.h>
#include <plat/omap_device.h>

#include "mux.h"

#ifdef CONFIG_MFD_OMAP_USB_HOST

#define OMAP_USBHS_DEVICE	"usbhs_omap"
#define OMAP_USBTLL_DEVICE	"usbhs_tll"
#define	USBHS_UHH_HWMODNAME	"usb_host_hs"
#define USBHS_TLL_HWMODNAME	"usb_tll_hs"

static struct usbhs_omap_platform_data		usbhs_data;
static struct usbtll_omap_platform_data		usbtll_data;
static struct ehci_hcd_omap_platform_data	ehci_data;
static struct ohci_hcd_omap_platform_data	ohci_data;

static struct omap_device_pm_latency omap_uhhtll_latency[] = {
	  {
		.deactivate_func = omap_device_idle_hwmods,
		.activate_func	 = omap_device_enable_hwmods,
		.flags = OMAP_DEVICE_LATENCY_AUTO_ADJUST,
	  },
};

/* MUX settings for EHCI pins */
static const struct omap_device_pad port1_phy_pads[] __initconst = {
	{
		.name = "usbb1_ulpitll_stp.usbb1_ulpiphy_stp",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_clk.usbb1_ulpiphy_clk",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dir.usbb1_ulpiphy_dir",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_nxt.usbb1_ulpiphy_nxt",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat0.usbb1_ulpiphy_dat0",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat1.usbb1_ulpiphy_dat1",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat2.usbb1_ulpiphy_dat2",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat3.usbb1_ulpiphy_dat3",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat4.usbb1_ulpiphy_dat4",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat5.usbb1_ulpiphy_dat5",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat6.usbb1_ulpiphy_dat6",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb1_ulpitll_dat7.usbb1_ulpiphy_dat7",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
};

static const struct omap_device_pad port1_tll_pads[] __initconst = {
	{
		.name = "usbb1_ulpitll_stp.usbb1_ulpitll_stp",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_clk.usbb1_ulpitll_clk",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dir.usbb1_ulpitll_dir",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_nxt.usbb1_ulpitll_nxt",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat0.usbb1_ulpitll_dat0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat1.usbb1_ulpitll_dat1",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat2.usbb1_ulpitll_dat2",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat3.usbb1_ulpitll_dat3",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat4.usbb1_ulpitll_dat4",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat5.usbb1_ulpitll_dat5",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat6.usbb1_ulpitll_dat6",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_ulpitll_dat7.usbb1_ulpitll_dat7",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
};

static const struct omap_device_pad port1_hsic_pads[] __initconst = {
	{
		.name = "usbb1_hsic_data.usbb1_hsic_data",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle =  OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb1_hsic_strobe.usbb1_hsic_strobe",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static const struct omap_device_pad port2_hsic_pads[] __initconst = {
	{
		.name = "usbb2_hsic_data.usbb2_hsic_data",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_hsic_strobe.usbb2_hsic_strobe",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static const struct omap_device_pad port3_hsic_pads[] __initconst = {
	{
		.name = "usbb3_hsic_data.usbb3_hsic_data",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb3_hsic_strobe.usbb3_hsic_strobe",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT | OMAP_MUX_MODE0,
	},
};

static const struct omap_device_pad port2_phy_pads[] __initconst = {
	{
		.name = "usbb2_ulpitll_stp.usbb2_ulpiphy_stp",
		.enable = OMAP_PIN_OUTPUT | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_clk.usbb2_ulpiphy_clk",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dir.usbb2_ulpiphy_dir",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_nxt.usbb2_ulpiphy_nxt",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat0.usbb2_ulpiphy_dat0",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat1.usbb2_ulpiphy_dat1",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat2.usbb2_ulpiphy_dat2",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat3.usbb2_ulpiphy_dat3",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat4.usbb2_ulpiphy_dat4",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat5.usbb2_ulpiphy_dat5",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat6.usbb2_ulpiphy_dat6",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "usbb2_ulpitll_dat7.usbb2_ulpiphy_dat7",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
};

static const struct omap_device_pad port2_tll_pads[] __initconst = {
	{
		.name = "usbb2_ulpitll_stp.usbb2_ulpitll_stp",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
		.idle = OMAP_PIN_INPUT_PULLUP | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_clk.usbb2_ulpitll_clk",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dir.usbb2_ulpitll_dir",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_nxt.usbb2_ulpitll_nxt",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat0.usbb2_ulpitll_dat0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat1.usbb2_ulpitll_dat1",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat2.usbb2_ulpitll_dat2",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat3.usbb2_ulpitll_dat3",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat4.usbb2_ulpitll_dat4",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat5.usbb2_ulpitll_dat5",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat6.usbb2_ulpitll_dat6",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
	{
		.name = "usbb2_ulpitll_dat7.usbb2_ulpitll_dat7",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE0,
	},
};

static const struct omap_device_pad port1_6pin_pads[] __initconst = {
	{
		.name = "usbb1_ulpitll_stp.usbb1_mm_rxdp",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_nxt.usbb1_mm_rxdm",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat0.usbb1_mm_rxrcv",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat3.usbb1_mm_txen",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat1.usbb1_mm_txdat",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat2.usbb1_mm_txse0",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
};

static const struct omap_device_pad port1_4pin_pads[] __initconst = {
	{
		.name = "usbb1_ulpitll_dat0.usbb1_mm_rxrcv",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat3.usbb1_mm_txen",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat1.usbb1_mm_txdat",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat2.usbb1_mm_txse0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
};

static const struct omap_device_pad port1_3pin_pads[] __initconst = {
	{
		.name = "usbb1_ulpitll_dat3.usbb1_mm_txen",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat1.usbb1_mm_txdat",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat2.usbb1_mm_txse0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
};

static const struct omap_device_pad port1_2pin_pads[] __initconst = {
	{
		.name = "usbb1_ulpitll_dat1.usbb1_mm_txdat",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
	{
		.name = "usbb1_ulpitll_dat2.usbb1_mm_txse0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE5,
	},
};

static const struct omap_device_pad port2_6pin_pads[] __initconst = {
	{
		.name = "abe_mcbsp2_dr.usbb2_mm_rxdp",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_mcbsp2_clkx.usbb2_mm_rxdm",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_mcbsp2_dx.usbb2_mm_rxrcv",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_mcbsp2_fsx.usbb2_mm_txen",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_din1.usbb2_mm_txdat",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_clk1.usbb2_mm_txse0",
		.flags  = OMAP_DEVICE_PAD_REMUX | OMAP_DEVICE_PAD_WAKEUP,
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
		.idle	= OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
};

static const struct omap_device_pad port2_4pin_pads[] __initconst = {
	{
		.name = "abe_mcbsp2_dx.usbb2_mm_rxrcv",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_mcbsp2_fsx.usbb2_mm_txen",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_din1.usbb2_mm_txdat",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_clk1.usbb2_mm_txse0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
};

static const struct omap_device_pad port2_3pin_pads[] __initconst = {
	{
		.name = "abe_mcbsp2_fsx.usbb2_mm_txen",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_din1.usbb2_mm_txdat",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_clk1.usbb2_mm_txse0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
};

static const struct omap_device_pad port2_2pin_pads[] __initconst = {
	{
		.name = "abe_mcbsp2_fsx.usbb2_mm_txen",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_din1.usbb2_mm_txdat",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
	{
		.name = "abe_dmic_clk1.usbb2_mm_txse0",
		.enable = OMAP_PIN_INPUT_PULLDOWN | OMAP_MUX_MODE4,
	},
};
/*
 * setup_ehci_io_mux - initialize IO pad mux for USBHOST
 */
static void __init setup_ehci_io_mux(const enum usbhs_omap_port_mode *port_mode)
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


static struct omap_hwmod_mux_info * __init
omap_hwmod_mux_array_init(struct platform_device	*pdev,
			  const struct omap_device_pad *bpads[],
			  int *nr_pads,
			  const enum usbhs_omap_port_mode *port_mode)
{
	struct omap_device_pad		*pads;
	struct omap_hwmod_mux_info	*hmux;
	struct omap_device		*od;
	struct omap_hwmod		*uhh_hwm;
	size_t				npads;
	u32				i, k, j;

	for (i = 0, npads = 0; i < OMAP3_HS_USB_PORTS; i++)
		npads += nr_pads[i];

	pads = kmalloc(sizeof(struct omap_device_pad)*npads, GFP_KERNEL);

	if (!pads) {
		pr_err("%s: Could not allocate device mux entry\n", __func__);
		return NULL;
	}

	for (i = 0, k = 0; i < OMAP3_HS_USB_PORTS; i++)
		if (nr_pads[i]) {
			memcpy(pads + k, bpads[i],
				sizeof(struct omap_device_pad) * nr_pads[i]);
			k +=  nr_pads[i];
		}
	hmux = omap_hwmod_mux_init(pads, npads);

	if (!pdev)
		goto end;

	od = to_omap_device(pdev);
	uhh_hwm = od->hwmods[0];
	if (!uhh_hwm)
		goto end;

	uhh_hwm->mux = hmux;
	for (i = 0, k = 0; i < OMAP3_HS_USB_PORTS; i++) {
		switch (port_mode[i]) {
		case OMAP_EHCI_PORT_MODE_PHY:
		case OMAP_EHCI_PORT_MODE_TLL:
		case OMAP_EHCI_PORT_MODE_HSIC:
			for (j = 0; j < nr_pads[i]; j++)
				omap_hwmod_pad_route_irq(uhh_hwm, k + j, 1);
			break;
		case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
		case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
		case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
		case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
		case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
		case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
		case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
			for (j = 0; j < nr_pads[i]; j++)
				omap_hwmod_pad_route_irq(uhh_hwm, k + j, 0);
			break;

		case OMAP_USBHS_PORT_MODE_UNUSED:
		default:
			break;
		}
		k += nr_pads[i];
	}

end:
	kfree(pads);
	return hmux;
}

static struct omap_hwmod_mux_info * __init
setup_4430_usbhs_io_mux(struct platform_device	*pdev,
			const enum usbhs_omap_port_mode *port_mode)
{
	const struct omap_device_pad	*pads[OMAP3_HS_USB_PORTS];
	int				pads_cnt[OMAP3_HS_USB_PORTS];

	switch (port_mode[0]) {
	case OMAP_EHCI_PORT_MODE_PHY:
		pads[0] = port1_phy_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_phy_pads);
			break;
	case OMAP_EHCI_PORT_MODE_TLL:
		pads[0] = port1_tll_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_tll_pads);
			break;
	case OMAP_EHCI_PORT_MODE_HSIC:
		pads[0] = port1_hsic_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_hsic_pads);
			break;
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		pads[0] = port1_6pin_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_6pin_pads);
			break;
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		pads[0] = port1_4pin_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_4pin_pads);
			break;
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		pads[0] = port1_3pin_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_3pin_pads);
			break;
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		pads[0] = port1_2pin_pads;
		pads_cnt[0] = ARRAY_SIZE(port1_2pin_pads);
			break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		pads_cnt[0] = 0;
			break;
	}
	switch (port_mode[1]) {
	case OMAP_EHCI_PORT_MODE_PHY:
		pads[1] = port2_phy_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_phy_pads);
			break;
	case OMAP_EHCI_PORT_MODE_TLL:
		pads[1] = port2_tll_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_tll_pads);
			break;
	case OMAP_EHCI_PORT_MODE_HSIC:
		pads[1] = port2_hsic_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_hsic_pads);
			break;
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_PHY_6PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_6PIN_DPDM:
		pads[1] = port2_6pin_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_6pin_pads);
		break;
	case OMAP_OHCI_PORT_MODE_PHY_4PIN_DPDM:
	case OMAP_OHCI_PORT_MODE_TLL_4PIN_DPDM:
		pads[1] = port2_4pin_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_4pin_pads);
		break;
	case OMAP_OHCI_PORT_MODE_PHY_3PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_3PIN_DATSE0:
		pads[1] = port2_3pin_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_3pin_pads);
		break;
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DATSE0:
	case OMAP_OHCI_PORT_MODE_TLL_2PIN_DPDM:
		pads[1] = port2_2pin_pads;
		pads_cnt[1] = ARRAY_SIZE(port2_3pin_pads);
		break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		pads_cnt[1] = 0;
			break;
	}
	switch (port_mode[2]) {
	case OMAP_EHCI_PORT_MODE_HSIC:
		pads[2] = port3_hsic_pads;
		pads_cnt[2] = ARRAY_SIZE(port3_hsic_pads);
			break;
	case OMAP_USBHS_PORT_MODE_UNUSED:
	default:
		pads_cnt[2] = 0;
			break;
	}

	return omap_hwmod_mux_array_init(pdev, pads, pads_cnt, port_mode);
}

static void __init setup_ohci_io_mux(const enum usbhs_omap_port_mode *port_mode)
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

void __init usbhs_init(const struct usbhs_omap_board_data *pdata)
{
	struct omap_hwmod	*uhh_hwm, *tll_hwm;
	struct platform_device	*pdev;
	int			bus_id = -1;
	int			i;

	for (i = 0; i < OMAP3_HS_USB_PORTS; i++) {
		usbhs_data.port_mode[i] = pdata->port_mode[i];
		usbtll_data.port_mode[i] = pdata->port_mode[i];
		ohci_data.port_mode[i] = pdata->port_mode[i];
		ehci_data.port_mode[i] = pdata->port_mode[i];
		ehci_data.reset_gpio_port[i] = pdata->reset_gpio_port[i];
		ehci_data.regulator[i] = pdata->regulator[i];
	}
	ehci_data.phy_reset = pdata->phy_reset;
	ohci_data.es2_compatibility = pdata->es2_compatibility;
	usbhs_data.ehci_data = &ehci_data;
	usbhs_data.ohci_data = &ohci_data;

	uhh_hwm = omap_hwmod_lookup(USBHS_UHH_HWMODNAME);
	if (!uhh_hwm) {
		pr_err("Could not look up %s\n", USBHS_UHH_HWMODNAME);
		return;
	}

	tll_hwm = omap_hwmod_lookup(USBHS_TLL_HWMODNAME);
	if (!tll_hwm) {
		pr_err("Could not look up %s\n", USBHS_TLL_HWMODNAME);
		return;
	}

	pdev = omap_device_build(OMAP_USBTLL_DEVICE, bus_id, tll_hwm,
				(void *)&usbtll_data, sizeof(usbtll_data),
				omap_uhhtll_latency,
				ARRAY_SIZE(omap_uhhtll_latency), false);
	if (IS_ERR(pdev)) {
		pr_err("Could not build hwmod device %s\n",
			USBHS_TLL_HWMODNAME);
		return;
	}

	pdev = omap_device_build(OMAP_USBHS_DEVICE, bus_id, uhh_hwm,
				(void *)&usbhs_data, sizeof(usbhs_data),
				omap_uhhtll_latency,
				ARRAY_SIZE(omap_uhhtll_latency), false);
	if (IS_ERR(pdev)) {
		pr_err("Could not build hwmod devices %s\n",
			USBHS_UHH_HWMODNAME);
		return;
	}

	if (cpu_is_omap34xx()) {
		setup_ehci_io_mux(pdata->port_mode);
		setup_ohci_io_mux(pdata->port_mode);
	} else if (cpu_is_omap44xx() || cpu_is_omap54xx())
		uhh_hwm->mux = setup_4430_usbhs_io_mux(pdev, pdata->port_mode);
}

#else

void __init usbhs_init(const struct usbhs_omap_board_data *pdata)
{
}

#endif
