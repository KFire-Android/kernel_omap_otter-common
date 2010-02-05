/*
 * tiler_allocator.h
 *
 * Author: Ravi Ramachandra <r.ramachandra@ti.com>
 *
 * Tiler Allocator Interface functions for TI OMAP processors.
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

#ifndef _TILER_ALLOCATOR_H_
#define _TILER_ALLOCATOR_H_

#include "tiler_common.h"
#include "tiler_utils.h"

#define OCCUPIED           YES
#define NOT_OCCUPIED    NO

#define FIT             YES
#define NO_FIT          NO


/*Provide inclusive length between co-ordinates */
#define INCL_LEN(high, low)      (high - low + 1)

#define INCL_LEN_MOD(start, end)      ((start > end) ? (start-end + 1) : \
(end - start + 1))

#define TOTAL_BOUNDARY(stat) ((stat)->top_boundary + (stat)->bottom_boundary + \
				(stat)->left_boundary + (stat)->right_boundary)
#define TOTAL_OCCUPIED(stat) ((stat)->top_occupied + (stat)->bottom_occupied + \
				(stat)->left_occupied + (stat)->right_occupied)


#define TILER_USER_SPACE

#ifdef TILER_USER_SPACE
#define MUTEX_LOCK(m)   (mutex_lock(m))
#define MUTEX_REL(m)     (mutex_unlock(m))
#endif

enum Criteria {
	CR_MAX_NEIGHS = 0x01,
	CR_FIRST_FOUND = 0x10,
	CR_BIAS_HORIZONTAL = 0x20,
	CR_BIAS_VERTICAL = 0x40,
	CR_DIAGONAL_BALANCE = 0x80
};

struct nearness_factor {
	s32 nf_x;
	s32 nf_y;
};



struct tiler_page {
	u8 is_occupied;  /* is tiler_page Occupied */
	/* Parent area to which this tiler_page belongs */
	struct area_spec parent_area;
	u32 type; /* 1D or 2D */
	u32 reserved;
};


/*@descrption: init_tiler
 *Initializes the tiler container
 */
s32 init_tiler(void);

s32 deinit_tiler(void);

s32 allocate_2d_area(IN u16 w, IN u16 h, IN enum alignment align,
				OUT struct area_spec *allocated_area);

s32 allocate_1d_pages(IN u32 num_of_pages,
					OUT struct area_spec *allocated_pages);

s32 deallocate(IN struct area_spec *to_be_removed_area);

s32 test_dump_alloc_list(void);

s32 test_allocate_2D_area(IN u16 w, IN u16 h, IN enum alignment align,
			u16 corner, OUT struct area_spec *allocated_area);

s32 test_get_busy_neigh_stats(u16 width, u16 height,
					struct area_spec *top_left_corner,
					struct neighbour_stats *neighbour_stat);

s32 test_check_busy_tile(IN u16 x, u16 y);

s32 retrieve_parent_area(u16 x0, u16 y0, struct area_spec *parent_area);

#endif
