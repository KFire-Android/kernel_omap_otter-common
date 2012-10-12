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

static uint8_t *omaprpc_map_parameter(struct omaprpc_instance_t *rpc,
				       struct omaprpc_param_t *param)
{
	uint32_t pri_offset = 0;
	uint8_t *kva = NULL;
	uint8_t *bkva = NULL;

	/* calc any primary offset if present */
	pri_offset = param->data - param->base;

	bkva = (uint8_t *) ion_map_kernel(rpc->ion_client,
					  (struct ion_handle *)param->reserved);

	/*
	 * set the kernel VA equal to the base kernel
	 * VA plus the primary offset
	 */
	kva = &bkva[pri_offset];

	/*
	 * in ION case, secondary offset is ignored here
	 * because the entire region is mapped.
	 */
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "Mapped UVA:%p to KVA:%p+OFF:%08x SIZE:%08x "
		      "(MKVA:%p to END:%p)\n",
		      (void *)param->data,
		      (void *)kva, pri_offset, param->size,
		      (void *)bkva, (void *)&bkva[param->size]);

	return kva;

}

static void omaprpc_unmap_parameter(struct omaprpc_instance_t *rpc,
				     struct omaprpc_param_t *param,
				     uint8_t *ptr, uint32_t sec_offset)
{
	ion_unmap_kernel(rpc->ion_client, (struct ion_handle *)param->reserved);
}

phys_addr_t omaprpc_buffer_lookup(struct omaprpc_instance_t *rpc,
				  uint32_t core, virt_addr_t uva,
				  virt_addr_t buva, void *reserved)
{
	phys_addr_t lpa = 0, rpa = 0;
	/* User VA - Base User VA = User Offset assuming not tiler 2D */
	/* For Tiler2D offset is corrected later */
	long uoff = uva - buva;

	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "CORE=%u BUVA=%p UVA=%p Uoff=%ld [0x%016lx] Hdl=%p\n",
		      core, (void *)buva, (void *)uva, uoff, (ulong) uoff,
		      reserved);

	if (uoff < 0) {
		OMAPRPC_ERR(rpc->rpcserv->dev,
			    "Offsets calculation for BUVA=%p from UVA=%p is a "
			    "negative number. Bad parameters!\n",
			    (void *)buva, (void *)uva);
		rpa = 0;
		goto to_end;
	}

	if (reserved) {
		struct ion_handle *handle;
		ion_phys_addr_t paddr;
		size_t len;

		/* is it an ion handle? */
		handle = (struct ion_handle *)reserved;
		if (!ion_phys(rpc->ion_client, handle, &paddr, &len)) {
			lpa = (phys_addr_t) paddr;
			OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
				"Handle %p is an ION Handle to ARM PA %p "
				"(Uoff=%ld) (len=%zu)\n", reserved, (void *)lpa,
				 uoff, len);
			if ((long)len < uoff) {
				/* if the offset is larger than the length,
				 * there is a potential security issue.
				 */
				pr_err("Offset %ld too large, must be < %zu\n",
					uoff, len);
				rpa = 0;
				goto to_end;
			}
			/* recalculate for 2d strides */
			uoff = omaprpc_recalc_off(lpa, uoff);
			lpa += uoff;
			goto to_va;
		} else {
			/* is it an pvr buffer wrapping an ion handle? */

			/*
			 * TODO: need to support 2 ion handles
			 * per 1 pvr handle (NV12 case)
			 */
			struct ion_buffer *ion_buffer = NULL;
			int num_handles = 1;
			handle = NULL;
			if (omap_ion_share_fd_to_buffers((int)reserved,
							 &ion_buffer,
							 &num_handles) < 0) {
				goto to_va;
			}
			if (ion_buffer) {
				handle = ion_import(rpc->ion_client,
						    ion_buffer);
			}
			if (handle && !ion_phys(rpc->ion_client, handle,
						&paddr, &len)) {
				lpa = (phys_addr_t) paddr;
				OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
					rpc->rpcserv->dev,
					"FD %d is an PVR Handle to ARM PA %p "
					"(Uoff=%ld)\n", (int)reserved,
					(void *)lpa, uoff);
				if ((long)len < uoff) {
					/* if the offset is larger than the
					 * length there is a security issue.
					 */
					pr_err(
					"Offset %ld too large, must be < %zu\n",
						uoff, len);
					rpa = 0;
					goto to_end;
				}
				uoff = omaprpc_recalc_off(lpa, uoff);
				lpa += uoff;
				goto to_va;
			}
			/*
			 * TODO: need to do some buffer tracking and call
			 * ion_free when it is no longer being used. this
			 * will make sure the buffer is not freed while
			 * we are still using it
			 */
			if (handle)
				ion_free(rpc->ion_client, handle);
		}
	}
	/* As a last resort ask tiler if it knows about this virtual address */
	lpa = (phys_addr_t)tiler_virt2phys(uva);
to_va:
	/* convert the local physical address to remote physical address */
	rpa = rpmsg_local_to_remote_pa(rpc, lpa);
to_end:
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "ARM VA %p == ARM PA %p => REMOTE[%u] PA %p (RESV %p)\n",
		      (void *)uva, (void *)lpa, core, (void *)rpa, reserved);
	return rpa;
}

int omaprpc_xlate_buffers(struct omaprpc_instance_t *rpc,
			  struct omaprpc_call_function_t *function,
			  int direction)
{
	int idx = 0, start = 0, inc = 1, limit = 0, ret = 0;
	uint32_t ptr_idx = 0, offset = 0, size = 0;
	/* not all the parameters are pointers so this may be sparse */
	uint8_t *base_ptrs[OMAPRPC_MAX_PARAMETERS];

	if (function->num_translations == 0)
		return 0;

	limit = function->num_translations;
	memset(base_ptrs, 0, sizeof(base_ptrs));
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "Operating on %d pointers\n", function->num_translations);
	/*
	 * we may have a failure during translation, in which case we
	 * need to unwind the whole operation from here
	 */
restart:
	for (idx = start; idx != limit; idx += inc) {
		/* conveinence variables */
		ptr_idx = function->translations[idx].index;
		offset = function->translations[idx].offset;

		/* if the pointer index for this translation is invalid */
		if (ptr_idx >= OMAPRPC_MAX_PARAMETERS) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Invalid parameter pointer index %u\n",
				    ptr_idx);
			goto unwind;
		} else if (function->params[ptr_idx].type !=
			   OMAPRPC_PARAM_TYPE_PTR) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Parameter index %u is not a pointer (type %u)"
				    "\n", ptr_idx,
				    function->params[ptr_idx].type);
			goto unwind;
		}

		size = function->params[ptr_idx].size;

		if (offset >= (size - sizeof(virt_addr_t))) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Offset is larger than data area! "
				    "(offset=%u size=%u)\n", offset, size);
			goto unwind;
		}

		if (function->params[ptr_idx].data == 0) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Supplied user pointer is NULL!\n");
			goto unwind;
		}

		/* if the KVA pointer has not been mapped */
		if (base_ptrs[ptr_idx] == NULL) {
			/*
			 * map the UVA pointer to KVA space, the offset could
			 * potentially be modified due to the mapping
			 */
			base_ptrs[ptr_idx] = omaprpc_map_parameter(rpc,
								   &function->
								   params
								   [ptr_idx]);
		}

		/* if the KVA pointer is not NULL */
		if (base_ptrs[ptr_idx] != NULL) {
			if (direction == OMAPRPC_UVA_TO_RPA) {
				/*
				 * get the kernel virtual pointer to
				 * the pointer to translate
				 */
				virt_addr_t kva = (virt_addr_t)
				    &((base_ptrs[ptr_idx])[offset]);
				virt_addr_t uva = 0;
				virt_addr_t buva = (virt_addr_t)
				    function->translations[idx].base;
				phys_addr_t rpa = 0;
				void *reserved = (void *)
				    function->translations[idx].reserved;

				/*
				 * make sure we won't cause
				 * an unalign mem access
				 */
				if ((kva & 0x3) > 0) {
					OMAPRPC_ERR(rpc->rpcserv->dev,
						    "ERROR: KVA %p is unaligned!\n",
						    (void *)kva);
					return -EADDRNOTAVAIL;
				}
				/* load the user's VA */
				uva = *(virt_addr_t *) kva;

				OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
					      rpc->rpcserv->dev,
					      "Replacing UVA %p at KVA %p PTRIDX:%u "
					      "OFFSET:%u IDX:%d\n",
					      (void *)uva, (void *)kva, ptr_idx,
					      offset, idx);

				/* calc the new RPA (remote physical address) */
				rpa = omaprpc_buffer_lookup(rpc, rpc->core,
							    uva, buva,
							    reserved);
				/* save the old value */
				function->translations[idx].reserved = uva;
				/* replace with new RPA */
				*(phys_addr_t *) kva = rpa;

				OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
					      rpc->rpcserv->dev,
					      "Replaced UVA %p with RPA %p at KVA %p\n",
					      (void *)uva, (void *)rpa,
					      (void *)kva);

				if (rpa == 0) {
					/* need to unwind all operations.. */
					direction = OMAPRPC_RPA_TO_UVA;
					start = idx - 1;
					inc = -1;
					limit = -1;
					ret = -ENODATA;
					goto restart;
				}
			} else if (direction == OMAPRPC_RPA_TO_UVA) {
				/* address of the pointer in memory */
				virt_addr_t kva = (virt_addr_t) &
				    ((base_ptrs[ptr_idx])[offset]);
				virt_addr_t uva = 0;
				phys_addr_t rpa = 0;
				/*
				 * make sure we won't cause
				 * an unalign mem access
				 */
				if ((kva & 0x3) > 0)
					return -EADDRNOTAVAIL;
				/* get what was there for debugging */
				rpa = *(phys_addr_t *) kva;
				/* convienence value of uva */
				uva = (virt_addr_t)
				    function->translations[idx].reserved;
				/*
				 * replace the translated value
				 * with the remember version
				 */
				*(virt_addr_t *) kva = uva;

				OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
					      rpc->rpcserv->dev,
				"Replaced RPA %p with UVA %p at KVA %p\n",
						(void *)rpa, (void *)uva,
						(void *)kva);

				if (uva == 0) {
					/* need to unwind all operations.. */
					direction = OMAPRPC_RPA_TO_UVA;
					start = idx - 1;
					inc = -1;
					limit = -1;
					ret = -ENODATA;
					goto restart;
				}
			}
		} else {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Failed to map UVA to KVA to do translation!"
				    "\n");
			/*
			 * we can arrive here from multiple points,
			 * but the action is the same from everywhere
			 */
unwind:
			if (direction == OMAPRPC_UVA_TO_RPA) {
				/*
				 * we've encountered an error which
				 * needs to unwind all the operations
				 */
				OMAPRPC_ERR(rpc->rpcserv->dev,
					    "Unwinding UVA to RPA translations!\n");
				direction = OMAPRPC_RPA_TO_UVA;
				start = idx - 1;
				inc = -1;
				limit = -1;
				ret = -ENOBUFS;
				goto restart;
			} else if (direction == OMAPRPC_RPA_TO_UVA) {
				/*
				 *there was a problem restoring the pointer,
				 * there's nothing to do but to continue
				 * processing
				 */
				continue;
			}
		}
	}
	/* unmap all the pointers that were mapped and not freed yet */
	for (idx = 0; idx < OMAPRPC_MAX_PARAMETERS; idx++) {
		if (base_ptrs[idx]) {
			omaprpc_unmap_parameter(rpc,
						&function->params[idx],
						base_ptrs[idx], 0);
			base_ptrs[idx] = NULL;
		}
	}
	return ret;
}
