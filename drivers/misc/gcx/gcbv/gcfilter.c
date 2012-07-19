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
#define GCZONE_FILTER		(1 << 11)

GCDBG_FILTERDEF(gcfilter, GCZONE_NONE,
		"filter")

enum bverror do_filter(struct bvbltparams *bltparams,
		       struct gcbatch *batch,
		       struct srcinfo *srcinfo)
{
/*	enum bverror bverror = BVERR_NONE;*/
	int verpass;

	GCENTER(GCZONE_FILTER);

	GCDBG(GCZONE_FILTER, "source rectangle:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom = %dx%d\n",
		srcinfo->geom->virtstride,
		srcinfo->geom->width, srcinfo->geom->height);
	GCDBG(GCZONE_FILTER, "  rotation = %d\n",
		srcinfo->angle);
	GCDBG(GCZONE_FILTER, "  rect = (%d,%d)-(%d,%d), %dx%d\n",
		srcinfo->rect->left, srcinfo->rect->top,
		srcinfo->rect->left + srcinfo->rect->width,
		srcinfo->rect->top + srcinfo->rect->height,
		srcinfo->rect->width, srcinfo->rect->height);

	GCDBG(GCZONE_FILTER, "destination rectangle:\n");
	GCDBG(GCZONE_FILTER, "  stride = %d, geom size = %dx%d\n",
		bltparams->dstgeom->virtstride,
		bltparams->dstgeom->width, bltparams->dstgeom->height);
	GCDBG(GCZONE_FILTER, "  rotaton = %d\n",
		batch->dstangle);
	GCDBG(GCZONE_FILTER, "  rect = (%d,%d)-(%d,%d), %dx%d\n",
		bltparams->dstrect.left, bltparams->dstrect.top,
		bltparams->dstrect.left + bltparams->dstrect.width,
		bltparams->dstrect.top + bltparams->dstrect.height,
		bltparams->dstrect.width, bltparams->dstrect.height);

	/***********************************************************************
	 * Update kernel arrays.
	 */

	/* Do we need the vertical pass? */
	verpass = (srcinfo->rect->height != bltparams->dstrect.height);

#if 0
	/* Recompute the table if necessary. */
	gcmONERROR(_CalculateSyncTable(
	hardware,
	State->newHorKernelSize,
	srcRectSize.x,
	destRectSize.x,
	horKernel
	));

	gcmONERROR(_CalculateSyncTable(
	hardware,
	State->newVerKernelSize,
	srcRectSize.y,
	destRectSize.y,
	verKernel
	));
#endif

	GCEXITARG(GCZONE_FILTER, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);

	/* Not implemented yet */
	return BVERR_OP;
}
