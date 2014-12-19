/*
 * OMAP Remote Procedure Call Driver.
 *
 * Copyright(c) 2012 Texas Instruments. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "omap_rpc_internal.h"

/**
 * tiler_stride_from_region() - Returns the stride value as seen by remote cores
 * based on the local address given to the function. This stride value is valid
 * for non-local cores only.
 * @localphys:	The local physical address.
 **/
static inline long tiler_stride_from_region(phys_addr_t localphys)
{
	/*
	 * The physical address range decoding of local addresses is as follows:
	 *
	 * 0x60000000 - 0x67FFFFFF : 8-bit region
	 * 0x68000000 - 0x6FFFFFFF : 16-bit region
	 * 0x70000000 - 0x77FFFFFF : 32-bit region
	 * 0x77000000 - 0x7FFFFFFF : Page mode region
	 */
	switch (localphys & 0xf8000000) {	/* Mask out the lower bits */
	case 0x60000000:	/* 8-bit  */
		return 0x4000;	/* 16 KB of stride */
	case 0x68000000:	/* 16-bit */
	case 0x70000000:	/* 32-bit */
		return 0x8000;	/* 32 KB of stride */
	default:
		return 0;
	}
}

/**
 * omaprpc_recalc_off() - Recalculated the unsigned offset in a buffer due to
 * it's location in the TILER.
 * @lpa:	local physical address
 * @uoff:	unsigned offset
 */
long omaprpc_recalc_off(phys_addr_t lpa, long uoff)
{
	long stride = tiler_stride_from_region(lpa);
	return (stride != 0) ? (stride * (uoff / PAGE_SIZE)) +
	    (uoff & (PAGE_SIZE - 1)) : uoff;
}
