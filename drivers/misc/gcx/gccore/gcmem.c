/*
 * gcmain.c
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/pagemap.h>
#include <linux/dma-mapping.h>
#include "gcmain.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_CACHED		(1 << 0)
#define GCZONE_NONCACHED	(1 << 1)
#define GCZONE_FLUSH		(1 << 2)

GCDBG_FILTERDEF(mem, GCZONE_NONE,
		"cached",
		"noncached",
		"flush")

enum gcerror gc_alloc_noncached(struct gcpage *p, unsigned int size)
{
	enum gcerror gcerror;

	GCENTERARG(GCZONE_NONCACHED, "p = 0x%08X\n", (unsigned int) p);

	p->pages = NULL;
	p->order = 0;
	p->size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	p->logical = NULL;
	p->physical = ~0UL;

	GCDBG(GCZONE_NONCACHED, "requested size=%d\n", size);
	GCDBG(GCZONE_NONCACHED, "aligned size=%d\n", p->size);

	p->logical = dma_alloc_coherent(NULL, p->size, &p->physical,
						GFP_KERNEL);
	if (!p->logical) {
		GCERR("failed to allocate memory\n");
		gcerror = GCERR_OOPM;
		goto exit;
	}

	GCDBG(GCZONE_NONCACHED, "logical=0x%08X\n",
		(unsigned int) p->logical);
	GCDBG(GCZONE_NONCACHED, "physical=0x%08X\n",
		(unsigned int) p->physical);

	GCEXIT(GCZONE_NONCACHED);
	return GCERR_NONE;

exit:
	gc_free_noncached(p);

	GCEXITARG(GCZONE_NONCACHED, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

void gc_free_noncached(struct gcpage *p)
{
	GCENTERARG(GCZONE_NONCACHED, "p = 0x%08X\n", (unsigned int) p);

	if (p->logical != NULL) {
		dma_free_coherent(NULL, p->size, p->logical, p->physical);
		p->logical = NULL;
	}

	p->physical = ~0UL;
	p->size = 0;

	GCEXIT(GCZONE_NONCACHED);
}

enum gcerror gc_alloc_cached(struct gcpage *p, unsigned int size)
{
	enum gcerror gcerror;
	struct page *pages;
	int count;

	GCENTERARG(GCZONE_CACHED, "p = 0x%08X\n", (unsigned int) p);

	p->order = get_order(size);
	p->pages = NULL;
	p->size = (1 << p->order) * PAGE_SIZE;
	p->logical = NULL;
	p->physical = ~0UL;

	GCDBG(GCZONE_CACHED, "requested size=%d\n", size);
	GCDBG(GCZONE_CACHED, "aligned size=%d\n", p->size);

	p->pages = alloc_pages(GFP_KERNEL, p->order);
	if (p->pages == NULL) {
		GCERR("failed to allocate memory\n");
		gcerror = GCERR_OOPM;
		goto exit;
	}

	p->physical = page_to_phys(p->pages);
	p->logical = (unsigned int *) page_address(p->pages);

	if (p->logical == NULL) {
		GCERR("failed to retrieve page virtual address\n");
		gcerror = GCERR_PMMAP;
		goto exit;
	}

	/* Reserve pages. */
	pages = p->pages;
	count = p->size / PAGE_SIZE;

	while (count--)
		SetPageReserved(pages++);

	GCDBG(GCZONE_CACHED, "page array=0x%08X\n", (unsigned int) p->pages);
	GCDBG(GCZONE_CACHED, "logical=0x%08X\n", (unsigned int) p->logical);
	GCDBG(GCZONE_CACHED, "physical=0x%08X\n", (unsigned int) p->physical);

	GCEXIT(GCZONE_CACHED);
	return GCERR_NONE;

exit:
	gc_free_cached(p);

	GCEXITARG(GCZONE_CACHED, "gcerror = 0x%08X\n", gcerror);
	return gcerror;
}

void gc_free_cached(struct gcpage *p)
{
	GCENTERARG(GCZONE_CACHED, "p = 0x%08X\n", (unsigned int) p);

	GCDBG(GCZONE_CACHED, "page array=0x%08X\n", (unsigned int) p->pages);
	GCDBG(GCZONE_CACHED, "logical=0x%08X\n", (unsigned int) p->logical);
	GCDBG(GCZONE_CACHED, "physical=0x%08X\n", (unsigned int) p->physical);
	GCDBG(GCZONE_CACHED, "size=%d\n", p->size);

	if (p->logical != NULL) {
		struct page *pages;
		int count;

		pages = p->pages;
		count = p->size / PAGE_SIZE;

		while (count--)
			ClearPageReserved(pages++);

		p->logical = NULL;
	}

	if (p->pages != NULL) {
		__free_pages(p->pages, p->order);
		p->pages = NULL;
	}

	p->physical = ~0UL;
	p->order = 0;
	p->size = 0;

	GCEXIT(GCZONE_CACHED);
}

void gc_flush_cached(struct gcpage *p)
{
	GCENTERARG(GCZONE_FLUSH, "p = 0x%08X\n", (unsigned int) p);

	dmac_flush_range(p->logical, (unsigned char *) p->logical + p->size);
	outer_flush_range(p->physical, p->physical + p->size);

	GCEXIT(GCZONE_FLUSH);
}

void gc_flush_region(unsigned int physical, void *logical,
			unsigned int offset, unsigned int size)
{
	unsigned char *startlog;
	unsigned int startphys;

	GCENTER(GCZONE_FLUSH);

	GCDBG(GCZONE_FLUSH, "logical=0x%08X\n", (unsigned int) logical);
	GCDBG(GCZONE_FLUSH, "physical=0x%08X\n", physical);
	GCDBG(GCZONE_FLUSH, "offset=%d\n", offset);
	GCDBG(GCZONE_FLUSH, "size=%d\n", size);

	startlog = (unsigned char *) logical + offset;
	startphys = physical + offset;

	dmac_flush_range(startlog, startlog + size);
	outer_flush_range(startphys, startphys + size);

	GCEXIT(GCZONE_FLUSH);
}
