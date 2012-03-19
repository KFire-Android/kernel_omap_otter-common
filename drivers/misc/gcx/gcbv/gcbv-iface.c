/*
 * gcbv-iface.c
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
#include <linux/gcbv.h>
#include "gcbv-priv.h"

static struct bventry ops = {NULL, NULL, NULL};

void gcbv_clear(void)
{
	ops.bv_map = NULL;
	ops.bv_unmap = NULL;
	ops.bv_blt = NULL;
}

void gcbv_assign(void)
{
	ops.bv_map = gcbv_map;
	ops.bv_unmap = gcbv_unmap;
	ops.bv_blt = gcbv_blt;
}

void gcbv_init(struct bventry *entry)
{
	*entry = ops;
}
EXPORT_SYMBOL(gcbv_init);
