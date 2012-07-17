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

#include "gcmain.h"

#define GCZONE_NONE		0
#define GCZONE_ALL		(~0U)
#define GCZONE_MAPPING		(1 << 0)
#define GCZONE_BUFFER_ALLOC	(1 << 1)
#define GCZONE_BUFFER		(1 << 2)
#define GCZONE_FIXUP		(1 << 3)
#define GCZONE_FORMAT		(1 << 4)
#define GCZONE_COLOR		(1 << 5)
#define GCZONE_BLEND		(1 << 6)
#define GCZONE_DEST		(1 << 7)
#define GCZONE_SURF		(1 << 8)
#define GCZONE_DO_FILL		(1 << 9)
#define GCZONE_DO_BLIT		(1 << 10)
#define GCZONE_DO_FILTER	(1 << 11)
#define GCZONE_BATCH		(1 << 12)
#define GCZONE_BLIT		(1 << 13)
#define GCZONE_CACHE		(1 << 14)

GCDBG_FILTERDEF(gcbv, GCZONE_NONE,
		"mapping",
		"buffer",
		"fixup",
		"format",
		"color",
		"blend",
		"dest",
		"surf",
		"do_fill",
		"do_blit",
		"do_filter",
		"batch",
		"blit",
		"cache")


/*******************************************************************************
** Miscellaneous defines and macros.
*/

#if !defined(BVBATCH_DESTRECT)
#define BVBATCH_DESTRECT (BVBATCH_DSTRECT_ORIGIN | BVBATCH_DSTRECT_SIZE)
#endif

#if !defined(BVBATCH_SRC1RECT)
#define BVBATCH_SRC1RECT (BVBATCH_SRC1RECT_ORIGIN | BVBATCH_SRC1RECT_SIZE)
#endif

#if !defined(BVBATCH_SRC2RECT)
#define BVBATCH_SRC2RECT (BVBATCH_SRC2RECT_ORIGIN | BVBATCH_SRC2RECT_SIZE)
#endif

#define EQ_SIZE(rect1, rect2) \
( \
	(rect1->width == rect2->width) && (rect1->height == rect2->height) \
)

#define STRUCTSIZE(structptr, lastmember) \
( \
	(size_t) &structptr->lastmember + \
	sizeof(structptr->lastmember) - \
	(size_t) structptr \
)

#define GET_MAP_HANDLE(map) \
( \
	((struct bvbuffmapinfo *) map->handle)->handle \
)

#define GC_CLIP_RESET_LEFT	((unsigned short) 0)
#define GC_CLIP_RESET_TOP	((unsigned short) 0)
#define GC_CLIP_RESET_RIGHT	((unsigned short) ((1 << 15) - 1))
#define GC_CLIP_RESET_BOTTOM	((unsigned short) ((1 << 15) - 1))

#define GPU_CMD_SIZE	(sizeof(unsigned int) * 2)

#define GC_BUFFER_RESERVE \
( \
	sizeof(struct gcmopipesel) + \
	sizeof(struct gcmommumaster) + \
	sizeof(struct gcmommuflush) + \
	sizeof(struct gcmosignal) + \
	sizeof(struct gccmdend) \
)

#if GCDEBUG_ENABLE
#define GCDUMPSCHEDULE() { \
	struct list_head *__temphead__; \
	struct gcschedunmap *__tempunmap__; \
	GCDBG(GCZONE_MAPPING, "scheduled unmaps:\n"); \
	list_for_each(__temphead__, &batch->unmap) { \
		__tempunmap__ = list_entry(__temphead__, \
					   struct gcschedunmap, \
					   link); \
		GCDBG(GCZONE_MAPPING, \
		      "  handle = 0x%08X\n", \
		      (unsigned int) __tempunmap__->handle); \
	} \
}
#else
#define GCDUMPSCHEDULE()
#endif


/*******************************************************************************
** Internal structures.
*/

struct gcblendconfig {
	unsigned char factor_mode;
	unsigned char color_reverse;

	unsigned char src1used;
	unsigned char src2used;
};

struct bvblendxlate {
	unsigned char match1;
	unsigned char match2;

	struct gcblendconfig k1;
	struct gcblendconfig k2;
};

/* Alpha blending descriptor. */
struct gcalpha {
	unsigned int src_global_color;
	unsigned int dst_global_color;

	unsigned char src_global_alpha_mode;
	unsigned char dst_global_alpha_mode;

	struct gcblendconfig *k1;
	struct gcblendconfig *k2;

	struct gcblendconfig *srcconfig;
	struct gcblendconfig *dstconfig;

	unsigned int src1used;
	unsigned int src2used;
};

/* Used by blitters to define an array of valid sources. */
struct srcinfo {
	/* BLTsville source index (0 for src1 and 1 for src2). */
	int index;

	/* Source surface buffer descriptor. */
	union bvinbuff buf;

	/* Source surface geometry. */
	struct bvsurfgeom *geom;

	/* Source rectangle. */
	struct bvrect *rect;

	/* Source surface format. */
	struct bvformatxlate *format;

	/* Source rotation angle. */
	int angle;
	unsigned int rot;

	/* Mirror setting. */
	unsigned int mirror;

	/* ROP. */
	unsigned short rop;

	/* Blending info. */
	struct gcalpha *gca;
};

/* bvbuffmap struct attachment. */
struct bvbuffmapinfo {
	/* Mapped handle for the buffer. */
	unsigned long handle;

	/* Number of times the client explicitly mapped this buffer. */
	int usermap;

	/* Number of times implicit mapping happened. */
	int automap;
};

/* Operation finalization call. */
struct gcbatch;
typedef enum bverror (*gcbatchend) (struct bvbltparams *bltparams,
				    struct gcbatch *batch);

/* Blit states. */
struct gcblit {
	unsigned int srccount;
	unsigned int multisrc;
	unsigned short rop;
};

/* Batch header. */
struct gcbatch {
	/* Used to ID structure version. */
	unsigned int structsize;

	/* Batch change flags. */
	unsigned long batchflags;

	/* Pointer to the function to finalize the current operation. */
	gcbatchend batchend;

	/* State of the current operation. */
	struct gcblit gcblit;

	/* Destination format. */
	struct bvformatxlate *dstformat;

	/* Clipping deltas; used to correct the source coordinates for
	 * single source blits. */
	int deltaleft;
	int deltatop;
	int deltaright;
	int deltabottom;

	/* Clipped destination rectangle coordinates. */
	unsigned short clippedleft;
	unsigned short clippedtop;
	unsigned short clippedright;
	unsigned short clippedbottom;

	/* Destination base address alignment in pixels. */
	int dstalign;

	/* Destination origin offset. */
	unsigned int dstoffsetX;
	unsigned int dstoffsetY;

	/* Rotation angle. */
	int dstangle;

	/* Geometry size of the destination surface. */
	unsigned int dstwidth;
	unsigned int dstheight;

	/* Physical size of the destination surface. */
	unsigned int dstphyswidth;
	unsigned int dstphysheight;

	/* Computed destination rectangle coordinates; in multi-source
	 * setup can be modified to match new destination and source
	 * geometry. */
	unsigned short left;
	unsigned short top;
	unsigned short right;
	unsigned short bottom;

	/* Physical size of the matched destination and source surfaces
	 * for multi-source setup. */
	unsigned int physwidth;
	unsigned int physheight;

	/* Alignment byte offset for the destination surface; in multi-
	 * source setup can be modified to match new destination and source
	 * geometry. */
	int dstbyteshift;

	/* Block walker enable. */
	int blockenable;

#if GCDEBUG_ENABLE
	/* Rectangle validation storage. */
	struct bvrect prevdstrect;
	struct bvrect prevsrc1rect;
	struct bvrect prevsrc2rect;
	struct bvrect prevmaskrect;
#endif

	/* Total size of the command buffer. */
	unsigned int size;

	/* Command buffer list. */
	struct gcbuffer *bufhead;
	struct gcbuffer *buftail;

	/* Scheduled implicit unmappings (gcschedunmap). */
	struct list_head unmap;
};

/* Vacant batch header. */
struct gcvacbatch {
	struct gcvacbatch *next;
};

/* Callback information. */
struct gccallbackinfo {
	/* BLTsville callback function. */
	void (*callbackfn) (struct bvcallbackerror *err,
			    unsigned long callbackdata);

	/* Callback data. */
	unsigned long callbackdata;

	/* Previous/next callback information. */
	struct list_head link;
};

/* Driver context. */
struct gccontext {
	/* Last generated error message. */
	char bverrorstr[128];

	/* Dynamically allocated structure cache. */
	struct list_head vac_unmap;		/* gcschedunmap */
	struct bvbuffmap *vac_buffmap;
	struct gcbuffer *vac_buffers;
	struct gcfixup *vac_fixups;
	struct gcvacbatch *vac_batches;

	/* Callback lists. */
	struct list_head callbacklist;
	struct list_head callbackvac;

	/* Access locks. */
	GCLOCK_TYPE batchlock;
	GCLOCK_TYPE bufferlock;
	GCLOCK_TYPE fixuplock;
	GCLOCK_TYPE maplock;
	GCLOCK_TYPE callbacklock;
};

static struct gccontext gccontext;


/*******************************************************************************
 * Debugging.
 */

#if GCDEBUG_ENABLE
#define GCDUMPBATCH(batch) \
	dumpbatch(batch)

static void dumpbatch(struct gcbatch *batch)
{
	if ((GCDBGFILTER.zone & (GCZONE_BUFFER)) != 0) {
		struct gcbuffer *buffer;
		unsigned int i, size;
		struct gcfixup *fixup;

		GCDBG(GCZONE_BUFFER, "BATCH DUMP (0x%08X)\n",
			(unsigned int) batch);

		buffer = batch->bufhead;
		while (buffer != NULL) {
			fixup = buffer->fixuphead;
			while (fixup != NULL) {
				GCDBG(GCZONE_BUFFER,
					"  Fixup table @ 0x%08X, count = %d:\n",
					(unsigned int) fixup, fixup->count);

				for (i = 0; i < fixup->count; i += 1) {
					GCDBG(GCZONE_BUFFER, "  [%02d]"
						" buffer offset = 0x%08X,"
						" surface offset = 0x%08X\n",
						i,
						fixup->fixup[i].dataoffset * 4,
						fixup->fixup[i].surfoffset);
				}

				fixup = fixup->next;
			}

			size = (unsigned char *) buffer->tail
				- (unsigned char *) buffer->head;
			GCDUMPBUFFER(GCZONE_BUFFER, buffer->head, 0, size);

			buffer = buffer->next;
		}
	}
}
#else
#	define GCDUMPBATCH(...)
#endif


/*******************************************************************************
 * Error handling.
 */

#define BVSETERROR(error, message, ...) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		 message, ##__VA_ARGS__); \
	GCDUMPSTRING("%s(%d): [ERROR] %s\n", __func__, __LINE__, \
		     gccontext.bverrorstr); \
	bverror = error; \
} while (0)

#define BVSETBLTERROR(error, message, ...) \
do { \
	BVSETERROR(error, message, ##__VA_ARGS__); \
	bltparams->errdesc = gccontext.bverrorstr; \
} while (0)

#define BVSETBLTSURFERROR(errorid, errordesc) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		 g_surferr[errorid].message, errordesc.id); \
	GCDUMPSTRING("%s(%d): [ERROR] %s\n", __func__, __LINE__, \
		     gccontext.bverrorstr); \
	bverror = errordesc.base + g_surferr[errorid].offset; \
	bltparams->errdesc = gccontext.bverrorstr; \
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

static enum bverror get_callbackinfo(struct gccallbackinfo **gccallbackinfo)
{
	enum bverror bverror;
	struct gccallbackinfo *temp;

	/* Lock access to callback info lists. */
	GCLOCK(&gccontext.callbacklock);

	if (list_empty(&gccontext.callbackvac)) {
		temp = gcalloc(struct gccallbackinfo,
			       sizeof(struct gccallbackinfo));
		if (temp == NULL) {
			bverror = BVERR_OOM;
			goto exit;
		}
		list_add(&temp->link, &gccontext.callbacklist);
	} else {
		struct list_head *head;
		head = gccontext.callbackvac.next;
		temp = list_entry(head, struct gccallbackinfo, link);
		list_move(head, &gccontext.callbacklist);
	}

	*gccallbackinfo = temp;
	bverror = BVERR_NONE;

exit:
	/* Unlock access to callback info lists. */
	GCUNLOCK(&gccontext.callbacklock);

	return bverror;
}

static void free_callback(struct gccallbackinfo *gccallbackinfo)
{
	/* Lock access to callback info lists. */
	GCLOCK(&gccontext.callbacklock);

	list_move(&gccallbackinfo->link, &gccontext.callbackvac);

	/* Unlock access to callback info lists. */
	GCUNLOCK(&gccontext.callbacklock);
}

void gccallback(void *callbackparam)
{
	struct gccallbackinfo *gccallbackinfo;

	GCENTER(GCZONE_BLIT);

	gccallbackinfo = (struct gccallbackinfo *) callbackparam;
	GCDBG(GCZONE_BLIT, "bltsville_callback = 0x%08X\n",
		(unsigned int) gccallbackinfo->callbackfn);
	GCDBG(GCZONE_BLIT, "bltsville_param    = 0x%08X\n",
		(unsigned int) gccallbackinfo->callbackdata);

	gccallbackinfo->callbackfn(NULL, gccallbackinfo->callbackdata);
	free_callback(gccallbackinfo);

	GCEXIT(GCZONE_BLIT);
}


/*******************************************************************************
 * Memory management.
 */

static enum bverror do_map(struct bvbuffdesc *bvbuffdesc,
			   struct gcbatch *batch,
			   struct bvbuffmap **map)
{
	static const int mapsize
		= sizeof(struct bvbuffmap)
		+ sizeof(struct bvbuffmapinfo);

	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;
	struct bvphysdesc *bvphysdesc;
	bool mappedbyothers;
	struct gcmap gcmap;
	struct gcschedunmap *gcschedunmap;

	GCENTERARG(GCZONE_MAPPING, "bvbuffdesc = 0x%08X\n",
		   (unsigned int) bvbuffdesc);

	/* Lock access to the mapping list. */
	GCLOCK(&gccontext.maplock);

	/* Try to find existing mapping. */
	bvbuffmap = bvbuffdesc->map;
	while (bvbuffmap != NULL) {
		if (bvbuffmap->bv_unmap == bv_unmap)
			break;
		bvbuffmap = bvbuffmap->nextmap;
	}

	/* Not mapped yet? */
	if (bvbuffmap == NULL) {
		/* New mapping, allocate a record. */
		if (gccontext.vac_buffmap == NULL) {
			bvbuffmap = gcalloc(struct bvbuffmap, mapsize);
			if (bvbuffmap == NULL) {
				BVSETERROR(BVERR_OOM,
					   "failed to allocate mapping record");
				goto fail;
			}

			bvbuffmap->structsize = sizeof(struct bvbuffmap);
			bvbuffmap->bv_unmap = bv_unmap;
			bvbuffmap->handle = (unsigned long) (bvbuffmap + 1);
		} else {
			bvbuffmap = gccontext.vac_buffmap;
			gccontext.vac_buffmap = bvbuffmap->nextmap;
		}

		/* Setup buffer mapping. Here we need to check and make sure
		 * that the buffer starts at a location that is supported by
		 * the hw. If it is not, offset is computed and the buffer is
		 * extended by the value of the offset. */
		gcmap.gcerror = GCERR_NONE;
		gcmap.handle = 0;

		if (bvbuffdesc->auxtype == BVAT_PHYSDESC) {
			bvphysdesc = (struct bvphysdesc *) bvbuffdesc->auxptr;

			if (bvphysdesc->structsize <
			    STRUCTSIZE(bvphysdesc, pageoffset)) {
				BVSETERROR(BVERR_BUFFERDESC_VERS,
					   "unsupported bvphysdesc version");
				goto fail;
			}

			gcmap.buf.offset = bvphysdesc->pageoffset;
			gcmap.pagesize = bvphysdesc->pagesize;
			gcmap.pagearray = bvphysdesc->pagearray;
			gcmap.size = bvbuffdesc->length;

			GCDBG(GCZONE_MAPPING, "new mapping (%s):\n",
			      (batch == NULL) ? "explicit" : "implicit");
			GCDBG(GCZONE_MAPPING, "pagesize = %lu\n",
			      bvphysdesc->pagesize);
			GCDBG(GCZONE_MAPPING, "pagearray = 0x%08X\n",
			      (unsigned int) bvphysdesc->pagearray);
			GCDBG(GCZONE_MAPPING, "pageoffset = %lu\n",
			      bvphysdesc->pageoffset);
			GCDBG(GCZONE_MAPPING, "mapping size = %d\n",
			      gcmap.size);
		} else {
			gcmap.buf.logical = bvbuffdesc->virtaddr;
			gcmap.pagesize = 0;
			gcmap.pagearray = NULL;
			gcmap.size = bvbuffdesc->length;

			GCDBG(GCZONE_MAPPING, "new mapping (%s):\n",
			      (batch == NULL) ? "explicit" : "implicit");
			GCDBG(GCZONE_MAPPING, "specified virtaddr = 0x%08X\n",
			      (unsigned int) bvbuffdesc->virtaddr);
			GCDBG(GCZONE_MAPPING, "aligned virtaddr = 0x%08X\n",
			      (unsigned int) gcmap.buf.logical);
			GCDBG(GCZONE_MAPPING, "mapping size = %d\n",
			      gcmap.size);
		}

		gc_map_wrapper(&gcmap);
		if (gcmap.gcerror != GCERR_NONE) {
			BVSETERROR(BVERR_OOM,
				   "unable to allocate gccore memory");
			goto fail;
		}

		/* Set map handle. */
		bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;
		bvbuffmapinfo->handle = gcmap.handle;

		/* Initialize reference counters. */
		if (batch == NULL) {
			/* Explicit mapping. */
			bvbuffmapinfo->usermap = 1;
			bvbuffmapinfo->automap = 0;
		} else {
			/* Implicit mapping; if there are existing mappings
			 * from other implementations, mark this an explicit
			 * mapping as well. */
			mappedbyothers = (bvbuffdesc->map != NULL);
			GCDBG(GCZONE_MAPPING, "%smapped by others.\n",
			      mappedbyothers ? "" : "not ");

			bvbuffmapinfo->usermap = mappedbyothers ? 1 : 0;
			bvbuffmapinfo->automap = 1;
		}

		/* Add the record to the list of mappings. */
		bvbuffmap->nextmap = bvbuffdesc->map;
		bvbuffdesc->map = bvbuffmap;
	} else {
		/* Mapping already exists. */
		bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

		/* Advance reference counters. */
		if (batch == NULL) {
			/* Explicit mapping. */
			GCDBG(GCZONE_MAPPING, "explicit map.\n");
			bvbuffmapinfo->usermap += 1;
		} else {
			/* Implicit mapping. */
			GCDBG(GCZONE_MAPPING, "implicit map.\n");
			bvbuffmapinfo->automap += 1;
		}

		GCDBG(GCZONE_MAPPING, "mapping exists.\n");
	}

	GCDBG(GCZONE_MAPPING, "bvbuffmap = 0x%08X\n",
		(unsigned int) bvbuffmap);
	GCDBG(GCZONE_MAPPING, "explicit count = %d\n",
		bvbuffmapinfo->usermap);
	GCDBG(GCZONE_MAPPING, "implicit count = %d\n",
		bvbuffmapinfo->automap);

	/* Schedule for unmapping. */
	if (batch != NULL) {
		if (list_empty(&gccontext.vac_unmap)) {
			gcschedunmap = gcalloc(struct gcschedunmap,
					       sizeof(struct gcschedunmap));
			if (gcschedunmap == NULL) {
				BVSETERROR(BVERR_OOM,
					   "failed to schedule unmapping");
				goto fail;
			}
			list_add(&gcschedunmap->link, &batch->unmap);
		} else {
			struct list_head *head;
			head = gccontext.vac_unmap.next;
			gcschedunmap = list_entry(head,
						  struct gcschedunmap,
						  link);
			list_move(&gcschedunmap->link, &batch->unmap);
		}

		gcschedunmap->handle = (unsigned long) bvbuffdesc;

		GCDBG(GCZONE_MAPPING, "scheduled for unmapping.\n");
		GCDUMPSCHEDULE();
	}

	/* Set the map pointer. */
	*map = bvbuffmap;

	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext.maplock);

	GCEXITARG(GCZONE_MAPPING, "handle = 0x%08X\n",
		  bvbuffmapinfo->handle);
	return BVERR_NONE;

fail:
	if (bvbuffmap != NULL) {
		bvbuffmap->nextmap = gccontext.vac_buffmap;
		gccontext.vac_buffmap = bvbuffmap;
	}

	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext.maplock);

	GCEXITARG(GCZONE_MAPPING, "bverror = %d\n", bverror);
	return bverror;
}

static void unmap_implicit(struct gcbatch *batch)
{
	struct list_head *head, *temphead;
	struct gcschedunmap *gcschedunmap;
	struct bvbuffdesc *bvbuffdesc;
	struct bvbuffmap *prev, *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;

	GCENTER(GCZONE_MAPPING);

	/* Lock access to the mapping list. */
	GCLOCK(&gccontext.maplock);

	/* Dump the schedule. */
	GCDUMPSCHEDULE();

	/* Scan scheduled unmappings and remove the ones that are still
	 * in use. */
	list_for_each_safe(head, temphead, &batch->unmap) {
		gcschedunmap = list_entry(head, struct gcschedunmap, link);

		/* Cast the handle. */
		bvbuffdesc = (struct bvbuffdesc *) gcschedunmap->handle;

		/* Find our mapping. */
		prev = NULL;
		bvbuffmap = bvbuffdesc->map;
		while (bvbuffmap != NULL) {
			if (bvbuffmap->bv_unmap == bv_unmap)
				break;
			prev = bvbuffmap;
			bvbuffmap = bvbuffmap->nextmap;
		}

		/* Not found? */
		if (bvbuffmap == NULL) {
			GCERR("mapping not found for bvbuffdesc 0x%08X.\n",
			      (unsigned int) bvbuffdesc);

			/* Remove scheduled unmapping. */
			list_move(head, &gccontext.vac_unmap);
			continue;
		}

		/* Get the info structure. */
		bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

		GCDBG(GCZONE_MAPPING, "head = 0x%08X\n",
		      (unsigned int) gcschedunmap);
		GCDBG(GCZONE_MAPPING, "  bvbuffmap = 0x%08X\n",
		      (unsigned int) bvbuffmap);
		GCDBG(GCZONE_MAPPING, "  handle = 0x%08X\n",
		      bvbuffmapinfo->handle);

		/* Implicit unmapping. */
		bvbuffmapinfo->automap -= 1;
		if (bvbuffmapinfo->automap < 0) {
			GCERR("implicit count negative.\n");
			bvbuffmapinfo->automap = 0;
		}

		GCDBG(GCZONE_MAPPING, "  explicit count = %d\n",
			bvbuffmapinfo->usermap);
		GCDBG(GCZONE_MAPPING, "  implicit count = %d\n",
			bvbuffmapinfo->automap);

		/* Still referenced? */
		if (bvbuffmapinfo->usermap || bvbuffmapinfo->automap) {
			GCDBG(GCZONE_MAPPING, "  still referenced.\n");

			/* Remove scheduled unmapping. */
			list_move(head, &gccontext.vac_unmap);
			continue;
		}

		GCDBG(GCZONE_MAPPING, "  ready for unmapping.\n");

		/* Set the handle. */
		gcschedunmap->handle = bvbuffmapinfo->handle;

		/* Remove from the buffer descriptor. */
		if (prev == NULL)
			bvbuffdesc->map = bvbuffmap->nextmap;
		else
			prev->nextmap = bvbuffmap->nextmap;

		/* Add to the vacant list. */
		bvbuffmap->nextmap = gccontext.vac_buffmap;
		gccontext.vac_buffmap = bvbuffmap;
	}

	/* Dump the schedule. */
	GCDUMPSCHEDULE();

	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext.maplock);

	GCEXIT(GCZONE_MAPPING);
}


/*******************************************************************************
 * Batch memory manager.
 */

static enum bverror allocate_batch(struct bvbltparams *bltparams,
				   struct gcbatch **batch);
static void free_batch(struct gcbatch *batch);
static enum bverror append_buffer(struct bvbltparams *bltparams,
				  struct gcbatch *batch);

static enum bverror do_end(struct bvbltparams *bltparams,
			   struct gcbatch *batch)
{
	return BVERR_NONE;
}

static enum bverror allocate_batch(struct bvbltparams *bltparams,
				   struct gcbatch **batch)
{
	enum bverror bverror;
	struct gcbatch *temp;

	GCENTER(GCZONE_BATCH);

	/* Lock access to batch management. */
	GCLOCK(&gccontext.batchlock);

	if (gccontext.vac_batches == NULL) {
		temp = gcalloc(struct gcbatch, sizeof(struct gcbatch));
		if (temp == NULL) {
			BVSETBLTERROR(BVERR_OOM,
				      "batch header allocation failed");
			goto exit;
		}

		GCDBG(GCZONE_BATCH, "allocated new batch = 0x%08X\n",
		      (unsigned int) temp);
	} else {
		temp = (struct gcbatch *) gccontext.vac_batches;
		gccontext.vac_batches = gccontext.vac_batches->next;

		GCDBG(GCZONE_BATCH, "reusing batch = 0x%08X\n",
		      (unsigned int) temp);
	}

	memset(temp, 0, sizeof(struct gcbatch));
	temp->structsize = sizeof(struct gcbatch);
	temp->batchend = do_end;
	INIT_LIST_HEAD(&temp->unmap);

	bverror = append_buffer(bltparams, temp);
	if (bverror != BVERR_NONE) {
		free_batch(temp);
		goto exit;
	}

	*batch = temp;

	GCDBG(GCZONE_BATCH, "batch allocated = 0x%08X\n", (unsigned int) temp);

exit:
	/* Unlock access to batch management. */
	GCUNLOCK(&gccontext.batchlock);

	GCEXITARG(GCZONE_BATCH, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static void free_batch(struct gcbatch *batch)
{
	struct gcbuffer *buffer;

	GCENTERARG(GCZONE_BATCH, "batch = 0x%08X\n", (unsigned int) batch);

	/* Lock access. */
	GCLOCK(&gccontext.batchlock);
	GCLOCK(&gccontext.bufferlock);
	GCLOCK(&gccontext.fixuplock);
	GCLOCK(&gccontext.maplock);

	list_splice_init(&batch->unmap, &gccontext.vac_unmap);

	buffer = batch->bufhead;
	if (buffer != NULL) {
		do {
			if (buffer->fixuphead != NULL) {
				buffer->fixuptail->next = gccontext.vac_fixups;
				gccontext.vac_fixups = buffer->fixuphead;
			}
			buffer = buffer->next;
		} while (buffer != NULL);

		batch->buftail->next = gccontext.vac_buffers;
		gccontext.vac_buffers = batch->bufhead;
	}

	((struct gcvacbatch *) batch)->next = gccontext.vac_batches;
	gccontext.vac_batches = (struct gcvacbatch *) batch;

	/* Unlock access. */
	GCUNLOCK(&gccontext.maplock);
	GCUNLOCK(&gccontext.fixuplock);
	GCUNLOCK(&gccontext.bufferlock);
	GCUNLOCK(&gccontext.batchlock);

	GCEXIT(GCZONE_BATCH);
}

static enum bverror append_buffer(struct bvbltparams *bltparams,
				  struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcbuffer *temp;

	GCENTERARG(GCZONE_BUFFER_ALLOC, "batch = 0x%08X\n",
		   (unsigned int) batch);

	/* Lock access to buffer management. */
	GCLOCK(&gccontext.bufferlock);

	if (gccontext.vac_buffers == NULL) {
		temp = gcalloc(struct gcbuffer, GC_BUFFER_SIZE);
		if (temp == NULL) {
			BVSETBLTERROR(BVERR_OOM,
				      "command buffer allocation failed");
			goto exit;
		}

		GCDBG(GCZONE_BUFFER_ALLOC, "allocated new buffer = 0x%08X\n",
		      (unsigned int) temp);
	} else {
		temp = gccontext.vac_buffers;
		gccontext.vac_buffers = temp->next;

		GCDBG(GCZONE_BUFFER_ALLOC, "reusing buffer = 0x%08X\n",
		      (unsigned int) temp);
	}

	memset(temp, 0, sizeof(struct gcbuffer));
	temp->head =
	temp->tail = (unsigned int *) (temp + 1);
	temp->available = GC_BUFFER_SIZE - max(sizeof(struct gcbuffer),
					       GC_BUFFER_RESERVE);

	if (batch->bufhead == NULL)
		batch->bufhead = temp;
	else
		batch->buftail->next = temp;
	batch->buftail = temp;

	GCDBG(GCZONE_BUFFER_ALLOC, "new buffer appended = 0x%08X\n",
	      (unsigned int) temp);

	bverror = BVERR_NONE;

exit:
	/* Unlock access to buffer management. */
	GCUNLOCK(&gccontext.bufferlock);

	GCEXITARG(GCZONE_BUFFER_ALLOC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror add_fixup(struct bvbltparams *bltparams,
			      struct gcbatch *batch,
			      unsigned int *fixup,
			      unsigned int surfoffset)
{
	enum bverror bverror = BVERR_NONE;
	struct gcbuffer *buffer;
	struct gcfixup *temp;

	GCENTERARG(GCZONE_FIXUP, "batch = 0x%08X, fixup ptr = 0x%08X\n",
		   (unsigned int) batch, (unsigned int) fixup);

	/* Lock access to fixup management. */
	GCLOCK(&gccontext.fixuplock);

	buffer = batch->buftail;
	temp = buffer->fixuptail;

	GCDBG(GCZONE_FIXUP, "buffer = 0x%08X, fixup struct = 0x%08X\n",
	      (unsigned int) buffer, (unsigned int) temp);

	if ((temp == NULL) || (temp->count == GC_FIXUP_MAX)) {
		if (gccontext.vac_fixups == NULL) {
			temp = gcalloc(struct gcfixup, sizeof(struct gcfixup));
			if (temp == NULL) {
				BVSETBLTERROR(BVERR_OOM,
					      "fixup allocation failed");
				goto exit;
			}

			GCDBG(GCZONE_FIXUP,
			      "new fixup struct allocated = 0x%08X\n",
			      (unsigned int) temp);
		} else {
			temp = gccontext.vac_fixups;
			gccontext.vac_fixups = temp->next;

			GCDBG(GCZONE_FIXUP, "fixup struct reused = 0x%08X\n",
			      (unsigned int) temp);
		}

		temp->next = NULL;
		temp->count = 0;

		if (buffer->fixuphead == NULL)
			buffer->fixuphead = temp;
		else
			buffer->fixuptail->next = temp;
		buffer->fixuptail = temp;

		GCDBG(GCZONE_FIXUP, "new fixup struct allocated = 0x%08X\n",
		      (unsigned int) temp);

	} else {
		GCDBG(GCZONE_FIXUP, "fixups accumulated = %d\n", temp->count);
	}

	temp->fixup[temp->count].dataoffset = fixup - buffer->head;
	temp->fixup[temp->count].surfoffset = surfoffset;
	temp->count += 1;

	GCDBG(GCZONE_FIXUP, "fixup offset = 0x%08X\n", fixup - buffer->head);
	GCDBG(GCZONE_FIXUP, "surface offset = 0x%08X\n", surfoffset);

exit:
	/* Unlock access to fixup management. */
	GCUNLOCK(&gccontext.fixuplock);

	GCEXITARG(GCZONE_FIXUP, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror claim_buffer(struct bvbltparams *bltparams,
				 struct gcbatch *batch,
				 unsigned int size,
				 void **buffer)
{
	enum bverror bverror;
	struct gcbuffer *curbuf;

	GCENTERARG(GCZONE_BUFFER_ALLOC, "batch = 0x%08X, size = %d\n",
		   (unsigned int) batch, size);

	/* Get the current command buffer. */
	curbuf = batch->buftail;

	GCDBG(GCZONE_BUFFER_ALLOC, "buffer = 0x%08X, available = %d\n",
	      (unsigned int) curbuf, curbuf->available);

	if (curbuf->available < size) {
		bverror = append_buffer(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		curbuf = batch->buftail;
	}

	if (curbuf->available < size) {
		GCDBG(GCZONE_BUFFER_ALLOC, "requested size is too large.\n");
		BVSETBLTERROR(BVERR_OOM,
			      "command buffer allocation failed");
		goto exit;
	}

	*buffer = curbuf->tail;
	curbuf->tail = (unsigned int *) ((unsigned char *) curbuf->tail + size);
	curbuf->available -= size;
	batch->size += size;
	bverror = BVERR_NONE;

exit:
	GCEXITARG(GCZONE_BUFFER_ALLOC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}


/*******************************************************************************
 * Pixel format parser.
 */

/* FIXME/TODO: change to use BLTsvile defines. */

#if defined(OCDFMTDEF_ALPHA_SHIFT)
#	undef OCDFMTDEF_ALPHA_SHIFT
#endif

#if defined(OCDFMTDEF_ALPHA_MASK)
#	undef OCDFMTDEF_ALPHA_MASK
#endif

#define OCDFMTDEF_ALPHA_SHIFT 18
#define OCDFMTDEF_ALPHA_MASK (1 << OCDFMTDEF_ALPHA_SHIFT)

#define OCDFMTDEF_PLACEMENT_SHIFT 9
#define OCDFMTDEF_PLACEMENT_MASK (3 << OCDFMTDEF_PLACEMENT_SHIFT)

#define OCDFMTDEF_BITS_SHIFT 3
#define OCDFMTDEF_BITS_MASK (3 << OCDFMTDEF_BITS_SHIFT)

#define OCDFMTDEF_BITS12 (0 << OCDFMTDEF_BITS_SHIFT)
#define OCDFMTDEF_BITS15 (1 << OCDFMTDEF_BITS_SHIFT)
#define OCDFMTDEF_BITS16 (2 << OCDFMTDEF_BITS_SHIFT)
#define OCDFMTDEF_BITS24 (3 << OCDFMTDEF_BITS_SHIFT)

enum bvformattype {
	BVFMT_RGB,
	BVFMT_YUV
};

struct bvcomponent {
	unsigned int shift;
	unsigned int size;
	unsigned int mask;
};

struct bvcsrgb {
	struct bvcomponent r;
	struct bvcomponent g;
	struct bvcomponent b;
	struct bvcomponent a;
};

struct bvformatxlate {
	enum bvformattype type;
	unsigned bitspp;
	unsigned format;
	unsigned swizzle;
	struct bvcsrgb rgba;
};

#define BVFORMATRGBA(BPP, Format, Swizzle, R, G, B, A) \
{ \
	BVFMT_RGB, \
	BPP, \
	GCREG_DE_FORMAT_ ## Format, \
	GCREG_DE_SWIZZLE_ ## Swizzle, \
	{ R, G, B, A } \
}

#define BVCOMP(Shift, Size) \
	{ Shift, Size, ((1 << Size) - 1) << Shift }

#define BVRED(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVGREEN(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVBLUE(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVALPHA(Shift, Size) \
	BVCOMP(Shift, Size)

#define BVFORMATINVALID \
	{ 0, 0, 0, 0, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } } }

static struct bvformatxlate g_format_nv12 = {
	.type = BVFMT_YUV,
	.bitspp = 8,
	.format = GCREG_DE_FORMAT_NV12,
};
static struct bvformatxlate g_format_uyvy = {
	.type = BVFMT_YUV,
	.bitspp = 16,
	.format = GCREG_DE_FORMAT_UYVY
};
static struct bvformatxlate g_format_yuy2 = {
	.type = BVFMT_YUV,
	.bitspp = 16,
	.format = GCREG_DE_FORMAT_YUY2
};

static struct bvformatxlate formatxlate[] = {
	/*  #0: OCDFMT_xRGB12
		BITS=12 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X4R4G4B4, ARGB,
		BVRED(8, 4), BVGREEN(4, 4), BVBLUE(0, 4), BVALPHA(12, 0)),

	/*  #1: OCDFMT_RGBx12
		BITS=12 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X4R4G4B4, RGBA,
		BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4, 4), BVALPHA(0, 0)),

	/*  #2: OCDFMT_xBGR12
		BITS=12 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X4R4G4B4, ABGR,
		BVRED(0, 4), BVGREEN(4, 4), BVBLUE(8, 4), BVALPHA(12, 0)),

	/*  #3: OCDFMT_BGRx12
		BITS=12 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X4R4G4B4, BGRA,
		BVRED(4, 4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0, 0)),

	/*  #4: OCDFMT_ARGB12
		BITS=12 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A4R4G4B4, ARGB,
		BVRED(8, 4), BVGREEN(4, 4), BVBLUE(0, 4), BVALPHA(12, 4)),

	/*  #5: OCDFMT_RGBA12
		BITS=12 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A4R4G4B4, RGBA,
		BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4, 4), BVALPHA(0, 4)),

	/*  #6: OCDFMT_ABGR12
		BITS=12 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A4R4G4B4, ABGR,
		BVRED(0, 4), BVGREEN(4, 4), BVBLUE(8, 4), BVALPHA(12, 4)),

	/*  #7: OCDFMT_BGRA12
		BITS=12 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A4R4G4B4, BGRA,
		BVRED(4, 4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0, 4)),

	/***********************************************/

	/*  #8: OCDFMT_xRGB15
		BITS=15 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X1R5G5B5, ARGB,
		BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0, 5), BVALPHA(15, 0)),

	/*  #9: OCDFMT_RGBx15
		BITS=15 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X1R5G5B5, RGBA,
		BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1, 5), BVALPHA(0, 0)),

	/* #10: OCDFMT_xBGR15
		BITS=15 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X1R5G5B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 0)),

	/* #11: OCDFMT_BGRx15
		BITS=15 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X1R5G5B5, BGRA,
		BVRED(1, 5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* #12: OCDFMT_ARGB15
		BITS=15 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A1R5G5B5, ARGB,
		BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0, 5), BVALPHA(15, 1)),

	/* #13: OCDFMT_RGBA15
		BITS=15 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A1R5G5B5, RGBA,
		BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1, 5), BVALPHA(0, 1)),

	/* #14: OCDFMT_ABGR15
		BITS=15 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A1R5G5B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 1)),

	/* #15: OCDFMT_BGRA15
		BITS=15 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A1R5G5B5, BGRA,
		BVRED(1, 5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0, 1)),

	/***********************************************/

	/* #16: OCDFMT_RGB16
		BITS=16 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, R5G6B5, ARGB,
		BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0, 5), BVALPHA(0, 0)),

	/* #17: OCDFMT_RGB16
		BITS=16 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, R5G6B5, ARGB,
		BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0, 5), BVALPHA(0, 0)),

	/* #18: OCDFMT_BGR16
		BITS=16 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, R5G6B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* #19: OCDFMT_BGR16
		BITS=16 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, R5G6B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* #20 */
	BVFORMATINVALID,

	/* #21 */
	BVFORMATINVALID,

	/* #22 */
	BVFORMATINVALID,

	/* #23 */
	BVFORMATINVALID,

	/***********************************************/

	/* #24: OCDFMT_xRGB24
		BITS=24 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, X8R8G8B8, BGRA,
		BVRED(8, 8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0, 0)),

	/* #25: OCDFMT_RGBx24
		BITS=24 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, X8R8G8B8, ABGR,
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(24, 0)),

	/* #26: OCDFMT_xBGR24
		BITS=24 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, X8R8G8B8, RGBA,
		BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8, 8), BVALPHA(0, 0)),

	/* #27: OCDFMT_BGRx24
		BITS=24 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, X8R8G8B8, ARGB,
		BVRED(16, 8), BVGREEN(8, 8), BVBLUE(0, 8), BVALPHA(24, 0)),

	/* #28: OCDFMT_ARGB24
		BITS=24 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, A8R8G8B8, BGRA,
		BVRED(8, 8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0, 8)),

	/* #29: OCDFMT_RGBA24
		BITS=24 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, A8R8G8B8, ABGR,
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(24, 8)),

	/* #30: OCDFMT_ABGR24
		BITS=24 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, A8R8G8B8, RGBA,
		BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8, 8), BVALPHA(0, 8)),

	/* #31: OCDFMT_BGRA24
		BITS=24 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, A8R8G8B8, ARGB,
		BVRED(16, 8), BVGREEN(8, 8), BVBLUE(0, 8), BVALPHA(24, 8)),
};

static int parse_format(enum ocdformat ocdformat, struct bvformatxlate **format)
{
	static unsigned int containers[] = {
		  8,	/* OCDFMTDEF_CONTAINER_8BIT */
		 16,	/* OCDFMTDEF_CONTAINER_16BIT */
		 24,	/* OCDFMTDEF_CONTAINER_24BIT */
		 32,	/* OCDFMTDEF_CONTAINER_32BIT */
		~0U,	/* reserved */
		 48,	/* OCDFMTDEF_CONTAINER_48BIT */
		~0U,	/* reserved */
		 64	/* OCDFMTDEF_CONTAINER_64BIT */
	};

	unsigned int cs;
	unsigned int bits;
	unsigned int swizzle;
	unsigned int alpha;
	unsigned int index;
	unsigned int cont;

	GCDBG(GCZONE_FORMAT, "ocdformat = 0x%08X\n", ocdformat);

	switch (ocdformat) {
	case OCDFMT_NV12:
		GCDBG(GCZONE_FORMAT, "OCDFMT_NV12\n");
		*format = &g_format_nv12;
		return 1;

	case OCDFMT_UYVY:
		GCDBG(GCZONE_FORMAT, "OCDFMT_UYVY\n");
		*format = &g_format_uyvy;
		return 1;

	case OCDFMT_YUY2:
		GCDBG(GCZONE_FORMAT, "OCDFMT_YUY2\n");
		*format = &g_format_yuy2;
		return 1;

	default:
		break;
	}

	cs = (ocdformat & OCDFMTDEF_CS_MASK) >> OCDFMTDEF_CS_SHIFT;
	bits = (ocdformat & OCDFMTDEF_COMPONENTSIZEMINUS1_MASK)
		>> OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT;
	cont = (ocdformat & OCDFMTDEF_CONTAINER_MASK)
		>> OCDFMTDEF_CONTAINER_SHIFT;

	switch (cs) {
	case (OCDFMTDEF_CS_RGB >> OCDFMTDEF_CS_SHIFT):
		GCDBG(GCZONE_FORMAT, "OCDFMTDEF_CS_RGB\n");
		GCDBG(GCZONE_FORMAT, "bits = %d\n", bits);
		GCDBG(GCZONE_FORMAT, "cont = %d\n", cont);

		if ((ocdformat & OCDFMTDEF_LAYOUT_MASK) != OCDFMTDEF_PACKED)
			return 0;

		swizzle = (ocdformat & OCDFMTDEF_PLACEMENT_MASK)
			>> OCDFMTDEF_PLACEMENT_SHIFT;
		alpha = (ocdformat & OCDFMTDEF_ALPHA_MASK)
			>> OCDFMTDEF_ALPHA_SHIFT;

		GCDBG(GCZONE_FORMAT, "swizzle = %d\n", swizzle);
		GCDBG(GCZONE_FORMAT, "alpha = %d\n", alpha);

		index = swizzle | (alpha << 2);

		switch (bits) {
		case 12 - 1:
			index |= OCDFMTDEF_BITS12;
			break;

		case 15 - 1:
			index |= OCDFMTDEF_BITS15;
			break;

		case 16 - 1:
			index |= OCDFMTDEF_BITS16;
			break;

		case 24 - 1:
			index |= OCDFMTDEF_BITS24;
			break;

		default:
			return 0;
		}

		GCDBG(GCZONE_FORMAT, "index = %d\n", index);
		break;

	default:
		return 0;
	}

	if (formatxlate[index].bitspp != containers[cont])
		return 0;

	*format = &formatxlate[index];

	GCDBG(GCZONE_FORMAT, "format record = 0x%08X\n",
		(unsigned int) &formatxlate[index]);
	GCDBG(GCZONE_FORMAT, "  bpp = %d\n", formatxlate[index].bitspp);
	GCDBG(GCZONE_FORMAT, "  format = %d\n", formatxlate[index].format);
	GCDBG(GCZONE_FORMAT, "  swizzle = %d\n", formatxlate[index].swizzle);

	return 1;
}

static inline unsigned int extract_component(unsigned int pixel,
						struct bvcomponent *desc)
{
	unsigned int component;
	unsigned int component8;

	component = (pixel & desc->mask) >> desc->shift;
	GCDBG(GCZONE_COLOR, "mask=0x%08X, shift=%d, component=0x%08X\n",
		desc->mask, desc->shift, component);

	switch (desc->size) {
	case 0:
		component8 = 0xFF;
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

	r = extract_component(srcpixel, &format->rgba.r);
	g = extract_component(srcpixel, &format->rgba.g);
	b = extract_component(srcpixel, &format->rgba.b);
	a = extract_component(srcpixel, &format->rgba.a);

	GCDBG(GCZONE_COLOR, "(r,g,b,a)=0x%02X,0x%02X,0x%02X,0x%02X\n",
		r, g, b, a);

	dstpixel = (a << 24) | (r << 16) | (g <<  8) | b;

	GCDBG(GCZONE_COLOR, "dstpixel=0x%08X\n", dstpixel);

	return dstpixel;
}


/*******************************************************************************
 * Alpha blending parser.
 */

#define BVBLENDMATCH(Mode, Inverse, Normal) \
( \
	BVBLENDDEF_ ## Mode | \
	BVBLENDDEF_ ## Inverse | \
	BVBLENDDEF_ ## Normal \
)

#define BVSRC1USE(Use) \
	Use

#define BVSRC2USE(Use) \
	Use

#define BVBLENDUNDEFINED() \
	{ ~0, ~0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }

static struct bvblendxlate blendxlate[64] = {
	/**********************************************************************/
	/* #0: color factor: 00 00 00 A:(1-C1,C1)=zero
	       alpha factor: zero ==> 00 00 00 */
	{
		0x00,
		0x00,

		{
			GCREG_BLENDING_MODE_ZERO,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(0), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_ZERO,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(0), BVSRC2USE(0)
		}
	},

	/* #1: color factor: 00 00 01 A:(1-C1,A1)=A1
	       alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #2: color factor: 00 00 10 A:(1-C1,C2)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #3: color factor: 00 00 11 A:(1-C1,A2)=A2
	       alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #4: color factor: 00 01 00 A:(1-A1,C1)=1-A1
	       alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #5: color factor: 00 01 01 A:(1-A1,A1)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #6: color factor: 00 01 10 A:(1-A1,C2)=1-A1
	       alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #7: color factor: 00 01 11 A:(1-A1,A2)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #8: color factor: 00 10 00 A:(1-C2,C1)=undefined
	       alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #9: color factor: 00 10 01 A:(1-C2,A1)=A1
	       alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #10: color factor: 00 10 10 A:(1-C2,C2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #11: color factor: 00 10 11 A:(1-C2,A2)=A2
		alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #12: color factor: 00 11 00 A:(1-A2,C1)=1-A2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #13: color factor: 00 11 01 A:(1-A2,A1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #14: color factor: 00 11 10 A:(1-A2,C2)=1-A2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #15: color factor: 00 11 11 A:(1-A2,A2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* #16: color factor: 01 00 00 MIN:(1-C1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #17: color factor: 01 00 01 MIN:(1-C1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #18: color factor: 01 00 10 MIN:(1-C1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #19: color factor: 01 00 11 MIN:(1-C1,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #20: color factor: 01 01 00 MIN:(1-A1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #21: color factor: 01 01 01 MIN:(1-A1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #22: color factor: 01 01 10 MIN:(1-A1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #23: color factor: 01 01 11 MIN:(1-A1,A2)
		alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_SATURATED_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #24: color factor: 01 10 00 MIN:(1-C2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #25: color factor: 01 10 01 MIN:(1-C2,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #26: color factor: 01 10 10 MIN:(1-C2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #27: color factor: 01 10 11 MIN:(1-C2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #28: color factor: 01 11 00 MIN:(1-A2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #29: color factor: 01 11 01 MIN:(1-A2,A1)
		alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			GCREG_BLENDING_MODE_SATURATED_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #30: color factor: 01 11 10 MIN:(1-A2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #31: color factor: 01 11 11 MIN:(1-A2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* #32: color factor: 10 00 00 MAX:(1-C1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #33: color factor: 10 00 01 MAX:(1-C1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #34: color factor: 10 00 10 MAX:(1-C1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #35: color factor: 10 00 11 MAX:(1-C1,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #36: color factor: 10 01 00 MAX:(1-A1,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #37: color factor: 10 01 01 MAX:(1-A1,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #38: color factor: 10 01 10 MAX:(1-A1,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #39: color factor: 10 01 11 MAX:(1-A1,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #40: color factor: 10 10 00 MAX:(1-C2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #41: color factor: 10 10 01 MAX:(1-C2,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #42: color factor: 10 10 10 MAX:(1-C2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #43: color factor: 10 10 11 MAX:(1-C2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #44: color factor: 10 11 00 MAX:(1-A2,C1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #45: color factor: 10 11 01 MAX:(1-A2,A1) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #46: color factor: 10 11 10 MAX:(1-A2,C2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #47: color factor: 10 11 11 MAX:(1-A2,A2) ==> not supported
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* #48: color factor: 11 00 00 C:(1-C1,C1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #49: color factor: 11 00 01 C:(1-C1,A1)=1-C1
		alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #50: color factor: 11 00 10 C:(1-C1,C2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #51: color factor: 11 00 11 C:(1-C1,A2)=1-C1
		alpha factor: 1-A1 ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #52: color factor: 11 01 00 C:(1-A1,C1)=C1
		alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #53: color factor: 11 01 01 C:(1-A1,A1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #54: color factor: 11 01 10 C:(1-A1,C2)=C2
		alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #55: color factor: 11 01 11 C:(1-A1,A2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #56: color factor: 11 10 00 C:(1-C2,C1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #57: color factor: 11 10 01 C:(1-C2,A1)=1-C2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #58: color factor: 11 10 10 C:(1-C2,C2)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #59: color factor: 11 10 11 C:(1-C2,A2)=1-C2
		alpha factor: 1-A2 ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(0)
		}
	},

	/* #60: color factor: 11 11 00 C:(1-A2,C1)=C1
		alpha factor: A1 ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1)
		}
	},

	/* #61: color factor: 11 11 01 C:(1-A2,A1)=undefined
		alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* #62: color factor: 11 11 10 C:(1-A2,C2)=C2
		alpha factor: A2 ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(1),
		},

		{
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	},

	/* #63: color factor: 11 11 11 C:(1-A2,A2)=one
		alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			GCREG_BLENDING_MODE_ONE,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(1), BVSRC2USE(0),
		},

		{
			GCREG_BLENDING_MODE_ONE,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVSRC1USE(0), BVSRC2USE(1)
		}
	}
};

static enum bverror parse_blend(struct bvbltparams *bltparams,
				enum bvblend blend, struct gcalpha *gca)
{
	enum bverror bverror;
	unsigned int global;
	unsigned int k1, k2, k3, k4;
	struct bvblendxlate *k1_xlate;
	struct bvblendxlate *k2_xlate;
	unsigned int alpha;

	GCDBG(GCZONE_BLEND, "blend = 0x%08X (%s)\n",
		blend, gc_bvblend_name(blend));

	if ((blend & BVBLENDDEF_REMOTE) != 0) {
		BVSETBLTERROR(BVERR_BLEND, "remote alpha not supported");
		goto exit;
	}

	global = (blend & BVBLENDDEF_GLOBAL_MASK) >> BVBLENDDEF_GLOBAL_SHIFT;

	switch (global) {
	case (BVBLENDDEF_GLOBAL_NONE >> BVBLENDDEF_GLOBAL_SHIFT):
		GCDBG(GCZONE_BLEND, "BVBLENDDEF_GLOBAL_NONE\n");

		gca->src_global_color =
		gca->dst_global_color = 0;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		break;

	case (BVBLENDDEF_GLOBAL_UCHAR >> BVBLENDDEF_GLOBAL_SHIFT):
		GCDBG(GCZONE_BLEND, "BVBLENDDEF_GLOBAL_UCHAR\n");

		gca->src_global_color =
		gca->dst_global_color =
			((unsigned int) bltparams->globalalpha.size8) << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;

	case (BVBLENDDEF_GLOBAL_FLOAT >> BVBLENDDEF_GLOBAL_SHIFT):
		GCDBG(GCZONE_BLEND, "BVBLENDDEF_GLOBAL_FLOAT\n");

		alpha = gcfp2norm8(bltparams->globalalpha.fp);

		gca->src_global_color =
		gca->dst_global_color = alpha << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;

	default:
		BVSETBLTERROR(BVERR_BLEND, "invalid global alpha mode");
		goto exit;
	}

	/*
		Co = k1 x C1 + k2 x C2
		Ao = k3 x A1 + k4 x A2
	*/

	k1 = (blend >> 18) & 0x3F;
	k2 = (blend >> 12) & 0x3F;
	k3 = (blend >>  6) & 0x3F;
	k4 =  blend        & 0x3F;

	GCDBG(GCZONE_BLEND, "k1 = %d\n", k1);
	GCDBG(GCZONE_BLEND, "k2 = %d\n", k2);
	GCDBG(GCZONE_BLEND, "k3 = %d\n", k3);
	GCDBG(GCZONE_BLEND, "k4 = %d\n", k4);

	k1_xlate = &blendxlate[k1];
	k2_xlate = &blendxlate[k2];

	if (((k3 != k1_xlate->match1) && (k3 != k1_xlate->match2)) ||
		((k4 != k2_xlate->match1) && (k4 != k2_xlate->match2))) {
		BVSETBLTERROR(BVERR_BLEND,
			      "not supported coefficient combination");
		goto exit;
	}

	gca->k1 = &k1_xlate->k1;
	gca->k2 = &k2_xlate->k2;

	gca->src1used = gca->k1->src1used | gca->k2->src1used;
	gca->src2used = gca->k1->src2used | gca->k2->src2used;

	bverror = BVERR_NONE;

exit:
	return bverror;
}


/*******************************************************************************
 * Rotation and mirror.
 */

#define BVFLAG_FLIP_MASK	0x00000003

#define BVFLAG_FLIP_SRC1_SHIFT	14
#define BVFLAG_FLIP_SRC2_SHIFT	16
#define BVFLAG_FLIP_MASK_SHIFT	18

#define GCREG_MIRROR_NONE	0x0
#define GCREG_MIRROR_X		0x1
#define GCREG_MIRROR_Y		0x2
#define GCREG_MIRROR_XY		0x3

#define GCREG_ROT_ANGLE_ROT0	0x0
#define GCREG_ROT_ANGLE_ROT90	0x4
#define GCREG_ROT_ANGLE_ROT180	0x5
#define GCREG_ROT_ANGLE_ROT270	0x6

#define ROT_ANGLE_INVALID	-1
#define ROT_ANGLE_0		0
#define ROT_ANGLE_90		1
#define ROT_ANGLE_180		2
#define ROT_ANGLE_270		3

/* NOTE: BLTsville rotation is defined conunter clock wise. */
static const unsigned int rotencoding[] = {
	GCREG_ROT_ANGLE_ROT0,		/* ROT_ANGLE_0 */
	GCREG_ROT_ANGLE_ROT270,		/* ROT_ANGLE_90 */
	GCREG_ROT_ANGLE_ROT180,		/* ROT_ANGLE_180 */
	GCREG_ROT_ANGLE_ROT90		/* ROT_ANGLE_270 */
};

static inline int get_angle(int orientation)
{
	int angle;

	/* Normalize the angle. */
	angle = orientation % 360;

	/* Flip to positive. */
	if (angle < 0)
		angle = 360 + angle;

	/* Translate the angle. */
	switch (angle) {
	case 0:   return ROT_ANGLE_0;
	case 90:  return ROT_ANGLE_90;
	case 180: return ROT_ANGLE_180;
	case 270: return ROT_ANGLE_270;
	}

	/* Not supported angle. */
	return ROT_ANGLE_INVALID;
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

static bool valid_geom(struct bvbuffdesc *buffdesc, struct bvsurfgeom *geom,
			struct bvformatxlate *format)
{
	unsigned int size;

	/* Compute the size of the surface. */
	size = (geom->width * geom->height * format->bitspp) / 8;

	/* Make sure the size is not greater then the surface. */
	if (size > buffdesc->length) {
		GCERR("invalid geometry detected:\n");
		GCERR("  specified dimensions: %dx%d, %d bitspp\n",
			geom->width, geom->height, format->bitspp);
		GCERR("  surface size based on the dimensions: %d\n",
			size);
		GCERR("  specified surface size: %lu\n",
			buffdesc->length);
		return false;
	}

	return true;
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

#if GCDEBUG_ENABLE
#	define VERIFYBATCH(changeflags, prevrect, currrect) \
		verify_batch(changeflags, prevrect, currrect)

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
#	define VERIFYBATCH(...)
#endif

static inline int get_pixeloffset(struct bvbuffdesc *bvbuffdesc,
				  struct bvformatxlate *format,
				  int offset)
{
	unsigned int alignment;
	int byteoffset;
	unsigned int alignedoffset;
	int pixeloffset;

	alignment = (format->type == BVFMT_YUV)
		? (64 - 1)
		: (16 - 1);

	/* Determine offset in bytes from the base modified by the
	 * given offset. */
	if (bvbuffdesc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *bvphysdesc;
		bvphysdesc = (struct bvphysdesc *) bvbuffdesc->auxptr;
		byteoffset = bvphysdesc->pageoffset + offset;
	} else {
		byteoffset = (unsigned int) bvbuffdesc->virtaddr + offset;
	}

	/* Compute the aligned offset. */
	alignedoffset = byteoffset & alignment;

	/* Convert to pixels. */
	pixeloffset = alignedoffset * 8 / format->bitspp;
	return -pixeloffset;
}

static enum bverror parse_destination(struct bvbltparams *bltparams,
				      struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;

	GCENTER(GCZONE_DEST);

	/* Did clipping/destination rects change? */
	if ((batch->batchflags & (BVBATCH_CLIPRECT |
				  BVBATCH_DESTRECT)) != 0) {
		struct bvrect *dstrect;
		struct bvrect *cliprect;
		int destleft, desttop, destright, destbottom;
		int clipleft, cliptop, clipright, clipbottom;

		/* Make shortcuts to the destination objects. */
		dstrect = &bltparams->dstrect;
		cliprect = &bltparams->cliprect;

		/* Determine destination rectangle. */
		destleft = dstrect->left;
		desttop = dstrect->top;
		destright = destleft + dstrect->width;
		destbottom = desttop + dstrect->height;

		GCDBG(GCZONE_DEST, "destination rectangle:\n");
		GCDBG(GCZONE_DEST, "  dstrect = (%d,%d)-(%d,%d), %dx%d\n",
			destleft, desttop, destright, destbottom,
			dstrect->width, dstrect->height);

		/* Determine clipping. */
		if ((bltparams->flags & BVFLAG_CLIP) == BVFLAG_CLIP) {
			clipleft = cliprect->left;
			cliptop = cliprect->top;
			clipright = clipleft + cliprect->width;
			clipbottom = cliptop + cliprect->height;

			if ((clipleft < GC_CLIP_RESET_LEFT) ||
				(cliptop < GC_CLIP_RESET_TOP) ||
				(clipright > GC_CLIP_RESET_RIGHT) ||
				(clipbottom > GC_CLIP_RESET_BOTTOM) ||
				(clipright < clipleft) ||
				(clipbottom < cliptop)) {
				BVSETBLTERROR(BVERR_CLIP_RECT,
					      "invalid clipping rectangle");
				goto exit;
			}

			GCDBG(GCZONE_DEST,
			      "  cliprect = (%d,%d)-(%d,%d), %dx%d\n",
			      clipleft, cliptop, clipright, clipbottom,
			      cliprect->width, cliprect->height);
		} else {
			clipleft = GC_CLIP_RESET_LEFT;
			cliptop = GC_CLIP_RESET_TOP;
			clipright = GC_CLIP_RESET_RIGHT;
			clipbottom = GC_CLIP_RESET_BOTTOM;
		}

		/* Compute clipping deltas and the adjusted destination rect. */
		if (clipleft <= destleft) {
			batch->deltaleft = 0;
			batch->clippedleft = destleft;
		} else {
			batch->deltaleft = clipleft - destleft;
			batch->clippedleft = clipleft;
		}

		if (cliptop <= desttop) {
			batch->deltatop = 0;
			batch->clippedtop = desttop;
		} else {
			batch->deltatop = cliptop - desttop;
			batch->clippedtop = cliptop;
		}

		if (clipright >= destright) {
			batch->deltaright = 0;
			batch->clippedright = destright;
		} else {
			batch->deltaright = clipright - destright;
			batch->clippedright = clipright;
		}

		if (clipbottom >= destbottom) {
			batch->deltabottom = 0;
			batch->clippedbottom = destbottom;
		} else {
			batch->deltabottom = clipbottom - destbottom;
			batch->clippedbottom = clipbottom;
		}

		/* Validate the rectangle. */
		if ((batch->clippedright  > (int) bltparams->dstgeom->width) ||
		    (batch->clippedbottom > (int) bltparams->dstgeom->height)) {
			BVSETBLTERROR(BVERR_DSTRECT,
				      "destination rect exceeds surface size");
			goto exit;
		}

		GCDBG(GCZONE_DEST,
		      "  clipped dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		      batch->clippedleft, batch->clippedtop,
		      batch->clippedright, batch->clippedbottom,
		      batch->clippedright - batch->clippedleft,
		      batch->clippedbottom - batch->clippedtop);
		GCDBG(GCZONE_DEST,
		      "  clipping delta = (%d,%d)-(%d,%d)\n",
		      batch->deltaleft, batch->deltatop,
		      batch->deltaright, batch->deltabottom);
	}

	/* Did the destination surface change? */
	if ((batch->batchflags & BVBATCH_DST) != 0) {
		struct bvbuffdesc *dstdesc;
		struct bvsurfgeom *dstgeom;
		unsigned int stridealign;

		/* Make shortcuts to the destination objects. */
		dstdesc = bltparams->dstdesc;
		dstgeom = bltparams->dstgeom;

		/* Check for unsupported dest formats. */
		switch (dstgeom->format) {
		case OCDFMT_NV12:
			BVSETBLTERROR(BVERR_DSTGEOM_FORMAT,
				      "destination format unsupported");
			goto exit;

		default:
			break;
		}

		/* Parse the destination format. */
		GCDBG(GCZONE_FORMAT, "parsing destination format.\n");
		if (!parse_format(dstgeom->format, &batch->dstformat)) {
			BVSETBLTERROR(BVERR_DSTGEOM_FORMAT,
				      "invalid destination format (%d)",
				      dstgeom->format);
			goto exit;
		}

		/* Validate geometry. */
		if (!valid_geom(dstdesc, dstgeom, batch->dstformat)) {
			BVSETBLTERROR(BVERR_DSTGEOM,
				      "destination geom exceeds surface size");
			goto exit;
		}

		/* Destination stride must be 8 pixel aligned. */
		stridealign = batch->dstformat->bitspp - 1;
		if ((dstgeom->virtstride & stridealign) != 0) {
			BVSETBLTERROR(BVERR_DSTGEOM_STRIDE,
				      "destination stride must be 8 pixel "
				      "aligned.");
			goto exit;
		}

		/* Parse orientation. */
		batch->dstangle = get_angle(dstgeom->orientation);
		if (batch->dstangle == ROT_ANGLE_INVALID) {
			BVSETBLTERROR(BVERR_DSTGEOM,
				      "unsupported destination orientation %d.",
				      dstgeom->orientation);
		}

		/* Compute the destination offset in pixels needed to compensate
		 * for the surface base address misalignment if any. */
		batch->dstalign = get_pixeloffset(dstdesc, batch->dstformat, 0);

		switch (batch->dstangle) {
		case ROT_ANGLE_0:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->width - batch->dstalign;
			batch->dstphysheight = dstgeom->height;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width - batch->dstalign;
			batch->dstheight = dstgeom->height;

			/* Determine the origin offset. */
			batch->dstoffsetX = -batch->dstalign;
			batch->dstoffsetY = 0;
			break;

		case ROT_ANGLE_90:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->height - batch->dstalign;
			batch->dstphysheight = dstgeom->width;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width;
			batch->dstheight = dstgeom->height - batch->dstalign;

			/* Determine the origin offset. */
			batch->dstoffsetX = 0;
			batch->dstoffsetY = -batch->dstalign;
			break;

		case ROT_ANGLE_180:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->width - batch->dstalign;
			batch->dstphysheight = dstgeom->height;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width - batch->dstalign;
			batch->dstheight = dstgeom->height;

			/* Determine the origin offset. */
			batch->dstoffsetX = 0;
			batch->dstoffsetY = 0;
			break;

		case ROT_ANGLE_270:
			/* Determine the physical size. */
			batch->dstphyswidth = dstgeom->height - batch->dstalign;
			batch->dstphysheight = dstgeom->width;

			/* Determine geometry size. */
			batch->dstwidth = dstgeom->width;
			batch->dstheight = dstgeom->height - batch->dstalign;

			/* Determine the origin offset. */
			batch->dstoffsetX = 0;
			batch->dstoffsetY = 0;
			break;
		}

		GCDBG(GCZONE_DEST, "destination surface:\n");
		GCDBG(GCZONE_SURF, "  rotation %d degrees.\n",
		      batch->dstangle * 90);

		if (dstdesc->auxtype == BVAT_PHYSDESC) {
			struct bvphysdesc *bvphysdesc;
			bvphysdesc = (struct bvphysdesc *) dstdesc->auxptr;
			GCDBG(GCZONE_DEST, "  page offset = 0x%08X\n",
			      bvphysdesc->pageoffset);
		} else {
			GCDBG(GCZONE_DEST, "  virtual address = 0x%08X\n",
			      (unsigned int) dstdesc->virtaddr);
		}

		GCDBG(GCZONE_DEST, "  stride = %ld\n",
		      dstgeom->virtstride);
		GCDBG(GCZONE_DEST, "  geometry size = %dx%d\n",
		      dstgeom->width, dstgeom->height);
		GCDBG(GCZONE_DEST, "  aligned geometry size = %dx%d\n",
		      batch->dstwidth, batch->dstheight);
		GCDBG(GCZONE_DEST, "  aligned physical size = %dx%d\n",
		      batch->dstphyswidth, batch->dstphysheight);
		GCDBG(GCZONE_DEST, "  origin offset (pixels) = %d,%d\n",
		      batch->dstoffsetX, batch->dstoffsetY);
		GCDBG(GCZONE_DEST, "  surface offset (pixels) = %d,0\n",
		      batch->dstalign);
	}

exit:
	GCEXITARG(GCZONE_DEST, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror parse_source(struct bvbltparams *bltparams,
				 struct gcbatch *batch,
				 struct srcinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;
	struct bvbuffdesc *srcdesc;
	struct bvsurfgeom *srcgeom;
	struct bvrect *srcrect;
	unsigned int stridealign;

	/* Make shortcuts to the source objects. */
	srcdesc = srcinfo->buf.desc;
	srcgeom = srcinfo->geom;
	srcrect = srcinfo->rect;

	/* Parse the source format. */
	GCDBG(GCZONE_FORMAT, "parsing source%d format.\n",
	      srcinfo->index + 1);
	if (!parse_format(srcgeom->format, &srcinfo->format)) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM_FORMAT
					: BVERR_SRC2GEOM_FORMAT,
			      "invalid source #%d format (%d).",
			      srcinfo->index + 1,
			      srcgeom->format);
		goto exit;
	}

	/* Validate source geometry. */
	if (!valid_geom(srcdesc, srcgeom, srcinfo->format)) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
			      "source%d geom exceeds surface size.",
			      srcinfo->index + 1);
		goto exit;
	}

	/* Source must be 8 pixel aligned. */
	stridealign = srcinfo->format->bitspp - 1;
	if ((srcgeom->virtstride & stridealign) != 0) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM_STRIDE
					: BVERR_SRC2GEOM_STRIDE,
			      "source stride must be 8 pixel aligned.");
		goto exit;
	}

	/* Parse orientation. */
	srcinfo->angle = get_angle(srcgeom->orientation);
	if (srcinfo->angle == ROT_ANGLE_INVALID) {
		BVSETBLTERROR((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
			      "unsupported source%d orientation %d.",
			      srcinfo->index + 1,
			      srcgeom->orientation);
	}

	/* Determine source mirror. */
	srcinfo->mirror = (srcinfo->index == 0)
			? (bltparams->flags >> BVFLAG_FLIP_SRC1_SHIFT)
			   & BVFLAG_FLIP_MASK
			: (bltparams->flags >> BVFLAG_FLIP_SRC2_SHIFT)
			   & BVFLAG_FLIP_MASK;

	GCDBG(GCZONE_SURF, "source surface %d:\n", srcinfo->index + 1);
	GCDBG(GCZONE_SURF, "  rotation %d degrees.\n", srcinfo->angle * 90);

	if (srcdesc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *bvphysdesc;
		bvphysdesc = (struct bvphysdesc *) srcdesc->auxptr;
		GCDBG(GCZONE_DEST, "  page offset = 0x%08X\n",
			bvphysdesc->pageoffset);
	} else {
		GCDBG(GCZONE_DEST, "  virtual address = 0x%08X\n",
			(unsigned int) srcdesc->virtaddr);
	}

	GCDBG(GCZONE_SURF, "  stride = %ld\n",
		srcgeom->virtstride);
	GCDBG(GCZONE_SURF, "  geometry size = %dx%d\n",
		srcgeom->width, srcgeom->height);
	GCDBG(GCZONE_SURF, "  rect = (%d,%d)-(%d,%d), %dx%d\n",
		srcrect->left, srcrect->top,
		srcrect->left + srcrect->width,
		srcrect->top  + srcrect->height,
		srcrect->width, srcrect->height);
	GCDBG(GCZONE_SURF, "  mirror = %d\n", srcinfo->mirror);

exit:
	return bverror;
}


/*******************************************************************************
 * Primitive renderers.
 */

static enum bverror set_dst(struct bvbltparams *bltparams,
			    struct gcbatch *batch,
			    struct bvbuffmap *dstmap)
{
	enum bverror bverror = BVERR_NONE;
	struct gcmodst *gcmodst;

	GCENTER(GCZONE_DEST);

	/* Did destination surface change? */
	if ((batch->batchflags & BVBATCH_DST) != 0) {
		/* Allocate command buffer. */
		bverror = claim_buffer(bltparams, batch,
				       sizeof(struct gcmodst),
				       (void **) &gcmodst);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Add the address fixup. */
		add_fixup(bltparams, batch, &gcmodst->address,
			  batch->dstbyteshift);

		/* Set surface parameters. */
		gcmodst->address_ldst = gcmodst_address_ldst;
		gcmodst->address = GET_MAP_HANDLE(dstmap);
		gcmodst->stride = bltparams->dstgeom->virtstride;

		/* Set surface width and height. */
		gcmodst->rotation.raw = 0;
		gcmodst->rotation.reg.surf_width = batch->physwidth;
		gcmodst->rotationheight_ldst = gcmodst_rotationheight_ldst;
		gcmodst->rotationheight.raw = 0;
		gcmodst->rotationheight.reg.height = batch->physheight;

		/* Set clipping. */
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

static enum bverror do_fill(struct bvbltparams *bltparams,
			    struct gcbatch *batch,
			    struct srcinfo *srcinfo)
{
	enum bverror bverror;
	int dstbyteshift;
	struct gcmofill *gcmofill;
	unsigned char *fillcolorptr;
	struct bvbuffmap *dstmap = NULL;

	GCENTER(GCZONE_DO_FILL);

	/* Finish previous batch if any. */
	bverror = batch->batchend(bltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Compute the surface offset in bytes. */
	dstbyteshift = batch->dstalign * (int) batch->dstformat->bitspp / 8;

	/* Verify if the destination parameter have been modified. */
	if ((batch->dstbyteshift != dstbyteshift) ||
	    (batch->physwidth != batch->dstphyswidth) ||
	    (batch->physheight != batch->dstphysheight)) {
		/* Set new values. */
		batch->dstbyteshift = dstbyteshift;
		batch->physwidth = batch->dstphyswidth;
		batch->physheight = batch->dstphysheight;

		/* Mark as modified. */
		batch->batchflags |= BVBATCH_DST;
	}

	/* Map the destination. */
	bverror = do_map(bltparams->dstdesc, batch, &dstmap);
	if (bverror != BVERR_NONE) {
		bltparams->errdesc = gccontext.bverrorstr;
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

	/***********************************************************************
	** Allocate command buffer.
	*/

	bverror = claim_buffer(bltparams, batch,
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
	gcmofill->src.rotationangle.reg.dst = GCREG_ROT_ANGLE_ROT0;
	gcmofill->src.rotationangle.reg.dst_mirror = GCREG_MIRROR_NONE;

	/* Disable alpha blending. */
	gcmofill->src.alphacontrol_ldst = gcmofillsrc_alphacontrol_ldst;
	gcmofill->src.alphacontrol.raw = 0;
	gcmofill->src.alphacontrol.reg.enable = GCREG_ALPHA_CONTROL_ENABLE_OFF;

	/***********************************************************************
	** Set fill color.
	*/

	fillcolorptr
		= (unsigned char *) srcinfo->buf.desc->virtaddr
		+ srcinfo->rect->top * srcinfo->geom->virtstride
		+ srcinfo->rect->left * srcinfo->format->bitspp / 8;

	gcmofill->clearcolor_ldst = gcmofill_clearcolor_ldst;
	gcmofill->clearcolor.raw = getinternalcolor(fillcolorptr,
						    srcinfo->format);

	/***********************************************************************
	** Configure and start fill.
	*/

	/* Set destination configuration. */
	gcmofill->dstconfig_ldst = gcmofill_dstconfig_ldst;
	gcmofill->dstconfig.raw = 0;
	gcmofill->dstconfig.reg.swizzle = batch->dstformat->swizzle;
	gcmofill->dstconfig.reg.format = batch->dstformat->format;
	gcmofill->dstconfig.reg.command = GCREG_DEST_CONFIG_COMMAND_CLEAR;

	/* Set ROP3. */
	gcmofill->rop_ldst = gcmofill_rop_ldst;
	gcmofill->rop.raw = 0;
	gcmofill->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmofill->rop.reg.fg = (unsigned char) bltparams->op.rop;

	/* Set START_DE command. */
	gcmofill->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmofill->rect.left = batch->clippedleft + batch->dstoffsetX;
	gcmofill->rect.top = batch->clippedtop + batch->dstoffsetY;
	gcmofill->rect.right = batch->clippedright + batch->dstoffsetX;
	gcmofill->rect.bottom = batch->clippedbottom + batch->dstoffsetY;

exit:
	GCEXITARG(GCZONE_DO_FILL, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

#define ENABLE_HW_BLOCK 1
static enum bverror do_blit_end(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcmobltconfig *gcmobltconfig;
	struct gcmostart *gcmostart;

	GCENTER(GCZONE_DO_BLIT);

	GCDBG(GCZONE_DO_BLIT, "finalizing the blit, scrcount = %d\n",
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

	GCDBG(GCZONE_DO_BLIT, "blockenable = %d\n", batch->blockenable);
	if (batch->blockenable && ENABLE_HW_BLOCK) {
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
	GCDBG(GCZONE_DO_BLIT, "format entry = 0x%08X\n",
	      (unsigned int) batch->dstformat);
	GCDBG(GCZONE_DO_BLIT, "  swizzle code = %d\n",
	      batch->dstformat->swizzle);
	GCDBG(GCZONE_DO_BLIT, "  format code = %d\n",
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

	GCDBG(GCZONE_DO_BLIT, "dstrect = (%d,%d)-(%d,%d)\n",
	      gcmostart->rect.left, gcmostart->rect.top,
	      gcmostart->rect.right, gcmostart->rect.bottom);

	/* Reset the finalizer. */
	batch->batchend = do_end;

	gc_debug_blt(batch->gcblit.srccount,
		     abs(batch->right - batch->left),
		     abs(batch->bottom - batch->top));

exit:
	GCEXITARG(GCZONE_DO_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror do_blit(struct bvbltparams *bltparams,
			    struct gcbatch *batch,
			    struct srcinfo *srcinfo)
{
	enum bverror bverror = BVERR_NONE;

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

	GCENTER(GCZONE_DO_BLIT);

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
	srcalign = get_pixeloffset(srcdesc, srcformat, srcbyteshift);

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
	dstalign = get_pixeloffset(dstdesc, dstformat, dstbyteshift);

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
		srcalign = get_pixeloffset(srcdesc, srcformat, 0);

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
		bltparams->errdesc = gccontext.bverrorstr;
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
		bltparams->errdesc = gccontext.bverrorstr;
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
	GCEXITARG(GCZONE_DO_BLIT, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror do_filter(struct bvbltparams *bltparams,
			      struct gcbatch *batch,
			      struct srcinfo *srcinfo)
{
	enum bverror bverror;
	GCENTER(GCZONE_DO_FILTER);

	GCDBG(GCZONE_DO_FILTER, "source rectangle:\n");
	GCDBG(GCZONE_DO_FILTER, "  stride = %d, geom = %dx%d\n",
		srcinfo->geom->virtstride,
		srcinfo->geom->width, srcinfo->geom->height);
	GCDBG(GCZONE_DO_FILTER, "  rotation = %d\n",
		get_angle(srcinfo->geom->orientation));
	GCDBG(GCZONE_DO_FILTER, "  rect = (%d,%d)-(%d,%d), %dx%d\n",
		srcinfo->rect->left, srcinfo->rect->top,
		srcinfo->rect->left + srcinfo->rect->width,
		srcinfo->rect->top + srcinfo->rect->height,
		srcinfo->rect->width, srcinfo->rect->height);

	GCDBG(GCZONE_DO_FILTER, "destination rectangle:\n");
	GCDBG(GCZONE_DO_FILTER, "  stride = %d, geom size = %dx%d\n",
		bltparams->dstgeom->virtstride,
		bltparams->dstgeom->width, bltparams->dstgeom->height);
	GCDBG(GCZONE_DO_FILTER, "  rotaton = %d\n",
		get_angle(bltparams->dstgeom->orientation));
	GCDBG(GCZONE_DO_FILTER, "  rect = (%d,%d)-(%d,%d), %dx%d\n",
		bltparams->dstrect.left, bltparams->dstrect.top,
		bltparams->dstrect.left + bltparams->dstrect.width,
		bltparams->dstrect.top + bltparams->dstrect.height,
		bltparams->dstrect.width, bltparams->dstrect.height);

	BVSETBLTERROR((srcinfo->index == 0)
			? BVERR_SRC1_HORZSCALE
			: BVERR_SRC2_HORZSCALE,
		      "scaling not supported");

	GCEXITARG(GCZONE_DO_FILTER, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}


/*******************************************************************************
 * Library constructor and destructor.
 */

void bv_init(void)
{
	GCDBG_REGISTER(gcbv);
	GCLOCK_INIT(&gccontext.batchlock);
	GCLOCK_INIT(&gccontext.bufferlock);
	GCLOCK_INIT(&gccontext.fixuplock);
	GCLOCK_INIT(&gccontext.maplock);
	GCLOCK_INIT(&gccontext.callbacklock);
	INIT_LIST_HEAD(&gccontext.callbacklist);
	INIT_LIST_HEAD(&gccontext.callbackvac);
	INIT_LIST_HEAD(&gccontext.vac_unmap);
}

void bv_exit(void)
{
	struct list_head *head;
	struct gcschedunmap *bufunmap;
	struct bvbuffmap *bufmap;
	struct gcbuffer *buffer;
	struct gcfixup *fixup;
	struct gcvacbatch *batch;

	while (!list_empty(&gccontext.vac_unmap)) {
		head = gccontext.vac_unmap.next;
		bufunmap = list_entry(head, struct gcschedunmap, link);
		list_del(head);
		gcfree(bufunmap);
	}

	while (gccontext.vac_buffmap != NULL) {
		bufmap = gccontext.vac_buffmap;
		gccontext.vac_buffmap = bufmap->nextmap;
		gcfree(bufmap);
	}

	while (gccontext.vac_buffers != NULL) {
		buffer = gccontext.vac_buffers;
		gccontext.vac_buffers = buffer->next;
		gcfree(buffer);
	}

	while (gccontext.vac_fixups != NULL) {
		fixup = gccontext.vac_fixups;
		gccontext.vac_fixups = fixup->next;
		gcfree(fixup);
	}

	while (gccontext.vac_batches != NULL) {
		batch = gccontext.vac_batches;
		gccontext.vac_batches = batch->next;
		gcfree(batch);
	}
}


/*******************************************************************************
 * Library API.
 */

enum bverror bv_map(struct bvbuffdesc *buffdesc)
{
	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;

	GCENTERARG(GCZONE_MAPPING, "buffdesc = 0x%08X\n",
		   (unsigned int) buffdesc);

	if (buffdesc == NULL) {
		BVSETERROR(BVERR_BUFFERDESC, "bvbuffdesc is NULL");
		goto exit;
	}

	if (buffdesc->structsize < STRUCTSIZE(buffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	bverror = do_map(buffdesc, NULL, &bvbuffmap);

exit:
	GCEXITARG(GCZONE_MAPPING, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror bv_unmap(struct bvbuffdesc *buffdesc)
{
	enum bverror bverror = BVERR_NONE;
	enum bverror otherbverror = BVERR_NONE;
	struct bvbuffmap *prev = NULL;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;
	struct gcmap gcmap;

	GCENTERARG(GCZONE_MAPPING, "buffdesc = 0x%08X\n",
		   (unsigned int) buffdesc);

	/* Lock access to the mapping list. */
	GCLOCK(&gccontext.maplock);

	if (buffdesc == NULL) {
		BVSETERROR(BVERR_BUFFERDESC, "bvbuffdesc is NULL");
		goto exit;
	}

	if (buffdesc->structsize < STRUCTSIZE(buffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	/* Is the buffer mapped? */
	bvbuffmap = buffdesc->map;
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
			bverror = buffdesc->map->bv_unmap(buffdesc);
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
			buffdesc->map = bvbuffmap->nextmap;
		else
			prev->nextmap = bvbuffmap->nextmap;

		/* Call other implementation. */
		otherbverror = buffdesc->map->bv_unmap(buffdesc);

		/* Add our mapping back. */
		bvbuffmap->nextmap = buffdesc->map;
		buffdesc->map = bvbuffmap;
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
	memset(&gcmap, 0, sizeof(gcmap));
	gcmap.handle = bvbuffmapinfo->handle;
	gc_unmap_wrapper(&gcmap);
	if (gcmap.gcerror != GCERR_NONE) {
		BVSETERROR(BVERR_OOM, "unable to free gccore memory");
		goto exit;
	}

	/* Remove from the buffer descriptor list. */
	if (prev == NULL)
		buffdesc->map = bvbuffmap->nextmap;
	else
		prev->nextmap = bvbuffmap->nextmap;

	/* Invalidate the record. */
	bvbuffmap->structsize = 0;

	/* Add to the vacant list. */
	bvbuffmap->nextmap = gccontext.vac_buffmap;
	gccontext.vac_buffmap = bvbuffmap;

exit:
	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext.maplock);

	GCEXITARG(GCZONE_MAPPING, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror bv_blt(struct bvbltparams *bltparams)
{
	enum bverror bverror = BVERR_NONE;
	struct gcalpha *gca = NULL;
	struct gcalpha _gca;
	unsigned int op, type, blend, format;
	unsigned int batchexec = 0;
	bool nop = false;
	struct gcbatch *batch;
	struct bvrect *dstrect;
	int src1used, src2used, maskused;
	struct srcinfo srcinfo[2];
	unsigned short rop;
	struct gccommit gccommit;
	int i, srccount, res;

	GCENTERARG(GCZONE_BLIT, "bltparams = 0x%08X\n",
		   (unsigned int) bltparams);

	/* Verify blt parameters structure. */
	if (bltparams == NULL) {
		BVSETERROR(BVERR_BLTPARAMS_VERS, "bvbltparams is NULL");
		goto exit;
	}

	if (bltparams->structsize < STRUCTSIZE(bltparams, callbackdata)) {
		BVSETERROR(BVERR_BLTPARAMS_VERS, "argument has invalid size");
		goto exit;
	}

	/* Reset the error message. */
	bltparams->errdesc = NULL;

	/* Verify the destination parameters structure. */
	GCDBG(GCZONE_SURF, "verifying destination parameters.\n");

	res = verify_surface(0, (union bvinbuff *) &bltparams->dstdesc,
				bltparams->dstgeom);
	if (res != -1) {
		BVSETBLTSURFERROR(res, g_destsurferr);
		goto exit;
	}

	/* Extract the operation flags. */
	op = (bltparams->flags & BVFLAG_OP_MASK) >> BVFLAG_OP_SHIFT;
	type = (bltparams->flags & BVFLAG_BATCH_MASK) >> BVFLAG_BATCH_SHIFT;

	switch (type) {
	case (BVFLAG_BATCH_NONE >> BVFLAG_BATCH_SHIFT):
		bverror = allocate_batch(bltparams, &batch);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		batchexec = 1;
		batch->batchflags = 0x7FFFFFFF;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_NONE(0x%08X)\n",
		      (unsigned int) batch);
		break;

	case (BVFLAG_BATCH_BEGIN >> BVFLAG_BATCH_SHIFT):
		bverror = allocate_batch(bltparams, &batch);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		bltparams->batch = (struct bvbatch *) batch;

		batchexec = 0;
		bltparams->batchflags =
		batch->batchflags = 0x7FFFFFFF;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_BEGIN(0x%08X)\n",
		      (unsigned int) batch);
		break;

	case (BVFLAG_BATCH_CONTINUE >> BVFLAG_BATCH_SHIFT):
		batch = (struct gcbatch *) bltparams->batch;
		if (batch == NULL) {
			BVSETBLTERROR(BVERR_BATCH, "batch is not initialized");
			goto exit;
		}

		if (batch->structsize < STRUCTSIZE(batch, unmap)) {
			BVSETBLTERROR(BVERR_BATCH, "invalid batch");
			goto exit;
		}

		batchexec = 0;
		batch->batchflags = bltparams->batchflags;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_CONTINUE(0x%08X)\n",
		      (unsigned int) batch);
		break;

	case (BVFLAG_BATCH_END >> BVFLAG_BATCH_SHIFT):
		batch = (struct gcbatch *) bltparams->batch;
		if (batch == NULL) {
			BVSETBLTERROR(BVERR_BATCH, "batch is not initialized");
			goto exit;
		}

		if (batch->structsize < STRUCTSIZE(batch, unmap)) {
			BVSETBLTERROR(BVERR_BATCH, "invalid batch");
			goto exit;
		}

		batchexec = 1;
		nop = (bltparams->batchflags & BVBATCH_ENDNOP) != 0;
		batch->batchflags = bltparams->batchflags;

		GCDBG(GCZONE_BATCH, "BVFLAG_BATCH_END(0x%08X)\n",
		      (unsigned int) batch);
		break;

	default:
		BVSETBLTERROR(BVERR_BATCH, "unrecognized batch type");
		goto exit;
	}

	GCDBG(GCZONE_BATCH, "batchflags=0x%08X\n",
		(unsigned int) batch->batchflags);

	GCDUMPSCHEDULE();

	if (!nop) {
		/* Get a shortcut to the destination rectangle. */
		dstrect = &bltparams->dstrect;

		/* Verify the batch change flags. */
		VERIFYBATCH(batch->batchflags >> 12,
			    &batch->prevdstrect, dstrect);

		switch (op) {
		case (BVFLAG_ROP >> BVFLAG_OP_SHIFT):
			GCDBG(GCZONE_BLIT, "BVFLAG_ROP\n");

			rop = bltparams->op.rop;
			src1used = ((rop & 0xCCCC) >> 2)
				 ^  (rop & 0x3333);
			src2used = ((rop & 0xF0F0) >> 4)
				 ^  (rop & 0x0F0F);
			maskused = ((rop & 0xFF00) >> 8)
				 ^  (rop & 0x00FF);
			break;

		case (BVFLAG_BLEND >> BVFLAG_OP_SHIFT):
			GCDBG(GCZONE_BLIT, "BVFLAG_BLEND\n");

			blend = bltparams->op.blend;
			format = (blend & BVBLENDDEF_FORMAT_MASK)
			       >> BVBLENDDEF_FORMAT_SHIFT;

			bverror = parse_blend(bltparams, blend, &_gca);
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

		/* Parse destination parameters. */
		bverror = parse_destination(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		srccount = 0;

		/* Verify the src1 parameters structure. */
		if (src1used) {
			GCDBG(GCZONE_BLIT, "src1used\n");
			GCDBG(GCZONE_SURF, "verifying source1 parameters.\n");

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_SRC1,
				&bltparams->src1, bltparams->src1geom);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src1surferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			VERIFYBATCH(batch->batchflags >> 14,
				    &batch->prevsrc1rect,
				    &bltparams->src1rect);

			/* Same as the destination? */
			if (same_phys_area(bltparams->src1.desc,
					   &bltparams->src1rect,
					   bltparams->dstdesc,
					   dstrect)) {
				GCDBG(GCZONE_BLIT, "src1 is the same as dst\n");
			} else {
				srcinfo[srccount].index = 0;
				srcinfo[srccount].buf = bltparams->src1;
				srcinfo[srccount].geom = bltparams->src1geom;
				srcinfo[srccount].rect = &bltparams->src1rect;

				bverror = parse_source(bltparams, batch,
						       &srcinfo[srccount]);
				if (bverror != BVERR_NONE)
					goto exit;

				srccount += 1;
			}
		}

		/* Verify the src2 parameters structure. */
		if (src2used) {
			GCDBG(GCZONE_BLIT, "src2used\n");
			GCDBG(GCZONE_SURF, "verifying source2 parameters.\n");

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_SRC2,
				&bltparams->src2, bltparams->src2geom);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src2surferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			VERIFYBATCH(batch->batchflags >> 16,
				    &batch->prevsrc2rect,
				    &bltparams->src2rect);

			/* Same as the destination? */
			if (same_phys_area(bltparams->src2.desc,
					   &bltparams->src2rect,
					   bltparams->dstdesc,
					   dstrect)) {
				GCDBG(GCZONE_BLIT, "src2 is the same as dst\n");
			} else {
				srcinfo[srccount].index = 1;
				srcinfo[srccount].buf = bltparams->src2;
				srcinfo[srccount].geom = bltparams->src2geom;
				srcinfo[srccount].rect = &bltparams->src2rect;

				bverror = parse_source(bltparams, batch,
						       &srcinfo[srccount]);
				if (bverror != BVERR_NONE)
					goto exit;

				srccount += 1;
			}
		}

		/* Verify the mask parameters structure. */
		if (maskused) {
			GCDBG(GCZONE_BLIT, "maskused\n");
			GCDBG(GCZONE_SURF, "verifying mask parameters.\n");

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_MASK,
				&bltparams->mask, bltparams->maskgeom);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_masksurferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			VERIFYBATCH(batch->batchflags >> 18,
				    &batch->prevmaskrect,
				    &bltparams->maskrect);

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
				if (gca == NULL) {
					srcinfo[i].rop = bltparams->op.rop;
					srcinfo[i].gca = NULL;
				} else if ((i + 1) != srccount) {
					srcinfo[i].rop = 0xCC;
					srcinfo[i].gca = NULL;
				} else {
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

				if (EQ_SIZE(srcinfo[i].rect, dstrect))
					bverror = do_blit(bltparams, batch,
							  &srcinfo[i]);
				else if ((srcinfo[i].rect->width == 1) &&
					 (srcinfo[i].rect->height == 1))
					bverror = do_fill(bltparams, batch,
							  &srcinfo[i]);
				else
					bverror = do_filter(bltparams, batch,
							    &srcinfo[i]);

				if (bverror != BVERR_NONE)
					goto exit;
			}
		}
	}

	if (batchexec) {
		struct gcmoflush *flush;

		GCDBG(GCZONE_BLIT, "preparing to submit the batch.\n");

		/* Finalize the current operation. */
		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Add PE flush. */
		GCDBG(GCZONE_BLIT, "appending the flush.\n");
		bverror = claim_buffer(bltparams, batch,
				       sizeof(struct gcmoflush),
				       (void **) &flush);
		if (bverror != BVERR_NONE)
			goto exit;

		flush->flush_ldst = gcmoflush_flush_ldst;
		flush->flush.reg = gcregflush_pe2D;

		/* Process asynchronous operation. */
		if ((bltparams->flags & BVFLAG_ASYNC) == 0) {
			GCDBG(GCZONE_BLIT, "synchronous batch.\n");
			gccommit.callback = NULL;
			gccommit.callbackparam = NULL;
			gccommit.asynchronous = false;
		} else {
			struct gccallbackinfo *gccallbackinfo;

			GCDBG(GCZONE_BLIT, "asynchronous batch (0x%08X):\n",
			      bltparams->flags);

			if (bltparams->callbackfn == NULL) {
				GCDBG(GCZONE_BLIT, "no callback given.\n");
				gccommit.callback = NULL;
				gccommit.callbackparam = NULL;
			} else {
				bverror = get_callbackinfo(&gccallbackinfo);
				if (bverror != BVERR_NONE) {
					BVSETBLTERROR(BVERR_OOM,
						      "callback allocation "
						      "failed");
					goto exit;
				}

				gccallbackinfo->callbackfn
					= bltparams->callbackfn;
				gccallbackinfo->callbackdata
					= bltparams->callbackdata;

				gccommit.callback = gccallback;
				gccommit.callbackparam = gccallbackinfo;

				GCDBG(GCZONE_BLIT,
				      "gcbv_callback = 0x%08X\n",
				      (unsigned int) gccommit.callback);
				GCDBG(GCZONE_BLIT,
				      "gcbv_param    = 0x%08X\n",
				      (unsigned int) gccommit.callbackparam);
				GCDBG(GCZONE_BLIT,
				      "bltsville_callback = 0x%08X\n",
				      (unsigned int)
				      gccallbackinfo->callbackfn);
				GCDBG(GCZONE_BLIT,
				      "bltsville_param    = 0x%08X\n",
				      (unsigned int)
				      gccallbackinfo->callbackdata);
			}

			gccommit.asynchronous = true;
		}

		/* Process scheduled unmappings. */
		unmap_implicit(batch);

		INIT_LIST_HEAD(&gccommit.unmap);
		list_splice_init(&batch->unmap, &gccommit.unmap);

		/* Pass the batch for execution. */
		GCDUMPBATCH(batch);

		gccommit.gcerror = GCERR_NONE;
		gccommit.entrypipe = GCPIPE_2D;
		gccommit.exitpipe = GCPIPE_2D;
		gccommit.buffer = batch->bufhead;

		GCDBG(GCZONE_BLIT, "submitting the batch.\n");
		gc_commit_wrapper(&gccommit);

		/* Move the list of mappings back to batch. */
		list_splice_init(&gccommit.unmap, &batch->unmap);

		/* Error? */
		if (gccommit.gcerror != GCERR_NONE) {
			switch (gccommit.gcerror) {
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
	if ((batch != NULL) && batchexec) {
		GCDUMPSCHEDULE();
		free_batch(batch);
		bltparams->batch = NULL;
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

		rgn[0].span = copparams->rect->width * bytespp;
		rgn[0].lines = copparams->rect->height;
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
		rgn[0].span = copparams->rect->width;
		rgn[0].lines = copparams->rect->height;
		rgn[0].stride = copparams->geom->virtstride;
		rgn[0].start = (void *)
			((unsigned long) copparams->desc->virtaddr +
			 copparams->rect->top * rgn[0].stride +
			 copparams->rect->left);

		rgn[1].span = copparams->rect->width;
		rgn[1].lines = copparams->rect->height / 2;
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
