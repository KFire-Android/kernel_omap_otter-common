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
#define GCZONE_BUFFER		(1 << 1)
#define GCZONE_FIXUP		(1 << 2)
#define GCZONE_FORMAT		(1 << 3)
#define GCZONE_COLOR		(1 << 4)
#define GCZONE_BLEND		(1 << 5)
#define GCZONE_DEST		(1 << 6)
#define GCZONE_SURF		(1 << 7)
#define GCZONE_DO_FILL		(1 << 8)
#define GCZONE_DO_BLIT		(1 << 9)
#define GCZONE_DO_FILTER	(1 << 10)
#define GCZONE_CLIP		(1 << 11)
#define GCZONE_BATCH		(1 << 12)
#define GCZONE_BLIT		(1 << 13)

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
		"clip",
		"batch",
		"blit")


/*******************************************************************************
** Miscellaneous defines and macros.
*/

#define EQ_SIZE(rect1, rect2) \
( \
	(rect1.width == rect2.width) && (rect1.height == rect2.height) \
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

#define GC_BASE_ALIGN	64

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

/* Used by blitters to define an array of valid sources. */
struct srcinfo {
	int index;
	union bvinbuff buf;
	struct bvsurfgeom *geom;
	struct bvrect *rect;
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

	/* Clipping deltas; used to correct the source coordinates for
	 * single source blits. */
	int deltaleft;
	int deltatop;
	int deltaright;
	int deltabottom;

	/* Destination format. */
	struct bvformatxlate *dstformat;

	/* Destination rectangle coordinates. */
	unsigned short dstleft;
	unsigned short dsttop;
	unsigned short dstright;
	unsigned short dstbottom;

	/* Destination base address alignment offset in pixels. */
	unsigned int dstoffset;

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
	if ((GCDBGFILTER.zone & GCZONE_BATCH) != 0) {
		struct gcbuffer *buffer;
		unsigned int i, size;
		struct gcfixup *fixup;

		GCDBG(GCZONE_BATCH, "BATCH DUMP (0x%08X)\n",
			(unsigned int) batch);

		buffer = batch->bufhead;
		while (buffer != NULL) {
			fixup = buffer->fixuphead;
			while (fixup != NULL) {
				GCDBG(GCZONE_BATCH,
					"  Fixup table @ 0x%08X, count = %d:\n",
					(unsigned int) fixup, fixup->count);

				for (i = 0; i < fixup->count; i += 1) {
					GCDBG(GCZONE_BATCH, "  [%02d]"
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

#define SETERROR(message) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		"%s(%d): " message, __func__, __LINE__); \
	GCDUMPSTRING("%s\n", gccontext.bverrorstr); \
} while (0)

#define SETERRORARG(message, arg) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
			 "%s(%d): " message, __func__, __LINE__, arg); \
	GCDUMPSTRING("%s\n", gccontext.bverrorstr);	 \
} while (0)

#define BVSETERROR(error, message) \
do { \
	SETERROR(message); \
	bverror = error; \
} while (0)

#define BVSETERRORARG(error, message, arg) \
do { \
	SETERRORARG(message, arg); \
	bverror = error; \
} while (0)

#define BVSETBLTERROR(error, message) \
do { \
	BVSETERROR(error, message); \
	bltparams->errdesc = gccontext.bverrorstr; \
} while (0)

#define BVSETBLTERRORARG(error, message, arg) \
do { \
	BVSETERRORARG(error, message, arg); \
	bltparams->errdesc = gccontext.bverrorstr; \
} while (0)

#define BVSETSURFERROR(errorid, errordesc) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		g_surferr[errorid].message, __func__, __LINE__, errordesc.id); \
	GCDUMPSTRING("%s\n", gccontext.bverrorstr); \
	bverror = errordesc.base + g_surferr[errorid].offset; \
} while (0)

#define BVSETBLTSURFERROR(errorid, errordesc) \
do { \
	BVSETSURFERROR(errorid, errordesc); \
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
	{    0, "%s(%d): %s desc structure is not set" },

	/* GCBVERR_DESC_VERS */
	{  100, "%s(%d): %s desc structure has invalid size" },

	/* GCBVERR_DESC_VIRTADDR */
	{  200, "%s(%d): %s desc virtual pointer is not set" },

	/* GCBVERR_TILE: FIXME/TODO define error code */
	{    0, "%s(%d): %s tileparams structure is not set" },

	/* GCBVERR_TILE_VERS */
	{ 3000, "%s(%d): %s tileparams structure has invalid size" },

	/* GCBVERR_TILE_VIRTADDR: FIXME/TODO define error code */
	{  200, "%s(%d): %s tileparams virtual pointer is not set" },

	/* GCBVERR_GEOM */
	{ 1000, "%s(%d): %s geom structure is not set" },

	/* GCBVERR_GEOM_VERS */
	{ 1100, "%s(%d): %s geom structure has invalid size" },

	/* GCBVERR_GEOM_FORMAT */
	{ 1200, "%s(%d): %s invalid format specified" },
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
	unsigned int offset;
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

			gcmap.buf.offset = bvphysdesc->pageoffset
					 & ~(GC_BASE_ALIGN - 1);
			offset = bvphysdesc->pageoffset
				& (GC_BASE_ALIGN - 1);
			gcmap.pagesize = bvphysdesc->pagesize;
			gcmap.pagearray = bvphysdesc->pagearray;

			GCDBG(GCZONE_MAPPING, "new mapping (%s):\n",
				(batch == NULL) ? "explicit" : "implicit");
			GCDBG(GCZONE_MAPPING, "pagesize = %lu\n",
				bvphysdesc->pagesize);
			GCDBG(GCZONE_MAPPING, "pagearray = 0x%08X\n",
				(unsigned int) bvphysdesc->pagearray);
			GCDBG(GCZONE_MAPPING, "specified pageoffset = %lu\n",
				bvphysdesc->pageoffset);
			GCDBG(GCZONE_MAPPING, "aligned pageoffset = %d\n",
				gcmap.buf.offset);
		} else {
			gcmap.buf.logical
				= (void *) ((unsigned int) bvbuffdesc->virtaddr
				& ~(GC_BASE_ALIGN - 1));
			offset = (unsigned int) bvbuffdesc->virtaddr
				& (GC_BASE_ALIGN - 1);
			gcmap.pagesize = 0;
			gcmap.pagearray = NULL;

			GCDBG(GCZONE_MAPPING, "new mapping (%s):\n",
				(batch == NULL) ? "explicit" : "implicit");
			GCDBG(GCZONE_MAPPING, "specified virtaddr = 0x%08X\n",
				(unsigned int) bvbuffdesc->virtaddr);
			GCDBG(GCZONE_MAPPING, "aligned virtaddr = 0x%08X\n",
				(unsigned int) gcmap.buf.logical);
		}

		gcmap.size = offset + bvbuffdesc->length;

		GCDBG(GCZONE_MAPPING, "surface offset = %d\n", offset);
		GCDBG(GCZONE_MAPPING, "mapping size = %d\n", gcmap.size);

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

static enum bverror allocate_batch(struct gcbatch **batch);
static void free_batch(struct gcbatch *batch);
static enum bverror append_buffer(struct gcbatch *batch);

static enum bverror do_end(struct bvbltparams *bltparams,
			   struct gcbatch *batch)
{
	return BVERR_NONE;
}

static enum bverror allocate_batch(struct gcbatch **batch)
{
	enum bverror bverror;
	struct gcbatch *temp;

	GCENTER(GCZONE_BATCH);

	/* Lock access to batch management. */
	GCLOCK(&gccontext.batchlock);

	if (gccontext.vac_batches == NULL) {
		temp = gcalloc(struct gcbatch, sizeof(struct gcbatch));
		if (temp == NULL) {
			BVSETERROR(BVERR_OOM,
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

	bverror = append_buffer(temp);
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

static enum bverror append_buffer(struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcbuffer *temp;

	GCENTERARG(GCZONE_BUFFER, "batch = 0x%08X\n", (unsigned int) batch);

	/* Lock access to buffer management. */
	GCLOCK(&gccontext.bufferlock);

	if (gccontext.vac_buffers == NULL) {
		temp = gcalloc(struct gcbuffer, GC_BUFFER_SIZE);
		if (temp == NULL) {
			BVSETERROR(BVERR_OOM,
					"command buffer allocation failed");
			goto exit;
		}

		GCDBG(GCZONE_BUFFER, "allocated new buffer = 0x%08X\n",
			(unsigned int) temp);
	} else {
		temp = gccontext.vac_buffers;
		gccontext.vac_buffers = temp->next;

		GCDBG(GCZONE_BUFFER, "reusing buffer = 0x%08X\n",
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

	GCDBG(GCZONE_BUFFER, "new buffer appended = 0x%08X\n",
		(unsigned int) temp);

	bverror = BVERR_NONE;

exit:
	/* Unlock access to buffer management. */
	GCUNLOCK(&gccontext.bufferlock);

	GCEXITARG(GCZONE_BUFFER, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror add_fixup(struct gcbatch *batch, unsigned int *fixup,
			      unsigned int surfoffset)
{
	enum bverror bverror;
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
				BVSETERROR(BVERR_OOM,
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

	bverror = BVERR_NONE;

exit:
	/* Unlock access to fixup management. */
	GCUNLOCK(&gccontext.fixuplock);

	GCEXITARG(GCZONE_FIXUP, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror claim_buffer(struct gcbatch *batch,
				 unsigned int size,
				 void **buffer)
{
	enum bverror bverror;
	struct gcbuffer *curbuf;

	GCENTERARG(GCZONE_BUFFER, "batch = 0x%08X, size = %d\n",
		(unsigned int) batch, size);

	/* Get the current command buffer. */
	curbuf = batch->buftail;

	GCDBG(GCZONE_BUFFER, "buffer = 0x%08X, available = %d\n",
		(unsigned int) curbuf, curbuf->available);

	if (curbuf->available < size) {
		bverror = append_buffer(batch);
		if (bverror != BVERR_NONE)
			goto exit;

		curbuf = batch->buftail;
	}

	if (curbuf->available < size) {
		GCDBG(GCZONE_BUFFER, "requested size is too large.\n");
		BVSETERROR(BVERR_OOM,
				"command buffer allocation failed");
		goto exit;
	}

	*buffer = curbuf->tail;
	curbuf->tail = (unsigned int *) ((unsigned char *) curbuf->tail + size);
	curbuf->available -= size;
	batch->size += size;
	bverror = BVERR_NONE;

exit:
	GCEXITARG(GCZONE_BUFFER, "bv%s = %d\n",
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
	unsigned bitspp;
	unsigned format;
	unsigned swizzle;
	struct bvcsrgb rgba;
};

#define BVFORMATRGBA(BPP, Format, Swizzle, R, G, B, A) \
{ \
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
	{ 0, 0, 0, { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 } } }

static struct bvformatxlate g_format_nv12 = {
	.bitspp = 8,
	.format = GCREG_DE_FORMAT_NV12,
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

struct gcalpha {
	unsigned int src_global_color;
	unsigned int dst_global_color;

	unsigned char src_global_alpha_mode;
	unsigned char dst_global_alpha_mode;

	struct gcblendconfig *k1;
	struct gcblendconfig *k2;

	unsigned int src1used;
	unsigned int src2used;
};

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
	int ret;

	GCENTER(GCZONE_SURF);

	if (tile) {
		if (surf->tileparams == NULL) {
			ret = GCBVERR_TILE;
			goto exit;
		}

		if (surf->tileparams->structsize <
		    STRUCTSIZE(surf->tileparams, srcheight)) {
			ret = GCBVERR_TILE_VERS;
			goto exit;
		}

		/* FIXME/TODO */
		ret = GCBVERR_TILE;
		goto exit;
	} else {
		if (surf->desc == NULL) {
			ret = GCBVERR_DESC;
			goto exit;
		}

		if (surf->desc->structsize < STRUCTSIZE(surf->desc, map)) {
			ret = GCBVERR_DESC_VERS;
			goto exit;
		}
	}

	if (geom == NULL) {
		ret = GCBVERR_GEOM;
		goto exit;
	}

	if (geom->structsize < STRUCTSIZE(geom, palette)) {
		ret = GCBVERR_GEOM_VERS;
		goto exit;
	}

	/* Validation successful. */
	ret = -1;

exit:
	GCEXITARG(GCZONE_SURF, "ret = %d\n", ret);
	return ret;
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


/*******************************************************************************
 * Primitive renderers.
 */

static enum bverror set_dst(struct bvbltparams *bltparams,
			    struct gcbatch *batch,
			    struct bvbuffmap *dstmap)
{
	enum bverror bverror = BVERR_NONE;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct gcmodst *gcmodst;
	unsigned int dstoffset;

	GCENTER(GCZONE_DEST);

	/* Did destination surface change? */
	if ((batch->batchflags & BVBATCH_DST) == 0)
		goto exit;

	GCDBG(GCZONE_DEST, "destination surface changed.\n");

	/* Parse the destination format. */
	GCDBG(GCZONE_FORMAT, "parsing the destination format.\n");
	if (!parse_format(dstgeom->format, &batch->dstformat)) {
		BVSETBLTERRORARG(BVERR_DSTGEOM_FORMAT,
				"invalid destination format (%d)",
				dstgeom->format);
		goto exit;
	}

	/* Validate geometry. */
	if (!valid_geom(bltparams->dstdesc, bltparams->dstgeom,
				batch->dstformat)) {
		BVSETBLTERROR(BVERR_DSTGEOM,
				"destination geom exceeds surface size");
		goto exit;
	}

	/* Compute the destination offset in pixels needed to compensate
	   for the surface base address misalignment if any. */
	dstoffset = (((unsigned int) bltparams->dstdesc->virtaddr
			& (GC_BASE_ALIGN - 1)) * 8)
			/ batch->dstformat->bitspp;

	/* Have to reset clipping with the new offset. */
	if (dstoffset != batch->dstoffset) {
		batch->dstoffset = dstoffset;
		batch->batchflags |= BVBATCH_CLIPRECT_ORIGIN;
	}

	/* Allocate command buffer. */
	bverror = claim_buffer(batch, sizeof(struct gcmodst),
				(void **) &gcmodst);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM,
				"failed to allocate command buffer");
		goto exit;
	}

	/* Add the address fixup. */
	add_fixup(batch, &gcmodst->address, 0);

	/* Set surface parameters. */
	gcmodst->address_ldst = gcmodst_address_ldst;
	gcmodst->address = GET_MAP_HANDLE(dstmap);
	gcmodst->stride = dstgeom->virtstride;

	/* Set surface width and height. */
	gcmodst->rotation.raw = 0;
	gcmodst->rotation.reg.surf_width = dstgeom->width + dstoffset;
	gcmodst->rotationheight_ldst = gcmodst_rotationheight_ldst;
	gcmodst->rotationheight.raw = 0;
	gcmodst->rotationheight.reg.height = dstgeom->height;

	GCDBG(GCZONE_DEST, "destination surface:\n");
	GCDBG(GCZONE_DEST, "  dstvirtaddr = 0x%08X\n",
		(unsigned int) bltparams->dstdesc->virtaddr);
	GCDBG(GCZONE_DEST, "  dstaddr = 0x%08X\n",
		(unsigned int) GET_MAP_HANDLE(dstmap));
	GCDBG(GCZONE_DEST, "  dstoffset = %d\n", dstoffset);
	GCDBG(GCZONE_DEST, "  dstsurf = %dx%d, stride = %ld\n",
		dstgeom->width, dstgeom->height, dstgeom->virtstride);

exit:
	GCEXITARG(GCZONE_DEST, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror set_clip(struct bvbltparams *bltparams,
			     struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;
	struct bvrect *dstrect = &bltparams->dstrect;
	struct bvrect *cliprect = &bltparams->cliprect;
	int destleft, desttop, destright, destbottom;
	int clipleft, cliptop, clipright, clipbottom;
	int clippedleft, clippedtop, clippedright, clippedbottom;
	int clipleftadj, cliprightadj;
	struct gcmoclip *gcmoclip;

	GCENTER(GCZONE_CLIP);

	/* Did clipping/destination rects change? */
	if ((batch->batchflags & (BVBATCH_CLIPRECT_ORIGIN |
					BVBATCH_CLIPRECT_SIZE |
					BVBATCH_DSTRECT_ORIGIN |
					BVBATCH_DSTRECT_SIZE)) == 0)
		goto exit;

	GCDBG(GCZONE_CLIP, "destination clipping changed.\n");

	/* Determine destination rectangle. */
	destleft = dstrect->left;
	desttop = dstrect->top;
	destright = dstrect->left + dstrect->width;
	destbottom = dstrect->top + dstrect->height;

	/* Determine clipping. */
	if ((bltparams->flags & BVFLAG_CLIP) == BVFLAG_CLIP) {
		clipleft = cliprect->left;
		cliptop = cliprect->top;
		clipright = cliprect->left + cliprect->width;
		clipbottom = cliprect->top + cliprect->height;

		if ((clipleft < GC_CLIP_RESET_LEFT) ||
			(cliptop < GC_CLIP_RESET_TOP) ||
			(clipright > GC_CLIP_RESET_RIGHT) ||
			(clipbottom > GC_CLIP_RESET_BOTTOM) ||
			(clipright < clipleft) || (clipbottom < cliptop)) {
			BVSETBLTERROR(BVERR_CLIP_RECT,
					"invalid clipping rectangle");
			goto exit;
		}
	} else {
		clipleft = GC_CLIP_RESET_LEFT;
		cliptop = GC_CLIP_RESET_TOP;
		clipright = GC_CLIP_RESET_RIGHT;
		clipbottom = GC_CLIP_RESET_BOTTOM;
	}

	/* Compute clipping deltas and the adjusted destination rect. */
	if (clipleft <= destleft) {
		batch->deltaleft = 0;
		clippedleft = destleft;
	} else {
		batch->deltaleft = clipleft - destleft;
		clippedleft = destleft + batch->deltaleft;
	}

	if (cliptop <= desttop) {
		batch->deltatop = 0;
		clippedtop = desttop;
	} else {
		batch->deltatop = cliptop - desttop;
		clippedtop = desttop + batch->deltatop;
	}

	if (clipright >= destright) {
		batch->deltaright = 0;
		clippedright = destright;
	} else {
		batch->deltaright = clipright - destright;
		clippedright = destright + batch->deltaright;
	}

	if (clipbottom >= destbottom) {
		batch->deltabottom = 0;
		clippedbottom = destbottom;
	} else {
		batch->deltabottom = clipbottom - destbottom;
		clippedbottom = destbottom + batch->deltabottom;
	}

	/* Validate the rectangle. */
	if ((clippedright > (int) bltparams->dstgeom->width) ||
		(clippedbottom > (int) bltparams->dstgeom->height)) {
		BVSETBLTERROR(BVERR_DSTRECT,
				"destination rect exceeds surface size");
		goto exit;
	}

	/* Set the destination rectangle. */
	batch->dstleft = clippedleft;
	batch->dsttop = clippedtop;
	batch->dstright = clippedright;
	batch->dstbottom = clippedbottom;

	/* Allocate command buffer. */
	bverror = claim_buffer(batch, sizeof(struct gcmoclip),
				(void **) &gcmoclip);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM,
				"failed to allocate command buffer");
		goto exit;
	}

	/* Compute adjusted clipping. */
	clipleftadj = min(clipleft + (int) batch->dstoffset,
				(int) GC_CLIP_RESET_RIGHT);
	cliprightadj = min(clipright + (int) batch->dstoffset,
				(int) GC_CLIP_RESET_RIGHT);

	/* Set clipping. */
	gcmoclip->lt_ldst = gcmoclip_lt_ldst;
	gcmoclip->lt.raw = 0;
	gcmoclip->lt.reg.left = clipleftadj;
	gcmoclip->lt.reg.top = cliptop;
	gcmoclip->rb.raw = 0;
	gcmoclip->rb.reg.right = cliprightadj;
	gcmoclip->rb.reg.bottom = clipbottom;

	GCDBG(GCZONE_CLIP, "destination rectangle:\n");
	GCDBG(GCZONE_CLIP, "  dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		destleft, desttop, destright, destbottom,
		destright - destleft, destbottom - desttop);

	GCDBG(GCZONE_CLIP, "  clipping rect = (%d,%d)-(%d,%d), %dx%d\n",
		clipleft, cliptop, clipright, clipbottom,
		clipright - clipleft, clipbottom - cliptop);

	GCDBG(GCZONE_CLIP,
		"  adjusted clipping rect = (%d,%d)-(%d,%d), %dx%d\n",
		clipleftadj, cliptop, cliprightadj, clipbottom,
		cliprightadj - clipleftadj, clipbottom - cliptop);

	GCDBG(GCZONE_CLIP, "  clipping delta = (%d,%d)-(%d,%d)\n",
		batch->deltaleft, batch->deltatop,
		batch->deltaright, batch->deltabottom);

	GCDBG(GCZONE_CLIP, "  clipped dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		clippedleft, clippedtop, clippedright, clippedbottom,
		clippedright - clippedleft, clippedbottom - clippedtop);

exit:
	GCEXITARG(GCZONE_CLIP, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror do_fill(struct bvbltparams *bltparams,
			    struct gcbatch *batch,
			    struct srcinfo *srcinfo)
{
	enum bverror bverror;
	struct gcmofill *gcmofill;
	unsigned char *fillcolorptr;
	struct bvformatxlate *srcformat;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct bvbuffmap *dstmap = NULL;

	GCENTER(GCZONE_DO_FILL);

	/* Finish previous batch if any. */
	bverror = batch->batchend(bltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

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

	/* Update clipping/destination rect if necessary. */
	bverror = set_clip(bltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Parse the source format. */
	GCDBG(GCZONE_FORMAT, "parsing the source format.\n");

	if (!parse_format(srcinfo->geom->format, &srcformat)) {
		BVSETBLTERRORARG((srcinfo->index == 0)
					? BVERR_SRC1GEOM_FORMAT
					: BVERR_SRC2GEOM_FORMAT,
				"invalid source format (%d)",
				srcinfo->geom->format);
		goto exit;
	}

	/* Validate soource geometry. */
	if (!valid_geom(srcinfo->buf.desc, srcinfo->geom, srcformat)) {
		BVSETBLTERRORARG((srcinfo->index == 0)
					? BVERR_SRC1GEOM
					: BVERR_SRC2GEOM,
				"source%d geom exceeds surface size",
				srcinfo->index + 1);
		goto exit;
	}

	bverror = claim_buffer(batch, sizeof(struct gcmofill),
				(void **) &gcmofill);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM, "failed to allocate command buffer");
		goto exit;
	}

	/***********************************************************************
	** Set source.
	*/

	/* Set surface dummy width and height. */
	gcmofill->src.rotation_ldst = gcmofillsrc_rotation_ldst;
	gcmofill->src.rotation.raw = 0;
	gcmofill->src.rotation.reg.surf_width = dstgeom->width;
	gcmofill->src.config.raw = 0;
	gcmofill->src.rotationheight_ldst = gcmofillsrc_rotationheight_ldst;
	gcmofill->src.rotationheight.reg.height = dstgeom->height;

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
		+ srcinfo->rect->left * srcformat->bitspp / 8;

	gcmofill->clearcolor_ldst = gcmofill_clearcolor_ldst;
	gcmofill->clearcolor.raw = getinternalcolor(fillcolorptr, srcformat);

	/***********************************************************************
	** Configure and start fill.
	*/

	/* Set destination configuration. */
	gcmofill->start.config_ldst = gcmostart_config_ldst;
	gcmofill->start.config.raw = 0;
	gcmofill->start.config.reg.swizzle = batch->dstformat->swizzle;
	gcmofill->start.config.reg.format = batch->dstformat->format;
	gcmofill->start.config.reg.command = GCREG_DEST_CONFIG_COMMAND_CLEAR;

	/* Set ROP3. */
	gcmofill->start.rop_ldst = gcmostart_rop_ldst;
	gcmofill->start.rop.raw = 0;
	gcmofill->start.rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmofill->start.rop.reg.fg = (unsigned char) bltparams->op.rop;

	/* Set START_DE command. */
	gcmofill->start.startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmofill->start.rect.left = batch->dstleft + batch->dstoffset;
	gcmofill->start.rect.top = batch->dsttop;
	gcmofill->start.rect.right = batch->dstright + batch->dstoffset;
	gcmofill->start.rect.bottom = batch->dstbottom;

exit:
	GCEXITARG(GCZONE_DO_FILL, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror do_blit_end(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcmomultisrc *gcmomultisrc;
	struct gcmostart *gcmostart;
	unsigned int buffersize;

	GCENTER(GCZONE_DO_BLIT);

	GCDBG(GCZONE_BATCH, "finalizing the blit, scrcount = %d\n",
		batch->gcblit.srccount);

	/* Allocate command buffer. */
	buffersize
		= sizeof(struct gcmomultisrc)
		+ sizeof(struct gcmostart);

	bverror = claim_buffer(batch, buffersize,
				(void **) &gcmomultisrc);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM, "failed to allocate command buffer");
		goto exit;
	}

	/* Configure multi-source control. */
	gcmomultisrc->control_ldst = gcmomultisrc_control_ldst;
	gcmomultisrc->control.raw = 0;
	gcmomultisrc->control.reg.srccount = batch->gcblit.srccount - 1;
	gcmomultisrc->control.reg.horblock
		= GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128;

	/* Skip to the next structure. */
	gcmostart = (struct gcmostart *) (gcmomultisrc + 1);

	/* Set destination configuration. */
	GCDBG(GCZONE_DO_BLIT, "format entry = 0x%08X\n",
		(unsigned int) batch->dstformat);
	GCDBG(GCZONE_DO_BLIT, "  swizzle code = %d\n",
		batch->dstformat->swizzle);
	GCDBG(GCZONE_DO_BLIT, "  format code = %d\n",
		batch->dstformat->format);

	gcmostart->config_ldst = gcmostart_config_ldst;
	gcmostart->config.raw = 0;
	gcmostart->config.reg.swizzle = batch->dstformat->swizzle;
	gcmostart->config.reg.format = batch->dstformat->format;
	gcmostart->config.reg.command = batch->gcblit.multisrc
		? GCREG_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT
		: GCREG_DEST_CONFIG_COMMAND_BIT_BLT;

	/* Set ROP. */
	gcmostart->rop_ldst = gcmostart_rop_ldst;
	gcmostart->rop.raw = 0;
	gcmostart->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmostart->rop.reg.fg = (unsigned char) batch->gcblit.rop;

	/* Set START_DE command. */
	gcmostart->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmostart->rect.left = batch->dstleft + batch->dstoffset;
	gcmostart->rect.top = batch->dsttop;
	gcmostart->rect.right = batch->dstright + batch->dstoffset;
	gcmostart->rect.bottom = batch->dstbottom;

	/* Reset the finalizer. */
	batch->batchend = do_end;

	gc_debug_blt(batch->gcblit.srccount,
			abs(batch->dstright - batch->dstleft),
			abs(batch->dsttop - batch->dstbottom));

exit:
	GCEXITARG(GCZONE_DO_BLIT, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}


static inline int get_offset(struct bvbuffdesc *desc)
{
	int offset;

	if (desc->auxtype == BVAT_PHYSDESC) {
		struct bvphysdesc *pdesc;
		pdesc = (struct bvphysdesc *) desc->auxptr;

		offset = pdesc->pageoffset &
			(GC_BASE_ALIGN - 1);
	} else {
		offset = (unsigned int) desc->virtaddr &
			(GC_BASE_ALIGN - 1);
	}
	return offset;
}

static enum bverror do_blit(struct bvbltparams *bltparams,
			    struct gcbatch *batch,
			    struct srcinfo *srcinfo,
			    unsigned int srccount,
			    struct gcalpha *gca)
{
	enum bverror bverror = BVERR_NONE;

	struct gcmosrc *gcmosrc;
	struct gcmosrcalpha *gcmosrcalpha;

	unsigned int i, index;

	struct bvbuffmap *dstmap = NULL;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct bvrect *dstrect = &bltparams->dstrect;

	struct bvformatxlate *srcformat;
	struct bvbuffmap *srcmap[2] = { NULL, NULL };
	struct bvbuffdesc *srcdesc;
	struct bvsurfgeom *srcgeom;
	struct bvrect *srcrect;
	int srcleft, srctop, srcright, srcbottom;
	int srcoffset, srcsurfleft, srcsurftop;
	int srcshift, srcadjust, srcalign;
	int multisrc;

	GCENTER(GCZONE_DO_BLIT);

	/* Did destination change? */
	if ((batch->batchflags & (BVBATCH_DST |
					BVBATCH_CLIPRECT_ORIGIN |
					BVBATCH_CLIPRECT_SIZE |
					BVBATCH_DSTRECT_ORIGIN |
					BVBATCH_DSTRECT_SIZE)) != 0) {
		/* Finalize the previous blit if any. */
		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;
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

	/* Update clipping/destination rect if necessary. */
	bverror = set_clip(bltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	for (i = 0; i < srccount; i += 1) {
		/***************************************************************
		** Parse the source format.
		*/

		srcdesc = srcinfo[i].buf.desc;
		srcgeom = srcinfo[i].geom;
		srcrect = srcinfo[i].rect;

		/* Parse the source format. */
		GCDBG(GCZONE_FORMAT, "parsing the source format.\n");

		if (!parse_format(srcgeom->format, &srcformat)) {
			BVSETBLTERRORARG((srcinfo[i].index == 0)
						? BVERR_SRC1GEOM_FORMAT
						: BVERR_SRC2GEOM_FORMAT,
					"invalid source format (%d)",
					srcgeom->format);
			goto exit;
		}

		/* Validate soource geometry. */
		if (!valid_geom(srcdesc, srcgeom, srcformat)) {
			BVSETBLTERRORARG((srcinfo->index == 0)
						? BVERR_SRC1GEOM
						: BVERR_SRC2GEOM,
					"source%d geom exceeds surface size",
					srcinfo->index + 1);
			goto exit;
		}

		srcright = srcrect->left + srcrect->width + batch->deltaright;
		srcbottom = srcrect->top + srcrect->height + batch->deltabottom;

		/* Validate the rectangle. */
		if ((srcright > (int) srcgeom->width) ||
			(srcbottom > (int) srcgeom->height)) {
			BVSETBLTERRORARG((srcinfo->index == 0)
						? BVERR_SRC1RECT
						: BVERR_SRC2RECT,
					"source%d rect exceeds surface size",
					srcinfo->index + 1);
			goto exit;
		}

		GCDBG(GCZONE_SURF, "source surface %d:\n", i);
		GCDBG(GCZONE_SURF, "  srcaddr[%d] = 0x%08X\n",
			i, (unsigned int) srcdesc->virtaddr);

		GCDBG(GCZONE_SURF, "  srcsurf = %dx%d, stride = %ld\n",
			srcgeom->width, srcgeom->height,
			srcgeom->virtstride);

		GCDBG(GCZONE_SURF, "  srcrect = (%d,%d)-(%d,%d), %dx%d\n",
			srcrect->left, srcrect->top,
			srcrect->left + srcrect->width,
			srcrect->top  + srcrect->height,
			srcrect->width, srcrect->height);

		/***************************************************************
		** Determine source placement.
		*/

		/* Compute the source offset in pixels needed to compensate
		   for the surface base address misalignment if any. */
		srcoffset = get_offset(srcdesc) * 8 / srcformat->bitspp;

		GCDBG(GCZONE_SURF, "  source offset = %d\n", srcoffset);

		srcsurfleft = srcrect->left - dstrect->left + srcoffset;
		srcsurftop = srcrect->top - dstrect->top;

		GCDBG(GCZONE_SURF, "  source surfaceorigin = %d,%d\n",
			srcsurfleft, srcsurftop);

		/* (srcsurfleft,srcsurftop) are the coordinates of the source
		   surface origin. PE requires alignment of the base
		   address. Compute the alignment requirement in pixels. */
		srcalign = 16 * 8 / srcformat->bitspp;

		GCDBG(GCZONE_SURF, "  srcalign = %d\n", srcalign);

		/* Compute the number of pixels the origin has to be
		   aligned by. */
		srcadjust = srcsurfleft % srcalign;

		GCDBG(GCZONE_SURF, "  srcadjust = %d\n", srcadjust);

		/* TODO: multisrc with nv12 format */
		multisrc = (srcformat->format == GCREG_DE_FORMAT_NV12) ? 0 :
			(srcadjust == 0);

		GCDBG(GCZONE_DO_BLIT, "  multisrc = %d\n", multisrc);

		if (multisrc) {
			/* Adjust the origin. */
			srcsurfleft -= srcadjust;

			GCDBG(GCZONE_SURF,
			      "  adjusted source surface origin = %d,%d\n",
			      srcsurfleft, srcsurftop);

			/* Adjust the source rectangle for the
			 * single source walker.
			 */
			srcleft = dstrect->left + batch->deltaleft + srcadjust;
			srctop = dstrect->top + batch->deltatop;

			GCDBG(GCZONE_SURF,
			      "  source %d rectangle origin = %d,%d\n",
			      i, srcleft, srctop);

			/* Compute the surface offset in bytes. */
			srcshift
				= srcsurftop * (int) srcgeom->virtstride
				+ srcsurfleft * (int) srcformat->bitspp / 8;

			GCDBG(GCZONE_SURF, "  srcshift = 0x%08X (%d)\n",
			      srcshift, srcshift);
		} else {
			srcadjust = srcoffset % srcalign;

			GCDBG(GCZONE_SURF, "  single source srcadjust = %d\n",
			      srcadjust);

			srcleft = srcrect->left + srcadjust;
			srctop = srcrect->top;

			GCDBG(GCZONE_SURF,
			      "  source %d rectangle origin = %d,%d\n",
			      i, srcleft, srctop);

			/* No shift needed for single source walker */
			srcshift = 0;
		}

		/***************************************************************
		** Finalize existing blit if necessary.
		*/

		if ((batch->batchend != do_blit_end) ||
			(batch->gcblit.srccount == 4) ||
			!batch->gcblit.multisrc || !multisrc) {
			/* Finalize the previous blit if any. */
			bverror = batch->batchend(bltparams, batch);
			if (bverror != BVERR_NONE)
				goto exit;

			/* Initialize the new batch. */
			batch->batchend = do_blit_end;
			batch->gcblit.srccount = 0;
			batch->gcblit.multisrc = multisrc;
			batch->gcblit.rop = (gca != NULL)
				? 0xCC : bltparams->op.rop;
		}

		/***************************************************************
		** Map the source.
		*/

		bverror = do_map(srcdesc, batch, &srcmap[i]);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		/***************************************************************
		** Configure source.
		*/

		/* Allocate command buffer. */
		bverror = claim_buffer(batch, sizeof(struct gcmosrc),
					(void **) &gcmosrc);
		if (bverror != BVERR_NONE) {
			BVSETBLTERROR(BVERR_OOM,
				"failed to allocate command buffer");
			goto exit;
		}

		/* Shortcut to the register index. */
		index = batch->gcblit.srccount;

		add_fixup(batch, &gcmosrc->address, srcshift);

		/* Set surface parameters. */
		gcmosrc->address_ldst = gcmosrc_address_ldst[index];
		gcmosrc->address = GET_MAP_HANDLE(srcmap[i]);

		gcmosrc->stride_ldst = gcmosrc_stride_ldst[index];
		gcmosrc->stride = srcgeom->virtstride;

		gcmosrc->rotation_ldst = gcmosrc_rotation_ldst[index];
		gcmosrc->rotation.raw = 0;
		gcmosrc->rotation.reg.surf_width
			= dstrect->left + srcgeom->width;

		gcmosrc->config_ldst = gcmosrc_config_ldst[index];
		gcmosrc->config.raw = 0;
		gcmosrc->config.reg.swizzle = srcformat->swizzle;
		gcmosrc->config.reg.format = srcformat->format;

		gcmosrc->origin_ldst = gcmosrc_origin_ldst[index];
		if (multisrc)
			gcmosrc->origin.reg = gcregsrcorigin_min;
		else {
			gcmosrc->origin.reg.x = srcleft;
			gcmosrc->origin.reg.y = srctop;
		}

		gcmosrc->size_ldst = gcmosrc_size_ldst[index];
		gcmosrc->size.reg = gcregsrcsize_max;

		gcmosrc->rotationheight_ldst
			= gcmosrc_rotationheight_ldst[index];
		gcmosrc->rotationheight.reg.height
			= dstrect->top + srcgeom->height;

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
			int uvshift;

			if (multisrc && srcsurftop % 2) {
				/* We can't shift the uv plane by
				 * an odd number of rows.
				 */
				BVSETBLTERROR(BVERR_SRC1RECT,
					      "src/dst y coordinate combination not supported");
				goto exit;
			}

			bverror = claim_buffer(batch,
					       sizeof(struct gcmosrcplanaryuv),
					       (void **) &yuv);
			if (bverror != BVERR_NONE) {
				BVSETBLTERROR(BVERR_OOM,
					"failed to allocate command buffer for planar yuv");
				goto exit;
			}

			yuv->uplaneaddress_ldst =
				gcmosrc_uplaneaddress_ldst[index];
			yuv->uplanestride_ldst =
				gcmosrc_uplanestride_ldst[index];
			yuv->vplaneaddress_ldst =
				gcmosrc_vplaneaddress_ldst[index];
			yuv->vplanestride_ldst =
				gcmosrc_vplanestride_ldst[index];

			if (multisrc) {
				/* uv plane is half height */
				uvshift = (srcsurftop / 2) *
					(int) srcgeom->virtstride +
					srcsurfleft *
					(int) srcformat->bitspp / 8;
			} else {
				/* No shift needed for single source walker */
				uvshift = 0;
			}

			GCDBG(GCZONE_SURF, "  uvshift = 0x%08X (%d)\n",
			      uvshift, uvshift);

			/* add fixed offset from Y plane */
			uvshift += srcgeom->virtstride * srcgeom->height;

			GCDBG(GCZONE_SURF, "  final uvshift = 0x%08X (%d)\n",
			      uvshift, uvshift);

			yuv->uplaneaddress = GET_MAP_HANDLE(srcmap[i]);
			add_fixup(batch, &yuv->uplaneaddress, uvshift);

			yuv->uplanestride = srcgeom->virtstride;

			yuv->vplaneaddress = GET_MAP_HANDLE(srcmap[i]);
			add_fixup(batch, &yuv->vplaneaddress, uvshift);

			yuv->vplanestride = srcgeom->virtstride;
		}

		if ((gca != NULL) && ((srccount == 1) || (i > 0))) {
			gcmosrc->alphacontrol_ldst
				= gcmosrc_alphacontrol_ldst[index];
			gcmosrc->alphacontrol.raw = 0;
			gcmosrc->alphacontrol.reg.enable
				= GCREG_ALPHA_CONTROL_ENABLE_ON;

			/* Allocate command buffer. */
			bverror = claim_buffer(batch,
						sizeof(struct gcmosrcalpha),
						(void **) &gcmosrcalpha);
			if (bverror != BVERR_NONE) {
				BVSETBLTERROR(BVERR_OOM,
					"failed to allocate command buffer");
				goto exit;
			}

			gcmosrcalpha->alphamodes_ldst
				= gcmosrcalpha_alphamodes_ldst[index];
			gcmosrcalpha->alphamodes.raw = 0;
			gcmosrcalpha->alphamodes.reg.src_global_alpha
				= gca->src_global_alpha_mode;
			gcmosrcalpha->alphamodes.reg.dst_global_alpha
				= gca->dst_global_alpha_mode;

			if (srccount == 1) {
				GCDBG(GCZONE_BLEND, "k1 sets src blend.\n");
				GCDBG(GCZONE_BLEND, "k2 sets dst blend.\n");

				gcmosrcalpha->alphamodes.reg.src_blend
					= gca->k1->factor_mode;
				gcmosrcalpha->alphamodes.reg.src_color_reverse
					= gca->k1->color_reverse;

				gcmosrcalpha->alphamodes.reg.dst_blend
					= gca->k2->factor_mode;
				gcmosrcalpha->alphamodes.reg.dst_color_reverse
					= gca->k2->color_reverse;
			} else {
				GCDBG(GCZONE_BLEND, "k1 sets dst blend.\n");
				GCDBG(GCZONE_BLEND, "k2 sets src blend.\n");

				gcmosrcalpha->alphamodes.reg.src_blend
					= gca->k2->factor_mode;
				gcmosrcalpha->alphamodes.reg.src_color_reverse
					= gca->k2->color_reverse;

				gcmosrcalpha->alphamodes.reg.dst_blend
					= gca->k1->factor_mode;
				gcmosrcalpha->alphamodes.reg.dst_color_reverse
					= gca->k1->color_reverse;
			}

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
			gcmosrcalpha->srcglobal.raw = gca->src_global_color;

			gcmosrcalpha->dstglobal_ldst
				= gcmosrcalpha_dstglobal_ldst[index];
			gcmosrcalpha->dstglobal.raw = gca->dst_global_color;
		} else {
			GCDBG(GCZONE_BLEND, "blending disabled.\n");

			gcmosrc->alphacontrol_ldst
				= gcmosrc_alphacontrol_ldst[index];
			gcmosrc->alphacontrol.raw = 0;
			gcmosrc->alphacontrol.reg.enable
				= GCREG_ALPHA_CONTROL_ENABLE_OFF;
		}

		batch->gcblit.srccount += 1;
	}

exit:
	GCEXITARG(GCZONE_DO_BLIT, "bv%s = %d\n",
		(bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror do_filter(struct bvbltparams *bltparams,
			      struct gcbatch *batch)
{
	enum bverror bverror;
	GCENTER(GCZONE_DO_FILTER);
	BVSETBLTERROR(BVERR_SRC1_HORZSCALE, "scaling not supported");
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
	int src1used, src2used, maskused;
	struct srcinfo srcinfo[2];
	unsigned short rop;
	struct gccommit gccommit;
	int srccount, res;

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
		bverror = allocate_batch(&batch);
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
		bverror = allocate_batch(&batch);
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
		/* Verify the batch change flags. */
		VERIFYBATCH(batch->batchflags >> 12, &batch->prevdstrect,
				&bltparams->dstrect);

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
						&bltparams->dstrect)) {
				GCDBG(GCZONE_BLIT, "src1 is the same as dst\n");
			} else {
				srcinfo[srccount].index = 0;
				srcinfo[srccount].buf = bltparams->src1;
				srcinfo[srccount].geom = bltparams->src1geom;
				srcinfo[srccount].rect = &bltparams->src1rect;

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
						&bltparams->dstrect)) {
				GCDBG(GCZONE_BLIT, "src2 is the same as dst\n");
			} else {
				srcinfo[srccount].index = 1;
				srcinfo[srccount].buf = bltparams->src2;
				srcinfo[srccount].geom = bltparams->src2geom;
				srcinfo[srccount].rect = &bltparams->src2rect;

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

			BVSETERROR(BVERR_OP,
				"operation with mask not supported");
			goto exit;
		}

		GCDBG(GCZONE_BLIT, "srccount = %d\n", srccount);

		switch (srccount) {
		case 0:
			bverror = do_blit(bltparams, batch, NULL, 0, gca);
			if (bverror != BVERR_NONE)
				goto exit;
			break;

		case 1:
			if (EQ_SIZE((*srcinfo[0].rect), bltparams->dstrect))
				bverror = do_blit(bltparams, batch,
						srcinfo, 1, gca);
			else if ((srcinfo[0].rect->width == 1) &&
				(srcinfo[0].rect->height == 1))
				bverror = do_fill(bltparams, batch, srcinfo);
			else
				bverror = do_filter(bltparams, batch);
			if (bverror != BVERR_NONE)
				goto exit;
			break;

		case 2:
			if (EQ_SIZE((*srcinfo[0].rect), bltparams->dstrect))
				if (EQ_SIZE((*srcinfo[1].rect),
					bltparams->dstrect))
					bverror = do_blit(bltparams, batch,
							srcinfo, 2, gca);
				else
					BVSETBLTERROR(BVERR_SRC2_HORZSCALE,
						"scaling not supported");
			else
				if (EQ_SIZE((*srcinfo[1].rect),
					bltparams->dstrect))
					BVSETBLTERROR(BVERR_SRC1_HORZSCALE,
						"scaling not supported");
				else
					BVSETBLTERROR(BVERR_SRC1_HORZSCALE,
						"scaling not supported");
			if (bverror != BVERR_NONE)
				goto exit;
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
		bverror = claim_buffer(batch, sizeof(struct gcmoflush),
				       (void **) &flush);
		if (bverror != BVERR_NONE) {
			BVSETBLTERROR(BVERR_OOM,
				"failed to allocate command buffer");
			goto exit;
		}

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
						"callback allocation failed");
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
					"unable to allocate gccore memory");
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
	int count; /* number of planes */
	unsigned int bpp = 0; /* bytes per pixel */
	unsigned long vert_offset, horiz_offset;

	struct c2dmrgn rgn[3];
	int container_size = 0;

	unsigned long subsample = copparams->geom->format &
			OCDFMTDEF_SUBSAMPLE_MASK;
	unsigned long vendor = copparams->geom->format &
			OCDFMTDEF_VENDOR_MASK;
	unsigned long layout = copparams->geom->format &
			OCDFMTDEF_LAYOUT_MASK;
	unsigned long sizeminus1 = copparams->geom->format &
			OCDFMTDEF_COMPONENTSIZEMINUS1_MASK;
	unsigned long container = copparams->geom->format &
			OCDFMTDEF_CONTAINER_MASK;

	if (vendor != OCDFMTDEF_VENDOR_ALL) {
		bverror = BVERR_FORMAT;
		goto Error;
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

		count = 1;

		switch (subsample) {
		case OCDFMTDEF_SUBSAMPLE_NONE:
			if (sizeminus1 >= 8) {
				bpp = container_size / 8;
			} else {
				bverror = BVERR_FORMAT;
				goto Error;
			}
			break;

		case OCDFMTDEF_SUBSAMPLE_422_YCbCr:
			bpp = (container_size / 2) / 8;
			break;

		case OCDFMTDEF_SUBSAMPLE_420_YCbCr:
			bverror = BVERR_FORMAT;
			goto Error;
			break;

		case OCDFMTDEF_SUBSAMPLE_411_YCbCr:
			bverror = BVERR_FORMAT;
			goto Error;
			break;

		default:
			bverror = BVERR_FORMAT;
			goto Error;
		}

		rgn[0].span = copparams->rect->width * bpp;
		rgn[0].lines = copparams->rect->height;
		rgn[0].stride = copparams->geom->virtstride;
		horiz_offset = copparams->rect->left * bpp;
		vert_offset = copparams->rect->top;

		rgn[0].start = (void *) ((unsigned long)
				copparams->desc->virtaddr +
				vert_offset * rgn[0].stride +
				horiz_offset);
		gcbvcacheop(count, rgn, copparams->cacheop);

		break;

	case OCDFMTDEF_DISTRIBUTED:
		bverror = BVERR_FORMAT;
		break;

	/*TODO: Multi plane still need to be implemented */
	case OCDFMTDEF_2_PLANE_YCbCr:
	case OCDFMTDEF_3_PLANE_STACKED:
	case OCDFMTDEF_3_PLANE_SIDE_BY_SIDE_YCbCr:
		GCERR("not yet implemented.\n");
		break;

	default:
		bverror = BVERR_FORMAT;
	}

Error:
	return bverror;
}
