/*
 * tcm.c
 *
 * Author: Ravi Ramachandra <r.ramachandra@ti.com>
 *
 * SImple Tiler Allocator (SiTA): 2D and 1D allocation(reservation) algorithm
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
 *
 */

#include "tcm_rr.h"
#include "tcm_sita.h"
#include "tcm_utils.h"

#define X_SCAN_LIMITER	1
#define Y_SCAN_LIMITER	1

/* Individual selection criteria for different scan areas */
static s32 g_scan_criteria_l2r_t2b = CR_BIAS_HORIZONTAL;
static s32 g_scan_criteria_r2l_t2b = CR_DIAGONAL_BALANCE;
#ifdef SCAN_BOTTOM_UP
static s32 g_scan_criteria_r2l_b2t = CR_FIRST_FOUND;
static s32 g_scan_criteria_l2r_b2t = CR_DIAGONAL_BALANCE;
#endif

/*********************************************
 *	TCM API - Sita Implementation
 *********************************************/
static s32 sita_reserve_2d(struct tcm *tcm, u16 h, u16 w, u8 align,
		    struct tcm_area *allocated_area);
static s32 sita_reserve_1d(struct tcm *tcm, u32 slots, struct tcm_area
		    *allocated_pages);
static s32 sita_free(struct tcm *tcm, struct tcm_area *to_be_removed_area);
static s32 sita_get_parent(struct tcm *tcm, struct tcm_pt *pt,
			struct tcm_area *area);
static s32 sita_deinit(struct tcm *tcm);

/*********************************************/


/*********************************************
 *	Main Scanner functions
 *********************************************/
static s32 scan_areas_and_find_fit(struct tcm *tcm, u16 w, u16 h, u16 stride,
				   struct tcm_area *allocated_area);

static s32 scan_l2r_t2b(struct tcm *tcm, u16 w, u16 h,
			u16 stride, struct tcm_area *scan_area,
			struct tcm_area *alloc_area);

static s32 scan_r2l_t2b(struct tcm *tcm, u16 w, u16 h, u16 stride,
			struct tcm_area *scan_area,
			struct tcm_area *alloc_area);

#ifdef SCAN_BOTTOM_UP
static s32 scan_l2r_b2t(struct tcm *tcm, u16 w, u16 h, u16 stride,
			struct tcm_area *scan_area,
			struct tcm_area *alloc_area);

static s32 scan_r2l_b2t(struct tcm *tcm, u16 w, u16 h, u16 stride,
			struct tcm_area *scan_area,
			struct tcm_area *alloc_area);
#endif
static s32 scan_r2l_b2t_one_dim(struct tcm *tcm, u32 num_of_pages,
			struct tcm_area *scan_area,
			struct tcm_area *alloc_area);


/*********************************************
 *	Support Infrastructure Methods
 *********************************************/
static s32 check_fit_r_and_b(struct tcm *tcm, u16 w, u16 h, u16 left_x,
			     u16 top_y);

static s32 check_fit_r_one_dim(struct tcm *tcm, u16 x, u16 y, u32 num_of_pages,
			       u16 *busy_x, u16 *busy_y);

static s32 select_candidate(struct tcm *tcm, IN u16 w, IN u16 h,
			IN u16 num_short_listed,
			IN struct area_spec_list *short_listed,
			IN struct tcm_area *scan_area, IN s32 criteria,
			OUT struct tcm_area *alloc_area);

static s32 get_nearness_factor(struct tcm_area *scan_area,
		       struct tcm_area *candidate, struct nearness_factor *nf);

static s32 get_busy_neigh_stats(struct tcm *tcm, u16 width, u16 height,
			struct tcm_area *top_left_corner,
			struct neighbour_stats *neighbour_stat);

static s32 insert_pages_with_tiler_page(struct tcm *tcm,
			struct tcm_area *area, struct tiler_page tile);

static s32 insert_area_with_tiler_page(struct tcm *tcm,
		       struct tcm_area *area, struct tiler_page tile);

static s32 move_left(struct tcm *tcm, u16 x, u16 y, u32 num_of_pages,
		     u16 *xx, u16 *yy);
static s32 move_right(struct tcm *tcm, u16 x, u16 y, u32 num_of_pages,
		      u16 *xx, u16 *yy);
/*********************************************/


struct tcm *sita_init(u16 width, u16 height, void *attr)
{
	struct tcm *tmp = NULL;
	struct sita_pvt *pvt = NULL;
	struct tiler_page init_tile;
	struct tcm_area area;
	struct tcm_pt *tmp_pt = (struct tcm_pt *)attr;
	s32 i = 0;
	memset(&area, 0, sizeof(struct tcm_area));

	if (width == 0 ||  height == 0) {
		PE("width/height is Zero\n");
		return tmp;
	}

	tmp = kmalloc(sizeof(struct tcm), GFP_KERNEL);
	if (tmp == NULL)
		return tmp;

	/* TO DO: Do i need to restrict width and height??? */
	init_tile.is_occupied = 0;
	init_tile.parent_area.p0.x = 0;
	init_tile.parent_area.p1.y = 0;
	init_tile.parent_area.p1.x = 0;
	init_tile.parent_area.p1.y = 0;
	init_tile.reserved = 0;
	init_tile.type = 0;

	pvt = kmalloc(sizeof(struct sita_pvt), GFP_KERNEL);
	if (pvt == NULL)	{
		kfree(tmp);
		tmp = NULL;
		return tmp;
	}

	memset(pvt, 0, sizeof(struct sita_pvt));
	pvt->res_list = NULL;

	/*Updating the pointers to SiTA implementation APIs*/
	tmp->height = height;
	tmp->width = width;
	tmp->reserve_2d = sita_reserve_2d;
	tmp->reserve_1d = sita_reserve_1d;
	tmp->get_parent = sita_get_parent;
	tmp->free = sita_free;
	tmp->deinit = sita_deinit;
	tmp->pvt = (void *)pvt;
	pvt->height = height;
	pvt->width = width;

	mutex_init(&(pvt->mtx));
	pvt->tcm_map = NULL;

	/* Creating tam map */
	pvt->tcm_map = (struct tiler_page **)
	       kmalloc(sizeof(struct tiler_page *) * pvt->width, GFP_KERNEL);

	if (pvt->tcm_map == NULL) {
		kfree(pvt);
		pvt = NULL;
		kfree(tmp);
		tmp = NULL;
		return tmp;
	}

	for (i = 0; i < pvt->width; ++i) {
		pvt->tcm_map[i] = NULL;
		pvt->tcm_map[i] = (struct tiler_page *)
		kmalloc(sizeof(struct tiler_page) * pvt->height, GFP_KERNEL);
		if (pvt->tcm_map[i] == NULL) {
			for (i -= 1; i >= 0; i--) {
				kfree(pvt->tcm_map[i]);
				pvt->tcm_map[i] = NULL;
			}
			kfree(pvt->tcm_map);
			pvt->tcm_map = NULL;
			kfree(pvt);
			pvt = NULL;
			kfree(tmp);
			tmp = NULL;
			return tmp;
		}
	}

	if (tmp_pt && tmp_pt->x < pvt->width && tmp_pt->y < pvt->height) {
		pvt->div_pt.x = tmp_pt->x;
		pvt->div_pt.y = tmp_pt->y;

	} else {
		/* Defaulting to 3:1 ratio on width for 2D area split */
		/* Defaulting to 3:1 ratio on height for 2D and 1D split */
		pvt->div_pt.x = (pvt->width*3)/4;
		pvt->div_pt.y = (pvt->height*3)/4;
	}

	memset(&area, 0, sizeof(struct tcm_area));
	area.p1.x = width-1;
	area.p1.y = height-1;

	MUTEX_LOCK(&(pvt->mtx));
	insert_area_with_tiler_page(tmp, &area, init_tile);
	MUTEX_REL(&(pvt->mtx));

	return tmp;
}


static s32 sita_deinit(struct tcm *tcm)
{
	struct tiler_page init_tile;
	struct sita_pvt *pvt = NULL;
	struct tcm_area area;
	s32 i = 0;

	init_tile.is_occupied = 0;
	init_tile.parent_area.p0.x = 0;
	init_tile.parent_area.p1.y = 0;
	init_tile.parent_area.p1.x = 0;
	init_tile.parent_area.p1.y = 0;
	init_tile.reserved = 0;
	init_tile.type = 0;

	pvt = (struct sita_pvt *)tcm->pvt;
	if (pvt == NULL) {
		PE("Private struct NULL\n");
		return TilerErrorGeneral;
	}

	memset(&area, 0, sizeof(struct tcm_area));
	area.p1.x = pvt->width-1;
	area.p1.y = pvt->height-1;

	MUTEX_LOCK(&(pvt->mtx));
	insert_area_with_tiler_page(tcm, &area, init_tile);
	MUTEX_REL(&(pvt->mtx));

	mutex_destroy(&(pvt->mtx));

	for (i = 0; i < pvt->height; i++) {
		kfree(pvt->tcm_map[i]);
		pvt->tcm_map[i] = NULL;
	}
	kfree(pvt->tcm_map);
	pvt->tcm_map = NULL;

	return TilerErrorNone;
}



/**
 * @description: Allocate 1d pages if the required number of pages are
 * available in the container
 *
 * @input:num_of_pages to be allocated
 *
 * @return 0 on success, non-0 error value on failure. On success
 * allocated_pages contain co-ordinates of start and end Tiles(inclusive)
 */
static s32 sita_reserve_1d(struct tcm *tcm, u32 num_of_pages,
		    struct tcm_area *allocated_pages)
{

	s32 ret = TilerErrorNone;
	struct tcm_area scan_area = {0, NULL, {0, 0}, {0, 0} };
	struct tiler_page tile;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	memset(&tile, 0, sizeof(struct tiler_page));
	P1("Allocate %d pages\n", num_of_pages);

	/* Basic checks */
	if (allocated_pages == NULL) {
		PE("NULL input found\n");
		return TilerErrorInvalidArg;
	}
	/* Delibrately checking outside to give out relavent error info */
	if (num_of_pages > (pvt->width * pvt->height)) {
		PE("num_of_pages exceed maximum pages available(%d)\n",
		   (pvt->width * pvt->height));
		return TilerErrorNoRoom;
	}
	MUTEX_LOCK(&(pvt->mtx));
#ifdef RESTRICT_1D
	/*scan within predefine 1D boundary */
	assign(&scan_area, (pvt->width - 1), (pvt->height - 1), 0,
	       tcm->div_spec.p1.y);
#else
	/* Scanning entire container */
	assign(&scan_area, pvt->width - 1, pvt->height - 1, 0, 0);
#endif
	ret = scan_r2l_b2t_one_dim(tcm, num_of_pages,
				   &scan_area, allocated_pages);
	/* There is not much to select, we pretty much give the first one which
								accomodates */
	if (ret != TilerErrorNone) {
		PE("Failed to Allocate 1D Pages\n");
	} else {
		P("Yahoo found a fit: %s\n", AREA_STR(a_str, allocated_pages));
		tile.is_occupied = OCCUPIED;
		assign(&tile.parent_area, allocated_pages->p0.x,
		       allocated_pages->p0.y, allocated_pages->p1.x,
		       allocated_pages->p1.y);
		/* some id, not useful now */
		tile.reserved = pvt->id++;
		/* Saying that type is 1d */
		tile.type = TCM_1D;
		/* inserting into tiler container */
		insert_pages_with_tiler_page(tcm, allocated_pages, tile);
		/*updating the list of allocations */
		insert_element(&pvt->res_list, allocated_pages, TCM_1D);
	}
	MUTEX_REL(&(pvt->mtx));
	return ret;
}


/**
 * @description: Allocate 2d area on availability in the container
 *
 * @input:'w'idth and 'h'eight of the 2d area, 'align'ment specification
 *
 * @return 0 on success, non-0 error value on failure. On success
 * allocated_area contain co-ordinates of TL corner Tile and BR corner Tile of
 * the rectangle (inclusive)
 */
static s32 sita_reserve_2d(struct tcm *tcm, u16 h, u16 w, u8 align,
		    struct tcm_area *allocated_area)
{
	s32 ret = TilerErrorNone;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	u16 stride = ALIGN_STRIDE(align);
	struct tiler_page tile;
	memset(&tile, 0, sizeof(struct tiler_page));
	P1("\n\nStart of allocation 2D Area for WxH : (%d x %d) with Alignment\
							%d \n", w, h, stride);

	/* Checking for input arguments */
	if (allocated_area == NULL || tcm == NULL) {
		PE("NULL input found\n");
		return TilerErrorInvalidArg;
	}
	/* ALIGN_16 is currently NOT supported*/
	if (align == ALIGN_16) {
		PE("Align 16 NOT supported \n");
		return TilerErrorNotSupported;
	}
	/*check if width and height are within limits */
	if (w > pvt->width || w == 0 || h > pvt->height || h == 0) {
		PE("Invalid dimension:: %d x %d\n", w, h);
		return TilerErrorInvalidDimension;
	}
	MUTEX_LOCK(&(pvt->mtx));
	ret = scan_areas_and_find_fit(tcm, w, h, stride, allocated_area);
	if (ret != TilerErrorNone) {
		PE("Did not find anything in the given area\n");
	} else {
		P("Yahoo found a fit: %s\n", AREA_STR(a_str, allocated_area));
		tile.is_occupied = OCCUPIED;
		assign(&tile.parent_area, allocated_area->p0.x,
		       allocated_area->p0.y, allocated_area->p1.x,
		       allocated_area->p1.y);

		/* some id, not useful now */
		tile.reserved = pvt->id++;
		/* Saying that type is 2D */
		tile.type = TCM_2D;
		/* inserting into tiler container */
		ret = insert_area_with_tiler_page(tcm, allocated_area, tile);
		if (ret == TilerErrorNone) {
			/*updating the list of allocations */
			insert_element(&(pvt->res_list),
				       allocated_area, TCM_2D);
		} else {
			PE("Could not insert area\n %s\n", AREA_STR(a_str,
					allocated_area));
		}
	}
	MUTEX_REL(&(pvt->mtx));
	return ret;
}

/**
 * @description: unreserve 2d or 1D allocations if previously allocated
 *
 * @input:'to_be_removed_area' specification: for 2D this should contain
 * TL Corner and BR Corner of the 2D area, or for 1D allocation this should
 * contain the start and end Tiles
 *
 * @return 0 on success, non-0 error value on failure. On success
 * the to_be_removed_area is removed from g_allocation_list and the
 * corresponding tiles are marked 'NOT_OCCUPIED'
 *
 */

static s32 sita_free(struct tcm *tcm, struct tcm_area *to_be_removed_area)
{
	s32 ret = TilerErrorNone;
	struct tiler_page reset_tile;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	u16 area_type;
	memset(&reset_tile, 0, sizeof(struct tiler_page));
	reset_tile.is_occupied = NOT_OCCUPIED;
	MUTEX_LOCK(&(pvt->mtx));
	/*First we check if the given Area is aleast valid in our list*/
	ret = rem_element_with_match(&(pvt->res_list), to_be_removed_area,
				     &area_type);

	/* If we found a positive match & removed the area details from list
	 * then we clear the contents of the associated tiles in the global
	 * container*/
	if (ret == TilerErrorNone) {
		if (area_type == TCM_2D) {
			P1("De-allocating TCM_2D allocation %s\n",
			   AREA_STR(a_str, to_be_removed_area));
			/*Reset tiles are inserted, ignoring ret values
			delibrately */
			ret = insert_area_with_tiler_page(tcm,
					  to_be_removed_area, reset_tile);
		} else {
			P1("De-allocating TCM_1D allocation %s\n",
			   AREA_STR(a_str, to_be_removed_area));
			ret = insert_pages_with_tiler_page(tcm,
					   to_be_removed_area, reset_tile);
		}
	} else {
		PE("Did not find a Match to remove\n");
	}
	MUTEX_REL(&(pvt->mtx));
	return ret;
}


/**
 * @description: raster scan right to left from top to bottom; find if there is
 * a free area to fit a given w x h inside the 'scan area'. If there is a free
 * area, then adds to short_listed candidates, which later is sent for selection
 * as per pre-defined criteria.
 *
 * @input:'w x h' width and height of the allocation area.
 * 'stride' - 64/32/None for start address alignment
 * 'scan_area' - area in which the scan operation should take place
 *
 * @return 0 on success, non-0 error value on failure. On success
 * the 'alloc_area' area contains TL and BR corners of the allocated area
 *
 */
s32 scan_r2l_t2b(struct tcm *tcm, u16 w, u16 h, u16 stride,
		 struct tcm_area *scan_area, struct tcm_area *alloc_area)
{

	s32 ret = TilerErrorNone;
	s32 xx = 0, yy = 0;
	s16 start_x = -1, end_x = -1, start_y = -1, end_y = -1;
	s16 found_x = -1, found_y = -1;
	u16 remainder;
	struct area_spec_list *short_listed = NULL;
	struct tcm_area candidate_area;
	u16 num_short_listed = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	memset(&candidate_area, 0, sizeof(struct tcm_area));
	P2("Scanning From Right 2 Left Top 2 Bottom: ScanArea: %s\n",
	   AREA_STR(a_str, scan_area));

	/*Basic checks */
	if (scan_area == NULL || alloc_area == NULL || tcm == NULL) {
		PE("Null value found\n");
		return TilerErrorInvalidArg;
	}

	/*Check if the scan area co-ordinates are valid */
	if ((scan_area->p0.x < scan_area->p1.x)  ||
			(scan_area->p1.y < scan_area->p0.y)) {
		PE("Invalid scan area: %s\n", AREA_STR(a_str, scan_area));
		return TilerErrorInvalidScanArea;
	}

	start_x = scan_area->p0.x;
	end_x = scan_area->p1.x;
	start_y = scan_area->p0.y;
	end_y = scan_area->p1.y;

	/* Check if we have a scan area bigger than the given input width and
	   height */
	if (w > INCL_LEN(start_x, end_x) || h >  INCL_LEN(end_y, start_y)) {
		PE("Scan area smaller than width and height\n");
		return TilerErrorInvalidDimension;
	}

	/*Adjusting start_x and end_y, why scan beyond a  point where we cant
	  allocate given wxh area */
	start_x = start_x - w + 1; /* - 1 to be inclusive */
	end_y = end_y - h + 1;


	/* calculating remainder */
	remainder = (start_x % stride);
	/* P("remainder = %d\n",remainder); */

	/* if start_x is not divisible by stride, then skip to PREV aligned
	   column */
	start_x -=  remainder ? (remainder) : 0 ;
	/* P("StartX = %d\n",start_x); */


	/* check if we have enough width to accomodate the request from the
	   aligned (start_y) column */
	if (start_x < end_x) {
		PE("No room to allocate at aligned lengths\n");
		return TilerErrorNoRoom;
	}

	P2("Final stride : %d\n, start_x : %d end_x : %d start_y :\
			%d end_y : %d\n", stride, start_x, end_x, start_y,
	   end_y);

	/* Start scanning: These scans are always inclusive ones  so if we are
	   given a start x = 0 is a valid value  so if we have a end_x = 255,
	   255th element is also checked
	*/
	for (yy = start_y; yy <= end_y; ++yy) {
		for (xx = start_x; xx >= end_x; xx -= stride) {
			if (pvt->tcm_map[xx][yy].is_occupied ==
					NOT_OCCUPIED) {
				if (FIT == check_fit_r_and_b(tcm, w, h,
							     xx, yy)) {
					P3("Found Free Shoulder at:\
							(%d, %d)\n", xx, yy);
					found_x = xx;
					found_y = yy;
					/* Insert this candidate, it is just a
						co-ordinate, reusing Area */
					assign(&candidate_area, xx, yy, 0, 0);
					insert_element(&short_listed,
						       &candidate_area, TCM_2D);
					num_short_listed++;
					/*changing upper bound on x direction */
#ifdef X_SCAN_LIMITER
					end_x = xx + 1;
#endif
					break;
				}
			} else {
				/* Optimization required only for Non Aligned,
				Aligned anyways skip by 32/64 tiles at a time */
				if (stride == 1 &&
						pvt->tcm_map[xx][yy].type ==
						TCM_2D) {
					xx = pvt->tcm_map
					     [xx][yy].parent_area.p0.x;
					P3("Moving to parent location start_x\
							(%d %d)\n", xx, yy);
				}
			}


		}

		/* if you find a free area shouldering the given scan area on
		then we can break
		*/
#ifdef Y_SCAN_LIMITER
		if (found_x == start_x)
			break;
#endif
	}


	if (!short_listed) {
		PE("No candidates found in a given scan area\n");
		return TilerErrorNoRoom;
	}

	/*Now if we are here it implies we have potential candidates*/
	ret = select_candidate(tcm, w, h, num_short_listed, short_listed,
			       scan_area, g_scan_criteria_r2l_t2b, alloc_area);

	if (ret != TilerErrorNone)
		PE("Error in Selecting a Candidate\n");
	/*Just clean up resources */
	clean_list(&short_listed);

	/* dump_list_entries(short_listed); */
	return ret;

}


#ifdef SCAN_BOTTOM_UP
/**
 * @description: raster scan right to left from bottom to top; find if there is
 * a free area to fit a given w x h inside the 'scan area'. If there is a free
 * area, then adds to short_listed candidates, which later is sent for selection
 * as per pre-defined criteria.
 *
 * @input:'w x h' width and height of the allocation area.
 * 'stride' - 64/32/None for start address alignment
 * 'scan_area' - area in which the scan operation should take place
 *
 * @return 0 on success, non-0 error value on failure. On success
 * the 'alloc_area' area contains TL and BR corners of the allocated area
 *
 */
static s32 scan_r2l_b2t(struct tcm *tcm, u16 w, u16 h, u16 stride,
			struct tcm_area *scan_area, struct tcm_area *alloc_area)
{
	s32 ret = TilerErrorNone;

	/* TO DO: Should i check scan area?
	Might have to take it as input during initialization
	*/
	s32 xx = 0, yy = 0;
	s16 start_x = -1, end_x = -1, start_y = -1, end_y = -1;
	s16 found_x = -1, found_y = -1;
	u16 remainder;
	struct area_spec_list *short_listed = NULL;
	struct tcm_area candidate_area;
	u16 num_short_listed = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	memset(&candidate_area, 0, sizeof(struct tcm_area));
	P2("Scanning From Right 2 Left Bottom 2 Top, ScanArea: %s\n",
	   AREA_STR(a_str, scan_area));
	start_x = scan_area->p0.x;
	end_x = scan_area->p1.x;
	start_y = scan_area->p0.y;
	end_y = scan_area->p1.y;


	/*Basic checks */
	if (scan_area == NULL || alloc_area == NULL || tcm == NULL) {
		PE("Null value found\n");
		return TilerErrorInvalidArg;
	}

	/*Check if the scan area co-ordinates are valid */
	if ((scan_area->p1.x < scan_area->p0.x)  ||
			(scan_area->p1.y < scan_area->p0.y)) {
		PE("Invalid scan area: %s\n", AREA_STR(a_str, scan_area));
		return TilerErrorInvalidScanArea;
	}

	/* Check if we have a scan area bigger than the given input width
	   and height */
	if (w > INCL_LEN(start_x, end_x) || h >  INCL_LEN(start_y, end_y)) {
		PE("Scan area smaller than width and height\n");
		return TilerErrorInvalidDimension;
	}

	/*Adjusting start_x and end_y, why scan beyond a  point where we cant
	  allocate given wxh area */
	start_x = start_x - w + 1; /* + 1 to be inclusive */
	start_y = start_y - h + 1;


	/* calculating remainder */
	remainder = (start_x % stride);
	/* P("remainder = %d\n",remainder); */

	/* if start_x is not divisible by stride, then skip to PREV aligned
	column */
	start_x -=  remainder ? (remainder) : 0 ;
	/* P("StartX = %d\n",start_x); */


	/* check if we have enough width to accomodate the request from the
	aligned (start_y) column */
	if (start_x < end_x) {
		PE("No room to allocate at aligned lengths\n");
		return TilerErrorNoRoom;
	}

	P2("Final stride : %d\n, start_x : %d end_x : %d start_y : %d end_y :\
				%d\n", stride, start_x, end_x, start_y, end_y);

	/* Start scanning: These scans are always inclusive ones  so if we are
	   given a start x = 0 is a valid value  so if we have a end_x = 255,
	   255th element is also checked
	*/
	for (yy = start_y; yy >= end_y; --yy) {
		for (xx = start_x; xx >= end_x; xx -= stride) {
			if (!pvt->tcm_map[xx][yy].is_occupied) {
				if (check_fit_r_and_b(tcm, w, h, xx, yy)
						== FIT) {
					P3("Found Free Shoulder at: (%d, %d)\n",
					   xx, yy);
					found_x = xx;
					found_y = yy;
					/* Insert this candidate, it is just a
						co-ordinate, reusing Area */
					assign(&candidate_area, xx, yy, 0, 0);
					insert_element(&short_listed,
						       &candidate_area, TCM_2D);
					num_short_listed++;
					/*changing upper bound on x direction */
#ifdef X_SCAN_LIMITER
					end_x = xx + 1;
#endif
					break;
				}
			} else {
				/* Optimization required only for Non Aligned,
				Aligned anyways skip by 32/64 tiles at a time */
				if (stride == 1 && pvt->tcm_map
						[xx][yy].type == TCM_2D) {
					xx = pvt->tcm_map
					     [xx][yy].parent_area.p0.x;
					P3("Moving to parent location start_x\
							(%d %d)\n", xx, yy);
				}
			}

		}

		/* if you find a free area shouldering the given scan area on
		then we can break
		*/
#ifdef Y_SCAN_LIMITER
		if (found_x == start_x)
			break;
#endif
	}


	if (!short_listed) {
		PE("No candidates found in a given scan area\n");
		return TilerErrorNoRoom;
	}

	/*Now if we are here it implies we have potential candidates*/
	ret = select_candidate(tcm, w, h, num_short_listed, short_listed,
			       scan_area, g_scan_criteria_r2l_b2t, alloc_area);

	if (ret != TilerErrorNone)
		PE("Error in Selecting a Candidate\n");
	/*Just clean up resources */
	clean_list(&short_listed);

	/* dump_list_entries(short_listed); */
	return ret;

}
#endif

/**
 * @description: raster scan left to right from top to bottom; find if there is
 * a free area to fit a given w x h inside the 'scan area'. If there is a free
 * area, then adds to short_listed candidates, which later is sent for selection
 * as per pre-defined criteria.
 *
 * @input:'w x h' width and height of the allocation area.
 * 'stride' - 64/32/None for start address alignment
 * 'scan_area' - area in which the scan operation should take place
 *
 * @return 0 on success, non-0 error value on failure. On success
 * the 'alloc_area' area contains TL and BR corners of the allocated area
 *
 */
s32 scan_l2r_t2b(struct tcm *tcm, u16 w, u16 h, u16 stride,
		 struct tcm_area *scan_area, struct tcm_area *alloc_area)
{
	s32 ret = TilerErrorNone;
	s32 xx = 0, yy = 0;
	s16 start_x = -1, end_x = -1, start_y = -1, end_y = -1;
	s16 found_x = -1, found_y = -1;
	u16 remainder;
	struct area_spec_list *short_listed = NULL;
	struct tcm_area candidate_area;
	u16 num_short_listed = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	memset(&candidate_area, 0, sizeof(struct tcm_area));
	P2("Scanning From Left 2 Right Top 2 Bottom, ScanArea: %s\n",
	   AREA_STR(a_str, scan_area));

	start_x = scan_area->p0.x;
	end_x = scan_area->p1.x;
	start_y = scan_area->p0.y;
	end_y = scan_area->p1.y;

	/*Basic checks */
	if (scan_area == NULL || alloc_area == NULL || tcm == NULL) {
		PE("Null value found\n");
		return TilerErrorInvalidArg;
	}

	/*Check if the scan area co-ordinates are valid */
	if ((scan_area->p1.x < scan_area->p0.x)  ||
			(scan_area->p1.y < scan_area->p0.y)) {
		PE("Invalid scan area: %s\n", AREA_STR(a_str, scan_area));
		return TilerErrorInvalidScanArea;
	}

	/* Check if we have a scan area bigger than the given input width and
	   height */
	if (w > INCL_LEN(end_x, start_x) || h > INCL_LEN(end_y, start_y)) {
		PE("Scan area smaller than width and height\n");
		return TilerErrorInvalidDimension;
	}

	/* calculating remainder */
	remainder = (start_x % stride);

	/* if start_x is not divisible by stride, then skip to next aligned
	   column */
	start_x +=  remainder ? (stride - remainder) : 0 ;

	/* check if we have enough width to accomodate the request from the
	   aligned (start_y) column */
	if (w > INCL_LEN(end_x, start_x)) {
		PE("No room to allocate at aligned lengths\n");
		return TilerErrorNoRoom;
	}

	/*Adjusting end_x and end_y, why scan beyond a  point where we cant
	  allocate given wxh area */
	end_x = end_x - w + 1; /* + 1 to be inclusive */
	end_y = end_y - h + 1;

	/* Just updating start and ends */

	/* P(" stride : %d\n, start_x : %d end_x : %d start_y : %d end_y : %d\n"
	   ,stride, start_x,end_x,start_y,end_y);*/

	/* Start scanning: These scans are always inclusive ones  so if we are
	   given a start x = 0 is a valid value  so if we have a end_x = 255,
	   255th element is also checked
	*/
	for (yy = start_y; yy <= end_y; ++yy) {
		for (xx = start_x; xx <= end_x; xx += stride) {
			/* if NOT occupied */
			if (pvt->tcm_map[xx][yy].is_occupied ==
					NOT_OCCUPIED) {
				if (check_fit_r_and_b(tcm, w, h, xx, yy)
						== FIT) {
					P3("Found Free Shoulder at: (%d, %d)\n",
					   xx, yy);
					found_x = xx;
					found_y = yy;
					/* Insert this candidate, it is just a
						co-ordinate, reusing Area */
					assign(&candidate_area, xx, yy, 0, 0);
					insert_element(&short_listed,
						       &candidate_area, TCM_2D);
					num_short_listed++;
					/*changing upper bound on x direction */
#ifdef X_SCAN_LIMITER
					end_x = xx - 1;
#endif
					break;
				}
			} else {
				/* Optimization required only for Non Aligned,
				Aligned anyways skip by 32/64 tiles at a time */
				if (stride == 1 && pvt->tcm_map
						[xx][yy].type == TCM_2D) {
					xx = pvt->tcm_map
					     [xx][yy].parent_area.p1.x;
					P3("Moving to parent location end_x\
							(%d %d)\n", xx, yy);
				}
			}
		}
		/* if you find a free area shouldering the given scan area on
		then we can break
		*/
#ifdef Y_SCAN_LIMITER
		if (found_x == start_x)
			break;
#endif
	}

	if (!short_listed) {
		PE("No candidates found in a given scan area\n");
		return TilerErrorNoRoom;
	}
	/*Now if we are here it implies we have potential candidates*/
	ret = select_candidate(tcm, w, h, num_short_listed, short_listed,
			       scan_area, g_scan_criteria_l2r_t2b, alloc_area);

	if (ret != TilerErrorNone)
		PE("Error in Selecting a Candidate\n");
	/*Just clean up resources */
	clean_list(&short_listed);
	/* dump_list_entries(short_listed); */
	return ret;
}

#ifdef SCAN_BOTTOM_UP
/**
 * @description: raster scan left to right from bottom to top; find if there is
 * a free area to fit a given w x h inside the 'scan area'. If there is a free
 * area, then adds to short_listed candidates, which later is sent for selection
 * as per pre-defined criteria.
 *
 * @input:'w x h' width and height of the allocation area.
 * 'stride' - 64/32/None for start address alignment
 * 'scan_area' - area in which the scan operation should take place
 *
 * @return 0 on success, non-0 error value on failure. On success
 * the 'alloc_area' area contains TL and BR corners of the allocated area
 *
 */
static s32 scan_l2r_b2t(struct tcm *tcm, u16 w, u16 h, u16 stride,
			struct tcm_area *scan_area, struct tcm_area *alloc_area)
{
	s32 ret = TilerErrorNone;
	s32 xx = 0, yy = 0;
	s16 start_x = -1, end_x = -1, start_y = -1, end_y = -1;
	s16 found_x = -1, found_y = -1;
	u16 remainder;
	struct area_spec_list *short_listed = NULL;
	struct tcm_area candidate_area;
	u16 num_short_listed = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	memset(&candidate_area, 0, sizeof(struct tcm_area));
	P2("Scanning From Left 2 Right Bottom 2 Top, ScanArea: %s\n",
	   AREA_STR(a_str, scan_area));

	start_x = scan_area->p0.x;
	end_x = scan_area->p1.x;
	start_y = scan_area->p0.y;
	end_y = scan_area->p1.y;

	/*Basic checks */
	if (scan_area == NULL || alloc_area == NULL || tcm == NULL) {
		PE("Null value found\n");
		return TilerErrorInvalidArg;
	}

	/*Check if the scan area co-ordinates are valid */
	if ((scan_area->p1.x < scan_area->p0.x)  ||
			(scan_area->p0.y < scan_area->p1.y)) {
		PE("Invalid scan area: %s\n", AREA_STR(a_str, scan_area));
		return TilerErrorInvalidScanArea;
	}

	/* Check if we have a scan area bigger than the given input width and
	   height */
	if (w > INCL_LEN(end_x, start_x) || h >  INCL_LEN(start_y, end_y)) {
		PE("Scan area smaller than width and height\n");
		return TilerErrorInvalidDimension;
	}

	/* calculating remainder */
	remainder = (start_x % stride);

	/* if start_x is not divisible by stride, then skip to next aligned
	   column */
	start_x +=  remainder ? (stride - remainder) : 0 ;

	/* check if we have enough width to accomodate the request from the
	aligned (start_x) column */
	if (w > INCL_LEN(end_x, start_x)) {
		PE("No room to allocate at aligned lengths\n");
		return TilerErrorNoRoom;
	}

	/*Adjusting end_x and end_y, why scan beyond a  point where we cant
	allocate given wxh area */
	end_x = end_x - w + 1; /* + 1 to be inclusive */
	start_y = start_y - h + 1;

	/* Just updating start and ends */

	P2(" stride : %d\n, start_x : %d end_x : %d start_y : %d end_y : %d\n",
	   stride, start_x, end_x, start_y, end_y);

	/* Start scanning: These scans are always inclusive ones  so if we are
	   given a start x = 0 is a valid value  so if we have a end_x = 255,
	   255th element is also checked
	*/
	for (yy = start_y; yy >= end_y; --yy) {
		for (xx = start_x; xx <= end_x; xx += stride) {
			/* if NOT occupied */
			if (!pvt->tcm_map[xx][yy].is_occupied) {
				if (check_fit_r_and_b(tcm, w, h, xx, yy)
						== FIT) {
					P3("Found Free Shoulder at: (%d, %d)\n",
					   xx, yy);
					found_x = xx;
					found_y = yy;
					/* Insert this candidate, it is just a
					 co-ordinate, reusing Area */
					assign(&candidate_area, xx, yy, 0, 0);
					insert_element(&short_listed,
						       &candidate_area, TCM_2D);
					num_short_listed++;
					/*changing upper bound on x direction */
#ifdef X_SCAN_LIMITER
					end_x = xx - 1;
#endif
					break;
				}
			} else {
				/* Optimization required only for Non Aligned,
				Aligned anyways skip by 32/64 tiles at a time */
				if (stride == 1 && pvt->tcm_map
						[xx][yy].type == TCM_2D) {
					xx = pvt->tcm_map
					     [xx][yy].parent_area.p1.x;
					P3("Moving to parent location end_x\
							(%d %d)\n", xx, yy);
				}
			}
		}

		/* if you find a free area shouldering the given scan area on
		then we can break
		*/
#ifdef Y_SCAN_LIMITER
		if (found_x == start_x)
			break;
#endif
	}

	if (!short_listed) {
		PE("No candidates found in a given scan area\n");
		return TilerErrorNoRoom;
	}

	/*Now if we are here it implies we have potential candidates*/
	ret = select_candidate(tcm, w, h, num_short_listed, short_listed,
			       scan_area, g_scan_criteria_l2r_b2t, alloc_area);

	if (ret != TilerErrorNone)
		PE("Error in Selecting a Candidate\n");
	/*Just clean up resources */
	clean_list(&short_listed);

	/* dump_list_entries(short_listed); */
	return ret;
}
#endif
/*
Note: In General the cordinates specified in the scan area area relevant to the
scan sweep directions. i.e A scan Area from Top Left Corner will have
p0.x <= p1.x and p0.y <= p1.y. Where as A scan Area from bottom Right Corner
will have p1.x <= p0.x and p1.y <= p0.y
*/

/**
 * @description: raster scan right to left from bottom to top; find if there are
 * continuous free pages(one tile is one page, continuity always from left to
 * right) inside the 'scan area'. If there are enough continous free pages,
 * then it returns the start and end Tile/page co-ordinates inside 'alloc_area'
 *
 * @input:'num_of_pages' required,
 * 'scan_area' - area in which the scan operation should take place
 *
 * @return 0 on success, non-0 error value on failure. On success
 * the 'alloc_area' area contains start and end tile (inclusive).
 *
 */
s32 scan_r2l_b2t_one_dim(struct tcm *tcm, u32 num_of_pages,
		 struct tcm_area *scan_area, struct tcm_area *alloc_area)
{
	s32 fit = NO_FIT;
	s32 ret = TilerErrorNone;
	u16 x, y;
	u16 left_x, left_y, busy_x, busy_y;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	/*Basic checks */
	if (scan_area == NULL || alloc_area == NULL || tcm == NULL) {
		PE("Null arguments found\n");
		return TilerErrorInvalidArg;
	}

	if (scan_area->p0.y < scan_area->p1.y) {
		PE("Invalid scan area: %s\n", AREA_STR(a_str, scan_area));
		return TilerErrorInvalidScanArea;
	}

	P2("Scanning From Right 2 Left Bottom 2 Top for 1D: ScanArea: %s\n",
	   AREA_STR(a_str, scan_area));

	/* Note: Checking sanctity of scan area
	 * The reason for checking this that 1D allocations assume that the X
	 ranges the entire TilerSpace X ie ALL Columns
	 * The scan area can limit only the Y ie, Num of Rows for 1D allocation.
	 We also expect we could have only 1 row for 1D allocation
	 * i.e our scan_area p0.y and p1.y may have a same value.
	 */

	/*
	Thinking Aloud: Should i even worry about width (X dimension)??
	Can't i just ignore X and then take the u16 (Y dimension) and assume X
					to range from MAX_X_DIMEN to 0 ??
	*/
	if (pvt->width != (1 + (scan_area->p0.x - scan_area->p1.x))) {
		PE("Not a valid Scan Area, for 1D the width should be entire\
			Tiler u16 (%d) but it is (%d)\n",
		   pvt->width, 1 + (scan_area->p0.x - scan_area->p1.x));
		return TilerErrorInvalidDimension;
	}


	/* checking if scan area can accomodate the num_of_pages */
	if (num_of_pages > pvt->width * INCL_LEN(scan_area->p0.y,
			scan_area->p1.y)) {
		PE("Num of Pages exceed Max possible (%d) for a given scan area\
			%s\n", pvt->width * (scan_area->p0.y - scan_area->p1.y),
		   AREA_STR(a_str, scan_area));
		return TilerErrorNoRoom;
	}

	/* Ah we are here, it implies we can try fitting now after we have
	checked everything */
	left_x = scan_area->p0.x;
	left_y = scan_area->p0.y;
	while (ret == TilerErrorNone) {
		x = left_x;
		y = left_y;

		/* P("Checking if (%d %d) is Occupied\n",x,y); */
		if (pvt->tcm_map[x][y].is_occupied == NOT_OCCUPIED) {
			ret = move_left(tcm, x, y, (num_of_pages - 1),
					&left_x, &left_y);
			if (ret == TilerErrorNone) {
				P3("Moved left to (%d %d) for num_of_pages (%d)\
					,checking for fit\n", left_x,
				   left_y, (num_of_pages - 1));
				fit = check_fit_r_one_dim(tcm, left_x, left_y,
					  num_of_pages, &busy_x, &busy_y);
				if (fit == FIT) {
					/*Implies we are fine, we found a place
							to put our 1D alloc */
					assign(alloc_area, left_x, left_y,
					       busy_x, busy_y);
					P3("Allocated 1D area: %s\n",
					   AREA_STR(a_str, alloc_area));
					break;
				} else {
					/* Implies it did not fit, the busy_x,
					busy_y will now be pointing to the Tile
					that was found busy/Occupied */
					x = busy_x;
					y = busy_y;
				}
			} else {
				PE("Error in Moving left: Error Code %d,\
							Breaking....\n", ret);
				break;
			}
		}

		/* Ah if we are here then we the Tile is occupied, now we might
		move to the start of parent locations */
		if (pvt->tcm_map[x][y].type == TCM_1D) {
			busy_x = pvt->tcm_map[x][y].parent_area.p0.x;
			busy_y = pvt->tcm_map[x][y].parent_area.p0.y;
		} else {
			busy_x = pvt->tcm_map[x][y].parent_area.p0.x;
			busy_y = y;
		}
		x = busy_x;
		y = busy_y;

		P3("Busy Tile found moving to ParentArea start :\
							(%d %d)\n", x, y);
		ret = move_left(tcm, x, y, 1, &left_x, &left_y);
	}

	if (!fit)
		ret = TilerErrorNoRoom;

	return ret;
}



/**
 * @description:
 *
 *
 *
 *
 * @input:
 *
 *
 * @return 0 on success, non-0 error value on failure. On success
 *
 *
 */
s32 scan_areas_and_find_fit(struct tcm *tcm, u16 w, u16 h, u16 stride,
			    struct tcm_area *allocated_area)
{
	s32 ret = TilerErrorGeneral;
	/* Checking for input arguments */
	/* No need to do this check, we have checked it in the parent call */
	struct tcm_area scan_area;
	u16 boundary_x = 0, boundary_y = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	s32 need_scan_flag = 2;
	memset(&scan_area, 0, sizeof(struct tcm_area));

	if (stride == 64 || stride == 32) {
		boundary_x = pvt->div_pt.x - 1;
		boundary_y = pvt->div_pt.y - 1;

		/* more intelligence here */
		if (w > pvt->div_pt.x) {
			boundary_x = pvt->width - 1;
			--need_scan_flag;
		}
		if (h > pvt->div_pt.y) {
			boundary_y = pvt->height - 1;
			--need_scan_flag;
		}

		assign(&scan_area, 0, 0, boundary_x, boundary_y);
		ret = scan_l2r_t2b(tcm, w, h, stride, &scan_area,
				   allocated_area);
		if (ret != TilerErrorNone && need_scan_flag) {
			/*Fall back Scan the entire Tiler area*/
			assign(&scan_area, 0, 0, pvt->width - 1,
			       pvt->height - 1);
			ret = scan_l2r_t2b(tcm, w, h, stride, &scan_area,
					   allocated_area);
		}
	} else if (stride == 1) {

		boundary_x = pvt->div_pt.x;
		boundary_y = pvt->div_pt.y-1;

		/* more intelligence here */
		if (w > (pvt->width - pvt->div_pt.x)) {
			boundary_x = 0;
			--need_scan_flag;
		}
		if (h > pvt->div_pt.y) {
			boundary_y = pvt->height - 1;
			--need_scan_flag;
		}

		assign(&scan_area, pvt->width - 1, 0, boundary_x,
		       boundary_y);
		ret = scan_r2l_t2b(tcm, w, h, stride, &scan_area,
				   allocated_area);

		if (ret != TilerErrorNone && need_scan_flag) {
			/*Fall back Scan the entire Tiler area*/
			assign(&scan_area, pvt->width - 1, 0, 0,
			       pvt->height - 1);
			ret = scan_r2l_t2b(tcm, w, h, stride, &scan_area,
					   allocated_area);
		}
	}

	/* March 30th: As per discussion: Moved 64 and 32 to left of Container
	 * and Align None to right of container.
	 */
#if 0
	else if (stride == 1) {

		/*The reason we use 64-Align area is because we dont want to
		grow down and reduced 1D space */
		if (h > pvt->div_pt.y) {
			need_scan_flag -= 2;
			assign(&scan_area, 0, 0, pvt->width - 1,
			       pvt->height - 1);
			ret = scan_l2r_t2b(tcm, w, h, stride, &scan_area,
					   allocated_area);
		} else {
			assign(&scan_area, 0,
			       pvt->div_pt.y - 1,
			       pvt->width - 1, 0);
			/*Scans Up in 64 and 32 areas accross the whole width*/
			ret = scan_l2r_b2t(tcm, w, h, stride, &scan_area,
					   allocated_area);
		}

		if (ret != TilerErrorNone && need_scan_flag) {
			assign(&scan_area, 0, 0, pvt->width - 1,
			       pvt->height - 1);
			ret = scan_l2r_t2b(tcm,  w, h, stride, &scan_area,
					   allocated_area);
		}
	}
#endif
	return ret;

}

s32 check_fit_r_and_b(struct tcm *tcm, u16 w, u16 h, u16 left_x,
		      u16 top_y)
{
	u16 xx = 0, yy = 0;
	s32 ret = FIT;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	for (yy = top_y; yy < top_y+h; ++yy) {
		for (xx = left_x; xx < left_x+w; ++xx) {
			/*P("Checking Occ: (%d %d) - %d\n",xx,yy,
			pvt->tcm_map[xx][yy].is_occupied); */
			if (pvt->tcm_map[xx][yy].is_occupied == OCCUPIED) {
				ret = NO_FIT;
				return ret;
			}
		}
	}
	return ret;
}

s32 check_fit_r_one_dim(struct tcm *tcm, u16 x, u16 y, u32 num_of_pages,
			u16 *busy_x, u16 *busy_y)
{
	s32 fit = FIT;
	s32 ret = TilerErrorNone;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	s32 i = 0;
	*busy_x = x;
	*busy_y = y;

	P2("checking Fit for (%d) pages from (%d %d)\n ", num_of_pages, x, y);
	while (i < num_of_pages) {
		/* P("Checking if occupied (%d %d)\n",x, y); */
		if (pvt->tcm_map[x][y].is_occupied) {
			/*Oh the Tile is occupied so we break and let know that
						we encoutered a BUSY tile */
			fit = NO_FIT;

			/* Now going to the start of the parent allocation
			so we avoid unecessary checking */

			if (pvt->tcm_map[x][y].type == TCM_1D) {
				*busy_x = pvt->tcm_map[x][y].parent_area.p0.x;
				*busy_y = pvt->tcm_map[x][y].parent_area.p0.y;
			} else {
				*busy_x = pvt->tcm_map[x][y].parent_area.p0.x;
				*busy_y = y;
			}
			/* To Do: */
			/*Could also move left in case of TCM_2D*/
			P2("Busy Tile found moving to ParentArea start :\
						(%d %d)\n", *busy_x, *busy_y);
			break;
		}

		i++;
		/* Sorry for this ugly code, i had to break before moving right
		unecessarily */
		/* This also helps my dual purpose busy_x and busy_y with the
		end co-ordinates  of 1D allocations*/
		if (i == num_of_pages)
			break;

		ret = move_right(tcm, x, y, 1, busy_x, busy_y);
		if (ret != TilerErrorNone) {
			PE("Error in Moving Right.. Breaking...\n");
			fit = NO_FIT;
			break;
		} else {
			x = *busy_x;
			y = *busy_y;
		}
	}

	return fit;
}


s32 insert_area_with_tiler_page(struct tcm *tcm,
				struct tcm_area *area, struct tiler_page tile)
{
	s32 x, y;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	if (area->p0.x < 0 || area->p1.x >= pvt->width || area->p0.y < 0 ||
			area->p1.y >= pvt->height) {
		PE("Invalid dimensions\n");
		return TilerErrorInvalidDimension;
	}
	P2("Inserting Tiler Page at (%d %d) (%d %d)\n", area->p0.x, area->p0.y,
	   area->p1.x, area->p1.y);

	/*If you are here: basic checks are done */
	for (x = area->p0.x; x <= area->p1.x; ++x)
		for (y = area->p0.y; y <= area->p1.y; ++y)
			pvt->tcm_map[x][y] = tile;
	return TilerErrorNone;
}

s32 insert_pages_with_tiler_page(struct tcm *tcm, struct tcm_area *area,
				 struct tiler_page tile)
{

	u16 x = 0, y = 0;
	u16 right_x = 0, right_y = 0;
	s32 ret = TilerErrorNone;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	if (area == NULL) {
		PE("Null input received\n");
		return TilerErrorInvalidArg;
	}

	/*Note: By the way I expect Pages specified from Right to Left */
	/* TO DO:
	*Do i really need to check integrity of area specs?
	*/

	P2("Inserting Tiler Pages from (%d %d) to (%d %d)\n", area->p0.x,
	   area->p0.y, area->p1.x, area->p1.y);


	x = area->p0.x;
	y = area->p0.y;

	while (!(x == area->p1.x && y == area->p1.y)) {
		/* P("(%d %d)\n",x,y); */
		pvt->tcm_map[x][y] = tile;
		ret = move_right(tcm, x, y, 1, &right_x, &right_y);
		if (ret == TilerErrorNone) {
			x = right_x;
			y = right_y;
		} else {
			PE("Error in Moving right\n");
			return ret;
		}
	}
	/*Of course since this is inclusive we need to set the last tile too */
	pvt->tcm_map[x][y] = tile;

	return TilerErrorNone;
}

s32 select_candidate(struct tcm *tcm, u16 w, u16 h,
		     u16 num_short_listed, struct area_spec_list *short_listed,
		     struct tcm_area *scan_area, s32 criteria,
		     struct tcm_area *alloc_area)
{

	/* book keeping the winner */
	struct area_spec_list *win_candidate = NULL;
	struct nearness_factor win_near_factor = {0, 0};
	struct neighbour_stats win_neigh_stats = {0, 0, 0, 0, 0, 0, 0, 0};
	u16 win_total_neighs = 0;

	/*book keeping the current one being evaluated */
	struct area_spec_list *cur_candidate = NULL;
	struct tcm_area *cur_area = NULL;
	struct nearness_factor cur_near_factor = {0, 0};
	struct neighbour_stats cur_neigh_stats = {0, 0, 0, 0, 0, 0, 0, 0};
	u16 cur_total_neighs = 0;

	/*Should i swap_flag? */
	u8 swap_flag = NO;

	/* I am sure that Alloc Area == NULL is checked earlier, but still
	checking */
	if (alloc_area == NULL || scan_area == NULL) {
		PE("NULL input found\n");
		return TilerErrorInvalidArg;
	}

	/*if there is only one candidate then that is the selection*/
	if (num_short_listed == 1) {
		/* Note: Sure we could have done this in the previous function,
		but just wanted this to be cleaner so having
		  * one place where the selection is made. Here I am returning
		  the first one
		  */
		assign(alloc_area, short_listed->area.p0.x,
		       short_listed->area.p0.y, short_listed->area.p0.x + w - 1,
		       short_listed->area.p0.y + h - 1);
		return TilerErrorNone;
	}

	/* If first found is enabled then we just provide bluntly the first
	found candidate
	  * NOTE: For Horizontal bias we just give the first found, because our
	  * scan is Horizontal raster based and the first candidate will always
	  * be the same as if selecting the Horizontal one.
	  */
	if (criteria & CR_FIRST_FOUND || criteria & CR_BIAS_HORIZONTAL) {
		assign(alloc_area, short_listed->area.p0.x,
		       short_listed->area.p0.y, short_listed->area.p0.x + w - 1,
		       short_listed->area.p0.y + h - 1);
		return TilerErrorNone;
	}

	/* lets calculate for the first candidate and assign him the winner and
	replace with the one who has better credentials w/ to the criteria */

	win_candidate = short_listed;
	get_busy_neigh_stats(tcm, w, h, &short_listed->area, &win_neigh_stats);
	win_total_neighs = TOTAL_BOUNDARY(&win_neigh_stats) +
			   TOTAL_OCCUPIED(&win_neigh_stats);
	get_nearness_factor(scan_area, &short_listed->area, &win_near_factor);
	/* now check from second candidate onwards */
	cur_candidate = short_listed->next;

	while (cur_candidate != NULL) {

		/* Calculating all the required statistics, though not using
		  * all of them in all Criteria, but makes simpler code
		  */
		cur_area = &cur_candidate->area;
		get_busy_neigh_stats(tcm, w, h, cur_area, &cur_neigh_stats);
		get_nearness_factor(scan_area, cur_area, &cur_near_factor);
		/* Check against the winner, if this one is better */
		cur_total_neighs =  TOTAL_BOUNDARY(&cur_neigh_stats) +
				    TOTAL_OCCUPIED(&cur_neigh_stats);



		/* PREFER MAX NEIGHBOURS  */
		if (criteria & CR_MAX_NEIGHS) {
			if (cur_total_neighs > win_total_neighs)
				swap_flag = YES;
		}

		/* I am not checking for the condition where both
		  * INCL_LENS are same, because the logic does not find new
		  * shoulders on the same row after it finds a fit
		  */
		if (criteria & CR_BIAS_VERTICAL) {
			P("cur distance :%d  win distance: %d\n",
			  INCL_LEN_MOD(cur_area->p0.y, scan_area->p0.y),
			  INCL_LEN_MOD(win_candidate->area.p0.y,
				       scan_area->p0.y))
			if (INCL_LEN_MOD(cur_area->p0.y, scan_area->p0.y) >
					INCL_LEN_MOD(win_candidate->area.p0.y,
						     scan_area->p0.y)) {
				swap_flag = YES;
			}
		}


		if (criteria & CR_DIAGONAL_BALANCE) {

			/* Check against the winner, if this one is better */
			cur_total_neighs =  TOTAL_BOUNDARY(&cur_neigh_stats) +
					    TOTAL_OCCUPIED(&cur_neigh_stats);

			if (win_total_neighs <= cur_total_neighs) {
				P3("Logic: Oh win_total_neighs(%d) <=\
					cur_total_neighs(%d)\n",
				   win_total_neighs, cur_total_neighs);
				if (win_total_neighs < cur_total_neighs ||
					(TOTAL_OCCUPIED(&win_neigh_stats) <
					 TOTAL_OCCUPIED(&cur_neigh_stats))) {
					P3("Logic: Found one with more\
						neighbours win_total_neighs:%d\
						cur_total_neighs:%d WinOcc: %d,\
						CurOcc: %d\n", win_total_neighs,
					   cur_total_neighs,
					   TOTAL_OCCUPIED(
						   &win_neigh_stats),
					   TOTAL_OCCUPIED(
						   &cur_neigh_stats));
					swap_flag = YES;
				} else if ((TOTAL_OCCUPIED(&win_neigh_stats) ==
					TOTAL_OCCUPIED(&cur_neigh_stats))) {
					/*If we are here implies that
					win_total_neighs == cur_total_neighs
					&& Total_Occupied(win) ==
					TotalOccupied(cur)  */
					/*Now we check the nearness factor */
					P3("Logic: Ah WinOcc(%d) == CurOcc:\
					(%d), so checking Nearness factor\n",
					   TOTAL_OCCUPIED(&win_neigh_stats),
					   TOTAL_OCCUPIED(&cur_neigh_stats));
					P3("Logic: Hmm winNF (%3f) & curNF\
						(%3f)\n",
					   (double)(win_near_factor.nf_x
						    + win_near_factor.nf_y),
					   (double)(cur_near_factor.nf_x
						    + cur_near_factor.nf_y));
					if ((s32)(win_near_factor.nf_x +
						win_near_factor.nf_y) >
						(s32)(cur_near_factor.nf_x +
						cur_near_factor.nf_y)) {
						P3("Logic: So, nearness factor\
						of Cur is <\
						than Win\n");
						swap_flag = YES;
					}

				}

			}


		}

		/* Swap the win candidate with cur-candidate with better
		  * credentials
		  */
		if (swap_flag) {
			win_candidate = cur_candidate;
			win_near_factor = cur_near_factor;
			win_neigh_stats = cur_neigh_stats;
			win_total_neighs = cur_total_neighs;
			swap_flag = NO;
		}

		/*Go to next candidate */
		cur_candidate = cur_candidate->next;

	}


	assign(alloc_area, win_candidate->area.p0.x, win_candidate->area.p0.y,
		win_candidate->area.p0.x+w - 1,
		win_candidate->area.p0.y + h - 1);

	return TilerErrorNone;
}

s32 get_nearness_factor(struct tcm_area *scan_area,
			struct tcm_area *candidate, struct nearness_factor *nf)
{

	if (nf == NULL || scan_area == NULL || candidate == NULL) {
		PE("NULL input found\n");
		return TilerErrorInvalidArg;
	}

	/* For the following calculation we need worry of +/- sign, the
	relative distances take of this. Multiplied by 1000, there
	is no floating point arithmetic used in kernel */

	nf->nf_x = (s32)(candidate->p0.x - scan_area->p0.x)*1000/
		   (scan_area->p1.x - scan_area->p0.x);
	nf->nf_y = (s32)(candidate->p0.y - scan_area->p0.y)*1000/
		   (scan_area->p1.y - scan_area->p0.y);

	return TilerErrorNone;

}

/*Neighbours

	|<-----T------>|
	_ _______________  _
	L |     Area            | R
	_ |______________|_
	|<-----B------>|
*/

s32 get_busy_neigh_stats(struct tcm *tcm, u16 width, u16 height,
			 struct tcm_area *top_left_corner,
			 struct neighbour_stats *neighbour_stat)
{
	s16 xx = 0, yy = 0;
	struct tcm_area left_edge;
	struct tcm_area right_edge;
	struct tcm_area top_edge;
	struct tcm_area bottom_edge;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	if (neighbour_stat == NULL) {
		PE("Null input received\n");
		return TilerErrorInvalidArg;
	}

	if (width == 0 || height == 0) {
		PE("u16 or height is Zero \n");
		return TilerErrorInvalidArg;
	}

	/*Clearing any exisiting values */
	memset(neighbour_stat, 0, sizeof(struct neighbour_stats));

	/* Finding Top Edge */
	assign(&top_edge, top_left_corner->p0.x, top_left_corner->p0.y,
	       top_left_corner->p0.x + width - 1, top_left_corner->p0.y);

	/* Finding Bottom Edge */
	assign(&bottom_edge, top_left_corner->p0.x,
	       top_left_corner->p0.y+height - 1,
	       top_left_corner->p0.x + width - 1,
	       top_left_corner->p0.y + height - 1);

	/* Finding Left Edge */
	assign(&left_edge, top_left_corner->p0.x, top_left_corner->p0.y,
	       top_left_corner->p0.x, top_left_corner->p0.y + height - 1);

	/* Finding Right Edge */
	assign(&right_edge, top_left_corner->p0.x + width - 1,
		top_left_corner->p0.y,
		top_left_corner->p0.x + width - 1,
		top_left_corner->p0.y + height - 1);

	/* dump_area(&top_edge);
	dump_area(&right_edge);
	dump_area(&bottom_edge);
	dump_area(&left_edge);
	*/

	/*Parsing through top & bottom edge*/
	for (xx = top_edge.p0.x; xx <= top_edge.p1.x; ++xx) {
		if ((top_edge.p0.y - 1) < 0) {
			neighbour_stat->top_boundary++;
		} else {
			if (pvt->tcm_map[xx][top_edge.p0.y - 1].is_occupied)
				neighbour_stat->top_occupied++;
		}

		/* Realized that we need to pass through the same iters for
		bottom edge. So trying to reduce passes by manipulating the
		checks */
		if ((bottom_edge.p0.y + 1) > (pvt->height - 1)) {
			neighbour_stat->bottom_boundary++;
		} else {
			if (pvt->tcm_map[xx][bottom_edge.p0.y+1].is_occupied)
				neighbour_stat->bottom_occupied++;
		}

	}

	/* Parsing throught left and right edge */
	for (yy = left_edge.p0.y; yy <= left_edge.p1.y; ++yy) {
		if ((left_edge.p0.x - 1) < 0) {
			neighbour_stat->left_boundary++;
		} else {
			if (pvt->tcm_map[left_edge.p0.x - 1][yy].is_occupied)
				neighbour_stat->left_occupied++;
		}

		if ((right_edge.p0.x + 1) > (pvt->width - 1)) {
			neighbour_stat->right_boundary++;
		} else {
			if (pvt->tcm_map[right_edge.p0.x + 1][yy].is_occupied)
				neighbour_stat->right_occupied++;
		}

	}

	return TilerErrorNone;
}

/**
	@description: Retrieves the parent area of the page at p0.x, p0.y if
	occupied
	@input:co-ordinates of the page (p0.x, p0.y) whoes parent area
	is required
	@return 0 on success, non-0 error value on failure. On success

	parent_area will contain co-ordinates (TL & BR corner) of the parent
	area
*/
static s32 sita_get_parent(struct tcm *tcm, struct tcm_pt *pt,
		    struct tcm_area *parent_area)
{
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	if (parent_area == NULL) {
		PE("NULL input found\n");
		return TilerErrorInvalidArg;
	}

	if (pt->x < 0 || pt->x >= pvt->width || pt->y < 0 ||
			pt->y >= pvt->height) {
		PE("Invalid dimensions\n");
		return TilerErrorInvalidDimension;
	}

	MUTEX_LOCK(&(pvt->mtx));
	assign(parent_area, 0, 0, 0, 0);

	if (pvt->tcm_map[pt->x][pt->y].is_occupied) {
		parent_area->p0.x = pvt->tcm_map[pt->x][pt->y].parent_area.p0.x;
		parent_area->p0.y = pvt->tcm_map[pt->x][pt->y].parent_area.p0.y;
		parent_area->p1.x = pvt->tcm_map[pt->x][pt->y].parent_area.p1.x;
		parent_area->p1.y = pvt->tcm_map[pt->x][pt->y].parent_area.p1.y;
	}
	MUTEX_REL(&(pvt->mtx));

	return TilerErrorNone;
}

static s32 move_left(struct tcm *tcm, u16 x, u16 y, u32 num_of_pages,
		     u16 *xx, u16 *yy)
{
	/* Num of Pages remaining to the left of the same ROW. */
	u16 num_of_pages_left = x;
	u16 remain_pages = 0;
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;

	/* I want this function to be really fast and dont take too much time,
					so i will not do detailed checks */
	if (x > pvt->width || y > pvt->height  || xx == NULL ||
			yy == NULL) {
		PE("Error in input arguments\n");
		return TilerErrorInvalidArg;
	}

	/*Checking if we are going to go out of bound with the given
	num_of_pages */
	if (num_of_pages > x + (y * pvt->width)) {
		PE("Overflows off the top left corner, can go at the Max (%d) \
			to left from (%d, %d)\n", (x + y * pvt->width), x, y);
		return TilerErrorOverFlow;
	}

	if (num_of_pages > num_of_pages_left) {
		/* we fit the num of Pages Left on the same column */
		remain_pages = num_of_pages - num_of_pages_left;

		if (0 == remain_pages % pvt->width) {
			*xx = 0;
			*yy = y - remain_pages / pvt->width;
		} else {
			*xx = pvt->width - remain_pages %
			      pvt->width;
			*yy = y - (1 + remain_pages / pvt->width);
		}
	} else {
		*xx = x - num_of_pages;
		*yy = y;
	}
	return TilerErrorNone;
}


s32 move_right(struct tcm *tcm, u16 x, u16 y, u32 num_of_pages,
	       u16 *xx, u16 *yy)
{
	struct sita_pvt *pvt = (struct sita_pvt *)tcm->pvt;
	u32 avail_pages = (pvt->width - x - 1) +
			  (pvt->height - y - 1) * pvt->width;
	/* Num of Pages remaining to the Right of the same ROW. */
	u16 num_of_pages_right = pvt->width - 1 - x;
	u16 remain_pages = 0;

	if (x > pvt->width || y > pvt->height || xx == NULL ||
			yy == NULL) {
		PE("Error in input arguments");
		return TilerErrorInvalidArg;
	}

	/*Checking if we are going to go out of bound with the given
	num_of_pages */
	if (num_of_pages > avail_pages) {
		PE("Overflows off the top Right corner, can go at the Max (%d)\
			to Right from (%d, %d)\n", avail_pages, x, y);
		return TilerErrorOverFlow;
	}

	if (num_of_pages > num_of_pages_right) {
		remain_pages = num_of_pages - num_of_pages_right;

		if (0 == remain_pages % pvt->width) {
			*xx = pvt->width - 1;
			*yy = y + remain_pages / pvt->width;
		} else {
			*xx = remain_pages % pvt->width - 1;
			*yy = y + (1 + remain_pages / pvt->width);
		}
	} else {
		*xx = x + num_of_pages;
		*yy = y;
	}

	return TilerErrorNone;
}


#ifdef TILER_TEST_FUNCTIONS
/* Test insert
 * Dummy insertion, No Error Checking.
 */
s32 test_insert(struct tam *tcm, IN struct area_spec_list *new_area)
{
	struct tiler_page tile;
	struct tcm_area area = new_area->area;

	if (new_area == NULL)
		return TilerErrorInvalidArg;

	tile.is_occupied = OCCUPIED;
	tile.reserved = 0;
	tile.parent_area.p0.x = area.p0.x;
	tile.parent_area.p0.y = area.p0.y;
	tile.parent_area.p1.x = area.p1.x;
	tile.parent_area.p1.y = area.p1.y;
	tile.type = new_area->area_type;

	if (new_area->area_type == TCM_2D)
		return insert_area_with_tiler_page(tcm, &area, tile);

	return insert_pages_with_tiler_page(tcm, &area, tile);
}

s32 test_dump_alloc_list(struct tam *tcm)
{
	dump_list_entries(pvt->res_list);
	return TilerErrorNone;
}


s32 test_allocate_2D_area(struct tam *tcm, IN u16 w, IN u16 h,
		  u16  align, u16 corner, OUT struct tcm_area *allocated_area)
{

	struct tcm_area scan_area = {0, 0, 0, 0};
	s32 ret = TilerErrorNone;
	u16 stride = ALIGN_STRIDE(align);
	struct tiler_page tile;

	/*check if width and height are within limits */
	if (w > pvt->width || w == 0 || h > pvt->height || h == 0) {
		PE("Invalid dimension:: %d x %d\n", w, h);
		return TilerErrorInvalidDimension;
	}

	/* Checking for input arguments */
	if (allocated_area == NULL) {
		PE("NULL input found\n");
		return TilerErrorInvalidArg;
	}

	if (corner == TL_CORNER) {
		assign(&scan_area, 0, 0, (pvt->width - 1),
		       (pvt->height - 1));
		ret = scan_l2r_t2b(tcm, w, h, stride, &scan_area,
				   allocated_area);
	} else if (corner == TR_CORNER) {
		assign(&scan_area, (pvt->width-1), 0, 0,
		       (pvt->height - 1));
		ret = scan_r2l_t2b(tcm, w, h, stride, &scan_area,
				   allocated_area);
	} else if (corner == BL_CORNER) {
		assign(&scan_area, 0, (pvt->height - 1),
		       (pvt->width - 1), 0);
		ret = scan_l2r_b2t(tcm, w, h, stride, &scan_area,
				   allocated_area);
	} else {
		assign(&scan_area, (pvt->width - 1),
		       (pvt->height - 1), 0, 0);
		ret = scan_r2l_b2t(tcm, w, h, stride, &scan_area,
				   allocated_area);
	}

	MUTEX_LOCK(&(pvt->mtx));

	if (ret != TilerErrorNone) {
		PE("Did not find anything in the given area\n");
	} else {
		P2("Yahoo found a fit: %s\n", AREA_STR(a_str, allocated_area));
		tile.is_occupied = OCCUPIED;
		assign(&tile.parent_area, allocated_area->p0.x,
		       allocated_area->p0.y, allocated_area->p1.x,
		       allocated_area->p1.y);

		/* some id, not useful now */
		tile.reserved = pvt->id++;

		/* Saying that type is 2D */
		tile.type = TCM_2D;

		/* inserting into tiler container */
		insert_area_with_tiler_page(tcm, allocated_area, tile);

		/*updating the list of allocations */
		insert_element(&pvt->res_list, allocated_area, TCM_2D);
	}

	MUTEX_REL(&(pvt->mtx));
	return ret;

}

s32 test_get_busy_neigh_stats(struct tam *tcm, u16 width, u16 height,
			      struct tcm_area *top_left_corner,
			      struct neighbour_stats *neighbour_stat)
{
	return get_busy_neigh_stats(tcm, width, height, top_left_corner,
				    neighbour_stat);
}


s32 test_check_busy(struct tam *tcm, IN u16 x, u16 y)
{
	return (s32)pvt->tcm_map[x][y].is_occupied;
}

#endif


