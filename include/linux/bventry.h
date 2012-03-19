/*
 * bventry.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef BVENTRY_H
#define BVENTRY_H

/* Forward declarations */
struct bvbuffdesc;
struct bvbltparams;

/*
 * BLTsville interface definition.
 */
typedef enum bverror (*BVFN_MAP) (struct bvbuffdesc *buffdesc);
typedef enum bverror (*BVFN_UNMAP) (struct bvbuffdesc *buffdesc);
typedef enum bverror (*BVFN_BLT) (struct bvbltparams *bltparams);

struct bventry {
	BVFN_MAP bv_map;
	BVFN_UNMAP bv_unmap;
	BVFN_BLT bv_blt;
};

#endif /* BVENTRY_H */
