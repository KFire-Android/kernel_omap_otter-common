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
#include "abe_main.h"
#include "abe_ref.h"

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
		base_address = (u32) abe->io_base + ABE_PMEM_BASE_OFFSET_MPU;
		break;
	case ABE_CMEM:
		base_address = (u32) abe->io_base + ABE_CMEM_BASE_OFFSET_MPU;
		break;
	case ABE_SMEM:
		base_address = (u32) abe->io_base + ABE_SMEM_BASE_OFFSET_MPU;
		break;
	case ABE_DMEM:
		base_address = (u32) abe->io_base + ABE_DMEM_BASE_OFFSET_MPU;
		break;
	case ABE_ATC:
		base_address = (u32) abe->io_base + ABE_ATC_BASE_OFFSET_MPU;
		break;
	default:
		base_address = (u32) abe->io_base + ABE_SMEM_BASE_OFFSET_MPU;
		abe->dbg_param |= ERR_LIB;
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
		base_address = (u32) abe->io_base + ABE_SMEM_BASE_OFFSET_MPU;
		break;
	case ABE_DMEM:
		base_address = (u32) abe->io_base + ABE_DMEM_BASE_OFFSET_MPU;
		break;
	case ABE_CMEM:
		base_address = (u32) abe->io_base + ABE_CMEM_BASE_OFFSET_MPU;
		break;
	}
	dst_ptr = (u32 *) (base_address + address);
	n = (nb_bytes / 4);
	for (i = 0; i < n; i++)
		*dst_ptr++ = 0;
}
