/*
 * tiler_pack.c
 *
 * TILER driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/init.h>
#include <linux/module.h>
#include "tiler.h"

#define ROUND_UP_2P(a, b) (((a) + (b) - 1) & ~((b) - 1))
#define DIVIDE_UP(a, b) (((a) + (b) - 1) / (b))
#define ROUND_UP(a, b) (DIVIDE_UP(a, b) * (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void tiler_packed_alloc_buf(int *count,
			    enum tiler_fmt fmt,
			    unsigned long width,
			    unsigned long height,
			    void **sysptr,
			    void **allocptr,
			    int aligned)
{
	int til_width, bpp, bpt, buf_width, alloc_width, map_width;
	int buf_map_width, n_per_m, m_per_a, i = 0, m, n;

	/* Check input parameters for correctness */
	if (!width || !height || !sysptr || !allocptr || !count ||
	    *count <= 0 || fmt < TILFMT_8BIT || fmt > TILFMT_32BIT) {
		if (count)
			*count = 0;
		return;
	}

	/* tiler page width in pixels, bytes per pixel, tiler page in bytes */
	til_width = fmt == TILFMT_32BIT ? 32 : 64;
	bpp = 1 << (fmt - TILFMT_8BIT);
	bpt = til_width * bpp;

	/* width of buffer in tiled pages */
	buf_width = DIVIDE_UP(width, til_width);

	/* :TODO: for now tiler allocation width is 64-multiple */
	alloc_width = ROUND_UP_2P(buf_width, 64);
	map_width = TILER_PAGE / bpt;

	/* ensure alignment if needed */
	buf_map_width = ROUND_UP_2P(buf_width, map_width);

	/* number of buffers in a map window */
	n_per_m = aligned ? 1 : (buf_map_width / buf_width);

	/* number of map windows per allocation */
	m_per_a = alloc_width / buf_map_width;

	printk(KERN_INFO "packing %d*%d buffers into an allocation\n",
	       n_per_m, m_per_a);

	while (i < *count) {
		/* allocate required width of a frame to fit remaining
		   frames */
		int n_alloc, m_alloc, tiles, res;
		void *base;

		n_alloc = MIN(*count - i, m_per_a * n_per_m);
		m_alloc = DIVIDE_UP(n_alloc, n_per_m);
		tiles = ((m_alloc - 1) * map_width +
			     buf_width * (n_alloc - (m_alloc - 1) * m_per_a));

		res = tiler_alloc(fmt, til_width * tiles, height,
				      (u32 *)*sysptr + i);
		if (res != 0)
			break;

		/* mark allocation */
		base = allocptr[i] = sysptr[i];
		i++;

		/* portion out remaining buffers */
		for (m = 0; m < m_per_a; m++, base += bpt * buf_map_width) {
			for (n = 0; n < n_per_m; n++) {
				/* first buffer is already allocated */
				if (n + m == 0)
					continue;

				/* stop if we are done */
				if (i == *count)
					break;

				/* set buffer address */
				sysptr[i] = base + bpt * n * buf_width;
				allocptr[i++] = NULL;
			}
		}
	}

	/* mark how many buffers we allocated */
	*count = i;
}
EXPORT_SYMBOL(tiler_packed_alloc_buf);

static int layout_packed_nv12(char *offsets, int y_width, int uv_width,
			      void **buf, int blocks, int i,
			      void **y_sysptr, void **uv_sysptr,
			      void **y_allocptr, void **uv_allocptr)
{
	int j;
	for (j = 0; j < blocks; j++, offsets += 3) {
		int page_offset = (63 & (int) offsets[0])
			+ y_width * ((int) offsets[1])
			+ uv_width * (int) offsets[2];
		void *base = buf[offsets[0] >> 6] + 64 * page_offset;

		if (j & 1) {
			/* convert 8-bit to 16-bit view */
			/* this formula only works for even ys */
			uv_sysptr[i] = base + (0x3FFF & (unsigned long) base)
				+ 0x8000000;
			uv_allocptr[i] = page_offset ? NULL : uv_sysptr[i];
			i++;
		} else {
			y_sysptr[i] = base;
			y_allocptr[i] = page_offset ? NULL : y_sysptr[i];
		}
	}
	return i;
}

void tiler_packed_alloc_nv12_buf(int *count,
				 unsigned long width,
				 unsigned long height,
				 void **y_sysptr,
				 void **uv_sysptr,
				 void **y_allocptr,
				 void **uv_allocptr,
				 int aligned)
{
	/* optimized packing table */
	/* we read this table from beginning to end, and determine whether
	   the optimization meets our requirement (e.g. allocating at least
	   i buffers, with max w y-width, and alignment a. If not, we get
	   to the next element.  Otherwise we do the allocation.  The table
	   is constructed in such a way that if an interim tiler allocation
	   fails, the next matching rule for the scenario will be able to
	   use the buffers already allocated. */

#define MAX_BUFS_TO_PACK 3
	void *buf[MAX_BUFS_TO_PACK];
	int   n_buf, buf_w[MAX_BUFS_TO_PACK];

	char packing[] = {
		/* min(i), max(w), aligned, buffers to alloc */
		5, 16, 0, 2,
			/* buffer widths in a + b * w(y) + c * w(uv) */
			64, 0, 0,  64, 0, 0,
				/* tiler-page offsets in
				   a + b * w(y) + c * w(uv) */
				0,   0, 0,  32,  0, 0,
				16,  0, 0,  40,  0, 0,
				64,  0, 0,  96,  0, 0,
				80,  0, 0,  104, 0, 0,
				112, 0, 0,  56,  0, 0,

		2, 16, 0, 1,
			32, 0, 2,
				0,   0, 0,  32,  0, 0,
				0,   0, 2,  32,  0, 1,

		2, 20, 0, 1,
			42, 1, 0,
				0,   0, 0,  32,  0, 0,
				42,  0, 0,  21,  0, 0,

		3, 24, 0, 2,
			48, 0, 1,  32, 1, 0,
				0,   0, 0,  64,  0, 0,
				24,  0, 0,  76,  0, 0,
				96,  0, 0,  48,  0, 0,

		4, 32, 0, 3,
			48, 0, 1,  32, 1, 0,  32, 1, 0,
				0,   0, 0,  32,  0, 0,
				96,  0, 0,  48,  0, 0,
				64,  0, 0,  128, 0, 0,
				160, 0, 0,  144, 0, 0,

		/* this is needed for soft landing if prior allocation fails
		   after two buffers */
		2, 32, 1, 2,
			32, 0, 1,  32, 0, 1,
				0,  0, 0,  32, 0, 0,
				64, 0, 0,  96, 0, 0,

		1, 32, 1, 1,
			32, 0, 1,
				0, 0, 0,  32, 0, 0,

		2, 64, 1, 3,
			0, 1, 0,  32, 0, 1,  0, 1, 0,
				0,   0, 0,  64, 0, 0,
				128, 0, 0,  96, 0, 0,
		/* this is the basic NV12 allocation using 2 buffers */
		1, 0, 1, 2,
			0, 1, 0,  0, 0, 1,
				0, 0, 0,  64, 0, 0,
		0 };
	int y_width, uv_width, i = 0;

	/* Check input parameters for correctness */
	if (!width || !height || !y_sysptr || !y_allocptr || !count ||
	    !uv_sysptr || !uv_allocptr || *count <= 0) {
		if (count)
			*count = 0;
	}

	y_width = DIVIDE_UP(width, 64);
	uv_width = DIVIDE_UP(width >> 1, 64);

	while (i < *count) {
		int n_alloc = *count - i;
		char *p = packing;
		n_buf = 0;

		/* skip packings that do not apply */
		while (*p) {
			/* see if this packing applies */
			if (p[0] <= n_alloc &&
			    (!p[1] || p[1] >= y_width) &&
			    (!aligned || p[2])) {

				/* allocate buffers */
				while (n_buf < p[3]) {
					buf_w[n_buf] = p[4 + 3 * n_buf] +
						y_width * p[5 + 3 * n_buf] +
						uv_width * p[6 + 3 * n_buf];

					if (0 != tiler_alloc(
						TILFMT_8BIT, buf_w[n_buf] * 64,
						height, (u32 *)buf + n_buf))
						break;
					n_buf++;
				}

				/* if successfully allocated buffers */
				if (n_buf >= p[3]) {
					i = layout_packed_nv12(p + 4 + 3 * p[3],
							       y_width,
							       uv_width,
							       buf, 2 * p[0], i,
							       y_sysptr,
							       uv_sysptr,
							       y_allocptr,
							       uv_allocptr);
					break;
				}
			}

			p += 4 + 3 * p[3] + 6 * p[0];
		}

		/* if allocation failed free any outstanding buffers and stop */
		if (!*p) {
			while (n_buf > 0)
				tiler_free((unsigned long)(buf[--n_buf]));
			break;
		}
	}

	/* mark how many buffers we allocated */
	*count = i;
}
EXPORT_SYMBOL(tiler_packed_alloc_nv12_buf);
