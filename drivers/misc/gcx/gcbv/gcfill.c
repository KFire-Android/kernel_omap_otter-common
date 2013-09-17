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
#define GCZONE_COLOR		(1 << 0)
#define GCZONE_FILL		(1 << 1)

GCDBG_FILTERDEF(fill, GCZONE_NONE,
		"color",
		"fill")


static inline unsigned int extract_component(unsigned int pixel,
					     const struct bvcomponent *desc,
					     bool zerofill)
{
	unsigned int component;
	unsigned int component8;

	component = (pixel & desc->mask) >> desc->shift;
	GCDBG(GCZONE_COLOR, "mask=0x%08X, shift=%d, component=0x%08X\n",
		desc->mask, desc->shift, component);

	switch (desc->size) {
	case 0:
		component8 = zerofill ? 0x00 : 0xFF;
		GCDBG(GCZONE_COLOR, "component8=0x%08X\n", component8);
		break;

	case 1:
		component8 = component ? 0xFF : 0x00;
		GCDBG(GCZONE_COLOR, "component8=0x%08X\n", component8);
		break;

	case 4:
		component8 = component | (component << 4);
		GCDBG(GCZONE_COLOR, "component8=0x%08X\n", component8);
		break;

	case 5:
		component8 = (component << 3) | (component >> 2);
		GCDBG(GCZONE_COLOR, "component8=0x%08X\n", component8);
		break;

	case 6:
		component8 = (component << 2) | (component >> 4);
		GCDBG(GCZONE_COLOR, "component8=0x%08X\n", component8);
		break;

	default:
		component8 = component;
		GCDBG(GCZONE_COLOR, "component8=0x%08X\n", component8);
	}

	return component8;
}

static unsigned int getinternalcolor(void *ptr, struct bvformatxlate *format)
{
	unsigned int srcpixel, dstpixel;
	unsigned int r, g, b, a;

	switch (format->bitspp) {
	case 16:
		srcpixel = *(unsigned short *) ptr;
		GCDBG(GCZONE_COLOR, "srcpixel=0x%08X\n", srcpixel);
		break;

	case 32:
		srcpixel = *(unsigned int *) ptr;
		GCDBG(GCZONE_COLOR, "srcpixel=0x%08X\n", srcpixel);
		break;

	default:
		srcpixel = 0;
		GCDBG(GCZONE_COLOR, "srcpixel=0x%08X\n", srcpixel);
	}

	r = extract_component(srcpixel, &format->cs.rgb.comp->r,
			      format->zerofill);
	g = extract_component(srcpixel, &format->cs.rgb.comp->g,
			      format->zerofill);
	b = extract_component(srcpixel, &format->cs.rgb.comp->b,
			      format->zerofill);
	a = extract_component(srcpixel, &format->cs.rgb.comp->a,
			      format->zerofill);

	GCDBG(GCZONE_COLOR, "(r,g,b,a)=0x%02X,0x%02X,0x%02X,0x%02X\n",
	      r, g, b, a);

	dstpixel = (a << 24) | (r << 16) | (g <<  8) | b;

	GCDBG(GCZONE_COLOR, "dstpixel=0x%08X\n", dstpixel);

	return dstpixel;
}

enum bverror do_fill(struct bvbltparams *bvbltparams,
		     struct gcbatch *batch,
		     struct gcsurface *srcinfo)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gcsurface *dstinfo;
	struct gcmofill *gcmofill;
	unsigned char *fillcolorptr;
	struct bvbuffmap *dstmap = NULL;
	int physleft, phystop;
	struct gcrect *srcorig;
	struct gcrect *dstadj;

	GCENTER(GCZONE_FILL);

	/* Get a shortcut to the destination surface descriptor. */
	dstinfo = &batch->dstinfo;

	/* Only RGB source is supported. */
	if (srcinfo->format.type != BVFMT_RGB) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM_FORMAT
					: BVERR_SRC2GEOM_FORMAT,
			      "only RGB source is supported for fill.");
		goto exit;
	}

	/* Finish previous batch if any. */
	bverror = batch->batchend(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Parse destination parameters. */
	bverror = parse_destination(bvbltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Ignore the blit if destination rectangle is empty. */
	if (null_rect(&dstinfo->rect.clip)) {
		GCDBG(GCZONE_FILL, "empty destination rectangle.\n");
		goto exit;
	}

	/* Map the destination. */
	bverror = do_map(bvbltparams->dstdesc, batch, &dstmap);
	if (bverror != BVERR_NONE) {
		bvbltparams->errdesc = gccontext->bverrorstr;
		goto exit;
	}

	/* Set the new destination. */
	bverror = set_dst(bvbltparams, batch, dstmap);
	if (bverror != BVERR_NONE)
		goto exit;

	/***********************************************************************
	** Allocate command buffer.
	*/

	bverror = claim_buffer(bvbltparams, batch,
			       sizeof(struct gcmofill),
			       (void **) &gcmofill);
	if (bverror != BVERR_NONE)
		goto exit;

	/***********************************************************************
	** Set dummy source.
	*/

	/* Set surface dummy width and height. */
	gcmofill->src.rotation_ldst = gcmofillsrc_rotation_ldst;
	gcmofill->src.rotation.raw = 0;
	gcmofill->src.rotation.reg.surf_width = 1;
	gcmofill->src.config.raw = 0;

	gcmofill->src.rotationheight_ldst = gcmofillsrc_rotationheight_ldst;
	gcmofill->src.rotationheight.reg.height = 1;
	gcmofill->src.rotationangle.raw = 0;
	gcmofill->src.rotationangle.reg.dst = rotencoding[dstinfo->angle];
	gcmofill->src.rotationangle.reg.dst_mirror = GCREG_MIRROR_NONE;

	/* Disable alpha blending. */
	gcmofill->src.alphacontrol_ldst = gcmofillsrc_alphacontrol_ldst;
	gcmofill->src.alphacontrol.raw = 0;
	gcmofill->src.alphacontrol.reg.enable = GCREG_ALPHA_CONTROL_ENABLE_OFF;

	/***********************************************************************
	** Set fill color.
	*/

	/* Get source rectangle shortcut. */
	srcorig = &srcinfo->rect.orig;

	switch (srcinfo->angle) {
	case ROT_ANGLE_0:
		physleft = srcorig->left;
		phystop  = srcorig->top;
		break;

	case ROT_ANGLE_90:
		physleft = srcorig->top;
		phystop  = srcinfo->width - srcorig->left - 1;
		break;

	case ROT_ANGLE_180:
		physleft = srcinfo->width  - srcorig->left - 1;
		phystop  = srcinfo->height - srcorig->top  - 1;
		break;

	case ROT_ANGLE_270:
		physleft = srcinfo->height - srcorig->top  - 1;
		phystop  = srcorig->left;
		break;

	default:
		physleft = 0;
		phystop  = 0;
		GCERR("invalid source angle %d.\n", srcinfo->angle);
	}

	fillcolorptr
		= (unsigned char *) srcinfo->buf.desc->virtaddr
		+ phystop * srcinfo->stride1
		+ physleft * srcinfo->format.bitspp / 8;

	gcmofill->clearcolor_ldst = gcmofill_clearcolor_ldst;
	gcmofill->clearcolor.raw = getinternalcolor(fillcolorptr,
						    &srcinfo->format);

	/***********************************************************************
	** Configure and start fill.
	*/

	/* Set destination configuration. */
	gcmofill->dstconfig_ldst = gcmofill_dstconfig_ldst;
	gcmofill->dstconfig.raw = 0;
	gcmofill->dstconfig.reg.swizzle = dstinfo->format.swizzle;
	gcmofill->dstconfig.reg.format = dstinfo->format.format;
	gcmofill->dstconfig.reg.endian = dstinfo->format.endian;
	gcmofill->dstconfig.reg.command = GCREG_DEST_CONFIG_COMMAND_CLEAR;

	/* Set ROP3. */
	gcmofill->rop_ldst = gcmofill_rop_ldst;
	gcmofill->rop.raw = 0;
	gcmofill->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmofill->rop.reg.fg = srcinfo->rop;

	/* Set START_DE command. */
	gcmofill->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	dstadj = &dstinfo->rect.adj;
	gcmofill->rect.left = dstadj->left;
	gcmofill->rect.top = dstadj->top;
	gcmofill->rect.right = dstadj->right;
	gcmofill->rect.bottom = dstadj->bottom;

exit:
	GCEXITARG(GCZONE_FILL, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}
