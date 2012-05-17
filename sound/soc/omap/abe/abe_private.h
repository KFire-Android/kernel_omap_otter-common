/*
 * abe_private.h  --  internal declarations for ABE HAL
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
 *
 */
#ifndef __SND_SOC_OMAP_ABE_HAL_ABE_PRIVATE_H__
#define __SND_SOC_OMAP_ABE_HAL_ABE_PRIVATE_H__

#include "abe_mem.h"

/* abe_addr.c */
extern struct omap_aess_init_task aess_init_table;

extern struct omap_aess_asrc_port aess_enable_vx_dl_8k_asrc;
extern struct omap_aess_asrc_port aess_enable_vx_dl_16k_asrc;
extern struct omap_aess_asrc_port aess_enable_vx_dl_48k_asrc;

extern struct omap_aess_asrc_port aess_enable_vx_ul_8k_asrc;
extern struct omap_aess_asrc_port aess_enable_vx_ul_16k_asrc;
extern struct omap_aess_asrc_port aess_enable_vx_ul_48k_asrc;

extern struct omap_aess_io_task aess_port_bt_vx_dl_8k;
extern struct omap_aess_io_task aess_port_bt_vx_dl_16k;
extern struct omap_aess_io_task aess_port_bt_vx_dl_48k;

extern struct omap_aess_io_task aess_port_bt_vx_ul_8k;
extern struct omap_aess_io_task aess_port_bt_vx_ul_16k;
extern struct omap_aess_io_task aess_port_bt_vx_ul_48k;

extern struct omap_aess_io_task aess_port_tones_dl_441k;
extern struct omap_aess_io_task aess_port_tones_dl_48k;

extern struct omap_aess_io_task aess_port_mm_dl_441k_pp;
extern struct omap_aess_io_task aess_port_mm_dl_441k;
extern struct omap_aess_io_task aess_port_mm_dl_48k;

extern struct omap_aess_init_task aess_port_mm_dl_48k_pp;

extern struct omap_aess_task aess_dl1_mono_mixer[];
extern struct omap_aess_task aess_dl2_mono_mixer[];
extern struct omap_aess_task aess_audul_mono_mixer[];

#endif /* __SND_SOC_OMAP_ABE_HAL_ABE_PRIVATE_H__ */
