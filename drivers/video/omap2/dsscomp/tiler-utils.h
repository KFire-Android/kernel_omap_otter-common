/*
 * drivers/video/omap2/dsscomp/tiler-utils.h
 *
 * Tiler Utility APIs header file
 *
 * Copyright (C) 2011 Texas Instruments, Inc
 * Author: Sreenidhi Koti <sreenidhi@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* type of tiler memory */
enum tiler_memtype {
	TILER_MEM_ALLOCED,		/* tiler allocated the memory */
	TILER_MEM_GOT_PAGES,		/* tiler used get_user_pages */
	TILER_MEM_USING,		/* tiler is using the pages */
};

/* physical pages to pin - mem must be kmalloced */
struct tiler_pa_info {
	u32 num_pg;			/* number of pages in page-list */
	u32 *mem;			/* list of phys page addresses */
	enum tiler_memtype memtype;	/* how we got physical pages */
};

/**
 * Frees a tiler_pa_info structure and associated memory
 *
 * @param pa		Ptr to tiler_pa_info structure
 *
 */
void tiler_pa_free(struct tiler_pa_info *pa);

/**
 * Creates and returns a tiler_pa_info structure from a user address
 *
 * @param usr_addr      User Address
 * @param num_pg        Number of pages
 *
 * @return Ptr to new tiler_pa_info structure
 */
struct tiler_pa_info *user_block_to_pa(u32 usr_addr, u32 num_pg);
/**
 * Get the physical address for a given user va.
 *
 * @param usr	user virtual address
 *
 * @return valid pa or 0 for error
 */
u32 tiler_virt2phys(u32 usr);
