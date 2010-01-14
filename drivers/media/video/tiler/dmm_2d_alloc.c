/*
 * dmm_2d_alloc.c
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

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/hardirq.h>
#include <linux/mutex.h>

#include "dmm_2d_alloc.h"
#include "tiler_def.h"
#include "../dmm/dmm_page_rep.h"

#define DMM_ASSERT_BREAK printk(KERN_ERR "DMM Assert(Fail)\n"); while (1);

/* ========================================================================== */
/**
 *  overlapping_test()
 *
 * @brief  Performs an area overlap test for errors. Debug only.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @return MSP_BOOL: MSP_TRUE if overlapping is detected.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
void overlapping_test(struct dmmTILERContCtxT *dmmTilerCtx)
{
	struct dmmTILERContPageLstT *itr1 = dmmTilerCtx->usdArList;

	while (itr1 != NULL) {
		struct dmmTILERContPageAreaT *chkPage = &(itr1->pgAr);
		struct dmmTILERContPageLstT *itr2 = dmmTilerCtx->usdArList;

		while (itr2 != NULL) {
			if (chkPage != &(itr2->pgAr)) {
				if ((itr2->pgAr.x0 <= chkPage->x1 &&
					itr2->pgAr.x1 >= chkPage->x0) &&
						(itr2->pgAr.y0 <= chkPage->y1 &&
						itr2->pgAr.y1 >= chkPage->y0)) {
					DMM_ASSERT_BREAK
				}
			}

			itr2 = itr2->pgArNext;
		}

		itr1 = itr1->pgArNext;
	}
	return;
}

/* ========================================================================== */
/**
 *  point_free_test()
 *
 * @brief  Tests if a page on a given coordinate is unoccupied.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param areaHit - struct dmmTILERContPageAreaT** - [out] Returns a pointer to
 *  the allocated area that was hit by the specified coordinate.
 *
 * @return MSP_BOOL: MSP_TRUE if the selected coordinate page is unoccupied.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline enum MSP_BOOL point_free_test(struct dmmTILERContCtxT *tlrCtx,
				signed long X,
				signed long Y,
				struct dmmTILERContPageAreaT **areaHit)
{
	struct dmmTILERContPageLstT *arUsd = tlrCtx->usdArList;
	*areaHit = NULL;
	while (arUsd != NULL) {
		if (arUsd->pgAr.x0 <= X && arUsd->pgAr.x1 >= X &&
				arUsd->pgAr.y0 <= Y && arUsd->pgAr.y1 >= Y) {
			*areaHit = &(arUsd->pgAr);
			return MSP_FALSE;
		}

		arUsd = arUsd->pgArNext;
	}
	return MSP_TRUE;
}

/* ========================================================================== */
/**
 *  pointAreaHit()
 *
 * @brief  Tests if a given coordinate is within a given area.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param areaHit - struct dmmTILERContPageAreaT* - [in] Given area to check if
 * contains the given coordinate point.
 *
 * @return MSP_BOOL: MSP_TRUE if the given coordinate is contained in the
 * given area.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
/*
inline enum MSP_BOOL pointAreaHit(signed long X,
				signed long Y,
				struct dmmTILERContPageAreaT* area)
{
    if ((area->x0 <= X && area->x1 >= X) && (area->y0 <= Y && area->y1 >= Y))
    {
	return MSP_TRUE;
    }
    return MSP_FALSE;
}
*/

/* ========================================================================== */
/**
 *  zone_area_overlap()
 *
 * @brief  Tests if two areas overlap.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param SizeX - signed long - [in] Page width.
 *
 * @param SizeY - signed long - [in] Page height.
 *
 * @param areaHit - struct dmmTILERContPageAreaT* - [in] Given area to check.
 *
 * @return MSP_BOOL: MSP_TRUE if the given areas overlap.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline enum MSP_BOOL zone_area_overlap(signed long X,
					signed long Y,
					signed long SizeX,
					signed long SizeY,
					struct dmmTILERContPageAreaT *area)
{
	if ((area->x0 <= X + SizeX && area->x1 >= X) &&
			(area->y0 <= Y + SizeY && area->y1 >= Y)) {
		return MSP_TRUE;
	}

	return MSP_FALSE;
}

/* ========================================================================== */
/**
 *  lineAreaHit()
 *
 * @brief  Tests if a page line overlaps with an area. Either width or height
 * can be used to perform checks in both coordinates.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param SizeX - signed long - [in] Page width.
 *
 * @param SizeY - signed long - [in] Page height.
 *
 * @param areaHit - struct dmmTILERContPageAreaT* - [in] Given area to check.
 *
 * @return MSP_BOOL: MSP_TRUE if the given areas overlap.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
/*
inline enum MSP_BOOL lineAreaHit(signed long X,
				signed long Y,
				signed long SizeX,
				signed long SizeY,
				struct dmmTILERContPageAreaT* area)
{
    if (((area->x0 <= X && area->x1 >= X) ||
    (area->x0 <= X + SizeX && area->x1 >= X + SizeX)) &&
	((area->y0 <= Y && area->y1 >= Y) ||
	(area->y0 <= Y + SizeY && area->y1 >= Y + SizeY)))
    {
	return MSP_TRUE;
    }

    return MSP_FALSE;
}
*/

/* ========================================================================== */
/**
 *  expand_right()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line (1D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param areaHit - struct dmmTILERContPageAreaT** - [out] Returns a pointer to
 * the allocated area that was hit during the "expansion" along the specified
 * coordinate line.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_right(struct dmmTILERContCtxT *tlrCtx,
				signed long X,
				signed long Y,
				struct dmmTILERContPageAreaT **areaHit)
{
	*areaHit = NULL;
	while (X < tlrCtx->contSizeX) {
		if (MSP_FALSE == point_free_test(tlrCtx, X, Y, areaHit))
			return X - 1;
		X++;
	}

	return X - 1;
}

/* ========================================================================== */
/**
 *  expand_left()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line (1D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param areaHit - struct dmmTILERContPageAreaT** - [out] Returns a pointer to
 * the allocated area that was hit during the "expansion" along the specified
 * coordinate line.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_left(struct dmmTILERContCtxT *tlrCtx,
				signed long X,
				signed long Y,
				struct dmmTILERContPageAreaT **areaHit)
{
	*areaHit = NULL;
	while (X >= 0) {
		if (MSP_FALSE == point_free_test(tlrCtx, X, Y, areaHit))
			return X + 1;
		X--;
	}

	return X + 1;
}

/* ========================================================================== */
/**
 *  expand_bottom()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line (1D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param areaHit - struct dmmTILERContPageAreaT** - [out] Returns a pointer to
 * the allocated area that was hit during the "expansion" along the specified
 * coordinate line.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_bottom(struct dmmTILERContCtxT *tlrCtx,
				signed long X,
				signed long Y,
				struct dmmTILERContPageAreaT **areaHit)
{
	*areaHit = NULL;
	while (Y < tlrCtx->contSizeY) {
		if (MSP_FALSE == point_free_test(tlrCtx, X, Y, areaHit))
			return Y - 1;
		Y++;
	}

	return Y - 1;
}

/* ========================================================================== */
/**
 *  expand_top()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line (1D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param areaHit - struct dmmTILERContPageAreaT** - [out] Returns a pointer to
 * the allocated area that was hit during the "expansion" along the specified
 * coordinate line.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_top(struct dmmTILERContCtxT *tlrCtx,
				signed long X,
				signed long Y,
				struct dmmTILERContPageAreaT **areaHit)
{
	*areaHit = NULL;
	while (Y >= 0) {
		if (MSP_FALSE == point_free_test(tlrCtx, X, Y, areaHit))
			return Y + 1;
		Y--;
	}

	return Y + 1;
}

/* ========================================================================== */
/**
 *  expand_line_on_bottom_to_right()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeY - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_line_on_bottom_to_right(
						struct dmmTILERContCtxT *tlrCtx,
						signed long X,
						signed long Y,
						signed long elSizeY)
{
	struct dmmTILERContPageLstT *iter;
	int elSizeX = 0;

	while (elSizeX < tlrCtx->contSizeX) {
		iter = tlrCtx->usdArList;
		while (iter != NULL) {
			if (1 == zone_area_overlap(X, Y, elSizeX,
						elSizeY, &(iter->pgAr))) {
				return X + elSizeX - 1;
			}
			iter = iter->pgArNext;
		}
		elSizeX++;
	}

	return tlrCtx->contSizeX - 1;
}

/* ========================================================================== */
/**
 *  expand_line_on_bottom_to_left()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeY - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_line_on_bottom_to_left(
						struct dmmTILERContCtxT *tlrCtx,
						signed long X,
						signed long Y,
						signed long elSizeY)
{
	struct dmmTILERContPageLstT *iter;
	int elSizeX = 0;

	while (X >= 0) {
		iter = tlrCtx->usdArList;
		while (iter != NULL) {
			if (1 == zone_area_overlap(X - elSizeX,
				Y, elSizeX, elSizeY, &(iter->pgAr))) {
				return X + 1;
			}
			iter = iter->pgArNext;
		}
		X--;
		elSizeX++;
	}

	return 0;
}

/* ========================================================================== */
/**
 *  expandLineOnTopToRight()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeY - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
/*
inline signed long expandLineOnTopToRight(struct dmmTILERContCtxT* tlrCtx,
					signed long X, signed long Y,
					signed long elSizeY)
{
    struct dmmTILERContPageLstT* iter;
    int elSizeX = 0;

    while (elSizeX < tlrCtx->contSizeX)
    {
	iter = tlrCtx->usdArList;
	while (iter != NULL)
	{
		if (1 == zone_area_overlap(X, Y, elSizeX, elSizeY,
								&(iter->pgAr)))
			return X + elSizeX - 1;
		iter = iter->pgArNext;
	}
	elSizeX++;
    }

    return tlrCtx->contSizeX - 1;
}
*/

/* ========================================================================== */
/**
 *  expandLineOnTopToLeft()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeY - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
/*
inline signed long expandLineOnTopToLeft(struct dmmTILERContCtxT* tlrCtx,
					signed long X, signed long Y,
					signed long elSizeY)
{
	struct dmmTILERContPageLstT* iter;
	int elSizeX = 0;

	while (X >= 0)
	{
		iter = tlrCtx->usdArList;
		while (iter != NULL)
		{
			if (1 == zone_area_overlap(X - elSizeX,
				Y - elSizeY, elSizeX, elSizeY,
				&(iter->pgAr)))
				return X + 1;
			iter = iter->pgArNext;
		}
		X--;
		elSizeX++;
	}
	return 0;
}
*/

/* ========================================================================== */
/**
 *  expand_line_on_right_to_bottom()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeX - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_line_on_right_to_bottom(
						struct dmmTILERContCtxT *tlrCtx,
						signed long X,
						signed long Y,
						signed long elSizeX)
{
	struct dmmTILERContPageLstT *iter;
	int elSizeY = 0;

	while (elSizeY < tlrCtx->contSizeY) {
		iter = tlrCtx->usdArList;
		while (iter != NULL) {
			if (1 == zone_area_overlap(
				X, Y, elSizeX, elSizeY, &(iter->pgAr))) {
				return Y + elSizeY - 1;
			}
			iter = iter->pgArNext;
		}
		elSizeY++;
	}

	return tlrCtx->contSizeY - 1;
}

/* ========================================================================== */
/**
 *  expand_line_on_right_to_top()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeX - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
inline signed long expand_line_on_right_to_top(struct dmmTILERContCtxT *tlrCtx,
						signed long X, signed long Y,
						signed long elSizeX)
{
	struct dmmTILERContPageLstT *iter;
	int elSizeY = 0;

	while (Y >= 0) {
		iter = tlrCtx->usdArList;
		while (iter != NULL) {
			if (1 == zone_area_overlap(X, Y - elSizeY,
					elSizeX, elSizeY, &(iter->pgAr))) {
				return Y + 1;
			}
			iter = iter->pgArNext;
		}
		Y--;
		elSizeY++;
	}

	return 0;
}

/* ========================================================================== */
/**
 *  expandLineOnLeftToBottom()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeX - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
/*
inline signed long expandLineOnLeftToBottom(struct dmmTILERContCtxT* tlrCtx,
					signed long X, signed long Y,
					signed long elSizeX)
{
	struct dmmTILERContPageLstT* iter;
	int elSizeY = 0;

	while (elSizeY < tlrCtx->contSizeY)
	{
		iter = tlrCtx->usdArList;
		while (iter != NULL)
		{
		if (1 == zone_area_overlap(X, Y, elSizeX, elSizeY,
								&(iter->pgAr)))
			return Y + elSizeY - 1;
		iter = iter->pgArNext;
	}
	elSizeY++;
	}
	return tlrCtx->contSizeY - 1;
}
*/

/* ========================================================================== */
/**
 *  expandLineOnLeftToTop()
 *
 * @brief  Finds all of the unocupied pages ("expansion") along the specified
 * coordinate line area (2D check).
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X page coordinate.
 *
 * @param Y - signed long - [in] Y page coordinate.
 *
 * @param elSizeX - signed long - [in] Size of the area to check.
 *
 * @return signed long the calculated coordinate point.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
/*
inline signed long expandLineOnLeftToTop(struct dmmTILERContCtxT* tlrCtx,
					signed long X, signed long Y,
					signed long elSizeX)
{
	struct dmmTILERContPageLstT* iter;
	int elSizeY = 0;

	while (Y >= 0)
	{
		iter = tlrCtx->usdArList;
		while (iter != NULL)
		{
			if (1 == zone_area_overlap(X - elSizeX, Y - elSizeY,
					elSizeX, elSizeY, &(iter->pgAr)))
				return Y + 1;
			iter = iter->pgArNext;
		}
		Y--;
		elSizeY++;
	}
	return 0;
}
*/

/* ========================================================================== */
/**
 *  area_required_to_allocated()
 *
 * @brief  Given the required area to be allcoted, total area present and anchor
 * points along a side of attachemnt, calculates the actual position of the new
 * new 2D area that is allocated.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required 2D area to be
 * allocated.
 *
 * @param areaTotal - struct dmmTILERContPageAreaT* - [in] Total 2D area
 * allocated for this area attachment.
 *
 * @param areaAlloc - struct dmmTILERContPageAreaT* - [out] The actual
 * allocated area.
 *
 * @param anchorX - signed long - [in] X page coordinate anchor point.
 *
 * @param anchorY - signed long - [in] Y page coordinate anchor point.
 *
 * @param side - enum SideAffinity - [in] Side of attachment.
 *
 * @return none
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
void area_required_to_allocated(struct dmmTILERContPageAreaT *areaReq,
				struct dmmTILERContPageAreaT *areaTotal,
				struct dmmTILERContPageAreaT *areaAlloc,
				signed long anchorX,
				signed long anchorY,
				enum SideAffinity side)
{
	switch (side) {
	case PSA_BOTTOM:
		if (areaTotal->x1 - anchorX < areaReq->x1) {
			if (areaTotal->x1 - areaTotal->x0 >= areaReq->x1) {
				areaAlloc->x0 =
				(unsigned short)(areaTotal->x1 - areaReq->x1);
				areaAlloc->x1 =
				(unsigned short)(areaAlloc->x0 + areaReq->x1);
			} else {
				areaAlloc->x0 = areaTotal->x0;
				areaAlloc->x1 = areaTotal->x1;
			}
		} else {
			areaAlloc->x0 = (unsigned short)(anchorX);
			areaAlloc->x1 = (unsigned short)(anchorX + areaReq->x1);
		}

		areaAlloc->y0 = (unsigned short)(anchorY);
		if (areaAlloc->y0 + areaReq->y1 <= areaTotal->y1) {
			areaAlloc->y1 = (unsigned short)
						(areaAlloc->y0 + areaReq->y1);
		} else {
			areaAlloc->y1 = areaTotal->y1;
		}
		areaAlloc->fitToSide = PSA_BOTTOM;
		break;

	case PSA_TOP:
		if (areaTotal->x1 - anchorX < areaReq->x1) {
			if (areaTotal->x1 - areaTotal->x0 >= areaReq->x1) {
				areaAlloc->x0 =
					(unsigned short)
					(areaTotal->x1 - areaReq->x1);
				areaAlloc->x1 =
					(unsigned short)
					(areaAlloc->x0 + areaReq->x1);
			} else {
				areaAlloc->x0 = areaTotal->x0;
				areaAlloc->x1 = areaTotal->x1;
			}
		} else {
			areaAlloc->x0 = (unsigned short)(anchorX);
			areaAlloc->x1 = (unsigned short)(anchorX + areaReq->x1);
		}

		areaAlloc->y1 = (unsigned short)(anchorY);
		if (areaAlloc->y1 - areaReq->y1 >= areaTotal->y0) {
			areaAlloc->y0 = (unsigned short)
						(areaAlloc->y1 - areaReq->y1);
		} else {
			areaAlloc->y0 = areaTotal->y0;
		}
		areaAlloc->fitToSide = PSA_TOP;
		break;

	case PSA_RIGHT:
		if (areaTotal->y1 - anchorY < areaReq->y1) {
			if (areaTotal->y1 - areaTotal->y0 >= areaReq->y1) {
				areaAlloc->y0 =
					(unsigned short)
					(areaTotal->y1 - areaReq->y1);
				areaAlloc->y1 =
					(unsigned short)
					(areaAlloc->y0 + areaReq->y1);
			} else {
				areaAlloc->y0 = areaTotal->y0;
				areaAlloc->y1 = areaTotal->y1;
			}
		} else {
			areaAlloc->y0 = (unsigned short)(anchorY);
			areaAlloc->y1 = (unsigned short)(anchorY + areaReq->y1);
		}

		areaAlloc->x0 = (unsigned short)(anchorX);
		if (areaAlloc->x0 + areaReq->x1 <= areaTotal->x1) {
			areaAlloc->x1 = (unsigned short)
					(areaAlloc->x0 + areaReq->x1);
		} else {
			areaAlloc->x1 = areaTotal->x1;
		}
		areaAlloc->fitToSide = PSA_RIGHT;
		break;

	case PSA_LEFT:
		if (areaTotal->y1 - anchorY < areaReq->y1) {
			if (areaTotal->y1 - areaTotal->y0 >= areaReq->y1) {
				areaAlloc->y0 =
					(unsigned short)
					(areaTotal->y1 - areaReq->y1);
				areaAlloc->y1 =
					(unsigned short)
					(areaAlloc->y0 + areaReq->y1);
			} else {
				areaAlloc->y0 = areaTotal->y0;
				areaAlloc->y1 = areaTotal->y1;
			}
		} else {
			areaAlloc->y0 = (unsigned short)(anchorY);
			areaAlloc->y1 = (unsigned short)(anchorY + areaReq->y1);
		}

		areaAlloc->x1 = (unsigned short)(anchorX);
		if (areaAlloc->x1 - areaReq->x1 >= areaTotal->x0) {
			areaAlloc->x0 = (unsigned short)
					(areaAlloc->x1 - areaReq->x1);
		} else {
			areaAlloc->x0 = areaTotal->x0;
		}
		areaAlloc->fitToSide = PSA_LEFT;
		break;
	case PSA_NONE:
		break;
	}
}

/* ========================================================================== */
/**
 *  area_check_for_fit()
 *
 * @brief  Checks if a specified free area is big enough for the required
 * allocation, and if the specified free area is the best (smallest) choice
 * of all discovered free areas.
 *
 * @param tmpArea - struct dmmTILERContPageAreaSpecT * - [in] A free area that
 * has been discovered and needs to be checked if it is big and small enough
 * for the required allocation area.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required allcoation
 * area.
 *
 * @return MSP_BOOL: True if the specified area is the new best fit.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL area_check_for_fit(struct dmmTILERContPageAreaSpecT *tmpArea,
				struct dmmTILERContCtxT *tlrCtx,
				struct dmmTILERContPageAreaT *areaReq)
{
	if ((tmpArea->ttlExpndAr.x1 - tmpArea->ttlExpndAr.x0) >= areaReq->x1 &&
		(tmpArea->ttlExpndAr.y1 - tmpArea->ttlExpndAr.y0) >=
								areaReq->y1) {
		if (/*The area of the just found total expanded space that may
					house the new 2D area to be allocated */
			(tmpArea->ttlExpndAr.x1 - tmpArea->ttlExpndAr.x0) *
			(tmpArea->ttlExpndAr.y1 - tmpArea->ttlExpndAr.y0) <
			/* The current smallest area found among all expanded
			spaces that can house the new 2D area to be allocated */
			(tlrCtx->tmpArSelect.ttlExpndAr.x1 -
			tlrCtx->tmpArSelect.ttlExpndAr.x0) *
			(tlrCtx->tmpArSelect.ttlExpndAr.y1 -
			tlrCtx->tmpArSelect.ttlExpndAr.y0)) {
			/* If the just found area is smaller than the current
							"smallest"... */
			tlrCtx->tmpArSelect.ttlExpndAr.x0 =
							tmpArea->ttlExpndAr.x0;
			tlrCtx->tmpArSelect.ttlExpndAr.y0 =
							tmpArea->ttlExpndAr.y0;
			tlrCtx->tmpArSelect.ttlExpndAr.x1 =
							tmpArea->ttlExpndAr.x1;
			tlrCtx->tmpArSelect.ttlExpndAr.y1 =
							tmpArea->ttlExpndAr.y1;
			tlrCtx->tmpArSelect.ttlExpndAr.fitToSide =
						tmpArea->ttlExpndAr.fitToSide;

			tlrCtx->tmpArSelect.plmntAr.x0 = tmpArea->plmntAr.x0;
			tlrCtx->tmpArSelect.plmntAr.y0 = tmpArea->plmntAr.y0;
			tlrCtx->tmpArSelect.plmntAr.x1 = tmpArea->plmntAr.x1;
			tlrCtx->tmpArSelect.plmntAr.y1 = tmpArea->plmntAr.y1;
			tlrCtx->tmpArSelect.plmntAr.fitToSide =
						tmpArea->plmntAr.fitToSide;

			tlrCtx->tmpArSelect.anchrAr = tmpArea->anchrAr;

			return MSP_TRUE;
		}
	}

	return MSP_FALSE;
}

/* ========================================================================== */
/**
 *  area_fit_to_left()
 *
 * @brief  Checks if a specified free area is big enough for the required
 * allocation, and if the specified free area is the best (smallest) choice
 * of all discovered free areas.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required allcoation
 * area.
 *
 * @param atchAr - struct dmmTILERContPageAreaT* - [in] Area along which
 * specified side the search for a new free area is performed.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @return MSP_BOOL: True if a qualified free zones is discovered.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL area_fit_to_left(struct dmmTILERContPageAreaT *areaReq,
				struct dmmTILERContPageAreaT *atchAr,
				struct dmmTILERContCtxT *tlrCtx)
{
	struct dmmTILERContPageAreaT *areaHit;
	struct dmmTILERContPageAreaSpecT  fitArea;
	signed long X;
	signed long Y;
	signed long anchorX;
	signed long anchorY;
	enum MSP_BOOL fit;

	areaHit = NULL;
	fitArea.anchrAr = atchAr;
	X = atchAr->x0 - 1;
	Y = atchAr->y0;
	anchorX = X;
	anchorY = Y;
	fit = MSP_FALSE;

	if (X >= 0 && X <= tlrCtx->contSizeX && Y >= 0 && Y <=
							tlrCtx->contSizeY) {
		while (Y <= atchAr->y1) {
			anchorY = Y;
			if (1 == point_free_test(tlrCtx, X, Y, &areaHit)) {
				if (Y == atchAr->y0) {
					/* If this is the first attempt to start
					"expanding" a 2D space, then it should
					be checked to the respective left,top,
					right or bottom directions if there is
					more points that can be concatenated to
					the area to discover. This "expansion"
					in certain direction is causing some
					interesting behaviour patterns.  */
					Y = (unsigned short)expand_top(tlrCtx,
					X, Y, &areaHit);
				}

				/* Once the vertical "top" and "bottom" free
				points along the side of the current area
				are found a "line" of free points is defined. */
				fitArea.ttlExpndAr.y0 = (unsigned short)(Y);
				fitArea.ttlExpndAr.y1 =
					(unsigned short)expand_bottom
					(tlrCtx, X, Y, &areaHit);
				/* The just defined line
				of free points is then "expanded" in the
				opposite direction of the side of the current
				area to witch the 2D space is being defined. */
				fitArea.ttlExpndAr.x0 =
				(unsigned short)expand_line_on_bottom_to_left(
				tlrCtx, X, Y, fitArea.ttlExpndAr.y1 - Y);
				fitArea.ttlExpndAr.x1 = (unsigned short)(X);

				fitArea.ttlExpndAr.fitToSide = PSA_LEFT;
				/* Once the new 2D space is defined along the
				specified side of the current 2D area, the
				required 2D area to be allocated has to be
				placed in the total expanded space as close to
				the current 2D area as possible (aligning to
				one of the corners of the new expanded space
				along the side of the current 2D area).
				Manipulating this function can change the
				allocation behaviour pattern drastically, and
				after further investigation will probably be
				target of changes. */
				area_required_to_allocated(areaReq,
				&(fitArea.ttlExpndAr), &(fitArea.plmntAr),
				anchorX, anchorY, PSA_LEFT);

				fit |= area_check_for_fit(&fitArea,
							tlrCtx, areaReq);

				break;
			} else if (areaHit != NULL) {
				/* If an area is hit when the first free point
				is being determined, it is safe to "jump" over
				it and start the new checks for free points. */
				Y = areaHit->y1 + 1;
				areaHit = NULL;
			} else {
				DMM_ASSERT_BREAK
			}
		}
	}
	return fit;
}

/* ========================================================================== */
/**
 *  area_fit_to_right()
 *
 * @brief  Checks if a specified free area is big enough for the required
 * allocation, and if the specified free area is the best (smallest) choice
 * of all discovered free areas.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required allcoation
 * area.
 *
 * @param atchAr - struct dmmTILERContPageAreaT* - [in] Area along which
 * specified side the search for a new free area is performed.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @return MSP_BOOL: True if a qualified free zones is discovered.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL area_fit_to_right(struct dmmTILERContPageAreaT *areaReq,
				struct dmmTILERContPageAreaT *atchAr,
				struct dmmTILERContCtxT *tlrCtx)
{
	struct dmmTILERContPageAreaT *areaHit;
	struct dmmTILERContPageAreaSpecT  fitArea;
	signed long X;
	signed long Y;
	signed long anchorX;
	signed long anchorY;
	enum MSP_BOOL fit;

	areaHit = NULL;
	fitArea.anchrAr = atchAr;
	X = atchAr->x1 + 1;
	Y = atchAr->y0;
	anchorX = X;
	anchorY = Y;
	fit = MSP_FALSE;

	if (X >= 0 && X <= tlrCtx->contSizeX && Y >= 0 && Y <=
							tlrCtx->contSizeY) {
		while (Y <= atchAr->y1) {
			anchorY = Y;
			if (1 == point_free_test(tlrCtx, X, Y, &areaHit)) {
				if (Y == atchAr->y0) {
					/*If this is the first attempt to start
					"expanding" a 2D space, then it should
					be checked to the respective left,top,
					right or bottom directions if there is
					more points that can be concatenated to
					the area to discover. This "expansion"
					in certain direction is causing some
					interesting behaviour patterns. */
					Y = (unsigned short)expand_top(tlrCtx,
								X, Y, &areaHit);
				}

				/* Once the vertical "top" and "bottom" free
				points along the side of the current area
				are found a "line" of free points is
				defined. */
				fitArea.ttlExpndAr.y0 = (unsigned short)(Y);
				fitArea.ttlExpndAr.y1 =
				(unsigned short)expand_bottom(tlrCtx, X, Y,
								&areaHit);
				/* The just defined line of free points is then
				"expanded" in the opposite direction of
				the side of the current area to witch the 2D
				space is being defined. */
				fitArea.ttlExpndAr.x0 = (unsigned short)(X);
				fitArea.ttlExpndAr.x1 =
				(unsigned short)expand_line_on_bottom_to_right(
				tlrCtx, X, Y, fitArea.ttlExpndAr.y1 - Y);

				fitArea.ttlExpndAr.fitToSide = PSA_RIGHT;
				/* Once the new 2D space is defined along the
				specified side of the current 2D area, the
				required 2D area to be allocated has to be
				placed in the total expanded space as close to
				the current 2D area as possible (aligning to one
				of the corners of the new expanded space along
				the side of the current 2D area). Manipulating
				this function can change the allocation
				behaviour pattern drastically, and after further
				investigation will probably be target of
				changes. */
				area_required_to_allocated(areaReq,
				&(fitArea.ttlExpndAr), &(fitArea.plmntAr),
				anchorX, anchorY, PSA_RIGHT);

				fit |= area_check_for_fit(&fitArea,
							tlrCtx, areaReq);

				break;
			} else if (areaHit != NULL) {
				/* If an area is hit when the first free point
				is being determined, it is safe to "jump" over
				it and start the new checks for free points. */
				Y = areaHit->y1 + 1;
				areaHit = NULL;
			} else {
				DMM_ASSERT_BREAK
			}
		}
	}
	return fit;
}

/* ========================================================================== */
/**
 *  area_fit_to_top()
 *
 * @brief  Checks if a specified free area is big enough for the required
 * allocation, and if the specified free area is the best (smallest) choice
 * of all discovered free areas.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required allcoation
 * area.
 *
 * @param atchAr - struct dmmTILERContPageAreaT* - [in] Area along which
 * specified side the search for a new free area is performed.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @return MSP_BOOL: True if a qualified free zones is discovered.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL area_fit_to_top(struct dmmTILERContPageAreaT *areaReq,
				struct dmmTILERContPageAreaT *atchAr,
				struct dmmTILERContCtxT *tlrCtx)
{
	struct dmmTILERContPageAreaT *areaHit;
	struct dmmTILERContPageAreaSpecT  fitArea;
	signed long X;
	signed long Y;
	signed long anchorX;
	signed long anchorY;
	enum MSP_BOOL fit;

	areaHit = NULL;
	fitArea.anchrAr = atchAr;
	X = atchAr->x0;
	Y = atchAr->y0 - 1;
	anchorX = X;
	anchorY = Y;
	fit = MSP_FALSE;

	if (X >= 0 && X < tlrCtx->contSizeX && Y >= 0 &&
							Y < tlrCtx->contSizeY) {
		while (X <= atchAr->x1) {
			anchorX = X;
			if (1 == point_free_test(tlrCtx, X, Y, &areaHit)) {
				if (X == atchAr->x0) {
					/* If this is the first attempt to start
					"expanding" a 2D space, then it should
					be checked to the respective left,top,
					right or bottom directions if there is
					more points that can be concatenated to
					the area to discover. This "expansion"
					in certain direction is causing some
					interesting behaviour patterns. */
					X = (unsigned short)expand_left(
					tlrCtx, X, Y, &areaHit);
				}

				/* Once the vertical "left" and "right" free
				points along the side of the current area
				are found a "line" of free points is defined. */
				fitArea.ttlExpndAr.x0 = (unsigned short)(X);
				fitArea.ttlExpndAr.x1 =
					(unsigned short)expand_right(tlrCtx,
					X, Y, &areaHit);
				/* The just defined line of free points is then
				"expanded" in the opposite direction of
				the side of the current area to witch the 2D
				space is being defined. */
				fitArea.ttlExpndAr.y0 =
				(unsigned short)expand_line_on_right_to_top(
				tlrCtx, X, Y, fitArea.ttlExpndAr.x1 - X);
				fitArea.ttlExpndAr.y1 = (unsigned short)(Y);

				fitArea.ttlExpndAr.fitToSide = PSA_TOP;
				/* Once the new 2D space is defined along the
				specified side of the current 2D area, the
				required 2D area to be allocated has to be
				placed in the total expanded space as close
				to the current 2D area as possible (aligning
				to one of the corners of the new expanded space
				along the side of the current 2D area).
				Manipulating this function can change the
				allocation behaviour pattern drastically, and
				after further investigation will probably be
				target of changes. */
				area_required_to_allocated(areaReq,
				&(fitArea.ttlExpndAr), &(fitArea.plmntAr),
				anchorX, anchorY, PSA_TOP);

				fit |= area_check_for_fit(&fitArea, tlrCtx,
								areaReq);

				break;
			} else if (areaHit != NULL) {
				/* If an area is hit when the first free point
				is being determined, it is safe to "jump" over
				it and start the new checks for free points. */
				X = areaHit->x1 + 1;
				areaHit = NULL;
			} else {
				DMM_ASSERT_BREAK
			}
		}
	}
	return fit;
}

/* ========================================================================== */
/**
 *  area_fit_to_bottom()
 *
 * @brief  Checks if a specified free area is big enough for the required
 * allocation, and if the specified free area is the best (smallest) choice
 * of all discovered free areas.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required allcoation
 * area.
 *
 * @param atchAr - struct dmmTILERContPageAreaT* - [in] Area along which
 * specified side the search for a new free area is performed.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @return MSP_BOOL: True if a qualified free zones is discovered.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL area_fit_to_bottom(struct dmmTILERContPageAreaT *areaReq,
				struct dmmTILERContPageAreaT *atchAr,
				struct dmmTILERContCtxT *tlrCtx)
{
	struct dmmTILERContPageAreaT *areaHit;
	struct dmmTILERContPageAreaSpecT  fitArea;
	signed long X;
	signed long Y;
	signed long anchorX;
	signed long anchorY;
	enum MSP_BOOL fit;

	areaHit = NULL;
	fitArea.anchrAr = atchAr;
	X = atchAr->x0;
	Y = atchAr->y1 + 1;
	anchorX = X;
	anchorY = Y;
	fit = MSP_FALSE;

	if (X >= 0 && X < tlrCtx->contSizeX && Y >= 0 &&
							Y < tlrCtx->contSizeY) {
		while (X <= atchAr->x1) {
			if (1 == point_free_test(tlrCtx, X, Y, &areaHit)) {
				anchorX = X;
				if (X == atchAr->x0) {
					/* If this is the first attempt to start
					"expanding" a 2D space, then it should
					be checked to the respective left,top,
					right or bottom directions if there is
					more points that can be concatenated to
					the area to discover. This "expansion"
					in certain direction is causing some
					interesting behaviour patterns. */
					X = (unsigned short)expand_left(
							tlrCtx, X, Y, &areaHit);
				}

				/* Once the vertical "left" and "right" free
				points along the side of the current area
				are found a "line" of free points is defined. */
				fitArea.ttlExpndAr.x0 = (unsigned short)(X);
				fitArea.ttlExpndAr.x1 =
					(unsigned short)expand_right(tlrCtx,
								X, Y, &areaHit);
				/* The just defined line of free points is then
				"expanded" in the opposite direction of
				the side of the current area to witch the 2D
				space is being defined. */
				fitArea.ttlExpndAr.y0 = (unsigned short)(Y);
				fitArea.ttlExpndAr.y1 =
				(unsigned short)expand_line_on_right_to_bottom(
				tlrCtx, X, Y, fitArea.ttlExpndAr.x1 - X);

				fitArea.ttlExpndAr.fitToSide = PSA_BOTTOM;
				/* Once the new 2D space is defined along the
				specified side of the current 2D area, the
				required 2D area to be allocated has to be
				placed in the total expanded space as close to
				the current 2D area as possible (aligning to
				one of the corners of the new expanded space
				along the side of the current 2D area).
				Manipulating this function can change the
				allocation behaviour pattern drastically, and
				after further investigation will probably be
				target of changes. */
				area_required_to_allocated(areaReq,
				&(fitArea.ttlExpndAr), &(fitArea.plmntAr),
						anchorX, anchorY, PSA_BOTTOM);

				fit |= area_check_for_fit(&fitArea, tlrCtx,
								areaReq);

				break;
			} else if (areaHit != NULL) {
				/* If an area is hit when the first free point
				is being determined, it is safe to "jump" over
				it and start the new checks for free points. */
				X = areaHit->x1 + 1;
				areaHit = NULL;
			} else {
				DMM_ASSERT_BREAK
			}
		}
	}
	return fit;
}

/* ========================================================================== */
/**
 *  alloc_2D_area()
 *
 * @brief  Allocates a 2D area and returns a pointer to it.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param areaReq - struct dmmTILERContPageAreaT* - [in] Required area for
 * allcoation.
 *
 * @return struct dmmTILERContPageAreaT*: pointer to the allocated area.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
struct dmmTILERContPageAreaT *
alloc_2d_area(struct dmmTILERContCtxT *dmmTilerCtx,
		struct dmmTILERContPageAreaT *areaReq)
{
	struct dmmTILERContPageLstT *allocatedArea = NULL;
	struct dmmTILERContPageLstT *usedIter;

	usedIter = dmmTilerCtx->usdArList;

	dmmTilerCtx->tmpArSelect.ttlExpndAr.x0 = 0;
	dmmTilerCtx->tmpArSelect.ttlExpndAr.y0 = 0;
	dmmTilerCtx->tmpArSelect.ttlExpndAr.x1 = dmmTilerCtx->contSizeX - 1;
	dmmTilerCtx->tmpArSelect.ttlExpndAr.y1 = dmmTilerCtx->contSizeY - 1;

	mutex_lock(&dmmTilerCtx->mtx);
	if (usedIter != NULL) {
		int fit = 0;
		while (usedIter != NULL) {
			fit |= area_fit_to_top(areaReq,
				&(usedIter->pgAr), dmmTilerCtx);
			fit |= area_fit_to_right(areaReq,
				&(usedIter->pgAr), dmmTilerCtx);
			fit |= area_fit_to_bottom(areaReq,
				&(usedIter->pgAr), dmmTilerCtx);
			fit |= area_fit_to_left(areaReq,
				&(usedIter->pgAr), dmmTilerCtx);
			usedIter = usedIter->pgArNext;
		}

		if (fit > 0) {
			allocatedArea = kmalloc
			(sizeof(struct dmmTILERContPageLstT), GFP_KERNEL);
			if (!allocatedArea)
				return NULL;
			memset(allocatedArea, 0x0,
					sizeof(struct dmmTILERContPageLstT));
			allocatedArea->pgAr.x0 =
					dmmTilerCtx->tmpArSelect.plmntAr.x0;
			allocatedArea->pgAr.y0 =
					dmmTilerCtx->tmpArSelect.plmntAr.y0;
			allocatedArea->pgAr.x1 =
					dmmTilerCtx->tmpArSelect.plmntAr.x1;
			allocatedArea->pgAr.y1 =
					dmmTilerCtx->tmpArSelect.plmntAr.y1;
			allocatedArea->pgAr.fitToSide =
				dmmTilerCtx->tmpArSelect.plmntAr.fitToSide;
			allocatedArea->anchrAr =
					dmmTilerCtx->tmpArSelect.anchrAr;

			usedIter = dmmTilerCtx->usdArList;

			while (usedIter->pgArNext != NULL)
				usedIter = usedIter->pgArNext;

			usedIter->pgArNext = allocatedArea;
		}
	} else {
		allocatedArea = kmalloc
			(sizeof(struct dmmTILERContPageLstT), GFP_KERNEL);
		if (!allocatedArea)
			return NULL;
		memset(allocatedArea, 0x0, sizeof(struct dmmTILERContPageLstT));
		allocatedArea->pgAr.x0 = 0;
		allocatedArea->pgAr.y0 = 0;
		allocatedArea->pgAr.x1 = areaReq->x1;
		allocatedArea->pgAr.y1 = areaReq->y1;
		allocatedArea->pgAr.fitToSide = PSA_BOTTOM;
		allocatedArea->anchrAr = NULL;
		allocatedArea->pgArNext = NULL;
		dmmTilerCtx->usdArList = allocatedArea;
	}

	mutex_unlock(&dmmTilerCtx->mtx);

	if (allocatedArea == NULL)
		return NULL;
	else
		return &(allocatedArea->pgAr);
}

/* ========================================================================== */
/**
 *  dealloc_2D_area()
 *
 * @brief  Deletes a 2D area from the TILER context.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param areaRem - struct dmmTILERContPageAreaT* - [in] Area to remove.
 *
 * @return MSP_BOOL: True if the specified area is successfuly deleted.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
enum MSP_BOOL dealloc_2d_area(struct dmmTILERContCtxT *dmmTilerCtx,
			struct dmmTILERContPageAreaT *areaRem)
{
	struct dmmTILERContPageLstT *delItm;
	struct dmmTILERContPageLstT *usedIter;
	struct dmmTILERContPageLstT *usedPrev;

	mutex_lock(&dmmTilerCtx->mtx);

	delItm = NULL;
	usedIter = dmmTilerCtx->usdArList;
	usedPrev = NULL;

	while (usedIter != NULL) {
		if (areaRem->x0 == usedIter->pgAr.x0 &&
				areaRem->y0 == usedIter->pgAr.y0) {
			delItm = usedIter;
			if (usedPrev != NULL)
				usedPrev->pgArNext = usedIter->pgArNext;
			else
				dmmTilerCtx->usdArList =
				dmmTilerCtx->usdArList->pgArNext;

			break;
		}

		usedPrev = usedIter;
		usedIter = usedIter->pgArNext;
	}

	mutex_unlock(&dmmTilerCtx->mtx);

	if (delItm != NULL) {
		signed long i;
		enum errorCodeT eCode = DMM_NO_ERROR;
		unsigned long numPages = 0x0;

		/* If the memory pages are provided by the dmm pages memory
			pool, then free them. Otherwise leave them for the user
			to free them.
		*/
		if (delItm->pgAr.patCustomPages == MSP_FALSE) {
			numPages = (delItm->pgAr.x1 - delItm->pgAr.x0 + 1)*
				(delItm->pgAr.y1 - delItm->pgAr.y0 + 1);
			/* Get the area to free associated physical memory pages
			   and free them - how can they be obtained from the
			   PAT lut?!
			   */
			/* Is it viable and easier that each allocated area has
				a list of all the physical pages mapped to it?
			*/
			/* As currently the system is working with a memory page
				pool leave it be.
			*/
			for (i = 0;
				i < numPages && eCode == DMM_NO_ERROR; i++) {
					dmm_free_phys_page((unsigned long)
					(delItm->pgAr.patPageEntries[i]));
			}
		}

		if (eCode == DMM_NO_ERROR && delItm->pgAr.patCustomPages == 0) {
			dma_free_coherent(NULL, delItm->pgAr.dma_size,
			delItm->pgAr.dma_va,
			delItm->pgAr.dma_pa);
			delItm->pgAr.patPageEntries = NULL;
			delItm->pgAr.patPageEntriesSpace = NULL;
		}

		kfree(delItm);
		return MSP_TRUE;
	} else {
		return MSP_FALSE;
	}
}

/* ========================================================================== */
/**
 *  search_2D_area()
 *
 * @brief  Deletes a 2D area from the TILER context.
 *
 * @param tlrCtx - struct dmmTILERContCtxT* - [in] TILER context structure.
 *
 * @param X - signed long - [in] X coordinate inside the search area.
 *
 * @param Y - signed long - [in] X coordinate inside the search area.
 *
 * @param xInvert - MSP_BOOL - [in] X coordinate is inverted.
 *
 * @param yInvert - MSP_BOOL - [in] Y coordinate is inverted.
 *
 * @return struct dmmTILERContPageAreaT*: pointer to the discovered 2D area
 * or NULL if no such area is found.
 *
 * @pre There is no pre conditions.
 *
 * @post There is no post conditions.
 *
 * @see
 */
/* ========================================================================== */
struct dmmTILERContPageAreaT *
search_2d_area(struct dmmTILERContCtxT *dmmTilerCtx,
		signed long X,
		signed long Y,
		enum MSP_BOOL xInvert,
		enum MSP_BOOL yInvert)
{
	struct dmmTILERContPageLstT *usedIter;
	usedIter = dmmTilerCtx->usdArList;

	while (usedIter != NULL) {
		if (usedIter->pgAr.x0 <= X && X <= usedIter->pgAr.x1 &&
			usedIter->pgAr.y0 <= Y && Y <= usedIter->pgAr.y1) {
			return &(usedIter->pgAr);
		}

		usedIter = usedIter->pgArNext;
	}

	return NULL;
}
