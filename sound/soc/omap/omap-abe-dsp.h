/*
 * omap-abe-dsp.h
 *
 * Copyright (C) 2010 Texas Instruments
 *
 * Contact: Liam Girdwood <lrg@slimlogic.co.uk>
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

#ifndef __OMAP_ABE_DSP_H__
#define __OMAP_ABE_DSP_H__

/* Pong start offset of DMEM */
#define ABE_DMEM_BASE_ADDRESS_MPU	0x40180000L
/* Ping pong buffer DMEM offset */
#define ABE_DMEM_BASE_OFFSET_PING_PONG	0x4000

void abe_dsp_mcpdm_shutdown(void);
void abe_dsp_pm_get(void);
void abe_dsp_pm_put(void);

#endif	/* End of __OMAP_ABE_DSP_H__ */
