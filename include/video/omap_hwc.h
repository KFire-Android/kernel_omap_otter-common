/*
 *
 * Copyright (C) 2012 Texas Instruments
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _LINUX_OMAP_HWC_H
#define _LINUX_OMAP_HWC_H

struct rgz_blt_entry {
	struct bvbltparams bp;
	struct bvsurfgeom dstgeom;
	struct bvsurfgeom src1geom;
	struct bvbuffdesc src1desc;
	struct bvsurfgeom src2geom;
	struct bvbuffdesc src2desc;
};

struct omap_hwc_blit_data {
	/* if rgz_items is 0 there is nothing to blit */
	__u16 rgz_items;
	struct rgz_blt_entry rgz_blts[0];
};

/*
 * This structure is passed down from the Android HWC HAL
 */
struct omap_hwc_data {
	struct dsscomp_setup_dispc_data dsscomp_data;
	struct omap_hwc_blit_data blit_data;
};

#endif
