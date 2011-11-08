/*
 * bvbuffdesc.h
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

#ifndef BVBUFFDESC_H
#define BVBUFFDESC_H

/*
 * bvbuffmap - This is a private structure used by BLTsville
 * implementations to manage resources associated with a buffer.  A pointer
 * to this is returned from bv_map() and used in subsequent bv_blt() and
 * bv_unmap() calls.
 */
struct bvbuffmap;

/*
 * bvbuffdesc - This structure is used to specify the buffer parameters
 * in a call to bv_map().
 */
struct bvbuffdesc {
	unsigned int structsize;	/* used to identify struct version */
	void *virtaddr;			/* virtual ptr to start of buffer */
	unsigned long length;		/* length of the buffer in bytes */
	struct bvbuffmap *map;		/* resource(s) associated w/buffer */
	unsigned long pagesize;		/* page size in bytes */
	unsigned long *pagearray;	/* array of physical page addresses */
	unsigned int pagecount;		/* number of pages in the page array */
	unsigned long pageoffset;	/* page offset in bytes */
};

#endif /* BVBUFFDESC_H */
