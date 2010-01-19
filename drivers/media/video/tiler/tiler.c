/*
 * tiler.c
 *
 * TILER driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>            /* struct cdev */
#include <linux/kdev_t.h>          /* MKDEV() */
#include <linux/fs.h>              /* register_chrdev_region() */
#include <linux/device.h>          /* struct class */
#include <linux/platform_device.h> /* platform_device() */
#include <linux/err.h>             /* IS_ERR() */
#include <linux/uaccess.h>         /* copy_to_user */
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>         /* page_cache_release() */

#include "tiler.h"
#include "tiler_def.h"
#include "dmm_2d_alloc.h"
#include "../dmm/dmm.h"

struct tiler_dev {
	struct cdev cdev;
};

struct platform_driver tiler_driver_ldm = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "tiler",
	},
	.probe = NULL,
	.shutdown = NULL,
	.remove = NULL,
};

struct map {
	struct list_head list;
	void *ssptr;   /* system space (L3) tiler addr */
	u32 num_pgs;   /* number of user space pages to be mapped */
	u32 usr;       /* user space address */
	void *va;      /* dma_alloc_coherent virt addr */
	u32 *va_align; /* 16 byte aligned dma_alloc_coherent virt addr */
	dma_addr_t pa; /* dma_alloc_coherent phys addr */
	u32 size;      /* dma_alloc_coherent size */
	u32 *pages;    /* list of pages */
};

struct __buf_info {
	struct list_head list;
	struct tiler_buf_info buf_info;
};

struct mem_info {
	struct list_head list;
	u32 sys_addr;          /* system space (L3) tiler addr */
	u32 *page;             /* virt addr to page-list */
	dma_addr_t page_pa;    /* phys addr to page-list */
	u32 size;              /* size of page-list */
	u32 num_pgs;           /* number of pages in page-list */
	s8 mapped;
};

static s32 tiler_major;
static s32 tiler_minor;
static struct tiler_dev *tiler_device;
static struct class *tilerdev_class;
static u32 id;
static struct map map_list;
static struct __buf_info buf_list;
static struct mem_info mem_list;
static struct mutex mtx;

/* TODO: required by container algorithm */
static struct dmmTILERContCtxT *tilctx;

static s32 set_area(enum tiler_fmt fmt, u32 width, u32 height,
				u32 *x_area, u32 *y_area)
{
	s32 x_pagedim = 0, y_pagedim = 0;
	u16 tiled_pages_per_ss_page = 0;

	switch (fmt) {
	case TILFMT_8BIT:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_8;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_8;
		tiled_pages_per_ss_page = TILER_PAGE / x_pagedim;
		break;
	case TILFMT_16BIT:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_16;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_16;
		tiled_pages_per_ss_page = TILER_PAGE / x_pagedim / 2;
		break;
	case TILFMT_32BIT:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_32;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_32;
		tiled_pages_per_ss_page = TILER_PAGE / x_pagedim / 4;
		break;
	case TILFMT_PAGE:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_8;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_8;
		width = ((width + TILER_PAGE - 1)/TILER_PAGE);
		tiled_pages_per_ss_page = 1;

		/* for 1D blocks larger than the container width, we need to
		   allocate multiple rows */
		if (width > TILER_WIDTH) {
			height = (width + TILER_WIDTH - 1) / TILER_WIDTH;
			width = TILER_WIDTH;
		} else {
			height = 1;
		}

		height *= x_pagedim;
		width  *= y_pagedim;
		break;
	default:
		return -1;
		break;
	}

	*x_area = (width + x_pagedim - 1) / x_pagedim - 1;
	*y_area = (height + y_pagedim - 1) / y_pagedim - 1;

	tiled_pages_per_ss_page = 64;
	*x_area = ((*x_area + tiled_pages_per_ss_page) &
					~(tiled_pages_per_ss_page - 1)) - 1;

	if (*x_area > TILER_WIDTH || *y_area > TILER_HEIGHT)
		return -1;
	return 0x0;
}

static s32 get_area(u32 sys_addr, u32 *x_area, u32 *y_area)
{
	enum tiler_fmt fmt;

	sys_addr &= TILER_ALIAS_VIEW_CLEAR;
	fmt = TILER_GET_ACC_MODE(sys_addr);

	switch (fmt + 1) {
	case TILFMT_8BIT:
		*x_area = DMM_HOR_X_PAGE_COOR_GET_8(sys_addr);
		*y_area = DMM_HOR_Y_PAGE_COOR_GET_8(sys_addr);
		break;
	case TILFMT_16BIT:
		*x_area = DMM_HOR_X_PAGE_COOR_GET_16(sys_addr);
		*y_area = DMM_HOR_Y_PAGE_COOR_GET_16(sys_addr);
		break;
	case TILFMT_32BIT:
		*x_area = DMM_HOR_X_PAGE_COOR_GET_32(sys_addr);
		*y_area = DMM_HOR_Y_PAGE_COOR_GET_32(sys_addr);
		break;
	case TILFMT_PAGE:
		*x_area = (sys_addr & 0x7FFFFFF) >> 12;
		*y_area = *x_area / 256;
		*x_area &= 255;
		break;
	default:
		return -EFAULT;
	}
	return 0x0;
}

static u32 get_alias_addr(enum tiler_fmt fmt, u16 x, u16 y)
{
	u32 acc_mode = -1;
	u32 x_shft = -1, y_shft = -1;

	switch (fmt) {
	case TILFMT_8BIT:
		acc_mode = 0; x_shft = 6; y_shft = 20;
		break;
	case TILFMT_16BIT:
		acc_mode = 1; x_shft = 7; y_shft = 20;
		break;
	case TILFMT_32BIT:
		acc_mode = 2; x_shft = 7; y_shft = 20;
		break;
	case TILFMT_PAGE:
		acc_mode = 3; y_shft = 8;
		break;
	default:
		return 0;
		break;
	}

	if (fmt == TILFMT_PAGE)
		return (u32)TIL_ALIAS_ADDR((x | y << y_shft) << 12, acc_mode);
	else
		return (u32)TIL_ALIAS_ADDR(x << x_shft | y << y_shft, acc_mode);
}


static s32 tiler_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct __buf_info *_b = NULL;
	struct tiler_buf_info *b = NULL;
	s32 i = 0, j = 0, k = 0, m = 0, p = 0, bpp = 1;
	struct list_head *pos = NULL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	mutex_lock(&mtx);
	list_for_each(pos, &buf_list.list) {
		_b = list_entry(pos, struct __buf_info, list);
		if ((vma->vm_pgoff << PAGE_SHIFT) == _b->buf_info.offset)
			break;
	}
	mutex_unlock(&mtx);
	if (!_b)
		return -EFAULT;

	b = &_b->buf_info;

	for (i = 0; i < b->num_blocks; i++) {
		if (b->blocks[i].fmt >= TILFMT_8BIT &&
					b->blocks[i].fmt <= TILFMT_32BIT) {
			/* get line width */
			bpp = (b->blocks[i].fmt == TILFMT_8BIT ? 1 :
			       b->blocks[i].fmt == TILFMT_16BIT ? 2 : 4);
			p = (b->blocks[i].dim.area.width * bpp +
				TILER_PAGE - 1) & ~(TILER_PAGE - 1);

			for (j = 0; j < b->blocks[i].dim.area.height; j++) {
				/* map each page of the line */
				vma->vm_pgoff =
					(b->blocks[i].ssptr + m) >> PAGE_SHIFT;
				if (remap_pfn_range(vma, vma->vm_start + k,
					(b->blocks[i].ssptr + m) >> PAGE_SHIFT,
					p, vma->vm_page_prot))
					return -EAGAIN;
				k += p;
				if (b->blocks[i].fmt == TILFMT_8BIT)
					m += 64*TILER_WIDTH;
				else
					m += 2*64*TILER_WIDTH;
			}
			m = 0;
		} else if (b->blocks[i].fmt == TILFMT_PAGE) {
			vma->vm_pgoff = (b->blocks[i].ssptr) >> PAGE_SHIFT;
			p = (b->blocks[i].dim.len + TILER_PAGE - 1) &
							~(TILER_PAGE - 1);
			if (remap_pfn_range(vma, vma->vm_start + k,
				(b->blocks[i].ssptr) >> PAGE_SHIFT, p,
				vma->vm_page_prot))
				return -EAGAIN;;
			k += p;
		}
	}
	return 0;
}

static s32 map_buffer(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr,
								u32 pg_list)
{
	u32 num_pages = -1, x_area = -1, y_area = -1;
	struct dmmTILERContPageAreaT *area_desc = NULL, __area_desc = {0};
	struct pat pat_desc = {0};
	struct mem_info *mi = NULL;

	/* reserve area in tiler container */
	if (set_area(fmt, width, height, &x_area, &y_area))
		return -EFAULT;

	__area_desc.x1 = (u16)x_area;
	__area_desc.y1 = (u16)y_area;

	area_desc = alloc_2d_area(tilctx, &__area_desc);
	if (!area_desc)
		return -ENOMEM;

	/* formulate system space address */
	*sys_addr = get_alias_addr(fmt, area_desc->x0, area_desc->y0);

	/* use user pages */
	area_desc->xPageCount = area_desc->x1 - area_desc->x0 + 1;
	area_desc->yPageCount = area_desc->y1 - area_desc->y0 + 1;
	num_pages = area_desc->xPageCount * area_desc->yPageCount;

	mi = kmalloc(sizeof(struct mem_info), GFP_KERNEL);
	if (!mi)
		return -ENOMEM;
	memset(mi, 0x0, sizeof(struct mem_info));

	mi->size =  num_pages * 4 + 16;
	mi->page_pa = pg_list;
	mi->num_pgs = num_pages;
	mi->sys_addr = *sys_addr;
	mi->mapped = 1;

	mutex_lock(&mtx);
	list_add(&(mi->list), &mem_list.list);
	mutex_unlock(&mtx);

	/* send pat descriptor to dmm driver */
	pat_desc.area.x0 = area_desc->x0 + area_desc->xPageOfst;
	pat_desc.area.y0 = area_desc->y0 + area_desc->yPageOfst;
	pat_desc.area.x1 = area_desc->x0 + area_desc->xPageOfst +
			   area_desc->xPageCount - 1;
	pat_desc.area.y1 = area_desc->y0 + area_desc->yPageOfst +
			   area_desc->yPageCount - 1;

	pat_desc.ctrl.direction = 0;
	pat_desc.ctrl.initiator = 0;
	pat_desc.ctrl.lut_id = 0;
	pat_desc.ctrl.start = 1;
	pat_desc.ctrl.sync = 0;

	pat_desc.next = NULL;

	/* must be a 16-byte aligned physical address */
	pat_desc.data = mi->page_pa;

	if (dmm_pat_refill(&pat_desc, 0, MANUAL, 0))
		return -ENOMEM;

	return 0x0;
}

s32 tiler_free(u32 sys_addr)
{
	u32 x_area = -1, y_area = -1, i = -1;
	struct dmmTILERContPageAreaT *area_desc =  NULL;
	struct list_head *pos = NULL, *q = NULL;
	struct mem_info *mi = NULL;

	if (get_area(sys_addr, &x_area, &y_area))
		return -EFAULT;

	area_desc = search_2d_area(tilctx, x_area, y_area, 0, 0);
	if (!area_desc)
		return -EFAULT;

	/* freeing of physical pages should *not* happen in this next API */
	if (!(dealloc_2d_area(tilctx, area_desc)))
		return -EFAULT;

	printk("(1),");
	mutex_lock(&mtx);
	list_for_each_safe(pos, q, &mem_list.list) {
		mi = list_entry(pos, struct mem_info, list);
		if (mi->sys_addr == sys_addr) {
			if (!mi->mapped) {
				for (i = 0; i < mi->num_pgs; i++)
					dmm_free_page(mi->page[i]);
				dma_free_coherent(NULL, mi->size, mi->page,
								mi->page_pa);
			}
			list_del(pos);
			kfree(mi);
		}
	}
	mutex_unlock(&mtx);
	printk("(2)\n");
	if (!mi)
		return -EFAULT;

	/* should there be a call to remove the PAT entries? */
	return 0x0;
}
EXPORT_SYMBOL(tiler_free);

static s32 tiler_ioctl(struct inode *ip, struct file *filp, u32 cmd,
			unsigned long arg)
{
	pgd_t *pgd = NULL;
	pmd_t *pmd = NULL;
	pte_t *ptep = NULL, pte = 0x0;
	s32 r = -1, i = -1;
	u32 til_addr = 0x0, write = 0, tmp = -1;

	struct __buf_info *_b = NULL;
	struct tiler_buf_info buf_info = {0};
	struct tiler_block_info block_info = {0};
	struct list_head *pos = NULL, *q = NULL;

	struct map *m = NULL;
	struct page *page = NULL;
	struct task_struct *curr_task = current;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;

	switch (cmd) {
	case TILIOC_GBUF:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		switch (block_info.fmt) {
		case TILFMT_PAGE:
			r = tiler_alloc(block_info.fmt, block_info.dim.len, 1,
				&til_addr);
			if (r)
				return r;
			break;
		case TILFMT_8BIT:
		case TILFMT_16BIT:
		case TILFMT_32BIT:
			r = tiler_alloc(block_info.fmt,
					block_info.dim.area.width,
					block_info.dim.area.height, &til_addr);
			if (r)
				return r;
			break;
		default:
			return -EINVAL;
		}

		block_info.ssptr = til_addr;
		if (copy_to_user((void __user *)arg, &block_info,
					sizeof(struct tiler_block_info)))
			return -EFAULT;
		break;
	case TILIOC_FBUF:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		if (tiler_free(block_info.ssptr))
			return -EFAULT;
		break;
	case TILIOC_GSSP:
		pgd = pgd_offset(current->mm, arg);
		if (!(pgd_none(*pgd) || pgd_bad(*pgd))) {
			pmd = pmd_offset(pgd, arg);
			if (!(pmd_none(*pmd) || pmd_bad(*pmd))) {
				ptep = pte_offset_map(pmd, arg);
				if (ptep) {
					pte = *ptep;
					if (pte_present(pte))
						return (pte & PAGE_MASK) |
							(~PAGE_MASK & arg);
				}
			}
		}
		/* va not in page table */
		return 0x0;
		break;
	case TILIOC_MBUF:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		if (!block_info.ptr)
			return -EFAULT;

		m = kmalloc(sizeof(struct map), GFP_KERNEL);
		if (!m)
			return -ENOMEM;
		memset(m, 0x0, sizeof(struct map));

		m->usr = (u32)block_info.ptr;

		/*
		 * Important Note: block_info.ptr is mapped from user
		 * application process to current process - it must lie
		 * completely within the current virtual memory address
		 * space in order to be of use to us here.
		 */

		down_read(&mm->mmap_sem);
		vma = find_vma(mm, m->usr);

		/*
		 * It is observed that under some circumstances, the user
		 * buffer is spread across several vmas, so loop through
		 * and check if the entire user buffer is covered.
		 */

		while ((vma) && (m->usr + block_info.dim.len > vma->vm_end)) {
			/* jump to the next VMA region */
			vma = find_vma(mm, vma->vm_end + 1);
		}
		if (!vma) {
			printk(KERN_ERR "Failed to get the vma region for \
				user buffer.\n");
			up_read(&mm->mmap_sem);
			kfree(m);
			return -EFAULT;
		}

		m->num_pgs = block_info.dim.len / TILER_PAGE;
		tmp = (m->num_pgs + 63) & ~63;
		m->size =  tmp * 4 + 16;

		m->va = dma_alloc_coherent(NULL, m->size, &m->pa, GFP_ATOMIC);
		if (!m->va) {
			up_read(&mm->mmap_sem);
			kfree(m);
			return -ENOMEM;
		}
		memset(m->va, 0x0, m->size);

		m->pages = kmalloc(m->size, GFP_KERNEL);
		if (!m->pages) {
			up_read(&mm->mmap_sem);
			dma_free_coherent(NULL, m->size, m->va, m->pa);
			kfree(m);
			return -ENOMEM;
		}
		memset(m->pages, 0x0, m->size);

		m->va_align = (u32 *)((((u32)m->va) + 15) & ~15);

		if (vma->vm_flags & (VM_WRITE | VM_MAYWRITE))
			write = 1;

		tmp = m->usr;

		for (i = 0; i < m->num_pgs; i++) {
			if (get_user_pages(curr_task, mm, tmp, 1, write, 1,
						&page, NULL)) {
				if (page_count(page) < 1) {
					printk(KERN_ERR "Bad page count from\
							get_user_pages()\n");
				}
				m->pages[i] = (u32)page;
				m->va_align[i] = page_to_phys(page);
				tmp += TILER_PAGE;
			} else {
				printk(KERN_ERR "get_user_pages() failed\n");
				up_read(&mm->mmap_sem);
				kfree(m->pages);
				dma_free_coherent(NULL, m->size, m->va, m->pa);
				kfree(m);
			}
		}

		up_read(&mm->mmap_sem);

		r = map_buffer(block_info.fmt, block_info.dim.len, 1,
				(u32)(&m->ssptr), (u32)m->pa);
			goto cleanup;

		block_info.ssptr = (u32)m->ssptr;
		mutex_lock(&mtx);
		list_add(&(m->list), &map_list.list);
		mutex_unlock(&mtx);

		if (copy_to_user((void *)arg, (const void *)(&block_info),
					sizeof(struct tiler_block_info)))
			goto cleanup;
		break;
cleanup:
		for (i = 0; i < m->num_pgs; i++) {
			page = (struct page *)m->pages[i];
			if (!PageReserved(page))
				SetPageDirty(page);
			page_cache_release(page);
		}
		kfree(m->pages);
		dma_free_coherent(NULL, m->size, m->va, m->pa);
		kfree(m);
		return r;
	case TILIOC_UMBUF:
		if (copy_from_user((void *)(&block_info), (const void *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		mutex_lock(&mtx);
		list_for_each_safe(pos, q, &map_list.list) {
			m = list_entry(pos, struct map, list);
			if (m->ssptr == (void *)block_info.ssptr) {
				if (tiler_free(block_info.ssptr))
					printk(KERN_ERR "warning: could not \
						free mapped tiler buffer\n");

				for (i = 0; i < m->num_pgs; i++) {
					page = (struct page *)m->pages[i];
					if (!PageReserved(page))
						SetPageDirty(page);
					page_cache_release(page);
				}

				kfree(m->pages);
				dma_free_coherent(NULL, m->size, m->va, m->pa);
				list_del(pos);
				kfree(m);
				break;
			}
		}
		mutex_unlock(&mtx);

		if (!m)
			return -EFAULT;
		break;
	case TILIOC_QBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;

		mutex_lock(&mtx);
		list_for_each(pos, &buf_list.list) {
			_b = list_entry(pos, struct __buf_info, list);
			if (buf_info.offset == _b->buf_info.offset) {
				if (copy_to_user((void __user *)arg,
					&_b->buf_info,
					sizeof(struct tiler_buf_info))) {
					mutex_unlock(&mtx);
					return -EFAULT;
				} else {
					mutex_unlock(&mtx);
					return 0;
				}
			}
		}
		mutex_unlock(&mtx);
		return -EFAULT;
		break;
	case TILIOC_RBUF:
		_b = kmalloc(sizeof(struct __buf_info), GFP_KERNEL);
		if (!_b)
			return -ENOMEM;
		memset(_b, 0x0, sizeof(struct __buf_info));

		if (copy_from_user(&_b->buf_info, (void __user *)arg,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;

		_b->buf_info.offset = id;
		id += 0x1000;

		mutex_lock(&mtx);
		list_add(&(_b->list), &buf_list.list);
		mutex_unlock(&mtx);

		if (copy_to_user((void __user *)arg, &_b->buf_info,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;
		break;
	case TILIOC_URBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;

		mutex_lock(&mtx);
		list_for_each_safe(pos, q, &buf_list.list) {
			_b = list_entry(pos, struct __buf_info, list);
			if (buf_info.offset == _b->buf_info.offset) {
				list_del(pos);
				kfree(_b);
				mutex_unlock(&mtx);
				return 0;
			}
		}
		mutex_unlock(&mtx);
		return -EFAULT;
		break;
	case TILIOC_QUERY_BLK:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		/* TODO: only for d2c, so rework this later */
		/*if (tiler_find_buf(block_info.ssptr, &block_info))
			return -EFAULT;*/

		if (copy_to_user((void __user *)arg, &block_info,
			sizeof(struct tiler_block_info)))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	}
	return 0x0;
}

s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr)
{
	u32 num_pages = -1, i = -1, x_area = -1, y_area = -1;
	struct dmmTILERContPageAreaT *area_desc = NULL, __area_desc = {0};
	struct pat pat_desc = {0};
	struct mem_info *mi = NULL;

	/* reserve area in tiler container */
	if (set_area(fmt, width, height, &x_area, &y_area))
		return -EFAULT;

	__area_desc.x1 = (u16)x_area;
	__area_desc.y1 = (u16)y_area;

	area_desc = alloc_2d_area(tilctx, &__area_desc);
	if (!area_desc)
		return -ENOMEM;

	/* formulate system space address */
	*sys_addr = get_alias_addr(fmt, area_desc->x0, area_desc->y0);

	/* allocate pages */
	area_desc->xPageCount = area_desc->x1 - area_desc->x0 + 1;
	area_desc->yPageCount = area_desc->y1 - area_desc->y0 + 1;
	num_pages = area_desc->xPageCount * area_desc->yPageCount;

	mi = kmalloc(sizeof(struct mem_info), GFP_KERNEL);
	if (!mi)
		return -ENOMEM;
	memset(mi, 0x0, sizeof(struct mem_info));

	mi->size =  num_pages * 4 + 16;
	mi->page = dma_alloc_coherent(NULL, mi->size, &mi->page_pa, GFP_ATOMIC);
	if (!mi->page) {
		kfree(mi); return -ENOMEM;
	}
	memset(mi->page, 0x0, mi->size);

	for (i = 0; i < num_pages; i++) {
		mi->page[i] = dmm_get_page();
		if (mi->page[i] == 0x0)
			goto cleanup;
	}
	mi->num_pgs = num_pages;
	mi->sys_addr = *sys_addr;

	mutex_lock(&mtx);
	list_add(&(mi->list), &mem_list.list);
	mutex_unlock(&mtx);

	/* send pat descriptor to dmm driver */
	pat_desc.area.x0 = area_desc->x0 + area_desc->xPageOfst;
	pat_desc.area.y0 = area_desc->y0 + area_desc->yPageOfst;
	pat_desc.area.x1 = area_desc->x0 + area_desc->xPageOfst +
			   area_desc->xPageCount - 1;
	pat_desc.area.y1 = area_desc->y0 + area_desc->yPageOfst +
			   area_desc->yPageCount - 1;

	pat_desc.ctrl.direction = 0;
	pat_desc.ctrl.initiator = 0;
	pat_desc.ctrl.lut_id = 0;
	pat_desc.ctrl.start = 1;
	pat_desc.ctrl.sync = 0;

	pat_desc.next = NULL;

	/* must be a 16-byte aligned physical address */
	pat_desc.data = mi->page_pa;

	if (dmm_pat_refill(&pat_desc, 0, MANUAL, 0))
		goto cleanup;

	return 0x0;
cleanup:
	dma_free_coherent(NULL, mi->size, mi->page, mi->page_pa);
	kfree(mi);
	return -ENOMEM;
}
EXPORT_SYMBOL(tiler_alloc);

static void __exit tiler_exit(void)
{
	struct __buf_info *_b = NULL;
	struct mem_info *mi = NULL;
	struct list_head *pos = NULL, *q = NULL;
	u32 i = -1;

	/* TODO: remove when replacing container algorithm */
	mutex_destroy(&tilctx->mtx);
	kfree(tilctx);
	/* ------------ */

	/* remove any leftover info structs that haven't been unregistered */
	mutex_lock(&mtx);
	list_for_each_safe(pos, q, &buf_list.list) {
		_b = list_entry(pos, struct __buf_info, list);
		list_del(pos);
		kfree(_b);
	}
	mutex_unlock(&mtx);

	/* free any remaining mem structures */
	mutex_lock(&mtx);
	list_for_each_safe(pos, q, &mem_list.list) {
		mi = list_entry(pos, struct mem_info, list);
		for (i = 0; i < mi->num_pgs; i++)
			dmm_free_page(mi->page[i]);
		dma_free_coherent(NULL, mi->size, mi->page, mi->page_pa);
		list_del(pos);
		kfree(mi);
	}
	mutex_unlock(&mtx);

	mutex_destroy(&mtx);
	platform_driver_unregister(&tiler_driver_ldm);
	cdev_del(&tiler_device->cdev);
	kfree(tiler_device);
	device_destroy(tilerdev_class, MKDEV(tiler_major, tiler_minor));
	class_destroy(tilerdev_class);
}

static s32 tiler_open(struct inode *ip, struct file *filp)
{
	return 0x0;
}

static s32 tiler_release(struct inode *ip, struct file *filp)
{
	return 0x0;
}

static const struct file_operations tiler_fops = {
	.open    = tiler_open,
	.ioctl   = tiler_ioctl,
	.release = tiler_release,
	.mmap    = tiler_mmap,
};

static s32 __init tiler_init(void)
{
	dev_t dev  = 0;
	s32 r = -1;
	struct device *device = NULL;

	if (tiler_major) {
		dev = MKDEV(tiler_major, tiler_minor);
		r = register_chrdev_region(dev, 1, "tiler");
	} else {
		r = alloc_chrdev_region(&dev, tiler_minor, 1, "tiler");
		tiler_major = MAJOR(dev);
	}

	tiler_device = kmalloc(sizeof(struct tiler_dev), GFP_KERNEL);
	if (!tiler_device) {
		unregister_chrdev_region(dev, 1);
		return -ENOMEM;
	}
	memset(tiler_device, 0x0, sizeof(struct tiler_dev));

	cdev_init(&tiler_device->cdev, &tiler_fops);
	tiler_device->cdev.owner = THIS_MODULE;
	tiler_device->cdev.ops   = &tiler_fops;

	r = cdev_add(&tiler_device->cdev, dev, 1);
	if (r)
		printk(KERN_ERR "cdev_add():failed\n");

	tilerdev_class = class_create(THIS_MODULE, "tiler");

	if (IS_ERR(tilerdev_class)) {
		printk(KERN_ERR "class_create():failed\n");
		goto EXIT;
	}

	device = device_create(tilerdev_class, NULL, dev, NULL, "tiler");
	if (device == NULL)
		printk(KERN_ERR "device_create() fail\n");

	r = platform_driver_register(&tiler_driver_ldm);

	/* tiler context structure required by container algorithm */
	tilctx = kmalloc(sizeof(struct dmmTILERContCtxT), GFP_KERNEL);
	if (!tilctx)
		return -ENOMEM;
	memset(tilctx, 0x0, sizeof(struct dmmTILERContCtxT));
	tilctx->contSizeX = TILER_WIDTH;
	tilctx->contSizeY = TILER_HEIGHT;
	mutex_init(&tilctx->mtx);
	/* ------------ */

	mutex_init(&mtx);
	INIT_LIST_HEAD(&buf_list.list);
	INIT_LIST_HEAD(&mem_list.list);
	id = 0xda7a000;

EXIT:
	return r;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("davidsin@ti.com");
module_init(tiler_init);
module_exit(tiler_exit);
