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

#define OMAP4_BLAZE_ID		0x10
#define OMAP4_TABLET_1_0	2143
#define OMAP4_TABLET_2_0	2158

int omap_get_board_version(void);
int omap_init_board_version(void);
bool omap_is_board_version(int req_board_version);
