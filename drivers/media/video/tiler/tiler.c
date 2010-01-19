/*
 * tiler.c
 *
 * tiler driver support functions for TI OMAP processors.
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
	u32 sys_addr;      /* system space (L3) tiler addr */
	void *dma_va;      /* dma_alloc_coherent virt addr */
	dma_addr_t dma_pa; /* dma_alloc_coherent phys addr */
	u32 size;          /* dma_alloc_coherent size */
	u32 *page;         /* list of pages */
	u32 num_pgs;       /* number of pages */
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

/* TODO: remove */
static struct dmmTILERContCtxT *tilctx;

static s32 tiler_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct __buf_info *_b = NULL;
	struct tiler_buf_info *b = NULL;
	s32 i = 0, j = 0, k = 0, m = 0, p = 0;
	s32 bpp = 1;
	struct list_head *pos = NULL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	list_for_each(pos, &buf_list.list) {
		_b = list_entry(pos, struct __buf_info, list);
		if ((vma->vm_pgoff << PAGE_SHIFT) == _b->buf_info.offset)
			break;
	}
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

static s32 tiler_ioctl(struct inode *ip, struct file *filp, u32 cmd,
			unsigned long arg)
{
	pgd_t *pgd = NULL;
	pmd_t *pmd = NULL;
	pte_t *ptep = NULL, pte = 0x0;
	s32 r = -1;
	u32 til_addr = 0x0;

	struct __buf_info *_b = NULL;
	struct tiler_buf_info buf_info = {0}; //*info = NULL;
	struct tiler_block_info block_info = {0};
	struct list_head *pos = NULL, *q = NULL;

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
		break;
	case TILIOC_QBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;

		list_for_each(pos, &buf_list.list) {
			_b = list_entry(pos, struct __buf_info, list);
			if (buf_info.offset == _b->buf_info.offset) {
				if (copy_to_user((void __user *)arg,
					&_b->buf_info,
					sizeof(struct tiler_buf_info)))
					return -EFAULT;
				else
					return 0;
			}
		}
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

		list_add(&(_b->list), &buf_list.list);

		if (copy_to_user((void __user *)arg, &_b->buf_info,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;
		break;
	case TILIOC_URBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(struct tiler_buf_info)))
			return -EFAULT;

		list_for_each_safe(pos, q, &buf_list.list) {
			_b = list_entry(pos, struct __buf_info, list);
			if (buf_info.offset == _b->buf_info.offset) {
				mutex_lock(&mtx);
				list_del(pos);
				mutex_unlock(&mtx);
				kfree(_b);
				return 0;
			}
		}
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
	mi->dma_va = dma_alloc_coherent(NULL, mi->size, &mi->dma_pa,
								GFP_ATOMIC);
	if (!mi->dma_va) {
		kfree(mi); return -ENOMEM;
	}
	memset(mi->dma_va, 0x0, mi->size);

	mi->page = (u32 *)(((u32)mi->dma_va + 15) & ~15);

	for (i = 0; i < num_pages; i++) {
		mi->page[i] = dmm_get_page();
		if (mi->page[i] == 0x0)
			goto cleanup;
	}
	mi->num_pgs = num_pages;
	mi->sys_addr = *sys_addr;
	list_add(&(mi->list), &mem_list.list);

	/* send pat desc to dmm driver */
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
	pat_desc.data = mi->dma_pa;

	if (dmm_pat_refill(&pat_desc, 0, MANUAL, 0))
		goto cleanup;

	return 0x0;
cleanup:
	dma_free_coherent(NULL, mi->size, mi->dma_va, mi->dma_pa);
	kfree(mi);
	return -ENOMEM;
}
EXPORT_SYMBOL(tiler_alloc);

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

	printk(KERN_NOTICE "(1),");
	list_for_each_safe(pos, q, &mem_list.list) {
		mi = list_entry(pos, struct mem_info, list);
		if (mi->sys_addr == sys_addr) {
			mutex_lock(&mtx);
			for (i = 0; i < mi->num_pgs; i++)
				dmm_free_page(mi->page[i]);
			mutex_unlock(&mtx);

			dma_free_coherent(NULL, mi->size, mi->dma_va,
								mi->dma_pa);

			mutex_lock(&mtx);
			list_del(pos);
			mutex_unlock(&mtx);

			kfree(mi);
		}
	}
	printk(KERN_NOTICE "(2),");
	if (!mi)
		return -EFAULT;

	/* should there be a call to remove the PAT entries? */

	printk(KERN_NOTICE "(3)\n");
	return 0x0;
}
EXPORT_SYMBOL(tiler_free);

static void __exit tiler_exit(void)
{
	struct __buf_info *_b = NULL;
	struct mem_info *mi = NULL;
	struct list_head *pos = NULL, *q = NULL;
	u32 i = -1;

	/* TODO: remove */
	mutex_destroy(&tilctx->mtx);
	kfree(tilctx);
	/* ------------ */

	/* remove any leftover info structs that haven't been unregistered */
	list_for_each_safe(pos, q, &buf_list.list) {
		_b = list_entry(pos, struct __buf_info, list);
		mutex_lock(&mtx);
		list_del(pos);
		mutex_unlock(&mtx);
		kfree(_b);
	}

	/* free any remaining page array lists */
	list_for_each_safe(pos, q, &mem_list.list) {
		mi = list_entry(pos, struct mem_info, list);
		mutex_lock(&mtx);
		for (i = 0; i < mi->num_pgs; i++)
			dmm_free_page(mi->page[i]);
		mutex_unlock(&mtx);
		dma_free_coherent(NULL, mi->size, mi->dma_va, mi->dma_pa);
		mutex_lock(&mtx);
		list_del(pos);
		mutex_unlock(&mtx);
		kfree(mi);
	}

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
		r = -ENOMEM;
		unregister_chrdev_region(dev, 1);
		printk(KERN_ERR "kmalloc():failed\n");
		goto EXIT;
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

	/* TODO: remove */
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
