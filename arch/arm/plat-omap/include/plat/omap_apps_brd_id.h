/*
 * arch/arm/plat-omap/include/mach/omap_tablet_id.h
 *
 * OMAP Tablet version detection
 *
 * Copyright (C) 2011 Texas Instruments.
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

#define OMAP4_PANDA		0xFF
#define OMAP4_BLAZE		0x10
#define OMAP4_TABLET_1_0	2143001
#define OMAP4_TABLET_1_1	2143002
#define OMAP4_TABLET_1_2	2143003
#define OMAP4_TABLET_2_0	2158001
#define OMAP4_TABLET_2_1	2158002
#define OMAP4_TABLET_2_1_1	2158003
#define OMAP4_TABLET_2_1_2	2158004

#define OMAP4_PANDA_ID		0
#define OMAP4_BLAZE_ID		1
#define OMAP4_TABLET_1_0_ID	2
#define OMAP4_TABLET_1_1_ID	3
#define OMAP4_TABLET_1_2_ID	4
#define OMAP4_TABLET_2_0_ID	5
#define OMAP4_TABLET_2_1_ID	6
#define OMAP4_TABLET_2_1_1_ID	7
#define OMAP4_TABLET_2_1_2_ID	8
#define OMAP4_MAX_ID	(OMAP4_TABLET_2_1_2_ID + 1)

int omap_get_board_version(void);
int omap_get_board_id(void);
int omap_init_board_version(int forced_rev);
bool omap_is_board_version(int req_board_version);
bool omap_board_uses_hsic(void);
