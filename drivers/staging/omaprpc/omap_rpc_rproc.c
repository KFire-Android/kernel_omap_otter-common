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

#if defined(OMAPRPC_USE_RPROC_LOOKUP)

phys_addr_t rpmsg_local_to_remote_pa(struct omaprpc_instance_t *rpc,
					phys_addr_t pa)
{
	int ret;
	struct rproc *rproc;
	u64 da;
	phys_addr_t rpa;

	if (mutex_lock_interruptible(&rpc->rpcserv->lock))
		return 0;

	rproc = rpmsg_get_rproc_handle(rpc->rpcserv->rpdev);
	ret = rproc_pa_to_da(rproc, pa, &da);
	if (ret) {
		pr_err("error from rproc_pa_to_da %d\n", ret);
		da = 0;
	}

	/*Revisit if remote address size increases */
	rpa = (phys_addr_t)da;

	mutex_unlock(&rpc->rpcserv->lock);
	return rpa;

}

#else

struct remote_mmu_region_t {
	phys_addr_t tiler_start;
	phys_addr_t tiler_end;
	phys_addr_t ion_1d_start;
	phys_addr_t ion_1d_end;
	phys_addr_t ion_1d_va;
} ;

static struct remote_mmu_region_t regions[OMAPRPC_CORE_REMOTE_MAX] = {
	/* Tesla */
	{0x60000000, 0x80000000, 0xBA300000, 0xBFD00000, 0x88000000},
	/* SIMCOP */
	{0x60000000, 0x80000000, 0xBA300000, 0xBFD00000, 0x88000000},
	/* MCU0 */
	{0x60000000, 0x80000000, 0xBA300000, 0xBFD00000, 0x88000000},
	/* MCU1 */
	{0x60000000, 0x80000000, 0xBA300000, 0xBFD00000, 0x88000000},
	/* EVE */
	{0x60000000, 0x80000000, 0xBA300000, 0xBFD00000, 0x88000000},
};
static u32 numCores = sizeof(regions)/sizeof(regions[0]);

phys_addr_t rpmsg_local_to_remote_pa(uint32_t core, phys_addr_t pa)
{
	if (core < numCores) {
		if (regions[core].tiler_start <= pa &&
			pa < regions[core].tiler_end)
			return pa;
		else if (regions[core].ion_1d_start <= pa &&
			pa < regions[core].ion_1d_end)
			return (pa - regions[core].ion_1d_start) +
				regions[core].ion_1d_va;
	}
	return 0;
}

#endif


