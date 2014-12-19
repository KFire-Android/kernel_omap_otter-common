/*
 * cache-2dmanager.c
 *
 * Copyright (C) 2011-2012 Texas Instruments Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>
#include <linux/sched.h>
#include <linux/cache-2dmanager.h>

static void per_cpu_cache_flush_arm(void *arg)
{
	flush_cache_all();
}

void c2dm_l1cache(int count,		/* number of regions */
		struct c2dmrgn rgns[],	/* array of regions */
		int dir)		/* cache operation */
{
	unsigned long size = 0;
	int rgn;
	for (rgn = 0; rgn < count; rgn++)
		size += rgns[rgn].span * rgns[rgn].lines;

	/* If the total size of the caller's request exceeds the threshold,
	 * we can perform the operation on the entire cache instead.
	 *
	 * If the caller requests a clean larger than the threshold, we want
	 * to clean all.  But this function does not exist in the L1 cache
	 * routines. So we use flush all.
	 *
	 * If the caller requests an invalidate larger than the threshold, we
	 * want to invalidate all. However, if the client does not fill the
	 * cache, an invalidate all will lose data from other processes, which
	 * can be catastrophic.  So we must clean the entire cache before we
	 * invalidate it. Flush all cleans and invalidates in one operation.
	 */
	if (size >= L1THRESHOLD) {
		switch (dir) {
		case DMA_TO_DEVICE:
			/* Use clean all when available */
			/* Fall through for now */
		case DMA_BIDIRECTIONAL:
			/* Can't invalidate all without cleaning, so fall
			 * through to flush all to do both. */
		case DMA_FROM_DEVICE:
			on_each_cpu(per_cpu_cache_flush_arm, NULL, 1);
			break;
		}
	} else {
		int rgn;
		for (rgn = 0; rgn < count; rgn++) {
			int line;
			char *start = rgns[rgn].start;
			for (line = 0; line < rgns[rgn].lines; line++) {
				if (dir == DMA_BIDIRECTIONAL)
					cpu_cache.dma_flush_range(
						start,
						start + rgns[rgn].span);
				else
					cpu_cache.dma_map_area(
						start,
						rgns[rgn].span,
						dir);
				start += rgns[rgn].stride;
			}
		}
	}
}
EXPORT_SYMBOL(c2dm_l1cache);

static u32 virt2phys(u32 usr)
{
	pmd_t *pmd;
	pte_t *ptep;
	pgd_t *pgd = pgd_offset(current->mm, usr);

	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return 0;

	pmd = pmd_offset(pgd, usr);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return 0;

	ptep = pte_offset_map(pmd, usr);
	if (ptep && pte_present(*ptep))
		return (*ptep & PAGE_MASK) | (~PAGE_MASK & usr);

	return 0;
}

void c2dm_l2cache(int count,		/* number of regions */
		struct c2dmrgn rgns[],	/* array of regions */
		int dir)		/* cache operation */
{

	unsigned long size = 0;
	int rgn;

	for (rgn = 0; rgn < count; rgn++)
		size += rgns[rgn].span * rgns[rgn].lines;

	if (size >= L2THRESHOLD) {
		switch (dir) {
		case DMA_TO_DEVICE:
			/* Use clean all when available */
			/* Fall through for now */
		case DMA_BIDIRECTIONAL:
			/* Can't invalidate all without cleaning, so fall
			 * through to flush all to do both. */
		case DMA_FROM_DEVICE:
			outer_flush_all();
			break;
		}
		return;
	}

	for (rgn = 0; rgn < count; rgn++) {
		int i, j;
		unsigned long linestart, start;
		unsigned long page_begin, end, offset,
			pageremain, lineremain;
		unsigned long phys, opsize;
		int page_num;

		/* beginning virtual address of each line */
		start = (unsigned long)rgns[rgn].start;

		for (i = 0; i < rgns[rgn].lines; i++) {

			linestart = start + (i * rgns[rgn].stride);

			/* beginning of the page for the new line */
			page_begin = linestart & PAGE_MASK;

			/* end of the new line */
			end = (unsigned long)linestart +
				rgns[rgn].span;

			page_num = DIV_ROUND_UP(
				end-page_begin, PAGE_SIZE);

			/* offset of the new line from page begin */
			offset = linestart - page_begin;

			/* track how long it is to the end of
			   the current page */
			pageremain = PAGE_SIZE - offset;

			/* keep track of how much of the line remains
			   to be copied */
			lineremain = rgns[rgn].span;

			for (j = 0; j < page_num; j++) {

				opsize = (lineremain < pageremain) ?
					lineremain : pageremain;

				phys = virt2phys(page_begin);
				if (phys) {
					phys = phys + offset;
					switch (dir) {
					case DMA_TO_DEVICE:
						outer_clean_range(
							phys, phys + opsize);
						break;
					case DMA_FROM_DEVICE:
						outer_inv_range(
							phys, phys + opsize);
						break;
					case DMA_BIDIRECTIONAL:
						outer_flush_range(
							phys, phys + opsize);
						break;
					}
				}

				lineremain -= opsize;
				/* Move to next page */
				page_begin += PAGE_SIZE;

				/* After first page, start address
				 * will be page aligned so offset
				 * is 0 */
				offset = 0;

				if (!lineremain)
					break;

				pageremain -= opsize;
				if (!pageremain)
					pageremain = PAGE_SIZE;

			}
		}
	}
}
EXPORT_SYMBOL(c2dm_l2cache);
