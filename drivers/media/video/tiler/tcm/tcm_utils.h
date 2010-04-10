#ifndef _TILER_UTILS_H
#define _TILER_UTILS_H

#include "tcm_rr.h"
#include "tcm_dbg.h"

#include <linux/init.h>
#include <linux/module.h>

#define AREA_FMT   "(%03d %03d)-(%03d %03d)"
#define AREA(area) (area)->p0.x, (area)->p0.y, (area)->p1.x, (area)->p1.y
#define PA(level, msg, area) P##level(msg " " AREA_FMT "\n", AREA(area))

/*
 * Assignment Utility Function
 */
static inline
void assign(IN struct tcm_area *a, IN u16 x0, IN u16 y0, IN u16 x1, IN u16 y1)
{
	a->p0.x = x0;
	a->p0.y = y0;
	a->p1.x = x1;
	a->p1.y = y1;
}

static inline
void dump_area(struct tcm_area *area)
{
	printk(KERN_NOTICE AREA_FMT "\n", AREA(area));
}

/*
 *      Inserts a given area at the end of a given list
 */
static
s32 insert_element(INOUT struct area_spec_list **list,
		   IN struct tcm_area *newArea, IN u16 area_type)
{
	struct area_spec_list *list_iter = *list;
	struct area_spec_list *new_elem = NULL;
	if (list_iter == NULL) {
		list_iter = kmalloc(sizeof(*list_iter), GFP_KERNEL);
		/* P("Created new List: 0x%x\n",list_iter); */
		assign(&list_iter->area, newArea->p0.x, newArea->p0.y,
						newArea->p1.x, newArea->p1.y);
		list_iter->area.tcm  = newArea->tcm;
		list_iter->area.type = newArea->type;
		list_iter->area_type = area_type;
		list_iter->next = NULL;
		*list = list_iter;
		return TilerErrorNone;
	}

	/* move till you find the last element */
	while (list_iter->next != NULL)
		list_iter = list_iter->next;

	/* now we are the last one */
	/* P("Adding to the end of list\n"); */
	/*To Do: Check for malloc failures */
	new_elem = kmalloc(sizeof(*new_elem), GFP_KERNEL);
	assign(&new_elem->area, newArea->p0.x, newArea->p0.y, newArea->p1.x,
								newArea->p1.y);
	new_elem->area.tcm = newArea->tcm;
	new_elem->area.type = newArea->type;
	new_elem->area_type = area_type;
	new_elem->next = NULL;
	list_iter->next = new_elem;
	return TilerErrorNone;
}

static
s32 rem_element_with_match(struct area_spec_list **listHead,
			   struct tcm_area *to_be_removed, u16 *area_type)
{
	struct area_spec_list *temp_list = NULL;
	struct area_spec_list *matched_elem = NULL;
	struct tcm_area *cur_area = NULL;
	u8 found_flag = NO;

	/*If the area to be removed matchs the list head itself,
	we need to put the next one as list head */
	if (*listHead !=  NULL) {
		cur_area = &(*listHead)->area;
		if (cur_area->p0.x == to_be_removed->p0.x && cur_area->p0.y ==
			to_be_removed->p0.y && cur_area->p1.x ==
			to_be_removed->p1.x && cur_area->p1.y ==
			to_be_removed->p1.y) {
			*area_type = (*listHead)->area_type;
			PA(1, "removing match", cur_area);

			temp_list = (*listHead)->next;
			kfree(*listHead);
			*listHead = temp_list;
			return TilerErrorNone;
		}
	}

	temp_list = *listHead;
	while (temp_list  !=  NULL) {
		/* we have already checked the list head,
			we check for the second in list */
		if (temp_list->next != NULL) {
			cur_area = &temp_list->next->area;
			if (cur_area->p0.x == to_be_removed->p0.x
				&& cur_area->p0.y == to_be_removed->p0.y
				&& cur_area->p1.x == to_be_removed->p1.x
				&& cur_area->p1.y == to_be_removed->p1.y) {
				PA(1, "removing match", cur_area);
				matched_elem = temp_list->next;
				*area_type = matched_elem->area_type;
				temp_list->next = matched_elem->next;
				kfree(matched_elem);
				matched_elem = NULL;
				found_flag = YES;
				break;
			}
		}
		temp_list = temp_list->next;
	}

	if (found_flag)
		return TilerErrorNone;

	PA(5, "Match Not found", to_be_removed);

	return TilerErrorMatchNotFound;
}

static
s32 clean_list(struct area_spec_list **list)
{
	struct area_spec_list *temp_list = NULL;
	struct area_spec_list *to_be_rem_elem = NULL;

	if (*list != NULL) {
		temp_list = (*list)->next;
		while (temp_list != NULL) {
			/*P("Freeing :");
			dump_area(&temp_list->area);*/
			to_be_rem_elem = temp_list->next;
			kfree(temp_list);
			temp_list = to_be_rem_elem;
		}

		/* freeing the head now */
		kfree(*list);
		*list = NULL;
	}

	return TilerErrorNone;
}

#if 0
static
s32 dump_list_entries(IN struct area_spec_list *list)
{
	struct area_spec_list *list_iter = NULL;
	P("Printing List Entries:\n");

	if (list == NULL) {
		PE("NULL List found\n");
		return TilerErrorInvalidArg;
	}

	/*Now if we have a valid list, let us print the values */
	list_iter = list;
	do {
		printk(KERN_NOTICE "%dD:" AREA_FMT "\n", list_iter->area_type,
					AREA(&list_iter->area));
		/* dump_area(&list_iter->area); */
		list_iter = list_iter->next;
	} while (list_iter != NULL);

	return TilerErrorNone;
}

static
s32 dump_neigh_stats(struct neighbour_stats *neighbour)
{
	P("Top  Occ:Boundary  %d:%d\n", neighbour->top_occupied,
						neighbour->top_boundary);
	P("Bot  Occ:Boundary  %d:%d\n", neighbour->bottom_occupied,
						neighbour->bottom_boundary);
	P("Left Occ:Boundary  %d:%d\n", neighbour->left_occupied,
						neighbour->left_boundary);
	P("Rigt Occ:Boundary  %d:%d\n", neighbour->right_occupied,
						neighbour->right_boundary);
	return TilerErrorNone;
}
#endif

#endif
