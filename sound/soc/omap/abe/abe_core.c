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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>

#include "abe.h"
#include "abe_gain.h"
#include "abe_aess.h"
#include "abe_port.h"
#include "abe_mem.h"
#include "abe_typedef.h"
#include "abe_dbg.h"
#include "abe_seq.h"
#include "abe_private.h"

#define OMAP_ABE_IRQ_FIFO_MASK ((OMAP_ABE_D_MCUIRQFIFO_SIZE >> 2) - 1)

void abe_irq_check_for_sequences(u32 seq_info);

/**
 * abe_omap_aess_reset_hal - reset the ABE/HAL
 * @abe: Pointer on abe handle
 *
 * Operations : reset the ABE by reloading the static variables and
 * default AESS registers.
 * Called after a PRCM cold-start reset of ABE
 */
int omap_aess_reset_hal(struct omap_aess *abe)
{
	u32 i;

	omap_aess_dbg_reset(abe);

	/* IRQ & DBG circular read pointer in DMEM */
	abe->irq_dbg_read_ptr = 0;

	/* default = disable the mixer's adaptive gain control */
	omap_aess_use_compensated_gain(abe, 0);

	/* reset the default gain values */
	for (i = 0; i < MAX_NBGAIN_CMEM; i++) {
		abe->muted_gains_indicator[i] = 0;
		abe->desired_gains_decibel[i] = (u32) GAIN_MUTE;
		abe->desired_gains_linear[i] = 0;
		abe->desired_ramp_delay_ms[i] = 0;
		abe->muted_gains_decibel[i] = (u32) GAIN_TOOLOW;
	}
	omap_aess_hw_configuration(abe);
	return 0;
}
EXPORT_SYMBOL(omap_aess_reset_hal);

/**
 * omap_aess_wakeup - Wakeup ABE
 * @abe: Pointer on abe handle
 *
 * Wakeup ABE in case of retention
 */
int omap_aess_wakeup(struct omap_aess *abe)
{
	/* Restart event generator */
	omap_aess_write_event_generator(abe, EVENT_TIMER);

	/* reconfigure DMA Req and MCU Irq visibility */
	omap_aess_hw_configuration(abe);
	return 0;
}
EXPORT_SYMBOL(omap_aess_wakeup);

/**
 * omap_aess_monitoring
 *
 * checks the internal status of ABE and HAL
 */
static void omap_aess_monitoring(struct omap_aess *abe)
{

}

/**
 * omap_aess_irq_ping_pong
 *
 * Call the respective subroutine depending on the IRQ FIFO content:
 * APS interrupts : IRQ_FIFO[31:28] = IRQtag_APS,
 *	IRQ_FIFO[27:16] = APS_IRQs, IRQ_FIFO[15:0] = loopCounter
 * SEQ interrupts : IRQ_FIFO[31:28] = IRQtag_COUNT,
 *	IRQ_FIFO[27:16] = Count_IRQs, IRQ_FIFO[15:0] = loopCounter
 * Ping-Pong Interrupts : IRQ_FIFO[31:28] = IRQtag_PP,
 *	IRQ_FIFO[27:16] = PP_MCU_IRQ, IRQ_FIFO[15:0] = loopCounter
 */
static void omap_aess_irq_ping_pong(struct omap_aess *abe)
{
	/* first IRQ doesn't represent a buffer transference completion */
	if (abe->pp_first_irq)
		abe->pp_first_irq = 0;
	else
		abe->pp_buf_id = (abe->pp_buf_id + 1) & 0x03;

	omap_aess_call_subroutine(abe, abe->seq.irq_pingpong_player_id, NOPARAMETER,
			    NOPARAMETER, NOPARAMETER, NOPARAMETER);
}

/**
 * omap_aess_irq_processing - Process ABE interrupt
 * @abe: Pointer on abe handle
 *
 * This subroutine is call upon reception of "MA_IRQ_99 ABE_MPU_IRQ" Audio
 * back-end interrupt. This subroutine will check the ATC Hrdware, the
 * IRQ_FIFO from the AE and act accordingly. Some IRQ source are originated
 * for the delivery of "end of time sequenced tasks" notifications, some are
 * originated from the Ping-Pong protocols, some are generated from
 * the embedded debugger when the firmware stops on programmable break-points,
 * etc ...
 */
int omap_aess_irq_processing(struct omap_aess *abe)
{
	u32 abe_irq_dbg_write_ptr, i, cmem_src, sm_cm;
	struct omap_aess_irq_data IRQ_data;
	struct omap_aess_addr addr;

	/* extract the write pointer index from CMEM memory (INITPTR format) */
	/* CMEM address of the write pointer in bytes */
	cmem_src = MCU_IRQ_FIFO_ptr_labelID << 2;
	omap_abe_mem_read(abe, OMAP_ABE_CMEM, cmem_src,
			&sm_cm, sizeof(abe_irq_dbg_write_ptr));
	/* AESS left-pointer index located on MSBs */
	abe_irq_dbg_write_ptr = sm_cm >> 16;
	abe_irq_dbg_write_ptr &= 0xFF;
	/* loop on the IRQ FIFO content */
	for (i = 0; i < OMAP_ABE_D_MCUIRQFIFO_SIZE; i++) {
		/* stop when the FIFO is empty */
		if (abe_irq_dbg_write_ptr == abe->irq_dbg_read_ptr)
			break;
		/* read the IRQ/DBG FIFO */
		memcpy(&addr, &omap_aess_map[OMAP_AESS_DMEM_MCUIRQFIFO_ID],
			sizeof(struct omap_aess_addr));
		addr.offset += (abe->irq_dbg_read_ptr << 2);
		addr.bytes = sizeof(IRQ_data);
		omap_aess_mem_read(abe, addr, (u32 *) &IRQ_data);
		abe->irq_dbg_read_ptr = (abe->irq_dbg_read_ptr + 1) & OMAP_ABE_IRQ_FIFO_MASK;
		/* select the source of the interrupt */
		switch (IRQ_data.tag) {
		case IRQtag_PP:
			omap_aess_irq_ping_pong(abe);
			break;
		case IRQtag_COUNT:
			/* abe_irq_check_for_sequences(IRQ_data.data); */
			omap_aess_monitoring(abe);
			break;
		default:
			break;
		}
	}
	return 0;
}
EXPORT_SYMBOL(omap_aess_irq_processing);

/**
 * oamp_abe_set_ping_pong_buffer
 * @abe: Pointer on abe handle
 * @port: ABE port ID
 * @n_bytes: Size of Ping/Pong buffer
 *
 * Updates the next ping-pong buffer with "size" bytes copied from the
 * host processor. This API notifies the FW that the data transfer is done.
 */
int omap_aess_set_ping_pong_buffer(struct omap_aess *abe, u32 port, u32 n_bytes)
{
	u32 struct_offset, n_samples, datasize, base_and_size;
	struct ABE_SPingPongDescriptor desc_pp;
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (port != OMAP_ABE_MM_DL_PORT) {
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	}
	/* translates the number of bytes in samples */
	/* data size in DMEM words */
	datasize = omap_aess_dma_port_iter_factor(abe, (struct omap_aess_data_format *)&((abe_port[port]).format));
	/* data size in bytes */
	datasize = datasize << 2;
	n_samples = n_bytes / datasize;
	memcpy(&addr, &omap_aess_map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
		sizeof(struct omap_aess_addr));
	addr.bytes = sizeof(struct ABE_SPingPongDescriptor);
	omap_aess_mem_read(abe, addr, (u32 *) &desc_pp);
	/*
	 * read the port SIO descriptor and extract the current pointer
	 * address after reading the counter
	 */
	if ((desc_pp.counter & 0x1) == 0) {
		struct_offset = (u32) &(desc_pp.nextbuff0_BaseAddr) -
			(u32) &(desc_pp);
		base_and_size = desc_pp.nextbuff0_BaseAddr;
	} else {
		struct_offset = (u32) &(desc_pp.nextbuff1_BaseAddr) -
			(u32) &(desc_pp);
		base_and_size = desc_pp.nextbuff1_BaseAddr;
	}

	base_and_size = abe->pp_buf_addr[abe->pp_buf_id_next];
	abe->pp_buf_id_next = (abe->pp_buf_id_next + 1) & 0x03;

	base_and_size = (base_and_size & 0xFFFFL) + (n_samples << 16);

	addr.offset += struct_offset;
	addr.bytes = 4;
	omap_aess_mem_write(abe, addr, (u32 *) &base_and_size);

	return 0;
}
EXPORT_SYMBOL(omap_aess_set_ping_pong_buffer);

/**
 * omap_aess_read_next_ping_pong_buffer
 * @abe: Pointer on abe handle
 * @port: ABE portID
 * @p: Next buffer address (pointer)
 * @n: Next buffer size (pointer)
 *
 * Tell the next base address of the next ping_pong Buffer and its size
 */
int omap_aess_read_next_ping_pong_buffer(struct omap_aess *abe, u32 port, u32 *p, u32 *n)
{
	struct ABE_SPingPongDescriptor desc_pp;
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (port != OMAP_ABE_MM_DL_PORT) {
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	}
	/* read the port SIO descriptor and extract the current pointer
	   address after reading the counter */
	memcpy(&addr, &omap_aess_map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
		sizeof(struct omap_aess_addr));
	addr.bytes = sizeof(struct ABE_SPingPongDescriptor);
	omap_aess_mem_read(abe, addr, (u32 *) &desc_pp);

	if ((desc_pp.counter & 0x1) == 0) {
		*p = desc_pp.nextbuff0_BaseAddr;
	} else {
		*p = desc_pp.nextbuff1_BaseAddr;
	}
	/* translates the number of samples in bytes */
	*n = abe->size_pingpong;

	return 0;
}
EXPORT_SYMBOL(omap_aess_read_next_ping_pong_buffer);

/**
 * omap_aess_init_ping_pong_buffer
 * @abe: Pointer on abe handle
 * @id: ABE port ID
 * @size_bytes:size of the ping pong
 * @n_buffers:number of buffers (2 = ping/pong)
 * @p:returned address of the ping-pong list of base addresses
 *	(byte offset from DMEM start)
 *
 * Computes the base address of the ping_pong buffers
 */
int omap_aess_init_ping_pong_buffer(struct omap_aess *abe,
				   u32 id, u32 size_bytes, u32 n_buffers,
				   u32 *p)
{
	u32 i, dmem_addr;
	struct omap_aess_addr addr;

	/* ping_pong is supported in 2 buffers configuration right now but FW
	   is ready for ping/pong/pung/pang... */
	if (id != OMAP_ABE_MM_DL_PORT || n_buffers > MAX_PINGPONG_BUFFERS) {
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	}

	memcpy(&addr, &omap_aess_map[OMAP_AESS_DMEM_PING_ID],
		sizeof(struct omap_aess_addr));

	for (i = 0; i < n_buffers; i++) {
		dmem_addr = addr.offset + (i * size_bytes);
		/* base addresses of the ping pong buffers in U8 unit */
		abe->base_address_pingpong[i] = dmem_addr;
	}

	for (i = 0; i < 4; i++)
		abe->pp_buf_addr[i] = addr.offset + (i * size_bytes);
	abe->pp_buf_id = 0;
	abe->pp_buf_id_next = 0;
	abe->pp_first_irq = 1;

	/* global data */
	abe->size_pingpong = size_bytes;
	*p = (u32) addr.offset;
	return 0;
}
EXPORT_SYMBOL(omap_aess_init_ping_pong_buffer);

/**
 * omap_aess_read_offset_from_ping_buffer
 * @abe: Pointer on abe handle
 * @id: ABE port ID
 * @n:  returned address of the offset
 *	from the ping buffer start address (in samples)
 *
 * Computes the current firmware ping pong read pointer location,
 * expressed in samples, as the offset from the start address of ping buffer.
 */
int omap_aess_read_offset_from_ping_buffer(struct omap_aess *abe,
					  u32 id, u32 *n)
{
	struct ABE_SPingPongDescriptor desc_pp;
	struct omap_aess_addr addr;

	/* ping_pong is only supported on MM_DL */
	if (OMAP_ABE_MM_DL_PORT != id) {
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_PARAMETER_ERROR);
	} else {
		/* read the port SIO ping pong descriptor */
		memcpy(&addr, &omap_aess_map[OMAP_AESS_DMEM_PINGPONGDESC_ID],
			sizeof(struct omap_aess_addr));
		addr.bytes = sizeof(struct ABE_SPingPongDescriptor);
		omap_aess_mem_read(abe, addr, (u32 *) &desc_pp);
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
		case MONO_MSB:
		case MONO_RSHIFTED_16:
		case STEREO_16_16:
			*n +=  abe->pp_buf_id * abe->size_pingpong / 4;
			break;
		case STEREO_MSB:
		case STEREO_RSHIFTED_16:
			*n += abe->pp_buf_id * abe->size_pingpong / 8;
			break;
		default:
			omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
					   ABE_PARAMETER_ERROR);
			return -EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(omap_aess_read_offset_from_ping_buffer);

/**
 * abe_set_router_configuration
 * @Id: name of the router
 * @Conf: id of the configuration
 * @param: list of output index of the route
 *
 * The uplink router takes its input from DMIC (6 samples), AMIC (2 samples)
 * and PORT1/2 (2 stereo ports). Each sample will be individually stored in
 * an intermediate table of 10 elements.
 *
 * Example of router table parameter for voice uplink with phoenix microphones
 *
 * indexes 0 .. 9 = MM_UL description (digital MICs and MMEXTIN)
 *	DMIC1_L_labelID, DMIC1_R_labelID, DMIC2_L_labelID, DMIC2_R_labelID,
 *	MM_EXT_IN_L_labelID, MM_EXT_IN_R_labelID, ZERO_labelID, ZERO_labelID,
 *	ZERO_labelID, ZERO_labelID,
 * indexes 10 .. 11 = MM_UL2 description (recording on DMIC3)
 *	DMIC3_L_labelID, DMIC3_R_labelID,
 * indexes 12 .. 13 = VX_UL description (VXUL based on PDMUL data)
 *	AMIC_L_labelID, AMIC_R_labelID,
 * indexes 14 .. 15 = RESERVED (NULL)
 *	ZERO_labelID, ZERO_labelID,
 */
int omap_aess_set_router_configuration(struct omap_aess *abe,
				      u32 id, u32 k, u32 *param)
{
	omap_aess_mem_write(abe,
		omap_aess_map[OMAP_AESS_DMEM_AUPLINKROUTING_ID],
		param);
	return 0;
}
EXPORT_SYMBOL(omap_aess_set_router_configuration);

/**
 * abe_set_opp_processing - Set OPP mode for ABE Firmware
 * @opp: OOPP mode
 *
 * New processing network and OPP:
 * 0: Ultra Lowest power consumption audio player (no post-processing, no mixer)
 * 1: OPP 25% (simple multimedia features, including low-power player)
 * 2: OPP 50% (multimedia and voice calls)
 * 3: OPP100% ( multimedia complex use-cases)
 *
 * Rearranges the FW task network to the corresponding OPP list of features.
 * The corresponding AE ports are supposed to be set/reset accordingly before
 * this switch.
 *
 */
int omap_aess_set_opp_processing(struct omap_aess *abe, u32 opp)
{
	u32 dOppMode32;

	switch (opp) {
	case ABE_OPP25:
		/* OPP25% */
		dOppMode32 = DOPPMODE32_OPP25;
		break;
	case ABE_OPP50:
		/* OPP50% */
		dOppMode32 = DOPPMODE32_OPP50;
		break;
	default:
		omap_aess_dbg_error(abe, OMAP_ABE_ERR_API,
				   ABE_BLOCK_COPY_ERR);
	case ABE_OPP100:
		/* OPP100% */
		dOppMode32 = DOPPMODE32_OPP100;
		break;
	}
	/* Write Multiframe inside DMEM */
	omap_aess_mem_write(abe,
		omap_aess_map[OMAP_AESS_DMEM_MAXTASKBYTESINSLOT_ID], &dOppMode32);

	return 0;

}
EXPORT_SYMBOL(omap_aess_set_opp_processing);

/**
 * omap_aess_check_activity - Check if some ABE activity.
 *
 * Check if any ABE ports are running.
 * return 1: still activity on ABE
 * return 0: no more activity on ABE. Event generator can be stopped
 *
 */
int omap_aess_check_activity(struct omap_aess *abe)
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
