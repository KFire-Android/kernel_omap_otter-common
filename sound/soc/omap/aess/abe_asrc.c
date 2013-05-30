/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2012 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.GPL.
 *
 * BSD LICENSE
 *
 * Copyright(c) 2010-2012 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe.h"
#include "abe_dbg.h"
#include "abe_mem.h"

/**
 * omap_aess_write_fifo
 * @abe: Pointer on aess handle
 * @memory_bank: currently only ABE_DMEM supported
 * @descr_addr: FIFO descriptor address (descriptor fields: READ ptr, WRITE ptr,
 * FIFO START_ADDR, FIFO END_ADDR)
 * @data: data to write to FIFO
 * @nb_data32: number of 32-bit words to write to DMEM FIFO
 *
 * write DMEM FIFO and update FIFO descriptor,
 * it is assumed that FIFO descriptor is located in DMEM
 */
static void omap_aess_write_fifo(struct omap_aess *abe, u32 memory_bank,
				 u32 descr_addr, u32 *data, u32 nb_data32)
{
	u32 fifo_addr[4];
	u32 i;

	/* read FIFO descriptor from DMEM */
	omap_abe_mem_read(abe, OMAP_ABE_DMEM, descr_addr,
			  &fifo_addr[0], 4 * sizeof(u32));

	/* WRITE ptr < FIFO start address */
	if ((fifo_addr[1] < fifo_addr[2]) || (fifo_addr[1] > fifo_addr[3]))
		aess_err("FIFO write pointer Error");

	switch (memory_bank) {
	case OMAP_ABE_DMEM:
		for (i = 0; i < nb_data32; i++) {
			omap_abe_mem_write(abe, OMAP_ABE_DMEM,
					   (s32)fifo_addr[1],
					   (u32 *)(data + i),
					   4);
			/* increment WRITE pointer */
			fifo_addr[1] = fifo_addr[1] + 4;
			if (fifo_addr[1] > fifo_addr[3])
				fifo_addr[1] = fifo_addr[2];
			if (fifo_addr[1] == fifo_addr[0])
				aess_err("FIFO write pointer Error");
		}
		/* update WRITE pointer in DMEM */
		omap_abe_mem_write(abe, OMAP_ABE_DMEM, descr_addr +
				   sizeof(u32), &fifo_addr[1], 4);
		break;
	default:
		break;
	}
}

/**
 * abe_init_asrc_vx_dl
 * @abe: Pointer on aess handle
 * @dppm: PPM drift value
 *
 * Initialize the following ASRC VX_DL parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_DL_VX_Coefs pointer
 * 10. CMEM for ASRC_DL_VX_Coefs pointer
 * ASRC_DL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
 * C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1
 * 11. SMEM for XinASRC_DL_VX pointer
 * 12. CMEM for XinASRC_DL_VX pointer
 * XinASRC_DL_VX = S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/0/1/0/0/0/0
 * 13. SMEM for IO_VX_DL_ASRC pointer
 * 14. CMEM for IO_VX_DL_ASRC pointer
 * IO_VX_DL_ASRC =
 *	S_XinASRC_DL_VX_ADDR/S_XinASRC_DL_VX_sizeof/
 *	ASRC_DL_VX_FIR_L+ASRC_margin/1/0/0/0/0
 */
void omap_aess_init_asrc_vx_dl(struct omap_aess *abe, s32 dppm)
{
	int offset, n_fifo_el;

	n_fifo_el = 42;
	if (dppm == 250)
		offset = 48 * 2 + n_fifo_el;
	else
		offset = 48 * 2;

	omap_aess_write_fifo(abe, OMAP_ABE_DMEM,
			     abe->fw_info->map[OMAP_AESS_DMEM_FWMEMINITDESCR_ID].offset,
			     &abe->fw_info->asrc[offset], n_fifo_el);
}
/**
 * abe_init_asrc_vx_ul
 * @abe: Pointer on aess handle
 * @dppm: PPM drift value
 *
 * Initialize the following ASRC VX_UL parameters :
 * 1. DriftSign = D_AsrcVars[1] = 1 or -1
 * 2. Subblock = D_AsrcVars[2] = 0
 * 3. DeltaAlpha = D_AsrcVars[3] =
 *	(round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 4. MinusDeltaAlpha = D_AsrcVars[4] =
 *	(-round(nb_phases * drift[ppm] * 10^-6 * 2^20)) << 2
 * 5. OneMinusEpsilon = D_AsrcVars[5] = 1 - DeltaAlpha/2
 * 6. AlphaCurrent = 0x000020 (CMEM), initial value of Alpha parameter
 * 7. BetaCurrent = 0x3fffe0 (CMEM), initial value of Beta parameter
 * AlphaCurrent + BetaCurrent = 1 (=0x400000 in CMEM = 2^20 << 2)
 * 8. drift_ASRC = 0 & drift_io = 0
 * 9. SMEM for ASRC_UL_VX_Coefs pointer
 * 10. CMEM for ASRC_UL_VX_Coefs pointer
 * ASRC_UL_VX_Coefs = C_CoefASRC16_VX_ADDR/C_CoefASRC16_VX_sizeof/0/1/
 *	C_CoefASRC15_VX_ADDR/C_CoefASRC15_VX_sizeof/0/1
 * 11. SMEM for XinASRC_UL_VX pointer
 * 12. CMEM for XinASRC_UL_VX pointer
 * XinASRC_UL_VX = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/0/1/0/0/0/0
 * 13. SMEM for UL_48_8_DEC pointer
 * 14. CMEM for UL_48_8_DEC pointer
 * UL_48_8_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
 *	ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0
 * 15. SMEM for UL_48_16_DEC pointer
 * 16. CMEM for UL_48_16_DEC pointer
 * UL_48_16_DEC = S_XinASRC_UL_VX_ADDR/S_XinASRC_UL_VX_sizeof/
 *	ASRC_UL_VX_FIR_L+ASRC_margin/1/0/0/0/0
 */
void omap_aess_init_asrc_vx_ul(struct omap_aess *abe, s32 dppm)
{
	int offset, n_fifo_el;

	n_fifo_el = 48;
	if (dppm == -250)
		offset = n_fifo_el;
	else
		offset = 0;

	omap_aess_write_fifo(abe, OMAP_ABE_DMEM,
			     abe->fw_info->map[OMAP_AESS_DMEM_FWMEMINITDESCR_ID].offset,
			     &abe->fw_info->asrc[offset], n_fifo_el);
}
