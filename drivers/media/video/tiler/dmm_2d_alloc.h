/*
 * dmm_2d_alloc.h
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

#ifndef _DMM_2D_ALLOC_H
#define _DMM_2D_ALLOC_H

#include <linux/dma-mapping.h>

enum MSP_BOOL {
	MSP_FALSE = 0,
	MSP_TRUE
};

enum SideAffinity {
	PSA_NONE,
	PSA_LEFT,
	PSA_TOP,
	PSA_BOTTOM,
	PSA_RIGHT
};

struct dmmTILERContPageAreaT {
	unsigned short x0;
	unsigned short y0;
	unsigned short x1;
	unsigned short y1;
	unsigned short xPageOfst;
	unsigned short yPageOfst;
	unsigned short xPageCount;
	unsigned short yPageCount;
	enum SideAffinity fitToSide;
	int patCustomPages;
	unsigned long *patPageEntriesSpace;
	struct page *page_list;
	unsigned long *page_list_virt;
	void *dma_va;
	unsigned long dma_size;
	dma_addr_t dma_pa;
	unsigned long *patPageEntries;
};

struct dmmTILERContPageAreaSpecT {
	struct dmmTILERContPageAreaT ttlExpndAr;
	struct dmmTILERContPageAreaT plmntAr;
	struct dmmTILERContPageAreaT *anchrAr;
};

struct dmmTILERContPageLstT {
	struct dmmTILERContPageLstT *pgArNext;
	struct dmmTILERContPageAreaT *anchrAr;
	struct dmmTILERContPageAreaT pgAr;
};

struct dmmTILERContCtxT {
	signed long contSizeX;
	signed long contSizeY;
	struct dmmTILERContPageLstT *usdArList;
	struct dmmTILERContPageAreaSpecT tmpArSelect;
	struct mutex mtx;
};

/* ========================================================================== */
/**
 *  alloc2DArea()
 *
 * @brief  Allocates a 2D area and returns a pointer to it.
 *
 * @param tlrCtx - dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param areaReq - dmmTILERContPageAreaT* - [in] Required area for allcoation.
 *
 * @return dmmTILERContPageAreaT*: pointer to the allocated area.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
struct dmmTILERContPageAreaT *alloc_2d_area(
	struct dmmTILERContCtxT *dmmTilerCtx,
	struct dmmTILERContPageAreaT *areaReq);

/* ========================================================================== */
/**
 *  deAlloc2DArea()
 *
 * @brief  Deletes a 2D area from the TILER context.
 *
 * @param tlrCtx - dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param areaRem - dmmTILERContPageAreaT* - [in] Area to remove.
 *
 * @return int: True if the specified area is successfuly deleted.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL dealloc_2d_area(struct dmmTILERContCtxT *dmmTilerCtx,
			 struct dmmTILERContPageAreaT *areaRem);

/* ========================================================================== */
/**
 *  search2DArea()
 *
 * @brief  Deletes a 2D area from the TILER context.
 *
 * @param tlrCtx - dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X coordinate of the search area.
 *
 * @param Y - signed long - [in] X coordinate of the search area.
 *
 * @param xInvert - int - [in] X coordinate is inverted.
 *
 * @param yInvert - int - [in] Y coordinate is inverted.
 *
 * @return dmmTILERContPageAreaT*: pointer to the discovered 2D area
 * or NULL if no such area is found.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
struct dmmTILERContPageAreaT *search_2d_area(
	struct dmmTILERContCtxT *dmmTilerCtx,
	signed long X,
	signed long Y,
	enum MSP_BOOL xInvert,
	enum MSP_BOOL yInvert);

#endif /* _DMM_2D_ALLOC_H */

