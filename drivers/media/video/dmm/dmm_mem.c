/*
 * dmm_mem.c
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

#include "dmm_mem.h"

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
	u32 *mem;
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

static struct fast fast_list;
static struct mem free_list;
static struct mem used_list;
static struct mutex mtx;

static void dmm_free_fast_list(struct fast *fast)
{
	struct list_head *pos = NULL, *q = NULL;
	struct fast *f = NULL;
	s32 i = 0;

	mutex_lock(&mtx);
	list_for_each_safe(pos, q, &fast->list) {
		f = list_entry(pos, struct fast, list);
		for (i = 0; i < f->num; i++) {
			list_add(&((struct mem *)f->mem[i])->list,
							&free_list.list);
		}
		kfree(f->pa);
		kfree(f->mem);
		list_del(pos);
		kfree(f);
	}
	mutex_unlock(&mtx);
}

static u32 fill_page_stack(struct mem *mem)
{
	s32 i = 0;
	struct mem *m = NULL;

	for (i = 0; i < MAX; i++) {
		m = kmalloc(sizeof(struct mem), GFP_KERNEL);
		if (!m)
			return -ENOMEM;
		memset(m, 0x0, sizeof(struct mem));

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

		mutex_lock(&mtx);
		list_add(&m->list, &mem->list);
		mutex_unlock(&mtx);
	}
	return 0x0;
}

static void dmm_free_page_stack(struct mem *mem)
{
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;

	mutex_lock(&mtx);
	list_for_each_safe(pos, q, &mem->list) {
		m = list_entry(pos, struct mem, list);
		__free_page(m->pg);
		list_del(pos);
		kfree(m);
	}
	mutex_unlock(&mtx);
}

u32 dmm_get_page(void)
{
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;

	if (list_empty_careful(&free_list.list))
		if (fill_page_stack(&free_list))
			return 0;

	mutex_lock(&mtx);
	pos = NULL;
	q = NULL;
	list_for_each_safe(pos, q, &free_list.list) {
		m = list_entry(pos, struct mem, list);
		list_move(&m->list, &used_list.list);
		break;
	}
	mutex_unlock(&mtx);

	return m->pa;
}
EXPORT_SYMBOL(dmm_get_page);

void dmm_free_page(u32 page_addr)
{
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;

	mutex_lock(&mtx);
	pos = NULL;
	q = NULL;
	list_for_each_safe(pos, q, &used_list.list) {
		m = list_entry(pos, struct mem, list);
		if (m->pa == page_addr) {
			list_move(&m->list, &free_list.list);
			break;
		}
	}
	mutex_unlock(&mtx);
}
EXPORT_SYMBOL(dmm_free_page);

u32 dmm_init_mem()
{
	INIT_LIST_HEAD(&free_list.list);
	INIT_LIST_HEAD(&used_list.list);
	INIT_LIST_HEAD(&fast_list.list);
	mutex_init(&mtx);

	if (list_empty_careful(&free_list.list))
		if (fill_page_stack(&free_list))
			return -ENOMEM;

	return 0;
}

void dmm_release_mem()
{
	dmm_free_fast_list(&fast_list);
	dmm_free_page_stack(&free_list);
	dmm_free_page_stack(&used_list);
	mutex_destroy(&mtx);
}

u32 *dmm_get_pages(s32 n)
{
	s32 i = 0;
	struct list_head *pos = NULL, *q = NULL;
	struct mem *m = NULL;
	struct fast *f = NULL;

	if (n <= 0 || n > 0x8000)
		return NULL;

	if (list_empty_careful(&free_list.list))
		if (fill_page_stack(&free_list))
			return NULL;

	f = kmalloc(sizeof(struct fast), GFP_KERNEL);
	if (!f)
		return NULL;
	memset(f, 0x0, sizeof(struct fast));

	/* array of mem struct pointers */
	f->mem = kmalloc(n * 4, GFP_KERNEL);
	if (!f->mem) {
		kfree(f); return NULL;
	}
	memset(f->mem, 0x0, n * 4);

	/* array of physical addresses */
	f->pa = kmalloc(n * 4, GFP_KERNEL);
	if (!f->pa) {
		kfree(f->mem); kfree(f); return NULL;
	}
	memset(f->pa, 0x0, n * 4);

	/*
	 * store the number of mem structs so that we
	 * know how many to free later.
	 */
	f->num = n;

	for (i = 0; i < n; i++) {
		if (list_empty_careful(&free_list.list))
			if (fill_page_stack(&free_list))
				goto cleanup;

		mutex_lock(&mtx);
		pos = NULL;
		q = NULL;

		/*
		 * remove one mem struct from the free list and
		 * add the address to the fast struct mem array
		 */
		list_for_each_safe(pos, q, &free_list.list) {
			m = list_entry(pos, struct mem, list);
			f->mem[i] = (u32)m;
			list_del(pos);
			break;
		}
		mutex_unlock(&mtx);

		f->pa[i] = m->pa;
	}

	mutex_lock(&mtx);
	list_add(&f->list, &fast_list.list);
	mutex_unlock(&mtx);

	return f->pa;

cleanup:
	for (; i > 0; i--) {
		mutex_lock(&mtx);
		list_add(&((struct mem *)f->mem[i - 1])->list, &free_list.list);
		mutex_unlock(&mtx);
	}
	kfree(f->pa);
	kfree(f->mem);
	kfree(f);
	return NULL;
}
EXPORT_SYMBOL(dmm_get_pages);

void dmm_free_pages(u32 *list)
{
	struct list_head *pos = NULL, *q = NULL;
	struct fast *f = NULL;
	s32 i = 0;

	mutex_lock(&mtx);
	pos = NULL;
	q = NULL;
	list_for_each_safe(pos, q, &fast_list.list) {
		f = list_entry(pos, struct fast, list);
		if (f->pa[0] == list[0]) {
			for (i = 0; i < f->num; i++) {
				list_add(&((struct mem *)f->mem[i])->list,
							&free_list.list);
			}
			list_del(pos);
			kfree(f->pa);
			kfree(f->mem);
			kfree(f);
			break;
		}
	}
	mutex_unlock(&mtx);
}
EXPORT_SYMBOL(dmm_free_pages);

