/*
 * bvblend.h
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

/*
 * This file defines the types of shared blends available.
 *
 * To extend the list of blends, create a file containing additional
 * enumerations to be added to enum bvblend below.  Then #define
 * BVBLEND_EXTERNAL_INCLUDE as the name of that file before including
 * this file in your project.
 */

#ifndef BVBLEND_H
#define BVBLEND_H

/*
 * bvblend - specifies the type of blending operation to perform; only valid
 *	     when BVFLAG_BLEND is set in the bvbltparams.flags field.
 */

/*
 * The blendmode value is divided into two sections.
 *
 * [31:28] The most significant 4 bits indicate the blend format.
 *
 * [27:0] The remainder of the bits is defined by the format chosen.
 *
 *   3322222222221111111111
 *   10987654321098765432109876543210
 *   [  ][                          ]
 *    |               |
 *  format    defined by format
 */

#define BVBLENDDEF_FORMAT_SHIFT 28
#define BVBLENDDEF_FORMAT_MASK (0xF << BVBLENDDEF_FORMAT_SHIFT)

#define BVBLENDDEF_FORMAT_CLASSIC	(0x0 << BVBLENDDEF_FORMAT_SHIFT)
#define BVBLENDDEF_FORMAT_ESSENTIAL	(0x1 << BVBLENDDEF_FORMAT_SHIFT)

/*
 * The BVBLENDDEF_FORMAT_CLASSIC is meant to handle the classic Porter-Duff
 * equations.  It can also handle the DirectFB blending.
 * BVBLENDDEF_FORMAT_CLASSIC is based on the following equations:
 *
 *   Cd = K1 x C1 + K2 x C2
 *   Ad = K3 x A1 + K4 x A2
 *
 * where:
 *   Cd: destination color
 *   C1: source 1 color
 *   C2: source 2 color
 *   Ad: destination alpha
 *   A1: source 1 alpha
 *   A2: source 2 alpha
 *   K#: one of the constants defined using the bitfields below.
 */

/*
 *  The 28 bits for BVBLENDDEF_FORMAT_CLASSIC are divided into 5 sections.
 *
 *  The most significant 4 bits are modifiers, used to include additional
 *  alpha values from global or remote sources.
 *
 *  [27] The most significant bit indicates that a remote alpha is to be
 *  included in the blend.  The format of this is defined by
 *  bvbltparams.maskgeom.format.
 *
 *  [26] The next bit is reserved.
 *
 *  [25:24] The next 2 bits are used to indicate that a global alpha is to be
 *  included, and what its format is:
 *    00: no global included
 *    01: global included; bvbltparams.globalalpha.size8 is used (0 -> 255)
 *    10: this value is reserved
 *    11: global included; bvbltparams.flogalalpha.fp is used (0.0 -> 1.0)
 *
 *  The remaining bits are divided into 4 sections, one to define each of the
 *  constants:
 *
 *  [23:18] - K1
 *  [17:12] - K2
 *  [11:6]  - K3
 *  [5:0]   - K4
 *
 *  The format is the same for all 4 constant fields:
 *
 *  [5:4] The first 2 bits of each field indicates the way in which the other
 *  2 fields are interpreted:
 *    00: only As: the other two fields contain only As; there should be only
 *                 one valid A value between the two fields
 *    01: minimum: the value of the constant is the minimum of the two fields
 *    10: maximum: the value of the constant is the maximum of the two fields
 *    11: only Cs: the other two fields contain only Cs; there should be only
 *                 one valid C value between the two fields
 *
 *  [3:2] The middle 2 bits of each field contain the inverse field:
 *    00: 1-C1 ("don't care" for "only As")
 *    01: 1-A1 ("don't care" for "only Cs")
 *    10: 1-C2 ("don't care" for "only As")
 *    11: 1-A2 ("don't care" for "only Cs")
 *
 *  [1:0] The last 2 bits if each field contain the normal field:
 *    00: C1 ("don't care" for "only As")
 *    01: A1 ("don't care" for "only Cs")
 *    10: C2 ("don't care" for "only As")
 *    11: A2 ("don't care" for "only Cs")
 *
 *  EXCEPTIONS:
 *
 *  00 00 00 - The value 00 00 00, which normally would indicate "only As"
 *             with two "don't care" fields, is interpreted as a 0.
 *
 *  11 11 11 - The value 11 11 11, which normally would indicate "only Cs"
 *             with two "don't care" fields, is interpreted as a 1.
 *
 * --------------------------------------------------------------------------
 *
 * Put together, these can define portions of the blend equations that can be
 * put together in a variety of ways:
 *
 *   00 00 00: undefined -> zero
 *   00 00 01: A1 (preferred)
 *   00 00 10: undefined
 *   00 00 11: A2 (preferred)
 *   00 01 00: 1-A1 (preferred)
 *   00 01 01: undefined
 *   00 01 10: 1-A1 (use 00 01 00)
 *   00 01 11: undefined
 *   00 10 00: undefined
 *   00 10 01: A1 (use 00 00 01)
 *   00 10 10: undefined
 *   00 10 11: A2 (use 00 00 11)
 *   00 11 00: 1-A2 (preferred)
 *   00 11 01: undefined
 *   00 11 10: 1-A2 (use 00 11 00)
 *   00 11 11: undefined
 *
 *   01 00 00: min(C1,1-C1)
 *   01 00 01: min(A1,1-C1)
 *   01 00 10: min(C2,1-C1)
 *   01 00 11: min(A2,1-C1)
 *   01 01 00: min(C1,1-A1)
 *   01 01 01: min(A1,1-A1)
 *   01 01 10: min(C2,1-A1)
 *   01 01 11: min(A2,1-A1)
 *   01 10 00: min(C1,1-C2)
 *   01 10 01: min(A1,1-C2)
 *   01 10 10: min(C2,1-C2)
 *   01 10 11: min(A2,1-C2)
 *   01 11 00: min(C1,1-A2)
 *   01 11 01: min(A1,1-A2)
 *   01 11 10: min(C2,1-A2)
 *   01 11 11: min(A2,1-A2)
 *
 *   10 00 00: max(C1,1-C1)
 *   10 00 01: max(A1,1-C1)
 *   10 00 10: max(C2,1-C1)
 *   10 00 11: max(A2,1-C1)
 *   10 01 00: max(C1,1-A1)
 *   10 01 01: max(A1,1-A1)
 *   10 01 10: max(C2,1-A1)
 *   10 01 11: max(A2,1-A1)
 *   10 10 00: max(C1,1-C2)
 *   10 10 01: max(A1,1-C2)
 *   10 10 10: max(C2,1-C2)
 *   10 10 11: max(A2,1-C2)
 *   10 11 00: max(C1,1-A2)
 *   10 11 01: max(A1,1-A2)
 *   10 11 10: max(C2,1-A2)
 *   10 11 11: max(A2,1-A2)
 *
 *   11 00 00: undefined
 *   11 00 01: 1-C1 (use 11 00 11)
 *   11 00 10: undefined
 *   11 00 11: 1-C1 (preferred)
 *   11 01 00: C1 (use 11 11 00)
 *   11 01 01: undefined
 *   11 01 10: C2 (use 11 11 10)
 *   11 01 11: undefined
 *   11 10 00: undefined
 *   11 10 01: 1-C2 (use 11 10 11)
 *   11 10 10: undefined
 *   11 10 11: 1-C2 (preferred)
 *   11 11 00: C1 (preferred)
 *   11 11 01: undefined
 *   11 11 10: C2 (preferred)
 *   11 11 11: undefined -> one
 *
 * ==========================================================================
 * DirectFB
 * ==========================================================================
 *
 * Putting these together into the proper constants, the blending equations
 * can be built for DirectFB as well:
 *
 * For DirectFB, the SetSrcBlendFunction() and SetDstBlendFunction() can
 * specify 121 combinations of blends (11 x 11). It's impractical to
 * specify these combinations individually. Instead, the settings indicated
 * by each call should be bitwise OR'd to make the proper single value used in
 * BLTsville.
 *
 * binary value <- SetSrcBlendFunction()
 *           [--K1--] [--K2--] [--K3--] [--K4--]
 * 0000 0000 00 00 00 xx xx xx 00 00 00 xx xx xx <- DSBF_ZERO
 * 0000 0000 11 11 11 xx xx xx 11 11 11 xx xx xx <- DSBF_ONE
 * 0000 0000 11 11 00 xx xx xx 00 00 01 xx xx xx <- DSBF_SRCCOLOR
 * 0000 0000 11 00 11 xx xx xx 00 01 00 xx xx xx <- DSBF_INVSRCCOLOR
 * 0000 0000 00 00 01 xx xx xx 00 00 01 xx xx xx <- DSBF_SRCALPHA
 * 0000 0000 00 01 00 xx xx xx 00 01 00 xx xx xx <- DSBF_INVSRCALPHA
 * 0000 0000 11 11 10 xx xx xx 00 00 11 xx xx xx <- DSBF_DESTCOLOR
 * 0000 0000 11 10 11 xx xx xx 00 11 00 xx xx xx <- DSBF_INVDESTCOLOR
 * 0000 0000 00 00 11 xx xx xx 00 00 11 xx xx xx <- DSBF_DESTALPHA
 * 0000 0000 00 11 00 xx xx xx 00 11 00 xx xx xx <- DSBF_INVDESTALPHA
 * 0000 0000 01 11 01 xx xx xx 11 11 11 xx xx xx <- DSBF_SRCALPHASAT
 *
 * binary value <- SetDstBlendFunction()
 *           [--K1--] [--K2--] [--K3--] [--K4--]
 * 0000 0000 xx xx xx 00 00 00 xx xx xx 00 00 00 <- DSBF_ZERO
 * 0000 0000 xx xx xx 11 11 11 xx xx xx 11 11 11 <- DSBF_ONE
 * 0000 0000 xx xx xx 11 11 00 xx xx xx 00 00 01 <- DSBF_SRCCOLOR
 * etc.
 *
 * ==========================================================================
 * Porter-Duff
 * ==========================================================================
 *
 * For Porter-Duff, the equations can be more specifically defined. For
 * convenience, these are enumerated below. These utilize the local alpha as
 * indicated. To use global or remote alpha, these enumerations need to be
 * modified. For example, to include the global alpha in the Porter-Duff
 * SRC1OVER blend, the blend could be defined like this:
 *   params.op.blend = BVBLEND_SRC1OVER +
 *                     BVBLENDDEF_GLOBAL_UCHAR;
 *
 * To include the remote alpha, the blend could be defined like this:
 *   params.op.blend = BVBLEND_SRC1OVER +
 *                     BVBLENDDEF_REMOTE;
 *
 * And to include both:
 *   params.op.blend = BVBLEND_SRC1OVER +
 *                     BVBLENDDEF_GLOBAL_UCHAR +
 *                     BVBLENDDEF_REMOTE;
 *
 * Note that if the source color formats include local alphas, the local
 * alphas, global alpha, and remote alpha will be used together.
 *
 * Note also that the equations assume the surfaces are premultiplied. So
 * if the surface formats indicate that they are not premultiplied, the
 * alpha multiplication of each color is done prior to using the surface
 * values in the equations.
 *
 * For example, BVBLEND_SRC1OVER specifies the equations:
 *   Cd = 1 x C1 + (1 - A1) x C2
 *   Ad = 1 x A1 + (1 - A1) x A2
 *
 * If the format of surface 1 is non-premultiplied, the equations
 * are modified to include the multiplication explicitly:
 *   Cd = 1 x A1 x C1 + (1 - A1) x C2
 *   Ad = 1 x A1      + (1 - A1) x A2
 *
 * Likewise, if the format of surface 2 is non-premultiplied, the
 * equations are modified for this:
 *   Cd = 1 x C1 + (1 - A1) x A2 x C2
 *   Ad = 1 x A1 + (1 - A1) x A2
 *
 * When including global or remote alphas, these values are used to modify
 * the source 1 value values before being used in the blend equation:
 *   C1 = Ag x C1
 *   A1 = Ag x A1
 *       -or-
 *   C1 = Ar x C1
 *   A1 = Ar x A1
 *       -or-
 *   C1 = Ag x Ar x C1
 *   A1 = Ag x Ar x A1
 *
 */

#define BVBLENDDEF_MODE_SHIFT	4
#define BVBLENDDEF_INV_SHIFT	2
#define BVBLENDDEF_NORM_SHIFT	0

#define BVBLENDDEF_ONLY_A	(0 << BVBLENDDEF_MODE_SHIFT)
#define BVBLENDDEF_MIN		(1 << BVBLENDDEF_MODE_SHIFT)
#define BVBLENDDEF_MAX		(2 << BVBLENDDEF_MODE_SHIFT)
#define BVBLENDDEF_ONLY_C	(3 << BVBLENDDEF_MODE_SHIFT)
#define BVBLENDDEF_MODE_MASK	(3 << BVBLENDDEF_MODE_SHIFT)

#define BVBLENDDEF_NORM_C1	(0 << BVBLENDDEF_NORM_SHIFT)
#define BVBLENDDEF_NORM_A1	(1 << BVBLENDDEF_NORM_SHIFT)
#define BVBLENDDEF_NORM_C2	(2 << BVBLENDDEF_NORM_SHIFT)
#define BVBLENDDEF_NORM_A2	(3 << BVBLENDDEF_NORM_SHIFT)
#define BVBLENDDEF_NORM_MASK	(3 << BVBLENDDEF_NORM_SHIFT)

#define BVBLENDDEF_INV_C1	(0 << BVBLENDDEF_INV_SHIFT)
#define BVBLENDDEF_INV_A1	(1 << BVBLENDDEF_INV_SHIFT)
#define BVBLENDDEF_INV_C2	(2 << BVBLENDDEF_INV_SHIFT)
#define BVBLENDDEF_INV_A2	(3 << BVBLENDDEF_INV_SHIFT)
#define BVBLENDDEF_INV_MASK	(3 << BVBLENDDEF_INV_SHIFT)

#define BVBLENDDEF_ONLY_A_NORM_xx	BVBLENDDEF_NORM_C1
#define BVBLENDDEF_ONLY_A_INV_xx	BVBLENDDEF_INV_C1
#define BVBLENDDEF_ONLY_C_NORM_xx	BVBLENDDEF_NORM_A2
#define BVBLENDDEF_ONLY_C_INV_xx	BVBLENDDEF_INV_A2

#define BVBLENDDEF_ZERO \
	(BVBLENDDEF_ONLY_A | \
	 BVBLENDDEF_ONLY_A_NORM_xx | \
	 BVBLENDDEF_ONLY_A_INV_xx)
#define BVBLENDDEF_C1 \
	(BVBLENDDEF_ONLY_C | \
	 BVBLENDDEF_NORM_C1 | \
	 BVBLENDDEF_ONLY_C_INV_xx)
#define BVBLENDDEF_A1 \
	(BVBLENDDEF_ONLY_A | \
	 BVBLENDDEF_NORM_A1 | \
	 BVBLENDDEF_ONLY_A_INV_xx)
#define BVBLENDDEF_C2 \
	(BVBLENDDEF_ONLY_C | \
	 BVBLENDDEF_NORM_C2 | \
	 BVBLENDDEF_ONLY_C_INV_xx)
#define BVBLENDDEF_A2 \
	(BVBLENDDEF_ONLY_A | \
	 BVBLENDDEF_NORM_A2 | \
	 BVBLENDDEF_ONLY_A_INV_xx)
#define BVBLENDDEF_ONE_MINUS_C1 \
	(BVBLENDDEF_ONLY_C | \
	 BVBLENDDEF_ONLY_C_NORM_xx | \
	 BVBLENDDEF_INV_C1)
#define BVBLENDDEF_ONE_MINUS_A1 \
	(BVBLENDDEF_ONLY_A | \
	 BVBLENDDEF_ONLY_A_NORM_xx | \
	 BVBLENDDEF_INV_A1)
#define BVBLENDDEF_ONE_MINUS_C2 \
	(BVBLENDDEF_ONLY_C | \
	 BVBLENDDEF_ONLY_C_NORM_xx | \
	 BVBLENDDEF_INV_C2)
#define BVBLENDDEF_ONE_MINUS_A2 \
	(BVBLENDDEF_ONLY_A | \
	 BVBLENDDEF_ONLY_A_NORM_xx | \
	 BVBLENDDEF_INV_A2)
#define BVBLENDDEF_ONE \
	(BVBLENDDEF_ONLY_C | \
	 BVBLENDDEF_ONLY_C_NORM_xx | \
	 BVBLENDDEF_ONLY_C_INV_xx)

#define BVBLENDDEF_K_MASK \
	(BVBLENDDEF_MODE_MASK | \
	 BVBLENDDEF_INV_MASK  | \
	 BVBLENDDEF_NORM_MASK)

#define BVBLENDDEF_K1_SHIFT 18
#define BVBLENDDEF_K2_SHIFT 12
#define BVBLENDDEF_K3_SHIFT 6
#define BVBLENDDEF_K4_SHIFT 0

#define BVBLENDDEF_K1_MASK \
	(BVBLENDDEF_K_MASK << BVBLENDDEF_K1_SHIFT)
#define BVBLENDDEF_K2_MASK \
	(BVBLENDDEF_K_MASK << BVBLENDDEF_K2_SHIFT)
#define BVBLENDDEF_K3_MASK \
	(BVBLENDDEF_K_MASK << BVBLENDDEF_K3_SHIFT)
#define BVBLENDDEF_K4_MASK \
	(BVBLENDDEF_K_MASK << BVBLENDDEF_K4_SHIFT)

#define BVBLENDDEF_CLASSIC_EQUATION_MASK 0x00FFFFFF

/*
 * The following definitions are be used to modify the enumerations.
 */
#define BVBLENDDEF_REMOTE	0x08000000	/* mask surface provides alpha
						   for source 1 */

/* Bit 26 reserved */

/* These enable global alpha and define the type of globalalpha */
#define BVBLENDDEF_GLOBAL_SHIFT 24
#define BVBLENDDEF_GLOBAL_MASK	(3 << BVBLENDDEF_GLOBAL_SHIFT)

#define BVBLENDDEF_GLOBAL_NONE	(0 << BVBLENDDEF_GLOBAL_SHIFT)
#define BVBLENDDEF_GLOBAL_UCHAR	(1 << BVBLENDDEF_GLOBAL_SHIFT)
/* 2 reserved */
#define BVBLENDDEF_GLOBAL_FLOAT	(3 << BVBLENDDEF_GLOBAL_SHIFT)

union bvalpha {
	unsigned char size8;	/* btwn 0 (0.0) and 255 (1.0) */
	float fp;		/* btwn 0.0 and 1.0 */
};


enum bvblend {
  /* Porter-Duff blending equations */
	BVBLEND_CLEAR = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC1 =	BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC2 =	BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC1OVER = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC2OVER = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC1IN = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_A2 << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_A2 << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC2IN = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_A1 << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_A1 << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC1OUT = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC2OUT = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ZERO << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC1ATOP = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_A2 << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_A2 << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K4_SHIFT),
	BVBLEND_SRC2ATOP = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_A1 << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_A1 << BVBLENDDEF_K4_SHIFT),
	BVBLEND_XOR = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A2 << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE_MINUS_A1 << BVBLENDDEF_K4_SHIFT),
	BVBLEND_PLUS = BVBLENDDEF_FORMAT_CLASSIC |
			(BVBLENDDEF_ONE << BVBLENDDEF_K1_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K2_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K3_SHIFT) |
			(BVBLENDDEF_ONE << BVBLENDDEF_K4_SHIFT),

/*
 * For FORMAT_ESSENTIAL, the variety of well-known blending functions from
 * popular image manipulation programs are specified.
 */

	BVBLEND_NORMAL = BVBLENDDEF_FORMAT_ESSENTIAL + 0,
	BVBLEND_LIGHTEN = BVBLENDDEF_FORMAT_ESSENTIAL + 1,
	BVBLEND_DARKEN = BVBLENDDEF_FORMAT_ESSENTIAL + 2,
	BVBLEND_MULTIPLY = BVBLENDDEF_FORMAT_ESSENTIAL + 3,
	BVBLEND_AVERAGE = BVBLENDDEF_FORMAT_ESSENTIAL + 4,
	BVBLEND_ADD = BVBLENDDEF_FORMAT_ESSENTIAL + 5,
	BVBLEND_LINEAR_DODGE = BVBLEND_ADD,
	BVBLEND_SUBTRACT = BVBLENDDEF_FORMAT_ESSENTIAL + 6,
	BVBLEND_LINEAR_BURN = BVBLEND_SUBTRACT,
	BVBLEND_DIFFERENCE = BVBLENDDEF_FORMAT_ESSENTIAL + 7,
	BVBLEND_NEGATE = BVBLENDDEF_FORMAT_ESSENTIAL + 8,
	BVBLEND_SCREEN = BVBLENDDEF_FORMAT_ESSENTIAL + 9,
	BVBLEND_EXCLUSION = BVBLENDDEF_FORMAT_ESSENTIAL + 10,
	BVBLEND_OVERLAY = BVBLENDDEF_FORMAT_ESSENTIAL + 11,
	BVBLEND_SOFT_LIGHT = BVBLENDDEF_FORMAT_ESSENTIAL + 12,
	BVBLEND_HARD_LIGHT = BVBLENDDEF_FORMAT_ESSENTIAL + 13,
	BVBLEND_COLOR_DODGE = BVBLENDDEF_FORMAT_ESSENTIAL + 14,
	BVBLEND_COLOR_BURN = BVBLENDDEF_FORMAT_ESSENTIAL + 15,
	BVBLEND_LINEAR_LIGHT = BVBLENDDEF_FORMAT_ESSENTIAL + 16,
	BVBLEND_VIVID_LIGHT = BVBLENDDEF_FORMAT_ESSENTIAL + 17,
	BVBLEND_PIN_LIGHT = BVBLENDDEF_FORMAT_ESSENTIAL + 18,
	BVBLEND_HARD_MIX = BVBLENDDEF_FORMAT_ESSENTIAL + 19,
	BVBLEND_REFLECT = BVBLENDDEF_FORMAT_ESSENTIAL + 20,
	BVBLEND_GLOW = BVBLENDDEF_FORMAT_ESSENTIAL + 21,
	BVBLEND_PHOENIX = BVBLENDDEF_FORMAT_ESSENTIAL + 22,

#ifdef BVBLEND_EXTERNAL_INCLUDE
#define BVBLEND_EXTERNAL_INCLUDE
#endif
};

#endif /* BVBLEND_H */
