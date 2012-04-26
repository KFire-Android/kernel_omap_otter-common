/*
 * gcbv-cache.c
 *
 * Copyright (C) 2012 Texas Instruments Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "gcmain.h"

enum bverror gcbvcacheop(int count, struct c2dmrgn rgn[],
		enum bvcacheop cacheop)
{
	enum bverror err = BVERR_NONE;

	switch (cacheop) {

	case DMA_FROM_DEVICE:
		c2dm_l2cache(count, rgn, cacheop);
		c2dm_l1cache(count, rgn, cacheop);
		break;
	case DMA_TO_DEVICE:
		c2dm_l1cache(count, rgn, cacheop);
		c2dm_l2cache(count, rgn, cacheop);
		break;
	case DMA_BIDIRECTIONAL:
		c2dm_l1cache(count, rgn, cacheop);
		c2dm_l2cache(count, rgn, cacheop);
		break;
	default:
		err = BVERR_CACHEOP;
		break;
	}

	return err;
}

