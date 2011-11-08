/*
 * ocd.h
 *
 * Open Color format Definitions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This file defines the Open Color format Definitions (OCD), an open,
 * extensible, color format definition.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef OCD_H
#define OCD_H

/*
 * ocdformat - specifies one of the supported color formats
 *
 * ocdformat consists of 8 bits indicating the vendor ID, followed by 24 bits
 * specified by the vendor.
 *
 * VENDOR_ALL is a common ID with formats defined below.
 */

/****** Bits 31-24 are the vendor ID. The other 24 are vendor defined. ******/
#define OCDFMTDEF_VENDOR_SHIFT 24
#define OCDFMTDEF_VENDOR_MASK (0xFF << OCDFMTDEF_VENDOR_SHIFT)

#define OCDFMTDEF_VENDOR_ALL \
	(0x00 << OCDFMTDEF_VENDOR_SHIFT) /* Common format */
#define OCDFMTDEF_VENDOR_TI  \
	(0x01 << OCDFMTDEF_VENDOR_SHIFT) /* Texas Instruments, Inc. */
/* 0xF0-0xFF reserved */

/***** OCDFMTDEF_VENDOR_ALL *****/
/* The formats in this group are created using combinations of the values
   listed below. */

/*
 * 33222222 222 21 1  1  1  11 111  1
 * 10987654 321 09 8  7  6  54 321  0  9 876 543210
 * [------] [-] [] |  |  |  [] [-]  |  | [-] [----]
 *    |      |  |  |  |  |  |   |   |  |  |    |
 *    |      |  |  |  |  |  |   |   |  |  |  color bits minus 1
 *    |      |  |  |  |  |  |   |   |  |  |
 *    |      |  |  |  |  |  |   |   |  | container
 *    |      |  |  |  |  |  |   |   |  |
 *    |      |  |  |  |  |  |   |   | left justified
 *    |      |  |  |  |  |  |   |   |
 *    |      |  |  |  |  |  |   | reversed
 *    |      |  |  |  |  |  |   |
 *    |      |  |  |  |  |  | layout
 *    |      |  |  |  |  |  |
 *    |      |  |  |  |  | subsampling
 *    |      |  |  |  |  |
 *    |      |  |  |  | subsample position     \
 *    |      |  |  |  |			       |
 *    |      |  |  | non-premult/fill empty 0	> alpha components
 *    |      |  |  |			       |
 *    |      |  | alpha			       /
 *    |      |  |
 *    |      | standard
 *    |      |
 *    |    color space
 *    |
 * vendor ID (VENDOR_ALL = 0x00)
 */

/**** Bits 23-21 define the color space. ****/
#define OCDFMTDEF_CS_SHIFT 21
#define OCDFMTDEF_CS_MASK (7 << OCDFMTDEF_CS_SHIFT)

#define OCDFMTDEF_CS_MONO \
	(0 << OCDFMTDEF_CS_SHIFT) /* Monochrome (luma only) */
#define OCDFMTDEF_CS_LUT \
	(1 << OCDFMTDEF_CS_SHIFT) /* Look-up table (using palette) */
#define OCDFMTDEF_CS_RGB \
	(2 << OCDFMTDEF_CS_SHIFT) /* Red, green, blue */
#define OCDFMTDEF_CS_YCbCr \
	(3 << OCDFMTDEF_CS_SHIFT) /* YCbCr (YUV) (luma & chroma) */
#define OCDFMTDEF_CS_ALPHA \
	(4 << OCDFMTDEF_CS_SHIFT) /* Alpha only (transparency) */
/* 5 reserved */
/* 6 reserved */
/* 7 reserved */

/**** Bits 20-19 define the standard ****/
#define OCDFMTDEF_STD_SHIFT 19
#define OCDFMTDEF_STD_MASK (3 << OCDFMTDEF_STD_SHIFT)

#define OCDFMTDEF_STD_ITUR_601_YCbCr \
	(0 << OCDFMTDEF_STD_SHIFT) /* ITU-R BT.601 - YCbCr only */
/* 0 default for non-YCbCr */
#define OCDFMTDEF_STD_ITUR_709_YCbCr \
	(1 << OCDFMTDEF_STD_SHIFT) /* ITU-R BT.709 - YCbCr only */
/* 1 reserved for non-YCbCr */
/* 2 reserved */
#define OCDFMTDEF_FULLSCALE_YCbCr \
	(3 << OCDFMTDEF_STD_SHIFT) /* RGB 0 to 255 =>
				      YCbCr 0 to 255, -128 to 127 */
/* 3 reserved for non-YCbCr */

/**** Bits 18-16 are component modifiers for non-alpha c/s only ****/
#define OCDFMTDEF_ALPHA	\
	(1 << 18) /* An alpha component is added to the format */
#define OCDFMTDEF_NON_PREMULT \
	(1 << 17) /* Component(s) is(are) not premultiplied by the alpha
		     (default is premultiplied) */
#define OCDFMTDEF_FILL_EMPTY_0 \
	(1 << 17) /* Empty bits are hard-wired to 0 (default is 1) */
#define OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED \
	(0 << 16) /* Subsamples aligned w/1st non-subsample (e.g. MPEG-2) */
#define OCDFMTDEF_SUBSAMPLE_HORZ_CENTERED \
	(1 << 16) /* Subsamples are between non-subsamples (e.g. MPEG-1) */

/*** Bits 18-16 are used differently for alpha c/s ***/
/* Bit 18 is reserved */
/*** Bits 17-16 define the number of alpha components for alpha c/s ***/
#define OCDFMTDEF_ALPHA_COMPONENTS_SHIFT 16
#define OCDFMTDEF_ALPHA_COMPONENTS_MASK (3 << OCDFMTDEF_ALPHA_COMPONENTS_SHIFT)

#define OCDFMTDEF_ALPHA_COMPONENTS_1 (0 << OCDFMTDEF_ALPHA_COMPONENTS_SHIFT)
#define OCDFMTDEF_ALPHA_COMPONENTS_2 (1 << OCDFMTDEF_ALPHA_COMPONENTS_SHIFT)
#define OCDFMTDEF_ALPHA_COMPONENTS_3 (2 << OCDFMTDEF_ALPHA_COMPONENTS_SHIFT)
#define OCDFMTDEF_ALPHA_COMPONENTS_4 (3 << OCDFMTDEF_ALPHA_COMPONENTS_SHIFT)

/**** Bits 15-14 define subsampling ****/
#define OCDFMTDEF_SUBSAMPLE_SHIFT 14
#define OCDFMTDEF_SUBSAMPLE_MASK	(3 << OCDFMTDEF_SUBSAMPLE_SHIFT)

#define OCDFMTDEF_SUBSAMPLE_NONE \
	(0 << OCDFMTDEF_SUBSAMPLE_SHIFT) /* No subsampling;
					    each pixel has each component */
#define OCDFMTDEF_SUBSAMPLE_422_YCbCr \
	(1 << OCDFMTDEF_SUBSAMPLE_SHIFT) /* 4:2:2 subsampling;
					    each horizontal pair of pixels
					    has one Y (luma) component each,
					    but shares one Cb and Cr (chroma)
					    component. */
/* 1 reserved for non-YCbCr */
#define OCDFMTDEF_SUBSAMPLE_420_YCbCr \
	(2 << OCDFMTDEF_SUBSAMPLE_SHIFT) /* 4:2:0 subsampling;
					    each square of four pixels has
					    one Y (luma) component each, but
					    shares one Cb and Cr (chroma)
					    component. */
/* 2 reserved for non-YCbCr */
#define OCDFMTDEF_SUBSAMPLE_411_YCbCr \
	(3 << OCDFMTDEF_SUBSAMPLE_SHIFT) /* 4:1:1 subsampling;
					    each horizontal four pixels have
					    one Y (luma) component each, but
					    shares one Cb and Cr (chroma)
					    component. */
/* 3 reserved for non-YCbCr */

/**** Bits 13-11 define the memory layout
      (combined with _REVERSED and _LEFT_JUSTIFIED) ****/
#define OCDFMTDEF_LAYOUT_SHIFT 11
#define OCDFMTDEF_LAYOUT_MASK (7 << OCDFMTDEF_LAYOUT_SHIFT)

#define OCDFMTDEF_PACKED \
	(0 << OCDFMTDEF_LAYOUT_SHIFT) /* Components interleaved together */
#define OCDFMTDEF_DISTRIBUTED \
	(1 << OCDFMTDEF_LAYOUT_SHIFT) /* Components are distributed evenly
					 across the container; e.g. a 64-bit
					 container with four 8-bit components
					 are distributed with 8 bits between
					 them: __C0__C1__C2__C3 */
#define OCDFMTDEF_2_PLANE_YCbCr \
	(2 << OCDFMTDEF_LAYOUT_SHIFT) /* Y component is separated from Cb & Cr
					 components.  After the Y plane, an
					 interleaved CbCr plane follows. */
/* 2 reserved for non-YCbCr */
#define OCDFMTDEF_3_PLANE_STACKED \
	(3 << OCDFMTDEF_LAYOUT_SHIFT) /* Y, Cb, and Cr components are
					 separated.  After the Y plane is a Cb
					 plane, and then a Cr plane. */
/* 3 reserved for non-YCbCr and non-RGB */
/* 4 reserved */
/* 5 reserved */
/* 6 reserved */
#define OCDFMTDEF_3_PLANE_SIDE_BY_SIDE_YCbCr \
	(7 << OCDFMTDEF_LAYOUT_SHIFT) /* Y, Cb, and Cr components are
					 separated.  After the Y plane the Cb
					 and Cr planes are separated but
					 side-by-side in memory (interleaved
					 on a line-by-line basis). */
/* 7 reserved for non-YCbCr */

/**** Bits 10-9 are layout modifiers. ****/
#define OCDFMTDEF_REVERSED \
	(1 << 10) /* Order of components reversed (default is RGB or CbCr) */
#define OCDFMTDEF_LEFT_JUSTIFIED \
	(1 << 9) /* Components are shifted left (default is shifted right);
		    for 3-plane YCbCr, this indicates wasted space to the
		    right of the Cb & Cr planes (stride matches Y plane). */

/**** Bits 6-8 specify the container type. ****/
#define OCDFMTDEF_CONTAINER_SHIFT 6
#define OCDFMTDEF_CONTAINER_MASK (7 << OCDFMTDEF_CONTAINER_SHIFT)

#define OCDFMTDEF_CONTAINER_8BIT  (0 << OCDFMTDEF_CONTAINER_SHIFT)
#define OCDFMTDEF_CONTAINER_16BIT (1 << OCDFMTDEF_CONTAINER_SHIFT)
#define OCDFMTDEF_CONTAINER_24BIT (2 << OCDFMTDEF_CONTAINER_SHIFT)
#define OCDFMTDEF_CONTAINER_32BIT (3 << OCDFMTDEF_CONTAINER_SHIFT)
/* 4 (0x008000) reserved */
#define OCDFMTDEF_CONTAINER_48BIT (5 << OCDFMTDEF_CONTAINER_SHIFT)
/* 6 (0x00C000) reserved */
#define OCDFMTDEF_CONTAINER_64BIT (7 << OCDFMTDEF_CONTAINER_SHIFT)

/**** Bits 0-5 contain the total number of component bits minus one. ****/
/* To calculate the number of bits for each RGBA component, use the following
 * formula:
 *
 * green bits = int((color bits + 2) / 3)
 * blue bits = int((color bits - green bits) / 2)
 * red bits = color bits - green bits - blue bits
 * alpha bits (when present) = container size - color bits
 *
 * Ex. 1:  RGB16 -> 16 bits
 *	   green bits = int((16 + 2) / 3) = 6
 *         blue bits = int((16 - 6) / 2) = 5
 *         red bits = 16 - 6 - 5 = 5
 *         alpha bits = n/a
 * Ex. 2:  ARGB16 -> 16 bits
 *	   green bits = int((16 + 2) / 3) = 6
 *         blue bits = int((16 - 6) / 2) = 5
 *         red bits = 16 - 6 - 5 = 5
 *         alpha bits = 24 - 16 = 8
 * Ex. 3:  RGB32 -> 32 bits
 *	   green bits = int((32 + 2) / 3) = 11
 *	   blue bits = int((32 - 11) / 2) = 10
 *	   red bits = 32 - 11 - 10 = 11
 *	   alpha bits = n/a
 *
 * For planar formats, the container indicates the total number of bits on the
 * subsampled boundary, while the component bits are the average number of
 * bits per pixel.
 *
 * Ex. 1:  YV12 -> YCbCr 4:2:0 w/8-bit samples -> 4x8 + 2x8 = 48 bit container
 *	   48 bits / 4 pixels = 12 bpp
 * Ex. 2:  NV16 -> YCbCr 4:2:2 w/8-bit samples -> 2x8 + 2x8 = 32 bit container
 *	   24 bits / 2 pixels = 16 bpp
 */
#define OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT 0
#define OCDFMTDEF_COMPONENTSIZEMINUS1_MASK \
	(0x3F << OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT)


/*
 * The formats below are constructed from the definitions above.  However, not
 * all formats possible are specified (and named) below.  The other formats
 * which can be uniquely formed using the above definitions are legitimate
 * formats, and may be used as well.
 */
enum ocdformat {
	OCDFMT_UNKNOWN = -1,
	OCDFMT_NONE = OCDFMT_UNKNOWN,

 /*** Alpha only ***/
 /** Packed **/
	OCDFMT_ALPHA1 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_1 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(1 - 1),
	OCDFMT_ALPHA2 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_1 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(2 - 1),
	OCDFMT_ALPHA4 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_1 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(4 - 1),
	OCDFMT_ALPHA8 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_1 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(8 - 1),
 /* Sub-pixel */
	OCDFMT_ALPHA4x1 = OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_4 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(4 - 1),
	OCDFMT_ALPHA3x8 = OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_3 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_24BIT |
			(24 - 1),
	OCDFMT_ALPHA4x8 = OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_ALPHA |
			OCDFMTDEF_ALPHA_COMPONENTS_4 |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_32BIT |
			(32 - 1),

 /*** Monochrome ***/
 /** Packed **/
	OCDFMT_MONO1 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_MONO |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(1 - 1),
	OCDFMT_MONO2 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_MONO |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(2 - 1),
	OCDFMT_MONO4 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_MONO |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(4 - 1),
	OCDFMT_MONO8 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_MONO |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(8 - 1),

  /*** Palettized (look-up-table) ***/
  /** Packed **/
	OCDFMT_LUT1 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_LUT |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(1 - 1),
	OCDFMT_LUT2 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_LUT |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(2 - 1),
	OCDFMT_LUT4 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_LUT |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(4 - 1),
	OCDFMT_LUT8 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_LUT |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_8BIT |
			(8 - 1),

 /*** RGB ***/
 /** Packed **/
 /* No subsampling */
	OCDFMT_RGB12 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_16BIT |
			(12 - 1),			/* (15):4:4:4 */
	OCDFMT_xRGB12 =	OCDFMT_RGB12,
	OCDFMT_1RGB12 =	OCDFMT_xRGB12,
	OCDFMT_0RGB12 =	OCDFMT_xRGB12 |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):4:4:4 */

	OCDFMT_BGR12 =	OCDFMT_RGB12 |
			OCDFMTDEF_REVERSED,		/* (15):4:4:4 */
	OCDFMT_xBGR12 =	OCDFMT_BGR12,
	OCDFMT_1BGR12 =	OCDFMT_xBGR12,
	OCDFMT_0BGR12 =	OCDFMT_xBGR12 |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):4:4:4 */

	OCDFMT_RGBx12 =	OCDFMT_xRGB12 |
			OCDFMTDEF_LEFT_JUSTIFIED,	/* 4:4:4:(15) */
	OCDFMT_RGB112 =	OCDFMT_RGBx12,
	OCDFMT_RGB012 =	OCDFMT_RGBx12 |
			OCDFMTDEF_FILL_EMPTY_0,		/* 4:4:4:(0) */

	OCDFMT_BGRx12 =	OCDFMT_xRGB12 |
			OCDFMTDEF_LEFT_JUSTIFIED |
			OCDFMTDEF_REVERSED,		/* 4:4:4:(15) */
	OCDFMT_BGR112 =	OCDFMT_BGRx12,
	OCDFMT_BGR012 =	OCDFMT_BGRx12 |
			OCDFMTDEF_FILL_EMPTY_0,		/* 4:4:4:(0) */

	OCDFMT_RGB15 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_16BIT |
			(15 - 1),			/* (1):5:5:5 */
	OCDFMT_xRGB15 =	OCDFMT_RGB15,
	OCDFMT_1RGB15 =	OCDFMT_xRGB15,
	OCDFMT_0RGB15 =	OCDFMT_xRGB15 |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):5:5:5 */

	OCDFMT_BGR15 =	OCDFMT_RGB15 |
			OCDFMTDEF_REVERSED,		/* (1):5:5:5 */
	OCDFMT_xBGR15 =	OCDFMT_BGR15,
	OCDFMT_1BGR15 =	OCDFMT_xBGR15,
	OCDFMT_0BGR15 =	OCDFMT_xBGR15 |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):5:5:5 */

	OCDFMT_RGBx15 =	OCDFMT_RGB15 |
			OCDFMTDEF_LEFT_JUSTIFIED,	/* 5:5:5:(1) */
	OCDFMT_RGB115 =	OCDFMT_RGBx15,
	OCDFMT_RGB015 =	OCDFMT_RGBx15 |
			OCDFMTDEF_FILL_EMPTY_0,		/* 5:5:5:(0) */

	OCDFMT_BGRx15 =	OCDFMT_RGB15 |
			OCDFMTDEF_LEFT_JUSTIFIED |
			OCDFMTDEF_REVERSED,		/* 5:5:5:(1) */
	OCDFMT_BGR115 =	OCDFMT_BGRx15,
	OCDFMT_BGR015 =	OCDFMT_BGRx15 |
			OCDFMTDEF_FILL_EMPTY_0,		/* 5:5:5:(0) */

	OCDFMT_RGB16 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_16BIT |
			(16 - 1),			/* 5:6:5 */
	OCDFMT_BGR16 =	OCDFMT_RGB16 |
			OCDFMTDEF_REVERSED,		/* 5:6:5 */

	OCDFMT_RGB24 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_24BIT |
			(24 - 1),			/* 8:8:8 */
	OCDFMT_BGR24 =	OCDFMT_RGB24 |
			OCDFMTDEF_REVERSED,		/* 8:8:8 */

	OCDFMT_xRGB16 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_24BIT |
			(16 - 1),			/* (255):5:6:5 */
	OCDFMT_1RGB16 =	OCDFMT_xRGB16,
	OCDFMT_0RGB16 =	OCDFMT_xRGB16  |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):5:6:5 */

	OCDFMT_xBGR16 =	OCDFMT_xRGB16 |
			OCDFMTDEF_REVERSED,		/* (255):5:6:5 */
	OCDFMT_1BGR16 =	OCDFMT_xBGR16,
	OCDFMT_0BGR16 =	OCDFMT_xBGR16  |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):5:6:5 */

	OCDFMT_RGBx16 =	OCDFMT_xRGB16 |
			OCDFMTDEF_LEFT_JUSTIFIED,	/* 5:6:5:(255) */
	OCDFMT_RGB116 =	OCDFMT_RGBx16,
	OCDFMT_RGB016 =	OCDFMT_RGBx16  |
			OCDFMTDEF_FILL_EMPTY_0,		/* 5:6:5:(0) */

	OCDFMT_BGRx16 =	OCDFMT_xRGB16 |
			OCDFMTDEF_LEFT_JUSTIFIED |
			OCDFMTDEF_REVERSED,		/* 5:6:5:(255) */
	OCDFMT_BGR116 =	OCDFMT_BGRx16,
	OCDFMT_BGR016 =	OCDFMT_BGRx16  |
			OCDFMTDEF_FILL_EMPTY_0,		/* 5:6:5:(0) */

	OCDFMT_xRGB24 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_32BIT |
			(24 - 1),			/* (255):8:8:8 */
	OCDFMT_1RGB24 =	OCDFMT_xRGB24,
	OCDFMT_0RGB24 =	OCDFMT_xRGB24 |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):8:8:8 */

	OCDFMT_xBGR24 =	OCDFMT_xRGB24 |
			OCDFMTDEF_REVERSED,		/* (255):8:8:8 */
	OCDFMT_1BGR24 =	OCDFMT_xBGR24,
	OCDFMT_0BGR24 =	OCDFMT_xBGR24 |
			OCDFMTDEF_FILL_EMPTY_0,		/* (0):8:8:8 */

	OCDFMT_RGBx24 =	OCDFMT_xRGB24 |
			OCDFMTDEF_LEFT_JUSTIFIED,	/* 8:8:8:(255) */
	OCDFMT_RGB124 =	OCDFMT_RGBx24,
	OCDFMT_RGB024 =	OCDFMT_RGBx24 |
			OCDFMTDEF_FILL_EMPTY_0,		/* 8:8:8:(0) */

	OCDFMT_BGRx24 =	OCDFMT_xRGB24 |
			OCDFMTDEF_LEFT_JUSTIFIED |
			OCDFMTDEF_REVERSED,		/* 8:8:8:(255) */
	OCDFMT_BGR124 =	OCDFMT_BGRx24,
	OCDFMT_BGR024 =	OCDFMT_BGRx24 |
			OCDFMTDEF_FILL_EMPTY_0,		/* 8:8:8:(0) */

  /* Premultiplied ARGB */
	OCDFMT_ARGB12 =	OCDFMT_xRGB12 |
			OCDFMTDEF_ALPHA,		/* 4:4:4:4 */
	OCDFMT_ABGR12 =	OCDFMT_xBGR12 |
			OCDFMTDEF_ALPHA,		/* 4:4:4:4 */
	OCDFMT_RGBA12 =	OCDFMT_RGBx12 |
			OCDFMTDEF_ALPHA,		/* 4:4:4:4 */
	OCDFMT_BGRA12 =	OCDFMT_BGRx12 |
			OCDFMTDEF_ALPHA,		/* 4:4:4:4 */

	OCDFMT_ARGB16 =	OCDFMT_xRGB16 |
			OCDFMTDEF_ALPHA,		/* 8:5:6:5 */
	OCDFMT_ABGR16 =	OCDFMT_ARGB16 |
			OCDFMTDEF_REVERSED,		/* 8:5:6:5 */
	OCDFMT_RGBA16 =	OCDFMT_ARGB16 |
			OCDFMTDEF_LEFT_JUSTIFIED,	/* 5:6:5:8 */
	OCDFMT_BGRA16 =	OCDFMT_ARGB16 |
			OCDFMTDEF_LEFT_JUSTIFIED |
			OCDFMTDEF_REVERSED,		/* 5:6:5:8 */

	OCDFMT_ARGB24 =	OCDFMT_xRGB24 |
			OCDFMTDEF_ALPHA,		/* 8:8:8:8 */
	OCDFMT_ABGR24 =	OCDFMT_xBGR24 |
			OCDFMTDEF_ALPHA,		/* 8:8:8:8 */
	OCDFMT_RGBA24 =	OCDFMT_RGBx24 |
			OCDFMTDEF_ALPHA,		/* 8:8:8:8 */
	OCDFMT_BGRA24 =	OCDFMT_BGRx24 |
			OCDFMTDEF_ALPHA,		/* 8:8:8:8 */

  /* Non-premultiplied ARGB */
	OCDFMT_nARGB12 =	OCDFMT_ARGB12 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_ARGB12_NON_PREMULT = OCDFMT_nARGB12,

	OCDFMT_nABGR12 =	OCDFMT_ABGR12 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_ABGR12_NON_PREMULT = OCDFMT_nABGR12,

	OCDFMT_nRGBA12 =	OCDFMT_RGBA12 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_RGBA12_NON_PREMULT = OCDFMT_nRGBA12,

	OCDFMT_nBGRA12 =	OCDFMT_BGRA12 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_BGRA12_NON_PREMULT = OCDFMT_nBGRA12,

	OCDFMT_ARGB15 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_RGB |
			OCDFMTDEF_ALPHA |
			OCDFMTDEF_NON_PREMULT |
			OCDFMTDEF_SUBSAMPLE_NONE |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_16BIT |
			(15 - 1),			/* 1:5:5:5 - "normal"
							   format is not
							   premultiplied */
	OCDFMT_nARGB15 =	OCDFMT_ARGB15,
	OCDFMT_ARGB15_NON_PREMULT = OCDFMT_nARGB15,

	OCDFMT_ABGR15 =	OCDFMT_ARGB15 |
			OCDFMTDEF_REVERSED,		/* 1:5:5:5 - "normal"
							   format is not
							   premultiplied */
	OCDFMT_nABGR15 =	OCDFMT_ABGR15,
	OCDFMT_ABGR15_NON_PREMULT = OCDFMT_nABGR15,

	OCDFMT_RGBA15 =	OCDFMT_ARGB15 |
			OCDFMTDEF_LEFT_JUSTIFIED,	/* 5:5:5:1 - "normal"
							   format is not
							   premultiplied */
	OCDFMT_nRGBA15 =	OCDFMT_RGBA15,
	OCDFMT_RGBA15_NON_PREMULT = OCDFMT_nRGBA15,

	OCDFMT_BGRA15 =	OCDFMT_ARGB15 |
			OCDFMTDEF_LEFT_JUSTIFIED |
			OCDFMTDEF_REVERSED,		/* 5:5:5:1 - "normal"
							   format is not
							   premultiplied */
	OCDFMT_nBGRA15 =	OCDFMT_BGRA15,
	OCDFMT_BGRA15_NON_PREMULT = OCDFMT_nRGBA15,

	OCDFMT_nARGB16 =	OCDFMT_ARGB16 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_ARGB16_NON_PREMULT = OCDFMT_nARGB16,

	OCDFMT_nABGR16 =	OCDFMT_ABGR16 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_ABGR16_NON_PREMULT =	OCDFMT_nABGR16,

	OCDFMT_nRGBA16 =	OCDFMT_RGBA16 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_RGBA16_NON_PREMULT = OCDFMT_nRGBA16,

	OCDFMT_nBGRA16 = OCDFMT_BGRA16 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_BGRA16_NON_PREMULT = OCDFMT_nBGRA16,

	OCDFMT_nARGB24 =	OCDFMT_ARGB24 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_ARGB24_NON_PREMULT = OCDFMT_nARGB24,

	OCDFMT_nABGR24 =	OCDFMT_ABGR24 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_ABGR24_NON_PREMULT = OCDFMT_nABGR24,

	OCDFMT_nRGBA24 =	OCDFMT_RGBA24 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_RGBA24_NON_PREMULT = OCDFMT_nRGBA24,

	OCDFMT_nBGRA24 =	OCDFMT_BGRA24 |
			OCDFMTDEF_NON_PREMULT,
	OCDFMT_BGRA24_NON_PREMULT = OCDFMT_nBGRA24,

  /*** YCbCr ***/
  /** Packed **/
  /* YCbCr 4:2:2 */
	OCDFMT_UYVY =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_SUBSAMPLE_422_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_PACKED |
			OCDFMTDEF_CONTAINER_32BIT |
			(16 - 1),
	OCDFMT_UYVY_601 = OCDFMT_UYVY |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_UYVY_709 = OCDFMT_UYVY |
			OCDFMTDEF_STD_ITUR_709_YCbCr,
	OCDFMT_Y422 = OCDFMT_UYVY,
	OCDFMT_Y422_601 = OCDFMT_UYVY_601,
	OCDFMT_Y422_709 = OCDFMT_UYVY_709,

	OCDFMT_VYUY =	OCDFMT_UYVY |
			OCDFMTDEF_REVERSED,
	OCDFMT_VYUY_601 = OCDFMT_VYUY |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_VYUY_709 = OCDFMT_VYUY |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_YUYV =	OCDFMT_UYVY |
			OCDFMTDEF_LEFT_JUSTIFIED,
	OCDFMT_YUYV_601 = OCDFMT_YUYV |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_YUYV_709 = OCDFMT_YUYV |
			OCDFMTDEF_STD_ITUR_709_YCbCr,
	OCDFMT_YUY2 = OCDFMT_YUYV,
	OCDFMT_YUY2_601 = OCDFMT_YUYV_601,
	OCDFMT_YUY2_709 = OCDFMT_YUYV_709,

	OCDFMT_YVYU =	OCDFMT_VYUY |
			OCDFMTDEF_LEFT_JUSTIFIED,
	OCDFMT_YVYU_601 = OCDFMT_YVYU |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_YVYU_709 = OCDFMT_YVYU |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

  /** 3-plane **/
  /* YCbCr 4:2:2 */
	OCDFMT_YV16 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_SUBSAMPLE_422_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_3_PLANE_STACKED |
			OCDFMTDEF_CONTAINER_32BIT |
			(16 - 1),
	OCDFMT_YV16_601 = OCDFMT_YV16 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_YV16_709 = OCDFMT_YV16 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

  /* YCbCr 4:2:0 */
	OCDFMT_IYUV =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_SUBSAMPLE_420_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_3_PLANE_STACKED |
			OCDFMTDEF_CONTAINER_48BIT |
			(12 - 1),
	OCDFMT_IYUV_601 = OCDFMT_IYUV |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_IYUV_709 = OCDFMT_IYUV |
			OCDFMTDEF_STD_ITUR_709_YCbCr,
	OCDFMT_I420 = OCDFMT_IYUV,
	OCDFMT_I420_601 = OCDFMT_IYUV_601,
	OCDFMT_I420_709 = OCDFMT_IYUV_709,

	OCDFMT_YV12 =	OCDFMT_IYUV |
			OCDFMTDEF_REVERSED,
	OCDFMT_YV12_601 = OCDFMT_YV12 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_YV12_709 = OCDFMT_YV12 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_IMC3 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_SUBSAMPLE_420_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_3_PLANE_STACKED |
			OCDFMTDEF_LEFT_JUSTIFIED |	/* Indicates wasted
							   space to the
							   right */
			OCDFMTDEF_CONTAINER_48BIT |
			(12 - 1),
	OCDFMT_IMC3_601 = OCDFMT_IMC3 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_IMC3_709 = OCDFMT_IMC3 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_IMC1 =	OCDFMT_IMC3 |
			OCDFMTDEF_REVERSED,
	OCDFMT_IMC1_601 = OCDFMT_IMC1 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_IMC1_709 = OCDFMT_IMC1 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_IMC4 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_STD_ITUR_601_YCbCr |
			OCDFMTDEF_SUBSAMPLE_420_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_3_PLANE_SIDE_BY_SIDE_YCbCr |
			OCDFMTDEF_CONTAINER_48BIT |
			(12 - 1),
	OCDFMT_IMC4_601 = OCDFMT_IMC4 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_IMC4_709 = OCDFMT_IMC4 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_IMC2 =	OCDFMT_IMC4 |
			OCDFMTDEF_REVERSED,
	OCDFMT_IMC2_601 = OCDFMT_IMC2 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_IMC2_709 = OCDFMT_IMC2 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

  /** 2-plane **/
  /* YCbCr 4:2:2 */
	OCDFMT_NV16 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_SUBSAMPLE_422_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_2_PLANE_YCbCr |
			OCDFMTDEF_CONTAINER_32BIT |
			(16 - 1),
	OCDFMT_NV16_601 = OCDFMT_NV16 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_NV16_709 = OCDFMT_NV16 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_NV61 =	OCDFMT_NV16 |
			OCDFMTDEF_REVERSED,
	OCDFMT_NV61_601 = OCDFMT_NV61 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_NV61_709 = OCDFMT_NV61 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

  /* YCbCr 4:2:0 */
	OCDFMT_NV12 =	OCDFMTDEF_VENDOR_ALL |
			OCDFMTDEF_CS_YCbCr |
			OCDFMTDEF_STD_ITUR_601_YCbCr |
			OCDFMTDEF_SUBSAMPLE_420_YCbCr |
			OCDFMTDEF_SUBSAMPLE_HORZ_ALIGNED |
			OCDFMTDEF_2_PLANE_YCbCr |
			OCDFMTDEF_CONTAINER_48BIT |
			(12 - 1),
	OCDFMT_NV12_601 = OCDFMT_NV12 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_NV12_709 = OCDFMT_NV12 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

	OCDFMT_NV21 =	OCDFMT_NV12 |
			OCDFMTDEF_REVERSED,
	OCDFMT_NV21_601 = OCDFMT_NV21 |
			OCDFMTDEF_STD_ITUR_601_YCbCr,
	OCDFMT_NV21_709 = OCDFMT_NV21 |
			OCDFMTDEF_STD_ITUR_709_YCbCr,

#ifdef OCD_EXTERNAL_INCLUDE
#include OCD_EXTERNAL_INCLUDE
#endif
};

#endif /* OCD_H */
