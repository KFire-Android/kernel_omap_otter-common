/*
 * ALSA SoC OMAP ABE driver
*
 * Author:          Laurent Le Faucheur <l-le-faucheur@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */
#ifndef _ABE_MEM_H_
#define _ABE_MEM_H_


#define OMAP_ABE_PMEM_OFFSET	0xe0000
#define OMAP_ABE_CMEM_OFFSET	0xa0000
#define OMAP_ABE_SMEM_OFFSET	0xc0000
#define OMAP_ABE_DMEM_OFFSET	0x80000
#define OMAP_ABE_AESS_OFFSET	0xf1000

/* Distinction between Read and Write from/to ABE memory
 * is useful for simulation tool */
static inline void omap_abe_mem_write(struct omap_abe *abe, u32 offset,
					u32 *src, size_t bytes)
{
	memcpy((abe->io_base + offset), src, bytes);
}

static inline void omap_abe_mem_read(struct omap_abe *abe, u32 offset,
					u32 *dest, size_t bytes)
{
	memcpy(dest, (abe->io_base + offset), bytes);
}

static inline u32 omap_abe_reg_readl(struct omap_abe *abe, u32 offset)
{
	return __raw_readl(abe->io_base + offset);
}

static inline void omap_abe_reg_writel(struct omap_abe *abe, u32 offset,
					u32 val)
{
	__raw_writel(val, (abe->io_base + offset));
}

static inline void *omap_abe_reset_mem(struct omap_abe *abe, u32 offset,
					size_t bytes)
{
	return memset(abe->io_base + offset, 0, bytes);
}

#endif /*_ABE_MEM_H_*/
