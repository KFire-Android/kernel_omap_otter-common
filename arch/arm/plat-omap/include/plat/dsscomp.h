/*
 * arch/arm/plat-omap/include/plat/dsscomp.h
 *
 * DSS Composition platform specific header
 *
 * Copyright (C) 2011 Texas Instruments, Inc
 * Author: Lajos Molnar <molnar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _ARCH_ARM_PLAT_OMAP_DSSCOMP_H
#define _ARCH_ARM_PLAT_OMAP_DSSCOMP_H

#include <video/omapdss.h>
#include <video/dsscomp.h>

struct dsscomp_platform_data {
	unsigned int tiler1d_slotsz;
};

/* queuing operations */
struct dsscomp;
struct dsscomp *dsscomp_new(struct omap_overlay_manager *mgr);
u32 dsscomp_get_ovls(struct dsscomp *comp);
int dsscomp_set_ovl(struct dsscomp *comp, struct dss2_ovl_info *ovl);
int dsscomp_get_ovl(struct dsscomp *comp, u32 ix, struct dss2_ovl_info *ovl);
int dsscomp_set_mgr(struct dsscomp *comp, struct dss2_mgr_info *mgr);
int dsscomp_get_mgr(struct dsscomp *comp, struct dss2_mgr_info *mgr);
int dsscomp_setup(struct dsscomp *comp, enum dsscomp_setup_mode mode,
			struct dss2_rect_t win);
int dsscomp_delayed_apply(struct dsscomp *comp);
void dsscomp_drop(struct dsscomp *c);

struct tiler_pa_info;
int dsscomp_gralloc_queue(struct dsscomp_setup_dispc_data *d,
			struct tiler_pa_info **pas,
			bool early_callback,
			void (*cb_fn)(void *, int), void *cb_arg);

void dsscomp_set_platform_data(struct dsscomp_platform_data *data);

#endif
