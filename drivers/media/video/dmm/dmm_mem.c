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

#define MAX 16
#define DMM_PAGE 0x1000

struct mem {
	struct list_head list;
	struct page *pg;
	u32 pa;
};

static struct mem free_list;
static struct mem used_list;
static struct mutex mtx;

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
	mutex_init(&mtx);

	if (list_empty_careful(&free_list.list))
		if (fill_page_stack(&free_list))
			return -ENOMEM;

	return 0;
}

void dmm_release_mem()
{
	dmm_free_page_stack(&free_list);
	dmm_free_page_stack(&used_list);
	mutex_destroy(&mtx);
}

