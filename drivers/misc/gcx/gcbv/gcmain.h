/*
 * gcmain.h
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

#ifndef GCMAIN_H
#define GCMAIN_H

#include <linux/gcx.h>
#include <linux/gcioctl.h>
#include <linux/gcbv.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gccore.h>
#include <linux/gcdebug.h>

#define GC_DEV_NAME	"gc2d"

/*******************************************************************************
 * Miscellaneous macros.
 */

#define gcalloc(type, size) \
	kmalloc(size, GFP_KERNEL)

#define gcfree(ptr) \
	kfree(ptr)

/*******************************************************************************
 * Core driver API definitions.
 */

#define gc_map_wrapper(gcmap) \
	gc_map(gcmap)

#define gc_unmap_wrapper(gcmap) \
	gc_unmap(gcmap)

#define gc_commit_wrapper(gccommit) \
	gc_commit(gccommit, false)

/*******************************************************************************
 * Floating point conversions.
 */

unsigned char gcfp2norm8(float value);

/*******************************************************************************
 * BLTsville initialization/cleanup.
 */

void bv_init(void);
void bv_exit(void);

#endif
