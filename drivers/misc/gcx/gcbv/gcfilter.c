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
#define GCZONE_KERNEL		(1 << 0)
#define GCZONE_FILTER		(1 << 1)
#define GCZONE_BLEND		(1 << 2)
#define GCZONE_TYPE		(1 << 3)

GCDBG_FILTERDEF(gcfilter, GCZONE_NONE,
		"kernel",
		"filter",
		"blend",
		"type")


/*******************************************************************************
 * Miscellaneous defines.
 */

#define GC_BITS_PER_CACHELINE	(64 * 8)


/*******************************************************************************
 * Scale factor format: unsigned 1.31 fixed point.
 */

#define GC_SCALE_TYPE		unsigned int
#define GC_SCALE_FRACTION	31
#define GC_SCALE_ONE		((GC_SCALE_TYPE) (1 << GC_SCALE_FRACTION))


/*******************************************************************************
 * X coordinate format: signed 4.28 fixed point.
 */

#define GC_COORD_TYPE		int
#define GC_COORD_FRACTION	28
#define GC_COORD_PI		((GC_COORD_TYPE) 0x3243F6C0)
#define GC_COORD_2OVERPI	((GC_COORD_TYPE) 0x0A2F9832)
#define GC_COORD_PIOVER2	((GC_COORD_TYPE) 0x1921FB60)
#define GC_COORD_ZERO		((GC_COORD_TYPE) 0)
#define GC_COORD_HALF		((GC_COORD_TYPE) (1 << (GC_COORD_FRACTION - 1)))
#define GC_COORD_ONE		((GC_COORD_TYPE) (1 << GC_COORD_FRACTION))
#define GC_COORD_NEGONE		((GC_COORD_TYPE) (~GC_COORD_ONE + 1))
#define GC_COORD_SUBPIX_STEP	((GC_COORD_TYPE) \
				(1 << (GC_COORD_FRACTION - GC_PHASE_BITS)))


/*******************************************************************************
 * Hardware coefficient format: signed 2.14 fixed point.
 */

#define GC_COEF_TYPE		short
#define GC_COEF_FRACTION	14
#define GC_COEF_ZERO		((GC_COEF_TYPE) 0)
#define GC_COEF_ONE		((GC_COEF_TYPE) (1 << GC_COEF_FRACTION))
#define GC_COEF_NEGONE		((GC_COEF_TYPE) (~GC_COEF_ONE + 1))


/*******************************************************************************
 * Weight sum format: x.28 fixed point.
 */

#define GC_SUM_TYPE		long long
#define GC_SUM_FRACTION		GC_COORD_FRACTION


/*******************************************************************************
 * Math shortcuts.
 */

#define computescale(dstsize, srcsize) ((GC_SCALE_TYPE) \
	div_u64(((u64) (dstsize)) << GC_SCALE_FRACTION, (srcsize)) \
)

#define normweight(weight, sum) ((GC_COORD_TYPE) \
	div64_s64(((s64) (weight)) << GC_COORD_FRACTION, (sum)) \
)

#define convertweight(weight) ((GC_COEF_TYPE) \
	((weight) >> (GC_COORD_FRACTION - GC_COEF_FRACTION)) \
)


/*******************************************************************************
 * Fixed point SINE function. Takes a positive value in range [0..pi/2].
 */

static GC_COORD_TYPE sine(GC_COORD_TYPE x)
{
	static const GC_COORD_TYPE sinetable[] = {
		0x00000000, 0x001FFFEB, 0x003FFF55, 0x005FFDC0,
		0x007FFAAB, 0x009FF596, 0x00BFEE01, 0x00DFE36C,
		0x00FFD557, 0x011FC344, 0x013FACB2, 0x015F9120,
		0x017F7010, 0x019F4902, 0x01BF1B78, 0x01DEE6F2,
		0x01FEAAEE, 0x021E66F0, 0x023E1A7C, 0x025DC50C,
		0x027D6624, 0x029CFD48, 0x02BC89F8, 0x02DC0BB8,
		0x02FB8204, 0x031AEC64, 0x033A4A5C, 0x03599B64,
		0x0378DF08, 0x039814CC, 0x03B73C2C, 0x03D654B0,
		0x03F55DDC, 0x04145730, 0x04334030, 0x04521868,
		0x0470DF58, 0x048F9488, 0x04AE3770, 0x04CCC7A8,
		0x04EB44A8, 0x0509ADF8, 0x05280328, 0x054643B0,
		0x05646F28, 0x05828508, 0x05A084E0, 0x05BE6E38,
		0x05DC4098, 0x05F9FB80, 0x06179E88, 0x06352928,
		0x06529AF8, 0x066FF380, 0x068D3248, 0x06AA56D8,
		0x06C760C0, 0x06E44F90, 0x070122C8, 0x071DD9F8,
		0x073A74B8, 0x0756F290, 0x07735308, 0x078F95B0,
		0x07ABBA20, 0x07C7BFD8, 0x07E3A678, 0x07FF6D88,
		0x081B14A0, 0x08369B40, 0x08520110, 0x086D4590,
		0x08886860, 0x08A36910, 0x08BE4730, 0x08D90250,
		0x08F39A20, 0x090E0E10, 0x09285DD0, 0x094288E0,
		0x095C8EF0, 0x09766F90, 0x09902A60, 0x09A9BEE0,
		0x09C32CC0, 0x09DC7390, 0x09F592F0, 0x0A0E8A70,
		0x0A2759C0, 0x0A400070, 0x0A587E20, 0x0A70D270,
		0x0A88FD00, 0x0AA0FD60, 0x0AB8D350, 0x0AD07E50,
		0x0AE7FE10, 0x0AFF5230, 0x0B167A50, 0x0B2D7610,
		0x0B444520, 0x0B5AE730, 0x0B715BC0, 0x0B87A290,
		0x0B9DBB40, 0x0BB3A580, 0x0BC960F0, 0x0BDEED30,
		0x0BF44A00, 0x0C0976F0, 0x0C1E73D0, 0x0C334020,
		0x0C47DBB0, 0x0C5C4620, 0x0C707F20, 0x0C848660,
		0x0C985B80, 0x0CABFE50, 0x0CBF6E60, 0x0CD2AB80,
		0x0CE5B550, 0x0CF88B80, 0x0D0B2DE0, 0x0D1D9C10,
		0x0D2FD5C0, 0x0D41DAB0, 0x0D53AAA0, 0x0D654540,
		0x0D76AA40, 0x0D87D970, 0x0D98D280, 0x0DA99530,
		0x0DBA2140, 0x0DCA7650, 0x0DDA9450, 0x0DEA7AD0,
		0x0DFA29B0, 0x0E09A0B0, 0x0E18DF80, 0x0E27E5F0,
		0x0E36B3C0, 0x0E4548B0, 0x0E53A490, 0x0E61C720,
		0x0E6FB020, 0x0E7D5F70, 0x0E8AD4C0, 0x0E980FF0,
		0x0EA510B0, 0x0EB1D6F0, 0x0EBE6260, 0x0ECAB2D0,
		0x0ED6C810, 0x0EE2A200, 0x0EEE4070, 0x0EF9A310,
		0x0F04C9E0, 0x0F0FB490, 0x0F1A6300, 0x0F24D510,
		0x0F2F0A80, 0x0F390340, 0x0F42BF10, 0x0F4C3DE0,
		0x0F557F70, 0x0F5E83C0, 0x0F674A80, 0x0F6FD3B0,
		0x0F781F20, 0x0F802CB0, 0x0F87FC40, 0x0F8F8DA0,
		0x0F96E0D0, 0x0F9DF5B0, 0x0FA4CC00, 0x0FAB63D0,
		0x0FB1BCF0, 0x0FB7D740, 0x0FBDB2B0, 0x0FC34F30,
		0x0FC8ACA0, 0x0FCDCAF0, 0x0FD2AA10, 0x0FD749E0,
		0x0FDBAA50, 0x0FDFCB50, 0x0FE3ACD0, 0x0FE74EC0,
		0x0FEAB110, 0x0FEDD3C0, 0x0FF0B6B0, 0x0FF359F0,
		0x0FF5BD50, 0x0FF7E0E0, 0x0FF9C490, 0x0FFB6850,
		0x0FFCCC30, 0x0FFDF010, 0x0FFED400, 0x0FFF77F0,
		0x0FFFDBF0, 0x0FFFFFE0, 0x0FFFE3D0, 0x0FFF87D0,
		0x0FFEEBC0, 0x0FFE0FC0, 0x0FFCF3D0, 0x0FFB97E0
	};

	static const unsigned int indexwidth = 8;
	static const unsigned int intwidth = 1;
	static const unsigned int indexshift = intwidth
					     + GC_COORD_FRACTION
					     - indexwidth;

	unsigned int p1, p2;
	GC_COORD_TYPE p1x, p2x;
	GC_COORD_TYPE p1y, p2y;
	GC_COORD_TYPE dx, dy;
	GC_COORD_TYPE a, b;
	GC_COORD_TYPE result;

	/* Determine the indices of two closest points in the table. */
	p1 = ((unsigned int) x) >> indexshift;
	p2 =  p1 + 1;

	if ((p1 >= countof(sinetable)) || (p2 >= countof(sinetable))) {
		GCERR("invalid table index.\n");
		return GC_COORD_ZERO;
	}

	/* Determine the coordinates of the two closest points.  */
	p1x = p1 << indexshift;
	p2x = p2 << indexshift;

	p1y = sinetable[p1];
	p2y = sinetable[p2];

	/* Determine the deltas. */
	dx = p2x - p1x;
	dy = p2y - p1y;

	/* Find the slope and the y-intercept. */
	b = (GC_COORD_TYPE) div64_s64(((s64) dy) << GC_COORD_FRACTION, dx);
	a = p1y - (GC_COORD_TYPE) (((s64) b * p1x) >> GC_COORD_FRACTION);

	/* Compute the result. */
	result = a + (GC_COORD_TYPE) (((s64) b * x) >> GC_COORD_FRACTION);
	return result;
}


/*******************************************************************************
 * SINC function used in filter kernel generation.
 */

static GC_COORD_TYPE sinc_filter(GC_COORD_TYPE x, int radius)
{
	GC_COORD_TYPE result;
	s64 radius64;
	s64 pit, pitd;
	s64 normpit, normpitd;
	int negpit, negpitd;
	int quadpit, quadpitd;
	GC_COORD_TYPE sinpit, sinpitd;
	GC_COORD_TYPE f1, f2;

	if (x == GC_COORD_ZERO)
		return GC_COORD_ONE;

	radius64 = abs(radius) << GC_COORD_FRACTION;
	if (x > radius64)
		return GC_COORD_ZERO;

	pit  = (((s64) GC_COORD_PI) * x) >> GC_COORD_FRACTION;
	pitd = div_s64(pit, radius);

	/* Sine table only has values for the first positive quadrant,
	 * remove the sign here. */
	if (pit < 0) {
		normpit = -pit;
		negpit = 1;
	} else {
		normpit = pit;
		negpit = 0;
	}

	if (pitd < 0) {
		normpitd = -pitd;
		negpitd = 1;
	} else {
		normpitd = pitd;
		negpitd = 0;
	}

	/* Determine which quadrant we are in. */
	quadpit = (int) ((normpit * GC_COORD_2OVERPI)
		>> (2 * GC_COORD_FRACTION));
	quadpitd = (int) ((normpitd * GC_COORD_2OVERPI)
		>> (2 * GC_COORD_FRACTION));

	/* Move coordinates to the first quadrant. */
	normpit -= (s64) GC_COORD_PIOVER2 * quadpit;
	normpitd -= (s64) GC_COORD_PIOVER2 * quadpitd;

	/* Normalize the quadrant numbers. */
	quadpit %= 4;
	quadpitd %= 4;

	/* Flip the coordinates if necessary. */
	if ((quadpit == 1) || (quadpit == 3))
		normpit = GC_COORD_PIOVER2 - normpit;

	if ((quadpitd == 1) || (quadpitd == 3))
		normpitd = GC_COORD_PIOVER2 - normpitd;

	sinpit = sine(normpit);
	sinpitd = sine(normpitd);

	/* Negate depending on the quadrant. */
	if (negpit) {
		if ((quadpit == 0) || (quadpit == 1))
			sinpit = -sinpit;
	} else {
		if ((quadpit == 2) || (quadpit == 3))
			sinpit = -sinpit;
	}

	if (negpitd) {
		if ((quadpitd == 0) || (quadpitd == 1))
			sinpitd = -sinpitd;
	} else {
		if ((quadpitd == 2) || (quadpitd == 3))
			sinpitd = -sinpitd;
	}

	f1 = (GC_COORD_TYPE)
	     div64_s64(((s64) sinpit) << GC_COORD_FRACTION, pit);
	f2 = (GC_COORD_TYPE)
	     div64_s64(((s64) sinpitd) << GC_COORD_FRACTION, pitd);

	result = (GC_COORD_TYPE) ((((s64) f1) * f2)
	       >> GC_COORD_FRACTION);

	return result;
}


/*******************************************************************************
 * Filter kernel generator based on SINC function.
 */

static void calculate_sync_filter(struct gcfilterkernel *gcfilterkernel)
{
	GC_SCALE_TYPE scale;
	GC_COORD_TYPE subpixset[GC_TAP_COUNT];
	GC_COORD_TYPE subpixeloffset;
	GC_COORD_TYPE x, weight;
	GC_SUM_TYPE weightsum;
	short convweightsum;
	int kernelhalf, padding;
	int subpixpos, kernelpos;
	short *kernelarray;
	short count, adjustfrom, adjustment;
	int index;

	/* Compute the scale factor. */
	scale = (gcfilterkernel->dstsize >= gcfilterkernel->srcsize)
	      ? GC_SCALE_ONE
	      : computescale(gcfilterkernel->dstsize, gcfilterkernel->srcsize);

	/* Calculate the kernel half. */
	kernelhalf = (int) (gcfilterkernel->kernelsize >> 1);

	/* Init the subpixel offset. */
	subpixeloffset = GC_COORD_HALF;

	/* Determine kernel padding size. */
	padding = (GC_TAP_COUNT - gcfilterkernel->kernelsize) / 2;

	/* Set initial kernel array pointer. */
	kernelarray = gcfilterkernel->kernelarray;

	/* Loop through each subpixel. */
	for (subpixpos = 0; subpixpos < GC_PHASE_LOAD_COUNT; subpixpos += 1) {
		/* Compute weights. */
		weightsum = GC_COORD_ZERO;
		for (kernelpos = 0; kernelpos < GC_TAP_COUNT; kernelpos += 1) {
			/* Determine the current index. */
			index = kernelpos - padding;

			/* Pad with zeros left side. */
			if (index < 0) {
				subpixset[kernelpos] = GC_COORD_ZERO;
				continue;
			}

			/* Pad with zeros right side. */
			if (index >= (int) gcfilterkernel->kernelsize) {
				subpixset[kernelpos] = GC_COORD_ZERO;
				continue;
			}

			/* "Filter off" case. */
			if (gcfilterkernel->kernelsize == 1) {
				subpixset[kernelpos] = GC_COORD_ONE;

				/* Update the sum of the weights. */
				weightsum += GC_COORD_ONE;
				continue;
			}

			/* Compute X coordinate. */
			x = ((index - kernelhalf) << GC_COORD_FRACTION)
			  + subpixeloffset;

			/* Scale the coordinate. */
			x = (GC_COORD_TYPE)
			    ((((s64) x) * scale) >> GC_SCALE_FRACTION);

			/* Compute the weight. */
			subpixset[kernelpos] = sinc_filter(x, kernelhalf);

			/* Update the sum of the weights. */
			weightsum += subpixset[kernelpos];
		}

		/* Convert the weights to the hardware format. */
		convweightsum = 0;
		for (kernelpos = 0; kernelpos < GC_TAP_COUNT; kernelpos += 1) {
			/* Normalize the current weight. */
			weight = normweight(subpixset[kernelpos], weightsum);

			/* Convert the weight to fixed point. */
			if (weight == GC_COORD_ZERO)
				kernelarray[kernelpos] = GC_COEF_ZERO;
			else if (weight >= GC_COORD_ONE)
				kernelarray[kernelpos] = GC_COEF_ONE;
			else if (weight <= GC_COORD_NEGONE)
				kernelarray[kernelpos] = GC_COEF_NEGONE;
			else
				kernelarray[kernelpos] = convertweight(weight);

			/* Compute the sum of all coefficients. */
			convweightsum += kernelarray[kernelpos];
		}

		/* Adjust the fixed point coefficients so that the sum is 1. */
		count = GC_COEF_ONE - convweightsum;
		if (count < 0) {
			count = -count;
			adjustment = -1;
		} else {
			adjustment = 1;
		}

		if (count > GC_TAP_COUNT) {
			GCERR("adjust count is too high = %d\n", count);
		} else {
			adjustfrom = (GC_TAP_COUNT - count) / 2;
			for (kernelpos = 0; kernelpos < count; kernelpos += 1)
				kernelarray[adjustfrom + kernelpos]
					+= adjustment;
		}

		/* Advance the array pointer. */
		kernelarray += GC_TAP_COUNT;

		/* Advance to the next subpixel. */
		subpixeloffset -= GC_COORD_SUBPIX_STEP;
	}
}


/*******************************************************************************
 * Loads a filter into the GPU.
 */

static enum bverror load_filter(struct bvbltparams *bvbltparams,
				struct gcbatch *batch,
				enum gcfiltertype type,
				unsigned int kernelsize,
				unsigned int scalefactor,
				unsigned int srcsize,
				unsigned int dstsize,
				struct gccmdldstate arraystate)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();
	struct gcfiltercache *filtercache;
	struct list_head *filterlist;
	struct list_head *filterhead;
	struct gcfilterkernel *gcfilterkernel;
	struct gcmofilterkernel *gcmofilterkernel;

	GCDBG(GCZONE_KERNEL, "kernelsize = %d\n", kernelsize);
	GCDBG(GCZONE_KERNEL, "srcsize = %d\n", srcsize);
	GCDBG(GCZONE_KERNEL, "dstsize = %d\n", dstsize);

	/* Is the filter already loaded? */
	if ((gccontext->loadedfilter != NULL) &&
	    (gccontext->loadedfilter->type == type) &&
	    (gccontext->loadedfilter->kernelsize == kernelsize) &&
	    (gccontext->loadedfilter->scalefactor == scalefactor)) {
		GCDBG(GCZONE_KERNEL, "filter already computed.\n");
		gcfilterkernel = gccontext->loadedfilter;
		goto load;
	}

	/* Get the proper filter cache. */
	filtercache = &gccontext->filtercache[type][kernelsize];
	filterlist = &filtercache->list;

	/* Try to find existing filter. */
	GCDBG(GCZONE_KERNEL, "scanning for existing filter.\n");
	list_for_each(filterhead, filterlist) {
		gcfilterkernel = list_entry(filterhead,
					    struct gcfilterkernel,
					    link);
		if (gcfilterkernel->scalefactor == scalefactor) {
			GCDBG(GCZONE_KERNEL, "filter found @ 0x%08X.\n",
			      (unsigned int) gcfilterkernel);
			break;
		}
	}

	/* Found the filter? */
	if (filterhead != filterlist) {
		/* Move the filter to the head of the list. */
		if (filterlist->next != filterhead) {
			GCDBG(GCZONE_KERNEL, "moving to the head.\n");
			list_move(filterhead, filterlist);
		}
	} else {
		GCDBG(GCZONE_KERNEL, "filter not found.\n");
		if (filtercache->count == GC_FILTER_CACHE_MAX) {
			GCDBG(GCZONE_KERNEL,
			      "reached the maximum number of filters.\n");
			filterhead = filterlist->prev;
			list_move(filterhead, filterlist);

			gcfilterkernel = list_entry(filterhead,
						    struct gcfilterkernel,
						    link);
		} else {
			GCDBG(GCZONE_KERNEL, "allocating new filter.\n");
			gcfilterkernel = gcalloc(struct gcfilterkernel,
						 sizeof(struct gcfilterkernel));
			if (gcfilterkernel == NULL) {
				BVSETBLTERROR(BVERR_OOM,
					      "filter allocation failed");
				goto exit;
			}

			list_add(&gcfilterkernel->link, filterlist);
		}

		/* Update the number of filters. */
		filtercache->count += 1;

		/* Initialize the filter. */
		gcfilterkernel->type = type;
		gcfilterkernel->kernelsize = kernelsize;
		gcfilterkernel->srcsize = srcsize;
		gcfilterkernel->dstsize = dstsize;
		gcfilterkernel->scalefactor = scalefactor;

		/* Compute the coefficients. */
		calculate_sync_filter(gcfilterkernel);
	}

load:
	/* Load the filter. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmofilterkernel),
			       (void **) &gcmofilterkernel);
	if (bverror != BVERR_NONE)
		goto exit;

	gcmofilterkernel->kernelarray_ldst = arraystate;
	memcpy(&gcmofilterkernel->kernelarray,
	       gcfilterkernel->kernelarray,
	       sizeof(gcfilterkernel->kernelarray));

	/* Set the filter. */
	gccontext->loadedfilter = gcfilterkernel;

exit:
	return bverror;
}


/*******************************************************************************
 * Compute the scale factor.
 */

static inline unsigned int get_scale_factor(unsigned int srcsize,
					    unsigned int dstsize)
{
	if ((srcsize <= 1) || (dstsize <= 1))
		return 0;

	return ((srcsize - 1) << 16) / (dstsize - 1);
}


/*******************************************************************************
 * Rasterizer setup.
 */

static enum bverror startvr(struct bvbltparams *bvbltparams,
			    struct gcbatch *batch,
			    struct bvbuffmap *srcmap,
			    struct bvbuffmap *dstmap,
			    struct surfaceinfo *srcinfo,
			    struct surfaceinfo *dstinfo,
			    unsigned int srcx,
			    unsigned int srcy,
			    struct gcrect *dstrect,
			    struct gcregvrconfig config,
			    bool prepass)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gcalpha *gca;

	struct gcmovrdst *gcmovrdst;
	struct gcmovrsrc *gcmovrsrc;
	struct gcmoalphaoff *gcmoalphaoff;
	struct gcmoalpha *gcmoalpha;
	struct gcmoglobal *gcmoglobal;
	struct gcmostartvr *gcmostartvr;

	struct gcrect srcrect;
	int srcpixalign, srcbytealign;
	int srcsurfwidth, srcsurfheight;

	GCENTER(GCZONE_FILTER);

	/***********************************************************************
	 * Program the destination.
	 */

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmovrdst),
			       (void **) &gcmovrdst);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Add the address fixup. */
	add_fixup(bvbltparams, batch, &gcmovrdst->address, dstinfo->bytealign);

	/* Set surface parameters. */
	gcmovrdst->config_ldst = gcmovrdst_config_ldst;
	gcmovrdst->address = GET_MAP_HANDLE(dstmap);
	gcmovrdst->stride = dstinfo->geom->virtstride;
	gcmovrdst->config.raw = 0;
	gcmovrdst->config.reg.swizzle = dstinfo->format.swizzle;
	gcmovrdst->config.reg.format = dstinfo->format.format;

	/* Set surface width and height. */
	gcmovrdst->rotation.raw = 0;
	gcmovrdst->rotation.reg.surf_width = dstinfo->physwidth;
	gcmovrdst->rotationheight_ldst = gcmovrdst_rotationheight_ldst;
	gcmovrdst->rotationheight.raw = 0;
	gcmovrdst->rotationheight.reg.height = dstinfo->physheight;

	/***********************************************************************
	 * Program the source.
	 */

	/* Initialize the source rectangle. */
	srcrect.left = srcinfo->rect.left;
	srcrect.top = srcinfo->rect.top;
	srcrect.right = srcinfo->rect.right;
	srcrect.bottom = srcinfo->rect.bottom;

	/* Compute the source alignments needed to compensate
	 * for the surface base address misalignment if any. */
	srcpixalign = get_pixel_offset(srcinfo, 0);
	srcbytealign = (srcpixalign * (int) srcinfo->format.bitspp) / 8;

	switch (srcinfo->angle) {
	case ROT_ANGLE_0:
		/* Adjust the source rectangle. */
		srcrect.left -= srcpixalign;
		srcrect.right -= srcpixalign;
		srcx -= (srcpixalign << 16);

		/* Determine source size. */
		srcsurfwidth = srcinfo->geom->width - srcpixalign;
		srcsurfheight = srcinfo->geom->height;
		break;

	case ROT_ANGLE_90:
		/* Adjust the source rectangle. */
		srcrect.top -= srcpixalign;
		srcrect.bottom -= srcpixalign;
		srcy -= (srcpixalign << 16);

		/* Determine source size. */
		srcsurfwidth = srcinfo->geom->height - srcpixalign;
		srcsurfheight = srcinfo->geom->width;
		break;

	case ROT_ANGLE_180:
		/* Determine source size. */
		srcsurfwidth = srcinfo->geom->width - srcpixalign;
		srcsurfheight = srcinfo->geom->height;
		break;

	case ROT_ANGLE_270:
		/* Determine source size. */
		srcsurfwidth = srcinfo->geom->height - srcpixalign;
		srcsurfheight = srcinfo->geom->width;
		break;

	default:
		srcsurfwidth = 0;
		srcsurfheight = 0;
	}

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmovrsrc),
			       (void **) &gcmovrsrc);
	if (bverror != BVERR_NONE)
		goto exit;

	add_fixup(bvbltparams, batch, &gcmovrsrc->address, srcbytealign);

	gcmovrsrc->config_ldst = gcmovrsrc_config_ldst;

	gcmovrsrc->address = GET_MAP_HANDLE(srcmap);
	gcmovrsrc->stride = srcinfo->geom->virtstride;

	gcmovrsrc->rotation.raw = 0;
	gcmovrsrc->rotation.reg.surf_width = srcsurfwidth;

	gcmovrsrc->config.raw = 0;
	gcmovrsrc->config.reg.swizzle = srcinfo->format.swizzle;
	gcmovrsrc->config.reg.format = srcinfo->format.format;

	if (gccontext->gcfeatures2.reg.l2cachefor420 &&
	    (srcinfo->format.type == BVFMT_YUV) &&
	    (srcinfo->format.cs.yuv.planecount > 1) &&
	    ((srcinfo->angle & 1) != 0))
		gcmovrsrc->config.reg.disable420L2cache
			= GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_DISABLED;

	gcmovrsrc->pos_ldst = gcmovrsrc_pos_ldst;

	/* Source image bounding box. */
	gcmovrsrc->lt.reg.left = srcrect.left;
	gcmovrsrc->lt.reg.top = srcrect.top;
	gcmovrsrc->rb.reg.right = srcrect.right;
	gcmovrsrc->rb.reg.bottom = srcrect.bottom;

	/* Fractional origin. */
	gcmovrsrc->x = srcx;
	gcmovrsrc->y = srcy;

	/* Program rotation. */
	gcmovrsrc->rotation_ldst = gcmovrsrc_rotation_ldst;
	gcmovrsrc->rotationheight.reg.height = srcsurfheight;
	gcmovrsrc->rotationangle.raw = 0;
	gcmovrsrc->rotationangle.reg.src = rotencoding[srcinfo->angle];
	gcmovrsrc->rotationangle.reg.dst = rotencoding[dstinfo->angle];

	if (prepass) {
		gcmovrsrc->rotationangle.reg.src_mirror = GCREG_MIRROR_NONE;
		gcmovrsrc->rotationangle.reg.dst_mirror = GCREG_MIRROR_NONE;
	} else {
		gcmovrsrc->rotationangle.reg.src_mirror = srcinfo->mirror;
		gcmovrsrc->rotationangle.reg.dst_mirror = dstinfo->mirror;
	}

	gcmovrsrc->rop_ldst = gcmovrsrc_rop_ldst;
	gcmovrsrc->rop.raw = 0;
	gcmovrsrc->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmovrsrc->rop.reg.fg = 0xCC;

	/* Program multiply modes. */
	gcmovrsrc->mult_ldst = gcmovrsrc_mult_ldst;
	gcmovrsrc->mult.raw = 0;
	gcmovrsrc->mult.reg.srcglobalpremul
	= GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE;

	if ((srcinfo->geom->format & OCDFMTDEF_NON_PREMULT) != 0)
		gcmovrsrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;
	else
		gcmovrsrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;

	if ((dstinfo->geom->format & OCDFMTDEF_NON_PREMULT) != 0) {
		gcmovrsrc->mult.reg.dstpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;

		gcmovrsrc->mult.reg.dstdemul
		= GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE;
	} else {
		gcmovrsrc->mult.reg.dstpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;

		gcmovrsrc->mult.reg.dstdemul
		= GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE;
	}

	if (srcinfo->format.format == GCREG_DE_FORMAT_NV12) {
		struct gcmoxsrcyuv *gcmoxsrcyuv;
		int index = 0;
		int uvshift;

		uvshift = srcbytealign
			+ srcinfo->geom->virtstride
			* srcinfo->geom->height;
		GCDBG(GCZONE_FILTER, "  uvshift = 0x%08X (%d)\n",
			uvshift, uvshift);

		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoxsrcyuv),
				       (void **) &gcmoxsrcyuv);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoxsrcyuv->uplaneaddress_ldst =
			gcmoxsrcyuv_uplaneaddress_ldst[index];
		gcmoxsrcyuv->uplaneaddress = GET_MAP_HANDLE(srcmap);
		add_fixup(bvbltparams, batch, &gcmoxsrcyuv->uplaneaddress,
			  uvshift);

		gcmoxsrcyuv->uplanestride_ldst =
			gcmoxsrcyuv_uplanestride_ldst[index];
		gcmoxsrcyuv->uplanestride = srcinfo->geom->virtstride;

		gcmoxsrcyuv->vplaneaddress_ldst =
			gcmoxsrcyuv_vplaneaddress_ldst[index];
		gcmoxsrcyuv->vplaneaddress = GET_MAP_HANDLE(srcmap);
		add_fixup(bvbltparams, batch, &gcmoxsrcyuv->vplaneaddress,
			  uvshift);

		gcmoxsrcyuv->vplanestride_ldst =
			gcmoxsrcyuv_vplanestride_ldst[index];
		gcmoxsrcyuv->vplanestride = srcinfo->geom->virtstride;

		gcmoxsrcyuv->pectrl_ldst =
			gcmoxsrcyuv_pectrl_ldst[index];
		gcmoxsrcyuv->pectrl = GCREG_PE_CONTROL_ResetValue;
	}

	/***********************************************************************
	 * Program blending.
	 */

	gca = srcinfo->gca;
	if (prepass || (gca == NULL)) {
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoalphaoff),
				       (void **) &gcmoalphaoff);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoalphaoff->control_ldst = gcmoalphaoff_control_ldst;
		gcmoalphaoff->control.reg = gcregalpha_off;

		GCDBG(GCZONE_BLEND, "blending disabled.\n");
	} else {
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoalpha),
				       (void **) &gcmoalpha);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoalpha->config_ldst = gcmoalpha_config_ldst;
		gcmoalpha->control.reg = gcregalpha_on;

		gcmoalpha->mode.raw = 0;
		gcmoalpha->mode.reg.src_global_alpha_mode
			= gca->src_global_alpha_mode;
		gcmoalpha->mode.reg.dst_global_alpha_mode
			= gca->dst_global_alpha_mode;

		gcmoalpha->mode.reg.src_blend
			= gca->srcconfig->factor_mode;
		gcmoalpha->mode.reg.src_color_reverse
			= gca->srcconfig->color_reverse;

		gcmoalpha->mode.reg.dst_blend
			= gca->dstconfig->factor_mode;
		gcmoalpha->mode.reg.dst_color_reverse
			= gca->dstconfig->color_reverse;

		GCDBG(GCZONE_BLEND, "dst blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmoalpha->mode.reg.dst_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmoalpha->mode.reg.dst_color_reverse);

		GCDBG(GCZONE_BLEND, "src blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmoalpha->mode.reg.src_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmoalpha->mode.reg.src_color_reverse);

		if ((gca->src_global_alpha_mode
			!= GCREG_GLOBAL_ALPHA_MODE_NORMAL) ||
		    (gca->dst_global_alpha_mode
			!= GCREG_GLOBAL_ALPHA_MODE_NORMAL)) {
			bverror = claim_buffer(bvbltparams, batch,
					       sizeof(struct gcmoglobal),
					       (void **) &gcmoglobal);
			if (bverror != BVERR_NONE)
				goto exit;

			gcmoglobal->color_ldst = gcmoglobal_color_ldst;
			gcmoglobal->srcglobal.raw = gca->src_global_color;
			gcmoglobal->dstglobal.raw = gca->dst_global_color;
		}
	}


	/***********************************************************************
	 * Start the operation.
	 */

	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmostartvr),
			       (void **) &gcmostartvr);
	if (bverror != BVERR_NONE)
		goto exit;

	gcmostartvr->scale_ldst = gcmostartvr_scale_ldst;
	gcmostartvr->scalex = batch->op.filter.horscalefactor;
	gcmostartvr->scaley = batch->op.filter.verscalefactor;

	gcmostartvr->rect_ldst = gcmostartvr_rect_ldst;
	gcmostartvr->lt.left = dstrect->left;
	gcmostartvr->lt.top = dstrect->top;
	gcmostartvr->rb.right = dstrect->right;
	gcmostartvr->rb.bottom = dstrect->bottom;

	gcmostartvr->config_ldst = gcmostartvr_config_ldst;
	gcmostartvr->config = config;

exit:
	GCEXITARG(GCZONE_FILTER, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}


/*******************************************************************************
 * Main fiter entry.
 */

enum bverror do_filter(struct bvbltparams *bvbltparams,
		       struct gcbatch *batch,
		       struct surfaceinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();

	struct surfaceinfo *dstinfo;
	struct bvrect *dstrect;
	struct gcrect *dstrectclipped;
	struct gcrect dstadjusted;
	int dstleftoffs, dsttopoffs, dstrightoffs;
	int srcleftoffs, srctopoffs, srcrightoffs;

	struct bvbuffmap *srcmap = NULL;
	struct bvbuffmap *tmpmap = NULL;
	struct bvbuffmap *dstmap = NULL;

	struct gcmovrconfigex *gcmovrconfigex;

	unsigned int srcx, srcy;
	unsigned int srcwidth, srcheight;
	unsigned int horscalefactor, verscalefactor;
	unsigned int kernelsize;
	int horpass, verpass;

	GCENTER(GCZONE_FILTER);

	/* Finish previous batch if any. */
	bverror = batch->batchend(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* ROP is not supported by the filters. */
	if ((srcinfo->rop & 0xFF) != 0xCC) {
		BVSETBLTERROR(BVERR_ROP,
			      "only copy ROP is supported in scaling mode");
		goto exit;
	}

	/* Parse the scale mode. */
	bverror = parse_scalemode(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Additional stride requirements. */
	if (srcinfo->format.format == GCREG_DE_FORMAT_NV12) {
		/* Nv12 may be shifted up to 32 bytes for alignment.
		 * In the worst case stride must be 32 bytes greater.
		 */
		int min_stride = srcinfo->geom->width + 32;

		if (srcinfo->geom->virtstride < min_stride) {
			BVSETBLTERROR((srcinfo->index == 0)
						  ? BVERR_SRC1GEOM_STRIDE
						  : BVERR_SRC2GEOM_STRIDE,
						  "nv12 source stride too small (%ld < %d)\n",
						  srcinfo->geom->virtstride,
						  min_stride);
			goto exit;
		}
	}

	/* Determine the destination rectangle. */
	if ((srcinfo->index == 1) &&
	   ((bvbltparams->flags & BVFLAG_SRC2_AUXDSTRECT) != 0)) {
		dstrect = &bvbltparams->src2auxdstrect;
		dstrectclipped = &batch->dstclippedaux;
	} else {
		dstrect = &bvbltparams->dstrect;
		dstrectclipped = &batch->dstclipped;
	}

	/* Get a shortcut to the destination surface. */
	dstinfo = &batch->dstinfo;

	/* Compute the size of the source rectangle. */
	srcwidth = srcinfo->rect.right - srcinfo->rect.left;
	srcheight = srcinfo->rect.bottom - srcinfo->rect.top;

	/* Compute the scale factors. */
	batch->op.filter.horscalefactor =
	horscalefactor = get_scale_factor(srcwidth, dstrect->width);

	batch->op.filter.verscalefactor =
	verscalefactor = get_scale_factor(srcheight, dstrect->height);

	/* Compute adjusted destination rectangle. */
	dstadjusted.left = dstrectclipped->left + batch->dstoffsetX;
	dstadjusted.top = dstrectclipped->top + batch->dstoffsetY;
	dstadjusted.right = dstrectclipped->right + batch->dstoffsetX;
	dstadjusted.bottom = dstrectclipped->bottom + batch->dstoffsetY;

	/* Compute the destination offsets. */
	dstleftoffs = dstrectclipped->left - dstrect->left;
	dsttopoffs = dstrectclipped->top - dstrect->top;
	dstrightoffs = dstrectclipped->right - dstrect->left;

	/* Compute the source offsets. */
	srcleftoffs = dstleftoffs * horscalefactor;
	srctopoffs = dsttopoffs * verscalefactor;
	srcrightoffs = (dstrightoffs - 1) * verscalefactor + (1 << 16);

	/* Before rendering each destination pixel, the HW will select the
	 * corresponding source center pixel to apply the kernel around.
	 * To make this process precise we need to add 0.5 to source initial
	 * coordinates here; this will make HW pick the next source pixel if
	 * the fraction is equal or greater then 0.5. */
	srcleftoffs += 0x00008000;
	srctopoffs += 0x00008000;
	srcrightoffs += 0x00008000;

	GCDBG(GCZONE_FILTER, "source rectangle:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom = %dx%d\n",
	      srcinfo->geom->virtstride,
	      srcinfo->geom->width, srcinfo->geom->height);
	GCDBG(GCZONE_FILTER, "  rotation = %d\n",
	      srcinfo->angle);
	GCDBG(GCZONE_FILTER, "  rect offsets = "
	      "(0x%08X,0x%08X)-(0x%08X,---)\n",
	      srcleftoffs, srctopoffs, srcrightoffs);

	GCDBG(GCZONE_FILTER, "destination rectangle:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom size = %dx%d\n",
	      bvbltparams->dstgeom->virtstride,
	      bvbltparams->dstgeom->width, bvbltparams->dstgeom->height);
	GCDBG(GCZONE_FILTER, "  rotaton = %d\n",
	      dstinfo->angle);
	GCDBG(GCZONE_FILTER, "  rect = (%d,%d)-(%d,%d)\n",
	      dstrectclipped->left, dstrectclipped->top,
	      dstrectclipped->right, dstrectclipped->bottom);

	/* Map the source. */
	bverror = do_map(srcinfo->buf.desc, batch, &srcmap);
	if (bverror != BVERR_NONE) {
		bvbltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/* Map the destination. */
	bverror = do_map(dstinfo->buf.desc, batch, &dstmap);
	if (bverror != BVERR_NONE) {
		bvbltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/* Determine needed passes. */
	horpass = (srcwidth != dstrect->width);
	verpass = (srcheight != dstrect->height);

	/* Do single pass filter if we can. */
	if (((batch->op.filter.horkernelsize == 3) ||
	     (batch->op.filter.horkernelsize == 5)) &&
	    ((batch->op.filter.verkernelsize == 3) ||
	     (batch->op.filter.verkernelsize == 5))) {
		/* Determine the kernel size to use. */
		kernelsize = max(batch->op.filter.horkernelsize,
				 batch->op.filter.verkernelsize);

		/* Set kernel size. */
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmovrconfigex),
				       (void **) &gcmovrconfigex);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmovrconfigex->config_ldst = gcmovrconfigex_config_ldst;
		gcmovrconfigex->config.raw = ~0U;
		gcmovrconfigex->config.reg.kernelsize = kernelsize;
		gcmovrconfigex->config.reg.mask_kernelsize
			= GCREG_VR_CONFIG_EX_MASK_FILTER_TAP_ENABLED;

		/* Setup single pass. */
		srcx = (srcinfo->rect.left << 16) + srcleftoffs;
		srcy = (srcinfo->rect.top  << 16) + srctopoffs;

		/* Load the horizontal filter. */
		bverror = load_filter(bvbltparams, batch, GC_FILTER_SYNC,
				      batch->op.filter.horkernelsize,
				      batch->op.filter.horscalefactor,
				      srcwidth, dstrect->width,
				      gcmofilterkernel_horizontal_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Load the vertical filter. */
		bverror = load_filter(bvbltparams, batch, GC_FILTER_SYNC,
				      batch->op.filter.verkernelsize,
				      batch->op.filter.verscalefactor,
				      srcheight, dstrect->height,
				      gcmofilterkernel_vertical_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		bverror = startvr(bvbltparams, batch,
				  srcmap, dstmap, srcinfo, dstinfo,
				  srcx, srcy, &dstadjusted,
				  gcregvrconfig_onepass, false);

		GCDBG(GCZONE_TYPE, "single pass\n");
	} else if (horpass && verpass) {
		unsigned int horkernelhalf;
		unsigned int leftextra, rightextra;
		unsigned int tmprectwidth, tmprectheight;
		unsigned int tmpalignmask;
		unsigned int tmpsize;
		struct surfaceinfo tmpinfo;
		struct bvsurfgeom tmpgeom;

		/* Initialize the temporaty surface geometry. */
		tmpgeom.structsize = sizeof(struct bvsurfgeom);
		tmpgeom.orientation = 0;
		tmpgeom.paletteformat = 0;
		tmpgeom.palette = NULL;

		/* Initialize the temporaty surface descriptor. */
		tmpinfo.index = -1;
		tmpinfo.geom = &tmpgeom;
		tmpinfo.pixalign = 0;
		tmpinfo.bytealign = 0;
		tmpinfo.angle = srcinfo->angle;
		tmpinfo.mirror = srcinfo->mirror;
		tmpinfo.rop = srcinfo->rop;
		tmpinfo.gca = srcinfo->gca;

		/* Determine temporary surface format. */
		if (srcinfo->format.type == BVFMT_YUV) {
			tmpinfo.format = dstinfo->format;
			tmpgeom.format = dstinfo->geom->format;
		} else {
			tmpinfo.format = srcinfo->format;
			tmpgeom.format = srcinfo->geom->format;
		}

		/* Determine pixel alignment. */
		tmpalignmask = GC_BITS_PER_CACHELINE
			     / tmpinfo.format.bitspp - 1;

		/* In partial filter blit cases, the vertical pass has to render
		 * more pixel information to the left and to the right of the
		 * temporary image so that the horizontal pass has its necessary
		 * kernel information on the edges of the image. */
		horkernelhalf = batch->op.filter.horkernelsize >> 1;

		leftextra  = srcleftoffs >> 16;
		rightextra = srcwidth - (srcrightoffs >> 16);

		if (leftextra > horkernelhalf)
			leftextra = horkernelhalf;

		if (rightextra > horkernelhalf)
			rightextra = horkernelhalf;

		/* Determine the source origin. */
		srcx = ((srcinfo->rect.left - leftextra) << 16) + srcleftoffs;
		srcy = (srcinfo->rect.top << 16) + srctopoffs;

		/* Determine the size of the temporary image. */
		tmprectwidth = leftextra + rightextra
			     + ((srcrightoffs >> 16) - (srcleftoffs >> 16));
		tmprectheight = dstadjusted.bottom - dstadjusted.top;

		/* Determine the destination coordinates. */
		tmpinfo.rect.left = (srcx >> 16) & tmpalignmask;
		tmpinfo.rect.top = dstadjusted.top;
		tmpinfo.rect.right = tmpinfo.rect.left + tmprectwidth;
		tmpinfo.rect.bottom = tmpinfo.rect.top + tmprectheight;

		/* Determine the size of the temporaty surface. */
		tmpgeom.width = (tmpinfo.rect.right + tmpalignmask)
			      & ~tmpalignmask;
		tmpgeom.height = tmpinfo.rect.bottom;
		tmpgeom.virtstride = (tmpgeom.width
				   * tmpinfo.format.bitspp) / 8;
		tmpsize = tmpgeom.virtstride * tmpgeom.height;

		/* Allocate the temporary buffer. */
		bverror = allocate_temp(bvbltparams, tmpsize);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Map the temporary buffer. */
		tmpinfo.buf.desc = gccontext->tmpbuffdesc;
		bverror = do_map(tmpinfo.buf.desc, batch, &tmpmap);
		if (bverror != BVERR_NONE) {
			bvbltparams->errdesc = gccontext->bverrorstr;
			goto exit;
		}

		/* Set the temporaty surface size. */
		tmpinfo.physwidth = tmpgeom.width;
		tmpinfo.physheight = tmpgeom.height;

		/* Load the vertical filter. */
		bverror = load_filter(bvbltparams, batch, GC_FILTER_SYNC,
				      batch->op.filter.verkernelsize,
				      batch->op.filter.verscalefactor,
				      srcheight, dstrect->height,
				      gcmofilterkernel_shared_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		bverror = startvr(bvbltparams, batch,
				  srcmap, tmpmap, srcinfo, &tmpinfo,
				  srcx, srcy, &tmpinfo.rect,
				  gcregvrconfig_vertical, true);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Determine the source origin. */
		srcx = ((leftextra + tmpinfo.rect.left) << 16)
		     + (srcleftoffs & 0xFFFF);
		srcy = (tmpinfo.rect.top << 16)
		     + (srctopoffs & 0xFFFF);

		/* Load the horizontal filter. */
		bverror = load_filter(bvbltparams, batch, GC_FILTER_SYNC,
				      batch->op.filter.horkernelsize,
				      batch->op.filter.horscalefactor,
				      srcwidth, dstrect->width,
				      gcmofilterkernel_shared_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		bverror = startvr(bvbltparams, batch,
				  tmpmap, dstmap, &tmpinfo, dstinfo,
				  srcx, srcy, &dstadjusted,
				  gcregvrconfig_horizontal, false);
		if (bverror != BVERR_NONE)
			goto exit;

		GCDBG(GCZONE_TYPE, "two pass\n");
	} else {
		/* Setup single pass. */
		srcx = (srcinfo->rect.left << 16) + srcleftoffs;
		srcy = (srcinfo->rect.top  << 16) + srctopoffs;

		if (verpass)
			/* Load the vertical filter. */
			bverror = load_filter(bvbltparams, batch,
					      GC_FILTER_SYNC,
					      batch->op.filter.verkernelsize,
					      batch->op.filter.verscalefactor,
					      srcheight, dstrect->height,
					      gcmofilterkernel_shared_ldst);
		else
			/* Load the horizontal filter. */
			bverror = load_filter(bvbltparams, batch,
					      GC_FILTER_SYNC,
					      batch->op.filter.horkernelsize,
					      batch->op.filter.horscalefactor,
					      srcwidth, dstrect->width,
					      gcmofilterkernel_shared_ldst);

		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		bverror = startvr(bvbltparams, batch,
				  srcmap, dstmap, srcinfo, dstinfo,
				  srcx, srcy, &dstadjusted,
				  verpass
					? gcregvrconfig_vertical
					: gcregvrconfig_horizontal,
				  false);

		GCDBG(GCZONE_TYPE, "two pass one pass config.\n");
	}

exit:
	GCEXITARG(GCZONE_FILTER, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}
