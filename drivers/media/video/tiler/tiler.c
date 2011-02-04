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
#include <linux/slab.h>

#include <mach/tiler.h>
#include <mach/dmm.h>
#include "../dmm/tmm.h"
#include "tiler_def.h"
#include "tcm/tcm_sita.h"	/* Algo Specific header */

#include <linux/syscalls.h>

struct tiler_dev {
	struct cdev cdev;

	struct blocking_notifier_head notifier;
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

/* per process (thread group) info */
struct process_info {
	struct list_head list;		/* other processes */
	struct list_head groups;	/* my groups */
	struct list_head bufs;		/* my registered buffers */
	pid_t pid;			/* really: thread group ID */
	u32 refs;			/* open tiler devices, 0 for processes
					   tracked via kernel APIs */
	bool kernel;			/* tracking kernel objects */
};

/* per group info (within a process) */
struct gid_info {
	struct list_head by_pid;	/* other groups */
	struct list_head areas;		/* all areas in this pid/gid */
	struct list_head reserved;	/* areas pre-reserved */
	struct list_head onedim;	/* all 1D areas in this pid/gid */
	u32 gid;			/* group ID */
	struct process_info *pi;	/* parent */
};

static struct list_head blocks;
static struct list_head procs;
static struct list_head orphan_areas;
static struct list_head orphan_onedim;

struct area_info {
	struct list_head by_gid;	/* areas in this pid/gid */
	struct list_head blocks;	/* blocks in this area */
	u32 nblocks;			/* # of blocks in this area */

	struct tcm_area area;		/* area details */
	struct gid_info *gi;		/* link to parent, if still alive */
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
	bool alloced;			/* still alloced */

	struct list_head by_area;	/* blocks in the same area / 1D */
	void *parent;			/* area info for 2D, else group info */
};

struct __buf_info {
	struct list_head by_pid;		/* list of buffers per pid */
	struct tiler_buf_info buf_info;
	struct mem_info *mi[TILER_MAX_NUM_BLOCKS];	/* blocks */
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
static u32 *dmac_va;
static dma_addr_t dmac_pa;

#define TCM(fmt)        tcm[(fmt) - TILFMT_8BIT]
#define TCM_SS(ssptr)   TCM(TILER_GET_ACC_MODE(ssptr))
#define TCM_SET(fmt, i) tcm[(fmt) - TILFMT_8BIT] = i
#define TMM(fmt)        tmm[(fmt) - TILFMT_8BIT]
#define TMM_SS(ssptr)   TMM(TILER_GET_ACC_MODE(ssptr))
#define TMM_SET(fmt, i) tmm[(fmt) - TILFMT_8BIT] = i

static void fill_map(u16 **map, int div, struct tcm_area *a, u8 c, bool ovw,
			u8 col)
{
	u16 val = c | ((u16) col << 8);
	int x, y;
	for (y = a->p0.y; y <= a->p1.y; y++)
		for (x = a->p0.x / div; x <= a->p1.x / div; x++)
			if (map[y][x] == ' ' || ovw)
				map[y][x] = val;
}

static void fill_map_pt(u16 **map, int div, struct tcm_pt *p, u8 c)
{
	map[p->y][p->x / div] = (map[p->y][p->x / div] & 0xff00) | c;
}

static u8 read_map_pt(u16 **map, int div, struct tcm_pt *p)
{
	return map[p->y][p->x / div] & 0xff;
}

static int map_width(int div, int x0, int x1)
{
	return (x1 / div) - (x0 / div) + 1;
}

static void text_map(u16 **map, int div, char *nice, int y, int x0, int x1,
								u8 col)
{
	u16 *p = map[y] + (x0 / div);
	int w = (map_width(div, x0, x1) - strlen(nice)) / 2;
	if (w >= 0) {
		p += w;
		while (*nice)
			*p++ = ((u16) col << 8) | (u8) *nice++;
	}
}

static void map_1d_info(u16 **map, int div, char *nice, struct tcm_area *a,
						u8 col)
{
	sprintf(nice, "%dK", tcm_sizeof(*a) * 4);
	if (a->p0.y + 1 < a->p1.y) {
		text_map(map, div, nice, (a->p0.y + a->p1.y) / 2, 0,
							TILER_WIDTH - 1, col);
	} else if (a->p0.y < a->p1.y) {
		if (strlen(nice) < map_width(div, a->p0.x, TILER_WIDTH - 1))
			text_map(map, div, nice, a->p0.y, a->p0.x + div,
							TILER_WIDTH - 1, col);
		else if (strlen(nice) < map_width(div, 0, a->p1.x))
			text_map(map, div, nice, a->p1.y, 0, a->p1.y - div,
									col);
	} else if (strlen(nice) + 1 < map_width(div, a->p0.x, a->p1.x)) {
		text_map(map, div, nice, a->p0.y, a->p0.x, a->p1.x, col);
	}
}

static void map_2d_info(u16 **map, int div, char *nice, struct mem_info *mi,
							u8 col)
{
	struct tcm_area *a = &mi->area;
	int y = (a->p0.y + a->p1.y) / 2;
	sprintf(nice, "(%d*%d)", tcm_awidth(*a), tcm_aheight(*a));
	if (strlen(nice) + 1 < map_width(div, a->p0.x, a->p1.x))
		text_map(map, div, nice, y, a->p0.x, a->p1.x, col);

	sprintf(nice, "<%s%d>", mi->alloced ? "a" : "", mi->refs);
	if (a->p1.y > a->p0.y + 1 &&
	    strlen(nice) + 1 < map_width(div, a->p0.x, a->p1.x))
		text_map(map, div, nice, y + 1, a->p0.x, a->p1.x, col);
}

static void write_out(u16 **map, char *fmt, int y, bool color, char *nice)
{
	u8 current_col = '\x0f';
	char *o = nice;
	u16 *d = map[y];

	/* boundary */
	if (color)
		o += sprintf(o, "\e[0;%d%sm", (y & 8) ? 34 : 36,
						(y & 16) ? ";1" : "");
	o += sprintf(o, fmt, y);
	o += sprintf(o, "%s:", color ? "\e[0;1m" : "");

	/* text */
	do {
		u16 p = *d ?: (':' | 0x0f00);
		u8 col_chg = current_col ^ (p >> 8);
		if (col_chg && color) {
			o += sprintf(o, "\e[");
			if ((col_chg & 0x88) && (p & 0x0800) == 0) {
				o += sprintf(o, "0;");
				col_chg = 0x07 ^ (p >> 8);
			}
			if (col_chg & 0x7)
				o += sprintf(o, "%d;", 30 + ((p >> 8) & 0x07));
			if (col_chg & 0x70)
				o += sprintf(o, "%d;", 40 + ((p >> 12) & 0x07));
			if (p & 0x0800)
				o += sprintf(o, "1;");
			o[-1] = 'm';
		}
		*o++ = p & 0xff;
		current_col = p >> 8;
	} while (*d++);

	if (color && current_col != 0x07)
		o += sprintf(o, "\e[0m");
	*o = 0;
	printk(KERN_ERR "%s\n", nice);
}

static void print_allocation_map(bool color)
{
	int div = 2;
	int i, j;
	u16 **map, *global_map;
	struct area_info *ai;
	struct mem_info *mi;
	struct tcm_area a, p;
	static u8 *m2d = "abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static u8 *a2d = ".,:;'\"`~!^-+";
	static u8 *m1d_c = "\x47\x57\x67\xc7\xd7\xe7";
	static u8 *m2d_c = "\x17\x27\x37\x97\xa7\xb7";
	static u8 *a2d_c = "\x01\x02\x03\x09\x0a\x0b";

	u8 *m2dp = m2d, *a2dp = a2d;
	u8 *m2dp_c = m2d_c, *m1dp_c = m1d_c, *a2dp_c = a2d_c;
	char *nice;

	/* allocate map */
	nice = kzalloc(TILER_WIDTH / div * 16, GFP_KERNEL);
	map = kzalloc((TILER_HEIGHT + 1) * sizeof(*map), GFP_KERNEL);
	global_map = kzalloc((TILER_WIDTH / div + 1)
			* (TILER_HEIGHT + 1) * sizeof(*global_map), GFP_KERNEL);
	if (!map || !global_map || !nice) {
		printk(KERN_ERR "could not allocate map for debug print\n");
		goto error;
	}
	for (i = 0; i <= TILER_HEIGHT; i++) {
		map[i] = global_map + i * (TILER_WIDTH / div + 1);
		for (j = 0; j < TILER_WIDTH / div; j++)
			map[i][j] = ' ';
		map[i][j] = 0;
	}

	/* get all allocations */
	mutex_lock(&mtx);

	list_for_each_entry(mi, &blocks, global) {
		if (mi->area.is2d) {
			ai = mi->parent;
			fill_map(map, div, &ai->area, *a2dp, false, *a2dp_c);
			fill_map(map, div, &mi->area, *m2dp, true, *m2dp_c);
			map_2d_info(map, div, nice, mi, *m2dp_c | 0xf);
			if (!*++a2dp)
				a2dp = a2d;
			if (!*++a2dp_c)
				a2dp_c = a2d_c;
			if (!*++m2dp)
				m2dp = m2d;
			if (!*++m2dp_c)
				m2dp_c = m2d_c;
		} else {
			bool start = read_map_pt(map, div, &mi->area.p0) == ' ';
			bool end = read_map_pt(map, div, &mi->area.p1) == ' ';
			tcm_for_each_slice(a, mi->area, p)
				fill_map(map, div, &a, '=', true, *m1dp_c);
			fill_map_pt(map, div, &mi->area.p0, start ? '<' : 'X');
			fill_map_pt(map, div, &mi->area.p1, end ? '>' : 'X');
			map_1d_info(map, div, nice, &mi->area, *m1dp_c | 0xf);
			if (!*++m1dp_c)
				m1dp_c = m1d_c;
		}
	}

	for (i = 0; i < TILER_WIDTH / div; i++)
		map[TILER_HEIGHT][i] = ':' | 0x0f00;
	text_map(map, div, " BEGIN TILER MAP ", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);

	printk(KERN_ERR "\n");
	write_out(map, "   ", TILER_HEIGHT, color, nice);
	for (i = 0; i < TILER_HEIGHT; i++)
		write_out(map, "%03d", i, color, nice);

	text_map(map, div, ": END TILER MAP :", TILER_HEIGHT, 0,
							TILER_WIDTH - 1, 0xf);
	write_out(map, "   ", TILER_HEIGHT, color, nice);

	mutex_unlock(&mtx);

error:
	kfree(map);
	kfree(global_map);
	kfree(nice);
}

static uint tiler_alloc_debug;
static int tiler_alloc_debug_set(const char *val, struct kernel_param *kp)
{
	int r = param_set_uint(val, kp);
	if (tiler_alloc_debug & 2) {
		print_allocation_map(tiler_alloc_debug & 4);
		tiler_alloc_debug &= ~2;
	}
	return r;
}

module_param_call(alloc_debug, tiler_alloc_debug_set, param_get_uint,
					&tiler_alloc_debug, 0644);

/* get process info, and increment refs for device tracking */
static struct process_info *__get_pi(pid_t pid, bool kernel)
{
	struct process_info *pi;

	/* find process context */
	mutex_lock(&mtx);
	list_for_each_entry(pi, &procs, list) {
		if (pi->pid == pid && pi->kernel == kernel)
			goto done;
	}

	/* create process context */
	pi = kmalloc(sizeof(*pi), GFP_KERNEL);
	if (!pi)
		goto done;

	memset(pi, 0, sizeof(*pi));
	pi->pid = pid;
	pi->kernel = kernel;
	INIT_LIST_HEAD(&pi->groups);
	INIT_LIST_HEAD(&pi->bufs);
	list_add(&pi->list, &procs);
done:
	if (pi && !kernel)
		pi->refs++;
	mutex_unlock(&mtx);
	return pi;
}

/* allocate an reserved area of size, alignment and link it to gi */
static struct area_info *area_new(u16 width, u16 height, u16 align,
				  struct tcm *tcm, struct gid_info *gi)
{
	struct area_info *ai = kmalloc(sizeof(*ai), GFP_KERNEL);
	if (!ai)
		return NULL;

	/* set up empty area info */
	memset(ai, 0x0, sizeof(*ai));
	INIT_LIST_HEAD(&ai->blocks);

	/* reserve an allocation area */
	if (tcm_reserve_2d(tcm, width, height, align, &ai->area)) {
		kfree(ai);
		return NULL;
	}

	ai->gi = gi;
	mutex_lock(&mtx);
	list_add_tail(&ai->by_gid, &gi->areas);
	mutex_unlock(&mtx);
	return ai;
}

/* (must have mutex) free an area and return NULL */
static inline void _m_area_free(struct area_info *ai)
{
	if (ai) {
		list_del(&ai->by_gid);
		kfree(ai);
	}
}

static s32 __analize_area(enum tiler_fmt fmt, u32 width, u32 height,
			  u16 *x_area, u16 *y_area, u16 *band,
			  u16 *align, u16 *offs)
{
	/* input: width, height is in pixels, align, offs in bytes */
	/* output: x_area, y_area, band, align, offs in slots */

	/* slot width, height, and row size */
	u32 slot_w, slot_h, slot_row, bpp;

	/* align must be 2 power */
	if (*align & (*align - 1))
		return -1;

	switch (fmt) {
	case TILFMT_8BIT:
		slot_w = DMM_PAGE_DIMM_X_MODE_8;
		slot_h = DMM_PAGE_DIMM_Y_MODE_8;
		break;
	case TILFMT_16BIT:
		slot_w = DMM_PAGE_DIMM_X_MODE_16;
		slot_h = DMM_PAGE_DIMM_Y_MODE_16;
		break;
	case TILFMT_32BIT:
		slot_w = DMM_PAGE_DIMM_X_MODE_32;
		slot_h = DMM_PAGE_DIMM_Y_MODE_32;
		break;
	case TILFMT_PAGE:
		/* adjust size to accomodate offset, only do page alignment */
		*align = PAGE_SIZE;
		width += *offs & (PAGE_SIZE - 1);

		/* for 1D area keep the height (1), width is in tiler slots */
		*x_area = DIV_ROUND_UP(width, TILER_PAGE);
		*y_area = *band = 1;

		if (*x_area * *y_area > TILER_WIDTH * TILER_HEIGHT)
			return -1;
		return 0;
	default:
		return -EINVAL;
	}

	/* get the # of bytes per row in 1 slot */
	bpp = tilfmt_bpp(fmt);
	slot_row = slot_w * bpp;

	/* how many slots are can be accessed via one physical page */
	*band = PAGE_SIZE / slot_row;

	/* minimum alignment is 1 slot, default alignment is page size */
	*align = ALIGN(*align ? : PAGE_SIZE, slot_row);

	/* offset must be multiple of bpp */
	if (*offs & (bpp - 1))
		return -EINVAL;

	/* round down the offset to the nearest slot size, and increase width
	   to allow space for having the correct offset */
	width += (*offs & (*align - 1)) / bpp;
	*offs &= ~(*align - 1);

	/* adjust to slots */
	*x_area = DIV_ROUND_UP(width, slot_w);
	*y_area = DIV_ROUND_UP(height, slot_h);
	*align /= slot_row;
	*offs /= slot_row;

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
 *              will be stored.  The block should be inserted
 *              before this block.
 *
 * @return the end coordinate (x1 + 1) where a block would fit,
 *         or 0 if it does not fit.
 *
 * (must have mutex)
 */
static u16 _m_blk_find_fit(u16 w, u16 align, u16 offs,
		     struct area_info *ai, struct list_head **before)
{
	int x = ai->area.p0.x + w + offs;
	struct mem_info *mi;

	/* area blocks are sorted by x */
	list_for_each_entry(mi, &ai->blocks, by_area) {
		/* check if buffer would fit before this area */
		if (x <= mi->area.p0.x) {
			*before = &mi->by_area;
			return x;
		}
		x = ALIGN(mi->area.p1.x + 1 - offs, align) + w + offs;
	}
	*before = &ai->blocks;

	/* check if buffer would fit after last area */
	return (x <= ai->area.p1.x + 1) ? x : 0;
}

/* (must have mutex) adds a block to an area with certain x coordinates */
static inline
struct mem_info *_m_add2area(struct mem_info *mi, struct area_info *ai,
				u16 x0, u16 x1, struct list_head *before)
{
	mi->parent = ai;
	mi->area = ai->area;
	mi->area.p0.x = x0;
	mi->area.p1.x = x1;
	list_add_tail(&mi->by_area, before);
	ai->nblocks++;
	return mi;
}

static struct mem_info *get_2d_area(u16 w, u16 h, u16 align, u16 offs, u16 band,
					struct gid_info *gi, struct tcm *tcm) {
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
			if (tiler_alloc_debug & 1)
				printk(KERN_ERR "(=2d (%d-%d,%d-%d) in (%d-%d,%d-%d) prereserved)\n",
					mi->area.p0.x, mi->area.p1.x,
					mi->area.p0.y, mi->area.p1.y,
			((struct area_info *) mi->parent)->area.p0.x,
			((struct area_info *) mi->parent)->area.p1.x,
			((struct area_info *) mi->parent)->area.p0.y,
			((struct area_info *) mi->parent)->area.p1.y);
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
	list_for_each_entry(ai, &gi->areas, by_gid) {
		if (ai->area.tcm == tcm &&
		    tcm_aheight(ai->area) == h) {
			x = _m_blk_find_fit(w, align, offs, ai, &before);
			if (x) {
				_m_add2area(mi, ai, x - w, x - 1, before);
				if (tiler_alloc_debug & 1)
					printk(KERN_ERR "(+2d (%d-%d,%d-%d) in (%d-%d,%d-%d) existing)\n",
						mi->area.p0.x, mi->area.p1.x,
						mi->area.p0.y, mi->area.p1.y,
				((struct area_info *) mi->parent)->area.p0.x,
				((struct area_info *) mi->parent)->area.p1.x,
				((struct area_info *) mi->parent)->area.p0.y,
				((struct area_info *) mi->parent)->area.p1.y);
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
		_m_add2area(mi, ai, ai->area.p0.x + offs,
			     ai->area.p0.x + offs + w - 1,
			     &ai->blocks);
		if (tiler_alloc_debug & 1)
			printk(KERN_ERR "(+2d (%d-%d,%d-%d) in (%d-%d,%d-%d) new)\n",
				mi->area.p0.x, mi->area.p1.x,
				mi->area.p0.y, mi->area.p1.y,
				ai->area.p0.x, ai->area.p1.x,
				ai->area.p0.y, ai->area.p1.y);
	} else {
		/* clean up */
		kfree(mi);
		return NULL;
	}

done:
	mutex_unlock(&mtx);
	return mi;
}

/* (must have mutex) */
static void _m_try_free_group(struct gid_info *gi)
{
	if (gi && list_empty(&gi->areas) && list_empty(&gi->onedim)) {
		WARN_ON(!list_empty(&gi->reserved));
		list_del(&gi->by_pid);

		/* if group is tracking kernel objects, we may free even
		   the process info */
		if (gi->pi->kernel && list_empty(&gi->pi->groups)) {
			list_del(&gi->pi->list);
			kfree(gi->pi);
		}

		kfree(gi);
	}
}

static void clear_pat(struct tmm *tmm, struct tcm_area *area)
{
	struct pat_area p_area = {0};
	struct tcm_area slice, area_s;

	tcm_for_each_slice(slice, *area, area_s) {
		p_area.x0 = slice.p0.x;
		p_area.y0 = slice.p0.y;
		p_area.x1 = slice.p1.x;
		p_area.y1 = slice.p1.y;

		tmm_clear(tmm, p_area);
	}
}

/* (must have mutex) free block and any freed areas */
static s32 _m_free(struct mem_info *mi)
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
	if (mi->by_area.next)
		list_del(&mi->by_area);

	/* remove block from area first if 2D */
	if (mi->area.is2d) {
		ai = mi->parent;
		if (!ai) {
			printk(KERN_ERR "Null parent pointer!\n");
			WARN_ON(1);
			kfree(mi);
			return -EFAULT;
		}
		/* check to see if area needs removing also */
		if (!--ai->nblocks) {
			if (tiler_alloc_debug & 1)
				printk(KERN_ERR "(-2d (%d-%d,%d-%d) in (%d-%d,%d-%d) last)\n",
					mi->area.p0.x, mi->area.p1.x,
					mi->area.p0.y, mi->area.p1.y,
					ai->area.p0.x, ai->area.p1.x,
					ai->area.p0.y, ai->area.p1.y);
			clear_pat(TMM_SS(mi->sys_addr), &ai->area);
			res = tcm_free(&ai->area);
			list_del(&ai->by_gid);
			/* try to remove parent if it became empty */
			_m_try_free_group(ai->gi);
			kfree(ai);
			ai = NULL;
		} else if (tiler_alloc_debug & 1)
			printk(KERN_ERR "(-2d (%d-%d,%d-%d) in (%d-%d,%d-%d) remaining)\n",
				mi->area.p0.x, mi->area.p1.x,
				mi->area.p0.y, mi->area.p1.y,
				ai->area.p0.x, ai->area.p1.x,
				ai->area.p0.y, ai->area.p1.y);

	} else {
		if (tiler_alloc_debug & 1)
			printk(KERN_ERR "(-1d: %d,%d..%d,%d)\n",
				mi->area.p0.x, mi->area.p0.y,
				mi->area.p1.x, mi->area.p1.y);
		/* remove 1D area */
		clear_pat(TMM_SS(mi->sys_addr), &mi->area);
		res = tcm_free(&mi->area);
		/* try to remove parent if it became empty */
		_m_try_free_group(mi->parent);
	}

	kfree(mi);
	return res;
}

/* (must have mutex) returns true if block was freed */
static bool _m_chk_ref(struct mem_info *mi)
{
	/* check references */
	if (mi->refs)
		return 0;

	if (_m_free(mi))
		printk(KERN_ERR "error while removing tiler block\n");

	return 1;
}

/* (must have mutex) */
static inline s32 _m_dec_ref(struct mem_info *mi)
{
	if (mi->refs-- <= 1)
		return _m_chk_ref(mi);

	return 0;
}

/* (must have mutex) */
static inline void _m_inc_ref(struct mem_info *mi)
{
	mi->refs++;
}

/* (must have mutex) returns true if block was freed */
static inline bool _m_try_free(struct mem_info *mi)
{
	if (mi->alloced) {
		mi->refs--;
		mi->alloced = false;
	}
	return _m_chk_ref(mi);
}

static s32 register_buf(struct __buf_info *_b, struct process_info *pi)
{
	struct mem_info *mi = NULL;
	struct tiler_buf_info *b = &_b->buf_info;
	u32 i, num = b->num_blocks, remain = num;

	/* check validity */
	if (num > TILER_MAX_NUM_BLOCKS)
		return -EINVAL;

	mutex_lock(&mtx);

	/* find each block */
	list_for_each_entry(mi, &blocks, global) {
		for (i = 0; i < num; i++) {
			if (!_b->mi[i] && mi->sys_addr == b->blocks[i].ssptr) {
				_b->mi[i] = mi;

				/* quit if found all*/
				if (!--remain)
					break;

			}
		}
	}

	/* if found all, register buffer */
	if (!remain) {
		b->offset = id;
		id += 0x1000;

		list_add(&_b->by_pid, &pi->bufs);

		/* using each block */
		for (i = 0; i < num; i++)
			_m_inc_ref(_b->mi[i]);
	}

	mutex_unlock(&mtx);

	return remain ? -EACCES : 0;
}

/* must have mutex */
static void _m_unregister_buf(struct __buf_info *_b)
{
	u32 i;

	/* unregister */
	list_del(&_b->by_pid);

	/* no longer using the blocks */
	for (i = 0; i < _b->buf_info.num_blocks; i++)
		_m_dec_ref(_b->mi[i]);

	kfree(_b);
}

static int tiler_notify_event(int event, void *data)
{
	return blocking_notifier_call_chain(&tiler_device->notifier,
								event, data);
}

/**
 * Free all info kept by a process:
 *
 * all registered buffers, allocated blocks, and unreferenced
 * blocks.  Any blocks/areas still referenced will move to the
 * orphaned lists to avoid issues if a new process is created
 * with the same pid.
 *
 * (must have mutex)
 */
static void _m_free_process_info(struct process_info *pi)
{
	struct area_info *ai, *ai_;
	struct mem_info *mi, *mi_;
	struct gid_info *gi, *gi_;
	struct __buf_info *_b = NULL, *_b_ = NULL;
	bool ai_autofreed, need2free;

	if (!list_empty(&pi->bufs))
		tiler_notify_event(TILER_DEVICE_CLOSE, NULL);

	/* unregister all buffers */
	list_for_each_entry_safe(_b, _b_, &pi->bufs, by_pid)
		_m_unregister_buf(_b);

	WARN_ON(!list_empty(&pi->bufs));

	/* free all allocated blocks, and remove unreferenced ones */
	list_for_each_entry_safe(gi, gi_, &pi->groups, by_pid) {

		/*
		 * Group info structs when they become empty on an _m_try_free.
		 * However, if the group info is already empty, we need to
		 * remove it manually
		 */
		need2free = list_empty(&gi->areas) && list_empty(&gi->onedim);
		list_for_each_entry_safe(ai, ai_, &gi->areas, by_gid) {
			ai_autofreed = true;
			list_for_each_entry_safe(mi, mi_, &ai->blocks, by_area)
				ai_autofreed &= _m_try_free(mi);

			/* save orphaned areas for later removal */
			if (!ai_autofreed) {
				need2free = true;
				ai->gi = NULL;
				list_move(&ai->by_gid, &orphan_areas);
			}
		}

		list_for_each_entry_safe(mi, mi_, &gi->onedim, by_area) {
			if (!_m_try_free(mi)) {
				need2free = true;
				/* save orphaned 1D blocks */
				mi->parent = NULL;
				list_move(&mi->by_area, &orphan_onedim);
			}
		}

		/* if group is still alive reserved list should have been
		   emptied as there should be no reference on those blocks */
		if (need2free) {
			WARN_ON(!list_empty(&gi->onedim));
			WARN_ON(!list_empty(&gi->areas));
			_m_try_free_group(gi);
		}
	}

	WARN_ON(!list_empty(&pi->groups));
	list_del(&pi->list);
	kfree(pi);
}

static s32 get_area(u32 sys_addr, struct tcm_pt *pt)
{
	enum tiler_fmt fmt;

	sys_addr &= TILER_ALIAS_VIEW_CLEAR;
	fmt = TILER_GET_ACC_MODE(sys_addr);

	switch (fmt) {
	case TILFMT_8BIT:
		pt->x = DMM_HOR_X_PAGE_COOR_GET_8(sys_addr);
		pt->y = DMM_HOR_Y_PAGE_COOR_GET_8(sys_addr);
		break;
	case TILFMT_16BIT:
		pt->x = DMM_HOR_X_PAGE_COOR_GET_16(sys_addr);
		pt->y = DMM_HOR_Y_PAGE_COOR_GET_16(sys_addr);
		break;
	case TILFMT_32BIT:
		pt->x = DMM_HOR_X_PAGE_COOR_GET_32(sys_addr);
		pt->y = DMM_HOR_Y_PAGE_COOR_GET_32(sys_addr);
		break;
	case TILFMT_PAGE:
		pt->x = (sys_addr & 0x7FFFFFF) >> 12;
		pt->y = pt->x / TILER_WIDTH;
		pt->x &= (TILER_WIDTH - 1);
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

/* must have mutex */
static struct gid_info *_m_get_gi(struct process_info *pi, u32 gid)
{
	struct gid_info *gi;

	/* see if group already exist */
	list_for_each_entry(gi, &pi->groups, by_pid) {
		if (gi->gid == gid)
			return gi;
	}

	/* create new group */
	gi = kmalloc(sizeof(*gi), GFP_KERNEL);
	if (!gi)
		return gi;

	memset(gi, 0, sizeof(*gi));
	INIT_LIST_HEAD(&gi->areas);
	INIT_LIST_HEAD(&gi->onedim);
	INIT_LIST_HEAD(&gi->reserved);
	gi->pi = pi;
	gi->gid = gid;
	list_add(&gi->by_pid, &pi->groups);
	return gi;
}

static struct mem_info *__get_area(enum tiler_fmt fmt, u32 width, u32 height,
				   u16 align, u16 offs, struct gid_info *gi)
{
	u16 x, y, band;
	struct mem_info *mi = NULL;

	/* calculate dimensions, band, offs and alignment in slots */
	if (__analize_area(fmt, width, height, &x, &y, &band, &align, &offs))
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
		if (tiler_alloc_debug & 1)
			printk(KERN_ERR "(+1d: %d,%d..%d,%d)\n",
				mi->area.p0.x, mi->area.p0.y,
				mi->area.p1.x, mi->area.p1.y);
		mutex_lock(&mtx);
		mi->parent = gi;
		list_add(&mi->by_area, &gi->onedim);
	} else {
		mi = get_2d_area(x, y, align, offs, band, gi, TCM(fmt));
		if (!mi)
			return NULL;

		mutex_lock(&mtx);
	}

	list_add(&mi->global, &blocks);
	mi->alloced = true;
	mi->refs++;
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
	struct process_info *pi = filp->private_data;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	mutex_lock(&mtx);
	list_for_each(pos, &pi->bufs) {
		_b = list_entry(pos, struct __buf_info, by_pid);
		if ((vma->vm_pgoff << PAGE_SHIFT) == _b->buf_info.offset)
			break;
	}
	mutex_unlock(&mtx);
	if (!_b)
		return -ENXIO;

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

static s32 refill_pat(struct tmm *tmm, struct tcm_area *area, u32 *ptr)
{
	s32 res = 0;
	struct pat_area p_area = {0};
	struct tcm_area slice, area_s;

	tcm_for_each_slice(slice, *area, area_s) {
		p_area.x0 = slice.p0.x;
		p_area.y0 = slice.p0.y;
		p_area.x1 = slice.p1.x;
		p_area.y1 = slice.p1.y;

		memcpy(dmac_va, ptr, sizeof(*ptr) * tcm_sizeof(slice));
		ptr += tcm_sizeof(slice);

		if (tmm_map(tmm, p_area, dmac_pa)) {
			res = -EFAULT;
			break;
		}
	}

	return res;
}

static s32 map_block(enum tiler_fmt fmt, u32 width, u32 height, u32 gid,
			struct process_info *pi, u32 *sys_addr, u32 usr_addr)
{
	u32 i = 0, tmp = -1, *mem = NULL;
	u8 write = 0;
	s32 res = -ENOMEM;
	struct mem_info *mi = NULL;
	struct page *page = NULL;
	struct task_struct *curr_task = current;
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma = NULL;
	struct gid_info *gi = NULL;

	/* we only support mapping a user buffer in page mode */
	if (fmt != TILFMT_PAGE)
		return -EPERM;

	/* check if mapping is supported by tmm */
	if (!tmm_can_map(TMM(fmt)))
		return -EPERM;

	/* get group context */
	mutex_lock(&mtx);
	gi = _m_get_gi(pi, gid);
	mutex_unlock(&mtx);

	if (!gi)
		return -ENOMEM;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, 0, 0, gi);
	if (!mi) {
		mutex_lock(&mtx);
		_m_try_free_group(gi);
		mutex_unlock(&mtx);
		return -ENOMEM;
	}

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

	/* Ensure the data reaches to main memory before PAT refill */
	wmb();

	if (refill_pat(TMM(fmt), &mi->area, mem))
		goto fault;

	res = 0;
	goto done;
fault:
	up_read(&mm->mmap_sem);
done:
	if (res) {
		mutex_lock(&mtx);
		_m_free(mi);
		mutex_unlock(&mtx);
	}
	kfree(mem);
	return res;
}

s32 tiler_mapx(enum tiler_fmt fmt, u32 width, u32 height, u32 gid,
		       pid_t pid, u32 *sys_addr, u32 usr_addr)
{
	return map_block(fmt, width, height, gid, __get_pi(pid, true),
							sys_addr, usr_addr);
}
EXPORT_SYMBOL(tiler_mapx);

s32 tiler_map(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr,
								u32 usr_addr)
{
	return tiler_mapx(fmt, width, height, 0, current->tgid, sys_addr,
								usr_addr);
}
EXPORT_SYMBOL(tiler_map);

static s32 free_block(u32 sys_addr, struct process_info *pi)
{
	struct gid_info *gi = NULL;
	struct area_info *ai = NULL;
	struct mem_info *mi = NULL;
	s32 res = -ENOENT;

	mutex_lock(&mtx);

	/* find block in process list and free it */
	list_for_each_entry(gi, &pi->groups, by_pid) {
		/* currently we know if block is 1D or 2D by the address */
		if (TILER_GET_ACC_MODE(sys_addr) == TILFMT_PAGE) {
			list_for_each_entry(mi, &gi->onedim, by_area) {
				if (mi->sys_addr == sys_addr) {
					_m_try_free(mi);
					res = 0;
					goto done;
				}
			}
		} else {
			list_for_each_entry(ai, &gi->areas, by_gid) {
				list_for_each_entry(mi, &ai->blocks, by_area) {
					if (mi->sys_addr == sys_addr) {
						_m_try_free(mi);
						res = 0;
						goto done;
					}
				}
			}
		}
	}

done:
	mutex_unlock(&mtx);

	/* for debugging, we can set the PAT entries to DMM_LISA_MAP__0 */
	return res;
}

s32 tiler_free(u32 sys_addr)
{
	struct mem_info *mi;
	s32 res = -ENOENT;

	mutex_lock(&mtx);

	/* find block in global list and free it */
	list_for_each_entry(mi, &blocks, global) {
		if (mi->sys_addr == sys_addr) {
			_m_try_free(mi);
			res = 0;
			break;
		}
	}
	mutex_unlock(&mtx);

	/* for debugging, we can set the PAT entries to DMM_LISA_MAP__0 */
	return res;
}
EXPORT_SYMBOL(tiler_free);

/* :TODO: Currently we do not track enough information from alloc to get back
   the actual width and height of the container, so we must make a guess.  We
   do not even have enough information to get the virtual stride of the buffer,
   which is the real reason for this ioctl */
static s32 find_block(u32 sys_addr, struct tiler_block_info *blk)
{
	struct mem_info *i;
	struct tcm_pt pt;

	if (get_area(sys_addr, &pt))
		return -EFAULT;

	list_for_each_entry(i, &blocks, global) {
		if (tcm_is_in(pt, i->area))
			goto found;
	}

	blk->fmt = TILFMT_INVALID;
	blk->dim.len = blk->stride = blk->ssptr = 0;
	return -EFAULT;

found:
	blk->ptr = NULL;
	blk->fmt = TILER_GET_ACC_MODE(sys_addr);
	blk->ssptr = __get_alias_addr(blk->fmt, i->area.p0.x, i->area.p0.y);

	if (blk->fmt == TILFMT_PAGE) {
		blk->dim.len = tcm_sizeof(i->area) * TILER_PAGE;
		blk->stride = 0;
	} else {
		blk->stride = blk->dim.area.width =
			tcm_awidth(i->area) * TILER_BLOCK_WIDTH;
		blk->dim.area.height = tcm_aheight(i->area)
							* TILER_BLOCK_HEIGHT;
		if (blk->fmt != TILFMT_8BIT) {
			blk->stride <<= 1;
			blk->dim.area.height >>= 1;
			if (blk->fmt == TILFMT_32BIT)
				blk->dim.area.width >>= 1;
		}
		blk->stride = PAGE_ALIGN(blk->stride);
	}
	return 0;
}

static s32 alloc_block(enum tiler_fmt fmt, u32 width, u32 height,
			u32 align, u32 offs, u32 gid, struct process_info *pi,
			u32 *sys_addr);

static s32 tiler_ioctl(struct inode *ip, struct file *filp, u32 cmd,
			unsigned long arg)
{
	pgd_t *pgd = NULL;
	pmd_t *pmd = NULL;
	pte_t *ptep = NULL, pte = 0x0;
	s32 r = -1;
	u32 til_addr = 0x0;
	struct process_info *pi = filp->private_data;

	struct __buf_info *_b = NULL;
	struct tiler_buf_info buf_info = {0};
	struct tiler_block_info block_info = {0};

	switch (cmd) {
	case TILIOC_GBUF:
		if (copy_from_user(&block_info, (void __user *)arg,
					sizeof(block_info)))
			return -EFAULT;

		switch (block_info.fmt) {
		case TILFMT_PAGE:
			r = alloc_block(block_info.fmt, block_info.dim.len, 1,
						0, 0, 0, pi, &til_addr);
			if (r)
				return r;
			break;
		case TILFMT_8BIT:
		case TILFMT_16BIT:
		case TILFMT_32BIT:
			r = alloc_block(block_info.fmt,
					block_info.dim.area.width,
					block_info.dim.area.height,
					0, 0, 0, pi, &til_addr);
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

		/* search current process first, then all processes */
		free_block(block_info.ssptr, pi) ?
			tiler_free(block_info.ssptr) : 0;

		/* free always succeeds */
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

		if (map_block(block_info.fmt, block_info.dim.len, 1, 0, pi,
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
		list_for_each_entry(_b, &pi->bufs, by_pid) {
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

		r = register_buf(_b, pi);
		if (r) {
			kfree(_b); return -EACCES;
		}

		if (copy_to_user((void __user *)arg, &_b->buf_info,
					sizeof(_b->buf_info))) {
			_m_unregister_buf(_b);
			return -EFAULT;
		}
		break;
	case TILIOC_URBUF:
		if (copy_from_user(&buf_info, (void __user *)arg,
					sizeof(buf_info)))
			return -EFAULT;

		mutex_lock(&mtx);
		/* buffer registration is per process */
		list_for_each_entry(_b, &pi->bufs, by_pid) {
			if (buf_info.offset == _b->buf_info.offset) {
				_m_unregister_buf(_b);
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

		if (find_block(block_info.ssptr, &block_info))
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

static s32 alloc_block(enum tiler_fmt fmt, u32 width, u32 height,
		   u32 align, u32 offs, u32 gid, struct process_info *pi,
		   u32 *sys_addr)
{
	struct mem_info *mi = NULL;
	struct gid_info *gi = NULL;

	/* only support up to page alignment */
	if (align > PAGE_SIZE || offs > align || !pi)
		return -EINVAL;

	/* get group context */
	mutex_lock(&mtx);
	gi = _m_get_gi(pi, gid);
	mutex_unlock(&mtx);

	if (!gi)
		return -ENOMEM;

	/* reserve area in tiler container */
	mi = __get_area(fmt, width, height, align, offs, gi);
	if (!mi) {
		mutex_lock(&mtx);
		_m_try_free_group(gi);
		mutex_unlock(&mtx);
		return -ENOMEM;
	}

	*sys_addr = mi->sys_addr;

	/* allocate and map if mapping is supported */
	if (tmm_can_map(TMM(fmt))) {
		mi->num_pg = tcm_sizeof(mi->area);

		mi->mem = tmm_get(TMM(fmt), mi->num_pg);
		if (!mi->mem)
			goto cleanup;

		/* Ensure the data reaches to main memory before PAT refill */
		wmb();

		/* program PAT */
		if (refill_pat(TMM(fmt), &mi->area, mi->mem))
			goto cleanup;
	}
	return 0;

cleanup:
	mutex_lock(&mtx);
	_m_free(mi);
	mutex_unlock(&mtx);
	return -ENOMEM;

}

s32 tiler_allocx(enum tiler_fmt fmt, u32 width, u32 height,
		 u32 align, u32 offs, u32 gid, pid_t pid, u32 *sys_addr)
{
	return alloc_block(fmt, width, height, align, offs, gid,
			      __get_pi(pid, true), sys_addr);
}
EXPORT_SYMBOL(tiler_allocx);

s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr)
{
	return tiler_allocx(fmt, width, height, 0, 0,
			    0, current->tgid, sys_addr);
}
EXPORT_SYMBOL(tiler_alloc);


static void reserve_nv12_blocks(u32 n, u32 width, u32 height,
				  u32 align, u32 offs, u32 gid, pid_t pid)
{
}

static void reserve_blocks(u32 n, enum tiler_fmt fmt, u32 width, u32 height,
			     u32 align, u32 offs, u32 gid, pid_t pid)
{
}

/* reserve area for n identical buffers */
s32 tiler_reservex(u32 n, struct tiler_buf_info *b, pid_t pid)
{
	u32 i;

	if (b->num_blocks > TILER_MAX_NUM_BLOCKS)
		return -EINVAL;

	for (i = 0; i < b->num_blocks; i++) {
		/* check for NV12 reservations */
		if (i + 1 < b->num_blocks &&
		    b->blocks[i].fmt == TILFMT_8BIT &&
		    b->blocks[i + 1].fmt == TILFMT_16BIT &&
		    b->blocks[i].dim.area.height ==
			b->blocks[i + 1].dim.area.height &&
		    b->blocks[i].dim.area.width ==
			b->blocks[i + 1].dim.area.width) {
			reserve_nv12_blocks(n,
					      b->blocks[i].dim.area.width,
					      b->blocks[i].dim.area.height,
					      0, /* align */
					      0, /* offs */
					      0, /* gid */
					      pid);
			i++;
		} else if (b->blocks[i].fmt >= TILFMT_8BIT &&
			   b->blocks[i].fmt <= TILFMT_32BIT) {
			/* other 2D reservations */
			reserve_blocks(n,
					 b->blocks[i].fmt,
					 b->blocks[i].dim.area.width,
					 b->blocks[i].dim.area.height,
					 0, /* align */
					 0, /* offs */
					 0, /* gid */
					 pid);
		} else {
			return -EINVAL;
		}
	}
	return 0;
}
EXPORT_SYMBOL(tiler_reservex);

s32 tiler_reserve(u32 n, struct tiler_buf_info *b)
{
	return tiler_reservex(n, b, current->tgid);
}
EXPORT_SYMBOL(tiler_reserve);

int tiler_reg_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_register(&tiler_device->notifier, nb);
}
EXPORT_SYMBOL(tiler_reg_notifier);

int tiler_unreg_notifier(struct notifier_block *nb)
{
	if (!nb)
		return -EINVAL;
	return blocking_notifier_chain_unregister(&tiler_device->notifier, nb);
}
EXPORT_SYMBOL(tiler_unreg_notifier);

static void __exit tiler_exit(void)
{
	struct process_info *pi = NULL, *pi_ = NULL;
	int i, j;

	mutex_lock(&mtx);

	/* free all process data */
	list_for_each_entry_safe(pi, pi_, &procs, list)
		_m_free_process_info(pi);

	/* all lists should have cleared */
	WARN_ON(!list_empty(&blocks));
	WARN_ON(!list_empty(&procs));
	WARN_ON(!list_empty(&orphan_onedim));
	WARN_ON(!list_empty(&orphan_areas));

	mutex_unlock(&mtx);

	dma_free_coherent(NULL, TILER_WIDTH * TILER_HEIGHT * sizeof(*dmac_va),
							dmac_va, dmac_pa);

	/* close containers only once */
	for (i = TILFMT_8BIT; i <= TILFMT_MAX; i++) {
		/* remove identical containers (tmm is unique per tcm) */
		for (j = i + 1; j <= TILFMT_MAX; j++)
			if (TCM(i) == TCM(j)) {
				TCM_SET(j, NULL);
				TMM_SET(j, NULL);
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
	struct process_info *pi = __get_pi(current->tgid, false);

	if (!pi)
		return -ENOMEM;

	filp->private_data = pi;
	return 0x0;
}

static s32 tiler_release(struct inode *ip, struct file *filp)
{
	struct process_info *pi = filp->private_data;

	mutex_lock(&mtx);
	/* free resources if last device in this process */
	if (0 == --pi->refs)
		_m_free_process_info(pi);

	mutex_unlock(&mtx);

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

	if (!cpu_is_omap44xx())
		return 0;

	/**
	  * Array of physical pages for PAT programming, which must be a 16-byte
	  * aligned physical address
	*/
	dmac_va = dma_alloc_coherent(NULL, TILER_WIDTH * TILER_HEIGHT *
					sizeof(*dmac_va), &dmac_pa, GFP_ATOMIC);
	if (!dmac_va)
		return -ENOMEM;

	/* Allocate tiler container manager (we share 1 on OMAP4) */
	div_pt.x = TILER_WIDTH;   /* hardcoded default */
	div_pt.y = (3 * TILER_HEIGHT) / 4;
	sita = sita_init(TILER_WIDTH, TILER_HEIGHT, (void *)&div_pt);

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
	INIT_LIST_HEAD(&blocks);
	INIT_LIST_HEAD(&procs);
	INIT_LIST_HEAD(&orphan_areas);
	INIT_LIST_HEAD(&orphan_onedim);
	BLOCKING_INIT_NOTIFIER_HEAD(&tiler_device->notifier);
	id = 0xda7a000;

error:
	/* TODO: error handling for device registration */
	if (r) {
		kfree(tiler_device);
		tcm_deinit(sita);
		tmm_deinit(tmm_pat);
		dma_free_coherent(NULL, TILER_WIDTH * TILER_HEIGHT *
					sizeof(*dmac_va), dmac_va, dmac_pa);
	}

	return r;
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("David Sin <davidsin@ti.com>");
MODULE_AUTHOR("Lajos Molnar <molnar@ti.com>");
module_init(tiler_init);
module_exit(tiler_exit);
