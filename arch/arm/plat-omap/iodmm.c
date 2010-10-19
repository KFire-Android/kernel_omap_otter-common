/*
 * OMAP DMM (Dynamic memory mapping) to IOMMU module
 *
 * Copyright (C) 2010 Texas Instruments. All rights reserved.
 *
 * Authors: Ramesh Gupta <grgupta@ti.com>
 *          Hari Kanigeri <h-kanigeri2@ti.com>
 *
 *          dma_map API usage in this code is inspired from Ohad Ben-Cohen's
 *          implementation in dspbridge code.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/platform_device.h>
#include <linux/pagemap.h>
#include <linux/kernel.h>
#include <linux/genalloc.h>
#include <linux/eventfd.h>

#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <asm/mach/map.h>
#include <linux/dma-mapping.h>

#include <plat/iommu.h>
#include <plat/dmm_user.h>

#include "iopgtable.h"

#ifndef CONFIG_DMM_DMA_API
/* Hack hack code to handle MM buffers */
int temp_user_dma_op(unsigned long start, unsigned long end, int op)
{

	struct mm_struct *mm = current->active_mm;
	void (*inner_op)(const void *, const void *);
	void (*outer_op)(unsigned long, unsigned long);

	switch (op) {
	case 1:
		inner_op = dmac_inv_range;
		outer_op = outer_inv_range;
		break;

	case 2:
		inner_op = dmac_clean_range;
		outer_op = outer_clean_range;
		break;

	case 3:
		inner_op = dmac_flush_range;
		outer_op = outer_flush_range;
		break;

	default:
		return -EINVAL;
	}

	if (end < start)
		return -EINVAL;

	down_read(&mm->mmap_sem);

	do {
		struct vm_area_struct *vma = find_vma(mm, start);

		if (!vma || start < vma->vm_start ||
			vma->vm_flags & (VM_IO | VM_PFNMAP)) {
			up_read(&mm->mmap_sem);
			return -EFAULT;
		}
		do {
			unsigned long e = (start | ~PAGE_MASK) + 1;
			struct page *page;

			if (e > end)
				e = end;
			page = follow_page(vma, start, FOLL_GET);
			if (IS_ERR(page)) {
				up_read(&mm->mmap_sem);
				return PTR_ERR(page);
			}
			if (page) {
				unsigned long phys;
				/*
				 * This flushes the userspace address - which
				 * is not what this API was intended to do.
				 * Things may go astray as a result.
				 */
				inner_op((void *)start, (void *)e);
				/*
				 * Now handle the L2 cache.
				 */
				phys = page_to_phys(page) +
						(start & ~PAGE_MASK);
				outer_op(phys, phys + e - start);
				put_page(page);
			}
			start = e;
		} while (start < end && start < vma->vm_end);
	} while (start < end);

	up_read(&mm->mmap_sem);

	return 0;
}
#endif

static inline struct gen_pool *get_pool_handle(struct iovmm_device *iovmm_obj,
								int pool_id)
{
	struct iovmm_pool *pool;

	list_for_each_entry(pool, &iovmm_obj->mmap_pool, list) {
		if (pool->pool_id == pool_id)
			return pool->genpool;
	}
	return NULL;
}

/*
 * This function walks through the page tables to convert a userland
 * virtual address to physical address
 */
static u32 __user_va2_pa(struct mm_struct *mm, u32 address)
{
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *ptep, pte;

	pgd = pgd_offset(mm, address);
	if (!(pgd_none(*pgd) || pgd_bad(*pgd))) {
		pmd = pmd_offset(pgd, address);
		if (!(pmd_none(*pmd) || pmd_bad(*pmd))) {
			ptep = pte_offset_map(pmd, address);
			if (ptep) {
				pte = *ptep;
				if (pte_present(pte))
					return pte & PAGE_MASK;
			}
		}
	}
	return 0;
}

/* remember mapping information */
static struct dmm_map_object *add_mapping_info(struct iodmm_struct *obj,
		struct gen_pool *gen_pool, u32 va, u32 da, u32 size)
{
	struct dmm_map_object *map_obj;

	u32 num_usr_pgs = size / PAGE_SIZE;

	pr_debug("%s: adding map info: va 0x%x virt 0x%x size 0x%x\n",
						__func__, va,
						da, size);
	map_obj = kzalloc(sizeof(struct dmm_map_object), GFP_KERNEL);
	if (!map_obj) {
		pr_err("%s: kzalloc failed\n", __func__);
		return NULL;
	}
	INIT_LIST_HEAD(&map_obj->link);

	map_obj->pages = kcalloc(num_usr_pgs, sizeof(struct page *),
								GFP_KERNEL);
	if (!map_obj->pages) {
		pr_err("%s: kzalloc failed\n", __func__);
		kfree(map_obj);
		return NULL;
	}

	map_obj->va = va;
	map_obj->da = da;
	map_obj->size = size;
	map_obj->num_usr_pgs = num_usr_pgs;
	map_obj->gen_pool = gen_pool;
	list_add(&map_obj->link, &obj->map_list);

	return map_obj;
}

static int match_exact_map_obj(struct dmm_map_object *map_obj,
					u32 da, u32 size)
{
	u32 res;

	if (map_obj->da == da && map_obj->size != size)
		pr_err("%s: addr match (0x%x), size don't (0x%x != 0x%x)\n",
				__func__, da, map_obj->size, size);

	if (map_obj->da == da && map_obj->size == size)
		res = 0;
	else
		res = -ENODATA;
	return res;
}

static void remove_mapping_information(struct iodmm_struct *obj,
						u32 da, u32 size)
{
	struct dmm_map_object *map_obj;

	pr_debug("%s: looking for virt 0x%x size 0x%x\n", __func__,
							da, size);
	list_for_each_entry(map_obj, &obj->map_list, link) {
		pr_debug("%s: candidate: va 0x%x virt 0x%x size 0x%x\n",
							__func__,
							map_obj->va,
							map_obj->da,
							map_obj->size);

		if (!match_exact_map_obj(map_obj, da, size)) {
			pr_debug("%s: match, deleting map info\n", __func__);
			if (map_obj->gen_pool != NULL)
				gen_pool_free(map_obj->gen_pool, da, size);
			list_del(&map_obj->link);
			kfree(map_obj->dma_info.sg);
			kfree(map_obj->pages);
			kfree(map_obj);
			goto out;
		}
		pr_debug("%s: candidate didn't match\n", __func__);
	}

	pr_err("%s: failed to find given map info\n", __func__);
out:
	return;
}

static int match_containing_map_obj(struct dmm_map_object *map_obj,
				u32 va, u32 da, bool check_va, u32 size)
{
	u32 res;
	u32 map_obj_end;

	if (check_va) {
		map_obj_end = map_obj->va + map_obj->size;
		if ((va >= map_obj->va) && (va + size <= map_obj_end))
			res = 0;
		else
			res = -ENODATA;
	} else {
		if (da == map_obj->da)
			res = 0;
		else
			res = -ENODATA;
	}
	return res;
}

/**
 * Find the mapping object based on either MPU virtual address or
 * Device virtual address. Which option to select to search for the mapping
 * is specified with check_va flag. check_va is set to TRUE if search is
 * based on MPU virtual address and FALSE if search is based on Device
 * virtual address
 */
static struct dmm_map_object *find_containing_mapping(
				struct iodmm_struct *obj,
				u32 va, u32 da, bool check_va,
				u32 size)
{
	struct dmm_map_object *map_obj, *temp_map;
	pr_debug("%s: looking for va 0x%x size 0x%x\n", __func__,
						va, size);
	list_for_each_entry_safe(map_obj, temp_map, &obj->map_list, link) {
		pr_debug("%s: candidate: va 0x%x virt 0x%x size 0x%x\n",
						__func__,
						map_obj->va,
						map_obj->da,
						map_obj->size);
		if (!match_containing_map_obj(map_obj, va, da, check_va,
								size)) {
			pr_debug("%s: match!\n", __func__);
			goto out;
		}

		pr_debug("%s: no match!\n", __func__);
	}

	map_obj = NULL;
out:
	return map_obj;
}


static inline struct page *get_mapping_page(struct dmm_map_object *map_obj,
								int pg_i)
{
	pr_debug("%s: looking for pg_i %d, num_usr_pgs: %d\n", __func__,
					pg_i, map_obj->num_usr_pgs);
	if (pg_i < 0 || pg_i >= map_obj->num_usr_pgs) {
		pr_err("%s: requested pg_i %d is out of mapped range\n",
				__func__, pg_i);
		return NULL;
	}
	return map_obj->pages[pg_i];
}

#ifdef CONFIG_DMM_DMA_API
static int find_first_page_in_cache(struct dmm_map_object *map_obj,
					unsigned long va)
{
	u32 mapped_base_page = map_obj->va >> PAGE_SHIFT;
	u32 requested_base_page = va >> PAGE_SHIFT;
	int pg_index = requested_base_page - mapped_base_page;

	if (pg_index < 0 || pg_index >= map_obj->num_usr_pgs) {
		pr_err("%s: failed (got %d)\n", __func__, pg_index);
		return -1;
	}

	pr_debug("%s: first page is %d\n", __func__, pg_index);
	return pg_index;
}

/* Cache operation against kernel address instead of users */
static int build_dma_sg(struct dmm_map_object *map_obj, unsigned long start,
								size_t len)
{
	struct page *page;
	unsigned long offset;
	ssize_t rest;
	int ret = 0, i = 0;
	unsigned long first_data_page = start >> PAGE_SHIFT;
	unsigned long last_data_page = ((u32)(start + len - 1) >> PAGE_SHIFT);
	/* calculating the number of pages this area spans */
	unsigned long num_pages = last_data_page - first_data_page + 1;
	struct scatterlist *sg;
	int pg_i;

	sg = kcalloc(num_pages, sizeof(*sg), GFP_KERNEL);
	if (!sg) {
		pr_err("%s: kcalloc failed\n", __func__);
		ret = -ENOMEM;
		goto out;
	}

	sg_init_table(sg, num_pages);

	/* cleanup a previous sg allocation */
	/* this may happen if application doesn't signal for e/o DMA */
	kfree(map_obj->dma_info.sg);

	map_obj->dma_info.sg = sg;
	map_obj->dma_info.num_pages = num_pages;

	pg_i = find_first_page_in_cache(map_obj, start);

	while (len) {
		page = get_mapping_page(map_obj, pg_i);
		if (!page) {
			pr_err("%s: no page for %08lx, pg_i is %x\n", __func__,
								start, pg_i);
			ret = -EINVAL;
			goto out;
		} else if (IS_ERR(page)) {
			pr_err("%s: err page for %08lx(%lu)\n", __func__, start,
					PTR_ERR(page));
			ret = PTR_ERR(page);
			goto out;
		}

		offset = start & ~PAGE_MASK;
		rest = min_t(ssize_t, PAGE_SIZE - offset, len);

		sg_set_page(&sg[i], page, rest, offset);

		len -= rest;
		start += rest;
		pg_i++, i++;
	}

	if (i != map_obj->dma_info.num_pages) {
		pr_err("%s: bad number of sg iterations\n", __func__);
		ret = -EFAULT;
		goto out;
	}

out:
	return ret;
}

static int memory_regain_ownership(struct device *dev,
		struct dmm_map_object *map_obj, unsigned long start,
		ssize_t len, enum dma_data_direction dir)
{
	int ret = 0;
	unsigned long first_data_page = start >> PAGE_SHIFT;
	unsigned long last_data_page = ((u32)(start + len - 1) >> PAGE_SHIFT);
	/* calculating the number of pages this area spans */
	unsigned long num_pages = last_data_page - first_data_page + 1;
	struct device_dma_map_info *dma_info = &map_obj->dma_info;
	long pg_i;

	if (!dma_info->sg)
		goto out;

	if (num_pages > dma_info->num_pages) {
		pr_err("%s: dma info params invalid\n", __func__);
		return -EINVAL;
	}

	pg_i = find_first_page_in_cache(map_obj, start);
	if (pg_i == -1) {
		ret = -EFAULT;
		goto out;
	}
	dma_unmap_sg(dev, (dma_info->sg), num_pages, dir);

	pr_debug("%s: dma_map_sg unmapped\n", __func__);

out:
	return ret;
}

/* Cache operation against kernel address instead of users */
static int memory_give_ownership(struct device *dev,
		struct dmm_map_object *map_obj, unsigned long start,
		ssize_t len, enum dma_data_direction dir)
{
	int ret, sg_num;
	struct device_dma_map_info *dma_info = &map_obj->dma_info;
	unsigned long first_data_page = start >> PAGE_SHIFT;
	unsigned long last_data_page = ((u32)(start + len - 1) >> PAGE_SHIFT);
	/* calculating the number of pages this area spans */
	unsigned long num_pages = last_data_page - first_data_page + 1;
	long pg_i;

	pg_i = find_first_page_in_cache(map_obj, start);
	if (pg_i == -1) {
		ret = -EFAULT;
		goto out;
	}

	sg_num = dma_map_sg(dev, (dma_info->sg), num_pages, dir);
	if (sg_num < 1) {
		pr_err("%s: dma_map_sg failed: %d\n", __func__, sg_num);
		ret = -EFAULT;
		goto out;
	}

	pr_debug("%s: dma_map_sg mapped %d elements\n", __func__, sg_num);

	return 0;
out:
	return ret;
}
#endif

int proc_begin_dma(struct iodmm_struct *obj, const void __user *args)
{
	int status = 0;
	struct dmm_dma_info dma_info;
#ifdef CONFIG_DMM_DMA_API
	struct dmm_map_object *map_obj;
	struct device *dev;

	if (copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info)))
		return -EFAULT;
	dev = obj->iovmm->iommu->dev;

	mutex_lock(&obj->iovmm->dmm_map_lock);
	pr_debug("%s: addr 0x%x, size 0x%x, type %d\n", __func__,
						(u32)dma_info.pva,
						dma_info.ul_size, dma_info.dir);
	/* find requested memory are in cached mapping information */
	map_obj = find_containing_mapping(obj, (u32)dma_info.pva, 0, true,
							dma_info.ul_size);
	if (!map_obj) {
		pr_err("%s: find_containing_mapping failed\n", __func__);
		status = -EFAULT;
		goto err_out;
	}

	if (memory_give_ownership(dev, map_obj, (u32)dma_info.pva,
				dma_info.ul_size, dma_info.dir)) {
		pr_err("%s: InValid address parameters %x %x\n",
			       __func__, (u32)dma_info.pva, dma_info.ul_size);
		status = -EFAULT;
	}

err_out:
	mutex_unlock(&obj->iovmm->dmm_map_lock);
#else
	if (copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info)))
		return -EFAULT;
	status = temp_user_dma_op((u32)dma_info.pva,
			(u32)dma_info.pva + dma_info.ul_size, 3);
#endif
	return status;
}

int proc_end_dma(struct iodmm_struct *obj,  const void __user *args)
{
	int status = 0;
	struct dmm_dma_info dma_info;
#ifdef CONFIG_DMM_DMA_API
	struct device *dev;
	struct dmm_map_object *map_obj;

	if (copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info)))
		return -EFAULT;
	dev = obj->iovmm->iommu->dev;

	pr_debug("%s: addr 0x%x, size 0x%x, type %d\n", __func__,
						(u32)dma_info.pva,
						dma_info.ul_size, dma_info.dir);
	mutex_lock(&obj->iovmm->dmm_map_lock);

	/* find requested memory are in cached mapping information */
	map_obj = find_containing_mapping(obj, (u32)dma_info.pva, 0, true,
							dma_info.ul_size);
	if (!map_obj) {
		pr_err("%s: find_containing_mapping failed\n", __func__);
		status = -EFAULT;
		goto err_out;
	}

	if (memory_regain_ownership(dev, map_obj, (u32)dma_info.pva,
					dma_info.ul_size, dma_info.dir)) {
		pr_err("%s: InValid address parameters %p %x\n",
		       __func__, dma_info.pva, dma_info.ul_size);
		status = -EFAULT;
		goto err_out;
	}

err_out:
	mutex_unlock(&obj->iovmm->dmm_map_lock);
#else
	if (copy_from_user(&dma_info, (void __user *)args,
						sizeof(struct dmm_dma_info)))
		return -EFAULT;
	status = temp_user_dma_op((u32)dma_info.pva,
				(u32)dma_info.pva + dma_info.ul_size, 1);
#endif
	return status;
}

/**
 * user_to_device_unmap() - unmaps Device virtual buffer.
 * @mmu:	Pointer to iommu handle.
 * @da		DSP address
 *
 * This function unmaps a user space buffer into DSP virtual address.
 *
 */
static int user_to_device_unmap(struct iommu *mmu, u32 da, unsigned size)
{
	unsigned total = size;
	unsigned start = da;

	while (total > 0) {
		size_t bytes;
		bytes = iopgtable_clear_entry(mmu, start);
		if (bytes == 0)
			bytes = PAGE_SIZE;
		else
			dev_dbg(mmu->dev, "%s: unmap 0x%x 0x%x\n",
				__func__, start, bytes);
		BUG_ON(!IS_ALIGNED(bytes, PAGE_SIZE));
		total -= bytes;
		start += bytes;
	}
	return 0;
}

static int __user_un_map(struct iodmm_struct *obj, u32 map_addr)
{
	int status = 0;
	u32 va_align;
	u32 size_align;
	struct dmm_map_object *map_obj;
	int i;
	struct page *pg;

	va_align = round_down(map_addr, PAGE_SIZE);

	mutex_lock(&obj->iovmm->dmm_map_lock);
	/*
	* Update DMM structures. Get the size to unmap.
	* This function returns error if the VA is not mapped
	*/
	/* find requested memory are in cached mapping information */
	map_obj = find_containing_mapping(obj, 0, map_addr, false, 0);
	if (!map_obj)
		goto err;
	size_align = map_obj->size;
	/* Remove mapping from the page tables. */
	status = user_to_device_unmap(obj->iovmm->iommu, va_align,
							size_align);
	if (status)
		goto err;

	i = size_align/PAGE_SIZE;
	while (i--) {
		pg = map_obj->pages[i];
		if (pg && pfn_valid(page_to_pfn(pg))) {
			if (page_count(pg) < 1)
				pr_info("%s UNMAP FAILURE !!!\n", __func__);
			else {
				SetPageDirty(pg);
				page_cache_release(pg);
			}
		}
	}
	/*
	* A successful unmap should be followed by removal of map_obj
	* from dmm_map_list, so that mapped memory resource tracking
	* remains uptodate
	*/
	remove_mapping_information(obj, map_obj->da, map_obj->size);
err:
	mutex_unlock(&obj->iovmm->dmm_map_lock);
	return status;
}


/**
 * user_un_map - Removes User's mapped address
 * @obj:	target dmm object
 * @args	Mapped address that needs to be unmapped
 *
 * removes user's dmm buffer mapping
 **/
int user_un_map(struct iodmm_struct *obj, const void __user *args)
{
	int status = 0;
	u32 map_addr;

	if (copy_from_user(&map_addr, (void __user *)args, sizeof(u32)))
		return -EFAULT;

	status = __user_un_map(obj, map_addr);
	if (status)
		pr_err("%s:Unmap of buffer 0x%x failedn", __func__, map_addr);

	return status;
}

/**
 * user_to_device_map() - maps user to dsp virtual address
 * @mmu:	Pointer to iommu handle.
 * @uva:		Virtual user space address.
 * @da		DSP address
 * @size		Buffer size to map.
 * @usr_pgs	struct page array pointer where the user pages will be stored
 *
 * This function maps a user space buffer into DSP virtual address.
 *
 */
static int user_to_device_map(struct iommu *mmu, u32 uva, u32 da, u32 size,
						struct page **usr_pgs)

{
	int res = 0;
	int w;
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	u32 pg_num;
	u32 status;
	int pg_i;
	u32 pa;
	unsigned int pages;
	struct iotlb_entry tlb_entry;
	struct page *mapped_page;

	if (!size || !usr_pgs)
		return -EINVAL;

	pages = size / PAGE_SIZE;

	vma = find_vma(mm, uva);

	if (vma->vm_flags & (VM_WRITE | VM_MAYWRITE))
		w = 1;

	for (pg_i = 0; pg_i < pages; pg_i++) {
		pg_num = get_user_pages(current, mm, uva, 1,
						w, 1, &mapped_page, NULL);
		if (pg_num > 0) {
			if (page_count(mapped_page) < 1) {
				pr_err("Bad page count after doing"
					"get_user_pages on"
					"user buffer\n");
				break;
			}
			pa = page_to_phys(mapped_page);
			iotlb_init_entry(&tlb_entry, da, pa,
						MMU_CAM_PGSZ_4K |
						MMU_RAM_ENDIAN_LITTLE |
						MMU_RAM_ELSZ_32);

			iopgtable_store_entry(mmu, &tlb_entry);
			if (usr_pgs)
				usr_pgs[pg_i] = mapped_page;
			da += PAGE_SIZE;
			uva += PAGE_SIZE;
		} else {
			pr_err("get_user_pages FAILED,"
				"MPU addr = 0x%x,"
				"vma->vm_flags = 0x%lx,"
				"get_user_pages Err"
				"Value = %d, Buffer"
				"size=0x%x\n", uva,
				vma->vm_flags, pg_num,
				size);
			status = -EFAULT;
			break;
		}
	}

	return res;
}

/**
 * phys_to_device_map() - maps physical addr
 * to device virtual address
 * @mmu:       Pointer to iommu handle.
 * @uva:               Virtual user space address.
 * @da         DSP address
 * @size               Buffer size to map.
 * @usr_pgs    struct page array pointer where the user pages will be stored
 *
 * This function maps a user space buffer into DSP virtual address.
 *
 */
static int phys_to_device_map(struct iodmm_struct *obj,
				int pool_id, u32 *mapped_addr,
				u32 pa, size_t bytes, u32 flags)
{
	struct iotlb_entry e;
	struct dmm_map_object *dmm_obj;
	int da;
	u32 all_bits;
	int err = 0;
	u32 pg_size[] = {SZ_16M, SZ_1M, SZ_64K, SZ_4K};
	int size_flag[] = {MMU_CAM_PGSZ_16M, MMU_CAM_PGSZ_1M,
				MMU_CAM_PGSZ_64K, MMU_CAM_PGSZ_4K};
	int i;
	struct gen_pool *gen_pool;

	if (!bytes) {
		err = -EINVAL;
		goto exit;
	}

	if (pool_id == -1) {
		da = round_down(*mapped_addr, PAGE_SIZE);
		gen_pool = NULL;
	} else {
		/* search through the list of available pools to
		 * pool handle
		 */
		gen_pool = get_pool_handle(obj->iovmm, pool_id);
		if (gen_pool) {
			da = gen_pool_alloc(gen_pool, bytes);
			*mapped_addr = (da | (pa & (PAGE_SIZE - 1)));
		} else {
			err = -EFAULT;
			goto exit;
		}
	}

	dmm_obj = add_mapping_info(obj, gen_pool, pa, *mapped_addr, bytes);
	if (dmm_obj == NULL) {
		err = -ENODEV;
		goto err_add_map;
	}

	while (bytes) {
		/*
		 * To find the max. page size with which both PA & VA are
		 * aligned
		 */
		all_bits = pa | da;
		for (i = 0; i < 4; i++) {
			if ((bytes >= pg_size[i]) && ((all_bits &
						(pg_size[i] - 1)) == 0)) {
				iotlb_init_entry(&e, da, pa,
						size_flag[i] |
						MMU_RAM_ENDIAN_LITTLE |
						MMU_RAM_ELSZ_32);
				iopgtable_store_entry(obj->iovmm->iommu, &e);
				bytes -= pg_size[i];
				da += pg_size[i];
				pa += pg_size[i];
				break;
			}
		}
	}
	return 0;

err_add_map:
	gen_pool_free(gen_pool, da, bytes);
exit:
	return err;
}

/**
 * dmm_user - Maps user buffer to Device address
 * @obj:	target dmm object
 * @args:	DMM map information
 *
 * Maps given user buffer to Device address
 **/
int dmm_user(struct iodmm_struct *obj, void __user *args)
{
	struct gen_pool *gen_pool;
	struct dmm_map_object *dmm_obj;
	struct iovmm_device *iovmm_obj = obj->iovmm;
	u32 addr_align, da_align, size_align, tmp_addr;
	int err = 0;
	int i, num_of_pages;
	struct page *pg;
	struct vm_area_struct *vma;
	struct mm_struct *mm = current->mm;
	u32 io_addr;
	struct  dmm_map_info map_info;
	struct iotlb_entry e;


	if (copy_from_user(&map_info, (void __user *)args,
						sizeof(struct dmm_map_info)))
		return -EFAULT;

	/*
	 * Important Note: va is mapped from user application process
	 * to current process - it must lie completely within the current
	 * virtual memory address space in order to be of use to us here!
	 */
	down_read(&mm->mmap_sem);

	/* Calculate the page-aligned PA, VA and size */
	addr_align = round_down((u32) map_info.mpu_addr, PAGE_SIZE);
	size_align = round_up(map_info.size + map_info.mpu_addr - addr_align,
								PAGE_SIZE);

	mutex_lock(&iovmm_obj->dmm_map_lock);

	/*
	 * User passed physical address to map. No DMM pool
	 * specified if pool_id as -1, so the da is interpreted
	 * as the Device Address.
	 */
	if (map_info.flags == DMM_DA_PHYS) {
		err = phys_to_device_map(obj, map_info.pool_id, map_info.da,
					addr_align, size_align, map_info.flags);
		goto exit;
	}

	vma = find_vma(mm, map_info.mpu_addr);
	if (vma) {
		dev_dbg(iovmm_obj->iommu->dev,
			"VMAfor UserBuf: ul_mpu_addr=%x, ul_num_bytes=%x, "
			"vm_start=%lx, vm_end=%lx, vm_flags=%lx\n",
			map_info.mpu_addr,
			map_info.size, vma->vm_start, vma->vm_end,
			vma->vm_flags);
	}
	/*
	 * It is observed that under some circumstances, the user buffer is
	 * spread across several VMAs. So loop through and check if the entire
	 * user buffer is covered
	 */
	while ((vma) && (map_info.mpu_addr + map_info.size > vma->vm_end)) {
		/* jump to the next VMA region */
		vma = find_vma(mm, vma->vm_end + 1);
		dev_dbg(iovmm_obj->iommu->dev,
			"VMA for UserBuf ul_mpu_addr=%x ul_num_bytes=%x, "
			"vm_start=%lx, vm_end=%lx, vm_flags=%lx\n",
			map_info.mpu_addr,
			map_info.size, vma->vm_start, vma->vm_end,
			vma->vm_flags);
	}
	if (!vma) {
		pr_err("%s: Failed to get VMA region for 0x%x (%d)\n",
		       __func__, map_info.mpu_addr, map_info.size);
		err = -EINVAL;
		goto exit;
	}

	/*
	 * If user provided anonymous address, then don't allocate it from
	 * from genpool
	 */
	if (map_info.flags == DMM_DA_ANON) {
		gen_pool = NULL;
		da_align = round_down((u32)map_info.da, PAGE_SIZE);
	} else  {
		/* search through the list of available pools to
		 * pool handle
		 */
		gen_pool = get_pool_handle(iovmm_obj, map_info.pool_id);
		if (gen_pool)
			da_align = gen_pool_alloc(gen_pool, size_align);
		else {
			err = -EFAULT;
			goto exit;
		}
	}

	/* Mapped address = MSB of VA | LSB of PA */
	tmp_addr = (da_align | ((u32)map_info.mpu_addr & (PAGE_SIZE - 1)));
	dmm_obj = add_mapping_info(obj, gen_pool, map_info.mpu_addr, tmp_addr,
							size_align);
	if (!dmm_obj)
		goto exit;

	*map_info.da = tmp_addr;

	/* Mapping the IO buffers */
	if (vma->vm_flags & VM_IO) {
		num_of_pages = size_align/PAGE_SIZE;
		for (i = 0; i < num_of_pages; i++) {
			io_addr = __user_va2_pa(current->mm, addr_align);
			pg = phys_to_page(io_addr);

			iotlb_init_entry(&e, da_align, io_addr,
						MMU_CAM_PGSZ_4K |
						MMU_RAM_ENDIAN_LITTLE |
						MMU_RAM_ELSZ_32);
			iopgtable_store_entry(obj->iovmm->iommu, &e);
			da_align += PAGE_SIZE;
			addr_align += PAGE_SIZE;
			dmm_obj->pages[i] = pg;
		}
		err = 0;
		goto exit;
	}

	/* Mapping the Userspace buffer */
	err = user_to_device_map(iovmm_obj->iommu, addr_align,
				da_align, size_align, dmm_obj->pages);
	if (err) {
		/* clean the entries that were mapped */
		__user_un_map(obj, tmp_addr);
		goto exit;
	}
#ifdef CONFIG_DMM_DMA_API
	/*
	 * Build the SG list that would be required for dma map and
	 * unmap APIs
	 */
	err = build_dma_sg(dmm_obj, map_info.mpu_addr, map_info.size);
	if (!err) {
		/*
		 * calling dma_map_sg(cache flush) is essential for
		 * dma_unmap_sg to work since the sg->dma_address required
		 * for dma_unmap_sg is built during dma_map_sg call.
		 */
		err = memory_give_ownership(iovmm_obj->iommu->dev, dmm_obj,
			map_info.mpu_addr, map_info.size, DMA_BIDIRECTIONAL);
	}
#endif

exit:
	copy_to_user((void __user *)args, &map_info,
					sizeof(struct dmm_map_info));
	mutex_unlock(&iovmm_obj->dmm_map_lock);
	up_read(&mm->mmap_sem);
	return err;
}

/**
 * user_remove_resources - Removes User's dmm resources
 * @obj:	target dmm object
 *
 * removes user's dmm resources
 **/
void user_remove_resources(struct iodmm_struct *obj)
{

	struct dmm_map_object *temp_map, *map_obj;
	int status = 0;

	/* Free DMM mapped memory resources */
	list_for_each_entry_safe(map_obj, temp_map, &obj->map_list, link) {
		status = __user_un_map(obj, map_obj->da);
		if (status) {
			pr_err("%s: proc_un_map failed!"
				" status = 0x%x\n", __func__, status);
		}
	}
}

/**
 * omap_create_dmm_pool - Create DMM pool
 * @obj:	target dmm object
 * @args	pool information
 **/
int omap_create_dmm_pool(struct iodmm_struct *obj, const void __user *args)
{
	struct iovmm_pool *pool;
	struct iovmm_device *iovmm = obj->iovmm;
	struct iovmm_pool_info pool_info;

	if (copy_from_user(&pool_info, args, sizeof(struct iovmm_pool_info)))
		return -EFAULT;

	pool = kzalloc(sizeof(struct iovmm_pool), GFP_KERNEL);
	if (!pool)
		return -EFAULT;

	pool->pool_id = pool_info.pool_id;
	pool->da_begin = pool_info.da_begin;
	pool->da_end = pool_info.da_begin + pool_info.size;

	pool->genpool = gen_pool_create(12, -1);
	gen_pool_add(pool->genpool, pool->da_begin,  pool_info.size, -1);

	INIT_LIST_HEAD(&pool->list);
	list_add_tail(&pool->list, &iovmm->mmap_pool);

	return 0;
}

/**
 * omap_delete_dmm_pool - Delete DMM pools
 * @obj:	target dmm object
 **/
int omap_delete_dmm_pools(struct iodmm_struct *obj)
{
	struct iovmm_pool *pool;
	struct iovmm_device *iovmm_obj = obj->iovmm;
	struct list_head *_pool, *_next_pool;

	list_for_each_safe(_pool, _next_pool, &iovmm_obj->mmap_pool) {
		pool = list_entry(_pool, struct iovmm_pool, list);
		gen_pool_destroy(pool->genpool);
		list_del(&pool->list);
		kfree(pool);
	}

	return 0;
}

/**
 * register_mmufault - Register for MMU fault notification
 * @obj:	target dmm object
 * @args:	Eventfd information
 *
 * Registering to MMU fault event notification
 **/
int register_mmufault(struct iodmm_struct *obj, const void __user *args)
{
	int fd;
	struct iommu_event_ntfy *fd_reg;

	if (copy_from_user(&fd, args, sizeof(int)))
		return -EFAULT;

	fd_reg = kzalloc(sizeof(struct iommu_event_ntfy), GFP_KERNEL);
	fd_reg->fd = fd;
	fd_reg->evt_ctx = eventfd_ctx_fdget(fd);
	INIT_LIST_HEAD(&fd_reg->list);
	spin_lock_irq(&obj->iovmm->iommu->event_lock);
	list_add_tail(&fd_reg->list, &obj->iovmm->iommu->event_list);
	spin_unlock_irq(&obj->iovmm->iommu->event_lock);

	return 0;
}

/**
 * unregister_mmufault - Unregister for MMU fault notification
 * @obj:	target dmm object
 * @args:	Eventfd information
 *
 * Unregister to MMU fault event notification
 **/
int unregister_mmufault(struct iodmm_struct *obj, const void __user *args)
{
	int fd;
	struct iommu_event_ntfy *fd_reg, *temp_reg;

	if (copy_from_user(&fd, (void __user *)args, sizeof(int)))
		return -EFAULT;

	/* Free DMM mapped memory resources */
	spin_lock_irq(&obj->iovmm->iommu->event_lock);
	list_for_each_entry_safe(fd_reg, temp_reg,
			&obj->iovmm->iommu->event_list, list) {
		if (fd_reg->fd == fd) {
			list_del(&fd_reg->list);
			kfree(fd_reg);
		}
	}
	spin_unlock_irq(&obj->iovmm->iommu->event_lock);

	return 0;
}

/**
 * program_tlb_entry - Program the IOMMU TLB entry
 * @obj:	target dmm object
 * @args:	TLB entry information
 *
 * This function loads the TLB entry that the user specifies.
 * This function should be used only during remote Processor
 * boot time.
 **/
int program_tlb_entry(struct iodmm_struct *obj, const void __user *args)
{
	struct iotlb_entry e;
	int ret;

	if (copy_from_user(&e, args, sizeof(struct iotlb_entry)))
		return -EFAULT;

	ret = load_iotlb_entry(obj->iovmm->iommu, &e);
	return ret;
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Userspace DMM to IOMMU");
MODULE_AUTHOR("Hari Kanigeri");
MODULE_AUTHOR("Ramesh Gupta");
