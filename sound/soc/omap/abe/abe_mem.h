/*

  This file is provided under a dual BSD/GPLv2 license.  When using or
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution
  in the file called LICENSE.GPL.

  BSD LICENSE

  Copyright(c) 2010-2011 Texas Instruments Incorporated,
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.
    * Neither the name of Texas Instruments Incorporated nor the names of
      its contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef _ABE_MEM_H_
#define _ABE_MEM_H_

#define OMAP_ABE_DMEM 0
#define OMAP_ABE_CMEM 1
#define OMAP_ABE_SMEM 2
#define OMAP_ABE_PMEM 3
#define OMAP_ABE_AESS 4

struct omap_aess_addr {
	int bank;
	unsigned int offset;
	unsigned int bytes;
};







#define OMAP_AESS_DMEM_MULTIFRAME_ID	0
#define OMAP_AESS_DMEM_DMIC_UL_FIFO_ID	1
#define OMAP_AESS_DMEM_MCPDM_UL_FIFO_ID	2
#define OMAP_AESS_DMEM_BT_UL_FIFO_ID	3
#define OMAP_AESS_DMEM_MM_UL_FIFO_ID	4
#define OMAP_AESS_DMEM_MM_UL2_FIFO_ID	5
#define OMAP_AESS_DMEM_VX_UL_FIFO_ID	6
#define OMAP_AESS_DMEM_MM_DL_FIFO_ID	7
#define OMAP_AESS_DMEM_VX_DL_FIFO_ID	8
#define OMAP_AESS_DMEM_TONES_DL_FIFO_ID	9
#define OMAP_AESS_DMEM_VIB_DL_FIFO_ID	10
#define OMAP_AESS_DMEM_BT_DL_FIFO_ID	11
#define OMAP_AESS_DMEM_MCPDM_DL_FIFO_ID	12
#define OMAP_AESS_DMEM_MM_EXT_OUT_FIFO_ID	13
#define OMAP_AESS_DMEM_MM_EXT_IN_FIFO_ID	14
#define OMAP_AESS_SMEM_DMIC0_96_48_DATA_ID	15
#define OMAP_AESS_SMEM_DMIC1_96_48_DATA_ID	16
#define OMAP_AESS_SMEM_DMIC2_96_48_DATA_ID	17
#define OMAP_AESS_SMEM_AMIC_96_48_DATA_ID	18
#define OMAP_AESS_SMEM_BT_UL_ID	19
#define OMAP_AESS_SMEM_BT_UL_8_48_HP_DATA_ID	20
#define OMAP_AESS_SMEM_BT_UL_8_48_LP_DATA_ID	21
#define OMAP_AESS_SMEM_BT_UL_16_48_HP_DATA_ID	22
#define OMAP_AESS_SMEM_BT_UL_16_48_LP_DATA_ID	23
#define OMAP_AESS_SMEM_MM_UL2_ID	24
#define OMAP_AESS_SMEM_MM_UL_ID	25
#define OMAP_AESS_SMEM_VX_UL_ID	26
#define OMAP_AESS_SMEM_VX_UL_48_8_HP_DATA_ID	27
#define OMAP_AESS_SMEM_VX_UL_48_8_LP_DATA_ID	28
#define OMAP_AESS_SMEM_VX_UL_48_16_HP_DATA_ID	29
#define OMAP_AESS_SMEM_VX_UL_48_16_LP_DATA_ID	30
#define OMAP_AESS_SMEM_MM_DL_ID	31
#define OMAP_AESS_SMEM_MM_DL_44P1_ID	32
#define OMAP_AESS_SMEM_MM_DL_44P1_XK_ID	33
#define OMAP_AESS_SMEM_VX_DL_ID	34
#define OMAP_AESS_SMEM_VX_DL_8_48_HP_DATA_ID	35
#define OMAP_AESS_SMEM_VX_DL_8_48_LP_DATA_ID	36
#define OMAP_AESS_SMEM_VX_DL_8_48_OSR_LP_DATA_ID	37
#define OMAP_AESS_SMEM_VX_DL_16_48_HP_DATA_ID	38
#define OMAP_AESS_SMEM_VX_DL_16_48_LP_DATA_ID	39
#define OMAP_AESS_SMEM_TONES_ID	40
#define OMAP_AESS_SMEM_TONES_44P1_ID	41
#define OMAP_AESS_SMEM_TONES_44P1_XK_ID	42
#define OMAP_AESS_SMEM_VIBRA_ID	43
#define OMAP_AESS_SMEM_BT_DL_ID	44
#define OMAP_AESS_SMEM_BT_DL_8_48_OSR_LP_DATA_ID	45
#define OMAP_AESS_SMEM_BT_DL_48_8_HP_DATA_ID	46
#define OMAP_AESS_SMEM_BT_DL_48_8_LP_DATA_ID	47
#define OMAP_AESS_SMEM_BT_DL_48_16_HP_DATA_ID	48
#define OMAP_AESS_SMEM_BT_DL_48_16_LP_DATA_ID	49
#define OMAP_AESS_SMEM_DL2_M_LR_EQ_DATA_ID	50
#define OMAP_AESS_SMEM_DL1_M_EQ_DATA_ID	51
#define OMAP_AESS_SMEM_EARP_48_96_LP_DATA_ID	52
#define OMAP_AESS_SMEM_IHF_48_96_LP_DATA_ID	53
#define OMAP_AESS_SMEM_DC_HS_ID	54
#define OMAP_AESS_SMEM_DC_HF_ID	55
#define OMAP_AESS_SMEM_SDT_F_DATA_ID	56
#define OMAP_AESS_SMEM_GTARGET1_ID	57
#define OMAP_AESS_SMEM_GCURRENT_ID	58
#define OMAP_AESS_CMEM_DL1_COEFS_ID	59
#define OMAP_AESS_CMEM_DL2_L_COEFS_ID	60
#define OMAP_AESS_CMEM_DL2_R_COEFS_ID	61
#define OMAP_AESS_CMEM_SDT_COEFS_ID	62
#define OMAP_AESS_CMEM_96_48_AMIC_COEFS_ID	63
#define OMAP_AESS_CMEM_96_48_DMIC_COEFS_ID	64
#define OMAP_AESS_CMEM_1_ALPHA_ID	65
#define OMAP_AESS_CMEM_ALPHA_ID	66
#define OMAP_AESS_DMEM_SLOT23_CTRL_ID	67
#define OMAP_AESS_DMEM_AUPLINKROUTING_ID	68
#define OMAP_AESS_DMEM_MAXTASKBYTESINSLOT_ID	69
#define OMAP_AESS_DMEM_PINGPONGDESC_ID	70
#define OMAP_AESS_DMEM_IODESCR_ID	71
#define OMAP_AESS_DMEM_MCUIRQFIFO_ID	72
#define OMAP_AESS_DMEM_PING_ID	73
#define OMAP_AESS_DMEM_DEBUG_FIFO_ID	74
#define OMAP_AESS_DMEM_DEBUG_FIFO_HAL_ID	75
#define OMAP_AESS_DMEM_DEBUG_HAL_TASK_ID	76
#define OMAP_AESS_DMEM_LOOPCOUNTER_ID	77

#define OMAP_AESS_CMEM__ID	7



extern struct omap_aess_addr omap_aess_map[];

/* Distinction between Read and Write from/to ABE memory
 * is useful for simulation tool */
static inline void omap_abe_mem_write(struct omap_aess *abe, int bank,
				u32 offset, u32 *src, size_t bytes)
{
	memcpy((abe->io_base[bank] + offset), src, bytes);
}

static inline void omap_abe_mem_read(struct omap_aess *abe, int bank,
				u32 offset, u32 *dest, size_t bytes)
{
	memcpy(dest, (abe->io_base[bank] + offset), bytes);
}

static inline u32 omap_aess_reg_readl(struct omap_aess *abe, u32 offset)
{
	return __raw_readl(abe->io_base[OMAP_ABE_AESS] + offset);
}

static inline void omap_aess_reg_writel(struct omap_aess *abe,
				u32 offset, u32 val)
{
	__raw_writel(val, (abe->io_base[OMAP_ABE_AESS] + offset));
}

static inline void *omap_abe_reset_mem(struct omap_aess *abe, int bank,
			u32 offset, size_t bytes)
{
	return memset(abe->io_base[bank] + offset, 0, bytes);
}

static inline void omap_aess_mem_write(struct omap_aess *abe,
			struct omap_aess_addr addr, u32 *src)
{
	memcpy((abe->io_base[addr.bank] + addr.offset), src, addr.bytes);
}

static inline void omap_aess_mem_read(struct omap_aess *abe,
				struct omap_aess_addr addr, u32 *dest)
{
	memcpy(dest, (abe->io_base[addr.bank] + addr.offset), addr.bytes);
}

static inline void *omap_aess_reset_mem(struct omap_aess *abe,
			struct omap_aess_addr addr)
{
	return memset(abe->io_base[addr.bank] + addr.offset, 0, addr.bytes);
}


#endif /*_ABE_MEM_H_*/
