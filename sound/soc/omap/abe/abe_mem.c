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

#define ABE_PMEM_BASE_OFFSET_MPU	0xe0000
#define ABE_CMEM_BASE_OFFSET_MPU	0xa0000
#define ABE_SMEM_BASE_OFFSET_MPU	0xc0000
#define ABE_DMEM_BASE_OFFSET_MPU	0x80000
#define ABE_ATC_BASE_OFFSET_MPU		0xf1000

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
 * abe_check_activity - Check if some ABE activity.
 *
 * Check if any ABE ports are running.
 * return 1: still activity on ABE
 * return 0: no more activity on ABE. Event generator can be stopped
 *
 */
u32 abe_check_activity(void)
{
	u32 i;
	u32 ret;

	ret = 0;
	for (i = 0; i < (LAST_PORT_ID - 1); i++) {
		if (abe_port[abe_port_priority[i]].status ==
				OMAP_ABE_PORT_ACTIVITY_RUNNING)
			break;
	}
	if (i < (LAST_PORT_ID - 1))
		ret = 1;

	return ret;
}
