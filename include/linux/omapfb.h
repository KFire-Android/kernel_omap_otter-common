/*
 * File: include/linux/omapfb.h
 *
 * Framebuffer driver for TI OMAP boards
 *
 * Copyright (C) 2004 Nokia Corporation
 * Author: Imre Deak <imre.deak@nokia.com>
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
#ifndef __LINUX_OMAPFB_H__
#define __LINUX_OMAPFB_H__

#include <uapi/linux/omapfb.h>
#ifdef CONFIG_ARCH_OMAP1
#define OMAPFB_PLANE_NUM		1
#else
#define OMAPFB_PLANE_NUM		3
#endif

struct omapfb_mem_region {
	u32		paddr;
	void __iomem	*vaddr;
	unsigned long	size;
	u8		type;		/* OMAPFB_PLANE_MEM_* */
	enum omapfb_color_format format;/* OMAPFB_COLOR_* */
	unsigned	format_used:1;	/* Must be set when format is set.
					 * Needed b/c of the badly chosen 0
					 * base for OMAPFB_COLOR_* values
					 */
	unsigned	alloc:1;	/* allocated by the driver */
	unsigned	map:1;		/* kernel mapped by the driver */
};

struct omapfb_mem_desc {
	int				region_cnt;
	struct omapfb_mem_region	region[OMAPFB_PLANE_NUM];
};

struct omap_lcd_config {
	char panel_name[16];
	char ctrl_name[16];
	s16  nreset_gpio;
	u8   data_lines;
};

struct omapfb_platform_data {
	struct omap_lcd_config		lcd;
	struct omapfb_mem_desc		mem_desc;
};

extern void omapfb_set_platform_data(struct omapfb_platform_data *data);

void __init omapfb_set_lcd_config(const struct omap_lcd_config *config);

/* helper methods that may be used by other modules */
enum omap_color_mode;
int omapfb_mode_to_dss_mode(struct fb_var_screeninfo *var,
                       enum omap_color_mode *mode);
struct omap_video_timings;
void omapfb_fb2dss_timings(struct fb_videomode *fb_timings,
				struct omap_video_timings *dss_timings);
void omapfb_dss2fb_timings(struct omap_video_timings *dss_timings,
				struct fb_videomode *fb_timings);

#endif /* __OMAPFB_H */
