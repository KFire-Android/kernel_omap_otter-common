/*
 * OMAP Remote Procedure Call Driver.
 *
 * Copyright(c) 2012 Texas Instruments. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in
 *	 the documentation and/or other materials provided with the
 *	 distribution.
 * * Neither the name Texas Instruments nor the names of its
 *	 contributors may be used to endorse or promote products derived
 *	 from this software without specific prior written permission.
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
 */

#include "omap_rpc_internal.h"

static inline long tiler_stride(phys_addr_t lpa)
{
	/*
	 * The access mode decoding is as follows:
	 *
	 * 0x60000000 - 0x67FFFFFF : 8-bit
	 * 0x68000000 - 0x6FFFFFFF : 16-bit
	 * 0x70000000 - 0x77FFFFFF : 32-bit
	 * 0x77000000 - 0x7FFFFFFF : Page mode
	 */
	switch (lpa & 0xf8000000) {	/* Mask out the lower bits */
	case 0x60000000:		/* 8-bit  */
		return 0x4000;		/* 16 KB of stride */
	case 0x68000000:		/* 16-bit */
	case 0x70000000:		/* 32-bit */
		return 0x8000;		/* 32 KB of stride */
	default:
		return 0;
	}
}

long omaprpc_recalc_off(phys_addr_t lpa, long uoff)
{
	long stride = tiler_stride(lpa);
	return (stride != 0) ? (stride*(uoff/PAGE_SIZE)) +
		(uoff & (PAGE_SIZE-1)) : uoff;
}

