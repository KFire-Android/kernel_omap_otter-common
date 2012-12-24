/*
 * cache-2dmanager.h
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

#ifndef CACHE_2DMANAGER_H_
#define CACHE_2DMANAGER_H_

#include "slab.h"

#ifdef CONFIG_ARCH_OMAP4
#define L1CACHE_SIZE 32768
#define L2CACHE_SIZE 1048576

#define L1THRESHOLD L1CACHE_SIZE
#define L2THRESHOLD L2CACHE_SIZE
#else
#error Cache configuration must be specified.
#endif

struct c2dmrgn {
	char *start;	/* addr of upper left of rect */
	size_t span;	/* bytes to be operated on per line */
	size_t lines;	/* lines to be operated on */
	long stride;	/* bytes per line */
};

/*
 *	c2dm_l1cache(count, rgns, dir)
 *
 *	L1 Cache operations in 2D
 *
 *	- count  - number of regions
 *	- rgns   - array of regions
 *	- dir	 - cache operation direction
 *
 */
void c2dm_l1cache(int count, struct c2dmrgn rgns[], int dir);

/*
 *	c2dm_l2cache(count, rgns, dir)
 *
 *	L2 Cache operations in 2D
 *
 *	- count  - number of regions
 *	- rgns   - array of regions
 *	- dir	 - cache operation direction
 *
 */
void c2dm_l2cache(int count, struct c2dmrgn rgns[], int dir);


#endif /* CACHE_2DMANAGER_H_ */
