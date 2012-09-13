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

static void omaprpc_dma_clear(struct omaprpc_instance_t *rpc)
{
	struct dma_info_t *pos, *n;
	mutex_lock(&rpc->lock);
	list_for_each_entry_safe(pos, n, &rpc->dma_list, list) {
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
			      "Removing Pinning for FD %u\n", pos->fd);
		list_del((struct list_head *)pos);
		dma_buf_unmap_attachment(pos->attach, pos->sgt,
					 DMA_BIDIRECTIONAL);
		dma_buf_detach(pos->dbuf, pos->attach);
		dma_buf_put(pos->dbuf);
		kfree(pos);
	}
	mutex_unlock(&rpc->lock);
	return;
}

static int omaprpc_dma_add(struct omaprpc_instance_t *rpc,
			   struct dma_info_t *dma)
{
	if (dma) {
		mutex_lock(&rpc->lock);
		list_add(&dma->list, &rpc->dma_list);
		mutex_unlock(&rpc->lock);
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
			      rpc->rpcserv->dev,
			      "Added FD %u to list", dma->fd);
	}
	return 0;
}

static phys_addr_t omaprpc_pin_buffer(struct omaprpc_instance_t *rpc,
				      void *reserved)
{
	struct dma_info_t *dma = kmalloc(sizeof(struct dma_info_t), GFP_KERNEL);
	if (dma == NULL)
		return 0;

	dma->fd = (int)reserved;
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
		      rpc->rpcserv->dev, "Pining with FD %u\n", dma->fd);
	dma->dbuf = dma_buf_get((int)reserved);
	if (!(IS_ERR(dma->dbuf))) {
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
			      rpc->rpcserv->dev, "DMA_BUF=%p\n", dma->dbuf);
		dma->attach = dma_buf_attach(dma->dbuf, rpc->rpcserv->dev);
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
			      rpc->rpcserv->dev, "attach=%p\n", dma->attach);
		dma->sgt = dma_buf_map_attachment(dma->attach,
						  DMA_BIDIRECTIONAL);
		omaprpc_dma_add(rpc, dma);
		return sg_dma_address(dma->sgt->sgl);
	} else
		kfree(dma);
	return 0;
}

static phys_addr_t omaprpc_dma_find(struct omaprpc_instance_t *rpc,
				    void *reserved)
{
	phys_addr_t addr = 0;
	struct list_head *pos = NULL;
	struct dma_info_t *node = NULL;
	int fd = (int)reserved;
	mutex_lock(&rpc->lock);
	list_for_each(pos, &rpc->dma_list) {
		node = (struct dma_info_t *)pos;
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
			      "Looking for FD %u, found FD %u\n", fd, node->fd);
		if (node->fd == fd) {
			addr = sg_dma_address(node->sgt->sgl);
			break;
		}
	}
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "Returning Addr %p for FD %u\n", (void *)addr, fd);
	mutex_unlock(&rpc->lock);
	return addr;
}

phys_addr_t omaprpc_buffer_lookup(struct omaprpc_instance_t *rpc,
				  uint32_t core,
				  virt_addr_t uva,
				  virt_addr_t buva,
				  void *reserved)
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
	} else {
		/* find the base of the dma buf from the list */
		lpa = omaprpc_dma_find(rpc, reserved);
		if (lpa == 0) {
			/* wasn't in the list, convert the pointer */
			lpa = omaprpc_pin_buffer(rpc, reserved);
		}

		/* recalculate the offset in the user buffer
		   (accounts for tiler 2D) */
		uoff = omaprpc_recalc_off(lpa, uoff);

		/* offset the lpa by the offset in the user buffer */
		lpa += uoff;

		/* convert the local physical address to remote physical
		   address */
		rpa = rpmsg_local_to_remote_pa(core, lpa);
	}
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
	uint32_t ptr_idx = 0, pri_offset = 0, sec_offset = 0, pg_offset = 0,
	    size = 0;

	/* @NOTE not all the parameters are pointers so this may be sparse */
	uint8_t *base_ptrs[OMAPRPC_MAX_PARAMETERS];
	struct dma_buf *dbufs[OMAPRPC_MAX_PARAMETERS];

	if (function->num_translations == 0)
		return 0;

	limit = function->num_translations;
	memset(base_ptrs, 0, sizeof(base_ptrs));
	OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
		      "Operating on %d pointers\n", function->num_translations);
	/* we may have a failure during translation, in which case we need to
	   unwind the whole operation from here */

	for (idx = start; idx != limit; idx += inc) {
		OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
			      "#### Starting Translation %d of %d by %d\n",
			      idx, limit, inc);
		/* conveinence variables */
		ptr_idx = function->translations[idx].index;
		sec_offset = function->translations[idx].offset;

		/* if the pointer index for this translation is invalid */
		if (ptr_idx >= OMAPRPC_MAX_PARAMETERS) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Invalid parameter pointer index %u\n",
				    ptr_idx);
			goto unwind;
		} else if (function->params[ptr_idx].type !=
			   OMAPRPC_PARAM_TYPE_PTR) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Parameter index %u is not a pointer "
				    "(type %u)\n",
				    ptr_idx, function->params[ptr_idx].type);
			goto unwind;
		}

		size = function->params[ptr_idx].size;

		if (sec_offset >= (size - sizeof(virt_addr_t))) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Offset is larger than data area! "
				    "(sec_offset=%u size=%u)\n", sec_offset,
				    size);
			goto unwind;
		}

		if (function->params[ptr_idx].data == 0) {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Supplied user pointer is NULL!\n");
			goto unwind;
		}

		/* if the KVA pointer has not been mapped */
		if (base_ptrs[ptr_idx] == NULL) {
			size_t start = (pri_offset + sec_offset) & PAGE_MASK;
			size_t end = PAGE_SIZE;
			int ret = 0;

			/* compute the secondary offset */
			pri_offset = function->params[ptr_idx].data -
			    function->params[ptr_idx].base;

			/* acquire a handle to the dma buf */
			dbufs[ptr_idx] = dma_buf_get((int)function->
						     params[ptr_idx].reserved);

			/* map the dma buf into cpu memory? */
			ret = dma_buf_begin_cpu_access(dbufs[ptr_idx],
						       start,
						       end, DMA_BIDIRECTIONAL);
			if (ret < 0) {
				OMAPRPC_ERR(rpc->rpcserv->dev,
					    "OMAPRPC: Failed to acquire cpu access "
					    "to the DMA Buf! ret=%d\n", ret);
				dma_buf_put(dbufs[ptr_idx]);
				goto unwind;
			}

			/* caluculate the base pointer for the region. */
			base_ptrs[ptr_idx] = dma_buf_kmap(dbufs[ptr_idx],
							  ((pri_offset +
							    sec_offset) >>
							   PAGE_SHIFT));

			/* calculate the new offset within that page */
			pg_offset =
			    ((pri_offset + sec_offset) & (PAGE_SIZE - 1));

			if (base_ptrs[ptr_idx] != NULL) {
				OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
					      rpc->rpcserv->dev,
					      "KMap'd base_ptr[%u]=%p dbuf=%p into "
					      "kernel from %zu for %zu bytes, "
					      "PG_OFFSET=%u\n",
					      ptr_idx,
					      base_ptrs[ptr_idx],
					      dbufs[ptr_idx],
					      start, end, pg_offset);
			}
		}

		/* if the KVA pointer is not NULL */
		if (base_ptrs[ptr_idx] != NULL) {
			if (direction == OMAPRPC_UVA_TO_RPA) {
				/* get the kernel virtual pointer to the
				   pointer to translate */
				virt_addr_t kva =
				    (virt_addr_t) &
				    ((base_ptrs[ptr_idx])[pg_offset]);
				virt_addr_t uva = 0;
				virt_addr_t buva =
				    (virt_addr_t) function->translations[idx].
				    base;
				phys_addr_t rpa = 0;
				void *reserved =
				    (void *)function->translations[idx].
				    reserved;

				/* make sure we won't cause an unalign mem
				   access */
				if ((kva & 0x3) > 0) {
					OMAPRPC_ERR(rpc->rpcserv->dev,
						    "ERROR: KVA %p is unaligned!\n",
						    (void *)kva);
					return -EADDRNOTAVAIL;
				}
				/* load the user's VA */
				uva = *(virt_addr_t *) kva;
				if (uva == 0) {
					OMAPRPC_ERR(rpc->rpcserv->dev,
						    "ERROR: Failed to access user "
						    "buffer to translate pointer"
						    "\n");
					print_hex_dump(KERN_DEBUG,
						       "OMAPRPC: KMAP: ",
						       DUMP_PREFIX_NONE, 16, 1,
						       base_ptrs[ptr_idx],
						       PAGE_SIZE, true);
					goto unwind;
				}

				OMAPRPC_PRINT(OMAPRPC_ZONE_INFO,
					      rpc->rpcserv->dev,
					      "Replacing UVA %p at KVA %p PTRIDX:%u "
					      "PG_OFFSET:%u IDX:%d RESV:%p\n",
					      (void *)uva, (void *)kva, ptr_idx,
					      pg_offset, idx, reserved);

				/* calc the new RPA (remote physical address) */
				rpa = omaprpc_buffer_lookup(rpc, rpc->core,
							    uva, buva,
							    reserved);
				/* save the old value */
				function->translations[idx].reserved =
				    (size_t) uva;
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
					/*
					 * TODO: unmap the parameter
					 * base pointer
					 */
					goto restart;
				}
			} else if (direction == OMAPRPC_RPA_TO_UVA) {
				/* address of the pointer in memory */
				virt_addr_t kva = 0;
				virt_addr_t uva = 0;
				phys_addr_t rpa = 0;
				kva = (virt_addr_t)
				    &((base_ptrs[ptr_idx])[pg_offset]);
				/*
				 * make sure we won't cause an
				 * unalign mem access
				 */
				if ((kva & 0x3) > 0)
					return -EADDRNOTAVAIL;
				/* get what was there for debugging */
				rpa = *(phys_addr_t *) kva;
				/* convienence value of uva */
				uva = (virt_addr_t)
				    function->translations[idx].reserved;
				/*
				 * replace the translated value with the
				 * remember version
				 */
				*(virt_addr_t *) kva = uva;

				/*
				 * TODO: DMA_BUF requires unmapping the
				 * data from the TILER.
				 */

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
					/* @TODO unmap the parameter base
					   pointer */
					goto restart;
				}
			}
		} else {
			OMAPRPC_ERR(rpc->rpcserv->dev,
				    "Failed to map UVA to KVA "
				    "to do translation!\n");
			/*
			 * we can arrive here from multiple points, but the
			 * action is the same from everywhere
			 */
unwind:
			if (direction == OMAPRPC_UVA_TO_RPA) {
				/*
				 * we've encountered an error which needs to
				 * unwind all the operations
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
				 * there was a problem restoring the pointer,
				 * there's nothing to do but to continue
				 * processing
				 */
				continue;
			}
		}
restart:
		if (base_ptrs[ptr_idx]) {
			size_t start = (pri_offset + sec_offset) & PAGE_MASK;
			size_t end = PAGE_SIZE;

			OMAPRPC_PRINT(OMAPRPC_ZONE_INFO, rpc->rpcserv->dev,
				      "Unkmaping base_ptrs[%u]=%p from dbuf=%p %zu "
				      "for %zu bytes\n",
				      ptr_idx,
				      base_ptrs[ptr_idx],
				      dbufs[ptr_idx], start, end);
			/*
			 * unmap the page in case this pointer needs to
			 * move to a different adddress
			 */
			dma_buf_kunmap(dbufs[ptr_idx],
				       ((pri_offset +
					 sec_offset) >> PAGE_SHIFT),
				       base_ptrs[ptr_idx]);
			/* end access to this page */
			dma_buf_end_cpu_access(dbufs[ptr_idx],
					       start, end, DMA_BIDIRECTIONAL);
			base_ptrs[ptr_idx] = NULL;
			dma_buf_put(dbufs[ptr_idx]);
			dbufs[ptr_idx] = NULL;
			pg_offset = 0;
		}
	}
	if (direction == OMAPRPC_RPA_TO_UVA) {
		/*
		 * we're done translating everything, unpin all the translations
		 * and the parameters
		 */
		omaprpc_dma_clear(rpc);
	}
	return ret;
}
