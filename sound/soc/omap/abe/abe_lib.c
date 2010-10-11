/*
 * ALSA SoC OMAP ABE driver
 *
 * Author:	Laurent Le Faucheur <l-le-faucheur@ti.com>
 * 	Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#include "abe_main.h"
#if PC_SIMULATION
#include <stdlib.h>
#endif
#define ABE_PMEM_BASE_OFFSET_MPU	0xe0000
#define ABE_CMEM_BASE_OFFSET_MPU	0xa0000
#define ABE_SMEM_BASE_OFFSET_MPU	0xc0000
#define ABE_DMEM_BASE_OFFSET_MPU	0x80000
#define ABE_ATC_BASE_OFFSET_MPU		0xf1000
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
void __iomem *io_base;
/**
 * abe_init_mem - Allocate Kernel space memory map for ABE
 *
 * Memory map of ABE memory space for PMEM/DMEM/SMEM/DMEM
 */
void abe_init_mem(void __iomem *_io_base)
{
	io_base = _io_base;
}
/**
* abe_fprintf
* @line: character line to be printed
*
* Print ABE debug messages.
*/
/**
 * abe_read_feature_from_port
 * @x: d
 *
 * TBD
 *
 */
void abe_read_feature_from_port(u32 x)
{
}
/**
 * abe_write_feature_to_port
 * @x: d
 *
 * TBD
 *
 */
void abe_write_feature_to_port(u32 x)
{
}
/**
 * abe_read_fifo
 * @x: d
 *
 * TBD
 */
void abe_read_fifo(u32 x)
{
}
/**
 * abe_write_fifo
 * @mem_bank: currently only ABE_DMEM supported
 * @addr: FIFO descriptor address ( descriptor fields : READ ptr, WRITE ptr,
 * FIFO START_ADDR, FIFO END_ADDR)
 * @data: data to write to FIFO
 * @number: number of 32-bit words to write to DMEM FIFO
 *
 * write DMEM FIFO and update FIFO descriptor, it is assumed that FIFO descriptor
 * is located in DMEM
 */
void abe_write_fifo(u32 memory_bank, u32 descr_addr, u32 *data, u32 nb_data32)
{
	u32 fifo_addr[4];
	u32 i;
	/* read FIFO descriptor from DMEM */
	abe_block_copy(COPY_FROM_ABE_TO_HOST, ABE_DMEM, descr_addr,
		       &fifo_addr[0], 4 * sizeof(u32));
	/* WRITE ptr < FIFO start address */
	if (fifo_addr[1] < fifo_addr[2])
		abe_dbg_error_log(ABE_FW_FIFO_WRITE_PTR_ERR);
	/* WRITE ptr > FIFO end address */
	if (fifo_addr[1] > fifo_addr[3])
		abe_dbg_error_log(ABE_FW_FIFO_WRITE_PTR_ERR);
	switch (memory_bank) {
	case ABE_DMEM:
		for (i = 0; i < nb_data32; i++) {
			abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM,
				       (s32) fifo_addr[1], (u32 *) (data + i),
				       4);
			/* increment WRITE pointer */
			fifo_addr[1] = fifo_addr[1] + 4;
			if (fifo_addr[1] > fifo_addr[3])
				fifo_addr[1] = fifo_addr[2];
			if (fifo_addr[1] == fifo_addr[0])
				abe_dbg_error_log(ABE_FW_FIFO_WRITE_PTR_ERR);
		}
		/* update WRITE pointer in DMEM */
		abe_block_copy(COPY_FROM_HOST_TO_ABE, ABE_DMEM, descr_addr +
			       sizeof(u32), &fifo_addr[1], 4);
		break;
	default:
		/* printf("currently only DMEM FIFO write supported ERROR\n"); */
		break;
	}
}
/**
 * abe_block_copy
 * @direction: direction of the data move (Read/Write)
 * @memory_bamk:memory bank among PMEM, DMEM, CMEM, SMEM, ATC/IO
 * @address: address of the memory copy (byte addressing)
 * @data: pointer to the data to transfer
 * @nb_bytes: number of data to move
 *
 * Memory transfer to/from ABE to MPU
 */
void abe_block_copy(u32 direction, u32 memory_bank, u32 address,
		    u32 *data, u32 nb_bytes)
{
	u32 i;
	u32 base_address = 0, *src_ptr, *dst_ptr, n;
	switch (memory_bank) {
	case ABE_PMEM:
		base_address = (u32) io_base + ABE_PMEM_BASE_OFFSET_MPU;
		break;
	case ABE_CMEM:
		base_address = (u32) io_base + ABE_CMEM_BASE_OFFSET_MPU;
		break;
	case ABE_SMEM:
		base_address = (u32) io_base + ABE_SMEM_BASE_OFFSET_MPU;
		break;
	case ABE_DMEM:
		base_address = (u32) io_base + ABE_DMEM_BASE_OFFSET_MPU;
		break;
	case ABE_ATC:
		base_address = (u32) io_base + ABE_ATC_BASE_OFFSET_MPU;
		break;
	default:
		base_address = (u32) io_base + ABE_SMEM_BASE_OFFSET_MPU;
		abe_dbg_param |= ERR_LIB;
		abe_dbg_error_log(ABE_BLOCK_COPY_ERR);
		break;
	}
	if (direction == COPY_FROM_HOST_TO_ABE) {
		dst_ptr = (u32 *) (base_address + address);
		src_ptr = (u32 *) data;
	} else {
		dst_ptr = (u32 *) data;
		src_ptr = (u32 *) (base_address + address);
	}
	n = (nb_bytes / 4);
	for (i = 0; i < n; i++)
		*dst_ptr++ = *src_ptr++;
}
#if 0
/*
 * ABE_SINGLE_COPY
 *
 * Parameter :
 * address of the memory copy (byte addressing)
 * long pointer to the data
 * number of data to move
 *
 * Operations :
 * 32bits data move
 *
 * Return value :
 * none
 */
void abe_write_dmem(u32 address, u32 *data, u32 nb_bytes)
     void abe_read_dmem(u32 address, u32 *data, u32 nb_bytes)
     void abe_write_cmem(u32 address, u32 *data, u32 nb_bytes)
     void abe_read_cmem(u32 address, u32 *data, u32 nb_bytes)
     void abe_write_smem(u32 address, u32 *data, u32 nb_bytes)
     void abe_read_smem(u32 address, u32 *data, u32 nb_bytes)
     void abe_write_atc(u32 address, u32 *data, u32 nb_bytes)
     void abe_read_atc(u32 address, u32 *data, u32 nb_bytes)
#endif
/**
 * abe_reset_mem
 *
 * @memory_bank: memory bank among DMEM, SMEM
 * @address: address of the memory copy (byte addressing)
 * @nb_bytes: number of data to move
 *
 * Reset ABE memory
 */
     void abe_reset_mem(u32 memory_bank, u32 address, u32 nb_bytes)
{
	u32 i;
	u32 *dst_ptr, n;
	u32 base_address = 0;
	switch (memory_bank) {
	case ABE_SMEM:
		base_address = (u32) io_base + ABE_SMEM_BASE_OFFSET_MPU;
		break;
	case ABE_DMEM:
		base_address = (u32) io_base + ABE_DMEM_BASE_OFFSET_MPU;
		break;
	case ABE_CMEM:
		base_address = (u32) io_base + ABE_CMEM_BASE_OFFSET_MPU;
		break;
	}
	dst_ptr = (u32 *) (base_address + address);
	n = (nb_bytes / 4);
	for (i = 0; i < n; i++)
		*dst_ptr++ = 0;
}
/**
 * abe_monitoring
 *
 * checks the internal status of ABE and HAL
 */
void abe_monitoring(void)
{
	abe_dbg_param = 0;
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
void abe_format_switch(abe_data_format_t *f, u32 *iter, u32 *mulfac)
{
	u32 n_freq;
#if FW_SCHED_LOOP_FREQ==4000
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
	default/*case 48000 */ :
		n_freq = 12;
		break;
	}
#else
	/* erroneous cases */
	n_freq = 0;
#endif
	switch (f->samp_format) {
	case MONO_MSB:
	case MONO_RSHIFTED_16:
	case STEREO_16_16:
		*mulfac = 1;
		break;
	case STEREO_MSB:
	case STEREO_RSHIFTED_16:
		*mulfac = 2;
		break;
	case THREE_MSB:
		*mulfac = 3;
		break;
	case FOUR_MSB:
		*mulfac = 4;
		break;
	case FIVE_MSB:
		*mulfac = 5;
		break;
	case SIX_MSB:
		*mulfac = 6;
		break;
	case SEVEN_MSB:
		*mulfac = 7;
		break;
	case EIGHT_MSB:
		*mulfac = 8;
		break;
	case NINE_MSB:
		*mulfac = 9;
		break;
	default:
		*mulfac = 1;
		break;
	}
	*iter = (n_freq * (*mulfac));
}
/**
 * abe_dma_port_iteration
 * @f: port format
 *
 * translates the sampling and data length to ITER number for the DMA
 */
u32 abe_dma_port_iteration(abe_data_format_t *f)
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
u32 abe_dma_port_iter_factor(abe_data_format_t *f)
{
	u32 iter, mulfac;
	abe_format_switch(f, &iter, &mulfac);
	return mulfac;
}
/**
 * abe_dma_port_copy_subroutine_id
 *
 * @port_id: ABE port ID
 *
 * returns the index of the function doing the copy in I/O tasks
 */
u32 abe_dma_port_copy_subroutine_id(u32 port_id)
{
	u32 sub_id;
	if (abe_port[port_id].protocol.direction == ABE_ATC_DIRECTION_IN) {
		switch (abe_port[port_id].format.samp_format) {
		case MONO_MSB:
			sub_id = D2S_MONO_MSB_CFPID;
			break;
		case MONO_RSHIFTED_16:
			sub_id = D2S_MONO_RSHIFTED_16_CFPID;
			break;
		case STEREO_RSHIFTED_16:
			sub_id = D2S_STEREO_RSHIFTED_16_CFPID;
			break;
		case STEREO_16_16:
			sub_id = D2S_STEREO_16_16_CFPID;
			break;
		case STEREO_MSB:
			sub_id = D2S_STEREO_MSB_CFPID;
			break;
		case SIX_MSB:
			if (port_id == DMIC_PORT) {
				sub_id = COPY_DMIC_CFPID;
				break;
			}
		default:
			sub_id = NULL_COPY_CFPID;
			break;
		}
	} else {
		switch (abe_port[port_id].format.samp_format) {
		case MONO_MSB:
			sub_id = S2D_MONO_MSB_CFPID;
			break;
		case MONO_RSHIFTED_16:
			sub_id = S2D_MONO_RSHIFTED_16_CFPID;
			break;
		case STEREO_RSHIFTED_16:
			sub_id = S2D_STEREO_RSHIFTED_16_CFPID;
			break;
		case STEREO_16_16:
			sub_id = S2D_STEREO_16_16_CFPID;
			break;
		case STEREO_MSB:
			sub_id = S2D_STEREO_MSB_CFPID;
			break;
		case SIX_MSB:
			if (port_id == PDM_DL_PORT) {
				sub_id = COPY_MCPDM_DL_CFPID;
				break;
			}
			if (port_id == MM_UL_PORT) {
				sub_id = COPY_MM_UL_CFPID;
				break;
			}
		case THREE_MSB:
		case FOUR_MSB:
		case FIVE_MSB:
		case SEVEN_MSB:
		case EIGHT_MSB:
		case NINE_MSB:
			sub_id = COPY_MM_UL_CFPID;
			break;
		default:
			sub_id = NULL_COPY_CFPID;
			break;
		}
	}
	return sub_id;
}
/**
 * abe_int_2_float
 * returns a mantissa on 16 bits and the exponent
 * 0x4000.0000 leads to M=0x4000 X=15
 * 0x0004.0000 leads to M=0x4000 X=4
 * 0x0000.0001 leads to M=0x4000 X=-14
 *
 */
void abe_int_2_float16(u32 data, u32 *mantissa, u32 *exp)
{
	u32 i;
	*exp = 0;
	*mantissa = 0;
	for (i = 0; i < 32; i++) {
		if ((1 << i) > data)
			break;
	}
	*exp = i - 15;
	*mantissa = (*exp > 0) ? data >> (*exp) : data << (*exp);
}
/**
 * abe_gain_offset
 * returns the offset to firmware data structures
 *
 */
void abe_gain_offset(u32 id, u32 *mixer_offset)
{
	switch (id) {
	default:
	case GAINS_DMIC1:
		*mixer_offset = dmic1_gains_offset;
		break;
	case GAINS_DMIC2:
		*mixer_offset = dmic2_gains_offset;
		break;
	case GAINS_DMIC3:
		*mixer_offset = dmic3_gains_offset;
		break;
	case GAINS_AMIC:
		*mixer_offset = amic_gains_offset;
		break;
	case GAINS_DL1:
		*mixer_offset = dl1_gains_offset;
		break;
	case GAINS_DL2:
		*mixer_offset = dl2_gains_offset;
		break;
	case GAINS_SPLIT:
		*mixer_offset = splitters_gains_offset;
		break;
	case MIXDL1:
		*mixer_offset = mixer_dl1_offset;
		break;
	case MIXDL2:
		*mixer_offset = mixer_dl2_offset;
		break;
	case MIXECHO:
		*mixer_offset = mixer_echo_offset;
		break;
	case MIXSDT:
		*mixer_offset = mixer_sdt_offset;
		break;
	case MIXVXREC:
		*mixer_offset = mixer_vxrec_offset;
		break;
	case MIXAUDUL:
		*mixer_offset = mixer_audul_offset;
		break;
	}
}
/**
 * abe_decide_main_port - Select stynchronization port for Event generator.
 * @id: audio port name
 *
 * tells the FW which is the reference stream for adjusting
 * the processing on 23/24/25 slots
 *
 * takes the first port in a list which is slave on the data interface
 */
u32 abe_valid_port_for_synchro(u32 id)
{
	if ((abe_port[id].protocol.protocol_switch == DMAREQ_PORT_PROT) ||
		(abe_port[id].protocol.protocol_switch == PINGPONG_PORT_PROT))
		return 0;
	else
		return 1;
}
void abe_decide_main_port(u32 id)
{
	u32 i;

	if (abe_valid_port_for_synchro(id)) {
		for (i = 0; i < (LAST_PORT_ID - 1); i++) {
			if (abe_port[abe_port_priority[i]].status ==
					OMAP_ABE_PORT_ACTIVITY_RUNNING)
				break;
		}

		if (i < (LAST_PORT_ID - 1))
			id = abe_port_priority[i];
		abe_select_main_port(id);
	}
}
