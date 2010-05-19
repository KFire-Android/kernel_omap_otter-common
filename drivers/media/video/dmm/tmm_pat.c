/*
 * tmm_pat.c
 *
 * DMM driver support functions for TI OMAP processors.
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
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <asm/cacheflush.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/slab.h>

#include "tmm.h"

/**
 * Number of pages to allocate when
 * refilling the free page stack.
 */
#define MAX 16
#define DMM_PAGE 0x1000

/**
  * Used to keep track of mem per
  * dmm_get_pages call.
  */
struct fast {
	struct list_head list;
	struct mem **mem;
	u32 *pa;
	u32 num;
};

/**
 * Used to keep track of the page struct ptrs
 * and physical addresses of each page.
 */
struct mem {
	struct list_head list;
	struct page *pg;
	u32 pa;
};

/**
 * TMM PAT private structure
 */
struct dmm_mem {
	struct fast fast_list;
	struct mem free_list;
	struct mem used_list;
	struct mutex mtx;
	struct dmm *dmm;
};

static void dmm_free_fast_list(struct fast *fast)
{
	struct list_head *pos = NULL, *q = NULL;
	struct fast *f = NULL;
	s32 i = 0;

	/* mutex is locked */
	list_for_each_safe(pos, q, &fast->list) {
		f = list_entry(pos, struct fast, list);
		for (i = 0; i < f->num; i++)
			__free_page(f->mem[i]->pg);
		kfree(f->pa);
		kfree(f->mem);
		list_del(pos);
		kfree(f);
	}
}

static u32 fill_page_stack(struct mem *mem, struct mutex *mtx)
{
	s32 i = 0;
	struct mem *m = NULL;

	for (i = 0; i < MAX; i++) {
		m = kmalloc(sizeof(*m), GFP_KERNEL);
		if (!m)
			return -ENOMEM;
		memset(m, 0x0, sizeof(*m));

		m->pg = alloc_page(GFP_KERNEL | GFP_DMA);
		if (!m->pg) {
			kfree(m);
			return -ENOMEM;
		}

		m->pa = page_to_phys(m->pg);

		/**
		 * Note: we need to flush the cache
		 * entry for each page we allocate.
		*/
		dmac_flush_range((void *)page_address(m->pg),
					(void *)page_address(m->pg) + DMM_PAGE);
		outer_flush_range(m->pa, m->pa + DMM_PAGE);

		mutex_lock(mtx);
		list_add(&m->list, &mem->list);
		mutex_unlock(mtx);
	}
	return 0x0;
}

static void dmm_free_page_stack(struct mem *mem)
{
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;

	/* mutex is locked */
	list_for_each_safe(pos, q, &mem->list) {
		m = list_entry(pos, struct mem, list);
		__free_page(m->pg);
		list_del(pos);
		kfree(m);
	}
}

static void tmm_pat_deinit(struct tmm *tmm)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;

	mutex_lock(&pvt->mtx);
	dmm_free_fast_list(&pvt->fast_list);
	dmm_free_page_stack(&pvt->free_list);
	dmm_free_page_stack(&pvt->used_list);
	mutex_destroy(&pvt->mtx);
}

static u32 *tmm_pat_get_pages(struct tmm *tmm, s32 n)
{
	s32 i = 0;
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;
	struct fast *f = NULL;
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;

	if (n <= 0 || n > 0x8000)
		return NULL;

	if (list_empty_careful(&pvt->free_list.list))
		if (fill_page_stack(&pvt->free_list, &pvt->mtx))
			return NULL;

	f = kmalloc(sizeof(*f), GFP_KERNEL);
	if (!f)
		return NULL;
	memset(f, 0x0, sizeof(*f));

	/* array of mem struct pointers */
	f->mem = kmalloc(n * sizeof(*f->mem), GFP_KERNEL);
	if (!f->mem) {
		kfree(f); return NULL;
	}
	memset(f->mem, 0x0, n * sizeof(*f->mem));

	/* array of physical addresses */
	f->pa = kmalloc(n * sizeof(*f->pa), GFP_KERNEL);
	if (!f->pa) {
		kfree(f->mem); kfree(f); return NULL;
	}
	memset(f->pa, 0x0, n * sizeof(*f->pa));

	/*
	 * store the number of mem structs so that we
	 * know how many to free later.
	 */
	f->num = n;

	for (i = 0; i < n; i++) {
		if (list_empty_careful(&pvt->free_list.list))
			if (fill_page_stack(&pvt->free_list, &pvt->mtx))
				goto cleanup;

		mutex_lock(&pvt->mtx);
		pos = NULL;
		q = NULL;

		/*
		 * remove one mem struct from the free list and
		 * add the address to the fast struct mem array
		 */
		list_for_each_safe(pos, q, &pvt->free_list.list) {
			m = list_entry(pos, struct mem, list);
			f->mem[i] = m;
			list_del(pos);
			break;
		}
		mutex_unlock(&pvt->mtx);

		if (m != NULL)
			f->pa[i] = m->pa;
		else
			goto cleanup;
	}

	mutex_lock(&pvt->mtx);
	list_add(&f->list, &pvt->fast_list.list);
	mutex_unlock(&pvt->mtx);

	if (f != NULL)
		return f->pa;
cleanup:
	for (; i > 0; i--) {
		mutex_lock(&pvt->mtx);
		list_add(&f->mem[i - 1]->list, &pvt->free_list.list);
		mutex_unlock(&pvt->mtx);
	}
	kfree(f->pa);
	kfree(f->mem);
	kfree(f);
	return NULL;
}

static void tmm_pat_free_pages(struct tmm *tmm, u32 *list)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;
	struct list_head *pos = NULL, *q = NULL;
	struct fast *f = NULL;
	s32 i = 0;

	mutex_lock(&pvt->mtx);
	pos = NULL;
	q = NULL;
	list_for_each_safe(pos, q, &pvt->fast_list.list) {
		f = list_entry(pos, struct fast, list);
		if (f->pa[0] == list[0]) {
			for (i = 0; i < f->num; i++) {
				list_add(&((struct mem *)f->mem[i])->list,
							&pvt->free_list.list);
			}
			list_del(pos);
			kfree(f->pa);
			kfree(f->mem);
			kfree(f);
			break;
		}
	}
	mutex_unlock(&pvt->mtx);
}

static s32 tmm_pat_map(struct tmm *tmm, struct pat_area area, u32 page_pa)
{
	struct dmm_mem *pvt = (struct dmm_mem *) tmm->pvt;
	struct pat pat_desc = {0};

	/* send pat descriptor to dmm driver */
	pat_desc.ctrl.dir = 0;
	pat_desc.ctrl.ini = 0;
	pat_desc.ctrl.lut_id = 0;
	pat_desc.ctrl.start = 1;
	pat_desc.ctrl.sync = 0;
	pat_desc.area = area;
	pat_desc.next = NULL;

	/* must be a 16-byte aligned physical address */
	pat_desc.data = page_pa;
	return dmm_pat_refill(pvt->dmm, &pat_desc, MANUAL);
}

struct tmm *tmm_pat_init(u32 pat_id)
{
	struct tmm *tmm = NULL;
	struct dmm_mem *pvt = NULL;

	struct dmm *dmm = dmm_pat_init(pat_id);
	if (dmm)
		tmm = kmalloc(sizeof(*tmm), GFP_KERNEL);
	if (tmm)
		pvt = kmalloc(sizeof(*pvt), GFP_KERNEL);
	if (pvt) {
		/* private data */
		pvt->dmm = dmm;
		INIT_LIST_HEAD(&pvt->free_list.list);
		INIT_LIST_HEAD(&pvt->used_list.list);
		INIT_LIST_HEAD(&pvt->fast_list.list);
		mutex_init(&pvt->mtx);

		if (list_empty_careful(&pvt->free_list.list))
			if (fill_page_stack(&pvt->free_list, &pvt->mtx))
				goto error;

		/* public data */
		tmm->pvt = pvt;
		tmm->deinit = tmm_pat_deinit;
		tmm->get = tmm_pat_get_pages;
		tmm->free = tmm_pat_free_pages;
		tmm->map = tmm_pat_map;
		tmm->clear = NULL;   /* not yet supported */

		return tmm;
	}

error:
	kfree(pvt);
	kfree(tmm);
	dmm_pat_release(dmm);
	return NULL;
}
EXPORT_SYMBOL(tmm_pat_init);

