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
#include <linux/cdev.h>    /* struct cdev */
#include <linux/kdev_t.h>  /* MKDEV() */
#include <linux/fs.h>      /* register_chrdev_region() */
#include <linux/device.h>  /* struct class */
#include <linux/platform_device.h> /* platform_device() */
#include <linux/err.h>     /* IS_ERR() */
#include <linux/uaccess.h> /* copy_to_user */
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#include "../dmm/dmm.h"
#include "tiler.h"
#include "dmm_2d_alloc.h"
#include "../dmm/dmm_page_rep.h"

/* TODO: remove */
#include "tiler_deprecate.h"
struct node *lsthd;
static u32 id;
static struct dmmTILERContCtxT *tilctx;

#if 0
#define DEBUG(x) printk(KERN_ERR "%s()::%d:%s=(0x%08x)\n", \
				__func__, __LINE__, #x, (s32)x);
#else
#define DEBUG(x)
#endif

static s32 tiler_open(struct inode *i, struct file *f);
static s32 tiler_release(struct inode *i, struct file *f);
static s32 tiler_ioctl(struct inode *i, struct file *f, u32 c, unsigned long a);
static s32 tiler_mmap(struct file *f, struct vm_area_struct *v);

static s32 tiler_major;
static s32 tiler_minor;

struct tiler_dev {
	struct cdev cdev;
};

static struct tiler_dev *tiler_device;
static struct class *tilerdev_class;
static const struct file_operations tiler_fops = {
	.open    = tiler_open,
	.ioctl   = tiler_ioctl,
	.release = tiler_release,
	.mmap    = tiler_mmap,
};

static struct platform_driver tiler_driver_ldm = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "tiler",
	},
	.probe = NULL,
	.shutdown = NULL,
	.remove = NULL,
};

static struct map {
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

static struct buf_info {
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

static struct map map_list;

static s32 __init tiler_init(void)
{
	dev_t dev  = 0;
	s32 r = -1;
	struct device *device = NULL;

	DEBUG(0);

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
	createlist(&lsthd);
	tilctx = kmalloc(sizeof(struct dmmTILERContCtxT), GFP_KERNEL);
	if (!tilctx)
		return -ENOMEM;
	memset(tilctx, 0x0, sizeof(struct dmmTILERContCtxT));
	tilctx->contSizeX = TILER_WIDTH;
	tilctx->contSizeY = TILER_HEIGHT;
	mutex_init(&tilctx->mtx);
	if(dmm_phys_page_rep_init())
		return -ENOMEM;

	id = 0xda7a000;

EXIT:
	return r;
}

static s32 tiler_mmap(struct file *filp, struct vm_area_struct *vma)
{
#if 0
	struct tiler_buf_info *b = NULL;
	s32 i = 0, j = 0, k = 0, m = 0, p = 0;
	s32 bpp = 1;

	DEBUG(0); DEBUG(0); DEBUG(0);

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (tiler_get_buf_info(lsthd, &b, vma->vm_pgoff << PAGE_SHIFT))
		return -EFAULT;

	for (i = 0; i < b->num_blocks; i++) {
		if (b->blocks[i].fmt >= TILFMT_8BIT && b->blocks[i].fmt <= TILFMT_32BIT) {
			/* get line width */
			bpp = (b->blocks[i].fmt == TILFMT_8BIT ? 1 : b->blocks[i].fmt == TILFMT_16BIT ? 2 : 4);
			p = (b->blocks[i].dim.area.width * bpp + TILER_PAGE - 1) & ~(TILER_PAGE - 1);

			for (j = 0; j < b->blocks[i].dim.area.height; j++) {
				vma->vm_pgoff = (b->blocks[i].ssptr + m) >> PAGE_SHIFT;
				if (remap_pfn_range(vma, vma->vm_start + k, (b->blocks[i].ssptr + m) >> PAGE_SHIFT, p, vma->vm_page_prot))
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
			p = (b->blocks[i].dim.len + TILER_PAGE - 1) & ~(TILER_PAGE - 1);
			if (remap_pfn_range(vma, vma->vm_start + k, (b->blocks[i].ssptr) >> PAGE_SHIFT, p, vma->vm_page_prot))
				return -EAGAIN;;
			k += p;
		}
	}
	DEBUG("MAPOK");
	return 0;
#else
	int ret = -1;
	struct tiler_buf_info *b = NULL;
	int i = 0, j = 0, k = 0, m = 0, p = 0;
	int bpp = 1;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	ret = tiler_get_buf_info(lsthd, &b, vma->vm_pgoff << PAGE_SHIFT);
	if (ret != 0) {
		printk(KERN_ERR "%s::%s():%d: tiler_get_buf_info failed\n",
						__FILE__, __func__, __LINE__);
		return 0x0;
	}

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
				if (0)
					printk(KERN_NOTICE
					"%s::%s():%d: vm_start+%d = 0x%lx,"
					"blk[%d].ssptr+%d = 0x%lx, w=0x%x\n",
					__FILE__, __func__, __LINE__,
					k, vma->vm_start + k, i, m,
					(b->blocks[i].ssptr + m), p);
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
			if (0)
				printk(KERN_NOTICE "%s::%s():%d:"
				"vm_start = 0x%lx, blk[%d].ssptr = 0x%lx,"
				"w=0x%x\n",
				__FILE__, __func__, __LINE__,
				vma->vm_start + k, i, (b->blocks[i].ssptr), p);
			if (remap_pfn_range(vma, vma->vm_start + k,
				(b->blocks[i].ssptr) >> PAGE_SHIFT, p,
				vma->vm_page_prot))
				return -EAGAIN;;
			k += p;
		}
	}
	return 0;
#endif
}

static s32 tiler_ioctl(struct inode *ip, struct file *filp, u32 cmd,
			unsigned long arg)
{
	struct tiler_buf_info *bufinfo = NULL;
	pgd_t *pgd = NULL;
	pmd_t *pmd = NULL;
	pte_t *ptep = NULL, pte = 0x0;
	s32 offset = 0x0, r = -1;
	u32 til_addr = 0x0;

	struct tiler_buf_info buf_info = {0};
	struct tiler_block_info block_info = {0};

	/* TODO: remove */
	s32 bytes, error;

	switch (cmd) {
	case TILIOC_GBUF:
		DEBUG(__LINE__);
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

		DEBUG(til_addr);
		block_info.ssptr = til_addr;
		if (copy_to_user((void __user *)arg, &block_info,
					sizeof(struct tiler_block_info)))
			return -EFAULT;
		break;
	case TILIOC_FBUF:
		DEBUG(__LINE__);
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		if (tiler_free(block_info.ssptr))
			{DEBUG(__LINE__); return -EFAULT;}
		break;
	case TILIOC_GSSP:
		DEBUG(__LINE__);
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
		break;
	case TILIOC_MBUF:
		DEBUG(__LINE__);
		break;
	case TILIOC_QBUF:
#if 0
		DEBUG(__LINE__);
		if (copy_from_user(&buf_info, (void __user *)arg,
						sizeof(struct tiler_buf_info)))
			return -EFAULT;

		offset = buf_info.offset;
		if (tiler_get_buf_info(lsthd, &bufinfo, offset))
			return -EFAULT;

		if (copy_to_user((void __user *)arg, bufinfo,
						sizeof(struct tiler_buf_info)));
			return -EFAULT;
#else
		bytes = copy_from_user((void *)(&buf_info),
			(const void *)arg, sizeof(struct tiler_buf_info));
		if (bytes != 0)
			return r;

		offset = buf_info.offset;
		error = tiler_get_buf_info(lsthd, &bufinfo, offset);
		if (error != 0)
			return r;

		bytes = copy_to_user((void *)arg,
			(const void *)bufinfo, sizeof(struct tiler_buf_info));
		if (bytes != 0)
			return r;

		r = 0;
#endif
		break;
	case TILIOC_RBUF:
#if 0
		DEBUG(__LINE__);
		bufinfo = kmalloc(sizeof(struct tiler_buf_info), GFP_KERNEL);
		if (!bufinfo)
			{DEBUG(__LINE__); return -ENOMEM;}
		memset(bufinfo, 0x0, sizeof(struct tiler_buf_info));
		if (copy_from_user(bufinfo, (void __user *)arg,
						sizeof(struct tiler_buf_info)))
			{DEBUG(__LINE__); return -EFAULT;}

		bufinfo->offset = id;
		id += 0x1000;

		if (copy_to_user((void __user *)arg, bufinfo,
						sizeof(struct tiler_buf_info)))
			{DEBUG(__LINE__); return -EFAULT;}

		/* replace with kernel linked list */
		addnode(lsthd, bufinfo);
#else
		bufinfo = kmalloc(sizeof(struct tiler_buf_info), GFP_KERNEL);
		if (!bufinfo)
			return r;
		memset(bufinfo, 0x0, sizeof(struct tiler_buf_info));
		bytes = copy_from_user((void *)bufinfo,
			(const void *)arg, sizeof(struct tiler_buf_info));
		if (bytes != 0)
			return r;

		bufinfo->offset = id;
		id += 0x1000;

		bytes = copy_to_user((void *)arg,
			(const void *)bufinfo, sizeof(struct tiler_buf_info));
		if (bytes != 0)
			return r;

		error = addnode(lsthd, bufinfo);
		if (error != 0)
			return r;
		r = 0;
#endif
		break;
	case TILIOC_URBUF:
#if 0
		DEBUG(__LINE__);
		if (copy_from_user(&buf_info, (void __user *)arg,
						sizeof(struct tiler_buf_info)))
			return -EFAULT;

		offset = buf_info.offset;
		if (removenode(lsthd, offset))
			return -EFAULT;
#else
		bytes = copy_from_user((void *)(&buf_info),
			(const void *)arg, sizeof(struct tiler_buf_info));
		if (bytes != 0)
			return r;

		offset = buf_info.offset;
		error = removenode(lsthd, offset);
		if (error != 0)
			return r;

		r = 0;
#endif
		break;
	case TILIOC_QUERY_BLK:
#if 0
		DEBUG(__LINE__);
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(struct tiler_block_info)))
			return -EFAULT;

		/* TODO: only for d2c, so rework this later */
		/*if (tiler_find_buf(block_info.ssptr, &block_info))
			return -EFAULT;*/

		if (copy_to_user((void __user *)arg, &block_info,
			sizeof(struct tiler_block_info)))
			return -EFAULT;
#else
		bytes = copy_from_user((void *)(&block_info),
			(const void *)arg, sizeof(struct tiler_block_info));
		if (bytes != 0)
			return r;

		DEBUG("ERRRRRRRRRRRRRRROOOOOOOOOOOOOR!\n");
		//error = tiler_find_buf(block_info.ssptr, &block_info);
		if (error != 0)
			return r;

		bytes = copy_to_user((void *)arg,
			(const void *)(&block_info),
			sizeof(struct tiler_block_info));
		if (bytes != 0)
			return r;

		r = 0;
#endif
		break;
	//default:
	//	return -EINVAL;
	}
	return 0x0;
}

static void __exit tiler_exit(void)
{
	DEBUG(0);
	/* TODO: remove */
	if (dmm_phys_page_rep_deinit())
		printk(KERN_ERR "dmm_phys_page_rep_deinit() -- error\n");
	mutex_destroy(&tilctx->mtx);
	kfree(tilctx);

	platform_driver_unregister(&tiler_driver_ldm);
	cdev_del(&tiler_device->cdev);
	kfree(tiler_device);
	device_destroy(tilerdev_class, MKDEV(tiler_major, tiler_minor));
	class_destroy(tilerdev_class);
}

static s32 tiler_open(struct inode *ip, struct file *filp)
{
	DEBUG(0);
	return 0x0;
}

static s32 tiler_release(struct inode *ip, struct file *filp)
{
	DEBUG(0);
	return 0x0;
}

static s32 set_area(enum tiler_fmt fmt, u32 width, u32 height,
				u32 *x_area, u32 *y_area)
{
	s32 x_pagedim = 0, y_pagedim = 0;
	u16 tiled_pages_per_ss_page = 0;

	DEBUG(0);
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

	if (*x_area > TILER_WIDTH || *y_area > TILER_HEIGHT) {
		return -1;
	}
	return 0x0;
}

static s32 get_area(u32 sys_addr, u32 *x_area, u32 *y_area)
{
	enum tiler_fmt fmt;

	/* check sys_addr here! */

	DEBUG(0);
	sys_addr &= TILER_ALIAS_VIEW_CLEAR;
	fmt = TILER_GET_ACC_MODE(sys_addr);

	DEBUG(fmt);
	switch(fmt+1) {
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

	DEBUG(0);
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
		return NULL;
		break;
	}

	if (fmt == TILFMT_PAGE)
		return COMPOSE_ALIAS_PTR((x | y << y_shft) << 12, acc_mode);
	else
		return COMPOSE_ALIAS_PTR(x << x_shft | y << y_shft, acc_mode);
}

s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr)
{
	s32 r = -1;
	u16 x_area = -1, y_area = -1;
	u32 num_pages = -1, i = -1;

	struct dmmTILERContPageAreaT *area_desc = NULL, __area_desc = {0};
	struct PATDescrT pat_desc = {0};

	DEBUG(0);
	/* driver shouldn't really be doing this */
	r = set_area(fmt, width, height, &x_area, &y_area);
	/* check r */
	DEBUG(fmt); DEBUG(width); DEBUG(height); DEBUG(x_area); DEBUG(y_area);

	__area_desc.x1 = (u16)x_area;
	__area_desc.y1 = (u16)y_area;

	DEBUG(__area_desc.x0);
	DEBUG(__area_desc.y0);
	DEBUG(__area_desc.x1);
	DEBUG(__area_desc.y1);
	DEBUG(__area_desc.xPageOfst);
	DEBUG(__area_desc.yPageOfst);
	DEBUG(__area_desc.xPageCount);
	DEBUG(__area_desc.yPageCount);
	DEBUG(__area_desc.fitToSide);

	area_desc = alloc_2d_area(tilctx, &__area_desc);
	if (!area_desc)
		return -ENOMEM;

	DEBUG(area_desc->x0);
	DEBUG(area_desc->y0);
	DEBUG(area_desc->x1);
	DEBUG(area_desc->y1);
	DEBUG(area_desc->xPageOfst);
	DEBUG(area_desc->yPageOfst);
	DEBUG(area_desc->xPageCount);
	DEBUG(area_desc->yPageCount);
	DEBUG(area_desc->fitToSide);

	*sys_addr = get_alias_addr(fmt, area_desc->x0, area_desc->y0);
	DEBUG(*sys_addr);

	/* calculate the number of pages needed */
	area_desc->xPageCount = area_desc->x1 - area_desc->x0 + 1;
	area_desc->yPageCount = area_desc->y1 - area_desc->y0 + 1;
	num_pages = area_desc->xPageCount * area_desc->yPageCount;
	DEBUG(num_pages);

	/* determine phys page array size -- move to pat_map struct */
	area_desc->dma_size =  num_pages * 4 + 16;

	/* move this into pat map structure */
	area_desc->dma_va = dma_alloc_coherent(NULL, area_desc->dma_size,
					&(area_desc->dma_pa), GFP_ATOMIC);
	if (!area_desc->dma_va)
		return -ENOMEM;
	memset(area_desc->dma_va, 0x0, area_desc->dma_size);

	/* may not be req'd since dma_alloc_coherent allocs on page boundry */
	area_desc->patPageEntries = (u32 *)((u32)area_desc->dma_va + 15 & ~15);

	for (i = 0; i < num_pages; i++) {
		area_desc->patPageEntries[i] = (u32)dmm_get_phys_page();
		if (area_desc->patPageEntries[i] == 0x0)
			goto cleanup;
	}

	pat_desc.area.x0 = area_desc->x0 + area_desc->xPageOfst;
	pat_desc.area.y0 = area_desc->y0 + area_desc->yPageOfst;
	pat_desc.area.x1 = area_desc->x0 + area_desc->xPageOfst +
			   area_desc->xPageCount - 1;
	pat_desc.area.y1 = area_desc->y0 + area_desc->yPageOfst +
			   area_desc->yPageCount - 1;

	DEBUG(pat_desc.area.x0);
	DEBUG(pat_desc.area.y0);
	DEBUG(pat_desc.area.x1);
	DEBUG(pat_desc.area.y1);

	pat_desc.ctrl.direction = 0;
	pat_desc.ctrl.initiator = 0;
	pat_desc.ctrl.lutID = 0;
	pat_desc.ctrl.start = 1;
	pat_desc.ctrl.sync = 0;

	pat_desc.nextPatEntry = NULL;
	pat_desc.data = (u32)area_desc->dma_pa;

	/* TODO: store the pat map struct in a linked list */

	if (dmm_pat_refill(&pat_desc, 0, MANUAL, 0))
		goto cleanup;

	return 0x0;
cleanup:
	printk(KERN_ERR "TILER error.\n");
	dma_free_coherent(NULL, area_desc->dma_size, area_desc->dma_va,
				area_desc->dma_pa);
	return -ENOMEM;
}
EXPORT_SYMBOL(tiler_alloc);

s32 tiler_free(u32 sys_addr)
{
	struct dmmTILERContPageAreaT *area_desc =  NULL;
	u32 x_area = -1, y_area = -1;

	if (get_area(sys_addr, &x_area, &y_area))
		{DEBUG(__LINE__); return -EFAULT;}

	area_desc = search_2d_area(tilctx, x_area, y_area, 0, 0);
	if (!area_desc)
		{DEBUG(__LINE__); return -EFAULT;}

	DEBUG(area_desc->x0);
	DEBUG(area_desc->y0);
	DEBUG(area_desc->x1);
	DEBUG(area_desc->y1);
	DEBUG(area_desc->xPageOfst);
	DEBUG(area_desc->yPageOfst);
	DEBUG(area_desc->xPageCount);
	DEBUG(area_desc->yPageCount);
	DEBUG(area_desc->fitToSide);

	/* freeing of physical pages should *not* happen in this next API */
	if (!(dealloc_2d_area(tilctx, area_desc)))
		{DEBUG(__LINE__); return -EFAULT;}

	/* should there be a call to remove the PAT entries */

	return 0x0;
}
EXPORT_SYMBOL(tiler_free);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("davidsin@ti.com");
module_init(tiler_init);
module_exit(tiler_exit);
