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
#define GCZONE_BLEND		(1 << 0)
#define GCZONE_SURF		(1 << 1)
#define GCZONE_BLIT		(1 << 3)

GCDBG_FILTERDEF(gcblit, GCZONE_NONE,
		"blend",
		"surf",
		"blit")


static enum bverror do_blit_end(struct bvbltparams *bvbltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcblit *gcblit;
	struct surfaceinfo *dstinfo;
	struct gcmobltconfig *gcmobltconfig;
	struct gcmostartde *gcmostartde;

	GCENTER(GCZONE_BLIT);

	/* Get a shortcut to the operation specific data. */
	gcblit = &batch->op.blit;

	GCDBG(GCZONE_BLIT, "finalizing the blit, scrcount = %d\n",
	      gcblit->srccount);

	/* Get a shortcut to the destination surface. */
	dstinfo = &batch->dstinfo;

	/***********************************************************************
	 * Configure the operation.
	 */

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmobltconfig),
			       (void **) &gcmobltconfig);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Configure multi-source control. */
	gcmobltconfig->multisource_ldst = gcmobltconfig_multisource_ldst;
	gcmobltconfig->multisource.raw = 0;
	gcmobltconfig->multisource.reg.srccount = gcblit->srccount - 1;

	GCDBG(GCZONE_BLIT, "blockenable = %d\n", batch->blockenable);
	if (batch->blockenable) {
		gcmobltconfig->multisource.reg.horblock
			= GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL16;
		gcmobltconfig->multisource.reg.verblock
			= GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE64;
	} else {
		gcmobltconfig->multisource.reg.horblock
			= GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128;
		gcmobltconfig->multisource.reg.verblock
			= GCREG_DE_MULTI_SOURCE_VERTICAL_BLOCK_LINE1;
	}

	/* Set destination configuration. */
	GCDBG(GCZONE_BLIT, "format entry = 0x%08X\n",
	      (unsigned int) dstinfo->format);
	GCDBG(GCZONE_BLIT, "  swizzle code = %d\n",
	      dstinfo->format->swizzle);
	GCDBG(GCZONE_BLIT, "  format code = %d\n",
	      dstinfo->format->format);

	gcmobltconfig->dstconfig_ldst = gcmobltconfig_dstconfig_ldst;
	gcmobltconfig->dstconfig.raw = 0;
	gcmobltconfig->dstconfig.reg.swizzle = dstinfo->format->swizzle;
	gcmobltconfig->dstconfig.reg.format = dstinfo->format->format;
	gcmobltconfig->dstconfig.reg.command = gcblit->multisrc
		? GCREG_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT
		: GCREG_DEST_CONFIG_COMMAND_BIT_BLT;

	/* Set ROP. */
	gcmobltconfig->rop_ldst = gcmobltconfig_rop_ldst;
	gcmobltconfig->rop.raw = 0;
	gcmobltconfig->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmobltconfig->rop.reg.fg = (unsigned char) gcblit->rop;

	/***********************************************************************
	 * Start the operation.
	 */

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmostartde),
			       (void **) &gcmostartde);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Set START_DE command. */
	gcmostartde->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmostartde->rect.left = gcblit->dstrect.left;
	gcmostartde->rect.top = gcblit->dstrect.top;
	gcmostartde->rect.right = gcblit->dstrect.right;
	gcmostartde->rect.bottom = gcblit->dstrect.bottom;

	GCDBG(GCZONE_BLIT, "dstrect = (%d,%d)-(%d,%d)\n",
	      gcmostartde->rect.left, gcmostartde->rect.top,
	      gcmostartde->rect.right, gcmostartde->rect.bottom);

	/* Reset the finalizer. */
	batch->batchend = do_end;

	gc_debug_blt(gcblit->srccount,
		     abs(gcblit->dstrect.right - gcblit->dstrect.left),
		     abs(gcblit->dstrect.bottom - gcblit->dstrect.top));

exit:
	GCEXITARG(GCZONE_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror do_blit(struct bvbltparams *bvbltparams,
		     struct gcbatch *batch,
		     struct surfaceinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();

	struct gcmosrc *gcmosrc;
	struct gcmoxsrcalpha *gcmoxsrcalpha;

	unsigned int index;
	struct bvbuffmap *dstmap = NULL;
	struct bvbuffmap *srcmap = NULL;

	struct surfaceinfo *dstinfo;
	int dstshiftX, dstshiftY;
	int dstalign, dstbyteshift;

	int srcshiftX, srcshiftY;
	int srcalign, srcbyteshift;

	int srcleft, srctop;
	struct gcrect dstadjusted;
	int srcsurfwidth, srcsurfheight;
	unsigned int physwidth, physheight;
	int orthogonal;
	int multisrc;

	GCENTER(GCZONE_BLIT);

	/* Get a shortcut to the destination surface. */
	dstinfo = &batch->dstinfo;

	/***********************************************************************
	 * Determine source surface alignment offset.
	 */

	/* Determine whether the source and the destination are orthogonal
	 * to each other. */
	orthogonal = (srcinfo->angle % 2) != (dstinfo->angle % 2);

	/* Compute adjusted destination rectangle. */
	dstadjusted.left = batch->dstclipped.left + batch->dstoffsetX;
	dstadjusted.top = batch->dstclipped.top + batch->dstoffsetY;
	dstadjusted.right = batch->dstclipped.right + batch->dstoffsetX;
	dstadjusted.bottom = batch->dstclipped.bottom + batch->dstoffsetY;

	/* Compute clipped source origin. */
	srcleft = srcinfo->rect.left + batch->clipdelta.left;
	srctop = srcinfo->rect.top + batch->clipdelta.top;

	GCDBG(GCZONE_SURF, "adjusted dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		dstadjusted.left, dstadjusted.top,
		dstadjusted.right, dstadjusted.bottom,
		dstadjusted.right - dstadjusted.left,
		dstadjusted.bottom - dstadjusted.top);

	/* Compute the source surface shift. */
	switch (srcinfo->angle) {
	case ROT_ANGLE_0:
		srcshiftX = srcleft - dstadjusted.left;
		srcshiftY = srctop - dstadjusted.top;
		break;

	case ROT_ANGLE_90:
		srcshiftX = srctop - dstadjusted.top;
		srcshiftY = (srcinfo->geom->width - srcleft)
			  - (batch->dstwidth - dstadjusted.left);
		break;

	case ROT_ANGLE_180:
		srcshiftX = (srcinfo->geom->width - srcleft)
			  - (batch->dstwidth - dstadjusted.left);
		srcshiftY = (srcinfo->geom->height - srctop)
			  - (batch->dstheight - dstadjusted.top);
		break;

	case ROT_ANGLE_270:
		srcshiftX = (srcinfo->geom->height - srctop)
			  - (batch->dstheight - dstadjusted.top);
		srcshiftY = srcleft - dstadjusted.left;
		break;

	default:
		srcshiftX = 0;
		srcshiftY = 0;
	}

	/* Compute the source surface offset in bytes. */
	srcbyteshift = srcshiftY * (int) srcinfo->geom->virtstride
		     + srcshiftX * (int) srcinfo->format->bitspp / 8;

	/* Compute the source offset in pixels needed to compensate
	 * for the surface base address misalignment if any. */
	srcalign = get_pixel_offset(srcinfo, srcbyteshift);

	GCDBG(GCZONE_SURF, "source surface %d:\n", srcinfo->index + 1);
	GCDBG(GCZONE_SURF, "  surface offset (pixels) = %d,%d\n",
	      srcshiftX, srcshiftY);
	GCDBG(GCZONE_SURF, "  surface offset (bytes) = 0x%08X\n",
	      srcbyteshift);
	GCDBG(GCZONE_SURF, "  srcalign = %d\n",
	      srcalign);

	/* Apply the source alignment. */
	srcbyteshift += srcalign * (int) srcinfo->format->bitspp / 8;
	srcshiftX += srcalign;

	GCDBG(GCZONE_SURF, "  adjusted surface offset (pixels) = %d,%d\n",
	      srcshiftX, srcshiftY);
	GCDBG(GCZONE_SURF, "  adjusted surface offset (bytes) = 0x%08X\n",
	      srcbyteshift);

	/* Determine the destination surface shift. */
	dstshiftX = dstinfo->pixalign;
	dstshiftY = (((srcinfo->angle + 3) % 4) == dstinfo->angle)
			? srcalign : 0;

	/* Compute the destination surface offset in bytes. */
	dstbyteshift = dstshiftY * (int) dstinfo->geom->virtstride
		     + dstshiftX * (int) dstinfo->format->bitspp / 8;

	/* Compute the destination offset in pixels needed to compensate
		* for the surface base address misalignment if any. */
	dstalign = get_pixel_offset(dstinfo, dstbyteshift);

	GCDBG(GCZONE_SURF, "destination surface:\n");
	GCDBG(GCZONE_SURF, "  surface offset (pixels) = %d,%d\n",
	      dstshiftX, dstshiftY);
	GCDBG(GCZONE_SURF, "  surface offset (bytes) = 0x%08X\n",
	      dstbyteshift);
	GCDBG(GCZONE_SURF, "  realignment = %d\n",
	      dstalign);

	if ((srcinfo->format->format == GCREG_DE_FORMAT_NV12) ||
	    (dstalign != 0) ||
	    ((srcalign != 0) && (srcinfo->angle == dstinfo->angle))) {
		/* Compute the source offset in pixels needed to compensate
		 * for the surface base address misalignment if any. */
		srcalign = get_pixel_offset(srcinfo, 0);

		/* Compute the surface offsets in bytes. */
		srcbyteshift = srcalign * (int) srcinfo->format->bitspp / 8;

		GCDBG(GCZONE_SURF, "recomputed for single-source setup:\n");
		GCDBG(GCZONE_SURF, "  srcalign = %d\n",
		      srcalign);
		GCDBG(GCZONE_SURF, "  srcsurf offset (bytes) = 0x%08X\n",
		      srcbyteshift);
		GCDBG(GCZONE_SURF, "  dstsurf offset (bytes) = 0x%08X\n",
		      dstinfo->bytealign);

		switch (srcinfo->angle) {
		case ROT_ANGLE_0:
			/* Adjust left coordinate. */
			srcleft -= srcalign;

			/* Determine source size. */
			srcsurfwidth = srcinfo->geom->width - srcalign;
			srcsurfheight = srcinfo->geom->height;
			break;

		case ROT_ANGLE_90:
			/* Adjust top coordinate. */
			srctop -= srcalign;

			/* Determine source size. */
			srcsurfwidth = srcinfo->geom->height - srcalign;
			srcsurfheight = srcinfo->geom->width;
			break;

		case ROT_ANGLE_180:
			/* Determine source size. */
			srcsurfwidth = srcinfo->geom->width - srcalign;
			srcsurfheight = srcinfo->geom->height;
			break;

		case ROT_ANGLE_270:
			/* Determine source size. */
			srcsurfwidth = srcinfo->geom->height - srcalign;
			srcsurfheight = srcinfo->geom->width;
			break;

		default:
			srcsurfwidth = 0;
			srcsurfheight = 0;
		}

		GCDBG(GCZONE_SURF, "srcrect origin = %d,%d\n",
		      srcleft, srctop);
		GCDBG(GCZONE_SURF, "source physical size = %dx%d\n",
		      srcsurfwidth, srcsurfheight);

		/* Set the physical destination size. */
		physwidth = dstinfo->physwidth;
		physheight = dstinfo->physheight;

		/* Disable multi source for YUV and for the cases where
		 * the destination and the base address alignment does
		 * not match. */
		multisrc = 0;
		GCDBG(GCZONE_SURF, "multi-source disabled.\n");
	} else {
		/* Source origin is not used in multi-source setup. */
		srcleft = 0;
		srctop = 0;

		/* Adjust the destination to match the source geometry. */
		switch (srcinfo->angle) {
		case ROT_ANGLE_0:
			dstadjusted.left -= srcalign;
			dstadjusted.right -= srcalign;

			/* Apply the source alignment. */
			if ((dstinfo->angle == ROT_ANGLE_0) ||
			    (dstinfo->angle == ROT_ANGLE_180)) {
				physwidth = dstinfo->physwidth - srcalign;
				physheight = dstinfo->physheight;
			} else {
				physwidth = dstinfo->physwidth;
				physheight = dstinfo->physheight - srcalign;
			}
			break;

		case ROT_ANGLE_90:
			dstadjusted.top -= srcalign;
			dstadjusted.bottom -= srcalign;

			/* Apply the source alignment. */
			if ((dstinfo->angle == ROT_ANGLE_0) ||
			    (dstinfo->angle == ROT_ANGLE_180)) {
				physwidth = dstinfo->physwidth;
				physheight = dstinfo->physheight - srcalign;
			} else {
				physwidth = dstinfo->physwidth - srcalign;
				physheight = dstinfo->physheight;
			}
			break;

		case ROT_ANGLE_180:
			/* Apply the source alignment. */
			if ((dstinfo->angle == ROT_ANGLE_0) ||
			    (dstinfo->angle == ROT_ANGLE_180)) {
				physwidth = dstinfo->physwidth - srcalign;
				physheight = dstinfo->physheight;
			} else {
				physwidth = dstinfo->physwidth;
				physheight = dstinfo->physheight - srcalign;
			}
			break;

		case ROT_ANGLE_270:
			/* Apply the source alignment. */
			if ((dstinfo->angle == ROT_ANGLE_0) ||
			    (dstinfo->angle == ROT_ANGLE_180)) {
				physwidth = dstinfo->physwidth;
				physheight = dstinfo->physheight - srcalign;
			} else {
				physwidth = dstinfo->physwidth - srcalign;
				physheight = dstinfo->physheight;
			}
			break;

		default:
			physwidth = 0;
			physheight = 0;
		}

		/* Source geometry is now the same as the destination. */
		if (orthogonal) {
			srcsurfwidth = physheight;
			srcsurfheight = physwidth;
		} else {
			srcsurfwidth = physwidth;
			srcsurfheight = physheight;
		}

		/* Enable multi-source. */
		multisrc = 1;
		GCDBG(GCZONE_SURF, "multi-source enabled.\n");
	}

	/* Verify if the destination has been modified. */
	if ((batch->dstbyteshift != dstbyteshift) ||
	    (batch->physwidth != physwidth) ||
	    (batch->physheight != physheight)) {
		/* Set new values. */
		batch->dstbyteshift = dstbyteshift;
		batch->physwidth = physwidth;
		batch->physheight = physheight;

		/* Mark as modified. */
		batch->batchflags |= BVBATCH_DST;
	}

	/* Check if we need to finalize existing batch. */
	if ((batch->batchend != do_blit_end) ||
	    (batch->op.blit.srccount == 4) ||
	    (batch->op.blit.multisrc == 0) ||
	    (multisrc == 0) ||
	    ((batch->batchflags & (BVBATCH_DST |
				   BVBATCH_CLIPRECT |
				   BVBATCH_DESTRECT)) != 0)) {
		/* Finalize existing batch if any. */
		bverror = batch->batchend(bvbltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Initialize the new batch. */
		batch->batchend = do_blit_end;
		batch->blockenable = 0;
		batch->op.blit.srccount = 0;
		batch->op.blit.multisrc = multisrc;
		batch->op.blit.rop = srcinfo->rop;
	}

	/* Set destination coordinates. */
	batch->op.blit.dstrect = dstadjusted;

	/* Map the destination. */
	bverror = do_map(dstinfo->buf.desc, batch, &dstmap);
	if (bverror != BVERR_NONE) {
		bvbltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/* Set the new destination. */
	bverror = set_dst(bvbltparams, batch, dstmap);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Reset the modified flag. */
	batch->batchflags &= ~(BVBATCH_DST |
			       BVBATCH_CLIPRECT |
			       BVBATCH_DESTRECT);

	/* Map the source. */
	bverror = do_map(srcinfo->buf.desc, batch, &srcmap);
	if (bverror != BVERR_NONE) {
		bvbltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/***************************************************************
	** Configure source.
	*/

	/* We need to walk in blocks if the source and the destination
	 * surfaces are orthogonal to each other. */
	batch->blockenable = orthogonal;

	/* Allocate command buffer. */
	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmosrc),
			       (void **) &gcmosrc);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Shortcut to the register index. */
	index = batch->op.blit.srccount;

	/* Set surface parameters. */
	gcmosrc->address_ldst = gcmosrc_address_ldst[index];
	gcmosrc->address = GET_MAP_HANDLE(srcmap);
	add_fixup(bvbltparams, batch, &gcmosrc->address, srcbyteshift);

	gcmosrc->stride_ldst = gcmosrc_stride_ldst[index];
	gcmosrc->stride = srcinfo->geom->virtstride;

	gcmosrc->rotation_ldst = gcmosrc_rotation_ldst[index];
	gcmosrc->rotation.raw = 0;
	gcmosrc->rotation.reg.surf_width = srcsurfwidth;

	gcmosrc->config_ldst = gcmosrc_config_ldst[index];
	gcmosrc->config.raw = 0;
	gcmosrc->config.reg.swizzle = srcinfo->format->swizzle;
	gcmosrc->config.reg.format = srcinfo->format->format;

	gcmosrc->origin_ldst = gcmosrc_origin_ldst[index];
	gcmosrc->origin.reg.x = srcleft;
	gcmosrc->origin.reg.y = srctop;

	gcmosrc->size_ldst = gcmosrc_size_ldst[index];
	gcmosrc->size.reg = gcregsrcsize_max;

	gcmosrc->rotationheight_ldst
		= gcmosrc_rotationheight_ldst[index];
	gcmosrc->rotationheight.reg.height = srcsurfheight;

	gcmosrc->rotationangle_ldst
		= gcmosrc_rotationangle_ldst[index];
	gcmosrc->rotationangle.raw = 0;
	gcmosrc->rotationangle.reg.src = rotencoding[srcinfo->angle];
	gcmosrc->rotationangle.reg.dst = rotencoding[dstinfo->angle];
	gcmosrc->rotationangle.reg.src_mirror = srcinfo->mirror;
	gcmosrc->rotationangle.reg.dst_mirror = GCREG_MIRROR_NONE;

	gcmosrc->rop_ldst = gcmosrc_rop_ldst[index];
	gcmosrc->rop.raw = 0;
	gcmosrc->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmosrc->rop.reg.fg = (unsigned char) batch->op.blit.rop;

	gcmosrc->mult_ldst = gcmosrc_mult_ldst[index];
	gcmosrc->mult.raw = 0;
	gcmosrc->mult.reg.srcglobalpremul
	= GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE;

	if ((srcinfo->geom->format & OCDFMTDEF_NON_PREMULT) != 0)
		gcmosrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;
	else
		gcmosrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;

	if ((dstinfo->geom->format & OCDFMTDEF_NON_PREMULT) != 0) {
		gcmosrc->mult.reg.dstpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;

		gcmosrc->mult.reg.dstdemul
		= GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_ENABLE;
	} else {
		gcmosrc->mult.reg.dstpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;

		gcmosrc->mult.reg.dstdemul
		= GCREG_COLOR_MULTIPLY_MODES_DST_DEMULTIPLY_DISABLE;
	}

	if (srcinfo->gca == NULL) {
		GCDBG(GCZONE_BLEND, "blending disabled.\n");
		gcmosrc->alphacontrol_ldst
			= gcmosrc_alphacontrol_ldst[index];
		gcmosrc->alphacontrol.reg = gcregalpha_off;
	} else {
		gcmosrc->alphacontrol_ldst
			= gcmosrc_alphacontrol_ldst[index];
		gcmosrc->alphacontrol.reg = gcregalpha_on;

		/* Allocate command buffer. */
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoxsrcalpha),
				       (void **) &gcmoxsrcalpha);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoxsrcalpha->alphamodes_ldst
			= gcmoxsrcalpha_alphamodes_ldst[index];
		gcmoxsrcalpha->alphamodes.raw = 0;
		gcmoxsrcalpha->alphamodes.reg.src_global_alpha_mode
			= srcinfo->gca->src_global_alpha_mode;
		gcmoxsrcalpha->alphamodes.reg.dst_global_alpha_mode
			= srcinfo->gca->dst_global_alpha_mode;

		gcmoxsrcalpha->alphamodes.reg.src_blend
			= srcinfo->gca->srcconfig->factor_mode;
		gcmoxsrcalpha->alphamodes.reg.src_color_reverse
			= srcinfo->gca->srcconfig->color_reverse;

		gcmoxsrcalpha->alphamodes.reg.dst_blend
			= srcinfo->gca->dstconfig->factor_mode;
		gcmoxsrcalpha->alphamodes.reg.dst_color_reverse
			= srcinfo->gca->dstconfig->color_reverse;

		GCDBG(GCZONE_BLEND, "dst blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmoxsrcalpha->alphamodes.reg.dst_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmoxsrcalpha->alphamodes.reg.dst_color_reverse);

		GCDBG(GCZONE_BLEND, "src blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmoxsrcalpha->alphamodes.reg.src_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmoxsrcalpha->alphamodes.reg.src_color_reverse);

		gcmoxsrcalpha->srcglobal_ldst
			= gcmoxsrcalpha_srcglobal_ldst[index];
		gcmoxsrcalpha->srcglobal.raw = srcinfo->gca->src_global_color;

		gcmoxsrcalpha->dstglobal_ldst
			= gcmoxsrcalpha_dstglobal_ldst[index];
		gcmoxsrcalpha->dstglobal.raw = srcinfo->gca->dst_global_color;
	}

	if (srcinfo->format->format == GCREG_DE_FORMAT_NV12) {
		struct gcmoxsrcyuv *gcmoxsrcyuv;
		int uvshift;

		uvshift = srcbyteshift
			+ srcinfo->geom->virtstride
			* srcinfo->geom->height;
		GCDBG(GCZONE_SURF, "  uvshift = 0x%08X (%d)\n",
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

	batch->op.blit.srccount += 1;

exit:
	GCEXITARG(GCZONE_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}
