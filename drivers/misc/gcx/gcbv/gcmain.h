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

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/gcx.h>
#include <linux/gcioctl.h>
#include <linux/gccore.h>
#include <linux/bltsville.h>
#include <linux/bvinternal.h>

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

#define gc_getcaps_wrapper(gcicaps) \
	gc_caps(gcicaps)

#define gc_commit_wrapper(gcicommit) \
	gc_commit(gcicommit, false)

#define gc_map_wrapper(gcimap) \
	gc_map(gcimap, false)

#define gc_unmap_wrapper(gcimap) \
	gc_unmap(gcimap, false)

#define gc_callback_wrapper(gcicallbackarm) \
	gc_callback(gcicallbackarm, false)


/*******************************************************************************
 * Surface allocation.
 */

enum bverror allocate_surface(struct bvbuffdesc **bvbuffdesc,
			      void **buffer,
			      unsigned int size);

void free_surface(struct bvbuffdesc *bvbuffdesc,
		  void *buffer);


/*******************************************************************************
 * Floating point conversions.
 */

unsigned char gcfp2norm8(float value);


/*******************************************************************************
 * Cache operation wrapper.
 */

enum bverror gcbvcacheop(int count, struct c2dmrgn rgn[],
			 enum bvcacheop cacheop);


/*******************************************************************************
 * BLTsville API.
 */

void bv_init(void);
void bv_exit(void);

enum bverror bv_map(struct bvbuffdesc *buffdesc);
enum bverror bv_unmap(struct bvbuffdesc *buffdesc);
enum bverror bv_blt(struct bvbltparams *bltparams);
enum bverror bv_cache(struct bvcopparams *copparams);

#endif
