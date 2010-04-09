#ifndef _TILER_UTILS_H
#define _TILER_UTILS_H

#include "tcm_rr.h"
#include "tcm_dbg.h"

#define AREA_STR(a_str, area) ({ \
	sprintf(a_str, "(%03d %03d)-(%03d %03d)", (area)->p0.x, (area)->p0.y, \
					(area)->p1.x, (area)->p1.y); a_str; })

void assign(struct tcm_area *a, u16 x0, u16 y0, u16 x1, u16 y1);
void dump_area(struct tcm_area *area);

s32 insert_element(INOUT struct area_spec_list **list,
				IN struct tcm_area *newArea, IN u16 area_type);

s32 dump_list_entries(IN struct area_spec_list *list);

s32 dump_neigh_stats(struct neighbour_stats *neighbour);

s32 rem_element_with_match(struct area_spec_list **list,
			struct tcm_area *to_be_removed, u16 *area_type);

s32 clean_list(struct area_spec_list **list);
#endif
