/*
 * omap_usb.h -- omap usb2 phy header file
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

#ifndef __DRIVERS_OMAP_USB2_H
#define __DRIVERS_OMAP_USB2_H

#include <linux/usb/otg.h>

struct omap_usb {
	struct usb_phy		phy;
	struct phy_companion	*comparator;
	struct device		*dev;
	struct device		*control_dev;
	struct clk		*wkupclk;
	u8			is_suspended:1;
};

#define	phy_to_omapusb(x)	container_of((x), struct omap_usb, phy)

#if defined(CONFIG_OMAP_USB2) || defined(CONFIG_OMAP_USB2_MODULE)
void omap_usb2_set_comparator(struct phy_companion *comparator);
#else
static inline void omap_usb2_set_comparator(struct phy_companion *comparator)
{
}
#endif

#endif /* __DRIVERS_OMAP_USB_H */
