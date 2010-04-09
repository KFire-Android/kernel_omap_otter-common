/*
 * tcm_rr.h
 *
 *  Created on: Mar 25, 2010
 *      Author: Ravikiran Ramachandra
 *
 *      This header describes the private structures required
 *      for the implementation of (SImple Tiler Allocator) SiTA Algo.
 */

#ifndef TCM_RR_H_
#define TCM_RR_H_

#include "tcm.h"
#include "tcm_dbg.h"

#define IN
#define OUT
#define INOUT

#define YES             1
#define NO              0

#define OCCUPIED        YES
#define NOT_OCCUPIED    NO

#define FIT             YES
#define NO_FIT          NO

#define TL_CORNER       0
#define TR_CORNER       1
#define BL_CORNER       3
#define BR_CORNER       4

/*Note Alignment 64 gets the highest priority */
#define ALIGN_STRIDE(align)	((align == ALIGN_64) ? 64 : \
				((align == ALIGN_32) ? 32 : \
				((align == ALIGN_16) ? 16 : 1)))

/*Provide inclusive length between co-ordinates */
#define INCL_LEN(high, low)		(high - low + 1)
#define INCL_LEN_MOD(start, end)	((start > end) ? (start-end + 1) : \
		(end - start + 1))

#define TOTAL_BOUNDARY(stat) ((stat)->top_boundary + (stat)->bottom_boundary + \
				(stat)->left_boundary + (stat)->right_boundary)
#define TOTAL_OCCUPIED(stat) ((stat)->top_occupied + (stat)->bottom_occupied + \
				(stat)->left_occupied + (stat)->right_occupied)


#define MUTEX_LOCK(m)   (mutex_lock(m))
#define MUTEX_REL(m)    (mutex_unlock(m))


enum tiler_error {
	TilerErrorNone = 0,
	TilerErrorGeneral = -1,
	TilerErrorInvalidDimension = -2,
	TilerErrorNoRoom = -3,
	TilerErrorInvalidArg = -4,
	TilerErrorMatchNotFound = -5,
	TilerErrorOverFlow = -6,
	TilerErrorInvalidScanArea = -7,
	TilerErrorNotSupported = -8,
};

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

/*
 * Linked list structure
 */
struct area_spec_list;

struct area_spec_list {
	struct tcm_area area;
	u16 area_type;
	struct area_spec_list *next;
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

struct tiler_page {
	/* is tiler_page Occupied */
	u8 is_occupied;
	/* Parent area to which this tiler_page belongs */
	struct tcm_area parent_area;
	/* 1D or 2D */
	u16 type;
	u32 reserved;
};


struct sita_pvt {
	u16 width;
	u16 height;
	/* This list keeps a track of all the allocations that happened in terms
	of area allocated, this list is checked for removing any allocations */
	struct area_spec_list *res_list;
	/* mutex */
	struct mutex mtx;
	/* Divider point splitting the tiler container
	horizontally and vertically */
	struct tcm_pt div_pt;
	/*map of busy/non busy tiler of a given container*/
	struct tiler_page **tcm_map;
	/*allocation counter, keeps rolling with */
	u32 id;
};

#endif /* TCM_RR_H_ */
