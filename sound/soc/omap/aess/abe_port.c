/*
 *
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2010-2013 Texas Instruments Incorporated,
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

#include "aess-fw.h"
#include "abe.h"
#include "abe_port.h"
#include "abe_dbg.h"
#include "abe_mem.h"
#include "abe_gain.h"
#include "abe_aess.h"

/*
 * GLOBAL DEFINITION
 */
#define ABE_ATC_DMIC_DMA_REQ 1
#define ABE_ATC_MCPDMDL_DMA_REQ 2
#define ABE_ATC_MCPDMUL_DMA_REQ 3

/* nb of samples to route */
#define NBROUTE_UL 16

/* ATC REGISTERS SIZE in bytes */
#define ABE_ATC_DESC_SIZE 512

#define ATC_SIZE 8		/* 8 bytes per descriptors */

struct omap_abe_atc_desc {
	unsigned rdpt:7;	/* first 32bits word of the descriptor */
	unsigned reserved0:1;
	unsigned cbsize:7;
	unsigned irqdest:1;
	unsigned cberr:1;
	unsigned reserved1:5;
	unsigned cbdir:1;
	unsigned nw:1;
	unsigned wrpt:7;
	unsigned reserved2:1;
	unsigned badd:12;	/* second 32bits word of the descriptor */
	unsigned iter:7;	/* iteration field overlaps 16-bit boundary */
	unsigned srcid:6;
	unsigned destid:6;
	unsigned desen:1;
};

struct omap_aess_pingpong_desc {
	/* 0: [W] asrc output used for the next ASRC call (+/- 1 / 0) */
	u16 drift_ASRC;
	/* 2: [W] asrc output used for controlling the number of
	   samples to be exchanged (+/- 1 / 0) */
	u16 drift_io;
	/* 4: DMAReq address or HOST IRQ buffer address (ATC ADDRESS) */
	u16 hw_ctrl_addr;
	/* 6: index of the copy subroutine */
	u8 copy_func_index;
	/* 7: X number of SMEM samples to move */
	u8 x_io;
	/* 8: 0 for mono data, 1 for stereo data */
	u8 data_size;
	/* 9: internal SMEM buffer INITPTR pointer index */
	u8 smem_addr;
	/* 10: data content to be loaded to "hw_ctrl_addr" */
	u8 atc_irq_data;
	/* 11: ping/pong buffer flag */
	u8 counter;
	/* 12: reseved */
	u16 dummy1;
	/* 14: reseved */
	u16 dummy2;
	/* 16 For 12/11 in case of 44.1 mode (same address as SIO desc)*/
	u16 split_addr1;
	/* 18: reseved */
	u16 dummy3;
	/* 20: current Base address of the working buffer */
	u16 workbuff_BaseAddr;
	/* 14: samples left in the working buffer */
	u16 workbuff_Samples;
	/* 16: Base address of the ping/pong buffer 0 */
	u16 nextbuff0_BaseAddr;
	/* 18: samples available in the ping/pong buffer 0 */
	u16 nextbuff0_Samples;
	/* 20: Base address of the ping/pong buffer 1 */
	u16 nextbuff1_BaseAddr;
	/* 22: samples available in the ping/pong buffer 1 */
	u16 nextbuff1_Samples;
};

/*
 * MAIN PORT SELECTION
 */
static const u32 abe_port_priority[LAST_PORT_ID - 1] = {
	OMAP_ABE_PDM_DL_PORT,
	OMAP_ABE_PDM_UL_PORT,
	OMAP_ABE_MM_EXT_OUT_PORT,
	OMAP_ABE_MM_EXT_IN_PORT,
	OMAP_ABE_DMIC_PORT,
	OMAP_ABE_MM_UL_PORT,
	OMAP_ABE_MM_UL2_PORT,
	OMAP_ABE_MM_DL_PORT,
	OMAP_ABE_TONES_DL_PORT,
	OMAP_ABE_BT_VX_DL_PORT,
	OMAP_ABE_BT_VX_UL_PORT,
	OMAP_ABE_VX_UL_PORT,
	OMAP_ABE_VX_DL_PORT,
	OMAP_ABE_MCASP_DL_PORT,
};

/*
 * AESS/ATC destination and source address translation (except McASPs)
 * from the original 64bits words address
 */
static const u32 abe_atc_dstid[ABE_ATC_DESC_SIZE >> 3] = {
	/* DMA_0 DMIC PDM_DL PDM_UL McB1TX McB1RX McB2TX McB2RX 0 .. 7 */
	0, 0, 12, 0, 1, 0, 2, 0,
	/* McB3TX McB3RX SLIMT0 SLIMT1 SLIMT2 SLIMT3 SLIMT4 SLIMT5 8 .. 15 */
	3, 0, 4, 5, 6, 7, 8, 9,
	/* SLIMT6 SLIMT7 SLIMR0 SLIMR1 SLIMR2 SLIMR3 SLIMR4 SLIMR5 16 .. 23 */
	10, 11, 0, 0, 0, 0, 0, 0,
	/* SLIMR6 SLIMR7 McASP1X ----- ----- McASP1R ----- ----- 24 .. 31 */
	0, 0, 14, 0, 0, 0, 0, 0,
	/* CBPrT0 CBPrT1 CBPrT2 CBPrT3 CBPrT4 CBPrT5 CBPrT6 CBPrT7 32 .. 39 */
	63, 63, 63, 63, 63, 63, 63, 63,
	/* CBP_T0 CBP_T1 CBP_T2 CBP_T3 CBP_T4 CBP_T5 CBP_T6 CBP_T7 40 .. 47 */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* CBP_T8 CBP_T9 CBP_T10 CBP_T11 CBP_T12 CBP_T13 CBP_T14
	   CBP_T15 48 .. 63 */
	0, 0, 0, 0, 0, 0, 0, 0,
};

static const u32 abe_atc_srcid[ABE_ATC_DESC_SIZE >> 3] = {
	/* DMA_0 DMIC PDM_DL PDM_UL McB1TX McB1RX McB2TX McB2RX 0 .. 7 */
	0, 12, 0, 13, 0, 1, 0, 2,
	/* McB3TX McB3RX SLIMT0 SLIMT1 SLIMT2 SLIMT3 SLIMT4 SLIMT5 8 .. 15 */
	0, 3, 0, 0, 0, 0, 0, 0,
	/* SLIMT6 SLIMT7 SLIMR0 SLIMR1 SLIMR2 SLIMR3 SLIMR4 SLIMR5 16 .. 23 */
	0, 0, 4, 5, 6, 7, 8, 9,
	/* SLIMR6 SLIMR7 McASP1X ----- ----- McASP1R ----- ----- 24 .. 31 */
	10, 11, 0, 0, 0, 14, 0, 0,
	/* CBPrT0 CBPrT1 CBPrT2 CBPrT3 CBPrT4 CBPrT5 CBPrT6 CBPrT7 32 .. 39 */
	63, 63, 63, 63, 63, 63, 63, 63,
	/* CBP_T0 CBP_T1 CBP_T2 CBP_T3 CBP_T4 CBP_T5 CBP_T6 CBP_T7 40 .. 47 */
	0, 0, 0, 0, 0, 0, 0, 0,
	/* CBP_T8 CBP_T9 CBP_T10 CBP_T11 CBP_T12 CBP_T13 CBP_T14
	   CBP_T15 48 .. 63 */
	0, 0, 0, 0, 0, 0, 0, 0,
};

static struct omap_aess_port abe_port[LAST_PORT_ID];	/* list of ABE ports */

static u32 abe_dma_port_iter_factor(struct omap_aess_data_format *f);
static u32 abe_dma_port_iteration(struct omap_aess_data_format *f);
void omap_aess_decide_main_port(struct omap_aess *aess);
int omap_aess_init_io_tasks(struct omap_aess *aess, u32 id,
			     struct omap_aess_data_format *format,
			     struct omap_aess_port_protocol *prot);
void abe_init_dma_t(u32 id, struct omap_aess_port_protocol *prot);

extern void omap_aess_init_asrc_vx_dl(struct omap_aess *aess, s32 dppm);
extern void omap_aess_init_asrc_vx_ul(struct omap_aess *aess, s32 dppm);


/**
 * omap_aess_reset_port
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 *
 * Stop the port activity and reload default parameters on the associated
 * processing features.
 * Clears the internal AE buffers.
 */
int omap_aess_reset_port(struct omap_aess *aess, u32 id)
{
	struct omap_aess_port *port = aess->fw_info->port;

	memcpy(&abe_port[id], &port[id], sizeof(struct omap_aess_port));

	return 0;
}


static void omap_aess_update_scheduling_table(struct omap_aess *aess,
					      struct omap_aess_init_task *init_task,
					      int enable)
{
	int i;
	struct omap_aess_task *task;

	for (i = 0; i < init_task->nb_task; i++) {
		task = &init_task->task[i];
		if (enable)
			aess->MultiFrame[task->frame][task->slot] = task->task;
		else
			aess->MultiFrame[task->frame][task->slot] = 0;
	}
}

static u32 omap_aess_update_io_task(struct omap_aess *aess,
				    struct omap_aess_io_task *io_task,
				    int enable)
{
	int i;
	struct omap_aess_task *task;

	for (i = 0; i < io_task->nb_task; i++) {
		task = &io_task->task[i];
		if (enable)
			aess->MultiFrame[task->frame][task->slot] = task->task;
		else
			aess->MultiFrame[task->frame][task->slot] = 0;
	}

	return io_task->smem;
}

static u32 omap_aess_update_io_task1(struct omap_aess *aess,
				     struct omap_aess_io_task1 *io_task,
				     int enable)
{
	int i;
	struct omap_aess_task *task;

	for (i = 0; i < io_task->nb_task; i++) {
		task = &io_task->task[i];
		if (enable)
			aess->MultiFrame[task->frame][task->slot] = task->task;
		else
			aess->MultiFrame[task->frame][task->slot] = 0;
	}

	return io_task->smem;
}

/**
 * omap_aess_build_scheduler_table
 * @aess: Pointer on aess handle
 *
 * Initialize Audio Engine scheduling table for ABE internal
 * processing. The content of the scheduling table is provided
 * by the firmware header. It can be changed according to the
 * ABE graph.
 */
void omap_aess_build_scheduler_table(struct omap_aess *aess)
{
	struct omap_aess_task *task;
	u16 aUplinkMuxing[NBROUTE_UL];
	int i, n;

	/* Initialize default scheduling table */
	memset(aess->MultiFrame, 0, sizeof(aess->MultiFrame));

	for (i = 0; i < aess->fw_info->nb_init_task; i++) {
		task = &aess->fw_info->init_table[i];
		aess->MultiFrame[task->frame][task->slot] = task->task;
	}

	omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_DMEM_MULTIFRAME_ID],
			    (u32 *)aess->MultiFrame);

	/* reset the uplink router */
	n = aess->fw_info->map[OMAP_AESS_DMEM_AUPLINKROUTING_ID].bytes >> 1;
	for (i = 0; i < n; i++)
		aUplinkMuxing[i] = aess->fw_info->label_id[OMAP_AESS_BUFFER_ZERO_ID];

	omap_aess_mem_write(aess,
			    aess->fw_info->map[OMAP_AESS_DMEM_AUPLINKROUTING_ID],
			    (u32 *)aUplinkMuxing);
}

/**
 * abe_dma_port_copy_subroutine_id
 * @aess: Pointer on aess handle
 * @port_id: ABE port ID
 *
 * returns the index of the function doing the copy in I/O tasks
 */
static u32 abe_dma_port_copy_subroutine_id(struct omap_aess *aess, u32 port_id)
{
	u32 sub_id;
	if (abe_port[port_id].protocol.direction == ABE_ATC_DIRECTION_IN) {
		switch (abe_port[port_id].format.samp_format) {
		case OMAP_AESS_FORMAT_MONO_MSB:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_D2S_MONO_MSB_ID];
			break;
		case OMAP_AESS_FORMAT_MONO_RSHIFTED_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_D2S_MONO_RSHIFTED_16_ID];
			break;
		case OMAP_AESS_FORMAT_STEREO_RSHIFTED_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_D2S_STEREO_RSHIFTED_16_ID];
			break;
		case OMAP_AESS_FORMAT_STEREO_16_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_D2S_STEREO_16_16_ID];
			break;
		case OMAP_AESS_FORMAT_MONO_16_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_D2S_MONO_16_16_ID];
			break;
		case OMAP_AESS_FORMAT_STEREO_MSB:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_D2S_STEREO_MSB_ID];
			break;
		case OMAP_AESS_FORMAT_SIX_MSB:
			if (port_id == OMAP_ABE_DMIC_PORT) {
				sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_DMIC_ID];
				break;
			}
		default:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_NULL_ID];
			break;
		}
	} else {
		switch (abe_port[port_id].format.samp_format) {
		case OMAP_AESS_FORMAT_MONO_MSB:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_S2D_MONO_MSB_ID];
			break;
		case OMAP_AESS_FORMAT_MONO_RSHIFTED_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_S2D_MONO_RSHIFTED_16_ID];
			break;
		case OMAP_AESS_FORMAT_STEREO_RSHIFTED_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_S2D_STEREO_RSHIFTED_16_ID];
			break;
		case OMAP_AESS_FORMAT_STEREO_16_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_S2D_STEREO_16_16_ID];
			break;
		case OMAP_AESS_FORMAT_MONO_16_16:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_S2D_MONO_16_16_ID];
			break;
		case OMAP_AESS_FORMAT_STEREO_MSB:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_S2D_STEREO_MSB_ID];
			break;
		case OMAP_AESS_FORMAT_SIX_MSB:
			if (port_id == OMAP_ABE_PDM_DL_PORT) {
				sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_MCPDM_DL_ID];
				break;
			}
			if (port_id == OMAP_ABE_MM_UL_PORT) {
				sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_MM_UL_ID];
				break;
			}
		case OMAP_AESS_FORMAT_THREE_MSB:
		case OMAP_AESS_FORMAT_FOUR_MSB:
		case OMAP_AESS_FORMAT_FIVE_MSB:
		case OMAP_AESS_FORMAT_SEVEN_MSB:
		case OMAP_AESS_FORMAT_EIGHT_MSB:
		case OMAP_AESS_FORMAT_NINE_MSB:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_MM_UL_ID];
			break;
		default:
			sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_NULL_ID];
			break;
		}
	}
	return sub_id;
}

/**
 * abe_clean_temporay buffers
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 *
 * clear temporary buffers according to the port ID.
 */
static void omap_aess_clean_temporary_buffers(struct omap_aess *aess, u32 id)
{
	switch (id) {
	case OMAP_ABE_DMIC_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_DMIC_UL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_DMIC0_96_48_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_DMIC1_96_48_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_DMIC2_96_48_DATA_ID]);
		/* reset working values of the gain, target gain is preserved */
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DMIC1_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DMIC1_RIGHT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DMIC2_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DMIC2_RIGHT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DMIC3_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DMIC3_RIGHT);
		break;
	case OMAP_ABE_PDM_UL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MCPDM_UL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_AMIC_96_48_DATA_ID]);
		/* reset working values of the gain, target gain is preserved */
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_AMIC_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_AMIC_RIGHT);
		break;
	case OMAP_ABE_BT_VX_UL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_BT_UL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_UL_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_UL_8_48_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_UL_8_48_LP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_UL_16_48_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_UL_16_48_LP_DATA_ID]);
		/* reset working values of the gain, target gain is preserved */
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_BTUL_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_BTUL_RIGHT);
		break;
	case OMAP_ABE_MM_UL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MM_UL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_MM_UL_ID]);
		break;
	case OMAP_ABE_MM_UL2_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MM_UL2_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_MM_UL2_ID]);
		break;
	case OMAP_ABE_VX_UL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_VX_UL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_UL_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_UL_48_8_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_UL_48_8_LP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_UL_48_16_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_UL_48_16_LP_DATA_ID]);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXAUDUL_UPLINK);
		break;
	case OMAP_ABE_MM_DL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MM_DL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_MM_DL_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_MM_DL_44P1_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_MM_DL_44P1_XK_ID]);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXDL1_MM_DL);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXDL2_MM_DL);
		break;
	case OMAP_ABE_VX_DL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_VX_DL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_DL_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_DL_8_48_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_DL_8_48_LP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_DL_8_48_OSR_LP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_DL_16_48_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_VX_DL_16_48_LP_DATA_ID]);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXDL1_VX_DL);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXDL2_VX_DL);
		break;
	case OMAP_ABE_TONES_DL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_TONES_DL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_TONES_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_TONES_44P1_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_TONES_44P1_XK_ID]);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXDL1_TONES);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXDL2_TONES);
		break;
	case OMAP_ABE_MCASP_DL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MCASP_DL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_MCASP1_ID]);
		break;
	case OMAP_ABE_BT_VX_DL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_BT_DL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_DL_ID]);
#if !defined(CONFIG_SND_OMAP4_ABE_USE_ALT_FW)
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_DL_8_48_OSR_LP_DATA_ID]);
#endif
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_DL_48_8_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_DL_48_8_LP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_DL_48_16_HP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_BT_DL_48_16_LP_DATA_ID]);
		break;
	case OMAP_ABE_PDM_DL_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MCPDM_DL_FIFO_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_DL2_M_LR_EQ_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_DL1_M_EQ_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_EARP_48_96_LP_DATA_ID]);
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_SMEM_IHF_48_96_LP_DATA_ID]);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DL1_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DL1_RIGHT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DL2_LEFT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_GAIN_DL2_RIGHT);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXSDT_UL);
		omap_aess_reset_gain_mixer(aess, OMAP_AESS_MIXSDT_DL);
		break;
	case OMAP_ABE_MM_EXT_OUT_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MM_EXT_OUT_FIFO_ID]);
		break;
	case OMAP_ABE_MM_EXT_IN_PORT:
		omap_aess_reset_mem(aess, aess->fw_info->map[OMAP_AESS_DMEM_MM_EXT_IN_FIFO_ID]);
		break;
	}
}

/**
 * omap_aess_disable_enable_dma_request
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 * @on_off: Enable/Disable
 *
 * Enable/Disable DMA request associated to a port.
 */
static void omap_aess_disable_enable_dma_request(struct omap_aess *aess, u32 id,
						 u32 on_off)
{
	u8 desc_third_word[4], irq_dmareq_field;
	struct omap_aess_io_desc sio_desc;
	struct omap_aess_pingpong_desc desc_pp;
	struct omap_aess_addr addr;

	if (abe_port[id].protocol.protocol_switch == OMAP_AESS_PORT_PINGPONG) {
		irq_dmareq_field = (u8) (on_off *
			      abe_port[id].protocol.p.prot_pingpong.irq_data);
		memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
		       sizeof(struct omap_aess_addr));
		addr.offset += (u32)&(desc_pp.data_size) - (u32)&(desc_pp);
		addr.bytes = 4;
		omap_aess_mem_read(aess, addr, (u32 *)desc_third_word);
		desc_third_word[2] = irq_dmareq_field;
		omap_aess_mem_write(aess, addr, (u32 *)desc_third_word);
	} else {
		/* serial interface: sync ATC with Firmware activity */
		memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_IODESCR_ID],
		       sizeof(struct omap_aess_addr));
		addr.offset += id * sizeof(struct omap_aess_io_desc);
		addr.bytes = sizeof(struct omap_aess_io_desc);
		omap_aess_mem_read(aess, addr, (u32 *)&sio_desc);
		if (on_off) {
			if (abe_port[id].protocol.protocol_switch != OMAP_AESS_PORT_SERIAL)
				sio_desc.atc_irq_data =
					(u8) abe_port[id].protocol.p.prot_dmareq.
					dma_data;
			sio_desc.on_off = 0x80;
		} else {
			sio_desc.atc_irq_data = 0;
			sio_desc.on_off = 0;
		}
		omap_aess_mem_write(aess, addr, (u32 *)&sio_desc);
	}

}

/**
 * omap_aess_enable_dma_request
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 *
 * Enable DMA request associated to the port ID
 */
static void omap_aess_enable_dma_request(struct omap_aess *aess, u32 id)
{
	omap_aess_disable_enable_dma_request(aess, id, 1);
}

/**
 * omap_aess_disable_dma_request
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 *
 * Disable DMA request associated to the port ID
 */
static void omap_aess_disable_dma_request(struct omap_aess *aess, u32 id)
{
	omap_aess_disable_enable_dma_request(aess, id, 0);
}

/**
 * omap_aess_init_atc
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 *
 * load the DMEM ATC/AESS descriptor associated to the port ID.
 * ATC is describing the internal flexible FIFO inside the DMEM
 * connected to HW IP (eg McBSP/DMIC/...)
 */
static void omap_aess_init_atc(struct omap_aess *aess, u32 id)
{
	u8 iter;
	s32 datasize;
	struct omap_abe_atc_desc atc_desc;

#define JITTER_MARGIN 4
	/* load default values of the descriptor */
	memset(&atc_desc, 0, sizeof(struct omap_abe_atc_desc));

	datasize = abe_dma_port_iter_factor(&((abe_port[id]).format));
	iter = (u8) abe_dma_port_iteration(&((abe_port[id]).format));
	/* if the ATC FIFO is too small there will be two ABE firmware
	   utasks to do the copy this happems on DMIC and MCPDMDL */
	/* VXDL_8kMono = 4 = 2 + 2x1 */
	/* VXDL_16kstereo = 12 = 8 + 2x2 */
	/* MM_DL_1616 = 14 = 12 + 2x1 */
	/* DMIC = 84 = 72 + 2x6 */
	/* VXUL_8kMono = 2 */
	/* VXUL_16kstereo = 4 */
	/* MM_UL2_Stereo = 4 */
	/* PDMDL = 12 */
	/* IN from AESS point of view */
	if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
		if (iter + 2 * datasize > 126)
			atc_desc.wrpt = (iter >> 1) +
				((JITTER_MARGIN-1) * datasize);
		else
			atc_desc.wrpt = iter + ((JITTER_MARGIN-1) * datasize);
	else
		atc_desc.wrpt = 0 + ((JITTER_MARGIN+1) * datasize);

	switch ((abe_port[id]).protocol.protocol_switch) {
	case OMAP_AESS_PORT_SERIAL:
		atc_desc.cbdir = (abe_port[id]).protocol.direction;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_serial.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_serial.buf_addr) >> 4;
		atc_desc.iter = (abe_port[id]).protocol.p.prot_serial.iter;
		atc_desc.srcid =
			abe_atc_srcid[(abe_port[id]).protocol.p.prot_serial.
				      desc_addr >> 3];
		atc_desc.destid =
			abe_atc_dstid[(abe_port[id]).protocol.p.prot_serial.
				      desc_addr >> 3];
		omap_abe_mem_write(aess, OMAP_ABE_DMEM,
				   (abe_port[id]).protocol.p.prot_serial.desc_addr,
				   (u32 *)&atc_desc, sizeof(atc_desc));
		break;
	case OMAP_AESS_PORT_DMIC:
		atc_desc.cbdir = ABE_ATC_DIRECTION_IN;
		atc_desc.cbsize = (abe_port[id]).protocol.p.prot_dmic.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_dmic.buf_addr) >> 4;
		atc_desc.iter = DMIC_ITER;
		atc_desc.srcid = abe_atc_srcid[ABE_ATC_DMIC_DMA_REQ];
		omap_abe_mem_write(aess, OMAP_ABE_DMEM,
				   (ABE_ATC_DMIC_DMA_REQ*ATC_SIZE),
				   (u32 *)&atc_desc, sizeof(atc_desc));
		break;
	case OMAP_AESS_PORT_MCPDMDL:
		atc_desc.cbdir = ABE_ATC_DIRECTION_OUT;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_mcpdmdl.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_mcpdmdl.buf_addr) >> 4;
		atc_desc.iter = MCPDM_DL_ITER;
		atc_desc.destid = abe_atc_dstid[ABE_ATC_MCPDMDL_DMA_REQ];
		omap_abe_mem_write(aess, OMAP_ABE_DMEM,
				   (ABE_ATC_MCPDMDL_DMA_REQ*ATC_SIZE),
				   (u32 *)&atc_desc, sizeof(atc_desc));
		break;
	case OMAP_AESS_PORT_MCPDMUL:
		atc_desc.cbdir = ABE_ATC_DIRECTION_IN;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_mcpdmul.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_mcpdmul.buf_addr) >> 4;
		atc_desc.iter = MCPDM_UL_ITER;
		atc_desc.srcid = abe_atc_srcid[ABE_ATC_MCPDMUL_DMA_REQ];
		omap_abe_mem_write(aess, OMAP_ABE_DMEM,
				   (ABE_ATC_MCPDMUL_DMA_REQ*ATC_SIZE),
				   (u32 *)&atc_desc, sizeof(atc_desc));
		break;
	case OMAP_AESS_PORT_PINGPONG:
		/* software protocol, nothing to do on ATC */
		break;
	case OMAP_AESS_PORT_DMAREQ:
		atc_desc.cbdir = (abe_port[id]).protocol.direction;
		atc_desc.cbsize =
			(abe_port[id]).protocol.p.prot_dmareq.buf_size;
		atc_desc.badd =
			((abe_port[id]).protocol.p.prot_dmareq.buf_addr) >> 4;
		/* CBPr needs ITER=1.
		It is the job of eDMA to do the iterations */
		atc_desc.iter = 1;
		/* input from ABE point of view */
		if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN) {
			/* atc_atc_desc.rdpt = 127; */
			/* atc_atc_desc.wrpt = 0; */
			atc_desc.srcid = abe_atc_srcid
				[(abe_port[id]).protocol.p.prot_dmareq.
				 desc_addr >> 3];
		} else {
			/* atc_atc_desc.rdpt = 0; */
			/* atc_atc_desc.wrpt = 127; */
			atc_desc.destid = abe_atc_dstid
				[(abe_port[id]).protocol.p.prot_dmareq.
				 desc_addr >> 3];
		}
		omap_abe_mem_write(aess, OMAP_ABE_DMEM,
				   (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
				   (u32 *)&atc_desc, sizeof(atc_desc));
		break;
	}
}

/**
 * omap_aess_disable_data_transfer
 * @aess: Pointer on aess handle
 * @id: ABE port id
 *
 * disables the ATC descriptor and stop IO/port activities
 * disable the IO task (@f = 0)
 * clear ATC DMEM buffer, ATC enabled
 */
int omap_aess_disable_data_transfer(struct omap_aess *aess, u32 id)
{

	switch (id) {
	case OMAP_ABE_MM_DL_PORT:
		aess->MultiFrame[18][1] = 0;
		break;
	default:
		break;
	}

	/* local host variable status= "port is running" */
	abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_IDLE;
	/* disable DMA requests */
	omap_aess_disable_dma_request(aess, id);
	/* disable ATC transfers */
	omap_aess_init_atc(aess, id);
	omap_aess_clean_temporary_buffers(aess, id);
	/* select the main port based on the desactivation of this port */
	omap_aess_decide_main_port(aess);

	return 0;
}
EXPORT_SYMBOL(omap_aess_disable_data_transfer);

/**
 * omap_aess_enable_data_transfer
 * @aess: Pointer on aess handle
 * @id: ABE port id
 *
 * enables the ATC descriptor
 * reset ATC pointers
 * enable the IO task (@f <> 0)
 */
int omap_aess_enable_data_transfer(struct omap_aess *aess, u32 id)
{
	struct omap_aess_port_protocol *protocol;
	struct omap_aess_data_format format;

	omap_aess_clean_temporary_buffers(aess, id);

	omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].task), 1);

	switch (id) {
	case OMAP_ABE_PDM_UL_PORT:
	case OMAP_ABE_PDM_DL_PORT:
	case OMAP_ABE_DMIC_PORT:
		/* initializes the ABE ATC descriptors in DMEM for BE ports */
		protocol = &(abe_port[id].protocol);
		format = abe_port[id].format;
		omap_aess_init_atc(aess, id);
		omap_aess_init_io_tasks(aess, id, &format, protocol);
		break;

	case OMAP_ABE_MM_DL_PORT:
		protocol = &(abe_port[OMAP_ABE_MM_DL_PORT].protocol);
		if (protocol->protocol_switch == OMAP_AESS_PORT_PINGPONG)
			omap_aess_update_scheduling_table(aess, &aess->fw_info->ping_pong->task, 1);
		break;
	default:
		break;
	}

	omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_DMEM_MULTIFRAME_ID],
			    (u32 *)aess->MultiFrame);

	/* local host variable status= "port is running" */
	abe_port[id].status = OMAP_ABE_PORT_ACTIVITY_RUNNING;
	/* enable DMA requests */
	omap_aess_enable_dma_request(aess, id);
	/* select the main port based on the activation of this new port */
	omap_aess_decide_main_port(aess);

	return 0;
}
EXPORT_SYMBOL(omap_aess_enable_data_transfer);

/**
 * omap_aess_read_port_address
 * @aess: Pointer on aess handle
 * @port: port name
 * @aess_dma: output pointer to the DMA iteration and data destination pointer
 *
 * This API returns the address of the DMA register used on this audio port.
 * Depending on the protocol being used, adds the base address offset L3
 * (DMA) or MPU (ARM)
 */
static void omap_aess_read_port_address(struct omap_aess *aess, u32 port,
				 struct omap_aess_dma *aess_dma)
{
	struct omap_aess_dma_offset *dma_offset = &abe_port[port].dma;

	switch (abe_port[port].protocol.protocol_switch) {
	case OMAP_AESS_PORT_PINGPONG:
		/* return the base address of the buffer in L3 and L4 spaces */
		aess_dma->data = (void *)(dma_offset->data +
			ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_DMEM_BASE_OFFSET_MPU);
		break;
	case OMAP_AESS_PORT_DMAREQ:
		/* return the CBPr(L3), DMEM(L3), DMEM(L4) address */
		aess_dma->data = (void *)(dma_offset->data +
			ABE_DEFAULT_BASE_ADDRESS_L3 + ABE_ATC_BASE_OFFSET_MPU);
		break;
	default:
		break;
	}
	aess_dma->iter = dma_offset->iter;
}

/**
 * omap_aess_connect_cbpr_dmareq_port
 * @aess: Pointer on aess handle
 * @id: port name
 * @f: desired data format
 * @d: desired dma_request line (0..7)
 * @aess_dma: returned pointer to the base address of the CBPr register and
 * 	number of samples to exchange during a DMA_request.
 *
 * enables the data echange between a DMA and the ABE through the
 *	CBPr registers of AESS.
 */
void omap_aess_connect_cbpr_dmareq_port(struct omap_aess *aess, u32 id,
					struct omap_aess_data_format *f, u32 d,
					struct omap_aess_dma *aess_dma)
{
	omap_aess_reset_port(aess, id);

	memcpy(&abe_port[id].format, f, sizeof(*f));

	abe_port[id].protocol.protocol_switch = OMAP_AESS_PORT_DMAREQ;
	abe_port[id].protocol.p.prot_dmareq.iter = abe_dma_port_iteration(f);
	abe_port[id].protocol.p.prot_dmareq.dma_addr = OMAP_AESS_DMASTATUS_RAW;
	abe_port[id].protocol.p.prot_dmareq.dma_data = (1 << d);

	/* load the dma_t with physical information from AE memory mapping */
	abe_init_dma_t(id, &abe_port[id].protocol);

	/* load the ATC descriptors - disabled */
	omap_aess_init_atc(aess, id);

	/* load the micro-task parameters */
	omap_aess_init_io_tasks(aess,  id, &abe_port[id].format,
				&abe_port[id].protocol);
	abe_port[id].status = OMAP_ABE_PORT_INITIALIZED;

	if (aess_dma)
		omap_aess_read_port_address(aess, id, aess_dma);
}
EXPORT_SYMBOL(omap_aess_connect_cbpr_dmareq_port);

/**
 * omap_aess_connect_serial_port()
 * @aess: Pointer on aess handle
 * @id: port name
 * @f: data format
 * @mcbsp_id: peripheral ID (McBSP #1, #2, #3)
 * @aess_dma: returned pointer to the base address of the CBPr register and
 * 	number of samples to exchange during a DMA_request.
 *
 * Operations : enables the data echanges between a McBSP and an ATC buffer in
 * DMEM. This API is used connect 48kHz McBSP streams to MM_DL and 8/16kHz
 * voice streams to VX_UL, VX_DL, BT_VX_UL, BT_VX_DL. It abstracts the
 * abe_write_port API.
 */
void omap_aess_connect_serial_port(struct omap_aess *aess, u32 id,
				   struct omap_aess_data_format *f,
				   u32 mcbsp_id, struct omap_aess_dma *aess_dma)
{
	omap_aess_reset_port(aess, id);

	memcpy(&abe_port[id].format, f, sizeof(*f));

	abe_port[id].protocol.protocol_switch = OMAP_AESS_PORT_SERIAL;
	/* McBSP peripheral connected to ATC */
	abe_port[id].protocol.p.prot_serial.desc_addr = mcbsp_id * ATC_SIZE;
	/* check the iteration of ATC */
	abe_port[id].protocol.p.prot_serial.iter = abe_dma_port_iter_factor(f);

	/* load the ATC descriptors - disabled */
	omap_aess_init_atc(aess, id);
	/* load the micro-task parameters */
	omap_aess_init_io_tasks(aess,  id, &abe_port[id].format,
				&abe_port[id].protocol);
	abe_port[id].status = OMAP_ABE_PORT_INITIALIZED;

	if (aess_dma)
		omap_aess_read_port_address(aess, id, aess_dma);
}
EXPORT_SYMBOL(omap_aess_connect_serial_port);

/**
 * abe_init_dma_t
 * @id: ABE port ID
 * @prot: protocol being used
 *
 * load the dma_t with physical information from AE memory mapping
 */
void abe_init_dma_t(u32 id, struct omap_aess_port_protocol *prot)
{
	struct omap_aess_dma_offset dma;
	u32 idx;
	/* default dma_t points to address 0000... */
	dma.data = 0;
	dma.iter = 0;
	switch (prot->protocol_switch) {
	case OMAP_AESS_PORT_PINGPONG:
		for (idx = 0; idx < 32; idx++) {
			if (((prot->p).prot_pingpong.irq_data) ==
			    (u32) (1 << idx))
				break;
		}
		(prot->p).prot_dmareq.desc_addr =
			((CBPr_DMA_RTX0 + idx)*ATC_SIZE);
		/* translate byte address/size in DMEM words */
		dma.data = (prot->p).prot_pingpong.buf_addr >> 2;
		dma.iter = (prot->p).prot_pingpong.buf_size >> 2;
		break;
	case OMAP_AESS_PORT_DMAREQ:
		for (idx = 0; idx < 32; idx++) {
			if (((prot->p).prot_dmareq.dma_data) ==
			    (u32) (1 << idx))
				break;
		}
		dma.data = (CIRCULAR_BUFFER_PERIPHERAL_R__0 + (idx << 2));
		dma.iter = (prot->p).prot_dmareq.iter;
		(prot->p).prot_dmareq.desc_addr =
			((CBPr_DMA_RTX0 + idx)*ATC_SIZE);
		break;
	case OMAP_AESS_PORT_SERIAL:
	case OMAP_AESS_PORT_DMIC:
	case OMAP_AESS_PORT_MCPDMDL:
	case OMAP_AESS_PORT_MCPDMUL:
	default:
		break;
	}
	/* upload the dma type */
	abe_port[id].dma = dma;
}

/**
 * omap_aess_enable_atc
 * @aess: Pointer on aess handle
 * @id: port name
 *
 * Enable ATC associated to the port ID
 */
static void omap_aess_enable_atc(struct omap_aess *aess, u32 id)
{
	struct omap_abe_atc_desc atc_desc;

	omap_abe_mem_read(aess, OMAP_ABE_DMEM,
			  (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
			  (u32 *)&atc_desc, sizeof(atc_desc));
	atc_desc.desen = 1;
	omap_abe_mem_write(aess, OMAP_ABE_DMEM,
			   (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
			   (u32 *)&atc_desc, sizeof(atc_desc));

}

/**
 * omap_aess_disable_atc
 * @aess: Pointer on aess handle
 * @id: port name
 *
 * Enable ATC associated to the port ID
 */
static void omap_aess_disable_atc(struct omap_aess *aess, u32 id)
{
	struct omap_abe_atc_desc atc_desc;

	omap_abe_mem_read(aess, OMAP_ABE_DMEM,
			  (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
			  (u32 *)&atc_desc, sizeof(atc_desc));
	atc_desc.desen = 0;
	omap_abe_mem_write(aess, OMAP_ABE_DMEM,
			   (abe_port[id]).protocol.p.prot_dmareq.desc_addr,
			   (u32 *)&atc_desc, sizeof(atc_desc));

}

/**
 * omap_aess_init_io_tasks
 * @aess: Pointer on aess handle
 * @id: port name
 * @format: data format being used
 * @prot: protocol being used
 *
 * load the micro-task parameters doing to DMEM <==> SMEM data moves
 *
 * I/O descriptors input parameters :
 * For Read from DMEM usually THR1/THR2 = X+1/X-1
 * For Write to DMEM usually THR1/THR2 = 2/0
 * UP_1/2 =X+1/X-1
 */
int omap_aess_init_io_tasks(struct omap_aess *aess, u32 id,
			    struct omap_aess_data_format *format,
			    struct omap_aess_port_protocol *prot)
{
	u32 x_io, direction, iter_samples, smem1, smem2, smem3, io_sub_id,
		io_flag;
	u32 copy_func_index, before_func_index, after_func_index;
	u32 dmareq_addr, dmareq_field;
	u32 datasize, iter, nsamp, datasize2;
	u32 atc_ptr_saved, atc_ptr_saved2, copy_func_index1;
	u32 copy_func_index2, atc_desc_address1, atc_desc_address2;
	struct omap_aess_addr addr;

	if (prot->protocol_switch == OMAP_AESS_PORT_PINGPONG) {
		struct omap_aess_pingpong_desc desc_pp;

		memset(&desc_pp, 0, sizeof(desc_pp));

		/* ping_pong is only supported on MM_DL */
		if (OMAP_ABE_MM_DL_PORT != id) {
			aess_err("Only Ping-pong port supported");
			return -AESS_EINVAL;
		}
		if (abe_port[id].format.f == 44100)
			smem1 = omap_aess_update_io_task1(aess, &(aess->fw_info->ping_pong->tsk_freq[2].task), 1);
		else
			smem1 = omap_aess_update_io_task1(aess, &(aess->fw_info->ping_pong->tsk_freq[3].task), 1);

		/* able  interrupt to be generated at the first frame */
		desc_pp.split_addr1 = 1;

		copy_func_index = (u8) abe_dma_port_copy_subroutine_id(aess, id);
		dmareq_addr = abe_port[id].protocol.p.prot_pingpong.irq_addr;
		dmareq_field = abe_port[id].protocol.p.prot_pingpong.irq_data;
		datasize = abe_dma_port_iter_factor(format);
		/* number of "samples" either mono or stereo */
		iter = abe_dma_port_iteration(format);
		iter_samples = (iter / datasize);

		/* load the IO descriptor */
		desc_pp.hw_ctrl_addr = (u16) dmareq_addr;
		desc_pp.copy_func_index = (u8) copy_func_index;
		desc_pp.smem_addr = (u8) smem1;
		/* DMA req 0 is used for CBPr0 */
		desc_pp.atc_irq_data = (u8) dmareq_field;
		/* size of block transfer */
		desc_pp.x_io = (u8) iter_samples;
		desc_pp.data_size = (u8) datasize;
		/* address comunicated in Bytes */
		desc_pp.workbuff_BaseAddr =
			(u16) (aess->base_address_pingpong[1]);

		/* size comunicated in XIO sample */
		desc_pp.workbuff_Samples = 0;
		desc_pp.nextbuff0_BaseAddr =
			(u16) (aess->base_address_pingpong[0]);
		desc_pp.nextbuff1_BaseAddr =
			(u16) (aess->base_address_pingpong[1]);
		if (dmareq_addr == OMAP_AESS_DMASTATUS_RAW) {
			desc_pp.nextbuff0_Samples =
				(u16) ((aess->size_pingpong >> 2) / datasize);
			desc_pp.nextbuff1_Samples =
				(u16) ((aess->size_pingpong >> 2) / datasize);
		} else {
			desc_pp.nextbuff0_Samples = 0;
			desc_pp.nextbuff1_Samples = 0;
		}
		/* next buffer to send is B1, first IRQ fills B0 */
		desc_pp.counter = 0;
		/* send a DMA req to fill B0 with N samples
		   abe_block_copy (COPY_FROM_HOST_TO_ABE,
			ABE_ATC,
			OMAP_AESS_DMASTATUS_RAW,
			&(abe_port[id].protocol.p.prot_pingpong.irq_data),
			4); */
		memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
		       sizeof(struct omap_aess_addr));
		addr.bytes = sizeof(desc_pp);
		omap_aess_mem_write(aess, addr, (u32 *)&desc_pp);
	} else {
		struct omap_aess_io_desc sio_desc;
		int idx;

		switch (abe_port[id].format.f) {
		case 8000:
		default:
			idx = 0;
			break;
		case 16000:
			idx = 1;
			break;
		case 44100:
			idx = 2;
			break;
		case 48000:
			idx = 3;
			break;
		}

		memset(&sio_desc, 0, sizeof(sio_desc));

		io_sub_id = OMAP_AESS_DMASTATUS_RAW;
		dmareq_addr = OMAP_AESS_DMASTATUS_RAW;
		dmareq_field = 0;
		atc_desc_address1 = 0;
		atc_desc_address2 = 0;
		/* default: repeat of the last downlink samples in case of
		   DMA errors, (disable=0x00) */
		io_flag = 0xFF;
		datasize2 = abe_dma_port_iter_factor(format);
		datasize = abe_dma_port_iter_factor(format);
		x_io = (u8) abe_dma_port_iteration(format);
		nsamp = (x_io / datasize);
		atc_ptr_saved2 = aess->fw_info->label_id[OMAP_AESS_BUFFER_DMIC_ATC_PTR_ID] + id;
		atc_ptr_saved = aess->fw_info->label_id[OMAP_AESS_BUFFER_DMIC_ATC_PTR_ID] + id;

		smem1 = abe_port[id].smem_buffer1;
		smem2 = abe_port[id].smem_buffer2;
		smem3 = abe_port[id].smem_buffer2;
		copy_func_index1 = (u8) abe_dma_port_copy_subroutine_id(aess, id);

		before_func_index = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_NULL_ID];
		after_func_index = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_NULL_ID];
		copy_func_index2 = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_NULL_ID];

		switch (prot->protocol_switch) {
		case OMAP_AESS_PORT_DMIC:
			/* DMIC port is read in two steps */
			x_io = x_io >> 1;
			nsamp = nsamp >> 1;
			atc_desc_address1 = (ABE_ATC_DMIC_DMA_REQ*ATC_SIZE);
			io_sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_IO_IP_ID];
			break;
		case OMAP_AESS_PORT_MCPDMDL:
			/* PDMDL port is written to in two steps */
			x_io = x_io >> 1;
			atc_desc_address1 = (ABE_ATC_MCPDMDL_DMA_REQ*ATC_SIZE);
			io_sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_IO_IP_ID];
			break;
		case OMAP_AESS_PORT_MCPDMUL:
			atc_desc_address1 = (ABE_ATC_MCPDMUL_DMA_REQ*ATC_SIZE);
			io_sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_IO_IP_ID];
			break;
		case OMAP_AESS_PORT_SERIAL:	/* McBSP/McASP */
			atc_desc_address1 = (s16) abe_port[id].protocol.p.prot_serial.desc_addr;
			io_sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_IO_IP_ID];
			break;
		case OMAP_AESS_PORT_DMAREQ:	/* DMA w/wo CBPr */
			dmareq_addr = abe_port[id].protocol.p.prot_dmareq.dma_addr;
			dmareq_field = 0;
			atc_desc_address1 = abe_port[id].protocol.p.prot_dmareq.desc_addr;
			io_sub_id = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_IO_IP_ID];
			break;
		}
		/* special situation of the PING_PONG protocol which
		has its own SIO descriptor format */
		/*
		   Sequence of operations on ping-pong buffers B0/B1
		   -------------- time ----------------------------
		   Host Application is ready to send data from DDR to B0
		   SDMA is initialized from "abe_connect_irq_ping_pong_port" to B0
		   FIRMWARE starts with #12 B1 data,
		   sends IRQ/DMAreq, sends #pong B1 data,
		   sends IRQ/DMAreq, sends #ping B0,
		   sends B1 samples
		   ARM / SDMA | fills B0 | fills B1 ... | fills B0 ...
		   Counter 0 1 2 3
		 */
		switch (id) {
		case OMAP_ABE_VX_DL_PORT:
			omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].task), 1);

			smem1 = omap_aess_update_io_task1(aess, &(aess->fw_info->port[id].tsk_freq[idx].task), 1);
			/* check for 8kHz/16kHz */
			if (idx < 2) {
				/* ASRC set only for McBSP */
				if ((prot->protocol_switch == OMAP_AESS_PORT_SERIAL)) {
					if ((abe_port[OMAP_ABE_VX_DL_PORT].status ==
						OMAP_ABE_PORT_ACTIVITY_IDLE) &&
					    (abe_port[OMAP_ABE_VX_UL_PORT].status ==
						OMAP_ABE_PORT_ACTIVITY_IDLE)) {
						/* the 1st opened port is VX_DL_PORT
						 * both VX_UL ASRC and VX_DL ASRC will add/remove sample
						 * referring to VX_DL flow_counter */
						omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].tsk_freq[idx].asrc.serial), 1);

						/* Init VX_UL ASRC & VX_DL ASRC and enable its adaptation */
						omap_aess_init_asrc_vx_ul(aess, -250);
						omap_aess_init_asrc_vx_dl(aess, 250);
					} else {
						/* Do nothing, Scheduling Table has already been patched */
					}
				} else {
					/* Enable only ASRC on VXDL port*/
					omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].tsk_freq[idx].asrc.cbpr), 1);
					omap_aess_init_asrc_vx_dl(aess, 0);
				}
			}
			break;
		case OMAP_ABE_VX_UL_PORT:
			omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].task), 1);

			smem1 = omap_aess_update_io_task1(aess, &(aess->fw_info->port[id].tsk_freq[idx].task), 1);
			/* check for 8kHz/16kHz */
			if (idx < 2) {
				/* ASRC set only for McBSP */
				if ((prot->protocol_switch == OMAP_AESS_PORT_SERIAL)) {
					if ((abe_port[OMAP_ABE_VX_DL_PORT].status ==
						OMAP_ABE_PORT_ACTIVITY_IDLE) &&
					    (abe_port[OMAP_ABE_VX_UL_PORT].status ==
						OMAP_ABE_PORT_ACTIVITY_IDLE)) {
						/* the 1st opened port is VX_UL_PORT
						 * both VX_UL ASRC and VX_DL ASRC will add/remove sample
						 * referring to VX_UL flow_counter */
						omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].tsk_freq[idx].asrc.serial), 1);
						/* Init VX_UL ASRC & VX_DL ASRC and enable its adaptation */
						omap_aess_init_asrc_vx_ul(aess, -250);
						omap_aess_init_asrc_vx_dl(aess, 250);
					} else {
						/* Do nothing, Scheduling Table has already been patched */
					}
				} else {
					/* Enable only ASRC on VXUL port*/
					omap_aess_update_scheduling_table(aess, &(aess->fw_info->port[id].tsk_freq[idx].asrc.cbpr), 1);
					omap_aess_init_asrc_vx_ul(aess, 0);
				}
			}
			break;
		case OMAP_ABE_BT_VX_DL_PORT:
		case OMAP_ABE_BT_VX_UL_PORT:
		case OMAP_ABE_MM_DL_PORT:
		case OMAP_ABE_TONES_DL_PORT:
			smem1 = omap_aess_update_io_task1(aess, &(aess->fw_info->port[id].tsk_freq[idx].task), 1);
			break;
		case OMAP_ABE_MM_UL_PORT:
			copy_func_index1 = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_MM_UL_ID];
			before_func_index = aess->fw_info->fct_id[OMAP_AESS_COPY_FCT_ROUTE_MM_UL_ID];
			break;
		case OMAP_ABE_MM_EXT_IN_PORT:
			/* set the SMEM buffer -- programming sequence */
			smem1 = aess->fw_info->label_id[OMAP_AESS_BUFFER_MM_EXT_IN_ID];
			break;
		case OMAP_ABE_PDM_DL_PORT:
		case OMAP_ABE_PDM_UL_PORT:
		case OMAP_ABE_DMIC_PORT:
		case OMAP_ABE_MM_UL2_PORT:
		case OMAP_ABE_MM_EXT_OUT_PORT:
		default:
			break;
		}

		if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
			direction = 0;
		else
			/* offset of the write pointer in the ATC descriptor */
			direction = 3;

		sio_desc.io_type_idx = (u8) io_sub_id;
		sio_desc.samp_size = (u8) datasize;
		sio_desc.hw_ctrl_addr = (u16) (dmareq_addr << 2);
		sio_desc.atc_irq_data = (u8) dmareq_field;
		sio_desc.flow_counter = (u16) 0;
		sio_desc.direction_rw = (u8) direction;
		sio_desc.repeat_last_samp = (u8) io_flag;
		sio_desc.nsamp = (u8) nsamp;
		sio_desc.x_io = (u8) x_io;
		/* set ATC ON */
		sio_desc.on_off = 0x80;
		sio_desc.split_addr1 = (u16) smem1;
		sio_desc.split_addr2 = (u16) smem2;
		sio_desc.split_addr3 = (u16) smem3;
		sio_desc.before_f_index = (u8) before_func_index;
		sio_desc.after_f_index = (u8) after_func_index;
		sio_desc.smem_addr1 = (u16) smem1;
		sio_desc.atc_address1 = (u16) atc_desc_address1;
		sio_desc.atc_pointer_saved1 = (u16) atc_ptr_saved;
		sio_desc.data_size1 = (u8) datasize;
		sio_desc.copy_f_index1 = (u8) copy_func_index1;
		sio_desc.smem_addr2 = (u16) smem2;
		sio_desc.atc_address2 = (u16) atc_desc_address2;
		sio_desc.atc_pointer_saved2 = (u16) atc_ptr_saved2;
		sio_desc.data_size2 = (u8) datasize2;
		sio_desc.copy_f_index2 = (u8) copy_func_index2;

		memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_IODESCR_ID],
		       sizeof(struct omap_aess_addr));
		addr.bytes = sizeof(struct omap_aess_io_desc);
		addr.offset += (id * sizeof(struct omap_aess_io_desc));

		omap_aess_mem_write(aess, addr, (u32 *)&sio_desc);

	}
	omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_DMEM_MULTIFRAME_ID],
			    (u32 *)aess->MultiFrame);

	return 0;
}

/**
 * omap_aess_select_main_port - Select stynchronization port for Event generator.
 * @aess: Pointer on aess handle
 * @id: port name
 *
 * tells the FW which is the reference stream for adjusting
 * the processing on 23/24/25 slots
 */
int omap_aess_select_main_port(struct omap_aess *aess, u32 id)
{
	u32 selection;

	/* flow control */
	selection = aess->fw_info->map[OMAP_AESS_DMEM_IODESCR_ID].offset + id * sizeof(struct omap_aess_io_desc) +
		offsetof(struct omap_aess_io_desc, flow_counter);
	/* when the main port is a sink port from AESS point of view
	   the sign the firmware task analysis must be changed  */
	selection &= 0xFFFFL;
	if (abe_port[id].protocol.direction == ABE_ATC_DIRECTION_IN)
		selection |= 0x80000;

	omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_DMEM_SLOT23_CTRL_ID],
			    &selection);
	return 0;
}

/**
 * omap_aess_decide_main_port()  - Select stynchronization port for Event generator.
 * @id: audio port name
 *
 * tells the FW which is the reference stream for adjusting
 * the processing on 23/24/25 slots
 *
 * takes the first port in a list which is slave on the data interface
 */
static u32 abe_valid_port_for_synchro(u32 id)
{
	if ((abe_port[id].protocol.protocol_switch == OMAP_AESS_PORT_DMAREQ) ||
	    (abe_port[id].protocol.protocol_switch == OMAP_AESS_PORT_PINGPONG) ||
	    (abe_port[id].status != OMAP_ABE_PORT_ACTIVITY_RUNNING))
		return 0;
	else
		return 1;
}

/**
 * omap_aess_decide_main_port()  - Decide main port selection for synchronization.
 * @aess: Pointer on aess handle
 *
 * Lock up on all ABE port in order to find out the correct port for the
 * Audio Engine synchronization.
 */
void omap_aess_decide_main_port(struct omap_aess *aess)
{
	u32 id, id_not_found;

	id_not_found = 1;
	for (id = 0; id < LAST_PORT_ID - 1; id++) {
		if (abe_valid_port_for_synchro(abe_port_priority[id])) {
			id_not_found = 0;
			break;
		}
	}

	/* if no port is currently activated, the default one is PDM_DL */
	if (id_not_found)
		omap_aess_select_main_port(aess, OMAP_ABE_PDM_DL_PORT);
	else
		omap_aess_select_main_port(aess, abe_port_priority[id]);
}

/**
 * abe_format_switch
 * @f: port format
 * @iter: port iteration
 * @mulfac: multiplication factor
 *
 * translates the sampling and data length to ITER number for the DMA
 * and the multiplier factor to apply during data move with DMEM
 *
 */
static void abe_format_switch(struct omap_aess_data_format *f, u32 *iter, u32 *mulfac)
{
	u32 n_freq;
	switch (f->f) {
		/* nb of samples processed by scheduling loop */
	case 8000:
		n_freq = 2;
		break;
	case 16000:
		n_freq = 4;
		break;
	case 24000:
		n_freq = 6;
		break;
	case 44100:
		n_freq = 12;
		break;
	case 96000:
		n_freq = 24;
		break;
	default:
		/*case 48000 */
		n_freq = 12;
		break;
	}

	switch (f->samp_format) {
	case OMAP_AESS_FORMAT_MONO_MSB:
	case OMAP_AESS_FORMAT_MONO_RSHIFTED_16:
	case OMAP_AESS_FORMAT_STEREO_16_16:
		*mulfac = 1;
		break;
	case OMAP_AESS_FORMAT_STEREO_MSB:
	case OMAP_AESS_FORMAT_STEREO_RSHIFTED_16:
		*mulfac = 2;
		break;
	case OMAP_AESS_FORMAT_THREE_MSB:
		*mulfac = 3;
		break;
	case OMAP_AESS_FORMAT_FOUR_MSB:
		*mulfac = 4;
		break;
	case OMAP_AESS_FORMAT_FIVE_MSB:
		*mulfac = 5;
		break;
	case OMAP_AESS_FORMAT_SIX_MSB:
		*mulfac = 6;
		break;
	case OMAP_AESS_FORMAT_SEVEN_MSB:
		*mulfac = 7;
		break;
	case OMAP_AESS_FORMAT_EIGHT_MSB:
		*mulfac = 8;
		break;
	case OMAP_AESS_FORMAT_NINE_MSB:
		*mulfac = 9;
		break;
	default:
		*mulfac = 1;
		break;
	}
	*iter = (n_freq * (*mulfac));
	if (f->samp_format == OMAP_AESS_FORMAT_MONO_16_16)
		*iter /= 2;
}

/**
 * abe_dma_port_iteration
 * @f: port format
 *
 * translates the sampling and data length to ITER number for the DMA
 */
static u32 abe_dma_port_iteration(struct omap_aess_data_format *f)
{
	u32 iter, mulfac;

	abe_format_switch(f, &iter, &mulfac);
	return iter;
}

/**
 * abe_dma_port_iter_factor
 * @f: port format
 *
 * returns the multiplier factor to apply during data move with DMEM
 */
static u32 abe_dma_port_iter_factor(struct omap_aess_data_format *f)
{
	u32 iter, mulfac;

	abe_format_switch(f, &iter, &mulfac);
	return mulfac;
}

/**
 * omap_aess_dma_port_iter_factor
 * @aess: Pointer on aess handle
 * @f: port format
 *
 * returns the multiplier factor to apply during data move with DMEM
 */
static u32 omap_aess_dma_port_iter_factor(struct omap_aess *aess, struct omap_aess_data_format *f)
{
	u32 iter, mulfac;

	abe_format_switch(f, &iter, &mulfac);
	return mulfac;
}

/**
 * omap_aess_mono_mixer
 * @aess: Pointer on aess handle
 * @id: name of the mixer (MIXDL1, MIXDL2 or MIXAUDUL)
 * @on_off: enable/disable flag
 *
 * This API Programs DL1Mixer or DL2Mixer to output mono data
 * on both left and right data paths.
 */
int omap_aess_mono_mixer(struct omap_aess *aess, u32 id, u32 on_off)
{
	struct omap_aess_task *task;

	switch (id) {
	case MIXDL1:
		task = &aess->fw_info->dl1_mono_mixer[on_off];
		break;
	case MIXDL2:
		task = &aess->fw_info->dl2_mono_mixer[on_off];
		break;
	case MIXAUDUL:
		task = &aess->fw_info->audul_mono_mixer[on_off];
		break;
	default:
		return 0;
		break;
	}

	aess->MultiFrame[task->frame][task->slot] = task->task;

	omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_DMEM_MULTIFRAME_ID],
			    (u32 *)aess->MultiFrame);

	return 0;
}
EXPORT_SYMBOL(omap_aess_mono_mixer);

/**
 * omap_aess_check_activity - Check if some ABE activity.
 * @aess: Pointer on aess handle
 *
 * Check if any ABE ports are running.
 * return 1: still activity on ABE
 * return 0: no more activity on ABE. Event generator can be stopped
 *
 */
int omap_aess_check_activity(struct omap_aess *aess)
{
	int i, ret = 0;

	for (i = 0; i < (LAST_PORT_ID - 1); i++) {
		if (abe_port[abe_port_priority[i]].status ==
				OMAP_ABE_PORT_ACTIVITY_RUNNING)
			break;
	}
	if (i < (LAST_PORT_ID - 1))
		ret = 1;
	return ret;
}
EXPORT_SYMBOL(omap_aess_check_activity);

/**
 * abe_write_pdmdl_offset - write the desired offset on the DL1/DL2 paths
 * @aess: Pointer on aess handle
 * @path: DL1 or DL2 port
 * @offset_left: integer value that will be added on all PDM left samples
 * @offset_right: integer value that will be added on all PDM right samples
 *
 * Set ABE internal DC offset cancellation parameter for McPDM IP. Value
 * depends on TWL604x triming parameters.
 */
void omap_aess_write_pdmdl_offset(struct omap_aess *aess, u32 path,
				  u32 offset_left, u32 offset_right)
{
	u32 offset[2];

	offset[0] = offset_right;
	offset[1] = offset_left;

	switch (path) {
	case 1:
		omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_SMEM_DC_HS_ID], offset);
		break;
	case 2:
		omap_aess_mem_write(aess, aess->fw_info->map[OMAP_AESS_SMEM_DC_HF_ID], offset);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL(omap_aess_write_pdmdl_offset);

/**
 * oamp_abe_set_ping_pong_buffer
 * @aess: Pointer on aess handle
 * @port: ABE port ID
 * @n_bytes: Size of Ping/Pong buffer
 *
 * Updates the next ping-pong buffer with "size" bytes copied from the
 * host processor. This API notifies the FW that the data transfer is done.
 */
int omap_aess_set_ping_pong_buffer(struct omap_aess *aess, u32 port, u32 n_bytes)
{
	u32 struct_offset, n_samples, datasize, base_and_size;
	struct omap_aess_pingpong_desc desc_pp;
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (port != OMAP_ABE_MM_DL_PORT) {
		aess_err("Only Ping-pong port supported");
		return -AESS_EINVAL;
	}
	/* translates the number of bytes in samples */
	/* data size in DMEM words */
	datasize = omap_aess_dma_port_iter_factor(aess, (struct omap_aess_data_format *)&((abe_port[port]).format));
	/* data size in bytes */
	datasize = datasize << 2;
	n_samples = n_bytes / datasize;
	memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
	       sizeof(struct omap_aess_addr));
	addr.bytes = sizeof(struct omap_aess_pingpong_desc);
	omap_aess_mem_read(aess, addr, (u32 *)&desc_pp);
	/*
	 * read the port SIO descriptor and extract the current pointer
	 * address after reading the counter
	 */
	if ((desc_pp.counter & 0x1) == 0) {
		struct_offset = (u32)&(desc_pp.nextbuff0_BaseAddr) -
			(u32)&(desc_pp);
		base_and_size = desc_pp.nextbuff0_BaseAddr;
	} else {
		struct_offset = (u32)&(desc_pp.nextbuff1_BaseAddr) -
			(u32)&(desc_pp);
		base_and_size = desc_pp.nextbuff1_BaseAddr;
	}

	base_and_size = aess->pp_buf_addr[aess->pp_buf_id_next];
	aess->pp_buf_id_next = (aess->pp_buf_id_next + 1) & 0x03;

	base_and_size = (base_and_size & 0xFFFFL) + (n_samples << 16);

	addr.offset += struct_offset;
	addr.bytes = 4;
	omap_aess_mem_write(aess, addr, (u32 *)&base_and_size);

	return 0;
}
EXPORT_SYMBOL(omap_aess_set_ping_pong_buffer);

/**
 * omap_aess_read_offset_from_ping_buffer
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 * @n:  returned address of the offset
 *	from the ping buffer start address (in samples)
 *
 * Computes the current firmware ping pong read pointer location,
 * expressed in samples, as the offset from the start address of ping buffer.
 */
int omap_aess_read_offset_from_ping_buffer(struct omap_aess *aess,
					  u32 id, u32 *n)
{
	struct omap_aess_pingpong_desc desc_pp;
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (OMAP_ABE_MM_DL_PORT != id) {
		aess_err("Only Ping-pong port supported");
		return -AESS_EINVAL;
	} else {
		/* read the port SIO ping pong descriptor */
		memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
		       sizeof(struct omap_aess_addr));
		addr.bytes = sizeof(struct omap_aess_pingpong_desc);
		omap_aess_mem_read(aess, addr, (u32 *)&desc_pp);
		/* extract the current ping pong buffer read pointer based on
		   the value of the counter */
		if ((desc_pp.counter & 0x1) == 0) {
			/* the next is buffer0, hence the current is buffer1 */
			*n = desc_pp.nextbuff1_Samples -
				desc_pp.workbuff_Samples;
		} else {
			/* the next is buffer1, hence the current is buffer0 */
			*n = desc_pp.nextbuff0_Samples -
				desc_pp.workbuff_Samples;
		}
		switch (abe_port[OMAP_ABE_MM_DL_PORT].format.samp_format) {
		case OMAP_AESS_FORMAT_MONO_MSB:
		case OMAP_AESS_FORMAT_MONO_RSHIFTED_16:
		case OMAP_AESS_FORMAT_STEREO_16_16:
			*n +=  aess->pp_buf_id * aess->size_pingpong / 4;
			break;
		case OMAP_AESS_FORMAT_STEREO_MSB:
		case OMAP_AESS_FORMAT_STEREO_RSHIFTED_16:
			*n += aess->pp_buf_id * aess->size_pingpong / 8;
			break;
		default:
			aess_err("Bad data format for Ping-pong buffer");
			return -AESS_EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(omap_aess_read_offset_from_ping_buffer);

/**
 * omap_aess_read_next_ping_pong_buffer
 * @aess: Pointer on aess handle
 * @port: ABE portID
 * @p: Next buffer address (pointer)
 * @n: Next buffer size (pointer)
 *
 * Tell the next base address of the next ping_pong Buffer and its size
 */
int omap_aess_read_next_ping_pong_buffer(struct omap_aess *aess, u32 port,
					 u32 *p, u32 *n)
{
	struct omap_aess_pingpong_desc desc_pp;
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (port != OMAP_ABE_MM_DL_PORT) {
		aess_err("Only Ping-pong port supported");
		return -AESS_EINVAL;
	}
	/* read the port SIO descriptor and extract the current pointer
	   address after reading the counter */
	memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
	       sizeof(struct omap_aess_addr));
	addr.bytes = sizeof(struct omap_aess_pingpong_desc);
	omap_aess_mem_read(aess, addr, (u32 *)&desc_pp);

	if ((desc_pp.counter & 0x1) == 0)
		*p = desc_pp.nextbuff0_BaseAddr;
	else
		*p = desc_pp.nextbuff1_BaseAddr;

	/* translates the number of samples in bytes */
	*n = aess->size_pingpong;

	return 0;
}
EXPORT_SYMBOL(omap_aess_read_next_ping_pong_buffer);

/**
 * omap_aess_init_ping_pong_buffer
 * @aess: Pointer on aess handle
 * @id: ABE port ID
 * @size_bytes:size of the ping pong
 * @n_buffers:number of buffers (2 = ping/pong)
 * @p:returned address of the ping-pong list of base addresses
 *	(byte offset from DMEM start)
 *
 * Computes the base address of the ping_pong buffers
 */
static int omap_aess_init_ping_pong_buffer(struct omap_aess *aess,
					   u32 id, u32 size_bytes,
					   u32 n_buffers, u32 *p)
{
	u32 i, dmem_addr;
	struct omap_aess_addr addr;

	/* ping_pong is supported in 2 buffers configuration right now but FW
	   is ready for ping/pong/pung/pang... */
	if (id != OMAP_ABE_MM_DL_PORT || n_buffers > MAX_PINGPONG_BUFFERS) {
		aess_err("Too Many Ping-pong buffers requested");
		return -AESS_EINVAL;
	}

	memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PING_ID],
	       sizeof(struct omap_aess_addr));

	for (i = 0; i < n_buffers; i++) {
		dmem_addr = addr.offset + (i * size_bytes);
		/* base addresses of the ping pong buffers in U8 unit */
		aess->base_address_pingpong[i] = dmem_addr;
	}

	for (i = 0; i < 4; i++)
		aess->pp_buf_addr[i] = addr.offset + (i * size_bytes);
	aess->pp_buf_id = 0;
	aess->pp_buf_id_next = 0;
	aess->pp_first_irq = 1;

	/* global data */
	aess->size_pingpong = size_bytes;
	*p = (u32)addr.offset;
	return 0;
}

/**
 * omap_aess_connect_irq_ping_pong_port
 * @aess: Pointer on aess handle
 * @id: port name
 * @f: desired data format
 * @subroutine_id: index of the call-back subroutine to call
 * @size: half-buffer (ping) size
 * @sink: returned base address of the first (ping) buffer)
 * @dsp_mcu_flag: Ping/pong interrupt direction (MPU or DSP)
 *
 * enables the data echanges between a direct access to the DMEM
 * memory of ABE using cache flush. On each IRQ activation a subroutine
 * registered with "abe_plug_subroutine" will be called. This subroutine
 * will generate an amount of samples, send them to DMEM memory and call
 * "abe_set_ping_pong_buffer" to notify the new amount of samples in the
 * pong buffer.
 */
int omap_aess_connect_irq_ping_pong_port(struct omap_aess *aess,
					 u32 id, struct omap_aess_data_format *f,
					 u32 subroutine_id, u32 size,
					 u32 *sink, u32 dsp_mcu_flag)
{
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (id != OMAP_ABE_MM_DL_PORT) {
		aess_err("Only Ping-pong port supported");
		return -AESS_EINVAL;
	}

	memcpy(&addr, &aess->fw_info->map[OMAP_AESS_DMEM_PING_ID],
	       sizeof(struct omap_aess_addr));

	abe_port[id] = ((struct omap_aess_port *)aess->fw_info->port)[id];
	(abe_port[id]).format = (*f);
	(abe_port[id]).protocol.protocol_switch = OMAP_AESS_PORT_PINGPONG;
	(abe_port[id]).protocol.p.prot_pingpong.buf_addr = addr.offset;
	(abe_port[id]).protocol.p.prot_pingpong.buf_size = size;
	(abe_port[id]).protocol.p.prot_pingpong.irq_data = (1);
	omap_aess_init_ping_pong_buffer(aess, OMAP_ABE_MM_DL_PORT, size, 2, sink);
	if (dsp_mcu_flag == PING_PONG_WITH_MCU_IRQ)
		(abe_port[id]).protocol.p.prot_pingpong.irq_addr =
			OMAP_AESS_MCU_IRQSTATUS_RAW;
	if (dsp_mcu_flag == PING_PONG_WITH_DSP_IRQ)
		(abe_port[id]).protocol.p.prot_pingpong.irq_addr =
			OMAP_AESS_DSP_IRQSTATUS_RAW;
	abe_port[id].status = OMAP_ABE_PORT_INITIALIZED;

	/* load the ATC descriptors - disabled */
	omap_aess_init_atc(aess, id);
	/* load the micro-task parameters */
	omap_aess_init_io_tasks(aess,  id, &((abe_port[id]).format),
				&((abe_port[id]).protocol));

	*sink = (abe_port[id]).protocol.p.prot_pingpong.buf_addr;
	return 0;
}
EXPORT_SYMBOL(omap_aess_connect_irq_ping_pong_port);

