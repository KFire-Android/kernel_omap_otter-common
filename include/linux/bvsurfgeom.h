/*
 * bvsurfgeom.h
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

#ifndef BVSURFGEOM_H
#define BVSURFGEOM_H

/*
 * bvsurfdesc - This structure specifies the way a buffer should be used in a
 * 2-D context.
 */

struct bvsurfgeom {
	unsigned int structsize;	/* used to identify struct version */
	enum ocdformat format;		/* color format of surface */
	unsigned int width;		/* width of the surface in pixels */
	unsigned int height;		/* height of the surface in lines */
	int orientation;		/* angle of the surface in degrees
					   (multiple of 90 only) */
	long virtstride;		/* distance from one pixel to the
					   pixel immediately below it in
					   virtual space */
	enum ocdformat paletteformat;	/* format of palette */
	void *palette;			/* array of palette entries of
					   paletteformat; only valid when
					   format includes BVFMTDEF_LUT;
					   number of entries is 2^bpp. */
};

#endif /* BVSURFGEOM_H */
