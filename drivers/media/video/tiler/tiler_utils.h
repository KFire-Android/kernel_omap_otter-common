#ifndef _TILER_UTILS_H
#define _TILER_UTILS_H

#include "tiler_common.h"
#include "tiler_debug.h"

#define AREA_STR(a_str, area) ({ \
	sprintf(a_str, "(%03d %03d)-(%03d %03d)", (area)->x0, (area)->y0, \
					(area)->x1, (area)->y1); a_str; })

void assign(struct area_spec *a, u16 x0, u16 y0, u16 x1, u16 y1);
void dump_area(struct area_spec *area);

s32 insert_element(INOUT struct area_spec_list **list,
				IN struct area_spec *newArea, IN u16 area_type);

s32 dump_list_entries(IN struct area_spec_list *list);

s32 dump_neigh_stats(struct neighbour_stats *neighbour);

s32 rem_element_with_match(struct area_spec_list **list,
			struct area_spec *to_be_removed, u16 *area_type);

s32 clean_list(struct area_spec_list **list);

s32 move_left(u16 x, u16 y, u32 num_of_pages, u16 *xx, u16 *yy);
s32 move_right(u16 x, u16 y, u32 num_of_pages, u16 *xx, u16 *yy);

#endif
