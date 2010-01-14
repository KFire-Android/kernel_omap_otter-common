#ifndef TILER_DEP_H
#define TILER_DEP_H

/** @enum errorCodeT
* Defining enumarated identifiers for general dmm driver errors. */
enum errorCodeT {
	DMM_NO_ERROR,
	DMM_WRONG_PARAM,
	DMM_HRDW_CONFIG_FAILED,
	DMM_HRDW_NOT_READY,
	DMM_SYS_ERROR
};

#define DMM_ALIAS_VIEW_CLEAR    (~0xE0000000)

#define DMM_TILE_DIMM_X_MODE_8    (32)
#define DMM_TILE_DIMM_Y_MODE_8    (32)

#define DMM_TILE_DIMM_X_MODE_16    (32)
#define DMM_TILE_DIMM_Y_MODE_16    (16)

#define DMM_TILE_DIMM_X_MODE_32    (16)
#define DMM_TILE_DIMM_Y_MODE_32    (16)

#define DMM_PAGE_DIMM_X_MODE_8    (DMM_TILE_DIMM_X_MODE_8*2)
#define DMM_PAGE_DIMM_Y_MODE_8    (DMM_TILE_DIMM_Y_MODE_8*2)

#define DMM_PAGE_DIMM_X_MODE_16    (DMM_TILE_DIMM_X_MODE_16*2)
#define DMM_PAGE_DIMM_Y_MODE_16    (DMM_TILE_DIMM_Y_MODE_16*2)

#define DMM_PAGE_DIMM_X_MODE_32    (DMM_TILE_DIMM_X_MODE_32*2)
#define DMM_PAGE_DIMM_Y_MODE_32    (DMM_TILE_DIMM_Y_MODE_32*2)

#define DMM_HOR_X_ADDRSHIFT_8            (0)
#define DMM_HOR_X_ADDRMASK_8            (0x3FFF)
#define DMM_HOR_X_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_HOR_X_ADDRSHIFT_8) & DMM_HOR_X_ADDRMASK_8)
#define DMM_HOR_X_PAGE_COOR_GET_8(x)\
				(DMM_HOR_X_COOR_GET_8(x)/DMM_PAGE_DIMM_X_MODE_8)

#define DMM_HOR_Y_ADDRSHIFT_8            (14)
#define DMM_HOR_Y_ADDRMASK_8            (0x1FFF)
#define DMM_HOR_Y_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_HOR_Y_ADDRSHIFT_8) & DMM_HOR_Y_ADDRMASK_8)
#define DMM_HOR_Y_PAGE_COOR_GET_8(x)\
				(DMM_HOR_Y_COOR_GET_8(x)/DMM_PAGE_DIMM_Y_MODE_8)

#define DMM_HOR_X_ADDRSHIFT_16            (1)
#define DMM_HOR_X_ADDRMASK_16            (0x7FFE)
#define DMM_HOR_X_COOR_GET_16(x)        (((unsigned long)x >> \
				DMM_HOR_X_ADDRSHIFT_16) & DMM_HOR_X_ADDRMASK_16)
#define DMM_HOR_X_PAGE_COOR_GET_16(x)    (DMM_HOR_X_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_X_MODE_16)

#define DMM_HOR_Y_ADDRSHIFT_16            (15)
#define DMM_HOR_Y_ADDRMASK_16            (0xFFF)
#define DMM_HOR_Y_COOR_GET_16(x)        (((unsigned long)x >>                  \
				DMM_HOR_Y_ADDRSHIFT_16) & DMM_HOR_Y_ADDRMASK_16)
#define DMM_HOR_Y_PAGE_COOR_GET_16(x)    (DMM_HOR_Y_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_Y_MODE_16)

#define DMM_HOR_X_ADDRSHIFT_32            (2)
#define DMM_HOR_X_ADDRMASK_32            (0x7FFC)
#define DMM_HOR_X_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_HOR_X_ADDRSHIFT_32) & DMM_HOR_X_ADDRMASK_32)
#define DMM_HOR_X_PAGE_COOR_GET_32(x)    (DMM_HOR_X_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_X_MODE_32)

#define DMM_HOR_Y_ADDRSHIFT_32            (15)
#define DMM_HOR_Y_ADDRMASK_32            (0xFFF)
#define DMM_HOR_Y_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_HOR_Y_ADDRSHIFT_32) & DMM_HOR_Y_ADDRMASK_32)
#define DMM_HOR_Y_PAGE_COOR_GET_32(x)    (DMM_HOR_Y_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_Y_MODE_32)

/* :TODO: to be determined */

#define DMM_VER_X_ADDRSHIFT_8            (14)
#define DMM_VER_X_ADDRMASK_8            (0x1FFF)
#define DMM_VER_X_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_VER_X_ADDRSHIFT_8) & DMM_VER_X_ADDRMASK_8)
#define DMM_VER_X_PAGE_COOR_GET_8(x)\
				(DMM_VER_X_COOR_GET_8(x)/DMM_PAGE_DIMM_X_MODE_8)

#define DMM_VER_Y_ADDRSHIFT_8            (0)
#define DMM_VER_Y_ADDRMASK_8            (0x3FFF)
#define DMM_VER_Y_COOR_GET_8(x)\
	(((unsigned long)x >> DMM_VER_Y_ADDRSHIFT_8) & DMM_VER_Y_ADDRMASK_8)
#define DMM_VER_Y_PAGE_COOR_GET_8(x)\
				(DMM_VER_Y_COOR_GET_8(x)/DMM_PAGE_DIMM_Y_MODE_8)

#define DMM_VER_X_ADDRSHIFT_16            (14)
#define DMM_VER_X_ADDRMASK_16            (0x1FFF)
#define DMM_VER_X_COOR_GET_16(x)        (((unsigned long)x >>                  \
				DMM_VER_X_ADDRSHIFT_16) & DMM_VER_X_ADDRMASK_16)
#define DMM_VER_X_PAGE_COOR_GET_16(x)    (DMM_VER_X_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_X_MODE_16)

#define DMM_VER_Y_ADDRSHIFT_16            (0)
#define DMM_VER_Y_ADDRMASK_16            (0x3FFF)
#define DMM_VER_Y_COOR_GET_16(x)        (((unsigned long)x >>                  \
				DMM_VER_Y_ADDRSHIFT_16) & DMM_VER_Y_ADDRMASK_16)
#define DMM_VER_Y_PAGE_COOR_GET_16(x)    (DMM_VER_Y_COOR_GET_16(x) /           \
				DMM_PAGE_DIMM_Y_MODE_16)

#define DMM_VER_X_ADDRSHIFT_32            (15)
#define DMM_VER_X_ADDRMASK_32            (0xFFF)
#define DMM_VER_X_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_VER_X_ADDRSHIFT_32) & DMM_VER_X_ADDRMASK_32)
#define DMM_VER_X_PAGE_COOR_GET_32(x)    (DMM_VER_X_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_X_MODE_32)

#define DMM_VER_Y_ADDRSHIFT_32            (0)
#define DMM_VER_Y_ADDRMASK_32            (0x7FFF)
#define DMM_VER_Y_COOR_GET_32(x)        (((unsigned long)x >>                  \
				DMM_VER_Y_ADDRSHIFT_32) & DMM_VER_Y_ADDRMASK_32)
#define DMM_VER_Y_PAGE_COOR_GET_32(x)    (DMM_VER_Y_COOR_GET_32(x) /           \
				DMM_PAGE_DIMM_Y_MODE_32)

#include "tiler.h"

struct node {
	struct tiler_buf_info *ptr;
	unsigned long reserved;
	struct node *nextnode;
};

int createlist(struct node **listhead);
int addnode(struct node *listhead, struct tiler_buf_info *ptr);
int removenode(struct node *listhead, int offset);
int tiler_destroy_buf_info_list(struct node *listhead);
int tiler_get_buf_info(struct node *listhead, struct tiler_buf_info **pp, int offst);

#endif

