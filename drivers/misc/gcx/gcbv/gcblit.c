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


static enum bverror do_blit_end(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcmobltconfig *gcmobltconfig;
	struct gcmostart *gcmostart;

	GCENTER(GCZONE_BLIT);

	GCDBG(GCZONE_BLIT, "finalizing the blit, scrcount = %d\n",
	      batch->gcblit.srccount);

	/***********************************************************************
	 * Configure the operation.
	 */

	/* Allocate command buffer. */
	bverror = claim_buffer(bltparams, batch,
			       sizeof(struct gcmobltconfig),
			       (void **) &gcmobltconfig);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Configure multi-source control. */
	gcmobltconfig->multisource_ldst = gcmobltconfig_multisource_ldst;
	gcmobltconfig->multisource.raw = 0;
	gcmobltconfig->multisource.reg.srccount = batch->gcblit.srccount - 1;

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
	      (unsigned int) batch->dstformat);
	GCDBG(GCZONE_BLIT, "  swizzle code = %d\n",
	      batch->dstformat->swizzle);
	GCDBG(GCZONE_BLIT, "  format code = %d\n",
	      batch->dstformat->format);

	gcmobltconfig->dstconfig_ldst = gcmobltconfig_dstconfig_ldst;
	gcmobltconfig->dstconfig.raw = 0;
	gcmobltconfig->dstconfig.reg.swizzle = batch->dstformat->swizzle;
	gcmobltconfig->dstconfig.reg.format = batch->dstformat->format;
	gcmobltconfig->dstconfig.reg.command = batch->gcblit.multisrc
		? GCREG_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT
		: GCREG_DEST_CONFIG_COMMAND_BIT_BLT;

	/* Set ROP. */
	gcmobltconfig->rop_ldst = gcmobltconfig_rop_ldst;
	gcmobltconfig->rop.raw = 0;
	gcmobltconfig->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmobltconfig->rop.reg.fg = (unsigned char) batch->gcblit.rop;

	/***********************************************************************
	 * Start the operation.
	 */

	/* Allocate command buffer. */
	bverror = claim_buffer(bltparams, batch,
			       sizeof(struct gcmostart),
			       (void **) &gcmostart);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Set START_DE command. */
	gcmostart->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmostart->rect.left = batch->left;
	gcmostart->rect.top = batch->top;
	gcmostart->rect.right = batch->right;
	gcmostart->rect.bottom = batch->bottom;

	GCDBG(GCZONE_BLIT, "dstrect = (%d,%d)-(%d,%d)\n",
	      gcmostart->rect.left, gcmostart->rect.top,
	      gcmostart->rect.right, gcmostart->rect.bottom);

	/* Reset the finalizer. */
	batch->batchend = do_end;

	gc_debug_blt(batch->gcblit.srccount,
		     abs(batch->right - batch->left),
		     abs(batch->bottom - batch->top));

exit:
	GCEXITARG(GCZONE_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror do_blit(struct bvbltparams *bltparams,
		     struct gcbatch *batch,
		     struct srcinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();

	struct gcmosrc *gcmosrc;
	struct gcmosrcalpha *gcmosrcalpha;

	unsigned int index;

	struct bvbuffmap *dstmap;
	struct bvbuffdesc *dstdesc;
	struct bvsurfgeom *dstgeom;
	struct bvformatxlate *dstformat;
	int dstshiftX, dstshiftY;
	int dstalign, dstbyteshift;

	struct bvbuffmap *srcmap;
	struct gcalpha *srcgca;
	struct bvrect *srcrect;
	struct bvbuffdesc *srcdesc;
	struct bvsurfgeom *srcgeom;
	struct bvformatxlate *srcformat;
	int srcshiftX, srcshiftY;
	int srcalign, srcbyteshift;

	int srcleft, srctop;
	int dstleft, dsttop, dstright, dstbottom;
	int srcsurfwidth, srcsurfheight;
	unsigned int physwidth, physheight;
	int orthogonal;
	int multisrc;

	GCENTER(GCZONE_BLIT);

	/* Create source object shortcuts. */
	srcmap = NULL;
	srcgca = srcinfo->gca;
	srcrect = srcinfo->rect;
	srcdesc = srcinfo->buf.desc;
	srcgeom = srcinfo->geom;
	srcformat = srcinfo->format;

	/* Create destination object shortcuts. */
	dstmap = NULL;
	dstdesc = bltparams->dstdesc;
	dstgeom = bltparams->dstgeom;
	dstformat = batch->dstformat;

	/***********************************************************************
	 * Determine source surface alignment offset.
	 */

	/* Determine whether the source and the destination are orthogonal
	 * to each other. */
	orthogonal = (srcinfo->angle % 2) != (batch->dstangle % 2);

	/* Compute adjusted destination rectangle. */
	dstleft = batch->clippedleft + batch->dstoffsetX;
	dsttop = batch->clippedtop + batch->dstoffsetY;
	dstright = batch->clippedright + batch->dstoffsetX;
	dstbottom = batch->clippedbottom + batch->dstoffsetY;

	/* Compute clipped source origin. */
	srcleft = srcrect->left + batch->deltaleft;
	srctop = srcrect->top + batch->deltatop;

	GCDBG(GCZONE_SURF, "adjusted dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		dstleft, dsttop, dstright, dstbottom,
		dstright - dstleft, dstbottom - dsttop);

	/* Compute the source surface shift. */
	switch (srcinfo->angle) {
	case ROT_ANGLE_0:
		srcshiftX = srcleft - dstleft;
		srcshiftY = srctop - dsttop;
		break;

	case ROT_ANGLE_90:
		srcshiftX = srctop - dsttop;
		srcshiftY = (srcgeom->width - srcleft)
			  - (batch->dstwidth - dstleft);
		break;

	case ROT_ANGLE_180:
		srcshiftX = (srcgeom->width - srcleft)
			  - (batch->dstwidth - dstleft);
		srcshiftY = (srcgeom->height - srctop)
			  - (batch->dstheight - dsttop);
		break;

	case ROT_ANGLE_270:
		srcshiftX = (srcgeom->height - srctop)
			  - (batch->dstheight - dsttop);
		srcshiftY = srcleft - dstleft;
		break;

	default:
		srcshiftX = 0;
		srcshiftY = 0;
	}

	/* Compute the source surface offset in bytes. */
	srcbyteshift = srcshiftY * (int) srcgeom->virtstride
		     + srcshiftX * (int) srcformat->bitspp / 8;

	/* Compute the source offset in pixels needed to compensate
	 * for the surface base address misalignment if any. */
	srcalign = get_pixel_offset(srcdesc, srcformat, srcbyteshift);

	GCDBG(GCZONE_SURF, "source surface %d:\n", srcinfo->index + 1);
	GCDBG(GCZONE_SURF, "  surface offset (pixels) = %d,%d\n",
	      srcshiftX, srcshiftY);
	GCDBG(GCZONE_SURF, "  surface offset (bytes) = 0x%08X\n",
	      srcbyteshift);
	GCDBG(GCZONE_SURF, "  srcalign = %d\n",
	      srcalign);

	/* Apply the source alignment. */
	srcbyteshift += srcalign * (int) srcformat->bitspp / 8;
	srcshiftX += srcalign;

	GCDBG(GCZONE_SURF, "  adjusted surface offset (pixels) = %d,%d\n",
	      srcshiftX, srcshiftY);
	GCDBG(GCZONE_SURF, "  adjusted surface offset (bytes) = 0x%08X\n",
	      srcbyteshift);

	/* Determine the destination surface shift. */
	dstshiftX = batch->dstalign;
	dstshiftY = (((srcinfo->angle + 3) % 4) == batch->dstangle)
			? srcalign : 0;

	/* Compute the destination surface offset in bytes. */
	dstbyteshift = dstshiftY * (int) dstgeom->virtstride
		     + dstshiftX * (int) dstformat->bitspp / 8;

	/* Compute the destination offset in pixels needed to compensate
		* for the surface base address misalignment if any. */
	dstalign = get_pixel_offset(dstdesc, dstformat, dstbyteshift);

	GCDBG(GCZONE_SURF, "destination surface:\n");
	GCDBG(GCZONE_SURF, "  surface offset (pixels) = %d,%d\n",
	      dstshiftX, dstshiftY);
	GCDBG(GCZONE_SURF, "  surface offset (bytes) = 0x%08X\n",
	      dstbyteshift);
	GCDBG(GCZONE_SURF, "  realignment = %d\n",
	      dstalign);

	if ((srcformat->format == GCREG_DE_FORMAT_NV12) ||
	    (dstalign != 0) ||
	    ((srcalign != 0) && (srcinfo->angle == batch->dstangle))) {
		/* Compute the source offset in pixels needed to compensate
		 * for the surface base address misalignment if any. */
		srcalign = get_pixel_offset(srcdesc, srcformat, 0);

		/* Compute the surface offsets in bytes. */
		srcbyteshift = srcalign * (int) srcformat->bitspp / 8;
		dstbyteshift = batch->dstalign * (int) dstformat->bitspp / 8;

		GCDBG(GCZONE_SURF, "recomputed for single-source setup:\n");
		GCDBG(GCZONE_SURF, "  srcalign = %d\n",
		      srcalign);
		GCDBG(GCZONE_SURF, "  srcsurf offset (bytes) = 0x%08X\n",
		      srcbyteshift);
		GCDBG(GCZONE_SURF, "  dstsurf offset (bytes) = 0x%08X\n",
		      dstbyteshift);

		switch (srcinfo->angle) {
		case ROT_ANGLE_0:
			/* Adjust left coordinate. */
			srcleft -= srcalign;

			/* Determine source size. */
			srcsurfwidth = srcgeom->width - srcalign;
			srcsurfheight = srcgeom->height;
			break;

		case ROT_ANGLE_90:
			/* Adjust top coordinate. */
			srctop -= srcalign;

			/* Determine source size. */
			srcsurfwidth = srcgeom->height - srcalign;
			srcsurfheight = srcgeom->width;
			break;

		case ROT_ANGLE_180:
			/* Determine source size. */
			srcsurfwidth = srcgeom->width - srcalign;
			srcsurfheight = srcgeom->height;
			break;

		case ROT_ANGLE_270:
			/* Determine source size. */
			srcsurfwidth = srcgeom->height - srcalign;
			srcsurfheight = srcgeom->width;
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
		physwidth = batch->dstphyswidth;
		physheight = batch->dstphysheight;

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
			dstleft -= srcalign;
			dstright -= srcalign;

			/* Apply the source alignment. */
			if ((batch->dstangle == ROT_ANGLE_0) ||
			    (batch->dstangle == ROT_ANGLE_180)) {
				physwidth = batch->dstphyswidth - srcalign;
				physheight = batch->dstphysheight;
			} else {
				physwidth = batch->dstphyswidth;
				physheight = batch->dstphysheight - srcalign;
			}
			break;

		case ROT_ANGLE_90:
			dsttop -= srcalign;
			dstbottom -= srcalign;

			/* Apply the source alignment. */
			if ((batch->dstangle == ROT_ANGLE_0) ||
			    (batch->dstangle == ROT_ANGLE_180)) {
				physwidth = batch->dstphyswidth;
				physheight = batch->dstphysheight - srcalign;
			} else {
				physwidth = batch->dstphyswidth - srcalign;
				physheight = batch->dstphysheight;
			}
			break;

		case ROT_ANGLE_180:
			/* Apply the source alignment. */
			if ((batch->dstangle == ROT_ANGLE_0) ||
			    (batch->dstangle == ROT_ANGLE_180)) {
				physwidth = batch->dstphyswidth - srcalign;
				physheight = batch->dstphysheight;
			} else {
				physwidth = batch->dstphyswidth;
				physheight = batch->dstphysheight - srcalign;
			}
			break;

		case ROT_ANGLE_270:
			/* Apply the source alignment. */
			if ((batch->dstangle == ROT_ANGLE_0) ||
			    (batch->dstangle == ROT_ANGLE_180)) {
				physwidth = batch->dstphyswidth;
				physheight = batch->dstphysheight - srcalign;
			} else {
				physwidth = batch->dstphyswidth - srcalign;
				physheight = batch->dstphysheight;
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
	    (batch->gcblit.srccount == 4) ||
	    (batch->gcblit.multisrc == 0) ||
	    (multisrc == 0) ||
	    ((batch->batchflags & (BVBATCH_DST |
				   BVBATCH_CLIPRECT |
				   BVBATCH_DESTRECT)) != 0)) {
		/* Finalize existing batch if any. */
		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Initialize the new batch. */
		batch->batchend = do_blit_end;
		batch->blockenable = 0;
		batch->gcblit.srccount = 0;
		batch->gcblit.multisrc = multisrc;
		batch->gcblit.rop = srcinfo->rop;
	}

	/* Set destination coordinates. */
	batch->left = dstleft;
	batch->top = dsttop;
	batch->right = dstright;
	batch->bottom = dstbottom;

	/* Map the destination. */
	bverror = do_map(dstdesc, batch, &dstmap);
	if (bverror != BVERR_NONE) {
		bltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/* Set the new destination. */
	bverror = set_dst(bltparams, batch, dstmap);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Reset the modified flag. */
	batch->batchflags &= ~(BVBATCH_DST |
			       BVBATCH_CLIPRECT |
			       BVBATCH_DESTRECT);

	/* Map the source. */
	bverror = do_map(srcdesc, batch, &srcmap);
	if (bverror != BVERR_NONE) {
		bltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/***************************************************************
	** Configure source.
	*/

	/* We need to walk in blocks if the source and the destination
	 * surfaces are orthogonal to each other. */
	batch->blockenable = orthogonal;

	/* Allocate command buffer. */
	bverror = claim_buffer(bltparams, batch,
			       sizeof(struct gcmosrc),
			       (void **) &gcmosrc);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Shortcut to the register index. */
	index = batch->gcblit.srccount;

	add_fixup(bltparams, batch, &gcmosrc->address, srcbyteshift);

	/* Set surface parameters. */
	gcmosrc->address_ldst = gcmosrc_address_ldst[index];
	gcmosrc->address = GET_MAP_HANDLE(srcmap);

	gcmosrc->stride_ldst = gcmosrc_stride_ldst[index];
	gcmosrc->stride = srcgeom->virtstride;

	gcmosrc->rotation_ldst = gcmosrc_rotation_ldst[index];
	gcmosrc->rotation.raw = 0;
	gcmosrc->rotation.reg.surf_width = srcsurfwidth;

	gcmosrc->config_ldst = gcmosrc_config_ldst[index];
	gcmosrc->config.raw = 0;
	gcmosrc->config.reg.swizzle = srcformat->swizzle;
	gcmosrc->config.reg.format = srcformat->format;

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
	gcmosrc->rotationangle.reg.dst = rotencoding[batch->dstangle];
	gcmosrc->rotationangle.reg.src_mirror = srcinfo->mirror;
	gcmosrc->rotationangle.reg.dst_mirror = GCREG_MIRROR_NONE;

	gcmosrc->rop_ldst = gcmosrc_rop_ldst[index];
	gcmosrc->rop.raw = 0;
	gcmosrc->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmosrc->rop.reg.fg = (unsigned char) batch->gcblit.rop;

	gcmosrc->mult_ldst = gcmosrc_mult_ldst[index];
	gcmosrc->mult.raw = 0;
	gcmosrc->mult.reg.srcglobalpremul
	= GCREG_COLOR_MULTIPLY_MODES_SRC_GLOBAL_PREMULTIPLY_DISABLE;

	if ((srcgeom->format & OCDFMTDEF_NON_PREMULT) != 0)
		gcmosrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_ENABLE;
	else
		gcmosrc->mult.reg.srcpremul
		= GCREG_COLOR_MULTIPLY_MODES_SRC_PREMULTIPLY_DISABLE;

	if ((dstgeom->format & OCDFMTDEF_NON_PREMULT) != 0) {
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

	if (srcformat->format == GCREG_DE_FORMAT_NV12) {
		struct gcmosrcplanaryuv *yuv;
		int uvshift = srcbyteshift;

#if 0
		/* TODO: needs rework */
		if (multisrc && (srcsurftop % 2)) {
			/* We can't shift the uv plane by an odd number
			 * of rows. */
			BVSETBLTERROR(BVERR_SRC1RECT,
				      "src/dst y coordinate combination"
				      " not supported");
			goto exit;
		}
#endif

		bverror = claim_buffer(bltparams, batch,
				       sizeof(struct gcmosrcplanaryuv),
				       (void **) &yuv);
		if (bverror != BVERR_NONE)
			goto exit;

		yuv->uplaneaddress_ldst =
			gcmosrc_uplaneaddress_ldst[index];
		yuv->uplanestride_ldst =
			gcmosrc_uplanestride_ldst[index];
		yuv->vplaneaddress_ldst =
			gcmosrc_vplaneaddress_ldst[index];
		yuv->vplanestride_ldst =
			gcmosrc_vplanestride_ldst[index];

#if 0
		/* TODO: needs rework */
		if (multisrc) {
			/* UV plane is half height. */
			uvshift = (srcsurftop / 2)
				* (int) srcgeom->virtstride
				+ srcsurfleft
				* (int) srcformat->bitspp / 8;
		} else {
			/* No shift needed for single source walker. */
			uvshift = 0;
		}
#endif

		GCDBG(GCZONE_SURF, "  uvshift = 0x%08X (%d)\n",
			uvshift, uvshift);

		/* add fixed offset from Y plane */
		uvshift += srcgeom->virtstride * srcgeom->height;

		GCDBG(GCZONE_SURF, "  final uvshift = 0x%08X (%d)\n",
			uvshift, uvshift);

		yuv->uplaneaddress = GET_MAP_HANDLE(srcmap);
		add_fixup(bltparams, batch, &yuv->uplaneaddress, uvshift);

		yuv->uplanestride = srcgeom->virtstride;

		yuv->vplaneaddress = GET_MAP_HANDLE(srcmap);
		add_fixup(bltparams, batch, &yuv->vplaneaddress, uvshift);

		yuv->vplanestride = srcgeom->virtstride;
	}

	if (srcgca != NULL) {
		gcmosrc->alphacontrol_ldst
			= gcmosrc_alphacontrol_ldst[index];
		gcmosrc->alphacontrol.raw = 0;
		gcmosrc->alphacontrol.reg.enable
			= GCREG_ALPHA_CONTROL_ENABLE_ON;

		/* Allocate command buffer. */
		bverror = claim_buffer(bltparams, batch,
				       sizeof(struct gcmosrcalpha),
				       (void **) &gcmosrcalpha);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmosrcalpha->alphamodes_ldst
			= gcmosrcalpha_alphamodes_ldst[index];
		gcmosrcalpha->alphamodes.raw = 0;
		gcmosrcalpha->alphamodes.reg.src_global_alpha
			= srcgca->src_global_alpha_mode;
		gcmosrcalpha->alphamodes.reg.dst_global_alpha
			= srcgca->dst_global_alpha_mode;

		gcmosrcalpha->alphamodes.reg.src_blend
			= srcgca->srcconfig->factor_mode;
		gcmosrcalpha->alphamodes.reg.src_color_reverse
			= srcgca->srcconfig->color_reverse;

		gcmosrcalpha->alphamodes.reg.dst_blend
			= srcgca->dstconfig->factor_mode;
		gcmosrcalpha->alphamodes.reg.dst_color_reverse
			= srcgca->dstconfig->color_reverse;

		GCDBG(GCZONE_BLEND, "dst blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmosrcalpha->alphamodes.reg.dst_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmosrcalpha->alphamodes.reg.dst_color_reverse);

		GCDBG(GCZONE_BLEND, "src blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmosrcalpha->alphamodes.reg.src_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmosrcalpha->alphamodes.reg.src_color_reverse);

		gcmosrcalpha->srcglobal_ldst
			= gcmosrcalpha_srcglobal_ldst[index];
		gcmosrcalpha->srcglobal.raw = srcgca->src_global_color;

		gcmosrcalpha->dstglobal_ldst
			= gcmosrcalpha_dstglobal_ldst[index];
		gcmosrcalpha->dstglobal.raw = srcgca->dst_global_color;
	} else {
		GCDBG(GCZONE_BLEND, "blending disabled.\n");

		gcmosrc->alphacontrol_ldst
			= gcmosrc_alphacontrol_ldst[index];
		gcmosrc->alphacontrol.raw = 0;
		gcmosrc->alphacontrol.reg.enable
			= GCREG_ALPHA_CONTROL_ENABLE_OFF;
	}

	batch->gcblit.srccount += 1;

exit:
	GCEXITARG(GCZONE_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}
