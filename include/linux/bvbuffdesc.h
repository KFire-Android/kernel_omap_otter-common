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

#define BVATDEF_VENDOR_SHIFT	24
#define BVATDEF_VENDOR_MASK	(0xFF << BVATDEF_VENDOR_SHIFT)

/* Common aux type */
#define BVATDEF_VENDOR_ALL	(0x00 << BVATDEF_VENDOR_SHIFT)

/* Texas Instruments, Inc. */
#define BVATDEF_VENDOR_TI	(0x01 << BVATDEF_VENDOR_SHIFT)

enum bvauxtype {
	BVAT_NONE = 0,	/* auxptr not used */
	BVAT_PHYSDESC =	/* handle points to bvphysdesc struct */
		BVATDEF_VENDOR_ALL + 1,

#ifdef BVAT_EXTERNAL_INCLUDE
#include BVAT_EXTERNAL_INCLUDE
#endif
};


struct bvphysdesc {
	unsigned int structsize;	/* used to identify struct version */
	unsigned long pagesize;		/* page size in bytes */
	unsigned long *pagearray;	/* array of physical pages */
	unsigned int pagecount;		/* number of pages in the pagearray */
	unsigned long pageoffset;	/* page offset in bytes */
};

/*
 * bvbuffdesc - This structure is used to specify the buffer parameters
 * in a call to bv_map().
 */
struct bvbuffdesc {
	unsigned int structsize;	/* used to identify struct version */
	void *virtaddr;			/* virtual ptr to start of buffer */
	unsigned long length;		/* length of the buffer in bytes */
	struct bvbuffmap *map;		/* resource(s) associated w/buffer */
	enum bvauxtype auxtype;		/* type of auxptr */
	void *auxptr;			/* additional buffer description data;
					type depends on auxtype */
};

#endif /* BVBUFFDESC_H */
