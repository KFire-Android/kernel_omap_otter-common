/*
 * gcbvdebug.c
 *
 * Copyright (C) 2010-2011 Vivante Corporation.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "gcbv.h"

static struct dentry *debug_root;

/*****************************************************************************/

static struct {
	unsigned int srccount;
	unsigned int multisrc;
	unsigned int align;
	unsigned int flags_dst;
	unsigned int flags_destrect;
	unsigned int flags_cliprect;

} finalize_batch_reason;

static int bfr_show(struct seq_file *s, void *data)
{
	seq_printf(s, "      srccount: %d\n",
		   finalize_batch_reason.srccount);
	seq_printf(s, "      multisrc: %d\n",
		   finalize_batch_reason.multisrc);
	seq_printf(s, "         align: %d\n",
		   finalize_batch_reason.align);
	seq_printf(s, "     flags_dst: %d\n",
		   finalize_batch_reason.flags_dst);
	seq_printf(s, "flags_destrect: %d\n",
		   finalize_batch_reason.flags_destrect);
	seq_printf(s, "flags_cliprect: %d\n",
		   finalize_batch_reason.flags_cliprect);

	memset(&finalize_batch_reason, 0, sizeof(finalize_batch_reason));

	return 0;
}

static int bfr_open(struct inode *inode, struct file *file)
{
	return single_open(file, bfr_show, 0);
}

static const struct file_operations fops_bfr = {
	.open    = bfr_open,
	.write   = NULL,
	.read    = seq_read,
	.llseek  = seq_lseek,
	.release = single_release,
};

void gcbv_debug_finalize_batch(unsigned int reason)
{
	if (reason & GCBV_BATCH_FINALIZE_SRCCOUNT)
		finalize_batch_reason.srccount++;

	if (reason & GCBV_BATCH_FINALIZE_MULTISRC)
		finalize_batch_reason.multisrc++;

	if (reason & GCBV_BATCH_FINALIZE_ALIGN)
		finalize_batch_reason.align++;

	if (reason & GCBV_BATCH_FINALIZE_FLAGS_DST)
		finalize_batch_reason.flags_dst++;

	if (reason & GCBV_BATCH_FINALIZE_FLAGS_CLIPRECT)
		finalize_batch_reason.flags_cliprect++;

	if (reason & GCBV_BATCH_FINALIZE_FLAGS_DESTRECT)
		finalize_batch_reason.flags_destrect++;
}

/*****************************************************************************/

#define MAX_BLT_SOURCES   8

static struct gc_blt_status {
	int totalCount;
	long long int totalPixels;
	int srcCount[MAX_BLT_SOURCES + 1];
	long long int srcCountPixels[MAX_BLT_SOURCES + 1];

	int two_dim_one_pass;
	int two_dim_two_pass;
	int one_dim;

} blt_stats;

void gcbv_debug_blt(int srccount, int dstWidth, int dstHeight)
{
	int pixels;

	if (srccount > MAX_BLT_SOURCES)
		return;

	pixels = dstWidth * dstHeight;

	blt_stats.srcCount[srccount]++;
	blt_stats.srcCountPixels[srccount] += pixels;

	blt_stats.totalPixels += pixels;
	blt_stats.totalCount++;
}

void gcbv_debug_scaleblt(bool scalex, bool scaley, bool singlepass)
{
	if (scalex && scaley) {
		if (singlepass)
			blt_stats.two_dim_one_pass++;
		else
			blt_stats.two_dim_two_pass++;
	} else
		blt_stats.one_dim++;
}

static void blt_stats_reset(void)
{
	int i;

	for (i = 1; i <= MAX_BLT_SOURCES; i++) {
		blt_stats.srcCount[i] = 0;
		blt_stats.srcCountPixels[i] = 0;
	}

	blt_stats.totalCount = 0;
	blt_stats.totalPixels = 0;

	blt_stats.two_dim_one_pass = 0;
	blt_stats.two_dim_two_pass = 0;
	blt_stats.one_dim = 0;
}

static int blt_stats_show(struct seq_file *s, void *data)
{
	int i;

	seq_printf(s, "total blts: %d\n", blt_stats.totalCount);

	if (blt_stats.totalCount) {
		for (i = 1; i <= MAX_BLT_SOURCES; i++) {
			int count = blt_stats.srcCount[i];

			seq_printf(s, " %d src: %d (%d%%)\n",
				   i,
				   count,
				   count * 100 / blt_stats.totalCount);
		}
	}

	seq_printf(s, "total dst pixels: %lld\n", blt_stats.totalPixels);

	if (blt_stats.totalPixels) {
		for (i = 1; i <= MAX_BLT_SOURCES; i++) {
			long long int count = blt_stats.srcCountPixels[i];
			long long int total = blt_stats.totalPixels;

			seq_printf(s, " %d src: %lld (%lld%%)\n",
				   i,
				   count,
				   div64_s64(count * 100, total));
		}
	}

	seq_printf(s, "scaled 2x2: %d\n", blt_stats.two_dim_two_pass);
	seq_printf(s, "scaled 2x1: %d\n", blt_stats.two_dim_one_pass);
	seq_printf(s, "scaled 1x1: %d\n", blt_stats.one_dim);

	blt_stats_reset();

	return 0;
}

static int blt_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, blt_stats_show, 0);
}

static const struct file_operations fops_blt_stats = {
	.open = blt_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*****************************************************************************/

void gcbv_debug_init(void)
{
	debug_root = debugfs_create_dir("gcbv", NULL);
	if (!debug_root)
		return;

	debugfs_create_file("blt_stats", 0664, debug_root, NULL,
			    &fops_blt_stats);
	debugfs_create_file("batch_finalize_reason", 0664, debug_root, NULL,
			    &fops_bfr);
}

void gcbv_debug_shutdown(void)
{
	if (debug_root)
		debugfs_remove_recursive(debug_root);
}
