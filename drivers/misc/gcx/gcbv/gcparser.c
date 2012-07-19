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
#define GCZONE_BLEND		(1 << 1)
#define GCZONE_DEST		(1 << 2)
#define GCZONE_SRC		(1 << 3)

GCDBG_FILTERDEF(gcparser, GCZONE_NONE,
		"format",
		"blend",
		"dest",
		"src")


/*******************************************************************************
 * Pixel format parser.
 */

/* FIXME/TODO: change to use BLTsvile defines. */

#if defined(OCDFMTDEF_ALPHA_SHIFT)
#	undef OCDFMTDEF_ALPHA_SHIFT
#endif

#if defined(OCDFMTDEF_ALPHA_MASK)
#	undef OCDFMTDEF_ALPHA_MASK
#endif

#define OCDFMTDEF_ALPHA_SHIFT 18
#define OCDFMTDEF_ALPHA_MASK (1 << OCDFMTDEF_ALPHA_SHIFT)

#define OCDFMTDEF_PLACEMENT_SHIFT 9
#define OCDFMTDEF_PLACEMENT_MASK (3 << OCDFMTDEF_PLACEMENT_SHIFT)

#define OCDFMTDEF_BITS_SHIFT 3
#define OCDFMTDEF_BITS_MASK (3 << OCDFMTDEF_BITS_SHIFT)

#define OCDFMTDEF_BITS12 (0 << OCDFMTDEF_BITS_SHIFT)
#define OCDFMTDEF_BITS15 (1 << OCDFMTDEF_BITS_SHIFT)
#define OCDFMTDEF_BITS16 (2 << OCDFMTDEF_BITS_SHIFT)
#define OCDFMTDEF_BITS24 (3 << OCDFMTDEF_BITS_SHIFT)

#define BVFORMATRGBA(BPP, Format, Swizzle, R, G, B, A) \
{ \
	BVFMT_RGB, \
	BPP, \
	GCREG_DE_FORMAT_ ## Format, \
	GCREG_DE_SWIZZLE_ ## Swizzle, \
	{ R, G, B, A } \
}

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

#define BVFORMATINVALID \
	{ 0, 0, 0, 0, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } } }

static struct bvformatxlate g_format_nv12 = {
	.type = BVFMT_YUV,
	.bitspp = 8,
	.format = GCREG_DE_FORMAT_NV12,
};
static struct bvformatxlate g_format_uyvy = {
	.type = BVFMT_YUV,
	.bitspp = 16,
	.format = GCREG_DE_FORMAT_UYVY
};
static struct bvformatxlate g_format_yuy2 = {
	.type = BVFMT_YUV,
	.bitspp = 16,
	.format = GCREG_DE_FORMAT_YUY2
};

static struct bvformatxlate formatxlate[] = {
	/*  #0: OCDFMT_xRGB12
		BITS=12 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X4R4G4B4, ARGB,
		BVRED(8, 4), BVGREEN(4, 4), BVBLUE(0, 4), BVALPHA(12, 0)),

	/*  #1: OCDFMT_RGBx12
		BITS=12 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X4R4G4B4, RGBA,
		BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4, 4), BVALPHA(0, 0)),

	/*  #2: OCDFMT_xBGR12
		BITS=12 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X4R4G4B4, ABGR,
		BVRED(0, 4), BVGREEN(4, 4), BVBLUE(8, 4), BVALPHA(12, 0)),

	/*  #3: OCDFMT_BGRx12
		BITS=12 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X4R4G4B4, BGRA,
		BVRED(4, 4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0, 0)),

	/*  #4: OCDFMT_ARGB12
		BITS=12 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A4R4G4B4, ARGB,
		BVRED(8, 4), BVGREEN(4, 4), BVBLUE(0, 4), BVALPHA(12, 4)),

	/*  #5: OCDFMT_RGBA12
		BITS=12 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A4R4G4B4, RGBA,
		BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4, 4), BVALPHA(0, 4)),

	/*  #6: OCDFMT_ABGR12
		BITS=12 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A4R4G4B4, ABGR,
		BVRED(0, 4), BVGREEN(4, 4), BVBLUE(8, 4), BVALPHA(12, 4)),

	/*  #7: OCDFMT_BGRA12
		BITS=12 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A4R4G4B4, BGRA,
		BVRED(4, 4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0, 4)),

	/***********************************************/

	/*  #8: OCDFMT_xRGB15
		BITS=15 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X1R5G5B5, ARGB,
		BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0, 5), BVALPHA(15, 0)),

	/*  #9: OCDFMT_RGBx15
		BITS=15 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X1R5G5B5, RGBA,
		BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1, 5), BVALPHA(0, 0)),

	/* #10: OCDFMT_xBGR15
		BITS=15 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X1R5G5B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 0)),

	/* #11: OCDFMT_BGRx15
		BITS=15 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X1R5G5B5, BGRA,
		BVRED(1, 5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* #12: OCDFMT_ARGB15
		BITS=15 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A1R5G5B5, ARGB,
		BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0, 5), BVALPHA(15, 1)),

	/* #13: OCDFMT_RGBA15
		BITS=15 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A1R5G5B5, RGBA,
		BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1, 5), BVALPHA(0, 1)),

	/* #14: OCDFMT_ABGR15
		BITS=15 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A1R5G5B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 1)),

	/* #15: OCDFMT_BGRA15
		BITS=15 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A1R5G5B5, BGRA,
		BVRED(1, 5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0, 1)),

	/***********************************************/

	/* #16: OCDFMT_RGB16
		BITS=16 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, R5G6B5, ARGB,
		BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0, 5), BVALPHA(0, 0)),

	/* #17: OCDFMT_RGB16
		BITS=16 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, R5G6B5, ARGB,
		BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0, 5), BVALPHA(0, 0)),

	/* #18: OCDFMT_BGR16
		BITS=16 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, R5G6B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* #19: OCDFMT_BGR16
		BITS=16 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, R5G6B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* #20 */
	BVFORMATINVALID,

	/* #21 */
	BVFORMATINVALID,

	/* #22 */
	BVFORMATINVALID,

	/* #23 */
	BVFORMATINVALID,

	/***********************************************/

	/* #24: OCDFMT_xRGB24
		BITS=24 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, X8R8G8B8, BGRA,
		BVRED(8, 8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0, 0)),

	/* #25: OCDFMT_RGBx24
		BITS=24 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, X8R8G8B8, ABGR,
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(24, 0)),

	/* #26: OCDFMT_xBGR24
		BITS=24 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, X8R8G8B8, RGBA,
		BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8, 8), BVALPHA(0, 0)),

	/* #27: OCDFMT_BGRx24
		BITS=24 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, X8R8G8B8, ARGB,
		BVRED(16, 8), BVGREEN(8, 8), BVBLUE(0, 8), BVALPHA(24, 0)),

	/* #28: OCDFMT_ARGB24
		BITS=24 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, A8R8G8B8, BGRA,
		BVRED(8, 8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0, 8)),

	/* #29: OCDFMT_RGBA24
		BITS=24 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, A8R8G8B8, ABGR,
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(24, 8)),

	/* #30: OCDFMT_ABGR24
		BITS=24 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, A8R8G8B8, RGBA,
		BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8, 8), BVALPHA(0, 8)),

	/* #31: OCDFMT_BGRA24
		BITS=24 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, A8R8G8B8, ARGB,
		BVRED(16, 8), BVGREEN(8, 8), BVBLUE(0, 8), BVALPHA(24, 8)),
};

enum bverror parse_format(struct bvbltparams *bvbltparams,
			  enum ocdformat ocdformat,
			  struct bvformatxlate **format)
{
	static unsigned int containers[] = {
		  8,	/* OCDFMTDEF_CONTAINER_8BIT */
		 16,	/* OCDFMTDEF_CONTAINER_16BIT */
		 24,	/* OCDFMTDEF_CONTAINER_24BIT */
		 32,	/* OCDFMTDEF_CONTAINER_32BIT */
		~0U,	/* reserved */
		 48,	/* OCDFMTDEF_CONTAINER_48BIT */
		~0U,	/* reserved */
		 64	/* OCDFMTDEF_CONTAINER_64BIT */
	};

	enum bverror bverror = BVERR_NONE;
	unsigned int cs;
	unsigned int bits;
	unsigned int swizzle;
	unsigned int alpha;
	unsigned int index;
	unsigned int cont;

	GCENTERARG(GCZONE_FORMAT, "ocdformat = 0x%08X\n", ocdformat);

	cs = (ocdformat & OCDFMTDEF_CS_MASK) >> OCDFMTDEF_CS_SHIFT;
	bits = (ocdformat & OCDFMTDEF_COMPONENTSIZEMINUS1_MASK)
		>> OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT;
	cont = (ocdformat & OCDFMTDEF_CONTAINER_MASK)
		>> OCDFMTDEF_CONTAINER_SHIFT;
	GCDBG(GCZONE_FORMAT, "cs = %d\n", cs);
	GCDBG(GCZONE_FORMAT, "bits = %d\n", bits);
	GCDBG(GCZONE_FORMAT, "cont = %d\n", cont);

	switch (cs) {
	case (OCDFMTDEF_CS_RGB >> OCDFMTDEF_CS_SHIFT):
		GCDBG(GCZONE_FORMAT, "OCDFMTDEF_CS_RGB\n");

		if ((ocdformat & OCDFMTDEF_LAYOUT_MASK) != OCDFMTDEF_PACKED) {
			BVSETBLTERROR(BVERR_UNK,
				      "only packed RGBA formats are supported");
			goto exit;
		}

		swizzle = (ocdformat & OCDFMTDEF_PLACEMENT_MASK)
			>> OCDFMTDEF_PLACEMENT_SHIFT;
		alpha = (ocdformat & OCDFMTDEF_ALPHA_MASK)
			>> OCDFMTDEF_ALPHA_SHIFT;

		GCDBG(GCZONE_FORMAT, "swizzle = %d\n", swizzle);
		GCDBG(GCZONE_FORMAT, "alpha = %d\n", alpha);

		index = swizzle | (alpha << 2);

		switch (bits) {
		case 12 - 1:
			index |= OCDFMTDEF_BITS12;
			break;

		case 15 - 1:
			index |= OCDFMTDEF_BITS15;
			break;

		case 16 - 1:
			index |= OCDFMTDEF_BITS16;
			break;

		case 24 - 1:
			index |= OCDFMTDEF_BITS24;
			break;

		default:
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported bit width %d", bits);
			goto exit;
		}

		GCDBG(GCZONE_FORMAT, "index = %d\n", index);
		break;

	case (OCDFMTDEF_CS_YCbCr >> OCDFMTDEF_CS_SHIFT):
		GCDBG(GCZONE_FORMAT, "OCDFMTDEF_CS_YCbCr\n");

		/* FIXME/TODO: add proper YUV parsing. */
		switch (ocdformat) {
		case OCDFMT_NV12:
			GCDBG(GCZONE_FORMAT, "OCDFMT_NV12\n");
			*format = &g_format_nv12;
			goto exit;

		case OCDFMT_UYVY:
			GCDBG(GCZONE_FORMAT, "OCDFMT_UYVY\n");
			*format = &g_format_uyvy;
			goto exit;

		case OCDFMT_YUY2:
			GCDBG(GCZONE_FORMAT, "OCDFMT_YUY2\n");
			*format = &g_format_yuy2;
			goto exit;

		default:
			BVSETBLTERROR(BVERR_UNK,
				      "unsupported YUV format %d", ocdformat);
			goto exit;
		}

	default:
		BVSETBLTERROR(BVERR_UNK,
			      "unsupported color space %d", cs);
		goto exit;
	}

	if (formatxlate[index].bitspp != containers[cont]) {
		BVSETBLTERROR(BVERR_UNK,
			      "unsupported bit width %d", bits);
		goto exit;
	}

	*format = &formatxlate[index];

	GCDBG(GCZONE_FORMAT, "format record = 0x%08X\n",
	      (unsigned int) &formatxlate[index]);
	GCDBG(GCZONE_FORMAT, "  bpp = %d\n", formatxlate[index].bitspp);
	GCDBG(GCZONE_FORMAT, "  format = %d\n", formatxlate[index].format);
	GCDBG(GCZONE_FORMAT, "  swizzle = %d\n", formatxlate[index].swizzle);

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

static bool valid_geom(struct bvbuffdesc *buffdesc,
		       struct bvsurfgeom *geom,
		       struct bvformatxlate *format)
{
	unsigned int size;

	/* Compute the size of the surface. */
	size = (geom->width * geom->height * format->bitspp) / 8;

	/* Make sure the size is not greater then the surface. */
	if (size > buffdesc->length) {
		GCERR("invalid geometry detected:\n");
		GCERR("  specified dimensions: %dx%d, %d bitspp\n",
			geom->width, geom->height, format->bitspp);
		GCERR("  surface size based on the dimensions: %d\n",
			size);
		GCERR("  specified surface size: %lu\n",
			buffdesc->length);
		return false;
	}

	return true;
}

int get_pixel_offset(struct bvbuffdesc *bvbuffdesc,
		     struct bvformatxlate *format,
		     int offset)
{
	unsigned int alignment;
	int byteoffset;
	unsigned int alignedoffset;
	int pixeloffset;

	alignment = (format->type == BVFMT_YUV)
		? (64 - 1)
		: (16 - 1);

	/* Determine offset in bytes from the base modified by the
	 * given offset. */
	if (bvbuffdesc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *bvphysdesc;
		bvphysdesc = (struct bvphysdesc *) bvbuffdesc->auxptr;
		byteoffset = bvphysdesc->pageoffset + offset;
	} else {
		byteoffset = (unsigned int) bvbuffdesc->virtaddr + offset;
	}

	/* Compute the aligned offset. */
	alignedoffset = byteoffset & alignment;

	/* Convert to pixels. */
	pixeloffset = alignedoffset * 8 / format->bitspp;
	return -pixeloffset;
}

enum bverror parse_destination(struct bvbltparams *bvbltparams,
				 struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;

	GCENTER(GCZONE_DEST);

	/* Did clipping/destination rects change? */
	if ((batch->batchflags & (BVBATCH_CLIPRECT |
				  BVBATCH_DESTRECT)) != 0) {
		struct bvrect *dstrect;
		struct bvrect *cliprect;
		int destleft, desttop, destright, destbottom;
		int clipleft, cliptop, clipright, clipbottom;
		unsigned short clippedleft, clippedtop;
		unsigned short clippedright, clippedbottom;

		/* Make shortcuts to the destination objects. */
		dstrect = &bvbltparams->dstrect;
		cliprect = &bvbltparams->cliprect;

		/* Determine destination rectangle. */
		destleft = dstrect->left;
		desttop = dstrect->top;
		destright = destleft + dstrect->width;
		destbottom = desttop + dstrect->height;

		GCDBG(GCZONE_DEST, "destination rectangle:\n");
		GCDBG(GCZONE_DEST, "  dstrect = (%d,%d)-(%d,%d), %dx%d\n",
			destleft, desttop, destright, destbottom,
			dstrect->width, dstrect->height);

		/* Determine clipping. */
		if ((bvbltparams->flags & BVFLAG_CLIP) == BVFLAG_CLIP) {
			clipleft = cliprect->left;
			cliptop = cliprect->top;
			clipright = clipleft + cliprect->width;
			clipbottom = cliptop + cliprect->height;

			if ((clipleft < GC_CLIP_RESET_LEFT) ||
				(cliptop < GC_CLIP_RESET_TOP) ||
				(clipright > GC_CLIP_RESET_RIGHT) ||
				(clipbottom > GC_CLIP_RESET_BOTTOM) ||
				(clipright < clipleft) ||
				(clipbottom < cliptop)) {
				BVSETBLTERROR(BVERR_CLIP_RECT,
					      "invalid clipping rectangle");
				goto exit;
			}

			GCDBG(GCZONE_DEST,
			      "  cliprect = (%d,%d)-(%d,%d), %dx%d\n",
			      clipleft, cliptop, clipright, clipbottom,
			      cliprect->width, cliprect->height);
		} else {
			clipleft = GC_CLIP_RESET_LEFT;
			cliptop = GC_CLIP_RESET_TOP;
			clipright = GC_CLIP_RESET_RIGHT;
			clipbottom = GC_CLIP_RESET_BOTTOM;
		}

		/* Compute clipping deltas and the adjusted destination rect. */
		if (clipleft <= destleft) {
			batch->deltaleft = 0;
			clippedleft = destleft;
		} else {
			batch->deltaleft = clipleft - destleft;
			clippedleft = clipleft;
		}

		if (cliptop <= desttop) {
			batch->deltatop = 0;
			clippedtop = desttop;
		} else {
			batch->deltatop = cliptop - desttop;
			clippedtop = cliptop;
		}

		if (clipright >= destright) {
			batch->deltaright = 0;
			clippedright = destright;
		} else {
			batch->deltaright = clipright - destright;
			clippedright = clipright;
		}

		if (clipbottom >= destbottom) {
			batch->deltabottom = 0;
			clippedbottom = destbottom;
		} else {
			batch->deltabottom = clipbottom - destbottom;
			clippedbottom = clipbottom;
		}

		/* Validate the rectangle. */
		if ((clippedright  > (int) bvbltparams->dstgeom->width) ||
		    (clippedbottom > (int) bvbltparams->dstgeom->height)) {
			BVSETBLTERROR(BVERR_DSTRECT,
				      "destination rect exceeds surface size");
			goto exit;
		}

		/* Set clipped coordinates. */
		batch->clippedleft = clippedleft;
		batch->clippedtop = clippedtop;
		batch->clippedright = clippedright;
		batch->clippedbottom = clippedbottom;

		GCDBG(GCZONE_DEST,
		      "  clipped dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		      clippedleft, clippedtop, clippedright, clippedbottom,
		      clippedright - clippedleft, clippedbottom - clippedtop);
		GCDBG(GCZONE_DEST,
		      "  clipping delta = (%d,%d)-(%d,%d)\n",
		      batch->deltaleft, batch->deltatop,
		      batch->deltaright, batch->deltabottom);
	}

	/* Did the destination surface change? */
	if ((batch->batchflags & BVBATCH_DST) != 0) {
		struct bvbuffdesc *dstdesc;
		struct bvsurfgeom *dstgeom;
		unsigned int stridealign;

		/* Make shortcuts to the destination objects. */
		dstdesc = bvbltparams->dstdesc;
		dstgeom = bvbltparams->dstgeom;

		/* Check for unsupported dest formats. */
		switch (dstgeom->format) {
		case OCDFMT_NV12:
			BVSETBLTERROR(BVERR_DSTGEOM_FORMAT,
				      "destination format unsupported");
			goto exit;

		default:
			break;
		}

		/* Parse the destination format. */
		GCDBG(GCZONE_FORMAT, "parsing destination format.\n");
		if (parse_format(bvbltparams, dstgeom->format,
				   &batch->dstformat) != BVERR_NONE) {
			bverror = BVERR_DSTGEOM_FORMAT;
			goto exit;
		}

		/* Validate geometry. */
		if (!valid_geom(dstdesc, dstgeom, batch->dstformat)) {
			BVSETBLTERROR(BVERR_DSTGEOM,
				      "destination geom exceeds surface size");
			goto exit;
		}

		/* Destination stride must be 8 pixel aligned. */
		stridealign = batch->dstformat->bitspp - 1;
		if ((dstgeom->virtstride & stridealign) != 0) {
			BVSETBLTERROR(BVERR_DSTGEOM_STRIDE,
				      "destination stride must be 8 pixel "
				      "aligned.");
			goto exit;
		}

		/* Parse orientation. */
		batch->dstangle = get_angle(dstgeom->orientation);
		if (batch->dstangle == ROT_ANGLE_INVALID) {
			BVSETBLTERROR(BVERR_DSTGEOM,
				      "unsupported destination orientation %d.",
				      dstgeom->orientation);
		}

		/* Compute the destination offset in pixels needed to compensate
		 * for the surface base address misalignment if any. */
		batch->dstalign = get_pixel_offset(dstdesc,
						   batch->dstformat, 0);

		switch (batch->dstangle) {
		case ROT_ANGLE_0:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->width - batch->dstalign;
			batch->dstphysheight = dstgeom->height;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width - batch->dstalign;
			batch->dstheight = dstgeom->height;

			/* Determine the origin offset. */
			batch->dstoffsetX = -batch->dstalign;
			batch->dstoffsetY = 0;
			break;

		case ROT_ANGLE_90:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->height - batch->dstalign;
			batch->dstphysheight = dstgeom->width;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width;
			batch->dstheight = dstgeom->height - batch->dstalign;

			/* Determine the origin offset. */
			batch->dstoffsetX = 0;
			batch->dstoffsetY = -batch->dstalign;
			break;

		case ROT_ANGLE_180:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->width - batch->dstalign;
			batch->dstphysheight = dstgeom->height;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width - batch->dstalign;
			batch->dstheight = dstgeom->height;

			/* Determine the origin offset. */
			batch->dstoffsetX = 0;
			batch->dstoffsetY = 0;
			break;

		case ROT_ANGLE_270:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->height - batch->dstalign;
			batch->dstphysheight = dstgeom->width;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width;
			batch->dstheight = dstgeom->height - batch->dstalign;

			/* Determine the origin offset. */
			batch->dstoffsetX = 0;
			batch->dstoffsetY = 0;
			break;
		}

		GCDBG(GCZONE_DEST, "destination surface:\n");
		GCDBG(GCZONE_DEST, "  rotation %d degrees.\n",
		      batch->dstangle * 90);

		if (dstdesc->auxtype == BVAT_PHYSDESC) {
			struct bvphysdesc *bvphysdesc;
			bvphysdesc = (struct bvphysdesc *) dstdesc->auxptr;
			GCDBG(GCZONE_DEST, "  page offset = 0x%08X\n",
			      bvphysdesc->pageoffset);
		} else {
			GCDBG(GCZONE_DEST, "  virtual address = 0x%08X\n",
			      (unsigned int) dstdesc->virtaddr);
		}

		GCDBG(GCZONE_DEST, "  stride = %ld\n",
		      dstgeom->virtstride);
		GCDBG(GCZONE_DEST, "  geometry size = %dx%d\n",
		      dstgeom->width, dstgeom->height);
		GCDBG(GCZONE_DEST, "  aligned geometry size = %dx%d\n",
		      batch->dstwidth, batch->dstheight);
		GCDBG(GCZONE_DEST, "  aligned physical size = %dx%d\n",
		      batch->dstphyswidth, batch->dstphysheight);
		GCDBG(GCZONE_DEST, "  origin offset (pixels) = %d,%d\n",
		      batch->dstoffsetX, batch->dstoffsetY);
		GCDBG(GCZONE_DEST, "  surface offset (pixels) = %d,0\n",
		      batch->dstalign);
	}

exit:
	GCEXITARG(GCZONE_DEST, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror parse_source(struct bvbltparams *bvbltparams,
			    struct gcbatch *batch,
			    struct srcinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct bvbuffdesc *srcdesc;
	struct bvsurfgeom *srcgeom;
	struct bvrect *srcrect;
	unsigned int stridealign;

	/* Make shortcuts to the source objects. */
	srcdesc = srcinfo->buf.desc;
	srcgeom = srcinfo->geom;
	srcrect = srcinfo->rect;

	/* Parse the source format. */
	GCDBG(GCZONE_FORMAT, "parsing source%d format.\n",
	      srcinfo->index + 1);
	if (parse_format(bvbltparams, srcgeom->format,
			   &srcinfo->format) != BVERR_NONE) {
		bverror = (srcinfo->index == 0)
			? BVERR_SRC1GEOM_FORMAT
			: BVERR_SRC2GEOM_FORMAT;
		goto exit;
	}

	/* Validate source geometry. */
	if (!valid_geom(srcdesc, srcgeom, srcinfo->format)) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
			      "source%d geom exceeds surface size.",
			      srcinfo->index + 1);
		goto exit;
	}

	/* Source must be 8 pixel aligned. */
	stridealign = srcinfo->format->bitspp - 1;
	if ((srcgeom->virtstride & stridealign) != 0) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM_STRIDE
					: BVERR_SRC2GEOM_STRIDE,
			      "source stride must be 8 pixel aligned.");
		goto exit;
	}

	/* Parse orientation. */
	srcinfo->angle = get_angle(srcgeom->orientation);
	if (srcinfo->angle == ROT_ANGLE_INVALID) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
			      "unsupported source%d orientation %d.",
			      srcinfo->index + 1,
			      srcgeom->orientation);
	}

	/* Determine source mirror. */
	srcinfo->mirror = (srcinfo->index == 0)
			? (bvbltparams->flags >> BVFLAG_FLIP_SRC1_SHIFT)
			   & BVFLAG_FLIP_MASK
			: (bvbltparams->flags >> BVFLAG_FLIP_SRC2_SHIFT)
			   & BVFLAG_FLIP_MASK;

	GCDBG(GCZONE_SRC, "source surface %d:\n", srcinfo->index + 1);
	GCDBG(GCZONE_SRC, "  rotation %d degrees.\n", srcinfo->angle * 90);

	if (srcdesc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *bvphysdesc;
		bvphysdesc = (struct bvphysdesc *) srcdesc->auxptr;
		GCDBG(GCZONE_DEST, "  page offset = 0x%08X\n",
			bvphysdesc->pageoffset);
	} else {
		GCDBG(GCZONE_DEST, "  virtual address = 0x%08X\n",
			(unsigned int) srcdesc->virtaddr);
	}

	GCDBG(GCZONE_SRC, "  stride = %ld\n",
	      srcgeom->virtstride);
	GCDBG(GCZONE_SRC, "  geometry size = %dx%d\n",
	      srcgeom->width, srcgeom->height);
	GCDBG(GCZONE_SRC, "  rect = (%d,%d)-(%d,%d), %dx%d\n",
	      srcrect->left, srcrect->top,
	      srcrect->left + srcrect->width,
	      srcrect->top  + srcrect->height,
	      srcrect->width, srcrect->height);
	GCDBG(GCZONE_SRC, "  mirror = %d\n", srcinfo->mirror);

exit:
	return bverror;
}
