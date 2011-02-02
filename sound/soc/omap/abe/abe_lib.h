/*
 * ALSA SoC OMAP ABE driver
 *
 * Author:	Laurent Le Faucheur <l-le-faucheur@ti.com>
 *		Liam Girdwood <lrg@slimlogic.co.uk>
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

/**
 * abe_fprintf
 *
 *  Parameter  :
 *      character line to be printed
 *
 *  Operations :
 *
 *  Return value :
 *	none
 */
void abe_fprintf(char *line);

/*
 *  ABE_READ_FEATURE_FROM_PORT
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *
 *  Return value :
 *	none
 */
void abe_read_feature_from_port(u32 x);

/*
 *  ABE_WRITE_FEATURE_TO_PORT
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *  Return value :
 *	none
 */
void abe_write_feature_to_port(u32 x);
/*
 *  ABE_READ_FIFO
 *
 *  Parameter  :
 *      x : d
 *
 *  Operations :
 *
 *  Return value :
 *	none
 */
void abe_read_fifo(u32 x);

/*
 *  ABE_WRITE_FIFO
 *
 *  Parameter  :
 *      mem_bank : currently only ABE_DMEM supported
 *	addr : FIFO descriptor address ( descriptor fields : READ ptr,
 *	WRITE ptr, FIFO START_ADDR, FIFO END_ADDR)
 *	data to write to FIFO
 *	number of 32-bit words to write to DMEM FIFO
 *
 *  Operations :
 *	write DMEM FIFO and update FIFO descriptor, it is assumed that FIFO
 *	descriptor is located in DMEM
 *
 *  Return value :
 *	none
 */
void abe_write_fifo(u32 mem_bank, u32 addr, u32 *data, u32 nb_data32);

/*
 *  ABE_BLOCK_COPY
 *
 *  Parameter  :
 *      direction of the data move (Read/Write)
 *      memory bank among PMEM, DMEM, CMEM, SMEM, ATC/IO
 *      address of the memory copy (byte addressing)
 *      long pointer to the data
 *      number of data to move
 *
 *  Operations :
 *      block data move
 *
 *  Return value :
 *	none
 */
void abe_block_copy(u32 direction, u32 memory_bank, u32 address, u32 *data,
		    u32 nb);

/*
 *  ABE_RESET_MEM
 *
 *  Parameter  :
 *      memory bank among DMEM, SMEM
 *      address of the memory copy (byte addressing)
 *      number of data to move
 *
 *  Operations :
 *      reset memory
 *
 *  Return value :
 *      none
 */
void abe_reset_mem(u32 memory_bank, u32 address, u32 nb_bytes);
