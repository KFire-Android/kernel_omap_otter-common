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

#include <plat/cpu.h>
#include "gcmain.h"


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
