/*
 * This file is provided under a dual BSD/GPLv2 license.  When using or
 * redistributing this file, you may do so under either license.
 *
 * GPL LICENSE SUMMARY
 *
 * Copyright(c) 2012 Vivante Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 *
 * BSD LICENSE
 *
 * Copyright(c) 2012 Vivante Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Vivante Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "gcbv.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_FORMAT		(1 << 0)
#define GCZONE_FORMAT_VERBOSE	(1 << 1)
#define GCZONE_BLEND		(1 << 2)
#define GCZONE_OFFSET		(1 << 3)
#define GCZONE_DEST		(1 << 4)
#define GCZONE_SRC		(1 << 5)
#define GCZONE_SCALING		(1 << 6)

GCDBG_FILTERDEF(parser, GCZONE_NONE,
		"format",
		"formatverbose",
		"blend",
		"offset",
		"dest",
		"src",
		"scaling")


/*******************************************************************************
 * Internal macros.
 */

#define GCCONVERT_RECT(zone, name, bvrect, gcrect) \
{ \
	(gcrect)->left = (bvrect)->left; \
	(gcrect)->top = (bvrect)->top; \
	(gcrect)->right = (bvrect)->left + (bvrect)->width; \
	(gcrect)->bottom = (bvrect)->top + (bvrect)->height; \
	\
	GCPRINT_RECT(zone, name, gcrect); \
}


/*******************************************************************************
 * Pixel format parser.
 */

#define OCDFMTDEF_PLACEMENT_SHIFT 9
#define OCDFMTDEF_PLACEMENT_MASK (3 << OCDFMTDEF_PLACEMENT_SHIFT)

#define BVCOMP(Shift, Size) \
	{ Shift, Size, ((1 << Size) - 1) << Shift }

#define BVRED(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVGREEN(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVBLUE(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVALPHA(Shift, Size) \
	BVCOMP(Shift, Size)

static const unsigned int rgba16swizzle[] = {
	GCREG_DE_SWIZZLE_ARGB,
	GCREG_DE_SWIZZLE_RGBA,
	GCREG_DE_SWIZZLE_ABGR,
	GCREG_DE_SWIZZLE_BGRA
};

static const unsigned int rgb16swizzle[] = {
	GCREG_DE_SWIZZLE_ARGB,
	GCREG_DE_SWIZZLE_ARGB,
	GCREG_DE_SWIZZLE_ABGR,
	GCREG_DE_SWIZZLE_ABGR
};

static const unsigned int rgba32swizzle[] = {
	GCREG_DE_SWIZZLE_BGRA,
	GCREG_DE_SWIZZLE_ABGR,
	GCREG_DE_SWIZZLE_RGBA,
	GCREG_DE_SWIZZLE_ARGB
};

static const struct bvcsrgb xrgb4444_bits[] = {
	{ BVRED(8,  4), BVGREEN(4, 4), BVBLUE(0,  4), BVALPHA(12, 0) },
	{ BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4,  4), BVALPHA(0,  0) },
	{ BVRED(0,  4), BVGREEN(4, 4), BVBLUE(8,  4), BVALPHA(12, 0) },
	{ BVRED(4,  4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0,  0) }
};

static const struct bvcsrgb argb4444_bits[] = {
	{ BVRED(8,  4), BVGREEN(4, 4), BVBLUE(0,  4), BVALPHA(12, 4) },
	{ BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4,  4), BVALPHA(0,  4) },
	{ BVRED(0,  4), BVGREEN(4, 4), BVBLUE(8,  4), BVALPHA(12, 4) },
	{ BVRED(4,  4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0,  4) }
};

static const struct bvcsrgb xrgb1555_bits[] = {
	{ BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0,  5), BVALPHA(15, 0) },
	{ BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1,  5), BVALPHA(0,  0) },
	{ BVRED(0,  5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 0) },
	{ BVRED(1,  5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0,  0) }
};

static const struct bvcsrgb argb1555_bits[] = {
	{ BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0,  5), BVALPHA(15, 1) },
	{ BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1,  5), BVALPHA(0,  1) },
	{ BVRED(0,  5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 1) },
	{ BVRED(1,  5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0,  1) }
};

static const struct bvcsrgb rgb565_bits[] = {
	{ BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0,  5), BVALPHA(0, 0) },
	{ BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0,  5), BVALPHA(0, 0) },
	{ BVRED(0,  5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0) },
	{ BVRED(0,  5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0) }
};

static const struct bvcsrgb xrgb8888_bits[] = {
	{ BVRED(8,  8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0,  0) },
	{ BVRED(0,  8), BVGREEN(8,  8), BVBLUE(16, 8), BVALPHA(24, 0) },
	{ BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8,  8), BVALPHA(0,  0) },
	{ BVRED(16, 8), BVGREEN(8,  8), BVBLUE(0,  8), BVALPHA(24, 0) }
};

static const struct bvcsrgb argb8888_bits[] = {
	{ BVRED(8,  8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0,  8) },
	{ BVRED(0,  8), BVGREEN(8,  8), BVBLUE(16, 8), BVALPHA(24, 8) },
	{ BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8,  8), BVALPHA(0,  8) },
	{ BVRED(16, 8), BVGREEN(8,  8), BVBLUE(0,  8), BVALPHA(24, 8) }
};

static const unsigned int container[] = {
	  8,	/* OCDFMTDEF_CONTAINER_8BIT */
	 16,	/* OCDFMTDEF_CONTAINER_16BIT */
	 24,	/* OCDFMTDEF_CONTAINER_24BIT */
	 32,	/* OCDFMTDEF_CONTAINER_32BIT */
	~0U,	/* reserved */
	 48,	/* OCDFMTDEF_CONTAINER_48BIT */
	~0U,	/* reserved */
	 64	/* OCDFMTDEF_CONTAINER_64BIT */
};

enum bverror parse_format(struct bvbltparams *bvbltparams,
			  struct surfaceinfo *surfaceinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct bvformatxlate *format;
	enum ocdformat ocdformat;
	unsigned int cs, std, alpha, subsample, layout;
	unsigned int reversed, leftjust, swizzle, cont, bits;

	format = &surfaceinfo->format;
	ocdformat = surfaceinfo->geom->format;
	GCENTERARG(GCZONE_FORMAT, "ocdformat = 0x%08X\n", ocdformat);

	cs = (ocdformat & OCDFMTDEF_CS_MASK)
		>> OCDFMTDEF_CS_SHIFT;
	std = (ocdformat & OCDFMTDEF_STD_MASK)
		>> OCDFMTDEF_STD_SHIFT;
	alpha = ocdformat & OCDFMTDEF_ALPHA;
	subsample = (ocdformat & OCDFMTDEF_SUBSAMPLE_MASK)
		>> OCDFMTDEF_SUBSAMPLE_SHIFT;
	layout = (ocdformat & OCDFMTDEF_LAYOUT_MASK)
		>> OCDFMTDEF_LAYOUT_SHIFT;
	cont = (ocdformat & OCDFMTDEF_CONTAINER_MASK)
		>> OCDFMTDEF_CONTAINER_SHIFT;
	bits = ((ocdformat & OCDFMTDEF_COMPONENTSIZEMINUS1_MASK)
		>> OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT) + 1;

	GCDBG(GCZONE_FORMAT_VERBOSE, "std = %d\n", std);
	GCDBG(GCZONE_FORMAT_VERBOSE, "cs = %d\n", cs);
	GCDBG(GCZONE_FORMAT_VERBOSE, "alpha = %d\n", alpha ? 1 : 0);
	GCDBG(GCZONE_FORMAT_VERBOSE, "subsample = %d\n", subsample);
	GCDBG(GCZONE_FORMAT_VERBOSE, "layout = %d\n", layout);
	GCDBG(GCZONE_FORMAT_VERBOSE, "cont = %d\n", cont);
	GCDBG(GCZONE_FORMAT_VERBOSE, "bits = %d\n", bits);

	switch (cs) {
	case (OCDFMTDEF_CS_RGB >> OCDFMTDEF_CS_SHIFT):
		GCDBG(GCZONE_FORMAT, "OCDFMTDEF_CS_RGB\n");

		/* Determine the swizzle. */
		swizzle = (ocdformat & OCDFMTDEF_PLACEMENT_MASK)
			>> OCDFMTDEF_PLACEMENT_SHIFT;
		GCDBG(GCZONE_FORMAT, "swizzle = %d\n", swizzle);

		/* RGB color space. */
		format->type = BVFMT_RGB;

		/* Has to be 0 for RGB. */
		if (std != 0) {
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported standard");
			goto exit;
		}

		/* Determine premultuplied or not. */
		if (alpha == OCDFMTDEF_ALPHA) {
			format->premultiplied
				= ((ocdformat & OCDFMTDEF_NON_PREMULT) == 0);
		} else {
			format->premultiplied = true;

			if ((ocdformat & OCDFMTDEF_FILL_EMPTY_0) != 0) {
				BVSETBLTERROR(BVERR_UNK,
					      "0 filling is not supported");
				goto exit;
			}
		}
		GCDBG(GCZONE_FORMAT, "premultiplied = %d\n",
		      format->premultiplied);

		/* No subsample support. */
		if (subsample !=
		    (OCDFMTDEF_SUBSAMPLE_NONE >> OCDFMTDEF_SUBSAMPLE_SHIFT)) {
			BVSETBLTERROR(BVERR_UNK,
					"subsampling for RGB is not supported");
			goto exit;
		}

		/* Only packed RGB is supported. */
		if (layout !=
		    (OCDFMTDEF_PACKED >> OCDFMTDEF_LAYOUT_SHIFT)) {
			BVSETBLTERROR(BVERR_UNK,
				      "only packed RGBA formats are supported");
			goto exit;
		}

		/* Determine the format. */
		switch (bits) {
		case 12:
			format->bitspp = 16;
			format->allocbitspp = 16;
			format->swizzle = rgba16swizzle[swizzle];

			if (alpha == OCDFMTDEF_ALPHA) {
				format->format = GCREG_DE_FORMAT_A4R4G4B4;
				format->cs.rgb.comp = &argb4444_bits[swizzle];
			} else {
				format->format = GCREG_DE_FORMAT_X4R4G4B4;
				format->cs.rgb.comp = &xrgb4444_bits[swizzle];
			}
			break;

		case 15:
			format->bitspp = 16;
			format->allocbitspp = 16;
			format->swizzle = rgba16swizzle[swizzle];

			if (alpha == OCDFMTDEF_ALPHA) {
				format->format = GCREG_DE_FORMAT_A1R5G5B5;
				format->cs.rgb.comp = &argb1555_bits[swizzle];
			} else {
				format->format = GCREG_DE_FORMAT_X1R5G5B5;
				format->cs.rgb.comp = &xrgb1555_bits[swizzle];
			}
			break;

		case 16:
			if (alpha == OCDFMTDEF_ALPHA) {
				BVSETBLTERROR(BVERR_UNK,
					      "alpha component is not supported"
					      "for this format.");
				goto exit;
			}

			format->bitspp = 16;
			format->allocbitspp = 16;
			format->swizzle = rgb16swizzle[swizzle];
			format->format = GCREG_DE_FORMAT_R5G6B5;
			format->cs.rgb.comp = &rgb565_bits[swizzle];
			break;

		case 24:
			format->bitspp = 32;
			format->allocbitspp = 32;
			format->swizzle = rgba32swizzle[swizzle];

			if (alpha == OCDFMTDEF_ALPHA) {
				format->format = GCREG_DE_FORMAT_A8R8G8B8;
				format->cs.rgb.comp = &argb8888_bits[swizzle];
			} else {
				format->format = GCREG_DE_FORMAT_X8R8G8B8;
				format->cs.rgb.comp = &xrgb8888_bits[swizzle];
			}
			break;

		default:
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported bit width %d", bits);
			goto exit;
		}

		if (format->allocbitspp != container[cont]) {
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported container");
			goto exit;
		}
		break;

	case (OCDFMTDEF_CS_YCbCr >> OCDFMTDEF_CS_SHIFT):
		GCDBG(GCZONE_FORMAT, "OCDFMTDEF_CS_YCbCr\n");

		/* YUV color space. */
		format->type = BVFMT_YUV;

		/* Determine the swizzle. */
		reversed = ocdformat & OCDFMTDEF_REVERSED;
		leftjust = ocdformat & OCDFMTDEF_LEFT_JUSTIFIED;
		GCDBG(GCZONE_FORMAT_VERBOSE, "reversed = %d\n",
		      reversed ? 1 : 0);
		GCDBG(GCZONE_FORMAT_VERBOSE, "leftjust = %d\n",
		      leftjust ? 1 : 0);

		/* Parse the standard. */
		switch (std) {
		case OCDFMTDEF_STD_ITUR_601_YCbCr >> OCDFMTDEF_STD_SHIFT:
			GCDBG(GCZONE_FORMAT, "OCDFMTDEF_STD_ITUR_601_YCbCr\n");
			format->cs.yuv.std = GCREG_PE_CONTROL_YUV_601;
			break;

		case OCDFMTDEF_STD_ITUR_709_YCbCr >> OCDFMTDEF_STD_SHIFT:
			GCDBG(GCZONE_FORMAT, "OCDFMTDEF_STD_ITUR_709_YCbCr\n");
			format->cs.yuv.std = GCREG_PE_CONTROL_YUV_709;
			break;

		default:
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported color standard");
			goto exit;
		}

		/* Alpha is not supported. */
		if (alpha == OCDFMTDEF_ALPHA) {
			BVSETBLTERROR(BVERR_UNK,
				      "alpha channel is not supported");
			goto exit;
		}

		format->premultiplied = true;

		/* Parse subsampling. */
		switch (subsample) {
		case OCDFMTDEF_SUBSAMPLE_422_YCbCr >> OCDFMTDEF_SUBSAMPLE_SHIFT:
			GCDBG(GCZONE_FORMAT, "OCDFMTDEF_SUBSAMPLE_422_YCbCr\n");

			/* Parse layout. */
			switch (layout) {
			case OCDFMTDEF_PACKED >> OCDFMTDEF_LAYOUT_SHIFT:
				GCDBG(GCZONE_FORMAT, "OCDFMTDEF_PACKED\n");

				if (container[cont] != 32) {
					BVSETBLTERROR(BVERR_UNK,
						      "unsupported container");
					goto exit;
				}

				format->bitspp = 16;
				format->allocbitspp = 16;
				format->format = leftjust
					? GCREG_DE_FORMAT_YUY2
					: GCREG_DE_FORMAT_UYVY;
				format->swizzle = reversed
					? GCREG_PE_CONTROL_UV_SWIZZLE_VU
					: GCREG_PE_CONTROL_UV_SWIZZLE_UV;
				format->cs.yuv.planecount = 1;
				format->cs.yuv.xsample = 2;
				format->cs.yuv.ysample = 1;
				break;

			default:
				BVSETBLTERROR(BVERR_UNK,
					      "specified 4:2:2 layout "
					      "is not supported");
				goto exit;
			}
			break;

		case OCDFMTDEF_SUBSAMPLE_420_YCbCr >> OCDFMTDEF_SUBSAMPLE_SHIFT:

			/* Parse layout. */
			switch (layout) {
			case OCDFMTDEF_2_PLANE_YCbCr
						>> OCDFMTDEF_LAYOUT_SHIFT:
				GCDBG(GCZONE_FORMAT,
				      "OCDFMTDEF_2_PLANE_YCbCr\n");

				if (container[cont] != 48) {
					BVSETBLTERROR(BVERR_UNK,
						      "unsupported container");
					goto exit;
				}

				format->bitspp = 8;
				format->allocbitspp = 12;
				format->format = GCREG_DE_FORMAT_NV12;
				format->swizzle = reversed
					? GCREG_PE_CONTROL_UV_SWIZZLE_VU
					: GCREG_PE_CONTROL_UV_SWIZZLE_UV;
				format->cs.yuv.planecount = 2;
				format->cs.yuv.xsample = 2;
				format->cs.yuv.ysample = 2;
				break;

			case OCDFMTDEF_3_PLANE_STACKED
						>> OCDFMTDEF_LAYOUT_SHIFT:
				GCDBG(GCZONE_FORMAT,
				      "OCDFMTDEF_3_PLANE_STACKED\n");

				if (container[cont] != 48) {
					BVSETBLTERROR(BVERR_UNK,
						      "unsupported container");
					goto exit;
				}

				format->bitspp = 8;
				format->allocbitspp = 12;
				format->format = GCREG_DE_FORMAT_YV12;
				format->swizzle = reversed
					? GCREG_PE_CONTROL_UV_SWIZZLE_VU
					: GCREG_PE_CONTROL_UV_SWIZZLE_UV;
				format->cs.yuv.planecount = 3;
				format->cs.yuv.xsample = 2;
				format->cs.yuv.ysample = 2;
				break;

			default:
				BVSETBLTERROR(BVERR_UNK,
					      "specified 4:2:2 layout "
					      "is not supported");
				goto exit;
			}
			break;

		default:
			BVSETBLTERROR(BVERR_UNK,
				      "specified subsampling is not supported");
			goto exit;
		}

		if (format->allocbitspp != bits) {
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported bit width %d", bits);
			goto exit;
		}
		break;

	default:
		BVSETBLTERROR(BVERR_UNK,
			      "unsupported color space %d", cs);
		goto exit;
	}

	GCDBG(GCZONE_FORMAT, "bpp = %d\n", format->bitspp);
	GCDBG(GCZONE_FORMAT, "gcformat = %d\n", format->format);
	GCDBG(GCZONE_FORMAT, "gcswizzle = %d\n", format->swizzle);

	bverror = BVERR_NONE;

exit:
	GCEXITARG(GCZONE_FORMAT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}


/*******************************************************************************
 * Alpha blending parser.
 */

#define BVBLENDMATCH(Mode, Inverse, Normal) \
( \
	BVBLENDDEF_ ## Mode | \
	BVBLENDDEF_ ## Inverse | \
	BVBLENDDEF_ ## Normal \
)

#define BVSRC1USE(Use) \
	Use

#define BVSRC2USE(Use) \
	Use

#define BVBLENDUNDEFINED() \
	{ ~0, ~0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }

struct bvblendxlate {
	unsigned char match1;
	unsigned char match2;

	struct gcblendconfig k1;
	struct gcblendconfig k2;
};

static struct bvblendxlate blendxlate[64] = {
	/**********************************************************************/
	/* #0: color factor: 00 00 00 A:(1-C1,C1)=zero
	       alpha factor: zero ==> 00 00 00 */
	{
		0x00,
		0x00,

		{
			GCREG_BLENDING_MODE_ZERO,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(false), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_ZERO,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(false), BVSRC2USE(false)
		}
	},

	/* #1: color factor: 00 00 01 A:(1-C1,A1)=A1
	       alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #2: color factor: 00 00 10 A:(1-C1,C2)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #3: color factor: 00 00 11 A:(1-C1,A2)=A2
	       alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #4: color factor: 00 01 00 A:(1-A1,C1)=1-A1
	       alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #5: color factor: 00 01 01 A:(1-A1,A1)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #6: color factor: 00 01 10 A:(1-A1,C2)=1-A1
	       alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #7: color factor: 00 01 11 A:(1-A1,A2)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #8: color factor: 00 10 00 A:(1-C2,C1)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #9: color factor: 00 10 01 A:(1-C2,A1)=A1
	       alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #10: color factor: 00 10 10 A:(1-C2,C2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #11: color factor: 00 10 11 A:(1-C2,A2)=A2
		alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #12: color factor: 00 11 00 A:(1-A2,C1)=1-A2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #13: color factor: 00 11 01 A:(1-A2,A1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #14: color factor: 00 11 10 A:(1-A2,C2)=1-A2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #15: color factor: 00 11 11 A:(1-A2,A2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* #16: color factor: 01 00 00 MIN:(1-C1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #17: color factor: 01 00 01 MIN:(1-C1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #18: color factor: 01 00 10 MIN:(1-C1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #19: color factor: 01 00 11 MIN:(1-C1,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #20: color factor: 01 01 00 MIN:(1-A1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #21: color factor: 01 01 01 MIN:(1-A1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #22: color factor: 01 01 10 MIN:(1-A1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #23: color factor: 01 01 11 MIN:(1-A1,A2)
		alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_SATURATED_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #24: color factor: 01 10 00 MIN:(1-C2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #25: color factor: 01 10 01 MIN:(1-C2,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #26: color factor: 01 10 10 MIN:(1-C2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #27: color factor: 01 10 11 MIN:(1-C2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #28: color factor: 01 11 00 MIN:(1-A2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #29: color factor: 01 11 01 MIN:(1-A2,A1)
		alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			GCREG_BLENDING_MODE_SATURATED_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #30: color factor: 01 11 10 MIN:(1-A2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #31: color factor: 01 11 11 MIN:(1-A2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* #32: color factor: 10 00 00 MAX:(1-C1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #33: color factor: 10 00 01 MAX:(1-C1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #34: color factor: 10 00 10 MAX:(1-C1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #35: color factor: 10 00 11 MAX:(1-C1,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #36: color factor: 10 01 00 MAX:(1-A1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #37: color factor: 10 01 01 MAX:(1-A1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #38: color factor: 10 01 10 MAX:(1-A1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #39: color factor: 10 01 11 MAX:(1-A1,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #40: color factor: 10 10 00 MAX:(1-C2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #41: color factor: 10 10 01 MAX:(1-C2,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #42: color factor: 10 10 10 MAX:(1-C2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #43: color factor: 10 10 11 MAX:(1-C2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #44: color factor: 10 11 00 MAX:(1-A2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #45: color factor: 10 11 01 MAX:(1-A2,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #46: color factor: 10 11 10 MAX:(1-A2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #47: color factor: 10 11 11 MAX:(1-A2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* #48: color factor: 11 00 00 C:(1-C1,C1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #49: color factor: 11 00 01 C:(1-C1,A1)=1-C1
		alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #50: color factor: 11 00 10 C:(1-C1,C2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #51: color factor: 11 00 11 C:(1-C1,A2)=1-C1
		alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #52: color factor: 11 01 00 C:(1-A1,C1)=C1
		alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #53: color factor: 11 01 01 C:(1-A1,A1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #54: color factor: 11 01 10 C:(1-A1,C2)=C2
		alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #55: color factor: 11 01 11 C:(1-A1,A2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #56: color factor: 11 10 00 C:(1-C2,C1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #57: color factor: 11 10 01 C:(1-C2,A1)=1-C2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #58: color factor: 11 10 10 C:(1-C2,C2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #59: color factor: 11 10 11 C:(1-C2,A2)=1-C2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(false)
		}
	},

	/* #60: color factor: 11 11 00 C:(1-A2,C1)=C1
		alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		}
	},

	/* #61: color factor: 11 11 01 C:(1-A2,A1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #62: color factor: 11 11 10 C:(1-A2,C2)=C2
		alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(true)
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	},

	/* #63: color factor: 11 11 11 C:(1-A2,A2)=one
		alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			GCREG_BLENDING_MODE_ONE,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(true), BVSRC2USE(false)
		},

		{
			GCREG_BLENDING_MODE_ONE,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(false), BVSRC2USE(true)
		}
	}
};

enum bverror parse_blend(struct bvbltparams *bvbltparams,
			 enum bvblend blend,
			 struct gcalpha *gca)
{
	enum bverror bverror;
	unsigned int global;
	unsigned int k1, k2, k3, k4;
	struct bvblendxlate *k1_xlate;
	struct bvblendxlate *k2_xlate;
	unsigned int alpha;

	GCENTERARG(GCZONE_BLEND, "blend = 0x%08X (%s)\n",
		   blend, gc_bvblend_name(blend));

	if ((blend & BVBLENDDEF_REMOTE) != 0) {
		BVSETBLTERROR(BVERR_BLEND, "remote alpha not supported");
		goto exit;
	}

	global = (blend & BVBLENDDEF_GLOBAL_MASK) >> BVBLENDDEF_GLOBAL_SHIFT;

	switch (global) {
	case (BVBLENDDEF_GLOBAL_NONE >> BVBLENDDEF_GLOBAL_SHIFT):
		GCDBG(GCZONE_BLEND, "BVBLENDDEF_GLOBAL_NONE\n");

		gca->src_global_color =
		gca->dst_global_color = 0;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		break;

	case (BVBLENDDEF_GLOBAL_UCHAR >> BVBLENDDEF_GLOBAL_SHIFT):
		GCDBG(GCZONE_BLEND, "BVBLENDDEF_GLOBAL_UCHAR\n");

		gca->src_global_color =
		gca->dst_global_color =
			((unsigned int) bvbltparams->globalalpha.size8) << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;

	case (BVBLENDDEF_GLOBAL_FLOAT >> BVBLENDDEF_GLOBAL_SHIFT):
		GCDBG(GCZONE_BLEND, "BVBLENDDEF_GLOBAL_FLOAT\n");

		alpha = gcfp2norm8(bvbltparams->globalalpha.fp);

		gca->src_global_color =
		gca->dst_global_color = alpha << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;

	default:
		BVSETBLTERROR(BVERR_BLEND, "invalid global alpha mode");
		goto exit;
	}

	/*
		Co = k1 x C1 + k2 x C2
		Ao = k3 x A1 + k4 x A2
	*/

	k1 = (blend >> 18) & 0x3F;
	k2 = (blend >> 12) & 0x3F;
	k3 = (blend >>  6) & 0x3F;
	k4 =  blend        & 0x3F;

	GCDBG(GCZONE_BLEND, "k1 = %d\n", k1);
	GCDBG(GCZONE_BLEND, "k2 = %d\n", k2);
	GCDBG(GCZONE_BLEND, "k3 = %d\n", k3);
	GCDBG(GCZONE_BLEND, "k4 = %d\n", k4);

	k1_xlate = &blendxlate[k1];
	k2_xlate = &blendxlate[k2];

	if (((k3 != k1_xlate->match1) && (k3 != k1_xlate->match2)) ||
		((k4 != k2_xlate->match1) && (k4 != k2_xlate->match2))) {
		BVSETBLTERROR(BVERR_BLEND,
			      "not supported coefficient combination");
		goto exit;
	}

	gca->k1 = &k1_xlate->k1;
	gca->k2 = &k2_xlate->k2;

	gca->src1used = gca->k1->src1used | gca->k2->src1used;
	gca->src2used = gca->k1->src2used | gca->k2->src2used;

	bverror = BVERR_NONE;

exit:
	GCEXITARG(BVERR_BLEND, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}


/*******************************************************************************
 * Rotation and mirror.
 */

#define BVFLAG_FLIP_MASK	0x00000003

#define BVFLAG_FLIP_SRC1_SHIFT	14
#define BVFLAG_FLIP_SRC2_SHIFT	16
#define BVFLAG_FLIP_MASK_SHIFT	18

#define GCREG_MIRROR_NONE	0x0
#define GCREG_MIRROR_X		0x1
#define GCREG_MIRROR_Y		0x2
#define GCREG_MIRROR_XY		0x3

#define GCREG_ROT_ANGLE_ROT0	0x0
#define GCREG_ROT_ANGLE_ROT90	0x4
#define GCREG_ROT_ANGLE_ROT180	0x5
#define GCREG_ROT_ANGLE_ROT270	0x6

#define ROT_ANGLE_INVALID	-1
#define ROT_ANGLE_0		0
#define ROT_ANGLE_90		1
#define ROT_ANGLE_180		2
#define ROT_ANGLE_270		3

/* NOTE: BLTsville rotation is defined conunter clock wise. */
const unsigned int rotencoding[] = {
	GCREG_ROT_ANGLE_ROT0,		/* ROT_ANGLE_0 */
	GCREG_ROT_ANGLE_ROT270,		/* ROT_ANGLE_90 */
	GCREG_ROT_ANGLE_ROT180,		/* ROT_ANGLE_180 */
	GCREG_ROT_ANGLE_ROT90		/* ROT_ANGLE_270 */
};

static inline int get_angle(int orientation)
{
	int angle;

	/* Normalize the angle. */
	angle = orientation % 360;

	/* Flip to positive. */
	if (angle < 0)
		angle = 360 + angle;

	/* Translate the angle. */
	switch (angle) {
	case 0:   return ROT_ANGLE_0;
	case 90:  return ROT_ANGLE_90;
	case 180: return ROT_ANGLE_180;
	case 270: return ROT_ANGLE_270;
	}

	/* Not supported angle. */
	return ROT_ANGLE_INVALID;
}


/*******************************************************************************
 * Surface compare and validation.
 */

bool valid_rect(struct bvsurfgeom *bvsurfgeom, struct gcrect *gcrect)
{
	int width, height;

	if ((gcrect->left < 0) || (gcrect->top < 0)) {
		GCERR("invalid rectangle origin: %d,%d.\n",
		      gcrect->left, gcrect->top);
		return false;
	}

	width  = gcrect->right  - gcrect->left;
	height = gcrect->bottom - gcrect->top;
	if ((width <= 0) || (height <= 0)) {
		GCERR("invalid rectangle size: %d,%d.\n",
		      width, height);
		return false;
	}

	if (gcrect->right > (int) bvsurfgeom->width) {
		GCERR("right coordinate (%d) exceeds surface width (%d).\n",
		      gcrect->right, bvsurfgeom->width);
		return false;
	}

	if (gcrect->bottom > (int) bvsurfgeom->height) {
		GCERR("bottom coordinate (%d) exceeds surface height (%d).\n",
		      gcrect->bottom, bvsurfgeom->height);
		return false;
	}

	return true;
}

static bool valid_geom(struct surfaceinfo *surfaceinfo)
{
	unsigned int size;
	unsigned int height;

	/* Compute the size of the surface. */
	size = (surfaceinfo->geom->width *
		surfaceinfo->geom->height *
		surfaceinfo->format.allocbitspp) / 8;

	/* Make sure the size is not greater then the surface. */
	if (size > surfaceinfo->buf.desc->length) {
		GCERR("invalid geometry detected:\n");
		GCERR("  specified dimensions: %dx%d, %d bitspp\n",
		      surfaceinfo->geom->width,
		      surfaceinfo->geom->height,
		      surfaceinfo->format.bitspp);
		GCERR("  surface size based on the dimensions: %d\n",
		      size);
		GCERR("  specified surface size: %lu\n",
		      surfaceinfo->buf.desc->length);
		return false;
	}

	/* Determine the height of the image. */
	height = ((surfaceinfo->angle % 2) == 0)
	       ? surfaceinfo->geom->height
	       : surfaceinfo->geom->width;

	/* Compute the size using the stide. */
	size = surfaceinfo->geom->virtstride * height;

	/* Make sure the size is not greater then the surface. */
	if (size > surfaceinfo->buf.desc->length) {
		GCERR("invalid geometry detected:\n");
		GCERR("  specified dimensions: %dx%d, %d bitspp\n",
		      surfaceinfo->geom->width,
		      surfaceinfo->geom->height,
		      surfaceinfo->format.bitspp);
		GCERR("  physical image height = %d\n", height);
		GCERR("  image stride = %d\n", surfaceinfo->geom->virtstride);
		GCERR("  computed surface size = %d\n", size);
		GCERR("  specified surface size: %lu\n",
		      surfaceinfo->buf.desc->length);
		return false;
	}

	return true;
}

int get_pixel_offset(struct surfaceinfo *surfaceinfo, int offset)
{
	unsigned int alignment;
	int byteoffset;
	unsigned int alignedoffset;
	int pixeloffset;

	GCENTERARG(GCZONE_OFFSET, "surfaceinfo=0x%08X, offset=%d\n",
		   surfaceinfo, offset);

	alignment = (surfaceinfo->format.type == BVFMT_YUV)
		? (64 - 1)
		: (16 - 1);

	GCDBG(GCZONE_OFFSET, "bpp = %d\n", surfaceinfo->format.bitspp);
	GCDBG(GCZONE_OFFSET, "alignment = %d\n", alignment);

	/* Determine offset in bytes from the base modified by the
	 * given offset. */
	if (surfaceinfo->buf.desc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *bvphysdesc;
		bvphysdesc = (struct bvphysdesc *)
			     surfaceinfo->buf.desc->auxptr;
		GCDBG(GCZONE_OFFSET, "physical descriptor = 0x%08X\n",
		      bvphysdesc);
		GCDBG(GCZONE_OFFSET, "first page = 0x%08X\n",
			bvphysdesc->pagearray[0]);
		GCDBG(GCZONE_OFFSET, "page offset = 0x%08X\n",
			bvphysdesc->pageoffset);

		byteoffset = bvphysdesc->pageoffset + offset;
	} else {
		GCDBG(GCZONE_OFFSET, "no physical descriptor.\n");
		byteoffset = (unsigned int)
			     surfaceinfo->buf.desc->virtaddr + offset;
	}

	GCDBG(GCZONE_OFFSET, "byteoffset = %d\n", byteoffset);

	/* Compute the aligned offset. */
	alignedoffset = byteoffset & alignment;

	/* Convert to pixels. */
	pixeloffset = alignedoffset * 8 / surfaceinfo->format.bitspp;

	GCDBG(GCZONE_OFFSET, "alignedoffset = %d\n", alignedoffset);
	GCDBG(GCZONE_OFFSET, "pixeloffset = %d\n", -pixeloffset);

	GCEXIT(GCZONE_OFFSET);
	return -pixeloffset;
}

enum bverror parse_destination(struct bvbltparams *bvbltparams,
			       struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;

	GCENTER(GCZONE_DEST);

	GCDBG(GCZONE_DEST, "parsing destination\n");

	/* Did the destination surface change? */
	if ((batch->batchflags & BVBATCH_DST) != 0) {
		struct surfaceinfo *dstinfo;

		/* Initialize the destination descriptor. */
		dstinfo = &batch->dstinfo;
		dstinfo->index = -1;
		dstinfo->buf.desc = bvbltparams->dstdesc;
		dstinfo->geom = bvbltparams->dstgeom;

		/* Initialize members that are not used. */
		dstinfo->mirror = GCREG_MIRROR_NONE;
		dstinfo->rop = 0;
		dstinfo->gca = NULL;

		/* Parse the destination format. */
		if (parse_format(bvbltparams, dstinfo) != BVERR_NONE) {
			bverror = BVERR_DSTGEOM_FORMAT;
			goto exit;
		}

		/* Parse orientation. */
		dstinfo->angle = get_angle(dstinfo->geom->orientation);
		if (dstinfo->angle == ROT_ANGLE_INVALID) {
			BVSETBLTERROR(BVERR_DSTGEOM,
				      "unsupported destination orientation %d.",
				      dstinfo->geom->orientation);
			goto exit;
		}

		/* Compute the destination alignments needed to compensate
		 * for the surface base address misalignment if any. */
		dstinfo->xpixalign = get_pixel_offset(dstinfo, 0);
		dstinfo->ypixalign = 0;
		dstinfo->bytealign = (dstinfo->xpixalign
				   * (int) dstinfo->format.bitspp) / 8;

		GCDBG(GCZONE_DEST, "  buffer length = %d\n",
		      dstinfo->buf.desc->length);
		GCDBG(GCZONE_DEST, "  rotation %d degrees.\n",
		      dstinfo->angle * 90);

		if (dstinfo->buf.desc->auxtype == BVAT_PHYSDESC) {
			struct bvphysdesc *bvphysdesc;
			bvphysdesc = (struct bvphysdesc *)
				     dstinfo->buf.desc->auxptr;
			GCDBG(GCZONE_DEST, "  physical descriptor = 0x%08X\n",
			      bvphysdesc);
			GCDBG(GCZONE_DEST, "  first page = 0x%08X\n",
			      bvphysdesc->pagearray[0]);
			GCDBG(GCZONE_DEST, "  page offset = 0x%08X\n",
			      bvphysdesc->pageoffset);
		} else {
			GCDBG(GCZONE_DEST, "  virtual address = 0x%08X\n",
			      (unsigned int) dstinfo->buf.desc->virtaddr);
		}

		GCDBG(GCZONE_DEST, "  stride = %ld\n",
		      dstinfo->geom->virtstride);
		GCDBG(GCZONE_DEST, "  geometry size = %dx%d\n",
		      dstinfo->geom->width, dstinfo->geom->height);
		GCDBG(GCZONE_DEST, "  surface offset (pixels) = %d,%d\n",
		      dstinfo->xpixalign, dstinfo->ypixalign);
		GCDBG(GCZONE_DEST, "  surface offset (bytes) = %d\n",
		      dstinfo->bytealign);

		/* Check for unsupported dest formats. */
		if ((dstinfo->format.type == BVFMT_YUV) &&
		    (dstinfo->format.cs.yuv.planecount > 1)) {
			BVSETBLTERROR(BVERR_DSTGEOM_FORMAT,
				      "destination format unsupported");
			goto exit;
		}

		/* Destination stride must be 8 pixel aligned. */
		if ((dstinfo->geom->virtstride
				& (dstinfo->format.bitspp - 1)) != 0) {
			BVSETBLTERROR(BVERR_DSTGEOM_STRIDE,
				      "destination stride must be 8 pixel "
				      "aligned.");
			goto exit;
		}

		/* Validate geometry. */
		if (!valid_geom(dstinfo)) {
			BVSETBLTERROR(BVERR_DSTGEOM,
				      "destination geom exceeds surface size");
			goto exit;
		}
	}

	/* Did clipping/destination rects change? */
	if ((batch->batchflags & (BVBATCH_CLIPRECT |
				  BVBATCH_DESTRECT |
				  BVBATCH_DST)) != 0) {
		struct surfaceinfo *dstinfo;
		struct gcrect cliprect;
		struct gcrect *dstrect;
		struct gcrect *dstrectaux;

		/* Get a shortcut to the destination surface. */
		dstinfo = &batch->dstinfo;

		/* Determine destination rectangle. */
		dstrect = &dstinfo->rect;
		GCCONVERT_RECT(GCZONE_DEST,
			       "  rect",
			       &bvbltparams->dstrect,
			       dstrect);

		/* Determine whether aux destination is specified. */
		batch->haveaux
			= ((bvbltparams->flags & BVFLAG_SRC2_AUXDSTRECT) != 0);
		GCDBG(GCZONE_DEST, "  have aux dest = %d\n", batch->haveaux);

		/* Is clipping rectangle specified? */
		if ((bvbltparams->flags & BVFLAG_CLIP) == BVFLAG_CLIP) {
			/* Convert the clipping rectangle. */
			GCCONVERT_RECT(GCZONE_DEST,
				       "  clipping",
				       &bvbltparams->cliprect,
				       &cliprect);

			if ((cliprect.left   < GC_CLIP_RESET_LEFT)  ||
			    (cliprect.top    < GC_CLIP_RESET_TOP)   ||
			    (cliprect.right  > GC_CLIP_RESET_RIGHT) ||
			    (cliprect.bottom > GC_CLIP_RESET_BOTTOM)) {
				BVSETERROR(BVERR_CLIP_RECT,
					   "clip rect is invalid");
				goto exit;
			}

			/* Compute clipping deltas and the adjusted
			 * destination rect. */
			if (cliprect.left <= dstrect->left) {
				batch->clipdelta.left = 0;
				batch->dstclipped.left = dstrect->left;
			} else {
				batch->clipdelta.left = cliprect.left
						      - dstrect->left;
				batch->dstclipped.left = cliprect.left;
			}

			if (cliprect.top <= dstrect->top) {
				batch->clipdelta.top = 0;
				batch->dstclipped.top = dstrect->top;
			} else {
				batch->clipdelta.top = cliprect.top
						     - dstrect->top;
				batch->dstclipped.top = cliprect.top;
			}

			if (cliprect.right >= dstrect->right) {
				batch->clipdelta.right = 0;
				batch->dstclipped.right = dstrect->right;
			} else {
				batch->clipdelta.right = cliprect.right
						       - dstrect->right;
				batch->dstclipped.right = cliprect.right;
			}

			if (cliprect.bottom >= dstrect->bottom) {
				batch->clipdelta.bottom = 0;
				batch->dstclipped.bottom = dstrect->bottom;
			} else {
				batch->clipdelta.bottom = cliprect.bottom
							- dstrect->bottom;
				batch->dstclipped.bottom = cliprect.bottom;
			}

			/* Clip the aux destination. */
			if (batch->haveaux) {
				/* Convert the aux rectangle. */
				dstrectaux = &batch->dstrectaux;
				GCCONVERT_RECT(GCZONE_DEST,
					       "  aux rect",
					       &bvbltparams->src2auxdstrect,
					       dstrectaux);

				if (cliprect.left <= dstrectaux->left)
					batch->dstclippedaux.left
						= dstrectaux->left;
				else
					batch->dstclippedaux.left
						= cliprect.left;

				if (cliprect.top <= dstrectaux->top)
					batch->dstclippedaux.top
						= dstrectaux->top;
				else
					batch->dstclippedaux.top
						= cliprect.top;

				if (cliprect.right >= dstrectaux->right)
					batch->dstclippedaux.right
						= dstrectaux->right;
				else
					batch->dstclippedaux.right
						= cliprect.right;

				if (cliprect.bottom >= dstrectaux->bottom)
					batch->dstclippedaux.bottom
						= dstrectaux->bottom;
				else
					batch->dstclippedaux.bottom
						= cliprect.bottom;
			}
		} else {
			batch->clipdelta.left =
			batch->clipdelta.top =
			batch->clipdelta.right =
			batch->clipdelta.bottom = 0;

			batch->dstclipped = *dstrect;
			if (batch->haveaux)
				/* Convert the aux rectangle. */
				GCCONVERT_RECT(GCZONE_DEST,
					       "   aux rect",
					       &bvbltparams->src2auxdstrect,
					       &batch->dstclippedaux);
		}

		GCPRINT_RECT(GCZONE_DEST, "  clipped dest",
			     &batch->dstclipped);

		/* Validate the destination rectangle. */
		if (!valid_rect(dstinfo->geom, &batch->dstclipped)) {
			BVSETBLTERROR(BVERR_DSTRECT,
				      "invalid destination rectangle.");
			goto exit;
		}

		if (batch->haveaux) {
			GCPRINT_RECT(GCZONE_DEST, "  clipped aux dest",
				     &batch->dstclippedaux);

			/* Validate the aux destination rectangle. */
			if (!valid_rect(dstinfo->geom, &batch->dstclippedaux)) {
				BVSETBLTERROR(BVERR_DSTRECT,
					      "invalid aux destination "
					      "rectangle.");
				goto exit;
			}
		}

		GCDBG(GCZONE_DEST,
		      "  clipping delta = (%d,%d)-(%d,%d)\n",
		      batch->clipdelta.left,
		      batch->clipdelta.top,
		      batch->clipdelta.right,
		      batch->clipdelta.bottom);
	}

exit:
	GCEXITARG(GCZONE_DEST, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

void process_dest_rotation(struct bvbltparams *bvbltparams,
			   struct gcbatch *batch)
{
	GCENTER(GCZONE_DEST);

	/* Did clipping/destination rects change? */
	if ((batch->batchflags & (BVBATCH_CLIPRECT |
				  BVBATCH_DESTRECT |
				  BVBATCH_DST)) != 0) {
		struct surfaceinfo *dstinfo;
		int dstoffsetX, dstoffsetY;

		/* Initialize the destination descriptor. */
		dstinfo = &batch->dstinfo;

		switch (dstinfo->angle) {
		case ROT_ANGLE_0:
			/* Determine the origin offset. */
			dstoffsetX = dstinfo->xpixalign;
			dstoffsetY = dstinfo->ypixalign;

			/* Determine geometry size. */
			batch->dstwidth  = dstinfo->geom->width
					 - dstinfo->xpixalign;
			batch->dstheight = dstinfo->geom->height
					 - dstinfo->ypixalign;

			/* Determine the physical size. */
			dstinfo->physwidth  = batch->dstwidth;
			dstinfo->physheight = batch->dstheight;
			break;

		case ROT_ANGLE_90:
			/* Determine the origin offset. */
			dstoffsetX = dstinfo->ypixalign;
			dstoffsetY = dstinfo->xpixalign;

			/* Determine geometry size. */
			batch->dstwidth  = dstinfo->geom->width
					 - dstinfo->ypixalign;
			batch->dstheight = dstinfo->geom->height
					 - dstinfo->xpixalign;

			/* Determine the physical size. */
			dstinfo->physwidth  = dstinfo->geom->height
					    - dstinfo->xpixalign;
			dstinfo->physheight = dstinfo->geom->width
					    - dstinfo->ypixalign;
			break;

		case ROT_ANGLE_180:
			/* Determine the origin offset. */
			dstoffsetX = 0;
			dstoffsetY = 0;

			/* Determine geometry size. */
			batch->dstwidth  = dstinfo->geom->width
					 - dstinfo->xpixalign;
			batch->dstheight = dstinfo->geom->height
					 - dstinfo->ypixalign;

			/* Determine the physical size. */
			dstinfo->physwidth  = batch->dstwidth;
			dstinfo->physheight = batch->dstheight;
			break;

		case ROT_ANGLE_270:
			/* Determine the origin offset. */
			dstoffsetX = 0;
			dstoffsetY = 0;

			/* Determine geometry size. */
			batch->dstwidth  = dstinfo->geom->width
					 - dstinfo->ypixalign;
			batch->dstheight = dstinfo->geom->height
					 - dstinfo->xpixalign;

			/* Determine the physical size. */
			dstinfo->physwidth  = dstinfo->geom->height
					    - dstinfo->xpixalign;
			dstinfo->physheight = dstinfo->geom->width
					    - dstinfo->ypixalign;
			break;

		default:
			dstoffsetX = 0;
			dstoffsetY = 0;
		}

		/* Compute adjusted destination rectangles. */
		batch->dstadjusted.left
			= batch->dstclipped.left
			- dstoffsetX;
		batch->dstadjusted.top
			= batch->dstclipped.top
			- dstoffsetY;
		batch->dstadjusted.right
			= batch->dstclipped.right
			- dstoffsetX;
		batch->dstadjusted.bottom
			= batch->dstclipped.bottom
			- dstoffsetY;

		GCPRINT_RECT(GCZONE_DEST, "adjusted dest",
			     &batch->dstadjusted);

		GCDBG(GCZONE_DEST, "aligned geometry size = %dx%d\n",
		      batch->dstwidth, batch->dstheight);
		GCDBG(GCZONE_DEST, "aligned physical size = %dx%d\n",
		      dstinfo->physwidth, dstinfo->physheight);
		GCDBG(GCZONE_DEST, "origin offset (pixels) = %d,%d\n",
		      dstoffsetX, dstoffsetY);
	}

	GCEXIT(GCZONE_DEST);
}

enum bverror parse_source(struct bvbltparams *bvbltparams,
			  struct gcbatch *batch,
			  struct bvrect *srcrect,
			  struct surfaceinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;

	GCENTER(GCZONE_SRC);
	GCDBG(GCZONE_SRC, "parsing source #%d\n",
	      srcinfo->index + 1);

	/* Parse the source format. */
	if (parse_format(bvbltparams, srcinfo) != BVERR_NONE) {
		bverror = (srcinfo->index == 0)
			? BVERR_SRC1GEOM_FORMAT
			: BVERR_SRC2GEOM_FORMAT;
		goto exit;
	}

	/* Parse orientation. */
	srcinfo->angle = get_angle(srcinfo->geom->orientation);
	if (srcinfo->angle == ROT_ANGLE_INVALID) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
			      "unsupported source%d orientation %d.",
			      srcinfo->index + 1,
			      srcinfo->geom->orientation);
		goto exit;
	}

	/* Determine source mirror. */
	srcinfo->mirror = (srcinfo->index == 0)
			? (bvbltparams->flags >> BVFLAG_FLIP_SRC1_SHIFT)
			   & BVFLAG_FLIP_MASK
			: (bvbltparams->flags >> BVFLAG_FLIP_SRC2_SHIFT)
			   & BVFLAG_FLIP_MASK;

	GCDBG(GCZONE_SRC, "  buffer length = %d\n", srcinfo->buf.desc->length);
	GCDBG(GCZONE_SRC, "  rotation %d degrees.\n", srcinfo->angle * 90);

	if (srcinfo->buf.desc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *bvphysdesc;
		bvphysdesc = (struct bvphysdesc *) srcinfo->buf.desc->auxptr;
		GCDBG(GCZONE_SRC, "  physical descriptor = 0x%08X\n",
		      bvphysdesc);
		GCDBG(GCZONE_SRC, "  first page = 0x%08X\n",
		      bvphysdesc->pagearray[0]);
		GCDBG(GCZONE_SRC, "  page offset = 0x%08X\n",
		      bvphysdesc->pageoffset);
	} else {
		GCDBG(GCZONE_SRC, "  virtual address = 0x%08X\n",
			(unsigned int) srcinfo->buf.desc->virtaddr);
	}

	GCDBG(GCZONE_SRC, "  stride = %ld\n",
	      srcinfo->geom->virtstride);
	GCDBG(GCZONE_SRC, "  geometry size = %dx%d\n",
	      srcinfo->geom->width, srcinfo->geom->height);
	GCDBG(GCZONE_SRC, "  mirror = %d\n", srcinfo->mirror);

	/* Convert the rectangle. */
	GCCONVERT_RECT(GCZONE_SRC,
		       "  rect", srcrect, &srcinfo->rect);

	/* Source must be 8 pixel aligned. */
	if ((srcinfo->geom->virtstride
			& (srcinfo->format.bitspp - 1)) != 0) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM_STRIDE
					: BVERR_SRC2GEOM_STRIDE,
			      "source stride must be 8 pixel aligned.");
		goto exit;
	}

	/* Planar YUV? */
	if ((srcinfo->format.type == BVFMT_YUV) &&
	    (srcinfo->format.cs.yuv.planecount > 1)) {
		int xpixalign;

		/* Source rotation is not supported. */
		if (srcinfo->angle != ROT_ANGLE_0) {
			BVSETBLTERROR((srcinfo->index == 0)
						? BVERR_SRC1_ROT
						: BVERR_SRC2_ROT,
				      "rotation of planar YUV is "
				      "not supported");
			goto exit;
		}

		/* Check base address alignment. */
		xpixalign = get_pixel_offset(srcinfo, 0);
		if (xpixalign != 0) {
			BVSETBLTERROR((srcinfo->index == 0)
						? BVERR_SRC1DESC_ALIGNMENT
						: BVERR_SRC2DESC_ALIGNMENT,
					"planar YUV base address must be "
					"64 byte aligned.");
			goto exit;
		}
	}

	/* Validate source geometry. */
	if (!valid_geom(srcinfo)) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
			      "source%d geom exceeds surface size.",
			      srcinfo->index + 1);
		goto exit;
	}

exit:
	GCEXITARG(GCZONE_SRC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror parse_implicitscale(struct bvbltparams *bvbltparams,
					struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;
	unsigned int quality;
	unsigned int technique;
	unsigned int imagetype;

	GCENTER(GCZONE_SCALING);

	quality = (bvbltparams->scalemode & BVSCALEDEF_QUALITY_MASK)
		>> BVSCALEDEF_QUALITY_SHIFT;
	technique = (bvbltparams->scalemode & BVSCALEDEF_TECHNIQUE_MASK)
		  >> BVSCALEDEF_TECHNIQUE_SHIFT;
	imagetype = (bvbltparams->scalemode & BVSCALEDEF_TYPE_MASK)
		  >> BVSCALEDEF_TYPE_SHIFT;

	GCDBG(GCZONE_SCALING, "quality = %d\n", quality);
	GCDBG(GCZONE_SCALING, "technique = %d\n", technique);
	GCDBG(GCZONE_SCALING, "imagetype = %d\n", imagetype);

	switch (quality) {
	case BVSCALEDEF_FASTEST >> BVSCALEDEF_QUALITY_SHIFT:
		batch->op.filter.horkernelsize = 3;
		batch->op.filter.verkernelsize = 3;
		break;

	case BVSCALEDEF_GOOD >> BVSCALEDEF_QUALITY_SHIFT:
		batch->op.filter.horkernelsize = 5;
		batch->op.filter.verkernelsize = 5;
		break;

	case BVSCALEDEF_BETTER >> BVSCALEDEF_QUALITY_SHIFT:
		batch->op.filter.horkernelsize = 7;
		batch->op.filter.verkernelsize = 7;
		break;

	case BVSCALEDEF_BEST >> BVSCALEDEF_QUALITY_SHIFT:
		batch->op.filter.horkernelsize = 9;
		batch->op.filter.verkernelsize = 9;
		break;

	default:
		BVSETBLTERROR(BVERR_SCALE_MODE,
			      "unsupported scale quality 0x%02X", quality);
		goto exit;
	}

	switch (technique) {
	case BVSCALEDEF_DONT_CARE >> BVSCALEDEF_TECHNIQUE_SHIFT:
	case BVSCALEDEF_NOT_NEAREST_NEIGHBOR >> BVSCALEDEF_TECHNIQUE_SHIFT:
		break;

	case BVSCALEDEF_POINT_SAMPLE >> BVSCALEDEF_TECHNIQUE_SHIFT:
		batch->op.filter.horkernelsize = 1;
		batch->op.filter.verkernelsize = 1;
		break;

	case BVSCALEDEF_INTERPOLATED >> BVSCALEDEF_TECHNIQUE_SHIFT:
		break;

	default:
		BVSETBLTERROR(BVERR_SCALE_MODE,
			      "unsupported scale technique %d", technique);
		goto exit;
	}

	switch (imagetype) {
	case 0:
	case BVSCALEDEF_PHOTO >> BVSCALEDEF_TYPE_SHIFT:
	case BVSCALEDEF_DRAWING >> BVSCALEDEF_TYPE_SHIFT:
		break;

	default:
		BVSETBLTERROR(BVERR_SCALE_MODE,
			      "unsupported image type %d", imagetype);
		goto exit;
	}

	GCDBG(GCZONE_SCALING, "kernel size = %dx%d\n",
	      batch->op.filter.horkernelsize,
	      batch->op.filter.verkernelsize);

exit:
	GCEXIT(GCZONE_SCALING);
	return bverror;
}

static enum bverror parse_explicitscale(struct bvbltparams *bvbltparams,
					struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;
	unsigned int horsize;
	unsigned int versize;

	GCENTER(GCZONE_SCALING);

	horsize = (bvbltparams->scalemode & BVSCALEDEF_HORZ_MASK)
		>> BVSCALEDEF_HORZ_SHIFT;
	versize = (bvbltparams->scalemode & BVSCALEDEF_VERT_MASK)
		  >> BVSCALEDEF_VERT_SHIFT;

	GCDBG(GCZONE_SCALING, "horsize = %d\n", horsize);
	GCDBG(GCZONE_SCALING, "versize = %d\n", versize);

	switch (horsize) {
	case BVSCALEDEF_NEAREST_NEIGHBOR:
		batch->op.filter.horkernelsize = 1;
		break;

	case BVSCALEDEF_LINEAR:
	case BVSCALEDEF_CUBIC:
	case BVSCALEDEF_3_TAP:
		batch->op.filter.horkernelsize = 3;
		break;

	case BVSCALEDEF_5_TAP:
		batch->op.filter.horkernelsize = 5;
		break;

	case BVSCALEDEF_7_TAP:
		batch->op.filter.horkernelsize = 7;
		break;

	case BVSCALEDEF_9_TAP:
		batch->op.filter.horkernelsize = 9;
		break;

	default:
		BVSETBLTERROR(BVERR_SCALE_MODE,
			      "unsupported horizontal kernel size %d", horsize);
		goto exit;
	}

	switch (versize) {
	case BVSCALEDEF_NEAREST_NEIGHBOR:
		batch->op.filter.verkernelsize = 1;
		break;

	case BVSCALEDEF_LINEAR:
	case BVSCALEDEF_CUBIC:
	case BVSCALEDEF_3_TAP:
		batch->op.filter.verkernelsize = 3;
		break;

	case BVSCALEDEF_5_TAP:
		batch->op.filter.verkernelsize = 5;
		break;

	case BVSCALEDEF_7_TAP:
		batch->op.filter.verkernelsize = 7;
		break;

	case BVSCALEDEF_9_TAP:
		batch->op.filter.verkernelsize = 9;
		break;

	default:
		BVSETBLTERROR(BVERR_SCALE_MODE,
			      "unsupported vertical kernel size %d", versize);
		goto exit;
	}

	GCDBG(GCZONE_SCALING, "kernel size = %dx%d\n",
	      batch->op.filter.horkernelsize,
	      batch->op.filter.verkernelsize);

exit:
	GCEXIT(GCZONE_SCALING);
	return bverror;
}

enum bverror parse_scalemode(struct bvbltparams *bvbltparams,
			     struct gcbatch *batch)
{
	enum bverror bverror;
	unsigned int scaleclass;

	GCENTER(GCZONE_SCALING);

	scaleclass = (bvbltparams->scalemode & BVSCALEDEF_CLASS_MASK)
		   >> BVSCALEDEF_CLASS_SHIFT;

	GCDBG(GCZONE_SCALING, "scaleclass = %d\n", scaleclass);

	switch (scaleclass) {
	case BVSCALEDEF_IMPLICIT >> BVSCALEDEF_CLASS_SHIFT:
		bverror = parse_implicitscale(bvbltparams, batch);
		break;

	case BVSCALEDEF_EXPLICIT >> BVSCALEDEF_CLASS_SHIFT:
		bverror = parse_explicitscale(bvbltparams, batch);
		break;

	default:
		BVSETBLTERROR(BVERR_SCALE_MODE,
			      "unsupported scale class %d", scaleclass);
		goto exit;
	}

exit:
	GCEXIT(GCZONE_SCALING);
	return bverror;
}
