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

/* #include "abe.h" */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe_typ.h"
#include "abe.h"
#include "abe_port.h"
#include "abe_dbg.h"
#include "abe_mem.h"
#include "abe_gain.h"

#include "abe_taskid.h"
#include "abe_functionsid.h"
#include "abe_initxxx_labels.h"
#include "abe_define.h"
#include "abe_def.h"
#include "abe_typedef.h"
#include "abe_private.h"


/* one scheduler loop = 4kHz = 12 samples at 48kHz */
#define FW_SCHED_LOOP_FREQ	4000
/* one scheduler loop = 4kHz = 12 samples at 48kHz */
#define FW_SCHED_LOOP_FREQ_DIV1000	(FW_SCHED_LOOP_FREQ/1000)
#define EVENT_FREQUENCY 96000
#define SLOTS_IN_SCHED_LOOP (96000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_8kHz (8000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_16kHz (16000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_24kHz (24000/FW_SCHED_LOOP_FREQ)
#define SCHED_LOOP_48kHz (48000/FW_SCHED_LOOP_FREQ)

#define dmem_mm_trace	OMAP_ABE_D_DEBUG_FIFO_ADDR
#define dmem_mm_trace_size ((OMAP_ABE_D_DEBUG_FIFO_SIZE)/4)
#define dmem_dmic OMAP_ABE_D_DMIC_UL_FIFO_ADDR
#define dmem_dmic_size (OMAP_ABE_D_DMIC_UL_FIFO_SIZE/4)
#define dmem_amic OMAP_ABE_D_MCPDM_UL_FIFO_ADDR
#define dmem_amic_size (OMAP_ABE_D_MCPDM_UL_FIFO_SIZE/4)
#define dmem_mcpdm OMAP_ABE_D_MCPDM_DL_FIFO_ADDR
#define dmem_mcpdm_size (OMAP_ABE_D_MCPDM_DL_FIFO_SIZE/4)
#define dmem_mm_ul OMAP_ABE_D_MM_UL_FIFO_ADDR
#define dmem_mm_ul_size (OMAP_ABE_D_MM_UL_FIFO_SIZE/4)
#define dmem_mm_ul2 OMAP_ABE_D_MM_UL2_FIFO_ADDR
#define dmem_mm_ul2_size (OMAP_ABE_D_MM_UL2_FIFO_SIZE/4)
#define dmem_mm_dl OMAP_ABE_D_MM_DL_FIFO_ADDR
#define dmem_mm_dl_size (OMAP_ABE_D_MM_DL_FIFO_SIZE/4)
#define dmem_vx_dl OMAP_ABE_D_VX_DL_FIFO_ADDR
#define dmem_vx_dl_size (OMAP_ABE_D_VX_DL_FIFO_SIZE/4)
#define dmem_vx_ul OMAP_ABE_D_VX_UL_FIFO_ADDR
#define dmem_vx_ul_size (OMAP_ABE_D_VX_UL_FIFO_SIZE/4)
#define dmem_tones_dl OMAP_ABE_D_TONES_DL_FIFO_ADDR
#define dmem_tones_dl_size (OMAP_ABE_D_TONES_DL_FIFO_SIZE/4)
#define dmem_vib_dl OMAP_ABE_D_VIB_DL_FIFO_ADDR
#define dmem_vib_dl_size (OMAP_ABE_D_VIB_DL_FIFO_SIZE/4)
#define dmem_mm_ext_out OMAP_ABE_D_MM_EXT_OUT_FIFO_ADDR
#define dmem_mm_ext_out_size (OMAP_ABE_D_MM_EXT_OUT_FIFO_SIZE/4)
#define dmem_mm_ext_in OMAP_ABE_D_MM_EXT_IN_FIFO_ADDR
#define dmem_mm_ext_in_size (OMAP_ABE_D_MM_EXT_IN_FIFO_SIZE/4)
#define dmem_bt_vx_dl OMAP_ABE_D_BT_DL_FIFO_ADDR
#define dmem_bt_vx_dl_size (OMAP_ABE_D_BT_DL_FIFO_SIZE/4)
#define dmem_bt_vx_ul OMAP_ABE_D_BT_UL_FIFO_ADDR
#define dmem_bt_vx_ul_size (OMAP_ABE_D_BT_UL_FIFO_SIZE/4)

/*
 * ATC BUFFERS + IO TASKS SMEM buffers
 */
#define smem_amic	AMIC_96_labelID

/* managed directly by the router */
#define smem_mm_ul MM_UL_labelID
/* managed directly by the router */
#define smem_mm_ul2 MM_UL2_labelID
#define smem_mm_dl MM_DL_labelID
#define smem_vx_dl	IO_VX_DL_ASRC_labelID	/* Voice_16k_DL_labelID */
#define smem_vx_ul Voice_8k_UL_labelID
#define smem_tones_dl Tones_labelID
#define smem_vib IO_VIBRA_DL_labelID
#define smem_mm_ext_out DL1_GAIN_out_labelID
/*IO_MM_EXT_IN_ASRC_labelID	 ASRC input buffer, size 40 */
#define smem_mm_ext_in_opp100 IO_MM_EXT_IN_ASRC_labelID
/* at OPP 50 without ASRC */
#define smem_mm_ext_in_opp50 MM_EXT_IN_labelID
#define smem_bt_vx_dl_opp50 BT_DL_8k_labelID
/*BT_DL_8k_opp100_labelID  ASRC output buffer, size 40 */
#define smem_bt_vx_dl_opp100 BT_DL_8k_opp100_labelID
#define smem_bt_vx_ul_opp50 BT_UL_8k_labelID
/*IO_BT_UL_ASRC_labelID	 ASRC input buffer, size 40 */
#define smem_bt_vx_ul_opp100 IO_BT_UL_ASRC_labelID


#define ATC_SIZE 8		/* 8 bytes per descriptors */


/*
 * HAL/FW ports status / format / sampling / protocol(call_back) / features
 *	/ gain / name
 */
struct omap_aess_port abe_port[LAST_PORT_ID];	/* list of ABE ports */
const struct omap_aess_port abe_port_init[LAST_PORT_ID] = {
	/* Status Data Format Drift Call-Back Protocol+selector desc_addr;
	   buf_addr; buf_size; iter; irq_addr irq_data DMA_T $Features
	   reseted at start Port Name for the debug trace */
	{
		/* DMIC */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 96000,
			.samp_format = SIX_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = 1,
		.smem_buffer2 = (DMIC_ITER/6),
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = DMIC_PORT_PROT,
			.p = {
				.prot_dmic = {
					.buf_addr = dmem_dmic,
					.buf_size = dmem_dmic_size,
					.nbchan = DMIC_ITER,
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 2,
			.task = {
				{
					.frame = 2,
					.slot = 5,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_DMIC),
				},
				{
					.frame = 14,
					.slot = 3,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_DMIC),
				},
			},
		},
	},
	{
		/* PDM_UL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 96000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_amic,
		.smem_buffer2 = (MCPDM_UL_ITER/2),
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = MCPDMUL_PORT_PROT,
			.p = {
				.prot_mcpdmul = {
					.buf_addr = dmem_amic,
					.buf_size = dmem_amic_size,
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 5,
					.slot = 2,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_UL),
				},
			},
		},
	},
	{
		/* BT_VX_UL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 8000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_bt_vx_ul_opp50,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = SERIAL_PORT_PROT,
			.p = {
				.prot_serial = {
					.desc_addr = (MCBSP1_DMA_TX*ATC_SIZE),
					.buf_addr = dmem_bt_vx_ul,
					.buf_size = dmem_bt_vx_ul_size,
					.iter = (1*SCHED_LOOP_8kHz),
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 15,
					.slot = 3,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_BT_VX_UL),
				},
			},
		},
	},
	{
		/* MM_UL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_mm_ul,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX3*ATC_SIZE),
					.buf_addr = dmem_mm_ul,
					.buf_size = dmem_mm_ul_size,
					.iter = (10*SCHED_LOOP_48kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 3),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__3,
			.iter = 120,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 19,
					.slot = 6,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_UL),
				},
			},
		},
	},
	{
		/* MM_UL2 */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_mm_ul2,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX4*ATC_SIZE),
					.buf_addr = dmem_mm_ul2,
					.buf_size = dmem_mm_ul2_size,
					.iter = (2*SCHED_LOOP_48kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 4),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__4,
			.iter = 24,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 17,
					.slot = 3,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_UL2),
				},
			},
		},
	},
	{
		/* VX_UL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 8000,
			.samp_format = MONO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_vx_ul,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX2*ATC_SIZE),
					.buf_addr = dmem_vx_ul,
					.buf_size = dmem_vx_ul_size,
					.iter = (1*SCHED_LOOP_8kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 2),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__2,
			.iter = 2,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 16,
					.slot = 3,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_VX_UL),
				},
			},
		},
	},
	{
		/* MM_DL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_mm_dl,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX0*ATC_SIZE),
					.buf_addr = dmem_mm_dl,
					.buf_size = dmem_mm_dl_size,
					.iter = (2*SCHED_LOOP_48kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 0),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__0,
			.iter = 24,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 18,
					.slot = 0,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_DL),
				},
			},
		},
	},
	{
		/* VX_DL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 8000,
			.samp_format = MONO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_vx_dl,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX1*ATC_SIZE),
					.buf_addr = dmem_vx_dl,
					.buf_size = dmem_vx_dl_size,
					.iter = (1*SCHED_LOOP_8kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 1),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__1,
			.iter = 2,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 22,
					.slot = 2,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_VX_DL),
				},
			},
		},
	},
	{
		/* TONES_DL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_tones_dl,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX5*ATC_SIZE),
					.buf_addr = dmem_tones_dl,
					.buf_size = dmem_tones_dl_size,
					.iter = (2*SCHED_LOOP_48kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 5),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__5,
			.iter = 24,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 20,
					.slot = 0,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_TONES_DL),
				},
			},
		},
	},
	{
		/* VIB_DL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 24000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_vib,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX6*ATC_SIZE),
					.buf_addr = dmem_vib_dl,
					.buf_size = dmem_vib_dl_size,
					.iter = (2*SCHED_LOOP_24kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 6),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__6,
			.iter = 12,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 1,
					.slot = 7,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_VIB_DL),
				},
			},
		},
	},
	{
		/* BT_VX_DL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 8000,
			.samp_format = MONO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_bt_vx_dl_opp50,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = SERIAL_PORT_PROT,
			.p = {
				.prot_serial = {
					.desc_addr = (MCBSP1_DMA_RX*ATC_SIZE),
					.buf_addr = dmem_bt_vx_dl,
					.buf_size = dmem_bt_vx_dl_size,
					.iter = (1*SCHED_LOOP_8kHz),
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 13,
					.slot = 5,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_BT_VX_DL),
				},
			},
		},
	},
	{
		/* PDM_DL */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 96000,
			.samp_format = SIX_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = 1,
		.smem_buffer2 = (MCPDM_DL_ITER/6),
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = MCPDMDL_PORT_PROT,
			.p = {
				.prot_mcpdmdl = {
					.buf_addr = dmem_mcpdm,
					.buf_size = dmem_mcpdm_size,
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 2,
			.task = {
				{
					.frame = 7,
					.slot = 0,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_DL),
				},
				{
					.frame = 19,
					.slot = 0,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_PDM_DL),
				},
			},
		},
	},
	{
		/* MM_EXT_OUT */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_mm_ext_out,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = SERIAL_PORT_PROT,
			.p = {
				.prot_serial = {
					.desc_addr = (MCBSP1_DMA_TX*ATC_SIZE),
					.buf_addr = dmem_mm_ext_out,
					.buf_size = dmem_mm_ext_out_size,
					.iter = (2*SCHED_LOOP_48kHz),
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 15,
					.slot = 0,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_EXT_OUT),
				},
			},
		},
	},
	{
		/* MM_EXT_IN */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = smem_mm_ext_in_opp100,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SNK_P,
			.protocol_switch = SERIAL_PORT_PROT,
			.p = {
				.prot_serial = {
					.desc_addr = (MCBSP1_DMA_RX*ATC_SIZE),
					.buf_addr = dmem_mm_ext_in,
					.buf_size = dmem_mm_ext_in_size,
					.iter = (2*SCHED_LOOP_48kHz),
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 1,
			.task = {
				{
					.frame = 21,
					.slot = 3,
					.task = ABE_TASK_ID(C_ABE_FW_TASK_IO_MM_EXT_IN),
				},
			},
		},
	},
	{
		/* PCM3_TX */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = 1,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = SERIAL_PORT_PROT,
			.p = {
				.prot_serial = {
					.desc_addr = (MCBSP3_DMA_TX * ATC_SIZE),
					.buf_addr = dmem_mm_ext_out,
					.buf_size = dmem_mm_ext_out_size,
					.iter = (2*SCHED_LOOP_48kHz),
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 0,
		},
	},
	{
		/* PCM3_RX */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = STEREO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = 1,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = SERIAL_PORT_PROT,
			.p = {
				.prot_serial = {
					.desc_addr = (MCBSP3_DMA_RX * ATC_SIZE),
					.buf_addr = dmem_mm_ext_in,
					.buf_size = dmem_mm_ext_in_size,
					.iter = (2*SCHED_LOOP_48kHz),
				},
			},
		},
		.dma = {
			.data = 0,
			.iter = 0,
		},
		.task = {
			.nb_task = 0,
		},
	},
	{
		/* SCHD_DBG_PORT */
		.status = OMAP_ABE_PORT_ACTIVITY_IDLE,
		.format = {
			.f = 48000,
			.samp_format = MONO_MSB,
		},
		.drift = NODRIFT,
		.callback = NOCALLBACK,
		.smem_buffer1 = 1,
		.smem_buffer2 = 1,
		.protocol = {
			.direction = SRC_P,
			.protocol_switch = DMAREQ_PORT_PROT,
			.p = {
				.prot_dmareq = {
					.desc_addr = (CBPr_DMA_RTX7 * ATC_SIZE),
					.buf_addr = dmem_mm_trace,
					.buf_size = dmem_mm_trace_size,
					.iter = (2*SCHED_LOOP_48kHz),
					.dma_addr = ABE_DMASTATUS_RAW,
					.dma_data = (1 << 4),
				},
			},
		},
		.dma = {
			.data = CIRCULAR_BUFFER_PERIPHERAL_R__7,
			.iter = 24,
		},
		.task = {
			.nb_task = 0,
		},
	},
};
/*
 * AESS/ATC destination and source address translation (except McASPs)
 * from the original 64bits words address
 */
const u32 abe_atc_dstid[ABE_ATC_DESC_SIZE >> 3] = {
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
const u32 abe_atc_srcid[ABE_ATC_DESC_SIZE >> 3] = {
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


/*
 * MAIN PORT SELECTION
 */
const u32 abe_port_priority[LAST_PORT_ID - 1] = {
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
	OMAP_ABE_VIB_DL_PORT,
};
