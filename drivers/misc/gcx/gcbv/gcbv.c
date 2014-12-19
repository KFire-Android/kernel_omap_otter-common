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
#define GCZONE_MAPPING		(1 << 0)
#define GCZONE_BUFFER		(1 << 1)
#define GCZONE_DEST		(1 << 2)
#define GCZONE_SRC		(1 << 3)
#define GCZONE_MASK		(1 << 4)
#define GCZONE_BATCH		(1 << 5)
#define GCZONE_BLIT		(1 << 6)
#define GCZONE_CACHE		(1 << 7)
#define GCZONE_CALLBACK		(1 << 8)
#define GCZONE_TEMP		(1 << 9)
#define GCZONE_BLEND		(1 << 10)

GCDBG_FILTERDEF(bv, GCZONE_NONE,
		"mapping",
		"buffer",
		"dest",
		"source",
		"mask",
		"batch",
		"blit",
		"cache",
		"callback",
		"tempbuffer",
		"blending")


/*******************************************************************************
** Global driver data access.
*/

struct gccontext *get_context(void)
{
	static struct gccontext gccontext;
	return &gccontext;
}


/*******************************************************************************
 * Debugging.
 */

#if GCDEBUG_ENABLE
#define GCDUMPBATCH(batch) \
	dumpbatch(batch)

#define GCVERIFYBATCH(changeflags, prevrect, currrect) \
	verify_batch(changeflags, prevrect, currrect)

static void dumpbatch(struct gcbatch *gcbatch)
{
	struct list_head *gcbufferhead;
	struct gcbuffer *gcbuffer;
	struct list_head *gcfixuphead;
	struct gcfixup *gcfixup;
	unsigned int i, size;

	if ((GCDBGFILTER.zone & (GCZONE_BUFFER)) == 0)
		return;

	GCDBG(GCZONE_BUFFER, "BATCH DUMP (0x%08X)\n",
		(unsigned int) gcbatch);

	list_for_each(gcbufferhead, &gcbatch->buffer) {
		gcbuffer = list_entry(gcbufferhead, struct gcbuffer, link);

		list_for_each(gcfixuphead, &gcbuffer->fixup) {
			gcfixup = list_entry(gcfixuphead, struct gcfixup, link);

			GCDBG(GCZONE_BUFFER,
				"  Fixup table @ 0x%08X, count = %d:\n",
				(unsigned int) gcfixup, gcfixup->count);

			for (i = 0; i < gcfixup->count; i += 1) {
				GCDBG(GCZONE_BUFFER, "  [%02d]"
					" buffer offset = 0x%08X,"
					" surface offset = 0x%08X\n",
					i,
					gcfixup->fixup[i].dataoffset * 4,
					gcfixup->fixup[i].surfoffset);
			}
		}

		size = (unsigned char *) gcbuffer->tail
		     - (unsigned char *) gcbuffer->head;
		GCDUMPBUFFER(GCZONE_BUFFER, gcbuffer->head, 0, size);
	}
}

static void verify_batch(unsigned int changeflags,
				struct bvrect *prevrect,
				struct bvrect *currrect)
{
	if ((changeflags & 1) == 0) {
		/* Origin did not change. */
		if ((prevrect->left != currrect->left) ||
			(prevrect->top != currrect->top)) {
			GCERR("origin changed\n");
			GCERR("  previous = %d,%d\n",
				prevrect->left, prevrect->top);
			GCERR("  current = %d,%d\n",
				currrect->left, currrect->top);
		}
	}

	if ((changeflags & 2) == 0) {
		/* Size did not change. */
		if ((prevrect->width != currrect->width) ||
			(prevrect->height != currrect->height)) {
			GCERR("size changed\n");
			GCERR("  previous = %dx%d\n",
				prevrect->width, prevrect->height);
			GCERR("  current = %dx%d\n",
				currrect->width, currrect->height);
		}
	}

	prevrect->left = currrect->left;
	prevrect->top = currrect->top;
	prevrect->width = currrect->width;
	prevrect->height = currrect->height;
}
#else
#define GCDUMPBATCH(...)
#define GCVERIFYBATCH(...)
#endif


/*******************************************************************************
 * Error handling.
 */

#define BVSETBLTSURFERROR(errorid, errordesc) \
do { \
	struct gccontext *tmpcontext = get_context(); \
	snprintf(tmpcontext->bverrorstr, sizeof(tmpcontext->bverrorstr), \
		 g_surferr[errorid].message, errordesc.id); \
	GCDUMPSTRING("%s(%d): [ERROR] %s\n", __func__, __LINE__, \
		     tmpcontext->bverrorstr); \
	bverror = errordesc.base + g_surferr[errorid].offset; \
	bvbltparams->errdesc = tmpcontext->bverrorstr; \
} while (0)

#define GCBVERR_DESC		0
#define GCBVERR_DESC_VERS	1
#define GCBVERR_DESC_VIRTADDR	2
#define GCBVERR_TILE		3
#define GCBVERR_TILE_VERS	4
#define GCBVERR_TILE_VIRTADDR	5
#define GCBVERR_GEOM		6
#define GCBVERR_GEOM_VERS	7
#define GCBVERR_GEOM_FORMAT	8

struct bvsurferrorid {
	char *id;
	enum bverror base;
};

struct bvsurferror {
	unsigned int offset;
	char *message;
};

static struct bvsurferror g_surferr[] = {
	/* GCBVERR_DESC */
	{    0, "%s desc structure is not set" },

	/* GCBVERR_DESC_VERS */
	{  100, "%s desc structure has invalid size" },

	/* GCBVERR_DESC_VIRTADDR */
	{  200, "%s desc virtual pointer is not set" },

	/* GCBVERR_TILE: FIXME/TODO define error code */
	{    0, "%s tileparams structure is not set" },

	/* GCBVERR_TILE_VERS */
	{ 3000, "%s tileparams structure has invalid size" },

	/* GCBVERR_TILE_VIRTADDR: FIXME/TODO define error code */
	{  200, "%s tileparams virtual pointer is not set" },

	/* GCBVERR_GEOM */
	{ 1000, "%s geom structure is not set" },

	/* GCBVERR_GEOM_VERS */
	{ 1100, "%s geom structure has invalid size" },

	/* GCBVERR_GEOM_FORMAT */
	{ 1200, "%s invalid format specified" },
};

static struct bvsurferrorid g_destsurferr = { "dst",  BVERR_DSTDESC };
static struct bvsurferrorid g_src1surferr = { "src1", BVERR_SRC1DESC };
static struct bvsurferrorid g_src2surferr = { "src2", BVERR_SRC2DESC };
static struct bvsurferrorid g_masksurferr = { "mask", BVERR_MASKDESC };


/*******************************************************************************
 * Callback info management.
 */

/* BLTsville callback function. */
struct gccallbackbltsville {
	/* Function pointer. */
	void (*fn) (struct bvcallbackerror *err, unsigned long callbackdata);

	/* Callback data. */
	unsigned long data;
};

/* Information for freeing a surface. */
struct gccallbackfreesurface {
	/* Pointer to the buffer descriptor. */
	struct bvbuffdesc *desc;

	/* Pointer to the buffer. */
	void *ptr;
};

/* Callback information. */
struct gccallbackinfo {
	union {
		/* BLTsville callback function. */
		struct gccallbackbltsville callback;

		/* Information for freeing a surface. */
		struct gccallbackfreesurface freesurface;
	} info;

	/* Previous/next callback information. */
	struct list_head link;
};

static enum bverror get_callbackinfo(struct gccallbackinfo **gccallbackinfo)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gccallbackinfo *temp;

	/* Lock access to callback info lists. */
	GCLOCK(&gccontext->callbacklock);

	if (list_empty(&gccontext->callbackvac)) {
		temp = gcalloc(struct gccallbackinfo,
			       sizeof(struct gccallbackinfo));
		if (temp == NULL) {
			bverror = BVERR_OOM;
			goto exit;
		}
		list_add(&temp->link, &gccontext->callbacklist);
	} else {
		struct list_head *head;
		head = gccontext->callbackvac.next;
		temp = list_entry(head, struct gccallbackinfo, link);
		list_move(head, &gccontext->callbacklist);
	}

	*gccallbackinfo = temp;
	bverror = BVERR_NONE;

exit:
	/* Unlock access to callback info lists. */
	GCUNLOCK(&gccontext->callbacklock);

	return bverror;
}

static void free_callback(struct gccallbackinfo *gccallbackinfo)
{
	struct gccontext *gccontext = get_context();

	/* Lock access to callback info lists. */
	GCLOCK(&gccontext->callbacklock);

	list_move(&gccallbackinfo->link, &gccontext->callbackvac);

	/* Unlock access to callback info lists. */
	GCUNLOCK(&gccontext->callbacklock);
}

void callbackbltsville(void *callbackinfo)
{
	struct gccallbackinfo *gccallbackinfo;

	GCENTER(GCZONE_CALLBACK);

	gccallbackinfo = (struct gccallbackinfo *) callbackinfo;
	GCDBG(GCZONE_CALLBACK, "bltsville_callback = 0x%08X\n",
	      (unsigned int) gccallbackinfo->info.callback.fn);
	GCDBG(GCZONE_CALLBACK, "bltsville_param    = 0x%08X\n",
	      (unsigned int) gccallbackinfo->info.callback.data);

	gccallbackinfo->info.callback.fn(NULL,
					 gccallbackinfo->info.callback.data);
	free_callback(gccallbackinfo);

	GCEXIT(GCZONE_CALLBACK);
}

void callbackfreesurface(void *callbackinfo)
{
	struct gccallbackinfo *gccallbackinfo;

	GCENTER(GCZONE_CALLBACK);

	gccallbackinfo = (struct gccallbackinfo *) callbackinfo;
	GCDBG(GCZONE_CALLBACK, "freeing descriptir @ 0x%08X\n",
	      (unsigned int) gccallbackinfo->info.freesurface.desc);
	GCDBG(GCZONE_CALLBACK, "freeing memory @ 0x%08X\n",
	      (unsigned int) gccallbackinfo->info.freesurface.ptr);

	free_surface(gccallbackinfo->info.freesurface.desc,
		     gccallbackinfo->info.freesurface.ptr);
	free_callback(gccallbackinfo);

	GCEXIT(GCZONE_CALLBACK);
}


/*******************************************************************************
 * Temporary buffer management.
 */

enum bverror allocate_temp(struct bvbltparams *bvbltparams,
			   unsigned int size)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();

	GCENTER(GCZONE_TEMP);

	/* Existing buffer too small? */
	if ((gccontext->tmpbuffdesc != NULL) &&
	    (gccontext->tmpbuffdesc->length < size)) {
		GCDBG(GCZONE_TEMP, "freeing current buffer.\n");
		bverror = free_temp(true);
		if (bverror != BVERR_NONE) {
			bvbltparams->errdesc = gccontext->bverrorstr;
			goto exit;
		}
	}

	/* Allocate new buffer if necessary. */
	if ((size > 0) && (gccontext->tmpbuffdesc == NULL)) {
		/* Allocate temporary surface. */
		bverror = allocate_surface(&gccontext->tmpbuffdesc,
					   &gccontext->tmpbuff,
					   size);
		if (bverror != BVERR_NONE) {
			bvbltparams->errdesc = gccontext->bverrorstr;
			goto exit;
		}

		GCDBG(GCZONE_TEMP, "buffdesc @ 0x%08X\n",
		      gccontext->tmpbuffdesc);
		GCDBG(GCZONE_TEMP, "allocated @ 0x%08X\n",
		      gccontext->tmpbuff);
		GCDBG(GCZONE_TEMP, "size = %d\n",
		      size);

		/* Map the buffer explicitly. */
		bverror = bv_map(gccontext->tmpbuffdesc);
		if (bverror != BVERR_NONE) {
			bvbltparams->errdesc = gccontext->bverrorstr;
			goto exit;
		}
	}

	/* Success. */
	bverror = BVERR_NONE;

exit:
	GCEXIT(GCZONE_TEMP);
	return bverror;
}

enum bverror free_temp(bool schedule)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gccallbackinfo *gccallbackinfo;
	struct gcicallbackarm gcicallbackarm;

	/* Is the buffer allocated? */
	if (gccontext->tmpbuffdesc == NULL) {
		bverror = BVERR_NONE;
		goto exit;
	}

	/* Unmap the buffer. */
	bverror = bv_unmap(gccontext->tmpbuffdesc);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Cannot be mapped. */
	if (gccontext->tmpbuffdesc->map != NULL) {
		BVSETERROR(BVERR_OOM, "temporary buffer is still mapped");
		goto exit;
	}

	/* Free the buffer. */
	if (schedule) {
		bverror = get_callbackinfo(&gccallbackinfo);
		if (bverror != BVERR_NONE) {
			BVSETERROR(BVERR_OOM,
				   "callback allocation failed");
			goto exit;
		}

		gccallbackinfo->info.freesurface.desc = gccontext->tmpbuffdesc;
		gccallbackinfo->info.freesurface.ptr = gccontext->tmpbuff;
		gcicallbackarm.callback = callbackfreesurface;
		gcicallbackarm.callbackparam = gccallbackinfo;

		/* Schedule to free the buffer. */
		gc_callback_wrapper(&gcicallbackarm);

		/* Error? */
		if (gcicallbackarm.gcerror != GCERR_NONE) {
			BVSETERROR(BVERR_OOM, "unable to schedule callback");
			goto exit;
		}
	} else {
		/* Free the buffer immediately. */
		free_surface(gccontext->tmpbuffdesc, gccontext->tmpbuff);
	}

	/* Reset the buffer descriptor. */
	gccontext->tmpbuffdesc = NULL;
	gccontext->tmpbuff = NULL;

exit:
	return bverror;
}


/*******************************************************************************
 * Program the destination.
 */

enum bverror set_dst(struct bvbltparams *bvbltparams,
		     struct gcbatch *batch,
		     struct bvbuffmap *dstmap)
{
	enum bverror bverror = BVERR_NONE;
	struct gcmodst *gcmodst;

	GCENTER(GCZONE_DEST);

	/* Did destination surface change? */
	if ((batch->batchflags & BVBATCH_DST) != 0) {
		/* Allocate command buffer. */
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmodst),
				       (void **) &gcmodst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Add the address fixup. */
		add_fixup(bvbltparams, batch, &gcmodst->address,
			  batch->dstbyteshift);

		/* Set surface parameters. */
		gcmodst->config_ldst = gcmodst_config_ldst;
		gcmodst->address = GET_MAP_HANDLE(dstmap);
		gcmodst->stride = bvbltparams->dstgeom->virtstride;

		/* Set surface width and height. */
		gcmodst->rotation.raw = 0;
		gcmodst->rotation.reg.surf_width = batch->dstphyswidth;
		gcmodst->rotationheight_ldst = gcmodst_rotationheight_ldst;
		gcmodst->rotationheight.raw = 0;
		gcmodst->rotationheight.reg.height = batch->dstphysheight;

		/* Disable hardware clipping. */
		gcmodst->clip_ldst = gcmodst_clip_ldst;
		gcmodst->cliplt.raw = 0;
		gcmodst->cliprb.raw = 0;
		gcmodst->cliprb.reg.right = GC_CLIP_RESET_RIGHT;
		gcmodst->cliprb.reg.bottom = GC_CLIP_RESET_BOTTOM;
	}

exit:
	GCEXITARG(GCZONE_DEST, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

/*******************************************************************************
 * Program blending.
 */

enum bverror set_blending(struct bvbltparams *bvbltparams,
			  struct gcbatch *batch,
			  struct surfaceinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct gcmoalphaoff *gcmoalphaoff;
	struct gcmoalpha *gcmoalpha;
	struct gcmoglobal *gcmoglobal;
	struct gcalpha *gca;

	GCENTER(GCZONE_BLEND);

	gca = srcinfo->gca;
	if (gca == NULL) {
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoalphaoff),
				       (void **) &gcmoalphaoff);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoalphaoff->control_ldst = gcmoalphaoff_control_ldst[0];
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

exit:
	GCEXITARG(GCZONE_BLEND, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror set_blending_index(struct bvbltparams *bvbltparams,
				struct gcbatch *batch,
				struct surfaceinfo *srcinfo,
				unsigned int index)
{
	enum bverror bverror = BVERR_NONE;
	struct gcmoalphaoff *gcmoalphaoff;
	struct gcmoxsrcalpha *gcmoxsrcalpha;
	struct gcmoxsrcglobal *gcmoxsrcglobal;
	struct gcalpha *gca;

	GCENTER(GCZONE_BLEND);

	gca = srcinfo->gca;
	if (gca == NULL) {
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoalphaoff),
				       (void **) &gcmoalphaoff);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoalphaoff->control_ldst = gcmoalphaoff_control_ldst[index];
		gcmoalphaoff->control.reg = gcregalpha_off;

		GCDBG(GCZONE_BLEND, "blending disabled.\n");
	} else {
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoxsrcalpha),
				       (void **) &gcmoxsrcalpha);
		if (bverror != BVERR_NONE)
			goto exit;

		gcmoxsrcalpha->control_ldst = gcmoxsrcalpha_control_ldst[index];
		gcmoxsrcalpha->control.reg = gcregalpha_on;

		gcmoxsrcalpha->mode_ldst = gcmoxsrcalpha_mode_ldst[index];
		gcmoxsrcalpha->mode.raw = 0;
		gcmoxsrcalpha->mode.reg.src_global_alpha_mode
			= gca->src_global_alpha_mode;
		gcmoxsrcalpha->mode.reg.dst_global_alpha_mode
			= gca->dst_global_alpha_mode;

		gcmoxsrcalpha->mode.reg.src_blend
			= gca->srcconfig->factor_mode;
		gcmoxsrcalpha->mode.reg.src_color_reverse
			= gca->srcconfig->color_reverse;

		gcmoxsrcalpha->mode.reg.dst_blend
			= gca->dstconfig->factor_mode;
		gcmoxsrcalpha->mode.reg.dst_color_reverse
			= gca->dstconfig->color_reverse;

		GCDBG(GCZONE_BLEND, "dst blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmoxsrcalpha->mode.reg.dst_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmoxsrcalpha->mode.reg.dst_color_reverse);

		GCDBG(GCZONE_BLEND, "src blend:\n");
		GCDBG(GCZONE_BLEND, "  factor = %d\n",
			gcmoxsrcalpha->mode.reg.src_blend);
		GCDBG(GCZONE_BLEND, "  inverse = %d\n",
			gcmoxsrcalpha->mode.reg.src_color_reverse);

		if ((gca->src_global_alpha_mode
			!= GCREG_GLOBAL_ALPHA_MODE_NORMAL) ||
		    (gca->dst_global_alpha_mode
			!= GCREG_GLOBAL_ALPHA_MODE_NORMAL)) {
			bverror = claim_buffer(bvbltparams, batch,
					       sizeof(struct gcmoxsrcglobal),
					       (void **) &gcmoxsrcglobal);
			if (bverror != BVERR_NONE)
				goto exit;

			gcmoxsrcglobal->srcglobal_ldst
				= gcmoxsrcglobal_srcglobal_ldst[index];
			gcmoxsrcglobal->srcglobal.raw = gca->src_global_color;

			gcmoxsrcglobal->dstglobal_ldst
				= gcmoxsrcglobal_dstglobal_ldst[index];
			gcmoxsrcglobal->dstglobal.raw = gca->dst_global_color;
		}
	}

exit:
	GCEXITARG(GCZONE_BLEND, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

/*******************************************************************************
 * Program YUV source.
 */

void set_computeyuv(struct surfaceinfo *srcinfo, int x, int y)
{
	int pixalign, bytealign;
	unsigned int height1, size1;
	unsigned int height2, size2;
	unsigned int origin;
	int ssX, ssY;

	GCENTER(GCZONE_SRC);

	/* Compute base address alignment. */
	pixalign = get_pixel_offset(srcinfo, 0);
	bytealign = (pixalign * (int) srcinfo->format.bitspp) / 8;

	/* Determine the physical height of the first plane. */
	height1 = ((srcinfo->angle % 2) == 0)
		? srcinfo->geom->height
		: srcinfo->geom->width;

	/* Determine the size of the first plane. */
	size1 = srcinfo->geom->virtstride * height1;

	/* Determine the stride of the second plane. */
	srcinfo->stride2 = srcinfo->geom->virtstride
			 / srcinfo->format.cs.yuv.xsample;

	/* Determine subsample pixel position. */
	ssX = x / srcinfo->format.cs.yuv.xsample;
	ssY = y / srcinfo->format.cs.yuv.ysample;

	switch (srcinfo->format.cs.yuv.planecount) {
	case 2:
		/* U and V are interleaved in one plane. */
		ssX *= 2;
		srcinfo->stride2 *= 2;

		/* Determnine the origin offset. */
		origin = ssY * srcinfo->stride2 + ssX;

		/* Compute the alignment of the second plane. */
		srcinfo->bytealign2 = bytealign + size1 + origin;

		GCDBG(GCZONE_SRC, "plane2 offset (bytes) = 0x%08X\n",
			srcinfo->bytealign2);
		GCDBG(GCZONE_SRC, "plane2 stride = %d\n",
			srcinfo->stride2);
		break;

	case 3:
		/* Determine the physical height of the U/V planes. */
		height2 = height1 / srcinfo->format.cs.yuv.ysample;

		/* Determine the size of the U/V planes. */
		size2 = srcinfo->stride2 * height2;

		/* Determnine the origin offset. */
		origin = ssY * srcinfo->stride2 + ssX;

		/* Compute the alignment of the U/V planes. */
		srcinfo->bytealign2 = bytealign + size1 + origin;
		srcinfo->bytealign3 = bytealign + size1 + size2 + origin;

		/* Determine the stride of the U/V planes. */
		srcinfo->stride3 = srcinfo->stride2;

		GCDBG(GCZONE_SRC, "plane2 offset (bytes) = 0x%08X\n",
		      srcinfo->bytealign2);
		GCDBG(GCZONE_SRC, "plane2 stride = %d\n",
			srcinfo->stride2);
		GCDBG(GCZONE_SRC, "plane3 offset (bytes) = 0x%08X\n",
		      srcinfo->bytealign3);
		GCDBG(GCZONE_SRC, "plane3 stride = %d\n",
			srcinfo->stride3);
		break;
	}

	GCEXIT(GCZONE_SRC);
}

enum bverror set_yuvsrc(struct bvbltparams *bvbltparams,
			struct gcbatch *batch,
			struct surfaceinfo *srcinfo,
			struct bvbuffmap *srcmap)
{
	enum bverror bverror = BVERR_NONE;
	struct gcmoyuv1 *gcmoyuv1;
	struct gcmoyuv2 *gcmoyuv2;
	struct gcmoyuv3 *gcmoyuv3;

	GCENTER(GCZONE_SRC);

	GCDBG(GCZONE_SRC, "plane count %d.\n",
	      srcinfo->format.cs.yuv.planecount);

	switch (srcinfo->format.cs.yuv.planecount) {
	case 1:
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoyuv1),
				       (void **) &gcmoyuv1);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Set YUV parameters. */
		gcmoyuv1->pectrl_ldst = gcmoyuv_pectrl_ldst;
		gcmoyuv1->pectrl.raw = 0;
		gcmoyuv1->pectrl.reg.standard
			= srcinfo->format.cs.yuv.std;
		gcmoyuv1->pectrl.reg.swizzle
			= srcinfo->format.swizzle;
		gcmoyuv1->pectrl.reg.convert
			= GCREG_PE_CONTROL_YUVRGB_DISABLED;
		break;

	case 2:
		bverror = claim_buffer(bvbltparams, batch,
					sizeof(struct gcmoyuv2),
					(void **) &gcmoyuv2);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Set YUV parameters. */
		gcmoyuv2->pectrl_ldst = gcmoyuv_pectrl_ldst;
		gcmoyuv2->pectrl.raw = 0;
		gcmoyuv2->pectrl.reg.standard
			= srcinfo->format.cs.yuv.std;
		gcmoyuv2->pectrl.reg.swizzle
			= srcinfo->format.swizzle;
		gcmoyuv2->pectrl.reg.convert
			= GCREG_PE_CONTROL_YUVRGB_DISABLED;

		/* Program U/V plane. */
		add_fixup(bvbltparams, batch, &gcmoyuv2->uplaneaddress,
			  srcinfo->bytealign2);
		gcmoyuv2->plane_ldst = gcmoyuv2_plane_ldst;
		gcmoyuv2->uplaneaddress = GET_MAP_HANDLE(srcmap);
		gcmoyuv2->uplanestride = srcinfo->stride2;
		break;

	case 3:
		bverror = claim_buffer(bvbltparams, batch,
					sizeof(struct gcmoyuv3),
					(void **) &gcmoyuv3);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Set YUV parameters. */
		gcmoyuv3->pectrl_ldst = gcmoyuv_pectrl_ldst;
		gcmoyuv3->pectrl.raw = 0;
		gcmoyuv3->pectrl.reg.standard
			= srcinfo->format.cs.yuv.std;
		gcmoyuv3->pectrl.reg.swizzle
			= srcinfo->format.swizzle;
		gcmoyuv3->pectrl.reg.convert
			= GCREG_PE_CONTROL_YUVRGB_DISABLED;

		/* Program U/V planes. */
		add_fixup(bvbltparams, batch, &gcmoyuv3->uplaneaddress,
			  srcinfo->bytealign2);
		add_fixup(bvbltparams, batch, &gcmoyuv3->vplaneaddress,
			  srcinfo->bytealign3);
		gcmoyuv3->plane_ldst = gcmoyuv3_plane_ldst;
		gcmoyuv3->uplaneaddress = GET_MAP_HANDLE(srcmap);
		gcmoyuv3->uplanestride  = srcinfo->stride2;
		gcmoyuv3->vplaneaddress = GET_MAP_HANDLE(srcmap);
		gcmoyuv3->vplanestride  = srcinfo->stride3;
		break;

	default:
		GCERR("invlaid plane count %d.\n",
		      srcinfo->format.cs.yuv.planecount);
	}

exit:
	GCEXITARG(GCZONE_SRC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror set_yuvsrc_index(struct bvbltparams *bvbltparams,
			      struct gcbatch *batch,
			      struct surfaceinfo *srcinfo,
			      struct bvbuffmap *srcmap,
			      unsigned int index)
{
	enum bverror bverror = BVERR_NONE;
	struct gcmoxsrcyuv1 *gcmoxsrcyuv1;
	struct gcmoxsrcyuv2 *gcmoxsrcyuv2;
	struct gcmoxsrcyuv3 *gcmoxsrcyuv3;

	GCENTER(GCZONE_SRC);

	GCDBG(GCZONE_SRC, "plane count %d.\n",
	      srcinfo->format.cs.yuv.planecount);

	switch (srcinfo->format.cs.yuv.planecount) {
	case 1:
		bverror = claim_buffer(bvbltparams, batch,
				       sizeof(struct gcmoxsrcyuv1),
				       (void **) &gcmoxsrcyuv1);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Set YUV parameters. */
		gcmoxsrcyuv1->pectrl_ldst
			= gcmoxsrcyuv_pectrl_ldst[index];
		gcmoxsrcyuv1->pectrl.raw = 0;
		gcmoxsrcyuv1->pectrl.reg.standard
			= srcinfo->format.cs.yuv.std;
		gcmoxsrcyuv1->pectrl.reg.swizzle
			= srcinfo->format.swizzle;
		gcmoxsrcyuv1->pectrl.reg.convert
			= GCREG_PE_CONTROL_YUVRGB_DISABLED;
		break;

	case 2:
		bverror = claim_buffer(bvbltparams, batch,
					sizeof(struct gcmoxsrcyuv2),
					(void **) &gcmoxsrcyuv2);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Set YUV parameters. */
		gcmoxsrcyuv2->pectrl_ldst
			= gcmoxsrcyuv_pectrl_ldst[index];
		gcmoxsrcyuv2->pectrl.raw = 0;
		gcmoxsrcyuv2->pectrl.reg.standard
			= srcinfo->format.cs.yuv.std;
		gcmoxsrcyuv2->pectrl.reg.swizzle
			= srcinfo->format.swizzle;
		gcmoxsrcyuv2->pectrl.reg.convert
			= GCREG_PE_CONTROL_YUVRGB_DISABLED;

		/* Program U/V plane. */
		add_fixup(bvbltparams, batch, &gcmoxsrcyuv2->uplaneaddress,
			  srcinfo->bytealign2);
		gcmoxsrcyuv2->uplaneaddress_ldst
			= gcmoxsrcyuv_uplaneaddress_ldst[index];
		gcmoxsrcyuv2->uplaneaddress = GET_MAP_HANDLE(srcmap);
		gcmoxsrcyuv2->uplanestride_ldst
			= gcmoxsrcyuv_uplanestride_ldst[index];
		gcmoxsrcyuv2->uplanestride = srcinfo->stride2;
		break;

	case 3:
		bverror = claim_buffer(bvbltparams, batch,
					sizeof(struct gcmoxsrcyuv3),
					(void **) &gcmoxsrcyuv3);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Set YUV parameters. */
		gcmoxsrcyuv3->pectrl_ldst
			= gcmoxsrcyuv_pectrl_ldst[index];
		gcmoxsrcyuv3->pectrl.raw = 0;
		gcmoxsrcyuv3->pectrl.reg.standard
			= srcinfo->format.cs.yuv.std;
		gcmoxsrcyuv3->pectrl.reg.swizzle
			= srcinfo->format.swizzle;
		gcmoxsrcyuv3->pectrl.reg.convert
			= GCREG_PE_CONTROL_YUVRGB_DISABLED;

		/* Program U/V planes. */
		add_fixup(bvbltparams, batch, &gcmoxsrcyuv3->uplaneaddress,
			  srcinfo->bytealign2);
		add_fixup(bvbltparams, batch, &gcmoxsrcyuv3->vplaneaddress,
			  srcinfo->bytealign3);
		gcmoxsrcyuv3->uplaneaddress_ldst
			= gcmoxsrcyuv_uplaneaddress_ldst[index];
		gcmoxsrcyuv3->uplaneaddress = GET_MAP_HANDLE(srcmap);
		gcmoxsrcyuv3->uplanestride_ldst
			= gcmoxsrcyuv_uplanestride_ldst[index];
		gcmoxsrcyuv3->uplanestride  = srcinfo->stride2;
		gcmoxsrcyuv3->vplaneaddress_ldst
			= gcmoxsrcyuv_vplaneaddress_ldst[index];
		gcmoxsrcyuv3->vplaneaddress = GET_MAP_HANDLE(srcmap);
		gcmoxsrcyuv3->vplanestride_ldst
			= gcmoxsrcyuv_vplanestride_ldst[index];
		gcmoxsrcyuv3->vplanestride  = srcinfo->stride3;
		break;

	default:
		GCERR("invlaid plane count %d.\n",
		      srcinfo->format.cs.yuv.planecount);
	}

exit:
	GCEXITARG(GCZONE_SRC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

/*******************************************************************************
 * Surface compare and validation.
 */

static inline bool equal_rects(struct bvrect *rect1, struct bvrect *rect2)
{
	if (rect1->left != rect2->left)
		return false;

	if (rect1->top != rect2->top)
		return false;

	if (rect1->width != rect2->width)
		return false;

	if (rect1->height != rect2->height)
		return false;

	return true;
}

/* The function verifies whether the two buffer descriptors and rectangles
   define the same physical area. */
static bool same_phys_area(struct bvbuffdesc *surf1, struct bvrect *rect1,
			   struct bvbuffdesc *surf2, struct bvrect *rect2)
{
	struct bvphysdesc *physdesc1;
	struct bvphysdesc *physdesc2;

	/* If pointers are the same, things are much easier. */
	if (surf1 == surf2)
		/* Compare the rectangles. For simplicity we don't consider
		   cases with partially overlapping rectangles at this time. */
		return equal_rects(rect1, rect2);

	/* Assume diffrent areas if the types are different. */
	if (surf1->auxtype != surf2->auxtype)
		return false;

	if (surf1->auxtype == BVAT_PHYSDESC) {
		physdesc1 = (struct bvphysdesc *) surf1->auxptr;
		physdesc2 = (struct bvphysdesc *) surf2->auxptr;

		/* Same physical descriptor? */
		if (physdesc1 == physdesc2)
			return equal_rects(rect1, rect2);

		/* Same page array? */
		if (physdesc1->pagearray == physdesc2->pagearray)
			return equal_rects(rect1, rect2);

		/* Pageoffsets must match since different buffers
		 * can share the same first page (eg nv12).
		 */
		if (physdesc1->pageoffset != physdesc2->pageoffset)
			return false;

		/* Assume the same surface if first pages match. */
		if (physdesc1->pagearray[0] == physdesc2->pagearray[0])
			return equal_rects(rect1, rect2);

	} else {
		if (surf1->virtaddr == surf2->virtaddr)
			return equal_rects(rect1, rect2);
	}

	return false;
}

static int verify_surface(unsigned int tile,
			  union bvinbuff *surf,
			  struct bvsurfgeom *geom)
{
	if (tile) {
		if (surf->tileparams == NULL)
			return GCBVERR_TILE;

		if (surf->tileparams->structsize <
		    STRUCTSIZE(surf->tileparams, srcheight))
			return GCBVERR_TILE_VERS;

		/* FIXME/TODO */
		return GCBVERR_TILE;
	} else {
		if (surf->desc == NULL)
			return GCBVERR_DESC;

		if (surf->desc->structsize < STRUCTSIZE(surf->desc, map))
			return GCBVERR_DESC_VERS;
	}

	if (geom == NULL)
		return GCBVERR_GEOM;

	if (geom->structsize < STRUCTSIZE(geom, palette))
		return GCBVERR_GEOM_VERS;

	/* Validation successful. */
	return -1;
}


/*******************************************************************************
 * Library constructor and destructor.
 */

void bv_init(void)
{
	struct gccontext *gccontext = get_context();
	struct gcicaps gcicaps;
	unsigned i, j;

	GCDBG_REGISTER(bv);
	GCDBG_REGISTER(parser);
	GCDBG_REGISTER(map);
	GCDBG_REGISTER(buffer);
	GCDBG_REGISTER(fill);
	GCDBG_REGISTER(blit);
	GCDBG_REGISTER(filter);

	GCLOCK_INIT(&gccontext->batchlock);
	GCLOCK_INIT(&gccontext->bufferlock);
	GCLOCK_INIT(&gccontext->fixuplock);
	GCLOCK_INIT(&gccontext->maplock);
	GCLOCK_INIT(&gccontext->callbacklock);

	INIT_LIST_HEAD(&gccontext->unmapvac);
	INIT_LIST_HEAD(&gccontext->buffervac);
	INIT_LIST_HEAD(&gccontext->fixupvac);
	INIT_LIST_HEAD(&gccontext->batchvac);
	INIT_LIST_HEAD(&gccontext->callbacklist);
	INIT_LIST_HEAD(&gccontext->callbackvac);

	/* Initialize the filter cache. */
	for (i = 0; i < GC_FILTER_COUNT; i += 1)
		for (j = 0; j < GC_TAP_COUNT; j += 1)
			INIT_LIST_HEAD(&gccontext->filtercache[i][j].list);

	/* Query hardware caps. */
	gc_getcaps_wrapper(&gcicaps);
	if (gcicaps.gcerror == GCERR_NONE) {
		gccontext->gcmodel = gcicaps.gcmodel;
		gccontext->gcrevision = gcicaps.gcrevision;
		gccontext->gcdate = gcicaps.gcdate;
		gccontext->gctime = gcicaps.gctime;
		gccontext->gcfeatures = gcicaps.gcfeatures;
		gccontext->gcfeatures0 = gcicaps.gcfeatures0;
		gccontext->gcfeatures1 = gcicaps.gcfeatures1;
		gccontext->gcfeatures2 = gcicaps.gcfeatures2;
		gccontext->gcfeatures3 = gcicaps.gcfeatures3;
	}
}

void bv_exit(void)
{
	struct gccontext *gccontext = get_context();
	struct bvbuffmap *bvbuffmap;
	struct list_head *head;
	struct gcschedunmap *gcschedunmap;
	struct gcbuffer *gcbuffer;
	struct gcfixup *gcfixup;
	struct gcbatch *gcbatch;
	struct gccallbackinfo *gccallbackinfo;

	while (gccontext->buffmapvac != NULL) {
		bvbuffmap = gccontext->buffmapvac;
		gccontext->buffmapvac = bvbuffmap->nextmap;
		gcfree(bvbuffmap);
	}

	while (!list_empty(&gccontext->unmapvac)) {
		head = gccontext->unmapvac.next;
		gcschedunmap = list_entry(head, struct gcschedunmap, link);
		list_del(head);
		gcfree(gcschedunmap);
	}

	while (!list_empty(&gccontext->buffervac)) {
		head = gccontext->buffervac.next;
		gcbuffer = list_entry(head, struct gcbuffer, link);
		list_del(head);
		gcfree(gcbuffer);
	}

	while (!list_empty(&gccontext->fixupvac)) {
		head = gccontext->fixupvac.next;
		gcfixup = list_entry(head, struct gcfixup, link);
		list_del(head);
		gcfree(gcfixup);
	}

	while (!list_empty(&gccontext->batchvac)) {
		head = gccontext->batchvac.next;
		gcbatch = list_entry(head, struct gcbatch, link);
		list_del(head);
		gcfree(gcbatch);
	}

	while (!list_empty(&gccontext->callbacklist)) {
		head = gccontext->callbacklist.next;
		list_move(head, &gccontext->callbackvac);
	}

	while (!list_empty(&gccontext->callbackvac)) {
		head = gccontext->callbackvac.next;
		gccallbackinfo = list_entry(head, struct gccallbackinfo, link);
		list_del(head);
		gcfree(gccallbackinfo);
	}

	free_temp(false);
}


/*******************************************************************************
 * Library API.
 */

enum bverror bv_map(struct bvbuffdesc *bvbuffdesc)
{
	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;

	GCENTERARG(GCZONE_MAPPING, "bvbuffdesc = 0x%08X\n",
		   (unsigned int) bvbuffdesc);

	if (bvbuffdesc == NULL) {
		BVSETERROR(BVERR_BUFFERDESC, "bvbuffdesc is NULL");
		goto exit;
	}

	if (bvbuffdesc->structsize < STRUCTSIZE(bvbuffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	bverror = do_map(bvbuffdesc, NULL, &bvbuffmap);

exit:
	GCEXITARG(GCZONE_MAPPING, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror bv_unmap(struct bvbuffdesc *bvbuffdesc)
{
	enum bverror bverror = BVERR_NONE;
	enum bverror otherbverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();
	struct bvbuffmap *prev = NULL;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;
	struct gcimap gcimap;

	GCENTERARG(GCZONE_MAPPING, "bvbuffdesc = 0x%08X\n",
		   (unsigned int) bvbuffdesc);

	/* Lock access to the mapping list. */
	GCLOCK(&gccontext->maplock);

	if (bvbuffdesc == NULL) {
		BVSETERROR(BVERR_BUFFERDESC, "bvbuffdesc is NULL");
		goto exit;
	}

	if (bvbuffdesc->structsize < STRUCTSIZE(bvbuffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	/* Is the buffer mapped? */
	bvbuffmap = bvbuffdesc->map;
	if (bvbuffmap == NULL) {
		GCDBG(GCZONE_MAPPING, "buffer isn't mapped.\n");
		goto exit;
	}

	/* Try to find our mapping. */
	while (bvbuffmap != NULL) {
		if (bvbuffmap->bv_unmap == bv_unmap)
			break;
		prev = bvbuffmap;
		bvbuffmap = bvbuffmap->nextmap;
	}

	/* Are there other implementations? */
	if ((prev != NULL) || (bvbuffmap->nextmap != NULL)) {
		GCDBG(GCZONE_MAPPING,
		      "have mappings from other implementations.\n");

		/* Was our mapping found? */
		if (bvbuffmap == NULL) {
			GCDBG(GCZONE_MAPPING,
			      "no mapping from our implementation.\n");

			/* No, call other implementations. */
			bverror = bvbuffdesc->map->bv_unmap(bvbuffdesc);
			goto exit;
		}

		if (bvbuffmap->structsize
				< STRUCTSIZE(bvbuffmap, nextmap)) {
			BVSETERROR(BVERR_BUFFERDESC_VERS,
				   "unsupported bvbuffdesc version");
			goto exit;
		}

		/* Remove our mapping. */
		if (prev == NULL)
			bvbuffdesc->map = bvbuffmap->nextmap;
		else
			prev->nextmap = bvbuffmap->nextmap;

		/* Call other implementation. */
		otherbverror = bvbuffdesc->map->bv_unmap(bvbuffdesc);

		/* Add our mapping back. */
		bvbuffmap->nextmap = bvbuffdesc->map;
		bvbuffdesc->map = bvbuffmap;
		prev = NULL;
	} else {
		GCDBG(GCZONE_MAPPING,
		      "no mappings from other implementations.\n");
	}

	/* Get the info structure. */
	bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

	GCDBG(GCZONE_MAPPING, "bvbuffmap = 0x%08X\n", (unsigned int) bvbuffmap);
	GCDBG(GCZONE_MAPPING, "handle = 0x%08X\n", bvbuffmapinfo->handle);

	/* Explicit unmapping. */
	if (bvbuffmapinfo->usermap == 0)
		GCERR("explicit count is already zero.\n");
	bvbuffmapinfo->usermap = 0;

	GCDBG(GCZONE_MAPPING, "explicit count = %d\n",
		bvbuffmapinfo->usermap);
	GCDBG(GCZONE_MAPPING, "implicit count = %d\n",
		bvbuffmapinfo->automap);

	/* Do we have implicit mappings? */
	if (bvbuffmapinfo->automap > 0) {
		GCDBG(GCZONE_MAPPING, "have implicit unmappings.\n");
		goto exit;
	}

	/* Unmap the buffer. */
	memset(&gcimap, 0, sizeof(gcimap));
	gcimap.handle = bvbuffmapinfo->handle;
	gc_unmap_wrapper(&gcimap);
	if (gcimap.gcerror != GCERR_NONE) {
		BVSETERROR(BVERR_OOM, "unable to free gccore memory");
		goto exit;
	}

	/* Remove from the buffer descriptor list. */
	if (prev == NULL)
		bvbuffdesc->map = bvbuffmap->nextmap;
	else
		prev->nextmap = bvbuffmap->nextmap;

	/* Invalidate the record. */
	bvbuffmap->structsize = 0;

	/* Add to the vacant list. */
	bvbuffmap->nextmap = gccontext->buffmapvac;
	gccontext->buffmapvac = bvbuffmap;

exit:
	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext->maplock);

	GCEXITARG(GCZONE_MAPPING, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror bv_blt(struct bvbltparams *bvbltparams)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();
	struct gcalpha *gca = NULL;
	struct gcalpha _gca;
	unsigned int op, type, blend, format;
	unsigned int batchexec = 0;
	bool nop = false;
	struct gcbatch *gcbatch;
	struct bvrect *dstrect;
	int src1used, src2used, maskused;
	struct surfaceinfo srcinfo[2];
	struct bvrect *srcrect[2];
	unsigned short rop;
	struct gcicommit gcicommit;
	int i, srccount, res;

	GCENTERARG(GCZONE_BLIT, "bvbltparams = 0x%08X\n",
		   (unsigned int) bvbltparams);

	/* Verify blt parameters structure. */
	if (bvbltparams == NULL) {
		BVSETERROR(BVERR_BLTPARAMS_VERS, "bvbltparams is NULL");
		goto exit;
	}

	if (bvbltparams->structsize < STRUCTSIZE(bvbltparams, callbackdata)) {
		BVSETERROR(BVERR_BLTPARAMS_VERS, "argument has invalid size");
		goto exit;
	}

	/* Reset the error message. */
	bvbltparams->errdesc = NULL;

	/* Verify the destination parameters structure. */
	res = verify_surface(0, (union bvinbuff *) &bvbltparams->dstdesc,
				bvbltparams->dstgeom);
	if (res != -1) {
		BVSETBLTSURFERROR(res, g_destsurferr);
		goto exit;
	}

	/* Extract the operation flags. */
	op = (bvbltparams->flags & BVFLAG_OP_MASK) >> BVFLAG_OP_SHIFT;
	type = (bvbltparams->flags & BVFLAG_BATCH_MASK) >> BVFLAG_BATCH_SHIFT;
	GCDBG(GCZONE_BLIT, "op = %d\n", op);
	GCDBG(GCZONE_BLIT, "type = %d\n", type);

	switch (type) {
	case (BVFLAG_BATCH_NONE >> BVFLAG_BATCH_SHIFT):
		bverror = allocate_batch(bvbltparams, &gcbatch);
		if (bverror != BVERR_NONE) {
			bvbltparams->errdesc = gccontext->bverrorstr;
			goto exit;
		}

		batchexec = 1;
		gcbatch->batchflags = 0x7FFFFFFF;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_NONE(0x%08X)\n",
		      (unsigned int) gcbatch);
		break;

	case (BVFLAG_BATCH_BEGIN >> BVFLAG_BATCH_SHIFT):
		bverror = allocate_batch(bvbltparams, &gcbatch);
		if (bverror != BVERR_NONE) {
			bvbltparams->errdesc = gccontext->bverrorstr;
			goto exit;
		}

		bvbltparams->batch = (struct bvbatch *) gcbatch;

		batchexec = 0;
		bvbltparams->batchflags =
		gcbatch->batchflags = 0x7FFFFFFF;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_BEGIN(0x%08X)\n",
		      (unsigned int) gcbatch);
		break;

	case (BVFLAG_BATCH_CONTINUE >> BVFLAG_BATCH_SHIFT):
		gcbatch = (struct gcbatch *) bvbltparams->batch;
		if (gcbatch == NULL) {
			BVSETBLTERROR(BVERR_BATCH, "batch is not initialized");
			goto exit;
		}

		if (gcbatch->structsize < STRUCTSIZE(gcbatch, unmap)) {
			BVSETBLTERROR(BVERR_BATCH, "invalid batch");
			goto exit;
		}

		batchexec = 0;
		gcbatch->batchflags = bvbltparams->batchflags;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_CONTINUE(0x%08X)\n",
		      (unsigned int) gcbatch);
		break;

	case (BVFLAG_BATCH_END >> BVFLAG_BATCH_SHIFT):
		gcbatch = (struct gcbatch *) bvbltparams->batch;
		if (gcbatch == NULL) {
			BVSETBLTERROR(BVERR_BATCH, "batch is not initialized");
			goto exit;
		}

		if (gcbatch->structsize < STRUCTSIZE(gcbatch, unmap)) {
			BVSETBLTERROR(BVERR_BATCH, "invalid batch");
			goto exit;
		}

		batchexec = 1;
		nop = (bvbltparams->batchflags & BVBATCH_ENDNOP) != 0;
		gcbatch->batchflags = bvbltparams->batchflags;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_END(0x%08X)\n",
		      (unsigned int) gcbatch);
		break;

	default:
		BVSETBLTERROR(BVERR_BATCH, "unrecognized batch type");
		goto exit;
	}

	GCDBG(GCZONE_BATCH, "batchflags=0x%08X\n",
		(unsigned int) gcbatch->batchflags);

	if (!nop) {
		/* Get a shortcut to the destination rectangle. */
		dstrect = &bvbltparams->dstrect;

		/* Verify the batch change flags. */
		GCVERIFYBATCH(gcbatch->batchflags >> 12,
			      &gcbatch->prevdstrect, dstrect);

		switch (op) {
		case (BVFLAG_ROP >> BVFLAG_OP_SHIFT):
			GCDBG(GCZONE_BLIT, "BVFLAG_ROP\n");

			rop = bvbltparams->op.rop;
			src1used = ((rop & 0xCCCC) >> 2)
				 ^  (rop & 0x3333);
			src2used = ((rop & 0xF0F0) >> 4)
				 ^  (rop & 0x0F0F);
			maskused = ((rop & 0xFF00) >> 8)
				 ^  (rop & 0x00FF);
			break;

		case (BVFLAG_BLEND >> BVFLAG_OP_SHIFT):
			GCDBG(GCZONE_BLIT, "BVFLAG_BLEND\n");

			blend = bvbltparams->op.blend;
			format = (blend & BVBLENDDEF_FORMAT_MASK)
			       >> BVBLENDDEF_FORMAT_SHIFT;

			bverror = parse_blend(bvbltparams, blend, &_gca);
			if (bverror != BVERR_NONE)
				goto exit;

			gca = &_gca;

			switch (format) {
			case (BVBLENDDEF_FORMAT_CLASSIC
				>> BVBLENDDEF_FORMAT_SHIFT):
				src1used = gca->src1used;
				src2used = gca->src2used;
				maskused = blend & BVBLENDDEF_REMOTE;
				break;

			default:
				BVSETBLTERROR(BVERR_BLEND,
					      "unrecognized blend format");
				goto exit;
			}
			break;

		case (BVFLAG_FILTER >> BVFLAG_OP_SHIFT):
			GCDBG(GCZONE_BLIT, "BVFLAG_FILTER\n");
			BVSETBLTERROR(BVERR_OP,
				      "filter operation not supported");
			goto exit;

		default:
			BVSETBLTERROR(BVERR_OP, "unrecognized operation");
			goto exit;
		}

		/* Reset the number of sources. */
		srccount = 0;

		/* Verify the src1 parameters structure. */
		if (src1used) {
			GCDBG(GCZONE_SRC, "source #1: used\n");
			res = verify_surface(
				bvbltparams->flags & BVBATCH_TILE_SRC1,
				&bvbltparams->src1, bvbltparams->src1geom);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src1surferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			GCVERIFYBATCH(gcbatch->batchflags >> 14,
				      &gcbatch->prevsrc1rect,
				      &bvbltparams->src1rect);

			/* Same as the destination? */
			if (same_phys_area(bvbltparams->src1.desc,
					   &bvbltparams->src1rect,
					   bvbltparams->dstdesc,
					   dstrect)) {
				GCDBG(GCZONE_BLIT, "  same as destination\n");
			} else {
				srcinfo[srccount].index = 0;
				srcinfo[srccount].buf = bvbltparams->src1;
				srcinfo[srccount].geom = bvbltparams->src1geom;
				srcinfo[srccount].newgeom
					= gcbatch->batchflags
						& BVBATCH_SRC1;
				srcinfo[srccount].newrect
					= gcbatch->batchflags
						& (BVBATCH_SRC1RECT_ORIGIN |
						   BVBATCH_SRC1RECT_SIZE);
				srcrect[srccount] = &bvbltparams->src1rect;

				bverror = parse_source(bvbltparams, gcbatch,
						       &bvbltparams->src1rect,
						       &srcinfo[srccount]);
				if (bverror != BVERR_NONE)
					goto exit;

				srccount += 1;
			}
		}

		/* Verify the src2 parameters structure. */
		if (src2used) {
			GCDBG(GCZONE_SRC, "source #2: used\n");
			res = verify_surface(
				bvbltparams->flags & BVBATCH_TILE_SRC2,
				&bvbltparams->src2, bvbltparams->src2geom);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src2surferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			GCVERIFYBATCH(gcbatch->batchflags >> 16,
				      &gcbatch->prevsrc2rect,
				      &bvbltparams->src2rect);

			/* Same as the destination? */
			if (same_phys_area(bvbltparams->src2.desc,
					   &bvbltparams->src2rect,
					   bvbltparams->dstdesc,
					   dstrect)) {
				GCDBG(GCZONE_BLIT, "  same as destination\n");
			} else {
				srcinfo[srccount].index = 1;
				srcinfo[srccount].buf = bvbltparams->src2;
				srcinfo[srccount].geom = bvbltparams->src2geom;
				srcinfo[srccount].newgeom
					= gcbatch->batchflags
						& BVBATCH_SRC2;
				srcinfo[srccount].newrect
					= gcbatch->batchflags
						& (BVBATCH_SRC2RECT_ORIGIN |
						   BVBATCH_SRC2RECT_SIZE);
				srcrect[srccount] = &bvbltparams->src2rect;

				bverror = parse_source(bvbltparams, gcbatch,
						       &bvbltparams->src2rect,
						       &srcinfo[srccount]);
				if (bverror != BVERR_NONE)
					goto exit;

				srccount += 1;
			}
		}

		/* Verify the mask parameters structure. */
		if (maskused) {
			GCDBG(GCZONE_MASK, "mask: used\n");
			res = verify_surface(
				bvbltparams->flags & BVBATCH_TILE_MASK,
				&bvbltparams->mask, bvbltparams->maskgeom);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_masksurferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			GCVERIFYBATCH(gcbatch->batchflags >> 18,
				      &gcbatch->prevmaskrect,
				      &bvbltparams->maskrect);

			BVSETBLTERROR(BVERR_OP,
				      "operation with mask not supported");
			goto exit;
		}

		GCDBG(GCZONE_BLIT, "srccount = %d\n", srccount);

		if (srccount == 0) {
			BVSETBLTERROR(BVERR_OP,
				      "operation not supported");
			goto exit;
		} else {
			for (i = 0; i < srccount; i += 1) {
				int srcw, srch;
				GCDBG(GCZONE_BLIT,
				      "processing source %d.\n",
				      srcinfo[i].index + 1);

				if (gca == NULL) {
					GCDBG(GCZONE_BLIT,
					      "  blending disabled.\n");
					srcinfo[i].rop = bvbltparams->op.rop;
					srcinfo[i].gca = NULL;
				} else if ((i + 1) != srccount) {
					GCDBG(GCZONE_BLIT,
					      "  disabling blending for "
					      "the first source.\n");
					srcinfo[i].rop = 0xCC;
					srcinfo[i].gca = NULL;
				} else {
					GCDBG(GCZONE_BLIT,
					      "  enabling blending.\n");
					srcinfo[i].rop = 0xCC;
					srcinfo[i].gca = gca;

					if (srccount == 1) {
						gca->srcconfig = gca->k1;
						gca->dstconfig = gca->k2;
					} else {
						gca->srcconfig = gca->k2;
						gca->dstconfig = gca->k1;
					}
				}

				GCDBG(GCZONE_BLIT, "  srcsize %dx%d.\n",
				      srcrect[i]->width, srcrect[i]->height);
				GCDBG(GCZONE_BLIT, "  dstsize %dx%d.\n",
				      dstrect->width, dstrect->height);

				srcw = srcrect[i]->width;
				srch = srcrect[i]->height;
				if ((srcw == 1) && (srch == 1) &&
				    (bvbltparams->src1.desc->virtaddr)) {
					GCDBG(GCZONE_BLIT, "  op: fill.\n");
					bverror = do_fill(bvbltparams,
							  gcbatch,
							  &srcinfo[i]);
				} else if ((srcw == dstrect->width) &&
					   (srch == dstrect->height)) {
					GCDBG(GCZONE_BLIT, "  op: bitblit.\n");
					bverror = do_blit(bvbltparams,
							  gcbatch,
							  &srcinfo[i]);
				} else {
					GCDBG(GCZONE_BLIT, "  op: filter.\n");
					bverror = do_filter(bvbltparams,
							    gcbatch,
							    &srcinfo[i]);
				}

				if (bverror != BVERR_NONE)
					goto exit;
			}
		}
	}

	if (batchexec) {
		struct gcmoflush *flush;

		GCDBG(GCZONE_BLIT, "preparing to submit the batch.\n");

		/* Finalize the current operation. */
		bverror = gcbatch->batchend(bvbltparams, gcbatch);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Add PE flush. */
		GCDBG(GCZONE_BLIT, "appending the flush.\n");
		bverror = claim_buffer(bvbltparams, gcbatch,
				       sizeof(struct gcmoflush),
				       (void **) &flush);
		if (bverror != BVERR_NONE)
			goto exit;

		flush->flush_ldst = gcmoflush_flush_ldst;
		flush->flush.reg = gcregflush_pe2D;

		/* Process asynchronous operation. */
		if ((bvbltparams->flags & BVFLAG_ASYNC) == 0) {
			GCDBG(GCZONE_BLIT, "synchronous batch.\n");
			gcicommit.callback = NULL;
			gcicommit.callbackparam = NULL;
			gcicommit.asynchronous = false;
		} else {
			struct gccallbackinfo *gccallbackinfo;

			GCDBG(GCZONE_BLIT, "asynchronous batch (0x%08X):\n",
			      bvbltparams->flags);

			if (bvbltparams->callbackfn == NULL) {
				GCDBG(GCZONE_BLIT, "no callback given.\n");
				gcicommit.callback = NULL;
				gcicommit.callbackparam = NULL;
			} else {
				bverror = get_callbackinfo(&gccallbackinfo);
				if (bverror != BVERR_NONE) {
					BVSETBLTERROR(BVERR_OOM,
						      "callback allocation "
						      "failed");
					goto exit;
				}

				gccallbackinfo->info.callback.fn
					= bvbltparams->callbackfn;
				gccallbackinfo->info.callback.data
					= bvbltparams->callbackdata;

				gcicommit.callback = callbackbltsville;
				gcicommit.callbackparam = gccallbackinfo;

				GCDBG(GCZONE_BLIT,
				      "gcbv_callback = 0x%08X\n",
				      (unsigned int) gcicommit.callback);
				GCDBG(GCZONE_BLIT,
				      "gcbv_param    = 0x%08X\n",
				      (unsigned int) gcicommit.callbackparam);
				GCDBG(GCZONE_BLIT,
				      "bltsville_callback = 0x%08X\n",
				      (unsigned int)
				      gccallbackinfo->info.callback.fn);
				GCDBG(GCZONE_BLIT,
				      "bltsville_param    = 0x%08X\n",
				      (unsigned int)
				      gccallbackinfo->info.callback.data);
			}

			gcicommit.asynchronous = true;
		}

		/* Process scheduled unmappings. */
		do_unmap_implicit(gcbatch);

		INIT_LIST_HEAD(&gcicommit.unmap);
		list_splice_init(&gcbatch->unmap, &gcicommit.unmap);

		/* Pass the batch for execution. */
		GCDUMPBATCH(gcbatch);

		gcicommit.gcerror = GCERR_NONE;
		gcicommit.entrypipe = GCPIPE_2D;
		gcicommit.exitpipe = GCPIPE_2D;

		INIT_LIST_HEAD(&gcicommit.buffer);
		list_splice_init(&gcbatch->buffer, &gcicommit.buffer);

		GCDBG(GCZONE_BLIT, "submitting the batch.\n");
		gc_commit_wrapper(&gcicommit);

		/* Move the lists back to the batch. */
		list_splice_init(&gcicommit.buffer, &gcbatch->buffer);
		list_splice_init(&gcicommit.unmap, &gcbatch->unmap);

		/* Error? */
		if (gcicommit.gcerror != GCERR_NONE) {
			switch (gcicommit.gcerror) {
			case GCERR_OODM:
			case GCERR_CTX_ALLOC:
				BVSETBLTERROR(BVERR_OOM,
					      "unable to allocate gccore "
					      "memory");
				goto exit;
			default:
				BVSETBLTERROR(BVERR_RSRC,
					      "gccore error");

				goto exit;
			}
		}

		GCDBG(GCZONE_BLIT, "batch is submitted.\n");
	}

exit:
	if ((gcbatch != NULL) && batchexec) {
		free_batch(gcbatch);
		bvbltparams->batch = NULL;
	}

	GCEXITARG(GCZONE_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror bv_cache(struct bvcopparams *copparams)
{
	enum bverror bverror = BVERR_NONE;
	unsigned int bytespp = 0; /* bytes per pixel */
	unsigned long vert_offset, horiz_offset;
	unsigned int true_width, true_height;

	struct c2dmrgn rgn[3];
	int container_size = 0;

	unsigned long subsample;
	unsigned long vendor;
	unsigned long layout;
	unsigned long size;
	unsigned long container;

	subsample = copparams->geom->format & OCDFMTDEF_SUBSAMPLE_MASK;
	vendor = copparams->geom->format & OCDFMTDEF_VENDOR_MASK;
	layout = copparams->geom->format & OCDFMTDEF_LAYOUT_MASK;
	size = copparams->geom->format & OCDFMTDEF_COMPONENTSIZEMINUS1_MASK;
	container = copparams->geom->format & OCDFMTDEF_CONTAINER_MASK;

	if (vendor != OCDFMTDEF_VENDOR_ALL) {
		bverror = BVERR_FORMAT;
		goto exit;
	}

	if (copparams->geom->orientation % 180 != 0) {
		true_width = copparams->rect->height;
		true_height = copparams->rect->width;
	} else {
		true_width = copparams->rect->width;
		true_height = copparams->rect->height;
	}

	switch (container) {
	case OCDFMTDEF_CONTAINER_8BIT:
		container_size = 8;
		break;

	case OCDFMTDEF_CONTAINER_16BIT:
		container_size = 16;
		break;

	case OCDFMTDEF_CONTAINER_24BIT:
		container_size = 24;
		break;

	case OCDFMTDEF_CONTAINER_32BIT:
		container_size = 32;
		break;

	case OCDFMTDEF_CONTAINER_48BIT:
		container_size = 48;
		break;

	case OCDFMTDEF_CONTAINER_64BIT:
		container_size = 64;
		break;
	}

	switch (layout) {
	case OCDFMTDEF_PACKED:
		switch (subsample) {
		case OCDFMTDEF_SUBSAMPLE_NONE:
			if (size >= 8) {
				bytespp = container_size / 8;
			} else {
				GCERR("format not supported.\n");
				bverror = BVERR_FORMAT;
				goto exit;
			}
			break;

		case OCDFMTDEF_SUBSAMPLE_422_YCbCr:
			bytespp = (container_size / 2) / 8;
			break;

		default:
			bverror = BVERR_FORMAT;
			goto exit;
		}

		rgn[0].span = true_width * bytespp;
		rgn[0].lines = true_height;
		rgn[0].stride = copparams->geom->virtstride;
		horiz_offset = copparams->rect->left * bytespp;
		vert_offset = copparams->rect->top;

		rgn[0].start = (void *) ((unsigned long)
				copparams->desc->virtaddr +
				vert_offset * rgn[0].stride +
				horiz_offset);

		gcbvcacheop(1, rgn, copparams->cacheop);
		break;

	case OCDFMTDEF_2_PLANE_YCbCr:
		/* 1 byte per pixel */
		rgn[0].span = true_width;
		rgn[0].lines = true_height;
		rgn[0].stride = copparams->geom->virtstride;
		rgn[0].start = (void *)
			((unsigned long) copparams->desc->virtaddr +
			 copparams->rect->top * rgn[0].stride +
			 copparams->rect->left);

		rgn[1].span = true_width;
		rgn[1].lines = true_height / 2;
		rgn[1].stride = copparams->geom->virtstride;
		rgn[1].start = rgn[0].start +
			copparams->geom->height * rgn[0].stride;

		GCDBG(GCZONE_CACHE,
		      "virtaddr %p start[0] 0x%08x start[1] 0x%08x\n",
		      copparams->desc->virtaddr, rgn[0].start, rgn[1].start);

		gcbvcacheop(2, rgn, copparams->cacheop);
		break;

	default:
		GCERR("format 0x%x (%d) not supported.\n",
		      copparams->geom->format, copparams->geom->format);
		bverror = BVERR_FORMAT;
		break;
	}

exit:
	if (bverror != BVERR_NONE)
		GCERR("bverror = %d\n", bverror);

	return bverror;
}
