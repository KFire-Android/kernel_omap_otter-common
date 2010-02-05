#include <linux/init.h>
#include <linux/module.h>

#include "tiler_utils.h"
#include "tiler_debug.h"

/*
* Assignment Utility Function
*/
void assign(IN struct area_spec *a, IN u16 x0, IN u16 y0, IN u16 x1, IN u16 y1)
{
	a->x0 = x0;
	a->x1 = x1;
	a->y1 = y1;
	a->y0 = y0;
}

void dump_area(struct area_spec *area)
{
	printk(KERN_NOTICE "(%d %d) - (%d %d)\n", area->x0, area->y0, area->x1,
								area->y1);
}


/*
 *      Inserts a given area at the end of a given list
 */
s32 insert_element(INOUT struct area_spec_list **list,
				IN struct area_spec *newArea, IN u16 area_type)
{
	struct area_spec_list *list_iter = *list;
	struct area_spec_list *new_elem = NULL;
	if (list_iter == NULL) {
		list_iter = kmalloc(sizeof(struct area_spec_list), GFP_KERNEL);
		/* P("Created new List: 0x%x\n",list_iter); */
		assign(&list_iter->area, newArea->x0, newArea->y0,
						newArea->x1, newArea->y1);
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
	new_elem = kmalloc(sizeof(struct area_spec_list), GFP_KERNEL);
	assign(&new_elem->area, newArea->x0, newArea->y0, newArea->x1,
								newArea->y1);
	new_elem->area_type = area_type;
	new_elem->next = NULL;
	list_iter->next = new_elem;
	return TilerErrorNone;
}

s32 rem_element_with_match(struct area_spec_list **listHead,
			struct area_spec *to_be_removed, u16 *area_type)
{
	struct area_spec_list *temp_list = NULL;
	struct area_spec_list *matched_elem = NULL;
	struct area_spec *cur_area = NULL;
	u8 found_flag = NO;

	/*If the area to be removed matchs the list head itself,
	we need to put the next one as list head */
	if (*listHead !=  NULL) {
		cur_area = &(*listHead)->area;
		if (cur_area->x0 == to_be_removed->x0 && cur_area->y0 ==
			to_be_removed->y0 && cur_area->x1 ==
			to_be_removed->x1 && cur_area->y1 ==
			to_be_removed->y1) {
			*area_type = (*listHead)->area_type;
			P1("Match found, Now Removing Area : %s\n",
				AREA_STR(a_str, cur_area));

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
			if (cur_area->x0 == to_be_removed->x0 && cur_area->y0 ==
				to_be_removed->y0 && cur_area->x1 ==
				to_be_removed->x1 && cur_area->y1 ==
				to_be_removed->y1) {
				P1("Match found, Now Removing Area : %s\n",
					AREA_STR(a_str, cur_area));
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

	PE("Match Not found :%s\n", AREA_STR(a_str, to_be_removed));

	return TilerErrorMatchNotFound;
}

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

s32 dump_list_entries(IN struct area_spec_list *list)
{
	struct area_spec_list *list_iter = NULL;
	char a_str[32] = {'\0'};
	P("Printing List Entries:\n");

	if (list == NULL) {
		PE("NULL List found\n");
		return TilerErrorInvalidArg;
	}

	/*Now if we have a valid list, let us print the values */
	list_iter = list;
	do {
		printk(KERN_NOTICE "%dD:%s\n", list_iter->area_type,
					AREA_STR(a_str, &list_iter->area));
		/* dump_area(&list_iter->area); */
		list_iter = list_iter->next;
	} while (list_iter != NULL);

	return TilerErrorNone;
}



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

s32 move_left(u16 x, u16 y, u32 num_of_pages, u16 *xx, u16 *yy)
{
	/* Num of Pages remaining to the left of the same ROW. */
	u16 num_of_pages_left = x;
	u16 remain_pages = 0;

	/* I want this function to be really fast and dont take too much time,
					so i will not do detailed checks */
	if (x > MAX_X_DIMMENSION || y > MAX_Y_DIMMENSION  || xx == NULL ||
								yy == NULL) {
		PE("Error in input arguments\n");
		return TilerErrorInvalidArg;
	}

	/*Checking if we are going to go out of bound with the given
	num_of_pages */
	if (num_of_pages > x + (y * MAX_X_DIMMENSION)) {
		PE("Overflows off the top left corner, can go at the Max (%d)\
			to left from (%d, %d)\n", (x + y * MAX_X_DIMMENSION),
									x, y);
		return TilerErrorOverFlow;
	}

	if (num_of_pages > num_of_pages_left) {
		/* we fit the num of Pages Left on the same column */
		remain_pages = num_of_pages - num_of_pages_left;

		if (0 == remain_pages % MAX_X_DIMMENSION) {
			*xx = 0;
			*yy = y - remain_pages / MAX_X_DIMMENSION;
		} else {
			*xx = MAX_X_DIMMENSION - remain_pages %
							MAX_X_DIMMENSION;
			*yy = y - (1 + remain_pages / MAX_X_DIMMENSION);
		}
	} else {
		*xx = x - num_of_pages;
		*yy = y;
	}

	return TilerErrorNone;
}

s32 move_right(u16 x, u16 y, u32 num_of_pages, u16 *xx, u16 *yy)
{
	u32 avail_pages = (MAX_X_DIMMENSION - x - 1) +
				(MAX_Y_DIMMENSION - y - 1) * MAX_X_DIMMENSION;
	 /* Num of Pages remaining to the Right of the same ROW. */
	u16 num_of_pages_right = MAX_X_DIMMENSION - 1 - x;
	u16 remain_pages = 0;

	if (x > MAX_X_DIMMENSION || y > MAX_Y_DIMMENSION || xx == NULL ||
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

		if (0 == remain_pages % MAX_X_DIMMENSION) {
			*xx = MAX_X_DIMMENSION - 1;
			*yy = y + remain_pages / MAX_X_DIMMENSION;
		} else {
			*xx = remain_pages % MAX_X_DIMMENSION - 1;
			*yy = y + (1 + remain_pages / MAX_X_DIMMENSION);
		}
	} else {
		*xx = x + num_of_pages;
		*yy = y;
	}

	return TilerErrorNone;
}
