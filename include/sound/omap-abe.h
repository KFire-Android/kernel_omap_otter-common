/*
 * omap-abe.h  --  OMAP Audio Backend
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@ti.com>
 *          Misael Lopez Cruz <misael.lopez@ti.com>
 *          Sebastien Guiriec <s-guiriec@ti.com>
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

#ifndef _OMAP_ABE_H
#define _OMAP_ABE_H

struct omap_abe_pdata {
	int (*get_context_loss_count)(struct device *dev);
	int (*device_scale)(struct device *target_dev,
			    unsigned long rate);
	void (*disable_idle_on_suspend)(struct platform_device *pdev);
};

#endif
