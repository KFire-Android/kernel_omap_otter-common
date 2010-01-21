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

#ifndef TILER_H
#define TILER_H

#define TILER_PAGE 0x1000
#define TILER_WIDTH    256
#define TILER_HEIGHT   128
#define TILER_BLOCK_WIDTH  64
#define TILER_BLOCK_HEIGHT 64
#define TILER_LENGTH (TILER_WIDTH * TILER_HEIGHT * TILER_PAGE)

#define TILER_MAX_NUM_BLOCKS 16

#define TILIOC_GBUF  _IOWR('z', 100, u32)
#define TILIOC_FBUF  _IOWR('z', 101, u32)
#define TILIOC_GSSP  _IOWR('z', 102, u32)
#define TILIOC_MBUF  _IOWR('z', 103, u32)
#define TILIOC_UMBUF _IOWR('z', 104, u32)
#define TILIOC_QBUF  _IOWR('z', 105, u32)
#define TILIOC_RBUF  _IOWR('z', 106, u32)
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

#define TILER_ACC_MODE_SHIFT  (27)
#define TILER_ACC_MODE_MASK   (3)
#define TILER_GET_ACC_MODE(x) ((enum tiler_fmt)\
(((u32)x & (TILER_ACC_MODE_MASK<<TILER_ACC_MODE_SHIFT))>>TILER_ACC_MODE_SHIFT))

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
	void *ptr;
	u32 ssptr;
};

struct tiler_buf_info {
	s32 num_blocks;
	struct tiler_block_info blocks[TILER_MAX_NUM_BLOCKS];
	s32 offset;
};

struct tiler_view_orient {
	u8 rotate_90;
	u8 x_invert;
	u8 y_invert;
};

s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr);

s32 tiler_free(u32 sys_addr);

u32 tiler_get_natural_addr(void *sysPtr);

void tiler_rotate_view(struct tiler_view_orient *orient, u32 rotation);

u32 tiler_stride(u32 tsptr);

void tiler_alloc_packed_nv12(s32 *count, u32 width, u32 height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, s32 aligned);

void tiler_alloc_packed(s32 *count, enum tiler_fmt fmt, u32 width, u32 height,
			void **sysptr, void **allocptr, s32 aligned);

#endif

