/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
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
 * Copyright(c) 2010-2011 Texas Instruments Incorporated,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Texas Instruments Incorporated nor the names of
 *   its contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
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

#include "abe_cm_addr.h"
#include "abe_dm_addr.h"
#include "abe_sm_addr.h"

#include "abe_initxxx_labels.h"
#include "abe_taskid.h"

#include "abe.h"
#include "abe_mem.h"
#include "abe_typedef.h"
#include "abe_private.h"

#define ABE_TASK_ID(ID) (OMAP_ABE_D_TASKSLIST_ADDR + sizeof(ABE_STask)*(ID))

struct omap_aess_mapping {
	struct omap_aess_addr *map;
	struct omap_aess_task *init_table;
	struct omap_aess_task *port;
	struct omap_aess_port *abe_port;
};


struct omap_aess_addr omap_aess_map[] = {
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_MULTIFRAME_ADDR,
		.bytes = OMAP_ABE_D_MULTIFRAME_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_DMIC_UL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_DMIC_UL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset =  OMAP_ABE_D_MCPDM_UL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MCPDM_UL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_BT_UL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_BT_UL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_MM_UL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MM_UL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_MM_UL2_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MM_UL2_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_VX_UL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_VX_UL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_MM_DL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MM_DL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_VX_DL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_VX_DL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_TONES_DL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_TONES_DL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_VIB_DL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_VIB_DL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_BT_DL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_BT_DL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset =  OMAP_ABE_D_MCPDM_DL_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MCPDM_DL_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset =  OMAP_ABE_D_MM_EXT_OUT_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MM_EXT_OUT_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset =  OMAP_ABE_D_MM_EXT_IN_FIFO_ADDR,
		.bytes = OMAP_ABE_D_MM_EXT_IN_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DMIC0_96_48_DATA_ADDR,
		.bytes = OMAP_ABE_S_DMIC0_96_48_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DMIC1_96_48_DATA_ADDR,
		.bytes = OMAP_ABE_S_DMIC1_96_48_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DMIC2_96_48_DATA_ADDR,
		.bytes = OMAP_ABE_S_DMIC2_96_48_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_AMIC_96_48_DATA_ADDR,
		.bytes = OMAP_ABE_S_AMIC_96_48_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_UL_ADDR,
		.bytes = OMAP_ABE_S_BT_UL_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_UL_8_48_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_UL_8_48_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_UL_8_48_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_UL_8_48_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_UL_16_48_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_UL_16_48_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_UL_16_48_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_UL_16_48_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_MM_UL2_ADDR,
		.bytes = OMAP_ABE_S_MM_UL2_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_MM_UL_ADDR,
		.bytes = OMAP_ABE_S_MM_UL_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_UL_ADDR,
		.bytes = OMAP_ABE_S_VX_UL_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_UL_48_8_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_UL_48_8_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_UL_48_8_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_UL_48_8_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_UL_48_16_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_UL_48_16_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_UL_48_16_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_UL_48_16_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_MM_DL_ADDR,
		.bytes = OMAP_ABE_S_MM_DL_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_MM_DL_44P1_ADDR,
		.bytes = OMAP_ABE_S_MM_DL_44P1_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_MM_DL_44P1_XK_ADDR,
		.bytes = OMAP_ABE_S_MM_DL_44P1_XK_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_DL_ADDR,
		.bytes = OMAP_ABE_S_VX_DL_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_DL_8_48_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_DL_8_48_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_DL_8_48_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_DL_8_48_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_DL_8_48_OSR_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_DL_8_48_OSR_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_DL_16_48_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_DL_16_48_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VX_DL_16_48_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_VX_DL_16_48_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_TONES_ADDR,
		.bytes = OMAP_ABE_S_TONES_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_TONES_44P1_ADDR,
		.bytes = OMAP_ABE_S_TONES_44P1_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_TONES_44P1_XK_ADDR,
		.bytes = OMAP_ABE_S_TONES_44P1_XK_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_VIBRA_ADDR,
		.bytes = OMAP_ABE_S_VIBRA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_DL_ADDR,
		.bytes = OMAP_ABE_S_BT_DL_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_DL_8_48_OSR_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_DL_8_48_OSR_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_DL_48_8_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_DL_48_8_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_DL_48_8_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_DL_48_8_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_DL_48_16_HP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_DL_48_16_HP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_BT_DL_48_16_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_BT_DL_48_16_LP_DATA_SIZE,
	},

	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DL2_M_LR_EQ_DATA_ADDR,
		.bytes = OMAP_ABE_S_DL2_M_LR_EQ_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DL1_M_EQ_DATA_ADDR,
		.bytes = OMAP_ABE_S_DL1_M_EQ_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_EARP_48_96_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_EARP_48_96_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_IHF_48_96_LP_DATA_ADDR,
		.bytes = OMAP_ABE_S_IHF_48_96_LP_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DC_HS_ADDR,
		.bytes = OMAP_ABE_S_DC_HS_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_DC_HF_ADDR,
		.bytes = OMAP_ABE_S_DC_HF_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_SDT_F_DATA_ADDR,
		.bytes = OMAP_ABE_S_SDT_F_DATA_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_GTARGET1_ADDR,
		.bytes = OMAP_ABE_S_GTARGET1_SIZE,
	},
	{
		.bank = OMAP_ABE_SMEM,
		.offset = OMAP_ABE_S_GCURRENT_ADDR,
		.bytes = OMAP_ABE_S_GCURRENT_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_DL1_COEFS_ADDR,
		.bytes = OMAP_ABE_C_DL1_COEFS_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_DL2_L_COEFS_ADDR,
		.bytes = OMAP_ABE_C_DL2_L_COEFS_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_DL2_R_COEFS_ADDR,
		.bytes = OMAP_ABE_C_DL2_R_COEFS_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_SDT_COEFS_ADDR,
		.bytes = OMAP_ABE_C_SDT_COEFS_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_96_48_AMIC_COEFS_ADDR,
		.bytes = OMAP_ABE_C_96_48_AMIC_COEFS_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_96_48_DMIC_COEFS_ADDR,
		.bytes = OMAP_ABE_C_96_48_DMIC_COEFS_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_1_ALPHA_ADDR,
		.bytes = OMAP_ABE_C_1_ALPHA_SIZE,
	},
	{
		.bank = OMAP_ABE_CMEM,
		.offset = OMAP_ABE_C_ALPHA_ADDR,
		.bytes = OMAP_ABE_C_ALPHA_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_SLOT23_CTRL_ADDR,
		.bytes = OMAP_ABE_D_SLOT23_CTRL_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_AUPLINKROUTING_ADDR,
		.bytes = OMAP_ABE_D_AUPLINKROUTING_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_MAXTASKBYTESINSLOT_ADDR,
		.bytes = 4,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_PINGPONGDESC_ADDR,
		.bytes = 4,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_IODESCR_ADDR,
		.bytes = 4,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_MCUIRQFIFO_ADDR,
		.bytes = 4,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_PING_ADDR,
		.bytes = OMAP_ABE_D_PING_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_DEBUG_FIFO_ADDR,
		.bytes = OMAP_ABE_D_DEBUG_FIFO_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_DEBUG_FIFO_HAL_ADDR,
		.bytes = OMAP_ABE_D_DEBUG_FIFO_HAL_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_DEBUG_HAL_TASK_ADDR,
		.bytes = OMAP_ABE_D_DEBUG_HAL_TASK_SIZE,
	},
	{
		.bank = OMAP_ABE_DMEM,
		.offset = OMAP_ABE_D_LOOPCOUNTER_ADDR,
		.bytes = OMAP_ABE_D_LOOPCOUNTER_SIZE,
	},


	{
		.bank = 0,
		.offset = 0,
		.bytes = 0,
	},
};

/* Default scheduling table for AESS (load after boot and OFF mode) */
static struct omap_aess_task init_table[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_8),
	},
	{
		.frame = 1,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_DL_8_48_FIR),
	},
	{
		.frame = 1,
		.slot = 6,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL2Mixer),
	},
	{
		.frame = 2,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL1Mixer),
	},
	{
		.frame = 2,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_SDTMixer),
	},
	{
		.frame = 3,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL1_GAIN),
	},
	{
		.frame = 3,
		.slot = 6,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL2_GAIN),
	},
	{
		.frame = 3,
		.slot = 7,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL2_EQ),
	},
	{
		.frame = 4,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL1_EQ),
	},
	{
		.frame = 4,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VXRECMixer),
	},
	{
		.frame = 4,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VXREC_SPLIT),
	},
	{
		.frame = 4,
		.slot = 6,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA1),
	},
	{
		.frame = 4,
		.slot = 7,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA2),
	},
	{
		.frame = 5,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_EARP_48_96_LP),
	},
	{
		.frame = 5,
		.slot = 7,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA_SPLIT),
	},
	{
		.frame = 6,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_EARP_48_96_LP),
	},
	{
		.frame = 6,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_EchoMixer),
	},
	{
		.frame = 6,
		.slot = 5,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_SPLIT),
	},
	{
		.frame = 7,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DBG_SYNC),
	},
	{
		.frame = 7,
		.slot = 5,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ECHO_REF_SPLIT),
	},
	{
		.frame = 8,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DMIC1_96_48_LP),
	},
	{
		.frame = 8,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DMIC1_SPLIT),
	},
	{
		.frame = 9,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DMIC2_96_48_LP),
	},
	{
		.frame = 9,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DMIC2_SPLIT),
	},
	{
		.frame = 9,
		.slot = 7,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_IHF_48_96_LP),
	},
	{
		.frame = 10,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DMIC3_96_48_LP),
	},
	{
		.frame = 10,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DMIC3_SPLIT),
	},
	{
		.frame = 10,
		.slot = 7,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_IHF_48_96_LP),
	},
	{
		.frame = 11,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_AMIC_96_48_LP),
	},
	{
		.frame = 11,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_AMIC_SPLIT),
	},
	{
		.frame = 11,
		.slot = 7,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VIBRA_PACK),
	},
	{
		.frame = 12,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_ROUTING),
	},
	{
		.frame = 12,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ULMixer),
	},
	{
		.frame = 12,
		.slot = 5,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_48_8),
	},
	{
		.frame = 13,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_MM_UL2_ROUTING),
	},
	{
		.frame = 13,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_SideTone),
	},
	{
		.frame = 14,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_DL_48_8_FIR_FW_COMPAT),
	},
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_8),
	},
	{
		.frame = 17,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_8_48),
	},
	{
		.frame = 21,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DEBUGTRACE_VX_ASRCs),
	},
	{
		.frame = 21,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_RIGHT_8K),
	},
	{
		.frame = 22,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DEBUG_IRQFIFO),
	},
	{
		.frame = 22,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_INIT_FW_MEMORY),
	},
	{
		.frame = 22,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_MM_EXT_IN_SPLIT),
	},
	{
		.frame = 23,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_GAIN_UPDATE),
	},
	{
		.frame = 23,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_LEFT_8K),
	},
};

struct omap_aess_init_task aess_init_table = {
	.nb_task = sizeof(init_table)/sizeof(struct omap_aess_task),
	.task = init_table,
};


struct omap_aess_task aess_dl1_mono_mixer[] = {
	{
		.frame = 2,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL1Mixer),
	},
	{
		.frame = 2,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL1Mixer_dual_mono),
	},
};

/* Structure is unused.  Code is retained for synchronization with upstream */
#if 0
struct omap_aess_init_task aess_init_dl1_mono_mixer = {
	.nb_task = sizeof(aess_dl1_mono_mixer)/sizeof(struct omap_aess_task),
	.task = aess_dl1_mono_mixer,
};
#endif

struct omap_aess_task aess_dl2_mono_mixer[] = {
	{
		.frame = 1,
		.slot = 6,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL2Mixer),
	},
	{
		.frame = 1,
		.slot = 6,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_DL2Mixer_dual_mono),
	},
};

/* Structure is unused.  Code is retained for synchronization with upstream */
#if 0
struct omap_aess_init_task aess_init_dl2_mono_mixer = {
	.nb_task = sizeof(aess_dl2_mono_mixer)/sizeof(struct omap_aess_task),
	.task = aess_dl2_mono_mixer,
};
#endif

struct omap_aess_task aess_audul_mono_mixer[] = {
	{
		.frame = 12,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ULMixer),
	},
	{
		.frame = 12,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ULMixer_dual_mono),
	},
};

/* Structure is unused.  Code is retained for synchronization with upstream */
#if 0
struct omap_aess_init_task aess_init_audul_mono_mixer = {
	.nb_task = sizeof(aess_audul_mono_mixer)/sizeof(struct omap_aess_task),
	.task = aess_audul_mono_mixer,
};
#endif

/* Generic Vx-DL 8 kHz tasks */
static struct omap_aess_task task_aess_enable_vx_dl_8k_port[] = {
	{
		.frame = 21,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_RIGHT_8K),
	},
	{
		.frame = 23,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_LEFT_8K),
	},
	{
		.frame = 1,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_DL_8_48_FIR),
	},
};

static struct omap_aess_io_task aess_port_vx_dl_8k = {
	.nb_task = sizeof(task_aess_enable_vx_dl_8k_port)/sizeof(struct omap_aess_task),
	.smem = IO_VX_DL_ASRC_labelID,
	.task = task_aess_enable_vx_dl_8k_port,
};

/* Serail port Vx-DL 8 kHz tasks - Siblink ASRC block */
static struct omap_aess_task task_aess_enable_vx_dl_8k_asrc_serial_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_8),
	},
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_8_SIB),
	},
};

static struct omap_aess_init_task aess_enable_vx_dl_8k_asrc_serial = {
	.nb_task = sizeof(task_aess_enable_vx_dl_8k_asrc_serial_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_dl_8k_asrc_serial_port,
};

/* CBPr port Vx-DL 8 kHz tasks - Only one ASRC block */
static struct omap_aess_task task_aess_enable_vx_dl_8k_asrc_cbpr_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_8),
	},
};

static struct omap_aess_init_task aess_enable_vx_dl_8k_asrc_cbpr = {
	.nb_task = sizeof(task_aess_enable_vx_dl_8k_asrc_cbpr_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_dl_8k_asrc_cbpr_port,
};

struct omap_aess_asrc_port aess_enable_vx_dl_8k_asrc = {
	.task = &aess_port_vx_dl_8k,
	.asrc = {
		.serial = &aess_enable_vx_dl_8k_asrc_serial,
		.cbpr = &aess_enable_vx_dl_8k_asrc_cbpr,
	},
};


/* Generic Vx-DL 16 kHz tasks */
static struct omap_aess_task task_aess_enable_vx_dl_16k_port[] = {
	{
		.frame = 21,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_RIGHT_16K),
	},
	{
		.frame = 23,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_LEFT_16K),
	},
	{
		.frame = 1,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_DL_16_48),
	},
};

static struct omap_aess_io_task aess_port_vx_dl_16k = {
	.nb_task = sizeof(task_aess_enable_vx_dl_16k_port)/sizeof(struct omap_aess_task),
	.smem = IO_VX_DL_ASRC_labelID,
	.task = task_aess_enable_vx_dl_16k_port,
};

static struct omap_aess_task task_aess_enable_vx_dl_16k_asrc_serial_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_16),
	},
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_16_SIB),
	},
};

static struct omap_aess_init_task aess_enable_vx_dl_16k_asrc_serial = {
	.nb_task = sizeof(task_aess_enable_vx_dl_16k_asrc_serial_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_dl_16k_asrc_serial_port,
};

static struct omap_aess_task task_aess_enable_vx_dl_16k_asrc_cbpr_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_16),
	},
};

static struct omap_aess_init_task aess_enable_vx_dl_16k_asrc_cbpr = {
	.nb_task = sizeof(task_aess_enable_vx_dl_16k_asrc_cbpr_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_dl_16k_asrc_cbpr_port,
};

struct omap_aess_asrc_port aess_enable_vx_dl_16k_asrc = {
	.task = &aess_port_vx_dl_16k,
	.asrc = {
		.serial = &aess_enable_vx_dl_16k_asrc_serial,
		.cbpr = &aess_enable_vx_dl_16k_asrc_cbpr,
	},
};

/* Generic Vx-DL 48 kHz tasks - No SRC/ASRC blocks */
static struct omap_aess_task task_aess_enable_vx_dl_48k_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = 0,
	},
	{
		.frame = 1,
		.slot = 3,
		.task = 0,
	},
};

static struct omap_aess_io_task aess_port_vx_dl_48k = {
	.nb_task = sizeof(task_aess_enable_vx_dl_48k_port)/sizeof(struct omap_aess_task),
	.smem = VX_DL_labelID,
	.task = task_aess_enable_vx_dl_48k_port,
};

struct omap_aess_asrc_port aess_enable_vx_dl_48k_asrc = {
	.task = &aess_port_vx_dl_48k,
};


/* Generic Vx-UL 8 kHz tasks */
static struct omap_aess_task task_aess_enable_vx_ul_8k_port[] = {
	{
		.frame = 21,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_RIGHT_8K),
	},
	{
		.frame = 23,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_LEFT_8K),
	},
	{
		.frame = 12,
		.slot = 5,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_48_8),
	},
};

static struct omap_aess_io_task aess_port_vx_ul_8k = {
	.nb_task = sizeof(task_aess_enable_vx_ul_8k_port)/sizeof(struct omap_aess_task),
	.smem = Voice_8k_UL_labelID,
	.task = task_aess_enable_vx_ul_8k_port,
};

/* Serail port Vx-UL 8 kHz tasks - Siblink ASRC block */
static struct omap_aess_task task_aess_enable_vx_ul_8k_asrc_serial_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_8_SIB),
	},
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_8),
	},
};

static struct omap_aess_init_task aess_enable_vx_ul_8k_asrc_serial = {
	.nb_task = sizeof(task_aess_enable_vx_ul_8k_asrc_serial_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_ul_8k_asrc_serial_port,
};

/* CBPr port Vx-UL 8 kHz tasks - Only one ASRC block */
static struct omap_aess_task task_aess_enable_vx_ul_8k_asrc_cbpr_port[] = {
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_8),
	},
};

static struct omap_aess_init_task aess_enable_vx_ul_8k_asrc_cbpr = {
	.nb_task = sizeof(task_aess_enable_vx_ul_8k_asrc_cbpr_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_ul_8k_asrc_cbpr_port,
};

struct omap_aess_asrc_port aess_enable_vx_ul_8k_asrc = {
	.task = &aess_port_vx_ul_8k,
	.asrc = {
		.serial = &aess_enable_vx_ul_8k_asrc_serial,
		.cbpr = &aess_enable_vx_ul_8k_asrc_cbpr,
	},
};


/* Generic Vx-UL 16 kHz tasks */
static struct omap_aess_task task_aess_enable_vx_ul_16k_port[] = {
	{
		.frame = 21,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_RIGHT_16K),
	},
	{
		.frame = 23,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_CHECK_IIR_LEFT_16K),
	},
	{
		.frame = 12,
		.slot = 5,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_VX_UL_48_16),
	},
};

static struct omap_aess_io_task aess_port_vx_ul_16k = {
	.nb_task = sizeof(task_aess_enable_vx_ul_16k_port)/sizeof(struct omap_aess_task),
	.smem = Voice_16k_UL_labelID,
	.task = task_aess_enable_vx_ul_16k_port,
};

static struct omap_aess_task task_aess_enable_vx_ul_16k_asrc_serial_port[] = {
	{
		.frame = 0,
		.slot = 3,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_DL_16_SIB),
	},
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_16),
	},
};

static struct omap_aess_init_task aess_enable_vx_ul_16k_asrc_serial = {
	.nb_task = sizeof(task_aess_enable_vx_ul_16k_asrc_serial_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_ul_16k_asrc_serial_port,
};

static struct omap_aess_task task_aess_enable_vx_ul_16k_asrc_cbpr_port[] = {
	{
		.frame = 16,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_ASRC_VX_UL_16),
	},
};

static struct omap_aess_init_task aess_enable_vx_ul_16k_asrc_cbpr = {
	.nb_task = sizeof(task_aess_enable_vx_ul_16k_asrc_cbpr_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_vx_ul_16k_asrc_cbpr_port,
};

struct omap_aess_asrc_port aess_enable_vx_ul_16k_asrc = {
	.task = &aess_port_vx_ul_16k,
	.asrc = {
		.serial = &aess_enable_vx_ul_16k_asrc_serial,
		.cbpr = &aess_enable_vx_ul_16k_asrc_cbpr,
	},
};

/* Generic Vx-UL 48 kHz tasks - No SRC/ASRC blocks */
static struct omap_aess_task task_aess_enable_vx_ul_48k_port[] = {
	{
		.frame = 16,
		.slot = 2,
		.task = 0,
	},
	{
		.frame = 12,
		.slot = 5,
		.task = 0,
	},
};

static struct omap_aess_io_task aess_port_vx_ul_48k = {
	.nb_task = sizeof(task_aess_enable_vx_ul_48k_port)/sizeof(struct omap_aess_task),
	.smem = VX_UL_M_labelID,
	.task = task_aess_enable_vx_ul_48k_port,
};

struct omap_aess_asrc_port aess_enable_vx_ul_48k_asrc = {
	.task = &aess_port_vx_ul_48k,
};


/* Generic BT-Vx-DL 8 kHz tasks */
static struct omap_aess_task task_aess_enable_bt_vx_dl_8k_port[] = {
	{
		.frame = 14,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_DL_48_8_FIR_FW_COMPAT),
	},
};

struct omap_aess_io_task aess_port_bt_vx_dl_8k = {
	.nb_task = sizeof(task_aess_enable_bt_vx_dl_8k_port)/sizeof(struct omap_aess_task),
	.smem = BT_DL_8k_labelID,
	.task = task_aess_enable_bt_vx_dl_8k_port,
};

/* Generic BT-Vx-DL 16 kHz tasks */
static struct omap_aess_task task_aess_enable_bt_vx_dl_16k_port[] = {
	{
		.frame = 14,
		.slot = 4,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_DL_48_16),
	},
};

struct omap_aess_io_task aess_port_bt_vx_dl_16k = {
	.nb_task = sizeof(task_aess_enable_bt_vx_dl_16k_port)/sizeof(struct omap_aess_task),
	.smem = BT_DL_16k_labelID,
	.task = task_aess_enable_bt_vx_dl_16k_port,
};

/* Generic BT-Vx-DL 48 kHz tasks */
static struct omap_aess_task task_aess_enable_bt_vx_dl_48k_port[] = {
	{
		.frame = 14,
		.slot = 4,
		.task = 0,
	},
};

struct omap_aess_io_task aess_port_bt_vx_dl_48k = {
	.nb_task = sizeof(task_aess_enable_bt_vx_dl_48k_port)/sizeof(struct omap_aess_task),
	.smem = DL1_GAIN_out_labelID,
	.task = task_aess_enable_bt_vx_dl_48k_port,
};

/* Generic BT-Vx-UL 8 kHz tasks */
static struct omap_aess_task task_aess_enable_bt_vx_ul_8k_port[] = {
	{
		.frame = 17,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_8_48),
	},
};

struct omap_aess_io_task aess_port_bt_vx_ul_8k = {
	.nb_task = sizeof(task_aess_enable_bt_vx_ul_8k_port)/sizeof(struct omap_aess_task),
	.smem = BT_UL_8k_labelID,
	.task = task_aess_enable_bt_vx_ul_8k_port,
};

/* Generic BT-Vx-UL 16 kHz tasks */
static struct omap_aess_task task_aess_enable_bt_vx_ul_16k_port[] = {
	{
		.frame = 17,
		.slot = 2,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_BT_UL_16_48),
	},
};

struct omap_aess_io_task aess_port_bt_vx_ul_16k = {
	.nb_task = sizeof(task_aess_enable_bt_vx_ul_16k_port)/sizeof(struct omap_aess_task),
	.smem = BT_DL_16k_labelID,
	.task = task_aess_enable_bt_vx_ul_16k_port,
};

/* Generic BT-Vx-UL 48 kHz tasks */
static struct omap_aess_task task_aess_enable_bt_vx_ul_48k_port[] = {
	{
		.frame = 15,
		.slot = 6,
		.task = 0,
	},
	{
		.frame = 17,
		.slot = 2,
		.task = 0,
	},
};

struct omap_aess_io_task aess_port_bt_vx_ul_48k = {
	.nb_task = sizeof(task_aess_enable_bt_vx_ul_48k_port)/sizeof(struct omap_aess_task),
	.smem = BT_UL_labelID,
	.task = task_aess_enable_bt_vx_ul_48k_port,
};

/* Generic TONES-DL 48 kHz tasks */
static struct omap_aess_task task_aess_enable_tones_dl_48k_port[] = {
	{
		.frame = 20,
		.slot = 1,
		.task = 0,
	},
};

struct omap_aess_io_task aess_port_tones_dl_48k = {
	.nb_task = sizeof(task_aess_enable_tones_dl_48k_port)/sizeof(struct omap_aess_task),
	.smem = Tones_labelID,
	.task = task_aess_enable_tones_dl_48k_port,
};

/* Generic TONES-DL 44.1 kHz tasks */
static struct omap_aess_task task_aess_enable_tones_dl_441k_port[] = {
	{
		.frame = 20,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_TONES_DL),
	},
	{
		.frame = 20,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_SRC44P1_TONES_1211),
	},
};

struct omap_aess_io_task aess_port_tones_dl_441k = {
	.nb_task = sizeof(task_aess_enable_tones_dl_441k_port)/sizeof(struct omap_aess_task),
	.smem = TONES_44P1_WPTR_labelID,
	.task = task_aess_enable_tones_dl_441k_port,
};

/* Generic MM-DL 48 kHz tasks */
static struct omap_aess_task task_aess_enable_mm_dl_48k_port[] = {
	{
		.frame = 18,
		.slot = 1,
		.task = 0,
	},
};

struct omap_aess_io_task aess_port_mm_dl_48k = {
	.nb_task = sizeof(task_aess_enable_mm_dl_48k_port)/sizeof(struct omap_aess_task),
	.smem = MM_DL_labelID,
	.task = task_aess_enable_mm_dl_48k_port,
};

/* Generic TONES-DL 44.1 kHz tasks */
static struct omap_aess_task task_aess_enable_mm_dl_441k_port[] = {
	{
		.frame = 18,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_SRC44P1_MMDL),
	},
};

struct omap_aess_io_task aess_port_mm_dl_441k = {
	.nb_task = sizeof(task_aess_enable_mm_dl_441k_port)/sizeof(struct omap_aess_task),
	.smem = MM_DL_44P1_WPTR_labelID,
	.task = task_aess_enable_mm_dl_441k_port,
};

/* Generic TONES-DL 44.1 kHz tasks */
static struct omap_aess_task task_aess_enable_mm_dl_441k_pp_port[] = {
	{
		.frame = 18,
		.slot = 1,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_SRC44P1_MMDL_PP),
	},
};

struct omap_aess_io_task aess_port_mm_dl_441k_pp = {
	.nb_task = sizeof(task_aess_enable_mm_dl_441k_pp_port)/sizeof(struct omap_aess_task),
	.smem = MM_DL_44P1_WPTR_labelID,
	.task = task_aess_enable_mm_dl_441k_pp_port,
};

static struct omap_aess_task task_aess_enable_mm_dl_48k_pp_port[] = {
	{
		.frame = 18,
		.slot = 0,
		.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_PING_PONG),
	},
};

struct omap_aess_init_task aess_port_mm_dl_48k_pp = {
	.nb_task = sizeof(task_aess_enable_mm_dl_48k_pp_port)/sizeof(struct omap_aess_task),
	.task = task_aess_enable_mm_dl_48k_pp_port,
};
