/*
 * Support for the TI OMAP4 Tablet Application board.
 *
 * Copyright (C) 2011 Texas Instruments
 *
 * Author: Dan Murphy <dmurphy@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */
#include <linux/kernel.h>
#include <linux/i2c.h>

#include <plat/omap_apps_brd_id.h>

static int board_revision;
static int board_id;
bool omap4_tablet_uses_hsic;

bool omap_is_board_version(int req_board_version)
{
	if (req_board_version == board_revision)
		return true;

	return false;
}

bool omap_board_uses_hsic(void)
{
	return omap4_tablet_uses_hsic;
}

int omap_get_board_version(void)
{
	return board_revision;
}

int omap_get_board_id(void)
{
	return board_id;
}


__init int omap_init_board_version(int forced_rev)
{
	if (forced_rev != 0)
		board_revision = forced_rev;
	else {
		switch (system_rev) {
		case OMAP4_TABLET_1_0:
			board_revision = OMAP4_TABLET_1_0;
			board_id = OMAP4_TABLET_1_0_ID;
			break;
		case OMAP4_TABLET_1_1:
			board_revision = OMAP4_TABLET_1_1;
			board_id = OMAP4_TABLET_1_1_ID;
			break;
		case OMAP4_TABLET_1_2:
			board_revision = OMAP4_TABLET_1_2;
			board_id = OMAP4_TABLET_1_2_ID;
			break;
		case OMAP4_TABLET_2_0:
			board_revision = OMAP4_TABLET_2_0;
			board_id = OMAP4_TABLET_2_0_ID;
			break;
		case OMAP4_TABLET_2_1:
			board_revision = OMAP4_TABLET_2_1;
			board_id = OMAP4_TABLET_2_1_ID;
			break;
		case OMAP4_TABLET_2_1_1:
			board_revision = OMAP4_TABLET_2_1_1;
			board_id = OMAP4_TABLET_2_1_1_ID;
			break;
		case OMAP4_TABLET_2_1_2:
			board_revision = OMAP4_TABLET_2_1_2;
			board_id = OMAP4_TABLET_2_1_2_ID;
			break;
		case OMAP4_BLAZE:
			board_revision = OMAP4_BLAZE;
			board_id = OMAP4_BLAZE_ID;
			break;
		default:
			board_revision = -1;
		}
	}

	return board_revision;
}
