/*
 * bvcache.h
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef BVCACHE_H_
#define BVCACHE_H_

/* Forward declarations */
struct bvbuffdesc;
struct bvsurfgeom;
struct bvrect;

/*
 * This defines which cache operation the user intends to use
 * BVCACHE_CPU_TO_DEVICE = clean
 * BVCACHE_CPU_FROM_DEVICE = invalidate
 * BVCACHE_BIDIRECTIONAL = flush
 */
enum bvcacheop {
	BVCACHE_BIDIRECTIONAL = 0,
	BVCACHE_CPU_TO_DEVICE = 1,
	BVCACHE_CPU_FROM_DEVICE = 2,
	BVCACHE_RESERVED3 = 3,
};

struct bvcopparams {
	unsigned int structsize;	/* used to identify struct version */
	struct bvbuffdesc *desc;
	struct bvsurfgeom *geom;
	struct bvrect     *rect;
	enum bvcacheop cacheop;
};

#endif /* BVCACHE_H_ */
