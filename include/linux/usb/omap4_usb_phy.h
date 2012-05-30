/*
 * OMAP4 USB-phy
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *
 * Contact:
 *    Kishon Vijay Abraham I <kishon@ti.com>
 *    Eduardo Valentin <eduardo.valentin@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP4_USB_PHY_H
#define __OMAP4_USB_PHY_H

#define	PHY_PD				0x1
#define	AVALID				BIT(0)
#define	BVALID				BIT(1)
#define	VBUSVALID			BIT(2)
#define	SESSEND				BIT(3)
#define	IDDIG				BIT(4)

/* USB-PHY helpers */
#if (defined(CONFIG_OMAP4_USB_PHY)) || (defined(CONFIG_OMAP4_USB_PHY_MODULE))
extern int omap4_usb_phy_mailbox(struct device *dev, u32 val);
extern int omap4_usb_phy_power(struct device *dev, int on);
#else
static int omap4_usb_phy_mailbox(struct device *dev, u32 val)
{
	return 0;
}
static int omap4_usb_phy_power(struct device *dev, int on)
{
	return 0;
}
#endif

#endif
