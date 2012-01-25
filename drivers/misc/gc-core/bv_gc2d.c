/*
 * bv_gc2d.c
 *
 * Copyright (C) 2011, Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/bltsville.h>
#include <linux/bv_gc2d.h>
#include "bv_gc2d-priv.h"

static struct bventry ops = {NULL, NULL, NULL};

void bv_gc2d_clearentry(void)
{
	ops.bv_map = NULL;
	ops.bv_unmap = NULL;
	ops.bv_blt = NULL;
}

void bv_gc2d_fillentry(void)
{
	ops.bv_map = bv_gc2d_map;
	ops.bv_unmap = bv_gc2d_unmap;
	ops.bv_blt = bv_gc2d_blt;
}

void bv_gc2d_getentry(struct bventry *entry)
{
	*entry = ops;
}
EXPORT_SYMBOL(bv_gc2d_getentry);
