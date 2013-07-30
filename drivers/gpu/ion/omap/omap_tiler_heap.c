/*
 * drivers/gpu/ion/omap_tiler_heap.c
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/spinlock.h>
#include <linux/err.h>
#include <linux/genalloc.h>
#include <linux/io.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/omap_ion.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "../../../drivers/gpu/drm/omapdrm/omap_dmm_tiler.h"
#include <asm/mach/map.h>
#include <asm/page.h>
#include "../../../arch/arm/mach-omap2/soc.h"



#include "../ion_priv.h"
#include "omap_ion_priv.h"
#include <asm/cacheflush.h>

struct omap_ion_heap {
	struct ion_heap heap;
	struct gen_pool *pool;
	ion_phys_addr_t base;
};

struct omap_tiler_info {
	struct tiler_block *tiler_handle;	/* handle of the allocation
						   intiler */
	bool lump;			/* true for a single lump allocation */
	u32 n_phys_pages;		/* number of physical pages */
	u32 *phys_addrs;		/* array addrs of pages */
	u32 n_tiler_pages;		/* number of tiler pages */
	u32 *tiler_addrs;		/* array of addrs of tiler pages */
	int fmt;			/* tiler buffer format */
	u32 tiler_start;		/* start addr in tiler -- if not page
					   aligned this may not equal the
					   first entry onf tiler_addrs */
	u32 vsize;			/* virtual stride of buffer */
	u32 vstride;			/* virtual size of buffer */
	u32 phys_stride;			/* Physical stride of the buffer */
	u32 flags;			/* Flags specifying cached or not */
};

static int omap_tiler_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size, unsigned long align,
				    unsigned long flags)
{
	struct omap_tiler_info *info;

	/* This means the buffer is already allocated and populated, we're getting here because
	 * of dummy handle creation, so simply return*/	
	if (size == 0) {
		/*
		  * Store the pointer to struct omap_tiler_info * into buffer here.
		  * This will be used later on inside map_dma function to create
		  * the sg list for tiler buffer
		  */
		info = (struct omap_tiler_info *) flags;
		if (!info)
			pr_err("%s: flags argument is not setupg\n", __func__);
		buffer->priv_virt = info;
		/* Re-update correct flags inside buffer */
		buffer->flags = info->flags;
		return 0;
	}

	pr_err("%s: This should never be called directly -- use the "
			"OMAP_ION_TILER_ALLOC flag to the ION_IOC_CUSTOM "
			"instead\n", __func__);
	return -EINVAL;
}

static int omap_tiler_alloc_carveout(struct ion_heap *heap,
				     struct omap_tiler_info *info)
{
	struct omap_ion_heap *omap_heap = (struct omap_ion_heap *)heap;
	int i;
	int ret;
	ion_phys_addr_t addr;

	addr = gen_pool_alloc(omap_heap->pool, info->n_phys_pages * PAGE_SIZE);
	if (addr) {
		info->lump = true;
		for (i = 0; i < info->n_phys_pages; i++)
			info->phys_addrs[i] = addr + i * PAGE_SIZE;
		return 0;
	}

	for (i = 0; i < info->n_phys_pages; i++) {
		addr = gen_pool_alloc(omap_heap->pool, PAGE_SIZE);

		if (addr == 0) {
			ret = -ENOMEM;
			pr_err("%s: failed to allocate pages to back "
			       "tiler address space\n", __func__);
			goto err;
		}
		info->phys_addrs[i] = addr;
	}
	return 0;

err:
	for (i -= 1; i >= 0; i--)
		gen_pool_free(omap_heap->pool, info->phys_addrs[i], PAGE_SIZE);
	return ret;
}

static void omap_tiler_free_carveout(struct ion_heap *heap,
				     struct omap_tiler_info *info)
{
	struct omap_ion_heap *omap_heap = (struct omap_ion_heap *)heap;
	int i;

	if (info->lump) {
		gen_pool_free(omap_heap->pool,
				info->phys_addrs[0],
				info->n_phys_pages * PAGE_SIZE);
		return;
	}

	for (i = 0; i < info->n_phys_pages; i++)
		gen_pool_free(omap_heap->pool, info->phys_addrs[i], PAGE_SIZE);
}

int omap_tiler_alloc(struct ion_heap *heap,
		     struct ion_client *client,
		     struct omap_ion_tiler_alloc_data *data)
{
	struct ion_handle *handle;
	struct ion_buffer *buffer;
	struct omap_tiler_info *info = NULL;
	u32 n_phys_pages;
	u32 n_tiler_pages;
	int i = 0, ret;
	uint32_t remainder;
	dma_addr_t ssptr;

	if (data->fmt == TILFMT_PAGE && data->h != 1) {
		pr_err("%s: Page mode (1D) allocations must have a height of "
				"one\n", __func__);
		return -EINVAL;
	}

	if (data->fmt == TILFMT_PAGE) {
		/* calculate required pages the usual way */
		n_phys_pages = round_up(data->w, PAGE_SIZE) >> PAGE_SHIFT;
		n_tiler_pages = n_phys_pages;
	} else {
		/* call APIs to calculate 2D buffer page requirements */
		n_phys_pages = tiler_size(data->fmt, data->w, data->h) >>
				PAGE_SHIFT;
		n_tiler_pages = tiler_vsize(data->fmt, data->w, data->h) >>
					PAGE_SHIFT;
	}

	info = kzalloc(sizeof(struct omap_tiler_info) +
		       sizeof(u32) * n_phys_pages +
		       sizeof(u32) * n_tiler_pages, GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->n_phys_pages = n_phys_pages;
	info->n_tiler_pages = n_tiler_pages;
	info->phys_addrs = (u32 *)(info + 1);
	info->tiler_addrs = info->phys_addrs + n_phys_pages;
	info->fmt = data->fmt;
	info->flags = data->flags;

	/* Allocate tiler space
	   FIXME: we only support PAGE_SIZE alignment right now. */
	if (data->fmt == TILFMT_PAGE)
		info->tiler_handle = tiler_reserve_1d(data->w);
	else
		info->tiler_handle = tiler_reserve_2d(data->fmt, data->w,
				data->h, PAGE_SIZE);

	info->tiler_handle->width = data->w;
	info->tiler_handle->height = data->h;

	if (IS_ERR_OR_NULL(info->tiler_handle)) {
		ret = PTR_ERR(info->tiler_handle);
		pr_err("%s: failure to allocate address space from tiler\n",
		       __func__);
		goto err_got_mem;
	}

	/* get physical address of tiler buffer */
	info->tiler_start = tiler_ssptr(info->tiler_handle);

	/* fill in tiler pages by using ssptr and stride */
	info->vstride = info->tiler_handle->stride;
	info->vsize = n_tiler_pages << PAGE_SHIFT;
	info->phys_stride = (data->fmt == TILFMT_PAGE) ? info->vstride :
				tiler_stride(data->fmt, 0);
	ssptr = info->tiler_start;
	remainder = info->vstride;

	for (i = 0; i < n_tiler_pages; i++) {
		info->tiler_addrs[i] = PAGE_ALIGN(ssptr);
		ssptr += PAGE_SIZE;
		remainder -= PAGE_SIZE;

		/* see if we are done with this line.  If so, go to the next
		   line */
		if (!remainder) {
			remainder = info->vstride;
			ssptr += info->phys_stride - info->vstride;
		}
	}

	if ((heap->id == OMAP_ION_HEAP_TILER) ||
	    (heap->id == OMAP_ION_HEAP_NONSECURE_TILER)) {
		ret = omap_tiler_alloc_carveout(heap, info);
		if (ret)
			goto err_got_tiler;

		ret = tiler_pin_phys(info->tiler_handle, info->phys_addrs,
					info->n_phys_pages);

		if (ret) {
			pr_err("%s: failure to pin pages to tiler\n",
				__func__);
			goto err_got_carveout;
		}
	}

	data->stride = info->vstride;

	/* create an ion handle  for the allocation */
	handle = ion_alloc(client, -1, 0, 1 << OMAP_ION_HEAP_TILER, (unsigned int) info);
	if (IS_ERR_OR_NULL(handle)) {
		ret = PTR_ERR(handle);
		pr_err("%s: failure to allocate handle to manage "
				"tiler allocation\n", __func__);
		goto err;
	}

	buffer = ion_handle_buffer(handle);
	buffer->size = n_tiler_pages * PAGE_SIZE;
	data->handle = handle;
	data->offset = (size_t)(info->tiler_start & ~PAGE_MASK);

	return 0;

err:
	tiler_unpin(info->tiler_handle);
err_got_carveout:
	if ((heap->id == OMAP_ION_HEAP_TILER) ||
	    (heap->id == OMAP_ION_HEAP_NONSECURE_TILER)) {
		omap_tiler_free_carveout(heap, info);
	}
err_got_tiler:
	tiler_release(info->tiler_handle);
err_got_mem:
	kfree(info);
	return ret;
}

static void omap_tiler_heap_free(struct ion_buffer *buffer)
{
	struct omap_tiler_info *info = buffer->priv_virt;

	tiler_unpin(info->tiler_handle);
	tiler_release(info->tiler_handle);

	if ((buffer->heap->id == OMAP_ION_HEAP_TILER) ||
	    (buffer->heap->id == OMAP_ION_HEAP_NONSECURE_TILER))
		omap_tiler_free_carveout(buffer->heap, info);

	kfree(info);
}

static int omap_tiler_phys(struct ion_heap *heap,
			   struct ion_buffer *buffer,
			   ion_phys_addr_t *addr, size_t *len)
{
	struct omap_tiler_info *info = buffer->priv_virt;

	*addr = info->tiler_start;
	*len = buffer->size;
	return 0;
}

int omap_tiler_pages(struct ion_client *client, struct ion_handle *handle,
		     int *n, u32 **tiler_addrs)
{
	ion_phys_addr_t addr;
	size_t len;
	int ret;
	struct omap_tiler_info *info = ion_handle_buffer(handle)->priv_virt;

	/* validate that the handle exists in this client */
	ret = ion_phys(client, handle, &addr, &len);
	if (ret)
		return ret;

	*n = info->n_tiler_pages;
	*tiler_addrs = info->tiler_addrs;
	return 0;
}
EXPORT_SYMBOL(omap_tiler_pages);

int omap_tiler_vinfo(struct ion_client *client, struct ion_handle *handle,
			unsigned int *vstride, unsigned int *vsize)
{
	struct omap_tiler_info *info = ion_handle_buffer(handle)->priv_virt;

	*vstride = info->vstride;
	*vsize = info->vsize;

	return 0;
}

static int omap_tiler_heap_map_user(struct ion_heap *heap,
		struct ion_buffer *buffer, struct vm_area_struct *vma)
{
	struct omap_tiler_info *info = buffer->priv_virt;
	unsigned long addr = vma->vm_start;
	u32 vma_pages = (vma->vm_end - vma->vm_start) / PAGE_SIZE;
	int n_pages = min(vma_pages, info->n_tiler_pages);
	int i, ret = 0;
	pgprot_t vm_page_prot;

	/* Use writecombined mappings unless on OMAP5 or DRA7.  If OMAP5 or DRA7, use
	shared device due to h/w issue. */
	if (soc_is_omap54xx() || soc_is_dra7xx())
		vm_page_prot = __pgprot_modify(vma->vm_page_prot, L_PTE_MT_MASK,
						L_PTE_MT_DEV_SHARED);
	else
		vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (TILER_PIXEL_FMT_PAGE == info->fmt) {
		/* Since 1D buffer is linear, map whole buffer in one shot */
		ret = remap_pfn_range(vma, addr,
				 __phys_to_pfn(info->tiler_addrs[0]),
				(vma->vm_end - vma->vm_start),
				vm_page_prot);
	} else {
		for (i = vma->vm_pgoff; i < n_pages; i++, addr += PAGE_SIZE) {
			ret = remap_pfn_range(vma, addr,
				 __phys_to_pfn(info->tiler_addrs[i]),
				PAGE_SIZE,
				vm_page_prot);
			if (ret)
				return ret;
		}
	}
	return ret;
}

static struct scatterlist *sg_alloc(unsigned int nents, gfp_t gfp_mask)
{
	return kmalloc(nents * sizeof(struct scatterlist), gfp_mask);
}

static void sg_free(struct scatterlist *sg, unsigned int nents)
{
	kfree(sg);
}



struct sg_table *omap_tiler_heap_map_dma(struct ion_heap *heap,
					      struct ion_buffer *buffer)
{
	int ret, i;
	struct sg_table *table = NULL;
	struct scatterlist *sg;
	struct omap_tiler_info *info = NULL;
	static phys_addr_t paddr;


	info = buffer->priv_virt;

	if(!info)
		return table;

	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table)
		return ERR_PTR(-ENOMEM);
	/* sg_alloc_table can only allocate multi-page scatter-gather list tables
	 * if the architecture supports scatter-gather lists chaining. ARM doesn't
	 * fit in that category.
	 * Use __sg_alloc_table instead of sg_alloc_table and allocate all entries
	 * in one go. Otherwise trying to allocate beyond SG_MAX_SINGLE_ALLOC
	 * when height > SG_MAX_SINGLE_ALLOC will hit a BUG_ON in __sg_alloc_table.
	 */

	ret = __sg_alloc_table(table, info->tiler_handle->height, -1, GFP_KERNEL, sg_alloc);
	if (ret) {
		kfree(table);
		return ERR_PTR(ret);
	}

	sg = table->sgl;
	for (i = 0; i < info->tiler_handle->height; i++) {
		paddr = info->tiler_start+ (i * info->phys_stride);
		sg_set_page(sg, phys_to_page(paddr), info->vstride, 0);
		sg = sg_next(sg);
	}

	return table;
}

void omap_tiler_heap_unmap_dma(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	__sg_free_table(buffer->sg_table, -1, sg_free);
}

void *ion_tiler_heap_map_kernel(struct ion_heap *heap,
				   struct ion_buffer *buffer)
{
	/* todo: Need to see how to implement this api. Seems like it is
	 * mandatory to implement in new ION
	 */
	return NULL;
}

void ion_tiler_heap_unmap_kernel(struct ion_heap *heap,
				    struct ion_buffer *buffer)
{
	/* todo: Need to see how to implement this api. Seems like it is
	 * mandatory to implement in new ION
	 */
	return;

}
static struct ion_heap_ops omap_tiler_ops = {
	.allocate = omap_tiler_heap_allocate,
	.free = omap_tiler_heap_free,
	.phys = omap_tiler_phys,
	.map_user = omap_tiler_heap_map_user,
	.map_dma = omap_tiler_heap_map_dma,
	.unmap_dma = omap_tiler_heap_unmap_dma,
	.map_kernel = ion_tiler_heap_map_kernel,
	.unmap_kernel = ion_tiler_heap_unmap_kernel,
};

struct ion_heap *omap_tiler_heap_create(struct ion_platform_heap *data)
{
	struct omap_ion_heap *heap;

	heap = kzalloc(sizeof(struct omap_ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);

	if ((data->id == OMAP_ION_HEAP_TILER) ||
	    (data->id == OMAP_ION_HEAP_NONSECURE_TILER)) {
		heap->pool = gen_pool_create(12, -1);
		if (!heap->pool) {
			kfree(heap);
			return ERR_PTR(-ENOMEM);
		}
		heap->base = data->base;
		gen_pool_add(heap->pool, heap->base, data->size, -1);
	}
	heap->heap.ops = &omap_tiler_ops;
	heap->heap.type = OMAP_ION_HEAP_TYPE_TILER;
	heap->heap.name = data->name;
	heap->heap.id = data->id;
	return &heap->heap;
}

void omap_tiler_heap_destroy(struct ion_heap *heap)
{
	struct omap_ion_heap *omap_ion_heap = (struct omap_ion_heap *)heap;
	if (omap_ion_heap->pool)
		gen_pool_destroy(omap_ion_heap->pool);
	kfree(heap);
}
