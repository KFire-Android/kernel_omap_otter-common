/*
 * arch/arm/mach-omap2/omap-rfkill-ste.h
 *
 * OMAP driver header for rfkill ST-E modem driver
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef __OMAP_RFKILL_STE_H__
#define __OMAP_RFKILL_STE_H__

#include <linux/types.h>
#include <linux/rfkill.h>
#include <plat/omap_hwmod.h>


/**
 * struct omap_rfkill_ste_platform_data - platform data for rfkill ste device.
 * @omap_device_pad:			array of OMAP/modem interface pads
 * @omap_pads_len:			pads array length
 */

struct omap_rfkill_ste_platform_data {
	struct omap_device_pad	*omap_pads;
	unsigned int		omap_pads_len;
};

#endif /* __OMAP_RFKILL_STE_H__ */
