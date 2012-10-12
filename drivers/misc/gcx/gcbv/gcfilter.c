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
 * Rotates the specified rectangle to the specified angle.
 */

static void rotate_gcrect(int angle,
			  struct bvsurfgeom *srcgeom, struct gcrect *srcrect,
			  struct bvsurfgeom *dstgeom, struct gcrect *dstrect)
{
	unsigned int width, height;
	struct gcrect rect;

	GCENTER(GCZONE_SURF);

	GCDBG(GCZONE_SURF, "src geom size = %dx%d\n",
	      srcgeom->width, srcgeom->height);

	switch (angle) {
	case ROT_ANGLE_0:
		GCDBG(GCZONE_SURF, "ROT_ANGLE_0\n");

		if (dstgeom != srcgeom) {
			dstgeom->width  = srcgeom->width;
			dstgeom->height = srcgeom->height;
		}

		if (dstrect != srcrect)
			*dstrect = *srcrect;
		break;

	case ROT_ANGLE_90:
		GCDBG(GCZONE_SURF, "ROT_ANGLE_90\n");

		width  = srcgeom->width;
		height = srcgeom->height;

		dstgeom->width  = height;
		dstgeom->height = width;

		rect.left   = height - srcrect->bottom;
		rect.top    = srcrect->left;
		rect.right  = height - srcrect->top;
		rect.bottom = srcrect->right;

		*dstrect = rect;
		break;

	case ROT_ANGLE_180:
		GCDBG(GCZONE_SURF, "ROT_ANGLE_180\n");

		width  = srcgeom->width;
		height = srcgeom->height;

		if (dstgeom != srcgeom) {
			dstgeom->width  = width;
			dstgeom->height = height;
		}

		rect.left   = width  - srcrect->right;
		rect.top    = height - srcrect->bottom;
		rect.right  = width  - srcrect->left;
		rect.bottom = height - srcrect->top;

		*dstrect = rect;
		break;

	case ROT_ANGLE_270:
		GCDBG(GCZONE_SURF, "ROT_ANGLE_270\n");

		width  = srcgeom->width;
		height = srcgeom->height;

		dstgeom->width  = height;
		dstgeom->height = width;

		rect.left   = srcrect->top;
		rect.top    = width - srcrect->right;
		rect.right  = srcrect->bottom;
		rect.bottom = width - srcrect->left;

		*dstrect = rect;
		break;
	}

	GCEXIT(GCZONE_SURF);
}


/*******************************************************************************
 * Setup destination rotation parameters.
 */

void process_rotation(struct bvbltparams *bvbltparams,
		      struct gcbatch *batch,
		      struct surfaceinfo *srcinfo,
		      int adjangle)
{
	GCENTER(GCZONE_DEST);

	if (srcinfo->newgeom ||
	    ((batch->batchflags & (BVBATCH_CLIPRECT |
				   BVBATCH_DESTRECT |
				   BVBATCH_DST)) != 0)) {
		bool orthogonal;
		struct gcfilter *gcfilter;
		struct surfaceinfo *dstinfo;
		int dstoffsetX, dstoffsetY;

		/* Get some shortcuts. */
		dstinfo = &batch->dstinfo;
		gcfilter = &batch->op.filter;

		/* Compute the adjusted destination angle. */
		gcfilter->dstangle
			= (dstinfo->angle + (4 - srcinfo->angle)) % 4;
		GCDBG(GCZONE_DEST, "dstangle = %d\n", gcfilter->dstangle);

		/* Determine whether the new and the old destination angles
		 * are orthogonal to each other. */
		orthogonal = (gcfilter->dstangle % 2) != (dstinfo->angle % 2);

		switch (gcfilter->dstangle) {
		case ROT_ANGLE_0:
			/* Determine the origin offset. */
			dstoffsetX = dstinfo->xpixalign;
			dstoffsetY = dstinfo->ypixalign;

			/* Determine geometry size. */
			if (orthogonal) {
				batch->dstwidth  = dstinfo->geom->height
						 - dstinfo->xpixalign;
				batch->dstheight = dstinfo->geom->width
						 - dstinfo->ypixalign;
			} else {
				batch->dstwidth  = dstinfo->geom->width
						 - dstinfo->xpixalign;
				batch->dstheight = dstinfo->geom->height
						 - dstinfo->ypixalign;
			}

			/* Determine the physical size. */
			dstinfo->physwidth  = batch->dstwidth;
			dstinfo->physheight = batch->dstheight;
			break;

		case ROT_ANGLE_90:
			/* Determine the origin offset. */
			dstoffsetX = dstinfo->ypixalign;
			dstoffsetY = dstinfo->xpixalign;

			if (orthogonal) {
				/* Determine geometry size. */
				batch->dstwidth  = dstinfo->geom->height
						 - dstinfo->ypixalign;
				batch->dstheight = dstinfo->geom->width
						 - dstinfo->xpixalign;

				/* Determine the physical size. */
				dstinfo->physwidth  = dstinfo->geom->width
						    - dstinfo->xpixalign;
				dstinfo->physheight = dstinfo->geom->height
						    - dstinfo->ypixalign;
			} else {
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
			}
			break;

		case ROT_ANGLE_180:
			/* Determine the origin offset. */
			dstoffsetX = 0;
			dstoffsetY = 0;

			/* Determine geometry size. */
			if (orthogonal) {
				batch->dstwidth  = dstinfo->geom->height
						 - dstinfo->xpixalign;
				batch->dstheight = dstinfo->geom->width
						 - dstinfo->ypixalign;
			} else {
				batch->dstwidth  = dstinfo->geom->width
						 - dstinfo->xpixalign;
				batch->dstheight = dstinfo->geom->height
						 - dstinfo->ypixalign;
			}

			/* Determine the physical size. */
			dstinfo->physwidth  = batch->dstwidth;
			dstinfo->physheight = batch->dstheight;
			break;

		case ROT_ANGLE_270:
			/* Determine the origin offset. */
			dstoffsetX = 0;
			dstoffsetY = 0;

			if (orthogonal) {
				/* Determine geometry size. */
				batch->dstwidth  = dstinfo->geom->height
						 = dstinfo->ypixalign;
				batch->dstheight = dstinfo->geom->width
						 - dstinfo->xpixalign;

				/* Determine the physical size. */
				dstinfo->physwidth  = dstinfo->geom->width
						    - dstinfo->xpixalign;
				dstinfo->physheight = dstinfo->geom->height
						    - dstinfo->ypixalign;
			} else {
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
			}
			break;

		default:
			dstoffsetX = 0;
			dstoffsetY = 0;
		}

		/* Rotate the original destination rectangle
		 * to match the new angle. */
		rotate_gcrect(adjangle,
			      dstinfo->geom, &dstinfo->rect,
			      &gcfilter->dstgeom, &gcfilter->dstrect);

		/* Rotate the clipped destination rectangle. */
		rotate_gcrect(adjangle,
			      dstinfo->geom, &batch->dstclipped,
			      &gcfilter->dstgeom, &gcfilter->dstclipped);

		/* Compute the adjusted the destination rectangle. */
		gcfilter->dstadjusted.left
			= gcfilter->dstclipped.left - dstoffsetX;
		gcfilter->dstadjusted.top
			= gcfilter->dstclipped.top - dstoffsetY;
		gcfilter->dstadjusted.right
			= gcfilter->dstclipped.right - dstoffsetX;
		gcfilter->dstadjusted.bottom
			= gcfilter->dstclipped.bottom - dstoffsetY;

		GCPRINT_RECT(GCZONE_DEST, "rotated dstrect",
			     &gcfilter->dstrect);
		GCPRINT_RECT(GCZONE_DEST, "rotated dstclipped",
			     &gcfilter->dstclipped);
		GCPRINT_RECT(GCZONE_DEST, "rotated dstadjusted",
			     &gcfilter->dstadjusted);

		if (batch->haveaux) {
			/* Rotate the original aux destination rectangle
			 * to match the new angle. */
			rotate_gcrect(adjangle, dstinfo->geom,
				      &batch->dstrectaux, &gcfilter->dstgeom,
				      &gcfilter->dstrectaux);

			/* Rotate the aux destination rectangle. */
			rotate_gcrect(adjangle, dstinfo->geom,
				      &batch->dstclippedaux, &gcfilter->dstgeom,
				      &gcfilter->dstclippedaux);

			/* Compute the adjust the aux destination rectangle. */
			gcfilter->dstadjustedaux.left
				= batch->dstclippedaux.left - dstoffsetX;
			gcfilter->dstadjustedaux.top
				= batch->dstclippedaux.top - dstoffsetY;
			gcfilter->dstadjustedaux.right
				= batch->dstclippedaux.right - dstoffsetX;
			gcfilter->dstadjustedaux.bottom
				= batch->dstclippedaux.bottom - dstoffsetY;

			GCPRINT_RECT(GCZONE_DEST, "rotated dstrectaux",
				     &gcfilter->dstrectaux);
			GCPRINT_RECT(GCZONE_DEST, "rotated dstclippedaux",
				     &gcfilter->dstclippedaux);
			GCPRINT_RECT(GCZONE_DEST, "rotated dstadjustedaux",
				     &gcfilter->dstadjustedaux);
		}

		GCDBG(GCZONE_DEST, "aligned geometry size = %dx%d\n",
		      batch->dstwidth, batch->dstheight);
		GCDBG(GCZONE_DEST, "aligned physical size = %dx%d\n",
		      dstinfo->physwidth, dstinfo->physheight);
		GCDBG(GCZONE_DEST, "origin offset (pixels) = %d,%d\n",
		      dstoffsetX, dstoffsetY);
	}

	GCEXIT(GCZONE_DEST);
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

	struct gcrect srcrect;

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
	GCDBG(GCZONE_FILTER, "  bytealign = %d\n", dstinfo->bytealign);
	GCDBG(GCZONE_FILTER, "  virtstride = %d\n", dstinfo->geom->virtstride);
	GCDBG(GCZONE_FILTER, "  format = %d\n", dstinfo->format.format);
	GCDBG(GCZONE_FILTER, "  swizzle = %d\n", dstinfo->format.swizzle);
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

	/* Determine adjusted source bounding rectangle and origin. */
	srcrect = srcinfo->rect;
	srcrect.left  -=  srcinfo->xpixalign;
	srcrect.right -=  srcinfo->xpixalign;
	srcx          -= (srcinfo->xpixalign << 16);

	GCDBG(GCZONE_FILTER, "source:\n");
	GCDBG(GCZONE_FILTER, "  angle = %d\n", srcangle);
	GCDBG(GCZONE_FILTER, "  pixalign = %d,%d\n",
	      srcinfo->xpixalign, srcinfo->ypixalign);
	GCDBG(GCZONE_FILTER, "  bytealign = %d\n", srcinfo->bytealign);
	GCDBG(GCZONE_FILTER, "  virtstride = %d\n", srcinfo->geom->virtstride);
	GCDBG(GCZONE_FILTER, "  format = %d\n", srcinfo->format.format);
	GCDBG(GCZONE_FILTER, "  swizzle = %d\n", srcinfo->format.swizzle);
	GCDBG(GCZONE_FILTER, "  premul = %d\n", srcinfo->format.premultiplied);
	GCDBG(GCZONE_FILTER, "  physwidth = %d\n", srcinfo->physwidth);
	GCDBG(GCZONE_FILTER, "  physheight = %d\n", srcinfo->physheight);
	GCPRINT_RECT(GCZONE_FILTER, "  rect", &srcrect);

	GCDBG(GCZONE_FILTER, "src origin: 0x%08X,0x%08X\n", srcx, srcy);

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmovrsrc),
			       (void **) &gcmovrsrc);
	if (bverror != BVERR_NONE)
		goto exit;

	add_fixup(bvbltparams, batch, &gcmovrsrc->address, srcinfo->bytealign);

	gcmovrsrc->config_ldst = gcmovrsrc_config_ldst;

	gcmovrsrc->address = GET_MAP_HANDLE(srcmap);
	gcmovrsrc->stride = srcinfo->geom->virtstride;

	gcmovrsrc->rotation.raw = 0;
	gcmovrsrc->rotation.reg.surf_width = srcinfo->physwidth;

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
	gcmovrsrc->mult.reg.srcglobalpremul
	= GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE;

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
		       struct surfaceinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();

	struct gcfilter *gcfilter;
	struct surfaceinfo *dstinfo;

	bool scalex, scaley;
	bool singlepass, twopass;

	struct gcrect *srcrect;
	struct gcrect *dstrect;
	struct gcrect *dstclipped;
	struct gcrect *dstadjusted;

	struct bvsurfgeom dstrotated0geom;
	struct gcrect  dstrotated0;

	struct gcrect dstdelta;
	struct gcrect srcdelta;
	struct gcrect srcclipped;

	struct bvbuffmap *srcmap = NULL;
	struct bvbuffmap *tmpmap = NULL;
	struct bvbuffmap *dstmap = NULL;

	struct gcmovrconfigex *gcmovrconfigex;

	int adjangle;
	unsigned int srcx, srcy;
	unsigned int srcwidth, srcheight;
	unsigned int dstwidth, dstheight;
	unsigned int horscalefactor, verscalefactor;
	unsigned int kernelsize;

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

	/* Parse the scale mode. */
	bverror = parse_scalemode(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Parse destination parameters. */
	bverror = parse_destination(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Compute the source alignments needed to compensate
	 * for the surface base address misalignment if any. */
	srcinfo->xpixalign = get_pixel_offset(srcinfo, 0);
	srcinfo->ypixalign = 0;
	srcinfo->bytealign = (srcinfo->xpixalign
			   * (int) srcinfo->format.bitspp) / 8;
	GCDBG(GCZONE_SRC, "source surface offset (pixels) = %d,%d\n",
		srcinfo->xpixalign, srcinfo->ypixalign);
	GCDBG(GCZONE_SRC, "source surface offset (bytes) = %d\n",
		srcinfo->bytealign);

	/* Compute U/V plane offsets. */
	if ((srcinfo->format.type == BVFMT_YUV) &&
	    (srcinfo->format.cs.yuv.planecount > 1))
		set_computeyuv(srcinfo, 0, 0);

	/* Determine physical size. */
	if ((srcinfo->angle % 2) == 0) {
		srcinfo->physwidth  = srcinfo->geom->width
				    - srcinfo->xpixalign;
		srcinfo->physheight = srcinfo->geom->height
				    - srcinfo->ypixalign;
	} else {
		srcinfo->physwidth  = srcinfo->geom->height
				    - srcinfo->xpixalign;
		srcinfo->physheight = srcinfo->geom->width
				    - srcinfo->ypixalign;
	}
	GCDBG(GCZONE_SRC, "source physical size = %dx%d\n",
		srcinfo->physwidth, srcinfo->physheight);

	/* OPF does not support source rotation, which can be compensated by
	 * using destination rotation. Compute the adjustment angle.
	 * For simplicity use the same algorythm for both OPF and TPF. */
	adjangle = (4 - srcinfo->angle) % 4;
	GCDBG(GCZONE_DEST, "adjangle = %d\n", adjangle);

	/* Compute destination rotation. */
	process_rotation(bvbltparams, batch, srcinfo, adjangle);

	/* Rotate the source rectangle to 0 degree. */
	srcrect = &srcinfo->rect;
	GCPRINT_RECT(GCZONE_FILTER, "full src", srcrect);
	rotate_gcrect(adjangle,
		      srcinfo->geom, srcrect,
		      srcinfo->geom, srcrect);
	GCPRINT_RECT(GCZONE_FILTER, "full adjusted src", srcrect);

	/* Get destination rect shortcuts. */
	if ((srcinfo->index == 1) && batch->haveaux) {
		dstrect = &gcfilter->dstrectaux;
		dstclipped = &gcfilter->dstclippedaux;
		dstadjusted = &gcfilter->dstadjustedaux;
	} else {
		dstrect = &gcfilter->dstrect;
		dstclipped = &gcfilter->dstclipped;
		dstadjusted = &gcfilter->dstadjusted;
	}

	GCPRINT_RECT(GCZONE_FILTER, "full adjusted dst", dstrect);
	GCPRINT_RECT(GCZONE_FILTER, "clipped adjusted dst", dstclipped);
	GCPRINT_RECT(GCZONE_FILTER, "aligned adjusted dst", dstadjusted);

	/* Determine the source and destination rectangles. */
	srcwidth  = srcrect->right  - srcrect->left;
	srcheight = srcrect->bottom - srcrect->top;
	dstwidth  = dstrect->right  - dstrect->left;
	dstheight = dstrect->bottom - dstrect->top;

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

	/* Compute the scale factors. */
	gcfilter->horscalefactor =
	horscalefactor = get_scale_factor(srcwidth, dstwidth);
	GCDBG(GCZONE_FILTER, "horscalefactor = 0x%08X\n", horscalefactor);

	gcfilter->verscalefactor =
	verscalefactor = get_scale_factor(srcheight, dstheight);
	GCDBG(GCZONE_FILTER, "verscalefactor = 0x%08X\n", verscalefactor);

	/* Compute the destination offsets. */
	dstdelta.left   = dstclipped->left   - dstrect->left;
	dstdelta.top    = dstclipped->top    - dstrect->top;
	dstdelta.right  = dstclipped->right  - dstrect->left;
	dstdelta.bottom = dstclipped->bottom - dstrect->top;
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
	srcclipped.left   = srcrect->left   + (srcdelta.left   >> 16);
	srcclipped.top    = srcrect->top    + (srcdelta.top    >> 16);
	srcclipped.right  = srcrect->left   + (srcdelta.right  >> 16);
	srcclipped.bottom = srcrect->top    + (srcdelta.bottom >> 16);

	GCDBG(GCZONE_FILTER, "source:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom = %dx%d\n",
	      srcinfo->geom->virtstride,
	      srcinfo->geom->width, srcinfo->geom->height);
	GCDBG(GCZONE_FILTER, "  rotation = %d\n",
	      srcinfo->angle);
	GCPRINT_RECT(GCZONE_FILTER, "  clipped rect", &srcclipped);

	GCDBG(GCZONE_FILTER, "destination:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom size = %dx%d\n",
	      dstinfo->geom->virtstride,
	      dstinfo->geom->width, dstinfo->geom->height);
	GCDBG(GCZONE_FILTER, "  rotation = %d\n",
	      dstinfo->angle);
	GCPRINT_RECT(GCZONE_FILTER, "  clipped rect", dstclipped);

	/* Validate the source rectangle. */
	if (!valid_rect(srcinfo->geom, &srcclipped)) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1RECT
					: BVERR_SRC2RECT,
			      "invalid source rectangle.");
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
		srcx = (srcrect->left << 16) + srcdelta.left;
		srcy = (srcrect->top  << 16) + srcdelta.top;
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
				  srcx, srcy, dstadjusted,
				  ROT_ANGLE_0, gcfilter->dstangle,
				  GC_SCALE_OPF);
	} else if (twopass) {
		unsigned int horkernelhalf;
		unsigned int leftextra, rightextra;
		unsigned int tmprectwidth, tmprectheight;
		unsigned int tmpalignmask, dstalignmask;
		unsigned int tmpsize;
		struct surfaceinfo tmpinfo;
		struct bvsurfgeom tmpgeom;

		GCDBG(GCZONE_TYPE, "two pass\n");

		/* Initialize the temporaty surface geometry. */
		tmpgeom.structsize = sizeof(struct bvsurfgeom);
		tmpgeom.orientation = 0;
		tmpgeom.paletteformat = 0;
		tmpgeom.palette = NULL;

		/* Initialize the temporaty surface descriptor. */
		tmpinfo.index = -1;
		tmpinfo.geom = &tmpgeom;
		tmpinfo.angle = gcfilter->dstangle;
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
				tmpgeom.format = OCDFMT_YUYV;
				parse_format(bvbltparams, &tmpinfo);
			} else {
				GCDBG(GCZONE_FILTER,
				      "tmp format = dst format\n");
				tmpgeom.format = dstinfo->geom->format;
				tmpinfo.format = dstinfo->format;
			}
		} else {
			GCDBG(GCZONE_FILTER,
			      "tmp format = src format\n");
			tmpgeom.format = srcinfo->geom->format;
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
		srcx = ((srcrect->left - leftextra) << 16) + srcdelta.left;
		srcy =  (srcrect->top << 16) + srcdelta.top;
		GCDBG(GCZONE_SRC, "src origin: 0x%08X,0x%08X\n", srcx, srcy);
		GCDBG(GCZONE_SRC, "src origin (int): %d,%d\n",
		      srcx >> 16, srcy >> 16);

		/* Determine the size of the temporary rectangle. */
		tmprectwidth = leftextra + rightextra
			     + ((srcdelta.right >> 16) - (srcdelta.left >> 16));
		tmprectheight = dstadjusted->bottom - dstadjusted->top;
		GCDBG(GCZONE_FILTER, "tmp rect size: %dx%d\n",
		      tmprectwidth, tmprectheight);

		/* Determine the temporary destination coordinates. */
		switch (tmpinfo.angle) {
		case ROT_ANGLE_0:
		case ROT_ANGLE_180:
			tmpinfo.rect.left   = (srcx >> 16) & tmpalignmask;
			tmpinfo.rect.top    = 0;
			tmpinfo.rect.right  = tmpinfo.rect.left + tmprectwidth;
			tmpinfo.rect.bottom = tmprectheight;

			tmpgeom.width  = (tmpinfo.rect.right + tmpalignmask)
				       & ~tmpalignmask;
			tmpgeom.height = tmprectheight;

			tmpinfo.physwidth  = tmpgeom.width;
			tmpinfo.physheight = tmpgeom.height;
			break;

		case ROT_ANGLE_90:
			tmpinfo.rect.left   = 0;
			tmpinfo.rect.top    = dstadjusted->left & dstalignmask;
			tmpinfo.rect.right  = tmprectwidth;
			tmpinfo.rect.bottom = tmpinfo.rect.top  + tmprectheight;

			tmpgeom.width  = tmprectwidth;
			tmpgeom.height = (tmpinfo.rect.bottom + tmpalignmask)
				       & ~tmpalignmask;

			tmpinfo.physwidth  = tmpgeom.height;
			tmpinfo.physheight = tmpgeom.width;
			break;

		case ROT_ANGLE_270:
			tmpinfo.rect.left   = 0;
			tmpinfo.rect.right  = tmprectwidth;
			tmpinfo.rect.bottom = dstadjusted->left & dstalignmask;

			tmpgeom.width  = tmprectwidth;
			tmpgeom.height = (tmpinfo.rect.bottom + tmprectheight
				       + tmpalignmask) & ~tmpalignmask;

			tmpinfo.rect.bottom = tmpgeom.height
					    - tmpinfo.rect.bottom;
			tmpinfo.rect.top    = tmpinfo.rect.bottom
					    - tmprectheight;

			tmpinfo.physwidth  = tmpgeom.height;
			tmpinfo.physheight = tmpgeom.width;
			break;
		}

		GCPRINT_RECT(GCZONE_DEST, "tmp dest", &tmpinfo.rect);
		GCDBG(GCZONE_FILTER, "tmp geometry size = %dx%d\n",
		      tmpgeom.width, tmpgeom.height);
		GCDBG(GCZONE_FILTER, "tmp physical size = %dx%d\n",
		      tmpinfo.physwidth, tmpinfo.physheight);

		/* Determine the size of the temporaty surface. */
		tmpgeom.virtstride = (tmpinfo.physwidth
				   *  tmpinfo.format.bitspp) / 8;
		tmpsize = tmpgeom.virtstride * tmpinfo.physheight;
		tmpsize += GC_BYTES_PER_CACHELINE;
		tmpsize = (tmpsize + ~PAGE_MASK) & PAGE_MASK;
		GCDBG(GCZONE_FILTER, "tmp stride = %d\n", tmpgeom.virtstride);
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
		tmpinfo.bytealign = (get_pixel_offset(&tmpinfo, 0)
				  * (int) tmpinfo.format.bitspp) / 8;
		GCDBG(GCZONE_SRC, "tmp offset (pixels) = %d,%d\n",
			tmpinfo.xpixalign, tmpinfo.ypixalign);
		GCDBG(GCZONE_SRC, "tmp offset (bytes) = %d\n",
			tmpinfo.bytealign);

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
				  srcx, srcy, &tmpinfo.rect,
				  ROT_ANGLE_0, tmpinfo.angle,
				  GC_SCALE_VER);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Fake no rotation. */
		adjangle = (4 - tmpinfo.angle) % 4;
		GCDBG(GCZONE_DEST, "adjangle = %d\n", adjangle);

		/* Rotate the source rectangle to 0 degree. */
		rotate_gcrect(adjangle,
			      tmpinfo.geom, &tmpinfo.rect,
			      tmpinfo.geom, &tmpinfo.rect);
		GCPRINT_RECT(GCZONE_DEST, "tmp src", &tmpinfo.rect);

		/* Rotate the destination rectangle to 0 degree. */
		rotate_gcrect(adjangle,
			      &gcfilter->dstgeom, dstclipped,
			      &dstrotated0geom, &dstrotated0);
		GCPRINT_RECT(GCZONE_DEST, "dest", &dstrotated0);

		/* Apply adjustment. */
		dstrotated0.left  -= dstinfo->xpixalign;
		dstrotated0.right -= dstinfo->xpixalign;

		/* Determine the source origin. */
		switch (tmpinfo.angle) {
		case ROT_ANGLE_0:
			srcx = ((tmpinfo.rect.left + leftextra) << 16)
			     + (srcdelta.left & 0xFFFF);
			srcy = (tmpinfo.rect.top << 16)
			     + (srcdelta.top & 0xFFFF);
			break;

		case ROT_ANGLE_90:
			srcx = (tmpinfo.rect.left << 16)
			     + (srcdelta.top & 0xFFFF);
			srcy = ((tmpinfo.rect.top + rightextra) << 16)
			     + (srcdelta.left & 0xFFFF);
			break;

		case ROT_ANGLE_180:
			srcx = ((tmpinfo.rect.left + rightextra) << 16)
			     + (srcdelta.left & 0xFFFF);
			srcy = (tmpinfo.rect.top << 16)
			     + (srcdelta.top & 0xFFFF);
			break;

		case ROT_ANGLE_270:
			srcx = (tmpinfo.rect.left << 16)
			     + (srcdelta.top & 0xFFFF);
			srcy = ((tmpinfo.rect.top + leftextra) << 16)
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
				  srcx, srcy, &dstrotated0,
				  ROT_ANGLE_0, ROT_ANGLE_0,
				  ((gcfilter->dstangle % 2) == 0)
					? GC_SCALE_HOR
					: GC_SCALE_HOR_FLIPPED);
		if (bverror != BVERR_NONE)
			goto exit;
	} else {
		GCDBG(GCZONE_TYPE, "two pass (%s pass config).\n",
		      scalex ? "horizontal" : "vertical");

		/* Setup single pass. */
		srcx = (srcrect->left << 16) + srcdelta.left;
		srcy = (srcrect->top  << 16) + srcdelta.top;
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
					  srcx, srcy, dstadjusted,
					  ROT_ANGLE_0, gcfilter->dstangle,
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
					  srcx, srcy, dstadjusted,
					  ROT_ANGLE_0, gcfilter->dstangle,
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
