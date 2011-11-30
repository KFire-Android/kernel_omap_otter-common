/*
 * drivers/gpu/ion/ion_system_heap.c
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

#include <linux/err.h>
#include <linux/ion.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ion_priv.h"

static int ion_system_heap_allocate(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    unsigned long size, unsigned long align,
				    unsigned long flags)
{
	int n_pages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **page_list;
	const int gfp_mask = GFP_KERNEL | __GFP_HIGHMEM | __GFP_ZERO;
	int i = 0;

	page_list = kmalloc(n_pages * sizeof(void *), GFP_KERNEL);

	for (i = 0; i < n_pages; i++) {
		page_list[i] = alloc_page(gfp_mask);
		if (page_list[i] == NULL)
			goto out;
	}

	buffer->priv_virt = page_list;
	return 0;

out:
	/* failed on i, so go to i-1 */
	i--;

	for (; i >= 0; i--)
		__free_page(page_list[i]);

	kfree(page_list);
	return -ENOMEM;
}

void ion_system_heap_free(struct ion_buffer *buffer)
{
	int i;
	int n_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **page_list = (struct page **)buffer->priv_virt;

	for (i = 0; i < n_pages; i++)
		__free_page(page_list[i]);
	kfree(page_list);
}

struct scatterlist *ion_system_heap_map_dma(struct ion_heap *heap,
					    struct ion_buffer *buffer)
{
	struct scatterlist *sglist;
	struct page **page_list = (struct page **)buffer->priv_virt;
	int i;
	int n_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;

	sglist = vmalloc(n_pages * sizeof(struct scatterlist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);
	memset(sglist, 0, n_pages * sizeof(struct scatterlist));
	sg_init_table(sglist, n_pages);
	for (i = 0; i < n_pages; i++)
		sg_set_page(&sglist[i], page_list[i], PAGE_SIZE, 0);
	/* XXX do cache maintenance for dma? */
	return sglist;
}

void ion_system_heap_unmap_dma(struct ion_heap *heap, struct ion_buffer *buffer)
{
	/* XXX undo cache maintenance for dma? */
	if (buffer->sglist)
		vfree(buffer->sglist);
}

void *ion_system_heap_map_kernel(struct ion_heap *heap,
				 struct ion_buffer *buffer)
{
	int n_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **page_list = (struct page **)buffer->priv_virt;

	return vm_map_ram(page_list, n_pages, -1, PAGE_KERNEL);
}

void ion_system_heap_unmap_kernel(struct ion_heap *heap,
				  struct ion_buffer *buffer)
{
	int n_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;

	vm_unmap_ram(buffer->vaddr, n_pages);
}

int ion_system_heap_map_user(struct ion_heap *heap, struct ion_buffer *buffer,
			     struct vm_area_struct *vma)
{
	unsigned long uaddr = vma->vm_start;
	unsigned long usize = vma->vm_end - vma->vm_start;
	int n_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	struct page **page_list = (struct page **)buffer->priv_virt;
	int i;

	if (usize /* + pgoff << PAGE_SHIFT */  > (n_pages << PAGE_SHIFT))
		return -EINVAL;

	i = 0;
	do {
		int ret;

		ret = vm_insert_page(vma, uaddr, page_list[i]);
		if (ret)
			return ret;

		uaddr += PAGE_SIZE;
		usize -= PAGE_SIZE;
	} while (usize > 0);

	vma->vm_flags |= VM_RESERVED;

	return 0;
}

static struct ion_heap_ops vmalloc_ops = {
	.allocate = ion_system_heap_allocate,
	.free = ion_system_heap_free,
	.map_dma = ion_system_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_system_heap_map_kernel,
	.unmap_kernel = ion_system_heap_unmap_kernel,
	.map_user = ion_system_heap_map_user,
};

struct ion_heap *ion_system_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &vmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM;
	return heap;
}

void ion_system_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}

static int ion_system_contig_heap_allocate(struct ion_heap *heap,
					   struct ion_buffer *buffer,
					   unsigned long len,
					   unsigned long align,
					   unsigned long flags)
{
	buffer->priv_virt = kzalloc(len, GFP_KERNEL);
	if (!buffer->priv_virt)
		return -ENOMEM;
	return 0;
}

void ion_system_contig_heap_free(struct ion_buffer *buffer)
{
	kfree(buffer->priv_virt);
}

static int ion_system_contig_heap_phys(struct ion_heap *heap,
				       struct ion_buffer *buffer,
				       ion_phys_addr_t *addr, size_t *len)
{
	*addr = virt_to_phys(buffer->priv_virt);
	*len = buffer->size;
	return 0;
}

struct scatterlist *ion_system_contig_heap_map_dma(struct ion_heap *heap,
						   struct ion_buffer *buffer)
{
	struct scatterlist *sglist;

	sglist = vmalloc(sizeof(struct scatterlist));
	if (!sglist)
		return ERR_PTR(-ENOMEM);
	sg_init_table(sglist, 1);
	sg_set_page(sglist, virt_to_page(buffer->priv_virt), buffer->size, 0);
	return sglist;
}

int ion_system_contig_heap_map_user(struct ion_heap *heap,
				    struct ion_buffer *buffer,
				    struct vm_area_struct *vma)
{
	unsigned long pfn = __phys_to_pfn(virt_to_phys(buffer->priv_virt));
	return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot);

}

static struct ion_heap_ops kmalloc_ops = {
	.allocate = ion_system_contig_heap_allocate,
	.free = ion_system_contig_heap_free,
	.phys = ion_system_contig_heap_phys,
	.map_dma = ion_system_contig_heap_map_dma,
	.unmap_dma = ion_system_heap_unmap_dma,
	.map_kernel = ion_system_heap_map_kernel,
	.unmap_kernel = ion_system_heap_unmap_kernel,
	.map_user = ion_system_contig_heap_map_user,
};

struct ion_heap *ion_system_contig_heap_create(struct ion_platform_heap *unused)
{
	struct ion_heap *heap;

	heap = kzalloc(sizeof(struct ion_heap), GFP_KERNEL);
	if (!heap)
		return ERR_PTR(-ENOMEM);
	heap->ops = &kmalloc_ops;
	heap->type = ION_HEAP_TYPE_SYSTEM_CONTIG;
	return heap;
}

void ion_system_contig_heap_destroy(struct ion_heap *heap)
{
	kfree(heap);
}
