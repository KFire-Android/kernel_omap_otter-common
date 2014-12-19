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
	if (!rproc)
		pr_err("error getting rproc id %p\n",
		       rpc->rpcserv->rpdev);
	else {
		ret = rproc_pa_to_da(rproc, pa, &da);
		if (ret) {
			pr_err("error from rproc_pa_to_da %d\n", ret);
			da = 0;
		}
	}
	/*Revisit if remote address size increases */
	rpa = (phys_addr_t) da;

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
};

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

static u32 numCores = sizeof(regions) / sizeof(regions[0]);

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

