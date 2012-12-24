/*
 * bverror.h
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef BVERROR_H
#define BVERROR_H

/*
 * bverror - These are error codes returned by BLTsville functions.
 */
#define BVERRDEF_VENDOR_SHIFT	24
#define BVERRDEF_VENDOR_MASK	(0xFF << BVERRDEF_VENDOR_SHIFT)

#define BVERRDEF_VENDOR_ALL	(0x00 << BVERRDEF_VENDOR_SHIFT)
#define BVERRDEF_VENDOR_TI	(0x01 << BVERRDEF_VENDOR_SHIFT)
/* 0xF0-0xFF reserved */

enum bverror {
	BVERR_NONE = 0,		/* no error */

	BVERR_UNK =		/* unknown error */
		BVERRDEF_VENDOR_ALL + 1,
	BVERR_OOM =		/* memory allocation failure */
		BVERRDEF_VENDOR_ALL + 2,
	BVERR_RSRC =		/* required resource unavailable */
		BVERRDEF_VENDOR_ALL + 3,

	BVERR_VIRTADDR =	/* virtaddr is bad */
		BVERRDEF_VENDOR_ALL + 1000,
	BVERR_VIRTPTR =
		BVERR_VIRTADDR,	/* for backwards compatibility*/

	BVERR_BUFFERDESC =	/* invalid bvbufferdesc */
		BVERRDEF_VENDOR_ALL + 10000,
	BVERR_BUFFERDESC_VERS =	/* bvbufferdesc.structsize too small */
		BVERRDEF_VENDOR_ALL + 11000,
	BVERR_BUFFERDESC_VIRTADDR = /* bad bvbufferdesc.virtaddr */
		BVERRDEF_VENDOR_ALL + 12000,
	BVERR_BUFFERDESC_LEN =	/* bvbufferdesc.length not supported */
		BVERRDEF_VENDOR_ALL + 13000,
	BVERR_BUFFERDESC_ALIGNMENT = /* unsupported buffer base address */
		BVERRDEF_VENDOR_ALL + 14000,

	BVERR_BLTPARAMS_VERS =	/* bvbltparams.structsize too small */
		BVERRDEF_VENDOR_ALL + 20000,
	BVERR_IMPLEMENTATION =	/* bvbltparams.implementation unsupported */
		BVERRDEF_VENDOR_ALL + 21000,
	BVERR_FLAGS =		/* bvbltparams.flags unsupported */
		BVERRDEF_VENDOR_ALL + 22000,
	BVERR_OP =		/* unsupported operation */
		BVERRDEF_VENDOR_ALL + 22100,
	BVERR_KEY =		/* type of color key not supported */
		BVERRDEF_VENDOR_ALL + 22200,
	BVERR_SRC1_TILE =	/* src1 tiling not supported */
		BVERRDEF_VENDOR_ALL + 22300,
	BVERR_SRC2_TILE =	/* src2 tiling not supported */
		BVERRDEF_VENDOR_ALL + 22310,
	BVERR_MASK_TILE =	/* mask tiling not supported */
		BVERRDEF_VENDOR_ALL + 22320,
	BVERR_FLIP =		/* flipping not supported */
		BVERRDEF_VENDOR_ALL + 22400,
	BVERR_ROP =		/* ROP code not supported */
		BVERRDEF_VENDOR_ALL + 23000,
	BVERR_BLEND =		/* blend not supported */
		BVERRDEF_VENDOR_ALL + 23100,
	BVERR_GLOBAL_ALPHA =	/* type of global alpha not supported */
		BVERRDEF_VENDOR_ALL + 23110,
	BVERR_FILTER =		/* filter type not supported */
		BVERRDEF_VENDOR_ALL + 23200,
	BVERR_FILTER_PARAMS_VERS = /* filter parameter structsize too small */
		BVERRDEF_VENDOR_ALL + 23210,
	BVERR_FILTER_PARAMS =	/* filter parameters not supported */
		BVERRDEF_VENDOR_ALL + 23220,
	BVERR_SCALE_MODE =	/* bvbltparams.scalemode not supported */
		BVERRDEF_VENDOR_ALL + 24000,
	BVERR_DITHER_MODE =	/* bvbltparams.dithermode not supported */
		BVERRDEF_VENDOR_ALL + 25000,

	BVERR_DSTDESC =		/* invalid bvbltparams.dstdesc */
		BVERRDEF_VENDOR_ALL + 26000,
	BVERR_DSTDESC_VERS =	/* bvbufferdesc.structsize too small */
		BVERRDEF_VENDOR_ALL + 26100,
	BVERR_DSTDESC_VIRTADDR = /* bad bvbufferdesc.virtaddr */
		BVERRDEF_VENDOR_ALL + 26200,
	BVERR_DSTDESC_LEN =	/* bvbufferdesc.length not supported */
		BVERRDEF_VENDOR_ALL + 26300,
	BVERR_DST_ALIGNMENT =	/* unsupported buffer base address */
		BVERRDEF_VENDOR_ALL + 26400,

	BVERR_DSTGEOM =		/* invalid bvbltparams.dstgeom */
		BVERRDEF_VENDOR_ALL + 27000,
	BVERR_DSTGEOM_VERS =	/* dstgeom.structsize too small */
		BVERRDEF_VENDOR_ALL + 27100,
	BVERR_DSTGEOM_FORMAT =	/* bltparams.dstgeom.format not supported */
		BVERRDEF_VENDOR_ALL + 27200,
	BVERR_DSTGEOM_STRIDE =	/* bltparams.dstgeom.stride not supported */
		BVERRDEF_VENDOR_ALL + 27300,
	BVERR_DSTGEOM_PALETTE =	/* dstgeom.paletteformat not supported */
		BVERRDEF_VENDOR_ALL + 27400,


	BVERR_DSTRECT =		/* bvbltparams.dstrect not supported */
		BVERRDEF_VENDOR_ALL + 28000,

	BVERR_SRC1DESC =	/* invalid bvbltparams.src1.desc */
		BVERRDEF_VENDOR_ALL + 29000,
	BVERR_SRC1DESC_VERS =	/* bvbufferdesc.structsize too small */
		BVERRDEF_VENDOR_ALL + 29100,
	BVERR_SRC1DESC_VIRTADDR = /* bad bvbufferdesc.virtaddr */
		BVERRDEF_VENDOR_ALL + 29200,
	BVERR_SRC1DESC_LEN =	/* bvbufferdesc.length not supported */
		BVERRDEF_VENDOR_ALL + 29300,
	BVERR_SRC1DESC_ALIGNMENT = /* unsupported buffer base address */
		BVERRDEF_VENDOR_ALL + 29400,

	BVERR_SRC1GEOM =	/* invalid bvbltparams.src1geom */
		BVERRDEF_VENDOR_ALL + 30000,
	BVERR_SRC1GEOM_VERS =	/* src1geom.structsize too small */
		BVERRDEF_VENDOR_ALL + 30100,
	BVERR_SRC1GEOM_FORMAT =	/* bltparams.src1geom.format not supported */
		BVERRDEF_VENDOR_ALL + 30200,
	BVERR_SRC1GEOM_STRIDE =	/* bltparams.src1geom.stride not supported */
		BVERRDEF_VENDOR_ALL + 30300,
	BVERR_SRC1GEOM_PALETTE = /* src1geom.paletteformat not supported */
		BVERRDEF_VENDOR_ALL + 30400,

	BVERR_SRC1RECT =	/* bvbltparams.src1rect not supported */
		BVERRDEF_VENDOR_ALL + 31000,

	BVERR_SRC1_HORZSCALE = /* horz scale for src1->dst not supported */
		BVERRDEF_VENDOR_ALL + 31100,
	BVERR_SRC1_VERTSCALE =	/* vert scale for src1->dst not supported */
		BVERRDEF_VENDOR_ALL + 31200,
	BVERR_SRC1_ROT =	/* src1->dst rotation angle not supported */
		BVERRDEF_VENDOR_ALL + 31300,

	BVERR_SRC1_TILEPARAMS =	/* invalid src1.tileparams */
		BVERR_SRC1DESC,
	BVERR_SRC1_TILE_VERS =	/* src1.tileparams.structsize too small */
		BVERRDEF_VENDOR_ALL + 32000,
	BVERR_SRC1_TILEPARAMS_VERS =
		BVERR_SRC1_TILE_VERS,
	BVERR_SRC1_TILE_FLAGS =	/* tileparams.flags not supported */
		BVERRDEF_VENDOR_ALL + 32100,
	BVERR_SRC1_TILEPARAMS_FLAGS =
		BVERR_SRC1_TILE_FLAGS,
	BVERR_SRC1_TILE_VIRTADDR =
		BVERR_SRC1DESC_VIRTADDR,
	BVERR_SRC1_TILEPARAMS_VIRTADDR =
		BVERR_SRC1_TILE_VIRTADDR,
	BVERR_SRC1_TILE_ORIGIN = /* tileparams.left or .top not supported */
		BVERRDEF_VENDOR_ALL + 32200,
	BVERR_SRC1_TILEPARAMS_ORIGIN =
		BVERR_SRC1_TILE_ORIGIN,
	BVERR_SRC1_TILE_SIZE =	/* tileparams.width or .height not supported */
		BVERRDEF_VENDOR_ALL + 32300,
	BVERR_SRC1_TILEPARAMS_SIZE =
		BVERR_SRC1_TILE_SIZE,

	BVERR_SRC2DESC =	/* invalid bvbltparams.src2.desc */
		BVERRDEF_VENDOR_ALL + 33000,
	BVERR_SRC2DESC_VERS =	/* bvbufferdesc.structsize too small */
		BVERRDEF_VENDOR_ALL + 33100,
	BVERR_SRC2DESC_VIRTADDR = /* bad bvbufferdesc.virtaddr */
		BVERRDEF_VENDOR_ALL + 33200,
	BVERR_SRC2DESC_LEN =	/* bvbufferdesc.length not supported */
		BVERRDEF_VENDOR_ALL + 33300,
	BVERR_SRC2DESC_ALIGNMENT = /* unsupported buffer base address */
		BVERRDEF_VENDOR_ALL + 33400,

	BVERR_SRC2GEOM =	/* invalid bvbltparams.src2geom */
		BVERRDEF_VENDOR_ALL + 34000,
	BVERR_SRC2GEOM_VERS =	/* src2geom.structsize too small */
		BVERRDEF_VENDOR_ALL + 34100,
	BVERR_SRC2GEOM_FORMAT =	/* bltparams.src2geom.format not supported */
		BVERRDEF_VENDOR_ALL + 34200,
	BVERR_SRC2GEOM_STRIDE =	/* bltparams.src2geom.stride not supported */
		BVERRDEF_VENDOR_ALL + 34300,
	BVERR_SRC2GEOM_PALETTE = /* src2geom.paletteformat not supported */
		BVERRDEF_VENDOR_ALL + 34400,

	BVERR_SRC2RECT =	/* bvbltparams.src2rect not supported */
		BVERRDEF_VENDOR_ALL + 35000,

	BVERR_SRC2_HORZSCALE = /* horz scale for src2->dst not supported */
		BVERRDEF_VENDOR_ALL + 35100,
	BVERR_SRC2_VERTSCALE =	/* vert scale for src2->dst not supported */
		BVERRDEF_VENDOR_ALL + 35200,
	BVERR_SRC2_ROT =	/* src2->dst rotation angle not supported */
		BVERRDEF_VENDOR_ALL + 35300,

	BVERR_SRC2_TILEPARAMS =	/* invalid src2.tileparams */
		BVERR_SRC2DESC,
	BVERR_SRC2_TILE_VERS =	/* src2.tileparams.structsize too small */
		BVERRDEF_VENDOR_ALL + 36000,
	BVERR_SRC2_TILEPARAMS_VERS =
		BVERR_SRC2_TILE_VERS,
	BVERR_SRC2_TILE_FLAGS =	/* tileparams.flags not supported */
		BVERRDEF_VENDOR_ALL + 36100,
	BVERR_SRC2_TILEPARAMS_FLAGS =
		BVERR_SRC2_TILE_FLAGS,
	BVERR_SRC2_TILE_VIRTADDR =
		BVERR_SRC2DESC_VIRTADDR,
	BVERR_SRC2_TILEPARAMS_VIRTADDR =
		BVERR_SRC2_TILE_VIRTADDR,
	BVERR_SRC2_TILE_ORIGIN = /* tileparams.left or .top not supported */
		BVERRDEF_VENDOR_ALL + 36200,
	BVERR_SRC2_TILEPARAMS_ORIGIN =
		BVERR_SRC2_TILE_ORIGIN,
	BVERR_SRC2_TILE_SIZE =	/* tileparams.width or .height not supported */
		BVERRDEF_VENDOR_ALL + 36300,
	BVERR_SRC2_TILEPARAMS_SIZE =
		BVERR_SRC2_TILE_SIZE,

	BVERR_MASKDESC =	/* invalid bvbltparams.mask.desc */
		BVERRDEF_VENDOR_ALL + 37000,
	BVERR_MASKDESC_VERS =	/* bvbufferdesc.structsize too small */
		BVERRDEF_VENDOR_ALL + 37100,
	BVERR_MASKDESC_VIRTADDR = /* bad bvbufferdesc.virtaddr */
		BVERRDEF_VENDOR_ALL + 37200,
	BVERR_MASKDESC_LEN =	/* bvbufferdesc.length not supported */
		BVERRDEF_VENDOR_ALL + 37300,
	BVERR_MASKDESC_ALIGNMENT = /* unsupported buffer base address */
		BVERRDEF_VENDOR_ALL + 37400,

	BVERR_MASKGEOM =	/* invalid bvbltparams.maskgeom */
		BVERRDEF_VENDOR_ALL + 38000,
	BVERR_MASKGEOM_VERS =	/* maskgeom.structsize too small */
		BVERRDEF_VENDOR_ALL + 38100,
	BVERR_MASKGEOM_FORMAT =	/* bltparams.maskgeom.format not supported */
		BVERRDEF_VENDOR_ALL + 38200,
	BVERR_MASKGEOM_STRIDE =	/* bltparams.maskgeom.stride not supported */
		BVERRDEF_VENDOR_ALL + 38300,
	BVERR_MASKGEOM_PALETTE = /* maskgeom.paletteformat not supported */
		BVERRDEF_VENDOR_ALL + 38400,

	BVERR_MASKRECT =	/* bvbltparams.maskrect not supported */
		BVERRDEF_VENDOR_ALL + 39000,

	BVERR_MASK_HORZSCALE = /* horz scale for mask->dst not supported */
		BVERRDEF_VENDOR_ALL + 39100,
	BVERR_MASK_VERTSCALE =	/* vert scale for mask->dst not supported */
		BVERRDEF_VENDOR_ALL + 39200,
	BVERR_MASK_ROT =	/* mask->dst rotation angle not supported */
		BVERRDEF_VENDOR_ALL + 39300,

	BVERR_MASK_TILEPARAMS =	/* invalid mask.tileparams */
		BVERR_MASKDESC,
	BVERR_MASK_TILE_VERS =	/* mask.tileparams.structsize too small */
		BVERRDEF_VENDOR_ALL + 40000,
	BVERR_MASK_TILEPARAMS_VERS =
		BVERR_MASK_TILE_VERS,
	BVERR_MASK_TILE_FLAGS =	/* tileparams.flags not supported */
		BVERRDEF_VENDOR_ALL + 40100,
	BVERR_MASK_TILEPARAMS_FLAGS =
		BVERR_MASK_TILE_FLAGS,
	BVERR_MASK_TILE_VIRTADDR =
		BVERR_MASKDESC_VIRTADDR,
	BVERR_MASK_TILEPARAMS_VIRTADDR =
		BVERR_MASK_TILE_VIRTADDR,
	BVERR_MASK_TILE_ORIGIN = /* tileparams.left or .top not supported */
		BVERRDEF_VENDOR_ALL + 40200,
	BVERR_MASK_TILEPARAMS_ORIGIN =
		BVERR_MASK_TILE_ORIGIN,
	BVERR_MASK_TILE_SIZE =	/* tileparams.width or .height not supported */
		BVERRDEF_VENDOR_ALL + 40300,
	BVERR_MASK_TILEPARAMS_SIZE =
		BVERR_MASK_TILE_SIZE,

	BVERR_CLIP_RECT =	/* bvbltparams.cliprect not supported */
		BVERRDEF_VENDOR_ALL + 41000,

	BVERR_BATCH_FLAGS =	/* bvbltparams.batchflags not supported */
		BVERRDEF_VENDOR_ALL + 42000,
	BVERR_BATCH =		/* bvbltparams.batch not valid */
		BVERRDEF_VENDOR_ALL + 43000,

	BVERR_OP_FAILED =	/* async operation failed to start */
		BVERRDEF_VENDOR_ALL + 50000,
	BVERR_OP_INCOMPLETE =	/* async operation failed mid-way */
		BVERRDEF_VENDOR_ALL + 50001,
	BVERR_MEMORY_ERROR =	/* async operation triggered memory error */
		BVERRDEF_VENDOR_ALL + 51000,

	BVERR_FORMAT =		/* unsupported format */
		BVERRDEF_VENDOR_ALL + 52000,

	BVERR_CACHEOP =		/* unsupported cache operation */
		BVERRDEF_VENDOR_ALL + 60000,

#ifdef BVERR_EXTERNAL_INCLUDE
#include BVERR_EXTERNAL_INCLUDE
#endif
};

#endif /* BVERROR_H */
