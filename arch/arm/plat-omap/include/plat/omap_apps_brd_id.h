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

/* Board class */
#define BOARD_OMAP4_PANDA	0x2170
#define BOARD_OMAP4_BLAZE	0x9012
#define BOARD_OMAP4_TABLET1	0x2143
#define BOARD_OMAP4_TABLET2	0x2158
#define BOARD_OMAP5_SEVM	0x2600
#define BOARD_OMAP5_SEVM2	0x2601
#define BOARD_OMAP5_SEVM3	0x2624
#define BOARD_OMAP5_UEVM	0x2617
#define BOARD_OMAP5_UEVM2	0x2628
#define BOARD_OMAP5_TEVM	0x2820

#define BOARD_CLASS_MASK	0xFFFF
#define BOARD_CLASS_OFF		12
#define BOARD_REV_MASK		0xFFF
#define BOARD_NAME_MAX_SIZE	16

#define BOARD_GET_CLASS(a)	((a >> BOARD_CLASS_OFF) & BOARD_CLASS_MASK)
#define BOARD_GET_REV(a)	(a & BOARD_REV_MASK)
#define BOARD_MAKE_VERSION(class, rev) \
			(((class & BOARD_CLASS_MASK) << BOARD_CLASS_OFF) |\
			(rev & BOARD_REV_MASK))

int  __init omap_init_board_version(int forced_rev);
void __init omap_create_board_props(void);
int omap_get_board_class(void);
int omap_get_board_revision(void);
int omap_get_board_version(void);

const char *omap_get_board_name(void);

bool omap_is_board_class(int req_class);
bool omap_is_board_revision(int req_rev);
bool omap_is_board_version(int req_board_version);
