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
#include <linux/module.h>
#include <mach/tiler.h>
#include <asm/mach/map.h>
#include <asm/page.h>

#include "../ion_priv.h"
#include <asm/cacheflush.h>

#ifdef CONFIG_ION_OMAP_DYNAMIC
#define FLUSH_PAGE
#ifdef FLUSH_PAGE
#include <asm/cacheflush.h>
#endif
#endif

// Was 1
#define TILER_ENABLE_NON_PAGE_ALIGNED_ALLOCATIONS  0

int omap_carveout_alloc(struct ion_heap *heap,
		     struct omap_ion_tiler_alloc_data *data,
		     struct ion_buffer *buffer);

static int omap_tiler_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size, unsigned long align,
				    unsigned long flags)
{
#ifdef CONFIG_ION_OMAP_DYNAMIC
	int ret = 0;
	int fmt = TILER_PIXEL_FMT_8BIT;
	struct omap_ion_tiler_alloc_data data;

	if (size == 0)
		return 0;

	if (!(align & 0xFFF)) {
		fmt = TILER_PIXEL_FMT_PAGE;
	} else
	if (!(align & 0x3)) {
		fmt = TILER_PIXEL_FMT_32BIT;
	} else
	if (!(align & 0x1)) {
		fmt = TILER_PIXEL_FMT_16BIT;
	}

	data.w = size;
	data.h = 1;
	data.fmt = fmt;
	data.flags =  flags;

	if (omap_carveout_alloc(heap, &data, buffer) != 0) {
	      pr_err("Error with 1D tiler allocate\n");
	      ret = -ENOMEM;
	}
	return ret;
#else
	pr_err("%s: This should never be called directly -- use the "
	       "OMAP_ION_TILER_ALLOC flag to the ION_IOC_CUSTOM "
	       "instead\n", __func__);
	return -EINVAL;
#endif
}

struct omap_tiler_info {
	tiler_blk_handle tiler_handle;	/* handle of the allocation intiler */
	bool lump;			/* true for a single lump allocation */
	u32 n_phys_pages;		/* number of physical pages */
	u32 *phys_addrs;		/* array addrs of pages */
	u32 n_tiler_pages;		/* number of tiler pages */
	u32 *tiler_addrs;		/* array of addrs of tiler pages */
	int fmt;			/* tiler buffer format */
	u32 tiler_start;		/* start addr in tiler -- if not page
					   aligned this may not equal the
					   first entry onf tiler_addrs */
};

static int tiler_memory_allocate(struct ion_heap *heap, struct omap_tiler_info *info, unsigned int nb_pages, unsigned long align, unsigned int* allocated_pages)
{
	int i;

#ifdef CONFIG_ION_OMAP_DYNAMIC
	struct page *pg;
#endif

	if (heap == NULL) return ION_CARVEOUT_ALLOCATE_FAIL;
#ifdef CONFIG_ION_OMAP_DYNAMIC
	(*allocated_pages)=0;
	for (i = 0; i < nb_pages; i++) {
		pg = alloc_page(GFP_KERNEL | GFP_DMA);
		if (pg)
			 info->phys_addrs[i] = page_to_phys(pg);
		else return ION_CARVEOUT_ALLOCATE_FAIL;
#ifdef FLUSH_PAGE
		dmac_flush_range((void *)page_address(pg),
			(void *)page_address(pg) + PAGE_SIZE);
		outer_flush_range(info->phys_addrs[i],
			info->phys_addrs[i] + PAGE_SIZE);
#endif
		(*allocated_pages)++;
	}
#else
	int addr;
	addr = ion_carveout_allocate(heap, nb_pages*PAGE_SIZE, align);
	if (addr == ION_CARVEOUT_ALLOCATE_FAIL) {
		(*allocated_pages)=0;
		for (i = 0; i < nb_pages; i++) {
			addr = ion_carveout_allocate(heap, PAGE_SIZE, align);
			if (addr == ION_CARVEOUT_ALLOCATE_FAIL)
				return addr;
			else
				info->phys_addrs[i] = addr;
			(*allocated_pages)++;
		}
	} else {
		info->lump = true;
		(*allocated_pages) = nb_pages-1;
		for (i = 0; i < nb_pages; i++)
			info->phys_addrs[i] = addr + i*PAGE_SIZE;
	}
#endif
	return 0;
}

void tiler_memory_free(struct ion_heap *heap, struct omap_tiler_info *info, unsigned int nb_pages)
{
	int i;
#ifdef CONFIG_ION_OMAP_DYNAMIC
	struct page *pg;
	for (i = 0; i < nb_pages; i++) {
		if (info->phys_addrs[i] != ION_CARVEOUT_ALLOCATE_FAIL) {
			pg = phys_to_page(info->phys_addrs[i]);
			if (pg != NULL)
				__free_page(pg);
			info->phys_addrs[i] = ION_CARVEOUT_ALLOCATE_FAIL;
		}
	}
#else
	if (info->lump)
		ion_carveout_free(heap, info->phys_addrs[0], nb_pages*PAGE_SIZE);
	else
		for (i = 0; i <= nb_pages; i++)
			ion_carveout_free(heap, info->phys_addrs[i], PAGE_SIZE);
#endif	
}

int omap_carveout_alloc(struct ion_heap *heap,
		     struct omap_ion_tiler_alloc_data *data,
		     struct ion_buffer *buffer)
{
	struct omap_tiler_info *info;
	u32 n_phys_pages;
	u32 n_tiler_pages;
	ion_phys_addr_t addr;
	int ret;
	int nb_allocated_pages = 0;

	if (data->fmt == TILER_PIXEL_FMT_PAGE && data->h != 1) {
		pr_err("%s: Page mode (1D) allocations must have a height "
		       "of one\n", __func__);
		return -EINVAL;
	}

	ret = tiler_memsize(data->fmt, data->w, data->h,
			    &n_phys_pages,
			    &n_tiler_pages);

	if (ret) {
		pr_err("%s: invalid tiler request w %u h %u fmt %u\n", __func__,
		       data->w, data->h, data->fmt);
		return ret;
	}

	BUG_ON(!n_phys_pages || !n_tiler_pages);

	info = kzalloc(sizeof(struct omap_tiler_info) +
		       sizeof(u32) * n_phys_pages +
		       sizeof(u32) * n_tiler_pages, GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->n_phys_pages = n_phys_pages;
	info->n_tiler_pages = n_tiler_pages;
	info->phys_addrs = (u32 *)(info + 1);
	info->tiler_addrs = info->phys_addrs + n_phys_pages;

	info->tiler_handle = tiler_alloc_block_area(data->fmt, data->w, data->h,
						    &info->tiler_start,
						    info->tiler_addrs);
	if (IS_ERR_OR_NULL(info->tiler_handle)) {
		ret = PTR_ERR(info->tiler_handle);
		pr_err("%s: failure to allocate address space from tiler\n",
		       __func__);
		goto err_nomem;
	}

	addr = tiler_memory_allocate(heap, info, n_phys_pages, 0, &nb_allocated_pages);
	if (addr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		pr_err("%s: failed to allocate pages to back "
				"tiler address space\n", __func__);
		goto err_alloc;
	}

	ret = tiler_pin_block(info->tiler_handle, info->phys_addrs,
			      info->n_phys_pages);
	if (ret) {
		pr_err("%s: failure to pin pages to tiler\n", __func__);
		goto err_alloc;
	}

	data->stride = tiler_block_vstride(info->tiler_handle);

	buffer->size = info->n_tiler_pages * PAGE_SIZE;
	buffer->priv_virt = info;
	return 0;

err_alloc:
	tiler_free_block_area(info->tiler_handle);
	tiler_memory_free(heap, info, nb_allocated_pages);
err_nomem:
	kfree(info);
	return ret;
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
//	u32 tiler_start = 0;
	u32 v_size;
//	tiler_blk_handle tiler_handle;
	ion_phys_addr_t addr = 0;
	int i = 0, ret;
#ifdef CONFIG_ION_OMAP_DYNAMIC
	int nb_allocated_pages = 0;
#endif

	if (data->fmt == TILER_PIXEL_FMT_PAGE && data->h != 1) {
		pr_err("%s: Page mode (1D) allocations must have a height "
		       "of one\n", __func__);
		return -EINVAL;
	}

	ret = tiler_memsize(data->fmt, data->w, data->h,
			    &n_phys_pages,
			    &n_tiler_pages);

	if (ret) {
		pr_err("%s: invalid tiler request w %u h %u fmt %u\n", __func__,
		       data->w, data->h, data->fmt);
		return ret;
	}

	BUG_ON(!n_phys_pages || !n_tiler_pages);

	info = kzalloc(sizeof(struct omap_tiler_info) +
		       sizeof(u32) * n_phys_pages +
		       sizeof(u32) * n_tiler_pages, GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->n_phys_pages = n_phys_pages;
	info->n_tiler_pages = n_tiler_pages;
	info->phys_addrs = (u32 *)(info + 1);
	info->tiler_addrs = info->phys_addrs + n_phys_pages;

	if( (TILER_ENABLE_NON_PAGE_ALIGNED_ALLOCATIONS)
			&& (data->token != 0) ) {
		info->tiler_handle = tiler_alloc_block_area_aligned(data->fmt, data->w, data->h,
									    &info->tiler_start,
									    info->tiler_addrs,
									    data->out_align,
									    data->offset,
									    data->token);
	} else {
		info->tiler_handle = tiler_alloc_block_area(data->fmt, data->w, data->h,
							    	&info->tiler_start,
							    	info->tiler_addrs);
	}

	if (IS_ERR_OR_NULL(info->tiler_handle)) {
		ret = PTR_ERR(info->tiler_handle);
		pr_err("%s: failure to allocate address space from tiler\n",
		       __func__);
		goto err_nomem;
	}

	v_size = tiler_block_vsize(info->tiler_handle);

	if(!v_size)
		goto err_alloc;

	n_tiler_pages = (PAGE_ALIGN(v_size) / PAGE_SIZE);

#ifndef CONFIG_ION_OMAP_DYNAMIC
	info = kzalloc(sizeof(struct omap_tiler_info) +
		       sizeof(u32) * n_phys_pages +
		       sizeof(u32) * n_tiler_pages, GFP_KERNEL);
	if (!info)
		goto err_alloc;

	info->tiler_handle = tiler_handle;
	info->tiler_start = tiler_start;
	info->n_phys_pages = n_phys_pages;
	info->n_tiler_pages = n_tiler_pages;
	info->phys_addrs = (u32 *)(info + 1);
	info->tiler_addrs = info->phys_addrs + n_phys_pages;
	info->fmt = data->fmt;

	addr = ion_carveout_allocate(heap, n_phys_pages*PAGE_SIZE, 0);
	if (addr == ION_CARVEOUT_ALLOCATE_FAIL) {
		for (i = 0; i < n_phys_pages; i++) {
			addr = ion_carveout_allocate(heap, PAGE_SIZE, 0);

			if (addr == ION_CARVEOUT_ALLOCATE_FAIL) {
				ret = -ENOMEM;
				pr_err("%s: failed to allocate pages to back "
					"tiler address space\n", __func__);
				goto err_alloc;
			}
			info->phys_addrs[i] = addr;
		}
	} else {
		info->lump = true;
		for (i = 0; i < n_phys_pages; i++)
			info->phys_addrs[i] = addr + i*PAGE_SIZE;
	}
#else
	addr = tiler_memory_allocate(heap, info, n_phys_pages, 0, &nb_allocated_pages);
	if (addr == ION_CARVEOUT_ALLOCATE_FAIL) {
		ret = -ENOMEM;
		pr_err("%s: failed to allocate pages to back "
				"tiler address space\n", __func__);
		goto err_alloc;
	}
#endif

	ret = tiler_pin_block(info->tiler_handle, info->phys_addrs,
			      info->n_phys_pages);
	if (ret) {
		pr_err("%s: failure to pin pages to tiler\n", __func__);
		goto err_alloc;
	}

	data->stride = tiler_block_vstride(info->tiler_handle);

	/* create an ion handle  for the allocation */
#ifdef CONFIG_ION_OMAP_DYNAMIC
	handle = ion_alloc(client, 0, 0, 1 << OMAP_ION_HEAP_TILER);
#else
	handle = ion_alloc(client, 0, 0, 1 << heap->id);
#endif
	if (IS_ERR_OR_NULL(handle)) {
		ret = PTR_ERR(handle);
		pr_err("%s: failure to allocate handle to manage tiler"
		       " allocation\n", __func__);
		goto err;
	}

	buffer = ion_handle_buffer(handle);
	buffer->size = v_size;
	buffer->priv_virt = info;
	data->handle = handle;
	data->offset = (size_t)(info->tiler_start & ~PAGE_MASK);

	if(tiler_fill_virt_array(info->tiler_handle, info->tiler_addrs,
			&n_tiler_pages) < 0) {
		pr_err("%s: failure filling tiler's virtual array %d\n",
				__func__, n_tiler_pages);
	}

	return 0;

err:
	tiler_unpin_block(info->tiler_handle);
err_alloc:
	tiler_free_block_area(info->tiler_handle);
	if(info)
	{
		if (info->lump)
#ifdef CONFIG_ION_OMAP_DYNAMIC
			tiler_memory_free(heap, info, nb_allocated_pages);
#else
			ion_carveout_free(heap, addr, n_phys_pages * PAGE_SIZE);
#endif
		else
			for (i -= 1; i >= 0; i--)
				ion_carveout_free(heap, info->phys_addrs[i], PAGE_SIZE);
	}
err_nomem:
	kfree(info);
	return ret;
}

void omap_tiler_heap_free(struct ion_buffer *buffer)
{
	struct omap_tiler_info *info = buffer->priv_virt;

	tiler_unpin_block(info->tiler_handle);
	tiler_free_block_area(info->tiler_handle);

#ifdef CONFIG_ION_OMAP_DYNAMIC
	tiler_memory_free(buffer->heap, info, info->n_phys_pages);
#else
	if (info->lump) {
		ion_carveout_free(buffer->heap, info->phys_addrs[0],
				  info->n_phys_pages*PAGE_SIZE);
	} else {
		int i;
		for (i = 0; i < info->n_phys_pages; i++)
			ion_carveout_free(buffer->heap,
					  info->phys_addrs[i], PAGE_SIZE);
	}
#endif

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

	*vstride = tiler_block_vstride(info->tiler_handle);
	*vsize = tiler_block_vsize(info->tiler_handle);

	return 0;
}

int omap_tiler_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			     struct vm_area_struct *vma)
{
	struct omap_tiler_info *info = buffer->priv_virt;
	unsigned long addr = vma->vm_start;
	u32 vma_pages = (vma->vm_end - vma->vm_start) / PAGE_SIZE;
	int n_pages = min(vma_pages, info->n_tiler_pages);
	int i, ret = 0;

	if (TILER_PIXEL_FMT_PAGE == info->fmt) {
		/* Since 1D buffer is linear, map whole buffer in one shot */
		ret = remap_pfn_range(vma, addr,
				 __phys_to_pfn(info->tiler_addrs[0]),
				(vma->vm_end - vma->vm_start),
				(buffer->cached ?
				(vma->vm_page_prot)
				: pgprot_writecombine(vma->vm_page_prot)));
	} else {
		for (i = vma->vm_pgoff; i < n_pages; i++, addr += PAGE_SIZE) {
			ret = remap_pfn_range(vma, addr,
				 __phys_to_pfn(info->tiler_addrs[i]),
				PAGE_SIZE,
				pgprot_writecombine(vma->vm_page_prot));
			if (ret)
				return ret;
		}
	}
	return ret;
}

static void per_cpu_cache_flush_arm(void *arg)
{
	   flush_cache_all();
}

int omap_tiler_cache_operation(struct ion_buffer *buffer, size_t len,
			unsigned long vaddr, enum cache_operation cacheop)
{
	struct omap_tiler_info *info;
	int n_pages;

	if (!buffer) {
		pr_err("%s(): buffer is NULL\n", __func__);
		return -EINVAL;
	}
	if (!buffer->cached) {
		pr_err("%s(): buffer not mapped as cacheable\n", __func__);
		return -EINVAL;
	}

	info = buffer->priv_virt;
	if (!info) {
		pr_err("%s(): tiler info of buffer is NULL\n", __func__);
		return -EINVAL;
	}

	n_pages = info->n_tiler_pages;
	if (len > (n_pages * PAGE_SIZE)) {
		pr_err("%s(): size to flush is greater than allocated size\n",
			__func__);
		return -EINVAL;
	}

	if (TILER_PIXEL_FMT_PAGE != info->fmt) {
		pr_err("%s(): only TILER 1D buffers can be cached\n",
			__func__);
		return -EINVAL;
	}

	if (len > FULL_CACHE_FLUSH_THRESHOLD) {
		on_each_cpu(per_cpu_cache_flush_arm, NULL, 1);
		outer_flush_all();
		return 0;
	}

	flush_cache_user_range(vaddr, vaddr + len);

	if (cacheop == CACHE_FLUSH)
		outer_flush_range(info->tiler_addrs[0],
			info->tiler_addrs[0] + len);
	else
		outer_inv_range(info->tiler_addrs[0],
			info->tiler_addrs[0] + len);
	return 0;
}

int omap_tiler_heap_flush_user(struct ion_buffer *buffer, size_t len,
			unsigned long vaddr)
{
	return omap_tiler_cache_operation(buffer, len, vaddr, CACHE_FLUSH);
}

int omap_tiler_heap_inval_user(struct ion_buffer *buffer, size_t len,
			unsigned long vaddr)
{
	return omap_tiler_cache_operation(buffer, len, vaddr, CACHE_INVALIDATE);
}

static struct ion_heap_ops omap_tiler_ops = {
	.allocate = omap_tiler_heap_allocate,
	.free = omap_tiler_heap_free,
	.phys = omap_tiler_phys,
	.map_user = omap_tiler_heap_map_user,
	.flush_user = omap_tiler_heap_flush_user,
	.inval_user = omap_tiler_heap_inval_user,
};

#ifdef CONFIG_ION_OMAP_DYNAMIC
static struct ion_heap_ops omap_carveout_tiler_ops = {
	.allocate = omap_tiler_heap_allocate,
	.free = omap_tiler_heap_free,
	.phys = omap_tiler_phys,
	.map_user = omap_tiler_heap_map_user,
	.map_kernel = omap_tiler_heap_map_kernel,
	.unmap_kernel = omap_tiler_heap_unmap_kernel,
};
#endif

struct ion_heap *omap_carveout_tiler_heap_create(struct ion_platform_heap *data)
{
	  struct ion_heap *heap;

	heap = ion_carveout_heap_create(data);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &omap_carveout_tiler_ops;
	heap->name = data->name;
	heap->id = data->id;
	return heap;
}

struct ion_heap *omap_tiler_heap_create(struct ion_platform_heap *data)
{
	struct ion_heap *heap;

	heap = ion_carveout_heap_create(data);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &omap_tiler_ops;
	heap->type = OMAP_ION_HEAP_TYPE_TILER;
	heap->name = data->name;
	heap->id = data->id;
	return heap;
}

void omap_tiler_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

void omap_carveout_tiler_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
