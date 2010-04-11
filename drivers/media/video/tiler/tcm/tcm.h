/*
 * tcm.h
 *
 *  Created on: Mar 1, 2010
 *      Authors: Ravikiran Ramachandra, Lajos Molnar
 *
 *		Revision History:
 *		01/3/10: Defined basic structs,func ptr based 'struct tcm'
 *				 Defined basic APIs - Ravi
 *		17/3/10: Modified APIs with error checking, added additional
 *				 field struct tmc * to tcm_area.
 *				 Added 'tcm_slice' to retrieve topmost 2D slice
 *				 Added 'tcm_area_is_valid' functionality - Lajos
 *
 */

#ifndef _TCM_H_
#define _TCM_H_

#include <linux/init.h>
#include <linux/module.h>

#define TCM_1D 1 /* old:0 */
#define TCM_2D 2 /* old:1 */

#define ALIGN_NONE	0x0
#define ALIGN_32	0x1
#define ALIGN_64	0x2
#define ALIGN_16	0x3


struct tcm;

struct tcm_pt {
	u16 x;
	u16 y;
};

struct tcm_area {
	int type;		/* TCM_1D or 2D */
	struct tcm    *tcm;	/* parent */
	struct tcm_pt  p0;
	struct tcm_pt  p1;
};

struct tcm {
	u16 width, height;	/* container dimensions */

	/* 'pvt' structure shall contain any tcm details (attr) along with
	linked list of allocated areas and mutex for mutually exclusive access
	to the list.  It may also contain copies of width and height to notice
	any changes to the publicly available width and height fields. */
	void *pvt;

	/* function table */
	s32 (*reserve_2d)(struct tcm *tcm, u16 height, u16 width, u8 align,
			  struct tcm_area *area);
	s32 (*reserve_1d)(struct tcm *tcm, u32 slots, struct tcm_area *area);
	s32 (*free)      (struct tcm *tcm, struct tcm_area *area);
	s32 (*get_parent)(struct tcm *tcm, struct tcm_pt *pt,
			  struct tcm_area *area);
	s32 (*deinit)    (struct tcm *tcm);
};

/*=============================================================================
    BASIC TILER CONTAINER MANAGER INTERFACE
=============================================================================*/

/**
 * Template for <ALGO_NAME>_tcm_init method.  Define as:
 * TCM_INIT(<ALGO_NAME>_tcm_init)
 *
 * Allocates and initializes a tiler container manager.
 *
 * @param width		Width of container
 * @param height	Height of container
 * @param attr		Container manager specific configuration
 *  			arguments.  Please describe these in
 *  			your header file.
 *
 * @return Pointer to the allocated and initialized container
 *  	   manager.  NULL on failure.  DO NOT leak any memory on
 *  	   failure!
 */
#define TCM_INIT(name) \
struct tcm *name(u16 width, u16 height, void *attr);

/**
 * Deinitialize tiler container manager.
 *
 * @author Ravi Ramachandra (3/1/2010)
 *
 * @param tcm	Pointer to container manager.
 *
 * @return 0 on success, non-0 error value on error.  The call
 *  	   should free as much memory as possible and meaningful
 *  	   even on failure.  Some error codes: -ENODEV: invalid
 *  	   manager.
 */
static inline s32 tcm_deinit(struct tcm *tcm)
{
	if (tcm)
		return tcm->deinit(tcm);
	else
		return -ENODEV;
}

/**
 * Reserves a 2D area in the container.
 *
 * @author Ravi Ramachandra (3/1/2010)
 *
 * @param tcm		Pointer to container manager.
 * @param height	Height(in pages) of area to be reserved.
 * @param width		Width(in pages) of area to be reserved.
 * @param align		Alignment requirement for top-left corner of area. Not
 *  			all values may be supported by the container manager,
 * 			but it must support 0 (1), 32 and 64.
 *  			0 value is equivalent to 1.
 * @param area		Pointer to where the reserved area should be stored.
 *
 * @return 0 on success.  Non-0 error code on failure.  Also,
 *  	   the tcm field of the area will be set to NULL on
 *  	   failure.  Some error codes: -ENODEV: invalid manager,
 *  	   -EINVAL: invalid area, -ENOMEM: not enough space for
 *  	    allocation.
 */
static inline s32 tcm_reserve_2d(struct tcm *tcm, u16 width, u16 height,
				 u16 align, struct tcm_area *area)
{
	/* perform rudimentary error checking */
	s32 res = (tcm  == NULL ? -ENODEV :
		   area == NULL ? -EINVAL :
		   (height > tcm->height || width > tcm->width) ? -ENOMEM :
		   tcm->reserve_2d(tcm, height, width, align, area));

	if (area)
		area->tcm = res ? NULL : tcm;

	return res;
}

/**
 * Reserves a 1D area in the container.
 *
 * @author Ravi Ramachandra (3/1/2010)
 *
 * @param tcm		Pointer to container manager.
 * @param slots		Number of (contiguous) slots to reserve.
 * @param area		Pointer to where the reserved area should be stored.
 *
 * @return 0 on success.  Non-0 error code on failure.  Also,
 *  	   the tcm field of the area will be set to NULL on
 *  	   failure.  Some error codes: -ENODEV: invalid manager,
 *  	   -EINVAL: invalid area, -ENOMEM: not enough space for
 *  	    allocation.
 */
static inline s32 tcm_reserve_1d(struct tcm *tcm, u32 slots,
				 struct tcm_area *area)
{
	/* perform rudimentary error checking */
	s32 res = (tcm  == NULL ? -ENODEV :
		   area == NULL ? -EINVAL :
		   slots > (tcm->width * (u32) tcm->height) ? -ENOMEM :
		   tcm->reserve_1d(tcm, slots, area));

	if (area)
		area->tcm = res ? NULL : tcm;

	return res;
}

/**
 * Free a previously reserved area from the container.
 *
 * @author Ravi Ramachandra (3/1/2010)
 *
 * @param area	Pointer to area reserved by a prior call to
 *  		tcm_reserve_1d or tcm_reserve_2d call, whether
 *  		it was successful or not. (Note: all fields of
 *  		the structure must match.)
 *
 * @return 0 on success.  Non-0 error code on failure.  Also, the tcm
 *  	   field of the area is set to NULL on success to avoid subsequent
 *  	   freeing.  This call will succeed even if supplying
 *  	   the area from a failed reserved call.
 */
static inline s32 tcm_free(struct tcm_area *area)
{
	s32 res = 0; /* free succeeds by default */

	if (area && area->tcm) {
		res = area->tcm->free(area->tcm, area);
		if (res == 0)
			area->tcm = NULL;
	}

	return res;
}


/**
 * Retrieves the parent area (1D or 2D) for a given co-ordinate in the
 * container.
 *
 * @author Ravi Ramachandra (3/1/2010)
 *
 * @param tcm		Pointer to container manager.
 * @param pt		Pointer to the coordinates of a slot in the container.
 * @param area		Pointer to where the reserved area should be stored.
 *
 * @return 0 on success.  Non-0 error code on failure.  Also,
 *  	   the tcm field of the area will be set to NULL on
 *  	   failure.  Some error codes: -ENODEV: invalid manager,
 *  	   -EINVAL: invalid area, -ENOENT: coordinate is not part of any
 * 	   active area.
 */
static inline s32 tcm_get_parent(struct tcm *tcm, struct tcm_pt *pt,
				 struct tcm_area *area)
{
	s32 res = (tcm  == NULL ? -ENODEV :
		   area == NULL ? -EINVAL :
		   (pt->x >= tcm->width || pt->y >= tcm->height) ? -ENOENT :
		   tcm->get_parent(tcm, pt, area));

	if (area)
		area->tcm = res ? NULL : tcm;

	return res;
}

/*=============================================================================
    HELPER FUNCTION FOR ANY TILER CONTAINER MANAGER
=============================================================================*/

/**
 * This method slices off the topmost 2D slice from the parent area, and stores
 * it in the 'slice' parameter.  The 'parent' parameter will get modified to
 * contain the remaining portion of the area.  If the whole parent area can
 * fit in a 2D slice, its tcm pointer is set to NULL to mark that it is no
 * longer a valid area.
 *
 * @author Lajos Molnar (3/17/2010)
 *
 * @param parent	Pointer to a VALID parent area that will get modified
 * @param slice		Pointer to the slice area that will get modified
 */
static inline void tcm_slice(struct tcm_area *parent, struct tcm_area *slice)
{
	*slice = *parent;

	/* check if we need to slice */
	if (slice->tcm && slice->type == TCM_1D &&
		slice->p0.y != slice->p1.y &&
		(slice->p0.x || (slice->p1.x != slice->tcm->width - 1))) {
		/* set end point of slice (start always remains) */
		slice->p1.x = slice->tcm->width - 1;
		slice->p1.y = (slice->p0.x) ? slice->p0.y : slice->p1.y - 1;
		/* adjust remaining area */
		parent->p0.x = 0;
		parent->p0.y = slice->p1.y + 1;
	} else {
		/* mark this as the last slice */
		parent->tcm = NULL;
	}
}

/**
 * Verifies if a tcm area is logically valid.
 *
 * @param area		Pointer to tcm area
 *
 * @return TRUE if area is logically valid, FALSE otherwise.
 */
static inline bool tcm_area_is_valid(struct tcm_area *area)
{
	return (area && area->tcm &&
		/* coordinate bounds */
		area->p1.x < area->tcm->width &&
		area->p1.y < area->tcm->height &&
		area->p0.y <= area->p1.y &&
		/* 1D coordinate relationship + p0.x check */
		((area->type == TCM_1D &&
		  area->p0.x < area->tcm->width &&
		  area->p0.x + area->p0.y * area->tcm->width <=
		  area->p1.x + area->p1.y * area->tcm->width) ||
		 /* 2D coordinate relationship */
		 (area->type == TCM_2D &&
		  area->p0.x <= area->p1.x))
	       );
}

/* calculate number of slots in an area */
static inline u16 __tcm_sizeof(struct tcm_area *area)
{
	return (area->type == TCM_2D ?
		(area->p1.x - area->p0.x + 1) * (area->p1.y - area->p0.y + 1) :
		(area->p1.x - area->p0.x + 1) + (area->p1.y - area->p0.y) *
							area->tcm->width);
}
#define tcm_sizeof(area) __tcm_sizeof(&(area))

/**
 * Iterate through 2D slices of a valid area. Behaves
 * syntactically as a for(;;) statement.
 *
 * @param var		Name of a local variable of type 'struct
 *  			tcm_area *' that will get modified to
 *  			contain each slice.
 * @param area		Pointer to the VALID parent area. This
 *  			structure will not get modified
 *  			throughout the loop.
 *
 */
#define tcm_for_each_slice(var, area, safe) \
	for (safe = area, \
	     tcm_slice(&safe, &var); \
	     var.tcm; tcm_slice(&safe, &var))

#endif /* _TCM_H_ */
