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

/* utility functions */
static inline u32 tilfmt_bpp(enum tiler_fmt fmt)
{
	return  fmt == TILFMT_8BIT ? 1 :
		fmt == TILFMT_16BIT ? 2 :
		fmt == TILFMT_32BIT ? 4 : 0;
}

/* Event types */
#define TILER_DEVICE_CLOSE	0

/**
 * Registers a notifier block with TILER driver.
 *
 * @param nb		notifier_block
 *
 * @return error status
 */
int tiler_reg_notifier(struct notifier_block *nb);

/**
 * Un-registers a notifier block with TILER driver.
 *
 * @param nb		notifier_block
 *
 * @return error status
 */
int tiler_unreg_notifier(struct notifier_block *nb);

/**
 * Reserves a 1D or 2D TILER block area and memory for the
 * current process with group ID 0.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 *
 * @return error status
 */
s32 tiler_alloc(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr);

/**
 * Reserves a 1D or 2D TILER block area and memory with extended
 * arguments.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param align 	block alignment (default: PAGE_SIZE)
 * @param offs 		block offset
 * @param gid 		group ID
 * @param pid 		process ID
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 *
 * @return error status
 */
s32 tiler_allocx(enum tiler_fmt fmt, u32 width, u32 height,
			u32 align, u32 offs, u32 gid, pid_t pid, u32 *sys_addr);

/**
 * Maps an existing buffer to a 1D or 2D TILER area for the
 * current process with group ID 0.
 *
 * Currently, only 1D area mapping is supported.
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param usr_addr	user space address of existing buffer.
 *
 * @return error status
 */
s32 tiler_map(enum tiler_fmt fmt, u32 width, u32 height, u32 *sys_addr,
								u32 usr_addr);

/**
 * Maps an existing buffer to a 1D or 2D TILER area with
 * extended arguments.
 *
 * Currently, only 1D area mapping is supported.
 *
 * NOTE: alignment is always PAGE_SIZE and offset is 0
 *
 * @param fmt 		TILER bit mode
 * @param width 	block width
 * @param height 	block height (must be 1 for 1D)
 * @param gid 		group ID
 * @param pid 		process ID
 * @param sys_addr 	pointer where system space (L3) address
 *  			will be stored.
 * @param usr_addr	user space address of existing buffer.
 *
 * @return error status
 */
s32 tiler_mapx(enum tiler_fmt fmt, u32 width, u32 height,
			u32 gid, pid_t pid, u32 *sys_addr, u32 usr_addr);

/**
 * Free TILER memory.
 *
 * @param sys_addr system space (L3) address.
 *
 * @return an error status.
 */
s32 tiler_free(u32 sys_addr);

/**
 * Reserves tiler area for n identical set of blocks (buffer)
 * for the current process.  Use this method to get optimal
 * placement of multiple related tiler blocks; however, it may
 * not reserve area if tiler_alloc is equally efficient.
 *
 * @param n 	number of identical set of blocks
 * @param b	information on the set of blocks (ptr, ssptr and
 *  		stride fields are ignored)
 *
 * @return error status
 */
s32 tiler_reserve(u32 n, struct tiler_buf_info *b);

/**
 * Reserves tiler area for n identical set of blocks (buffer) fo
 * a given process. Use this method to get optimal placement of
 * multiple related tiler blocks; however, it may not reserve
 * area if tiler_alloc is equally efficient.
 *
 * @param n 	number of identical set of blocks
 * @param b	information on the set of blocks (ptr, ssptr and
 *  		stride fields are ignored)
 * @param pid   process ID
 *
 * @return error status
 */
s32 tiler_reservex(u32 n, struct tiler_buf_info *b, pid_t pid);

u32 tiler_reorient_addr(u32 tsptr, struct tiler_view_orient orient);

u32 tiler_get_natural_addr(void *sys_ptr);

u32 tiler_reorient_topleft(u32 tsptr, struct tiler_view_orient orient,
				u32 width, u32 height);

u32 tiler_stride(u32 tsptr);

void tiler_rotate_view(struct tiler_view_orient *orient, u32 rotation);

void tiler_alloc_packed(s32 *count, enum tiler_fmt fmt, u32 width, u32 height,
			void **sysptr, void **allocptr, s32 aligned);

void tiler_alloc_packed_nv12(s32 *count, u32 width, u32 height, void **y_sysptr,
				void **uv_sysptr, void **y_allocptr,
				void **uv_allocptr, s32 aligned);

#endif

