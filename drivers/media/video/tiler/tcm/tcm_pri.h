#ifndef _TCM_PRIVATE_H_
#define _TCM_PRIVATE_H_

#define IN
#define OUT
#define INOUT

#define MAX_X_DIMMENSION        256
#define MAX_Y_DIMMENSION        128

#define YES             1
#define NO              0

#define TL_CORNER       0
#define TR_CORNER       1
#define BL_CORNER       3
#define BR_CORNER       4


/*Note Alignment 64 gets the highest priority */
#define ALIGN_STRIDE(align)((align == ALIGN_64) ? 64 : ((align == ALIGN_32) ? \
					32 :  ((align == ALIGN_16) ? 16 : 1)))

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

enum alignment {
	ALIGN_NONE = 0,
	/*ALIGN_16 = 0x1,*/
	ALIGN_32 = 0x1, /*0x2 */
	ALIGN_64 = 0x2, /*0x4 */
	ALIGN_16 = 0x3 /* 0x1*/
};

enum dim_type {
	ONE_D = 1,
	TWO_D = 2
};


struct area_spec {
	u16 x0;
	u16 y0;
	u16 x1;
	u16 y1;
};


struct area_spec_list;

struct area_spec_list {
	struct area_spec area;
	u16 area_type;
	struct area_spec_list *next;
};


/*Everything is a rectangle with four sides 	and on
 * each side you could have a boundaryor another Tile.
 * The tile could be Occupied or Not. These info is stored
 */
struct neighbour_stats {
	/* num of tiles on left touching the boundary */
	u16 left_boundary;
	/* num of tiles on left that are occupied */
	u16 left_occupied;

	u16 top_boundary;
	u16 top_occupied;

	u16 right_boundary;
	u16 right_occupied;

	u16 bottom_boundary;
	u16 bottom_occupied;
};

#endif
