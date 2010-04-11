/*
 * dmm_mem.h
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
#ifndef DMM_MEM_H
#define DMM_MEM_H

#include <mach/dmm.h>
/**
 * TMM interface
 */
struct tmm {
	void *pvt;

	/* function table */
	u32 *(*get)    (struct tmm *tmm, s32 num_pages);
	void (*free)   (struct tmm *tmm, u32 *pages);
	s32  (*map)    (struct tmm *tmm, struct pat_area area, u32 page_pa);
	void (*deinit) (struct tmm *tmm);
};

/**
 * Request a set of pages from the DMM free page stack.
 * @return a pointer to a list of physical page addresses.
 */
static inline
u32 *tmm_get(struct tmm *tmm, s32 num_pages)
{
	if (tmm && tmm->pvt)
		return tmm->get(tmm, num_pages);
	return NULL;
}

/**
 * Return a set of used pages to the DMM free page stack.
 * @param list a pointer to a list of physical page addresses.
 */
static inline
void tmm_free(struct tmm *tmm, u32 *pages)
{
	if (tmm && tmm->pvt)
		tmm->free(tmm, pages);
}

/**
 * Program the physical address translator.
 * @param area PAT area
 * @param list of pages
 */
static inline
s32 tmm_map(struct tmm *tmm, struct pat_area area, u32 page_pa)
{
	if (tmm && tmm->map && tmm->pvt)
		return tmm->map(tmm, area, page_pa);
	return -ENODEV;
}

/**
 * Checks whether tiler memory manager supports mapping
 */
static inline
bool tmm_can_map(struct tmm *tmm)
{
	return tmm && tmm->map;
}

/**
 * Deinitialize tiler memory manager
 */
static inline
void tmm_deinit(struct tmm *tmm)
{
	if (tmm && tmm->pvt)
		tmm->deinit(tmm);
}

/**
 * Initialize TMM for PAT with given id.
 */
struct tmm *tmm_pat_init(u32 pat_id);

#endif
