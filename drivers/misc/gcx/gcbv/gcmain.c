/*
 * gcmain.c
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

#include "gcbv.h"


/*******************************************************************************
 * BLTsville interface exposure.
 */

static struct bventry ops = {
	.structsize = sizeof(struct bventry),
};

static void gcbv_clear(void)
{
	ops.bv_map = NULL;
	ops.bv_unmap = NULL;
	ops.bv_blt = NULL;
	ops.bv_cache = NULL;
}

static void gcbv_assign(void)
{
	ops.bv_map = bv_map;
	ops.bv_unmap = bv_unmap;
	ops.bv_blt = bv_blt;
	ops.bv_cache = bv_cache;
}

void gcbv_init(struct bventry *entry)
{
	*entry = ops;
}
EXPORT_SYMBOL(gcbv_init);


/*******************************************************************************
 * Convert floating point in 0..1 range to an 8-bit value in range 0..255.
 */

union gcfp {
	struct {
		unsigned int mantissa:23;
		unsigned int exponent:8;
		unsigned int sign:1;
	} comp;

	float value;
};

unsigned char gcfp2norm8(float value)
{
	union gcfp gcfp;
	int exponent;
	unsigned int mantissa;
	int shift;

	/* Get access to components. */
	gcfp.value = value;

	/* Clamp negatives. */
	if (gcfp.comp.sign)
		return 0;

	/* Get unbiased exponent. */
	exponent = (int) gcfp.comp.exponent - 127;

	/* Clamp if too large. */
	if (exponent >= 0)
		return 255;

	/* Clamp if too small. */
	if (exponent < -8)
		return 0;

	/* Determine the shift value. */
	shift = (23 - 8) - exponent;

	/* Compute the mantissa. */
	mantissa = (gcfp.comp.mantissa | 0x00800000) >> shift;

	/* Normalize. */
	mantissa = (mantissa * 255) >> 8;

	return (unsigned char) mantissa;
}


/*******************************************************************************
 * Surface allocation.
 */

enum bverror allocate_surface(struct bvbuffdesc **bvbuffdesc,
			      void **buffer,
			      unsigned int size)
{
	enum bverror bverror;
	struct bvbuffdesc *tempbuffdesc = NULL;
	void *tempbuff = NULL;
	unsigned long *temparray = NULL;
	struct bvphysdesc *tempphysdesc = NULL;
	unsigned char *pageaddr;
	unsigned int i;

	/* Allocate surface buffer descriptor. */
	tempbuffdesc = vmalloc(sizeof(struct bvbuffdesc));
	if (tempbuffdesc == NULL) {
		BVSETERROR(BVERR_OOM, "failed to allocate surface");
		goto exit;
	}

	/* Initialize buffer descriptor. */
	tempbuffdesc->structsize = sizeof(struct bvbuffdesc);
	tempbuffdesc->virtaddr = NULL;
	tempbuffdesc->length = size;
	tempbuffdesc->map = NULL;
	tempbuffdesc->auxtype = BVAT_PHYSDESC;
	tempbuffdesc->auxptr = NULL;

	/* Allocate the surface. */
	tempbuff = vmalloc(size);
	if (tempbuff == NULL) {
		BVSETERROR(BVERR_OOM, "failed to allocate surface");
		goto exit;
	}
	tempbuffdesc->virtaddr = tempbuff;

	/* Allocate the physical descriptor. */
	tempphysdesc = vmalloc(sizeof(struct bvphysdesc));
	if (tempphysdesc == NULL) {
		BVSETERROR(BVERR_OOM, "failed to allocate surface");
		goto exit;
	}
	tempbuffdesc->auxptr = tempphysdesc;

	/* Initialize physical descriptor. */
	tempphysdesc->structsize = sizeof(struct bvphysdesc);
	tempphysdesc->pagesize = PAGE_SIZE;
	tempphysdesc->pagearray = NULL;
	tempphysdesc->pagecount = (size + PAGE_SIZE - 1) / PAGE_SIZE;
	tempphysdesc->pageoffset = 0;

	/* Allocate array of pages. */
	temparray = vmalloc(tempphysdesc->pagecount * sizeof(unsigned long));
	if (temparray == NULL) {
		BVSETERROR(BVERR_OOM, "failed to allocate surface");
		goto exit;
	}
	tempphysdesc->pagearray = temparray;

	/* Initialize the array. */
	pageaddr = (unsigned char *) tempbuff;
	for (i = 0; i < tempphysdesc->pagecount; i += 1) {
		temparray[i] = PFN_PHYS(vmalloc_to_pfn(pageaddr));
		pageaddr += PAGE_SIZE;
	}

	/* Set return pointers. */
	*bvbuffdesc = tempbuffdesc;
	*buffer = tempbuff;
	return BVERR_NONE;

exit:
	free_surface(tempbuffdesc, tempbuff);
	return bverror;
}

void free_surface(struct bvbuffdesc *bvbuffdesc,
		  void *buffer)
{
	if (bvbuffdesc != NULL) {
		if (bvbuffdesc->virtaddr != NULL)
			vfree(bvbuffdesc->virtaddr);

		if (bvbuffdesc->auxptr != NULL) {
			struct bvphysdesc *bvphysdesc;

			bvphysdesc = (struct bvphysdesc *) bvbuffdesc->auxptr;
			if (bvphysdesc->pagearray != NULL)
				vfree(bvphysdesc->pagearray);

			vfree(bvphysdesc);
		}

		vfree(bvbuffdesc);
	}
}


/*******************************************************************************
 * Cache operation wrapper.
 */

enum bverror gcbvcacheop(int count, struct c2dmrgn rgn[],
			 enum bvcacheop cacheop)
{
	enum bverror err = BVERR_NONE;

	switch (cacheop) {

	case DMA_FROM_DEVICE:
		c2dm_l2cache(count, rgn, cacheop);
		c2dm_l1cache(count, rgn, cacheop);
		break;

	case DMA_TO_DEVICE:
		c2dm_l1cache(count, rgn, cacheop);
		c2dm_l2cache(count, rgn, cacheop);
		break;

	case DMA_BIDIRECTIONAL:
		c2dm_l1cache(count, rgn, cacheop);
		c2dm_l2cache(count, rgn, cacheop);
		break;

	default:
		err = BVERR_CACHEOP;
		break;
	}

	return err;
}


/*******************************************************************************
 * Device init/cleanup.
 */

static int __init mod_init(void)
{
	bv_init();

	/* Assign BV function parameters only if SoC contains a GC core */
	if (cpu_is_omap447x())
		gcbv_assign();

	return 0;
}

static void __exit mod_exit(void)
{
	gcbv_clear();
	bv_exit();
}

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("www.vivantecorp.com");
MODULE_AUTHOR("www.ti.com");
module_init(mod_init);
module_exit(mod_exit);
