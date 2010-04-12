/*
 * _tcm_sita.h
 *
 * SImple Tiler Allocator (SiTA) private structures.
 *
 * Author: Ravi Ramachandra <r.ramachandra@ti.com>
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

#ifndef _TCM_SITA_H_
#define _TCM_SITA_H_

#include "tcm.h"

#define TL_CORNER       0
#define TR_CORNER       1
#define BL_CORNER       3
#define BR_CORNER       4

/*Provide inclusive length between co-ordinates */
#define INCL_LEN(high, low)		((high) - (low) + 1)
#define INCL_LEN_MOD(start, end)   ((start) > (end) ? (start) - (end) + 1 : \
		(end) - (start) + 1)

#define BOUNDARY(stat) ((stat)->top_boundary + (stat)->bottom_boundary + \
				(stat)->left_boundary + (stat)->right_boundary)
#define OCCUPIED(stat) ((stat)->top_occupied + (stat)->bottom_occupied + \
				(stat)->left_occupied + (stat)->right_occupied)

enum Criteria {
	CR_MAX_NEIGHS       = 0x01,
	CR_FIRST_FOUND      = 0x10,
	CR_BIAS_HORIZONTAL  = 0x20,
	CR_BIAS_VERTICAL    = 0x40,
	CR_DIAGONAL_BALANCE = 0x80
};

struct nearness_factor {
	s32 x;
	s32 y;
};

/*
 * Area info kept
 */
struct area_spec {
	struct tcm_area area;
	struct list_head list;
};

/*
 * Everything is a rectangle with four sides 	and on
 * each side you could have a boundary or another Tile.
 * The tile could be Occupied or Not. These info is stored
 */
struct neighbour_stats {
	u16 left_boundary;
	u16 left_occupied;
	u16 top_boundary;
	u16 top_occupied;
	u16 right_boundary;
	u16 right_occupied;
	u16 bottom_boundary;
	u16 bottom_occupied;
};

struct slot {
	u8 busy;		/* is slot occupied */
	struct tcm_area parent; /* parent area */
	u32 reserved;
};

struct sita_pvt {
	u16 width;
	u16 height;
	struct list_head res;	/* all allocations */
	struct mutex mtx;
	struct tcm_pt div_pt;	/* divider point splitting container */
	struct slot **map;	/* container slots */
};

#endif /* _TCM_SITA_H_ */
