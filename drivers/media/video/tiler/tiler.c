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
#include <linux/dma-mapping.h>
#include <linux/pagemap.h>         /* page_cache_release() */

#include <mach/tiler.h>
#include <mach/dmm.h>
#include "../dmm/tmm.h"
#include "tiler_def.h"
#include "tcm/tcm_sita.h"	/* Algo Specific header */

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

struct __buf_info {
	struct list_head list;
	struct tiler_buf_info buf_info;
};

/* per process info */
struct gid_info {
	struct list_head areas;		/* all areas in this pid/gid */
	struct list_head reserved; 	/* areas pre-reserved */
	struct list_head onedim;	/* all 1D areas in this pid/gid */
} gi_main;

struct list_head blocks;
struct list_head bufs;

struct area_info {
	struct list_head area_list;	/* blocks in the same area */
	u32 blocks;			/* blocks in this area */

	struct list_head gid_list;	/* areas in this pid/gid */

	struct tcm_area area;		/* area details */
	struct gid_info *gi;		/* link to parent */
};

struct mem_info {
	struct list_head global;	/* reserved / global blocks */
	u32 sys_addr;          /* system space (L3) tiler addr */
	u32 num_pg;            /* number of pages in page-list */
	u32 usr;               /* user space address */
	u32 *pg_ptr;           /* list of mapped struct page pointers */
	struct tcm_area area;
	u32 *mem;              /* list of alloced phys addresses */
	u32 refs;              /* number of times referenced */

	struct list_head area_list;	/* blocks in the same area */
	struct area_info *ai;		/* area info */
};

#define TILER_FORMATS 4

static s32 tiler_major;
static s32 tiler_minor;
static struct tiler_dev *tiler_device;
static struct class *tilerdev_class;
static u32 id;
static struct mutex mtx;
static struct tcm *tcm[TILER_FORMATS];
static struct tmm *tmm[TILER_FORMATS];

#define TCM(fmt)        tcm[(fmt) - TILFMT_8BIT]
#define TCM_SS(ssptr)   TCM(TILER_GET_ACC_MODE(ssptr))
#define TCM_SET(fmt, i) tcm[(fmt) - TILFMT_8BIT] = i
#define TMM(fmt)        tmm[(fmt) - TILFMT_8BIT]
#define TMM_SS(ssptr)   TMM(TILER_GET_ACC_MODE(ssptr))
#define TMM_SET(fmt, i) tmm[(fmt) - TILFMT_8BIT] = i

/* allocate an reserved area of size, alignment and link it to gi */
static struct area_info *area_new(u16 width, u16 height, u16 align,
				  struct tcm *tcm, struct gid_info *gi)
{
	struct area_info *ai = kmalloc(sizeof(*ai), GFP_KERNEL);
	if (!ai)
		return NULL;

	/* set up empty area info */
	memset(ai, 0x0, sizeof(*ai));
	INIT_LIST_HEAD(&ai->area_list);

	/* reserve an allocation area */
	if (tcm_reserve_2d(tcm, width, height,
			     align <= 1 ? ALIGN_NONE :
			     align <= 32 ? ALIGN_32 : ALIGN_64,
			     &ai->area)) {
		kfree(ai);
		return NULL;
	}

	ai->gi = gi;
	mutex_lock(&mtx);
	list_add_tail(&ai->gid_list, &gi->areas);
	mutex_unlock(&mtx);
	return ai;
}

/* free an area and return NULL */
static inline void area_free(struct area_info *ai)
{
	if (ai) {
		list_del(&ai->gid_list);
		kfree(ai);
	}
}

static s32 __analize_area(enum tiler_fmt fmt, u32 width, u32 height,
			  u16 *x_area, u16 *y_area, u16 *band)
{
	/* x_area and y_area is in slots */
	/* band is set to tiled slots per virtual page */

	u32 x_pagedim = 0, y_pagedim = 0;

	switch (fmt) {
	case TILFMT_8BIT:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_8;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_8;
		*band = PAGE_SIZE / x_pagedim;
		break;
	case TILFMT_16BIT:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_16;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_16;
		*band = PAGE_SIZE / x_pagedim / 2;
		break;
	case TILFMT_32BIT:
		x_pagedim = DMM_PAGE_DIMM_X_MODE_32;
		y_pagedim = DMM_PAGE_DIMM_Y_MODE_32;
		*band = PAGE_SIZE / x_pagedim / 4;
		break;
	case TILFMT_PAGE:
		/* for 1D area keep the height (1), width is in tiler slots */
		*x_area = DIV_ROUND_UP(width, TILER_PAGE);
		*y_area = *band = 1;

		if (*x_area * *y_area > TILER_WIDTH * TILER_HEIGHT)
			return -1;
		return 0;
	default:
		return -1;
	}

	/* adjust and check 2D areas */
	*x_area = DIV_ROUND_UP(width, x_pagedim);
	*y_area = DIV_ROUND_UP(height, y_pagedim);

	if (*x_area > TILER_WIDTH || *y_area > TILER_HEIGHT)
		return -1;
	return 0x0;
}

/**
 * Find a place where a 2D block would fit into a 2D area of the
 * same height.
 *
 * @author a0194118 (3/19/2010)
 *
 * @param w	Width of the block.
 * @param align	Alignment of the block.
 * @param offs	Offset of the block (within alignment)
 * @param ai	Pointer to area info
 * @param next	Pointer to the variable where the next block
 *  		will be stored.  The block should be inserted
 *  		before this block.
 *
 * @return the end coordinate (x1 + 1) where a block would fit,
 *  	   or 0 if it does not fit.
 *
 * mutex must be locked
 */
static u16 blk_find_fit(u16 w, u16 align, u16 offs,
		     struct area_info *ai, struct list_head **before)
{
	int x = ai->area.p0.x + w + offs;
	struct mem_info *mi;

	/* area blocks are sorted by x */
	list_for_each_entry(mi, &ai->area_list, area_list) {
		/* check if buffer would fit before this area */
		if (x <= mi->area.p0.x) {
			*before = &mi->area_list;
			return x;
		}
		x = ALIGN(mi->area.p1.x + 1 - offs, align) + w + offs;
	}
	*before = &ai->area_list;

	/* check if buffer would fit after last area */
	return (x <= ai->area.p1.x + 1) ? x : 0;
}

/* adds a block to an area with certain x coordinates */
/* mutex must be locked */
static inline
struct mem_info *blk_add2area(struct mem_info *mi, struct area_info *ai,
				u16 x0, u16 x1, struct list_head *before)
{
	mi->ai = ai;
	mi->area = ai->area;
	mi->area.p0.x = x0;
	mi->area.p1.x = x1;
	list_add_tail(&mi->area_list, before);
	ai->blocks++;
	return mi;
}

static struct mem_info *get_2d_area(u16 w, u16 h, u16 align, u16 offs, u16 band,
							struct tcm *tcm) {
	struct gid_info *gi = &gi_main;
	struct area_info *ai = NULL;
	struct mem_info *mi = NULL;
	struct list_head *before = NULL;
	u16 x = 0;   /* this holds the end of a potential area */

	/* allocate map info */

	/* see if there is available prereserved space */
	mutex_lock(&mtx);
	list_for_each_entry(mi, &gi->reserved, global) {
		if (mi->area.tcm == tcm &&
		    tcm_aheight(mi->area) == h &&
		    tcm_awidth(mi->area) == w &&
		    (mi->area.p0.x & (align - 1)) == offs) {
			/* this area is already set up */

			/* remove from reserved list */
			list_del(&mi->global);
			goto done;

		}
	}
	mutex_unlock(&mtx);

	/* if not, reserve a block struct */
	mi = kmalloc(sizeof(*mi), GFP_KERNEL);
	if (!mi)
		return mi;
	memset(mi, 0, sizeof(*mi));

	/* see if allocation fits in one of the existing areas */
	/* this sets x, ai and before */
	mutex_lock(&mtx);
	list_for_each_entry(ai, &gi->areas, gid_list) {
		if (ai->area.tcm == tcm &&
		    tcm_aheight(ai->area) == h) {
			x = blk_find_fit(w, align, offs, ai, &before);
			if (x) {
				blk_add2area(mi, ai, x - w, x - 1, before);
				goto done;
			}
		}
	}
	mutex_unlock(&mtx);

	/* if no area fit, reserve a new one */
	ai = area_new(ALIGN(w + offs, max(band, align)), h,
		      max(band, align), tcm, gi);
	if (ai) {
		mutex_lock(&mtx);
		blk_add2area(mi, ai, ai->area.p0.x + offs,
			     ai->area.p0.x + offs + w - 1,
			     &ai->area_list);
	} else {
		/* clean up */
		kfree(mi);
		return NULL;
	}

done:
	mutex_unlock(&mtx);
	return mi;
}

/* mutex must be locked when calling these methods */

/* free block and any freed areas */
static s32 __free(struct mem_info *mi)
{
	struct area_info *ai = NULL;
	struct page *page = NULL;
	s32 res = 0;
	u32 i;

	/* release memory */
	if (mi->pg_ptr) {
		for (i = 0; i < mi->num_pg; i++) {
			page = (struct page *)mi->pg_ptr[i];
			if (page) {
				if (!PageReserved(page))
					SetPageDirty(page);
				page_cache_release(page);
			}
		}
		kfree(mi->pg_ptr);
	} else if (mi->mem) {
		tmm_free(TMM_SS(mi->sys_addr), mi->mem);
	}
	/* safe deletion as list may not have been assigned */
	if (mi->global.next)
		list_del(&mi->global);
	if (mi->area_list.next)
		list_del(&mi->area_list);

	/* remove block from area first if 2D */
	if (mi->area.type == TCM_2D) {
		ai = mi->ai;

		/* check to see if area needs removing also */
		if (ai && !--ai->blocks) {
			res = tcm_free(&ai->area);
			list_del(&ai->gid_list);
			kfree(ai);
			ai = NULL;
		}
	} else {
		/* remove 1D area */
		res = tcm_free(&mi->area);
	}
	kfree(mi);
	return res;
}

/* returns true if block was freed */
static bool __chk_ref(struct mem_info *mi)
{
	/* check references */
	if (mi->refs)
		return 0;

	if (__free(mi))
		printk(KERN_ERR "error while removing tiler block\n");

	return 1;
}

static inline s32 __dec_ref(struct mem_info *mi)
{
	if (mi->refs-- <= 1)
		return __chk_ref(mi);
	return 0;
}

static inline void __inc_ref(struct mem_info *mi)
{
	mi->refs++;
}

#if 0
/**
 * Free all unreferenced blocks
 */
static void free_blocks(struct gid_info *gi)
{
	struct area_info *ai, *ai_;
	struct mem_info *mi, *mi_;

	mutex_lock(&mtx);

	list_for_each_entry_safe(ai, ai_, &gi->areas, area_list) {
		list_for_each_entry_safe(mi, mi_, &ai->area_list, area_list)
			__chk_ref(mi);
	}

	list_for_each_entry_safe(mi, mi_, &gi->onedim, area_list)
		__chk_ref(mi);

	mutex_unlock(&mtx);
}
#endif

static s32 get_area(u32 sys_addr, u32 *x_area, u32 *y_area)
{
	enum tiler_fmt fmt;

	sys_addr &= TILER_ALIAS_VIEW_CLEAR;
	fmt = TILER_GET_ACC_MODE(sys_addr);

	switch (fmt) {
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

static u32 __get_alias_addr(enum tiler_fmt fmt, u16 x, u16 y)
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

static struct mem_info *__get_area(enum tiler_fmt fmt, u32 width, u32 height,
				   u16 align, u16 offset)
{
	u16 x, y, band;
	struct mem_info *mi = NULL;

	/* calculate slot dimensions and band */
	if (__analize_area(fmt, width, height, &x, &y, &band))
		return NULL;

	if (fmt == TILFMT_PAGE)	{
		/* 1D areas don't pack */
		mi = kmalloc(sizeof(*mi), GFP_KERNEL);
		if (!mi)
			return NULL;
		memset(mi, 0x0, sizeof(*mi));

		if (tcm_reserve_1d(TCM(fmt), x * y, &mi->area)) {
			kfree(mi);
			return NULL;
		}

		mutex_lock(&mtx);
		list_add(&mi->area_list, &gi_main.onedim);
	} else {
		mi = get_2d_area(x, y, align ? : band, offset, band, TCM(fmt));
		if (!mi)
			return NULL;

		mutex_lock(&mtx);
	}

	list_add(&mi->global, &blocks);
	mutex_unlock(&mtx);

	mi->sys_addr = __get_alias_addr(fmt, mi->area.p0.x, mi->area.p0.y);
	return mi;
}

static s32 tiler_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct __buf_info *_b = NULL;
	struct tiler_buf_info *b = NULL;
	s32 i = 0, j = 0, k = 0, m = 0, p = 0, bpp = 1;
	struct list_head *pos = NULL;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	mutex_lock(&mtx);
	list_for_each(pos, &bufs) {
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
			p = PAGE_ALIGN(b->blocks[i].dim.area.width * bpp);

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
			p = PAGE_ALIGN(b->blocks[i].dim.len);
			if (remap_pfn_range(vma, vma->vm_start + k,
				(b->blocks[i].ssptr) >> PAGE_SHIFT, p,
				vma->vm_page_prot))
				return -EAGAIN;;
			k += p;
		}
	}
	return 0;
}

static s32 tiler_pat_refill(struct tmm *tmm, struct tcm_area *area, u32 *ptr)
{
	s32 res = 0;
	s32 size = tcm_sizeof(*area) * sizeof(*ptr);
	u32 *page;
	dma_addr_t page_pa;
	struct pat_area p_area = {0};
	struct tcm_area slice, area_s;

	/* must be a 16-byte aligned physical address */
	page = dma_alloc_coherent(NULL, size, &page_pa, GFP_ATOMIC);
	if (!page)
		return -ENOMEM;

	tcm_for_each_slice(slice, *area, area_s) {
		p_area.x0 = slice.p0.x;
		p_area.y0 = slice.p0.y;
		p_area.x1 = slice.p1.x;
		p_area.y1 = slice.p1.y;

		memcpy(page, ptr, sizeof(*ptr) * tcm_sizeof(slice));
		ptr += tcm_sizeof(slice);

		if (tmm_map(tmm, p_area, page_pa)) {
			res = -EFAULT;
			break;
		}
	}

	dma_free_coherent(NULL, size, page, page_pa);

	return res;
}

static s32 map_buffer(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr,
								u32 usr_addr)
{
	u32 i = 0, tmp = -1, *mem = NULL;
	u8 write = 0;
	s32 res = -ENOMEM;
	struct mem_info *mi = NULL;
	struct page *page = NULL;
	struct task_struct *curr_task = current;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;

	/* we only support mapping a user buffer in page mode */
	if (fmt != TILFMT_PAGE)
		return -EPERM;

	/* check if mapping is supported by tmm */
	if (!tmm_can_map(TMM(fmt)))
		return -EPERM;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, 0, 0);
	if (!mi)
		return -ENOMEM;

	*sys_addr = mi->sys_addr;
	mi->usr = usr_addr;

	/* allocate pages */
	mi->num_pg = tcm_sizeof(mi->area);

	mem = kmalloc(mi->num_pg * sizeof(*mem), GFP_KERNEL);
	if (!mem)
		goto done;
	memset(mem, 0x0, sizeof(*mem) * mi->num_pg);

	mi->pg_ptr = kmalloc(mi->num_pg * sizeof(*mi->pg_ptr), GFP_KERNEL);
	if (!mi->pg_ptr)
		goto done;
	memset(mi->pg_ptr, 0x0, sizeof(*mi->pg_ptr) * mi->num_pg);

	/*
	 * Important Note: usr_addr is mapped from user
	 * application process to current process - it must lie
	 * completely within the current virtual memory address
	 * space in order to be of use to us here.
	 */
	down_read(&mm->mmap_sem);
	vma = find_vma(mm, mi->usr);
	res = -EFAULT;

	/*
	 * It is observed that under some circumstances, the user
	 * buffer is spread across several vmas, so loop through
	 * and check if the entire user buffer is covered.
	 */
	while ((vma) && (mi->usr + width > vma->vm_end)) {
		/* jump to the next VMA region */
		vma = find_vma(mm, vma->vm_end + 1);
	}
	if (!vma) {
		printk(KERN_ERR "Failed to get the vma region for "
			"user buffer.\n");
		goto fault;
	}

	if (vma->vm_flags & (VM_WRITE | VM_MAYWRITE))
		write = 1;

	tmp = mi->usr;
	for (i = 0; i < mi->num_pg; i++) {
		if (get_user_pages(curr_task, mm, tmp, 1, write, 1, &page,
									NULL)) {
			if (page_count(page) < 1) {
				printk(KERN_ERR "Bad page count from"
							"get_user_pages()\n");
			}
			mi->pg_ptr[i] = (u32)page;
			mem[i] = page_to_phys(page);
			tmp += PAGE_SIZE;
		} else {
			printk(KERN_ERR "get_user_pages() failed\n");
			goto fault;
		}
	}
	up_read(&mm->mmap_sem);

	if (tiler_pat_refill(TMM(fmt), &mi->area, mem))
		goto fault;

	res = 0;
	goto done;
fault:
	up_read(&mm->mmap_sem);
done:
	if (res) {
		mutex_lock(&mtx);
		__free(mi);
		mutex_unlock(&mtx);
	}
	kfree(mem);
	return res;
}

s32 tiler_free(u32 sys_addr)
{
	struct mem_info *mi = NULL, *q = NULL;

	mutex_lock(&mtx);
	/* find block in global list and free it */
	list_for_each_entry_safe(mi, q, &blocks, global) {
		if (mi->sys_addr == sys_addr)
			__chk_ref(mi);
	}
	mutex_unlock(&mtx);

	/* for debugging, we can set the PAT entries to DMM_LISA_MAP__0 */
	return 0x0;
}
EXPORT_SYMBOL(tiler_free);

/* :TODO: Currently we do not track enough information from alloc to get back
   the actual width and height of the container, so we must make a guess.  We
   do not even have enough information to get the virtual stride of the buffer,
   which is the real reason for this ioctl */
s32 tiler_find_buf(u32 sys_addr, struct tiler_block_info *blk)
{
	struct tcm_area area;
	struct tcm_pt pt;
	u32 x_area = -1, y_area = -1;
	u16 x_page = 0, y_page = 0;
	s32 mode = -1;

	if (get_area(sys_addr & 0x0FFFFF000, &x_area, &y_area))
		return -EFAULT;

	memset(&area, 0, sizeof(area));
	pt.x = x_area;
	pt.y = y_area;
	if (tcm_get_parent(TCM_SS(sys_addr), &pt, &area))
		return -EFAULT;
	x_page = area.p1.x - area.p0.x + 1;
	y_page = area.p1.y - area.p0.y + 1;
	blk->ptr = NULL;

	mode = TILER_GET_ACC_MODE(sys_addr);
	blk->fmt = (mode);
	if (blk->fmt == TILFMT_PAGE) {
		blk->dim.len = x_page * y_page * TILER_PAGE;
		if (blk->dim.len == 0)
			goto error;
		blk->stride = 0;
		blk->ssptr = (u32)TIL_ALIAS_ADDR(((area.p0.x |
						(area.p0.y << 8)) << 12), mode);
	} else {
		blk->stride = blk->dim.area.width = x_page * TILER_BLOCK_WIDTH;
		blk->dim.area.height = y_page * TILER_BLOCK_HEIGHT;
		if (blk->dim.area.width == 0 || blk->dim.area.height == 0)
			goto error;
		if (blk->fmt == TILFMT_8BIT) {

			blk->ssptr = (u32)TIL_ALIAS_ADDR(((area.p0.x << 6) |
						(area.p0.y << 20)), mode);
		} else {
			blk->ssptr = (u32)TIL_ALIAS_ADDR(((area.p0.x << 7) |
						(area.p0.y << 20)), mode);

			blk->stride <<= 1;
			blk->dim.area.height >>= 1;
			if (blk->fmt == TILFMT_32BIT)
				blk->dim.area.width >>= 1;
		}
		blk->stride = PAGE_ALIGN(blk->stride);
	}
	return 0;

error:
	blk->fmt = TILFMT_INVALID;
	blk->dim.len = blk->stride = blk->ssptr = 0;
	return -EFAULT;
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
	struct tiler_buf_info buf_info = {0};
	struct tiler_block_info block_info = {0};
	struct list_head *pos = NULL, *q = NULL;

	switch (cmd) {
	case TILIOC_GBUF:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(block_info)))
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
					sizeof(block_info)))
			return -EFAULT;
		break;
	case TILIOC_FBUF:
	case TILIOC_UMBUF:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(block_info)))
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
					sizeof(block_info)))
			return -EFAULT;

		if (!block_info.ptr)
			return -EFAULT;

		if (map_buffer(block_info.fmt, block_info.dim.len, 1,
				&block_info.ssptr, (u32)block_info.ptr))
			return -ENOMEM;

		if (copy_to_user((void __user *)arg, &block_info,
					sizeof(block_info)))
			return -EFAULT;
		break;
	case TILIOC_QBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(buf_info)))
			return -EFAULT;

		mutex_lock(&mtx);
		list_for_each(pos, &bufs) {
			_b = list_entry(pos, struct __buf_info, list);
			if (buf_info.offset == _b->buf_info.offset) {
				if (copy_to_user((void __user *)arg,
					&_b->buf_info,
					sizeof(_b->buf_info))) {
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
		_b = kmalloc(sizeof(*_b), GFP_KERNEL);
		if (!_b)
			return -ENOMEM;
		memset(_b, 0x0, sizeof(*_b));

		if (copy_from_user(&_b->buf_info, (void __user *)arg,
					sizeof(_b->buf_info))) {
			kfree(_b); return -EFAULT;
		}

		_b->buf_info.offset = id;
		id += 0x1000;

		mutex_lock(&mtx);
		list_add(&(_b->list), &bufs);
		mutex_unlock(&mtx);

		if (copy_to_user((void __user *)arg, &_b->buf_info,
					sizeof(_b->buf_info))) {
			kfree(_b); return -EFAULT;
		}
		break;
	case TILIOC_URBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(buf_info)))
			return -EFAULT;

		mutex_lock(&mtx);
		list_for_each_safe(pos, q, &bufs) {
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
					sizeof(block_info)))
			return -EFAULT;

		if (tiler_find_buf(block_info.ssptr, &block_info))
			return -EFAULT;

		if (copy_to_user((void __user *)arg, &block_info,
			sizeof(block_info)))
			return -EFAULT;
		break;
	default:
		return -EINVAL;
	}
	return 0x0;
}

s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr)
{
	struct mem_info *mi = NULL;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, 0, 0);
	if (!mi)
		return -ENOMEM;

	*sys_addr = mi->sys_addr;

	/* allocate and map if mapping is supported */
	if (tmm_can_map(TMM(fmt))) {
		mi->num_pg = tcm_sizeof(mi->area);

		mi->mem = tmm_get(TMM(fmt), mi->num_pg);
		if (!mi->mem)
			goto cleanup;

		/* program PAT */
		if (tiler_pat_refill(TMM(fmt), &mi->area, mi->mem))
			goto cleanup;
	}

	mutex_lock(&mtx);
	mutex_unlock(&mtx);
	return 0;

cleanup:
	mutex_lock(&mtx);
	__free(mi);
	mutex_unlock(&mtx);
	return -ENOMEM;
}
EXPORT_SYMBOL(tiler_alloc);

static void __exit tiler_exit(void)
{
	struct __buf_info *_b = NULL, *_b_ = NULL;
	struct mem_info *mi = NULL, *mi_ = NULL;
	int i, j;

	mutex_lock(&mtx);

	/* remove any buf info structs */
	list_for_each_entry_safe(_b, _b_, &bufs, list) {
		list_del(&_b->list);
		kfree(_b);
	}

	/* free any remaining mem structures */
	list_for_each_entry_safe(mi, mi_, &blocks, global) {
		mi->refs = 0;
		__free(mi);
	}
	list_for_each_entry_safe(mi, mi_, &gi_main.reserved, global) {
		mi->refs = 0;
		__free(mi);
	}
	mutex_unlock(&mtx);

	/* close containers only once */
	for (i = TILFMT_8BIT; i <= TILFMT_MAX; i++) {
		/* remove identical containers (tmm is unique per tcm) */
		for (j = i + 1; j <= TILFMT_MAX; j++)
			if (tcm[i] == tcm[j]) {
				tcm[j] = NULL;
				tmm[j] = NULL;
			}

		tcm_deinit(TCM(i));
		tmm_deinit(TMM(i));
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
	struct tcm_pt div_pt;
	struct tcm *sita = NULL;
	struct tmm *tmm_pat = NULL;

	/* Allocate tiler container manager (we share 1 on OMAP4) */
	div_pt.x = 192;   /* hardcoded default */
	div_pt.y = 96;
	sita = sita_init(256, 128, (void *)&div_pt);

	TCM_SET(TILFMT_8BIT, sita);
	TCM_SET(TILFMT_16BIT, sita);
	TCM_SET(TILFMT_32BIT, sita);
	TCM_SET(TILFMT_PAGE, sita);

	/* Allocate tiler memory manager (must have 1 unique TMM per TCM ) */
	tmm_pat = tmm_pat_init(0);
	TMM_SET(TILFMT_8BIT, tmm_pat);
	TMM_SET(TILFMT_16BIT, tmm_pat);
	TMM_SET(TILFMT_32BIT, tmm_pat);
	TMM_SET(TILFMT_PAGE, tmm_pat);

	tiler_device = kmalloc(sizeof(*tiler_device), GFP_KERNEL);
	if (!tiler_device || !sita || !tmm_pat) {
		r = -ENOMEM;
		goto error;
	}

	memset(tiler_device, 0x0, sizeof(*tiler_device));
	if (tiler_major) {
		dev = MKDEV(tiler_major, tiler_minor);
		r = register_chrdev_region(dev, 1, "tiler");
	} else {
		r = alloc_chrdev_region(&dev, tiler_minor, 1, "tiler");
		tiler_major = MAJOR(dev);
	}

	cdev_init(&tiler_device->cdev, &tiler_fops);
	tiler_device->cdev.owner = THIS_MODULE;
	tiler_device->cdev.ops   = &tiler_fops;

	r = cdev_add(&tiler_device->cdev, dev, 1);
	if (r)
		printk(KERN_ERR "cdev_add():failed\n");

	tilerdev_class = class_create(THIS_MODULE, "tiler");

	if (IS_ERR(tilerdev_class)) {
		printk(KERN_ERR "class_create():failed\n");
		goto error;
	}

	device = device_create(tilerdev_class, NULL, dev, NULL, "tiler");
	if (device == NULL)
		printk(KERN_ERR "device_create() fail\n");

	r = platform_driver_register(&tiler_driver_ldm);

	mutex_init(&mtx);
	INIT_LIST_HEAD(&bufs);
	INIT_LIST_HEAD(&blocks);
	INIT_LIST_HEAD(&gi_main.onedim);
	INIT_LIST_HEAD(&gi_main.areas);
	INIT_LIST_HEAD(&gi_main.reserved);
	id = 0xda7a000;

error:
	/* TODO: error handling for device registration */
	if (r) {
		kfree(tiler_device);
		tcm_deinit(sita);
		tmm_deinit(tmm_pat);
	}

	return r;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("davidsin@ti.com");
module_init(tiler_init);
module_exit(tiler_exit);
