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
#define GCZONE_SRC		(1 << 4)
#define GCZONE_DEST		(1 << 5)
#define GCZONE_SURF		(1 << 6)

GCDBG_FILTERDEF(filter, GCZONE_NONE,
		"kernel",
		"filter",
		"blend",
		"type",
		"src",
		"dest",
		"surf")


/*******************************************************************************
 * Miscellaneous defines.
 */

#define GC_BYTES_PER_CACHELINE	(64)
#define GC_BITS_PER_CACHELINE	(GC_BYTES_PER_CACHELINE * 8)
#define GC_CACHELINE_ALIGN_16	(GC_BITS_PER_CACHELINE / 16 - 1)
#define GC_CACHELINE_ALIGN_32	(GC_BITS_PER_CACHELINE / 32 - 1)

enum gcscaletype {
	GC_SCALE_OPF,
	GC_SCALE_HOR,
	GC_SCALE_VER,
	GC_SCALE_HOR_FLIPPED,
	GC_SCALE_VER_FLIPPED
};

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

	enum {
		indexwidth = 8,
		intwidth = 1,
		indexshift = intwidth
			   + GC_COORD_FRACTION
			   - indexwidth
	};

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

	sinpit = sine((GC_COORD_TYPE) normpit);
	sinpitd = sine((GC_COORD_TYPE) normpitd);

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
	GCDBG(GCZONE_KERNEL, "scalefactor = 0x%08X\n", scalefactor);

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
	GCDBG(GCZONE_KERNEL, "loading filter.\n");

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
			    struct gcsurface *srcinfo,
			    struct gcsurface *dstinfo,
			    unsigned int srcx,
			    unsigned int srcy,
			    struct gcrect *dstrect,
			    int srcangle,
			    int dstangle,
			    enum gcscaletype scaletype)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gcfilter *gcfilter;

	struct gcmovrdst *gcmovrdst;
	struct gcmovrsrc *gcmovrsrc;
	struct gcmostartvr *gcmostartvr;

	struct gcrect srcorig;

	GCENTERARG(GCZONE_FILTER, "scaletype = %d\n", scaletype);

	/* Get a shortcut to the filter properties. */
	gcfilter = &batch->op.filter;

	/***********************************************************************
	 * Program the destination.
	 */

	GCDBG(GCZONE_FILTER, "destination:\n");
	GCDBG(GCZONE_FILTER, "  angle = %d\n", dstangle);
	GCDBG(GCZONE_FILTER, "  pixalign = %d,%d\n",
	      dstinfo->xpixalign, dstinfo->ypixalign);
	GCDBG(GCZONE_FILTER, "  bytealign = %d\n", dstinfo->bytealign1);
	GCDBG(GCZONE_FILTER, "  virtstride = %d\n", dstinfo->stride1);
	GCDBG(GCZONE_FILTER, "  format = %d\n", dstinfo->format.format);
	GCDBG(GCZONE_FILTER, "  swizzle = %d\n", dstinfo->format.swizzle);
	GCDBG(GCZONE_FILTER, "  endian = %d\n", dstinfo->format.endian);
	GCDBG(GCZONE_FILTER, "  premul = %d\n", dstinfo->format.premultiplied);
	GCDBG(GCZONE_FILTER, "  physwidth = %d\n", dstinfo->physwidth);
	GCDBG(GCZONE_FILTER, "  physheight = %d\n", dstinfo->physheight);
	GCPRINT_RECT(GCZONE_FILTER, "  rect", dstrect);

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmovrdst),
			       (void **) &gcmovrdst);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Add the address fixup. */
	add_fixup(bvbltparams, batch, &gcmovrdst->address, dstinfo->bytealign1);

	/* Set surface parameters. */
	gcmovrdst->config_ldst = gcmovrdst_config_ldst;
	gcmovrdst->address = GET_MAP_HANDLE(dstmap);
	gcmovrdst->stride = dstinfo->stride1;
	gcmovrdst->config.raw = 0;
	gcmovrdst->config.reg.swizzle = dstinfo->format.swizzle;
	gcmovrdst->config.reg.format = dstinfo->format.format;
	gcmovrdst->config.reg.endian = dstinfo->format.endian;

	/* Set surface width and height. */
	gcmovrdst->rotation.raw = 0;
	gcmovrdst->rotation.reg.surf_width = dstinfo->physwidth;
	gcmovrdst->rotationheight_ldst = gcmovrdst_rotationheight_ldst;
	gcmovrdst->rotationheight.raw = 0;
	gcmovrdst->rotationheight.reg.height = dstinfo->physheight;

	/***********************************************************************
	 * Program the source.
	 */

	/* Determine adjusted source bounding rectangle and origin. */
	srcorig = srcinfo->rect.orig;
	srcorig.left  -=  srcinfo->xpixalign;
	srcorig.right -=  srcinfo->xpixalign;
	srcx          -= (srcinfo->xpixalign << 16);

	GCDBG(GCZONE_FILTER, "source:\n");
	GCDBG(GCZONE_FILTER, "  angle = %d\n", srcangle);
	GCDBG(GCZONE_FILTER, "  pixalign = %d,%d\n",
	      srcinfo->xpixalign, srcinfo->ypixalign);
	GCDBG(GCZONE_FILTER, "  bytealign = %d\n", srcinfo->bytealign1);
	GCDBG(GCZONE_FILTER, "  virtstride = %d\n", srcinfo->stride1);
	GCDBG(GCZONE_FILTER, "  format = %d\n", srcinfo->format.format);
	GCDBG(GCZONE_FILTER, "  swizzle = %d\n", srcinfo->format.swizzle);
	GCDBG(GCZONE_FILTER, "  endian = %d\n", srcinfo->format.endian);
	GCDBG(GCZONE_FILTER, "  premul = %d\n", srcinfo->format.premultiplied);
	GCDBG(GCZONE_FILTER, "  physwidth = %d\n", srcinfo->physwidth);
	GCDBG(GCZONE_FILTER, "  physheight = %d\n", srcinfo->physheight);
	GCPRINT_RECT(GCZONE_FILTER, "  rect", &srcorig);

	GCDBG(GCZONE_FILTER, "src origin: 0x%08X,0x%08X\n", srcx, srcy);

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmovrsrc),
			       (void **) &gcmovrsrc);
	if (bverror != BVERR_NONE)
		goto exit;

	add_fixup(bvbltparams, batch, &gcmovrsrc->address, srcinfo->bytealign1);

	gcmovrsrc->config_ldst = gcmovrsrc_config_ldst;

	gcmovrsrc->address = GET_MAP_HANDLE(srcmap);
	gcmovrsrc->stride = srcinfo->stride1;

	gcmovrsrc->rotation.raw = 0;
	gcmovrsrc->rotation.reg.surf_width = srcinfo->physwidth;

	gcmovrsrc->config.raw = 0;
	gcmovrsrc->config.reg.swizzle = srcinfo->format.swizzle;
	gcmovrsrc->config.reg.format = srcinfo->format.format;
	gcmovrsrc->config.reg.endian = srcinfo->format.endian;

	if (gccontext->gccaps.l2cachefor420 &&
	    (srcinfo->format.type == BVFMT_YUV) &&
	    (srcinfo->format.cs.yuv.planecount > 1) &&
	    ((srcinfo->angle & 1) != 0))
		gcmovrsrc->config.reg.disable420L2cache
			= GCREG_SRC_CONFIG_DISABLE420_L2_CACHE_DISABLED;

	gcmovrsrc->pos_ldst = gcmovrsrc_pos_ldst;

	/* Source image bounding box. */
	gcmovrsrc->lt.reg.left = srcorig.left;
	gcmovrsrc->lt.reg.top = srcorig.top;
	gcmovrsrc->rb.reg.right = srcorig.right;
	gcmovrsrc->rb.reg.bottom = srcorig.bottom;

	/* Fractional origin. */
	gcmovrsrc->x = srcx;
	gcmovrsrc->y = srcy;

	/* Program rotation. */
	gcmovrsrc->rotation_ldst = gcmovrsrc_rotation_ldst;
	gcmovrsrc->rotationheight.reg.height = srcinfo->physheight;
	gcmovrsrc->rotationangle.raw = 0;
	gcmovrsrc->rotationangle.reg.src = rotencoding[srcangle];
	gcmovrsrc->rotationangle.reg.dst = rotencoding[dstangle];
	gcmovrsrc->rotationangle.reg.src_mirror = srcinfo->mirror;
	gcmovrsrc->rotationangle.reg.dst_mirror = dstinfo->mirror;

	gcmovrsrc->rop_ldst = gcmovrsrc_rop_ldst;
	gcmovrsrc->rop.raw = 0;
	gcmovrsrc->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmovrsrc->rop.reg.fg = 0xCC;

	/* Program multiply modes. */
	gcmovrsrc->mult_ldst = gcmovrsrc_mult_ldst;
	gcmovrsrc->mult.raw = 0;
	gcmovrsrc->mult.reg.srcglobalpremul = srcinfo->srcglobalpremul;

	if (srcinfo->format.premultiplied)
		gcmovrsrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;
	else
		gcmovrsrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;

	if (dstinfo->format.premultiplied) {
		gcmovrsrc->mult.reg.dstpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;

		gcmovrsrc->mult.reg.dstdemul
		= GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE;
	} else {
		gcmovrsrc->mult.reg.dstpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;

		gcmovrsrc->mult.reg.dstdemul
		= GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE;
	}

	/* Program YUV source. */
	if (srcinfo->format.type == BVFMT_YUV) {
		bverror = set_yuvsrc(bvbltparams, batch, srcinfo, srcmap);
		if (bverror != BVERR_NONE)
			goto exit;
	}

	/***********************************************************************
	 * Program blending.
	 */

	bverror = set_blending(bvbltparams, batch, srcinfo);
	if (bverror != BVERR_NONE)
		goto exit;

	/***********************************************************************
	 * Start the operation.
	 */

	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmostartvr),
			       (void **) &gcmostartvr);
	if (bverror != BVERR_NONE)
		goto exit;

	switch (scaletype) {
	case GC_SCALE_OPF:
		gcmostartvr->scalex = gcfilter->horscalefactor;
		gcmostartvr->scaley = gcfilter->verscalefactor;
		gcmostartvr->config = gcregvrconfig_onepass;
		break;

	case GC_SCALE_HOR:
		gcmostartvr->scalex = gcfilter->horscalefactor;
		gcmostartvr->scaley = 0;
		gcmostartvr->config = gcregvrconfig_horizontal;
		break;

	case GC_SCALE_VER:
		gcmostartvr->scalex = 0;
		gcmostartvr->scaley = gcfilter->verscalefactor;
		gcmostartvr->config = gcregvrconfig_vertical;
		break;

	case GC_SCALE_HOR_FLIPPED:
		gcmostartvr->scalex = 0;
		gcmostartvr->scaley = gcfilter->horscalefactor;
		gcmostartvr->config = gcregvrconfig_vertical;
		break;

	case GC_SCALE_VER_FLIPPED:
		gcmostartvr->scalex = gcfilter->verscalefactor;
		gcmostartvr->scaley = 0;
		gcmostartvr->config = gcregvrconfig_horizontal;
		break;
	}

	gcmostartvr->scale_ldst = gcmostartvr_scale_ldst;
	gcmostartvr->rect_ldst = gcmostartvr_rect_ldst;
	gcmostartvr->config_ldst = gcmostartvr_config_ldst;

	gcmostartvr->lt.left = dstrect->left;
	gcmostartvr->lt.top = dstrect->top;
	gcmostartvr->rb.right = dstrect->right;
	gcmostartvr->rb.bottom = dstrect->bottom;

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
		       struct gcsurface *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();

	struct gcfilter *gcfilter;
	struct gcsurface *dstinfo;

	bool scalex, scaley;
	bool singlepass, twopass;

	struct gcrect *tmporig;
	struct gcrect *srcorig, *srcclip;
	struct gcrect *dstorig, *dstclip, *dstadj;

	struct gcrect dstdelta;
	struct gcrect srcdelta;

	struct bvbuffmap *srcmap = NULL;
	struct bvbuffmap *tmpmap = NULL;
	struct bvbuffmap *dstmap = NULL;

	struct gcmovrconfigex *gcmovrconfigex;

	unsigned int srcx, srcy;
	unsigned int srcwidth, srcheight;
	unsigned int dstwidth, dstheight;
	unsigned int horscalefactor, verscalefactor;
	unsigned int kernelsize;
	int angle;

	GCENTER(GCZONE_FILTER);

	/* Get some shortcuts. */
	dstinfo = &batch->dstinfo;
	gcfilter = &batch->op.filter;

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

	/* Zero-fill for source is not supported. */
	if (srcinfo->format.zerofill) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM_FORMAT
					: BVERR_SRC2GEOM_FORMAT,
			      "0 filling is not supported.");
		goto exit;
	}

	/* Parse the scale mode. */
	bverror = parse_scalemode(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Parse destination parameters. */
	bverror = parse_destination(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Ignore the blit if destination rectangle is empty. */
	if (null_rect(&dstinfo->rect.clip)) {
		GCDBG(GCZONE_DEST, "empty destination rectangle.\n");
		goto exit;
	}

	/* Get rectangle shortcuts. */
	srcorig = &srcinfo->rect.orig;
	srcclip = &srcinfo->rect.clip;

	if ((srcinfo->index == 1) && dstinfo->haveaux) {
		GCDBG(GCZONE_FILTER, "picking aux set");
		dstorig = &dstinfo->auxrect.orig;
		dstclip = &dstinfo->auxrect.clip;
		dstadj = &dstinfo->auxrect.adj;
	} else {
		GCDBG(GCZONE_FILTER, "picking main set");
		dstorig = &dstinfo->rect.orig;
		dstclip = &dstinfo->rect.clip;
		dstadj = &dstinfo->rect.adj;
	}

	GCPRINT_RECT(GCZONE_FILTER, "original src", srcorig);
	GCPRINT_RECT(GCZONE_FILTER, "original dst", dstorig);
	GCPRINT_RECT(GCZONE_FILTER, "clipped dst", dstclip);
	GCPRINT_RECT(GCZONE_FILTER, "adjusted dst", dstadj);

	/* Compute the source alignments needed to compensate
	 * for the surface base address misalignment if any. */
	srcinfo->xpixalign = get_pixel_offset(srcinfo, 0);
	srcinfo->ypixalign = 0;
	srcinfo->bytealign1 = (srcinfo->xpixalign
			    * (int) srcinfo->format.bitspp) / 8;
	GCDBG(GCZONE_SRC, "source surface offset (pixels) = %d,%d\n",
		srcinfo->xpixalign, srcinfo->ypixalign);
	GCDBG(GCZONE_SRC, "source surface offset (bytes) = %d\n",
		srcinfo->bytealign1);

	/* Determine physical size. */
	if ((srcinfo->angle % 2) == 0) {
		srcinfo->physwidth  = srcinfo->width
				    - srcinfo->xpixalign;
		srcinfo->physheight = srcinfo->height
				    - srcinfo->ypixalign;
	} else {
		srcinfo->physwidth  = srcinfo->height
				    - srcinfo->xpixalign;
		srcinfo->physheight = srcinfo->width
				    - srcinfo->ypixalign;
	}
	GCDBG(GCZONE_SRC, "source physical size = %dx%d\n",
	      srcinfo->physwidth, srcinfo->physheight);

	/* OPF does not support source rotation, which can be compensated by
	 * using destination rotation. Compute the adjustment angle.
	 * For simplicity use the same algorythm for both OPF and TPF. */
	srcinfo->adjangle = (4 - srcinfo->angle) % 4;
	adjust_angle(srcinfo, dstinfo);

	/* Set rotation angle. */
	angle = dstinfo->angle;

	/* Compute U/V plane offsets. */
	if ((srcinfo->format.type == BVFMT_YUV) &&
	    (srcinfo->format.cs.yuv.planecount > 1))
		set_computeyuv(srcinfo, 0, 0);

	/* Determine the source and destination rectangles. */
	srcwidth  = srcorig->right  - srcorig->left;
	srcheight = srcorig->bottom - srcorig->top;
	dstwidth  = dstorig->right  - dstorig->left;
	dstheight = dstorig->bottom - dstorig->top;

	GCDBG(GCZONE_FILTER, "adjusted input src size: %dx%d\n",
	      srcwidth, srcheight);
	GCDBG(GCZONE_FILTER, "adjusted input dst size: %dx%d\n",
	      dstwidth, dstheight);

	/* Determine the data path. */
	scalex = (srcwidth  != dstwidth);
	scaley = (srcheight != dstheight);

	twopass = scalex && scaley;
	if (twopass) {
		if (((gcfilter->horkernelsize == 3) ||
		     (gcfilter->horkernelsize == 5)) &&
		    ((gcfilter->verkernelsize == 3) ||
		     (gcfilter->verkernelsize == 5))) {
			singlepass = true;
			twopass = false;
		} else {
			singlepass = false;
		}
	} else {
		/* Two pass filter in one pass mode. */
		if (!scalex && !scaley)
			GCERR("no scaling needed.\n");

		GCDBG(GCZONE_FILTER, "only %s scaling needed.\n",
			scalex ? "horizontal" : "vertical");

		singlepass = false;
	}

	gcbv_debug_scaleblt(scalex, scaley, singlepass);

	/* Compute the scale factors. */
	gcfilter->horscalefactor =
	horscalefactor = get_scale_factor(srcwidth, dstwidth);
	GCDBG(GCZONE_FILTER, "horscalefactor = 0x%08X\n", horscalefactor);

	gcfilter->verscalefactor =
	verscalefactor = get_scale_factor(srcheight, dstheight);
	GCDBG(GCZONE_FILTER, "verscalefactor = 0x%08X\n", verscalefactor);

	/* Compute the destination offsets. */
	dstdelta.left   = dstclip->left   - dstorig->left;
	dstdelta.top    = dstclip->top    - dstorig->top;
	dstdelta.right  = dstclip->right  - dstorig->left;
	dstdelta.bottom = dstclip->bottom - dstorig->top;
	GCDBG(GCZONE_FILTER, "dst deltas = (%d,%d)-(%d,%d)\n",
	      dstdelta.left, dstdelta.top, dstdelta.right, dstdelta.bottom);

	/* Compute the source offsets. */
	srcdelta.left   =  dstdelta.left        * horscalefactor;
	srcdelta.top    =  dstdelta.top         * verscalefactor;
	srcdelta.right  = (dstdelta.right  - 1) * horscalefactor + (1 << 16);
	srcdelta.bottom = (dstdelta.bottom - 1) * verscalefactor + (1 << 16);

	/* Before rendering each destination pixel, the HW will select the
	 * corresponding source center pixel to apply the kernel around.
	 * To make this process precise we need to add 0.5 to source initial
	 * coordinates here; this will make HW pick the next source pixel if
	 * the fraction is equal or greater then 0.5. */
	srcdelta.left   += 0x00008000;
	srcdelta.top    += 0x00008000;
	srcdelta.right  += 0x00008000;
	srcdelta.bottom += 0x00008000;
	GCDBG(GCZONE_FILTER, "src deltas = "
	      "(0x%08X,0x%08X)-(0x%08X,0x%08X)\n",
	      srcdelta.left, srcdelta.top, srcdelta.right, srcdelta.bottom);
	GCDBG(GCZONE_FILTER, "src deltas (int) = (%d,%d)-(%d,%d)\n",
	      srcdelta.left >> 16, srcdelta.top >> 16,
	      srcdelta.right >> 16, srcdelta.bottom >> 16);

	/* Determine clipped source rectangle. */
	srcclip->left   = srcorig->left + (srcdelta.left   >> 16);
	srcclip->top    = srcorig->top  + (srcdelta.top    >> 16);
	srcclip->right  = srcorig->left + (srcdelta.right  >> 16);
	srcclip->bottom = srcorig->top  + (srcdelta.bottom >> 16);

	GCDBG(GCZONE_FILTER, "source:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom = %dx%d\n",
	      srcinfo->stride1, srcinfo->width, srcinfo->height);
	GCDBG(GCZONE_FILTER, "  rotation = %d\n", srcinfo->angle);
	GCPRINT_RECT(GCZONE_FILTER, "  clipped rect", srcclip);

	GCDBG(GCZONE_FILTER, "destination:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom size = %dx%d\n",
	      dstinfo->stride1, dstinfo->width, dstinfo->height);
	GCDBG(GCZONE_FILTER, "  rotation = %d\n", dstinfo->angle);
	GCPRINT_RECT(GCZONE_FILTER, "  clipped rect", dstclip);

	/* Validate the source rectangle. */
	if (!valid_rect(srcinfo, srcclip)) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1RECT
					: BVERR_SRC2RECT,
			      "invalid source rectangle.");
		goto exit;
	}

	/* Ignore the blit if source rectangle is empty. */
	if (null_rect(srcclip)) {
		GCDBG(GCZONE_SURF, "empty source rectangle.\n");
		goto exit;
	}

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

	/* Do single pass filter if we can. */
	if (singlepass) {
		GCDBG(GCZONE_TYPE, "single pass\n");

		/* Determine the kernel size to use. */
		kernelsize = max(gcfilter->horkernelsize,
				 gcfilter->verkernelsize);

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
		srcx = (srcorig->left << 16) + srcdelta.left;
		srcy = (srcorig->top  << 16) + srcdelta.top;
		GCDBG(GCZONE_SRC, "src origin: 0x%08X,0x%08X\n", srcx, srcy);

		/* Load the horizontal filter. */
		bverror = load_filter(bvbltparams, batch,
				      GC_FILTER_SYNC,
				      gcfilter->horkernelsize,
				      gcfilter->horscalefactor,
				      srcwidth, dstwidth,
				      gcmofilterkernel_horizontal_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Load the vertical filter. */
		bverror = load_filter(bvbltparams, batch,
				      GC_FILTER_SYNC,
				      gcfilter->verkernelsize,
				      gcfilter->verscalefactor,
				      srcheight, dstheight,
				      gcmofilterkernel_vertical_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		bverror = startvr(bvbltparams, batch,
				  srcmap, dstmap, srcinfo, dstinfo,
				  srcx, srcy, dstadj,
				  ROT_ANGLE_0, angle,
				  GC_SCALE_OPF);
	} else if (twopass) {
		unsigned int horkernelhalf;
		unsigned int leftextra, rightextra;
		unsigned int tmprectwidth, tmprectheight;
		unsigned int tmpalignmask, dstalignmask;
		unsigned int tmpsize;
		struct gcsurface tmpinfo;

		GCDBG(GCZONE_TYPE, "two pass\n");

		/* Initialize the temporaty surface descriptor. */
		tmpinfo.index = -1;
		tmpinfo.angle = angle;
		tmpinfo.mirror = GCREG_MIRROR_NONE;
		tmpinfo.rop = 0;
		GCDBG(GCZONE_FILTER, "tmp angle = %d\n", tmpinfo.angle);

		/* Transfer blending parameters from the source to the
		 * temporary buffer so that the blending would happen
		 * on the second pass. */
		tmpinfo.gca = srcinfo->gca;
		srcinfo->gca = NULL;

		/* Determine temporary surface format. */
		if (srcinfo->format.type == BVFMT_YUV) {
			if (tmpinfo.angle == ROT_ANGLE_0) {
				GCDBG(GCZONE_FILTER,
				      "tmp format = 4:2:2\n");
				parse_format(bvbltparams, OCDFMT_YUYV,
					     &tmpinfo.format);
			} else {
				GCDBG(GCZONE_FILTER,
				      "tmp format = dst format\n");
				tmpinfo.format = dstinfo->format;
			}
		} else {
			GCDBG(GCZONE_FILTER,
			      "tmp format = src format\n");
			tmpinfo.format = srcinfo->format;
		}

		/* Determine pixel alignment masks. */
		tmpalignmask = GC_BITS_PER_CACHELINE
			     / tmpinfo.format.bitspp - 1;
		dstalignmask = GC_BITS_PER_CACHELINE
			     / dstinfo->format.bitspp - 1;

		/* In partial filter blit cases, the vertical pass has to render
		 * more pixel information to the left and to the right of the
		 * temporary image so that the next pass has its necessary
		 * kernel information on the edges of the image. */
		horkernelhalf = gcfilter->horkernelsize >> 1;

		leftextra  = srcdelta.left >> 16;
		rightextra = srcwidth - (srcdelta.right >> 16);

		if (leftextra > horkernelhalf)
			leftextra = horkernelhalf;

		if (rightextra > horkernelhalf)
			rightextra = horkernelhalf;

		GCDBG(GCZONE_FILTER, "leftextra = %d, rightextra = %d\n",
		      leftextra, rightextra);

		/* Determine the source origin. */
		srcx = ((srcorig->left - leftextra) << 16) + srcdelta.left;
		srcy =  (srcorig->top << 16) + srcdelta.top;
		GCDBG(GCZONE_SRC, "src origin: 0x%08X,0x%08X\n", srcx, srcy);
		GCDBG(GCZONE_SRC, "src origin (int): %d,%d\n",
		      srcx >> 16, srcy >> 16);

		/* Determine the size of the temporary rectangle. */
		tmprectwidth = leftextra + rightextra
			     + ((srcdelta.right >> 16) - (srcdelta.left >> 16));
		tmprectheight = dstadj->bottom - dstadj->top;
		GCDBG(GCZONE_FILTER, "tmp rect size: %dx%d\n",
		      tmprectwidth, tmprectheight);

		/* Shortcut to temporary rectangle. */
		tmporig = &tmpinfo.rect.orig;

		/* Determine the temporary destination coordinates. */
		switch (angle) {
		case ROT_ANGLE_0:
		case ROT_ANGLE_180:
			tmporig->left   = (srcx >> 16) & tmpalignmask;
			tmporig->top    = 0;
			tmporig->right  = tmporig->left + tmprectwidth;
			tmporig->bottom = tmprectheight;

			tmpinfo.width  = (tmporig->right + tmpalignmask)
				       & ~tmpalignmask;
			tmpinfo.height = tmprectheight;

			tmpinfo.physwidth  = tmpinfo.width;
			tmpinfo.physheight = tmpinfo.height;
			break;

		case ROT_ANGLE_90:
			tmporig->left   = 0;
			tmporig->top    = dstadj->left & dstalignmask;
			tmporig->right  = tmprectwidth;
			tmporig->bottom = tmporig->top  + tmprectheight;

			tmpinfo.width  = tmprectwidth;
			tmpinfo.height = (tmporig->bottom + tmpalignmask)
				       & ~tmpalignmask;

			tmpinfo.physwidth  = tmpinfo.height;
			tmpinfo.physheight = tmpinfo.width;
			break;

		case ROT_ANGLE_270:
			tmporig->left   = 0;
			tmporig->right  = tmprectwidth;
			tmporig->bottom = dstadj->left & dstalignmask;

			tmpinfo.width  = tmprectwidth;
			tmpinfo.height = (tmporig->bottom + tmprectheight
				       + tmpalignmask) & ~tmpalignmask;

			tmporig->bottom = tmpinfo.height - tmporig->bottom;
			tmporig->top    = tmporig->bottom - tmprectheight;

			tmpinfo.physwidth  = tmpinfo.height;
			tmpinfo.physheight = tmpinfo.width;
			break;
		}

		GCPRINT_RECT(GCZONE_DEST, "tmp dest", tmporig);
		GCDBG(GCZONE_FILTER, "tmp geometry size = %dx%d\n",
		      tmpinfo.width, tmpinfo.height);
		GCDBG(GCZONE_FILTER, "tmp physical size = %dx%d\n",
		      tmpinfo.physwidth, tmpinfo.physheight);

		/* Determine the size of the temporaty surface. */
		tmpinfo.stride1 = (tmpinfo.physwidth
				*  tmpinfo.format.bitspp) / 8;
		tmpsize = tmpinfo.stride1 * tmpinfo.physheight;
		tmpsize += GC_BYTES_PER_CACHELINE;
		tmpsize = (tmpsize + ~PAGE_MASK) & PAGE_MASK;
		GCDBG(GCZONE_FILTER, "tmp stride = %d\n", tmpinfo.stride1);
		GCDBG(GCZONE_FILTER, "tmp size (bytes) = %d\n", tmpsize);

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

		/* Compute the temp buffer alignments needed to compensate
		 * for the surface base address misalignment if any. */
		tmpinfo.xpixalign = 0;
		tmpinfo.ypixalign = 0;
		tmpinfo.bytealign1 = (get_pixel_offset(&tmpinfo, 0)
				   * (int) tmpinfo.format.bitspp) / 8;
		GCDBG(GCZONE_SRC, "tmp offset (pixels) = %d,%d\n",
			tmpinfo.xpixalign, tmpinfo.ypixalign);
		GCDBG(GCZONE_SRC, "tmp offset (bytes) = %d\n",
			tmpinfo.bytealign1);

		/* Load the vertical filter. */
		bverror = load_filter(bvbltparams, batch,
				      GC_FILTER_SYNC,
				      gcfilter->verkernelsize,
				      gcfilter->verscalefactor,
				      srcheight, dstheight,
				      gcmofilterkernel_shared_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		GCDBG(GCZONE_TYPE, "vertical pass\n");
		bverror = startvr(bvbltparams, batch,
				  srcmap, tmpmap, srcinfo, &tmpinfo,
				  srcx, srcy, tmporig,
				  ROT_ANGLE_0, angle,
				  GC_SCALE_VER);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Rotation is done in the first pass. */
		tmpinfo.adjangle = (4 - angle) % 4;
		adjust_angle(&tmpinfo, dstinfo);

		/* Determine the source origin. */
		switch (angle) {
		case ROT_ANGLE_0:
			srcx = ((tmporig->left + leftextra) << 16)
			     + (srcdelta.left & 0xFFFF);
			srcy = tmporig->top << 16;
			break;

		case ROT_ANGLE_90:
			srcx = tmporig->left << 16;
			srcy = ((tmporig->top + rightextra) << 16)
			     + ((~srcdelta.right + 1) & 0xFFFF);
			break;

		case ROT_ANGLE_180:
			srcx = ((tmporig->left + rightextra) << 16)
			     + ((~srcdelta.right + 1) & 0xFFFF);
			srcy = tmporig->top << 16;
			break;

		case ROT_ANGLE_270:
			srcx = tmporig->left << 16;
			srcy = ((tmporig->top + leftextra) << 16)
			     + (srcdelta.left & 0xFFFF);
			break;
		}

		GCDBG(GCZONE_SRC, "src origin: 0x%08X,0x%08X\n", srcx, srcy);

		/* Load the horizontal filter. */
		bverror = load_filter(bvbltparams, batch,
				      GC_FILTER_SYNC,
				      gcfilter->horkernelsize,
				      gcfilter->horscalefactor,
				      srcwidth, dstwidth,
				      gcmofilterkernel_shared_ldst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Start the operation. */
		GCDBG(GCZONE_TYPE, "horizontal pass\n");
		bverror = startvr(bvbltparams, batch,
				  tmpmap, dstmap, &tmpinfo, dstinfo,
				  srcx, srcy, dstadj,
				  ROT_ANGLE_0, ROT_ANGLE_0,
				  ((angle % 2) == 0)
					? GC_SCALE_HOR
					: GC_SCALE_HOR_FLIPPED);
		if (bverror != BVERR_NONE)
			goto exit;
	} else {
		GCDBG(GCZONE_TYPE, "two pass (%s pass config).\n",
		      scalex ? "horizontal" : "vertical");

		/* Setup single pass. */
		srcx = (srcorig->left << 16) + srcdelta.left;
		srcy = (srcorig->top  << 16) + srcdelta.top;
		GCDBG(GCZONE_SRC, "src origin: 0x%08X,0x%08X\n", srcx, srcy);

		if (scalex) {
			/* Load the horizontal filter. */
			bverror = load_filter(bvbltparams, batch,
					      GC_FILTER_SYNC,
					      gcfilter->horkernelsize,
					      gcfilter->horscalefactor,
					      srcwidth, dstwidth,
					      gcmofilterkernel_shared_ldst);
			if (bverror != BVERR_NONE)
				goto exit;

			/* Start the operation. */
			bverror = startvr(bvbltparams, batch,
					  srcmap, dstmap, srcinfo, dstinfo,
					  srcx, srcy, dstadj,
					  ROT_ANGLE_0, angle,
					  GC_SCALE_HOR);
			if (bverror != BVERR_NONE)
				goto exit;
		} else {
			/* Load the vertical filter. */
			bverror = load_filter(bvbltparams, batch,
					      GC_FILTER_SYNC,
					      gcfilter->verkernelsize,
					      gcfilter->verscalefactor,
					      srcheight, dstheight,
					      gcmofilterkernel_shared_ldst);
			if (bverror != BVERR_NONE)
				goto exit;

			/* Start the operation. */
			bverror = startvr(bvbltparams, batch,
					  srcmap, dstmap, srcinfo, dstinfo,
					  srcx, srcy, dstadj,
					  ROT_ANGLE_0, angle,
					  GC_SCALE_VER);
			if (bverror != BVERR_NONE)
				goto exit;
		}
	}

exit:
	GCEXITARG(GCZONE_FILTER, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}
