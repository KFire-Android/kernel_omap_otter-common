/*
 * tiler.h
 *
 * TILER driver support functions for TI OMAP processors.
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

#ifndef _TILER_H_
#define _TILER_H_

#define TILER_MAX_NUM_BLOCKS 16

#define TILIOC_GBUF _IOWR('z', 101, u32)
#define TILIOC_FBUF _IOWR('z', 102, u32)
#define TILIOC_GPA _IOWR('z', 103, u32)
#define TILIOC_MBUF _IOWR('z', 104, u32)
#define TILIOC_QBUF _IOWR('z', 105, u32)
#define TILIOC_RBUF _IOWR('z', 106, u32)
#define TILIOC_URBUF _IOWR('z', 107, u32)
#define TILIOC_QUERY_BLK _IOWR('z', 108, u32)

enum tiler_fmt {
	TILFMT_MIN     = -1,
	TILFMT_INVALID = -1,
	TILFMT_NONE    = 0,
	TILFMT_8BIT    = 1,
	TILFMT_16BIT   = 2,
	TILFMT_32BIT   = 3,
	TILFMT_PAGE    = 4,
	TILFMT_MAX     = 4
};

struct area {
	u16 width;
	u16 height;
};

struct tiler_block_info {
	enum tiler_fmt fmt;
	union {
		struct area area;
		u32 len;
	} dim;
	u32 stride;
	void *va;
	u32 pa;
};

struct tiler_alloc_info {
	u32 num_blocks;
	struct tiler_block_info blocks[TILER_MAX_NUM_BLOCKS];
	u32 offset;
};

u32 tiler_alloc(struct tiler_alloc_info);

#endif
