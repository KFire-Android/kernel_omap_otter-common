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

#ifndef GCDUMPBATCH_ENABLE
#	define GCDUMPBATCH_ENABLE 0
#endif

#if GCDUMPBATCH_ENABLE
#	define GCDUMPBATCH(batch) \
		dumpbatch(batch)
#else
#	define GCDUMPBATCH(batch)
#endif

/*******************************************************************************
** Miscellaneous defines and macros.
*/

#define EQ_ORIGIN(rect1, rect2) \
( \
	(rect1.left == rect2.left) && (rect1.top == rect2.top) \
)

#define EQ_SIZE(rect1, rect2) \
( \
	(rect1.width == rect2.width) && (rect1.height == rect2.height) \
)

#if 1
#define STRUCTSIZE(structptr, lastmember) \
( \
	(size_t) &structptr->lastmember + \
	sizeof(structptr->lastmember) - \
	(size_t) structptr \
)
#else
#define STRUCTSIZE(structptr, lastmember) \
( \
	(size_t) sizeof(*structptr) \
)
#endif

#define GET_MAP_HANDLE(map) \
( \
	((struct bvbuffmapinfo *) map->handle)->handle \
)

#define GC_CLIP_RESET_LEFT	((unsigned short) 0)
#define GC_CLIP_RESET_TOP	((unsigned short) 0)
#define GC_CLIP_RESET_RIGHT	((unsigned short) ((1 << 15) - 1))
#define GC_CLIP_RESET_BOTTOM	((unsigned short) ((1 << 15) - 1))

#define GC_BASE_ALIGN	16

#define GPU_CMD_SIZE	(sizeof(unsigned int) * 2)

#define GC_BUFFER_RESERVE \
( \
	sizeof(struct gcmopipesel) + \
	sizeof(struct gcmommumaster) + \
	sizeof(struct gcmommuflush) + \
	sizeof(struct gcmosignal) + \
	sizeof(struct gccmdend) \
)

/*******************************************************************************
** Internal structures.
*/

/* Used by blitters to define an array of valid sources. */
struct srcdesc {
	int index;
	union bvinbuff buf;
	struct bvsurfgeom *geom;
	struct bvrect *rect;
};

/* bvbuffmap struct attachment. */
struct bvbuffmapinfo {
	unsigned long handle;	/* Mapped handle for the buffer. */

	int usermap;		/* Number of times the client explicitely
				   mapped this buffer. */

	int automap;		/* Number of times automapping happened. */
};

/* Defines a link list of scheduled unmappings. */
struct gcschedunmap {
	struct bvbuffdesc *bvbuffdesc;
	struct gcschedunmap *next;
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
	struct bvformatxlate *format;
	struct gccmdstartderect rect;
};

/* Batch header. */
struct gcbatch {
	unsigned int structsize;	/* Used to ID structure version. */

	unsigned int dstchanged;	/* Destination change flag. */

	gcbatchend batchend;		/* Pointer to the function to finilize
					   the current operation. */
	struct gcblit gcblit;		/* States of the current operation. */

	int deltaleft;			/* Clipping deltas. */
	int deltatop;
	int deltaright;
	int deltabottom;

#if GCDEBUG_ENABLE
	struct bvrect prevdstrect;	/* Rectangle validation. */
	struct bvrect prevsrc1rect;
	struct bvrect prevsrc2rect;
	struct bvrect prevmaskrect;
#endif

	unsigned int size;		/* Total size of the command buffer. */

	struct gcbuffer *bufhead;	/* Command buffer list. */
	struct gcbuffer *buftail;

	struct gcschedunmap *unmap;	/* Scheduled unmappings. */
};

/* Vacant batch header. */
struct gcvacbatch {
	struct gcvacbatch *next;
};

/* Driver context. */
struct gccontext {
	char bverrorstr[128];		/* Last generated error message. */

	struct bvbuffmap *vac_buffmap;	/* Vacant mappping structures. */
	struct gcschedunmap *vac_unmap;	/* Vacant unmapping structures. */

	struct gcbuffer *vac_buffers;	/* Vacant command buffers. */
	struct gcfixup *vac_fixups;	/* Vacant fixups. */
	struct gcvacbatch *vac_batches;	/* Vacant batches. */
};

static struct gccontext gccontext;

/*******************************************************************************
 * Debugging.
 */

#if GCDUMPBATCH_ENABLE
static void dumpbatch(struct gcbatch *batch)
{
	struct gcbuffer *buffer;
	unsigned int i, size;
	struct gcfixup *fixup;

	GCPRINT(NULL, 0, GC_MOD_PREFIX
		"BATCH DUMP (0x%08X)\n",
		__func__, __LINE__, (unsigned int) batch);

	buffer = batch->bufhead;
	while (buffer != NULL) {
		fixup = buffer->fixuphead;
		while (fixup != NULL) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"  Fixup table @ 0x%08X, count = %d:\n",
				__func__, __LINE__,
				(unsigned int) fixup, fixup->count);

			for (i = 0; i < fixup->count; i += 1) {
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"  [%02d] buffer offset = 0x%08X, "
					"surface offset = 0x%08X\n",
					__func__, __LINE__,
					i, fixup->fixup[i].dataoffset * 4,
					fixup->fixup[i].surfoffset);
			}

			fixup = fixup->next;
		}

		size = (unsigned char *) buffer->tail
			- (unsigned char *) buffer->head;
		GCDUMPBUFFER(NULL, 0, buffer->head, 0, size);

		buffer = buffer->next;
	}
}
#endif

/*******************************************************************************
 * Error handling.
 */

#define SETERROR(message) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		"%s(%d): " message, __func__, __LINE__); \
	GCPRINT(NULL, 0, "%s\n", gccontext.bverrorstr); \
} while (0)

#define SETERRORARG(message, arg) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		"%s(%d): " message, __func__, __LINE__, arg); \
	GCPRINT(NULL, 0, "%s\n", gccontext.bverrorstr); \
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
	GCPRINT(NULL, 0, "%s\n", gccontext.bverrorstr); \
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
 * Memory management.
 */

static enum bverror do_map(struct bvbuffdesc *buffdesc, int client,
				struct bvbuffmap **map)
{
	static const int mapsize
		= sizeof(struct bvbuffmap)
		+ sizeof(struct bvbuffmapinfo);

	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;
	struct gcmap gcmap;
	struct bvphysdesc *physdesc;

	/* Try to find existing mapping. */
	bvbuffmap = buffdesc->map;
	while (bvbuffmap != NULL) {
		if (bvbuffmap->bv_unmap == gcbv_unmap)
			break;
		bvbuffmap = bvbuffmap->nextmap;
	}

	/* Already mapped? */
	if (bvbuffmap != NULL) {
		bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

		if (client)
			bvbuffmapinfo->usermap += 1;
		else
			bvbuffmapinfo->automap += 1;

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"buffer already mapped:\n",
			__func__, __LINE__);

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"  virtaddr = 0x%08X\n",
			__func__, __LINE__,
			buffdesc->virtaddr);

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"  addr = 0x%08X\n",
			__func__, __LINE__,
			GET_MAP_HANDLE(bvbuffmap));

		*map = bvbuffmap;
		return BVERR_NONE;
	}

	/* New mapping, allocate a record. */
	if (gccontext.vac_buffmap == NULL) {
		bvbuffmap = gcalloc(struct bvbuffmap, mapsize);
		if (bvbuffmap == NULL) {
			BVSETERROR(BVERR_OOM,
					"failed to allocate mapping record");
			goto exit;
		}

		bvbuffmap->structsize = sizeof(struct bvbuffmap);
		bvbuffmap->bv_unmap = gcbv_unmap;
		bvbuffmap->handle = (unsigned long) (bvbuffmap + 1);
	} else {
		bvbuffmap = gccontext.vac_buffmap;
		gccontext.vac_buffmap = bvbuffmap->nextmap;
	}

	/* Map the buffer. */
	gcmap.gcerror = GCERR_NONE;
	gcmap.logical = 0;
	gcmap.size = buffdesc->length;
	gcmap.handle = 0;
	if (buffdesc->auxtype == BVAT_PHYSDESC) {
		physdesc = buffdesc->auxptr;
		gcmap.pagecount = physdesc->pagecount;
		gcmap.pagearray = physdesc->pagearray;
		/* TODO: structsize needs to be checked to validate the
		 * version */
	}

	gc_map(&gcmap);

	bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

	bvbuffmapinfo->handle = gcmap.handle;

	if (client) {
		bvbuffmapinfo->usermap = 1;
		bvbuffmapinfo->automap = 0;
	} else {
		bvbuffmapinfo->usermap = 0;
		bvbuffmapinfo->automap = 1;
	}

	bvbuffmap->nextmap = buffdesc->map;
	buffdesc->map = bvbuffmap;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"new mapping:\n",
		__func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  virtaddr = 0x%08X\n",
		__func__, __LINE__,
		buffdesc->virtaddr);

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
		"  addr = 0x%08X\n",
		__func__, __LINE__,
		GET_MAP_HANDLE(bvbuffmap));

	*map = bvbuffmap;
	return BVERR_NONE;

exit:
	if (bvbuffmap != NULL) {
		bvbuffmap->nextmap = gccontext.vac_buffmap;
		gccontext.vac_buffmap = bvbuffmap;
	}

	return bverror;
}

static enum bverror do_unmap(struct bvbuffdesc *buffdesc, int client)
{
	enum bverror bverror;
	struct bvbuffmap *prev = NULL;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;
	struct gcmap gcmap;

	/* Try to find existing mapping. */
	bvbuffmap = buffdesc->map;
	while (bvbuffmap != NULL) {
		if (bvbuffmap->bv_unmap == gcbv_unmap)
			break;
		prev = bvbuffmap;
		bvbuffmap = bvbuffmap->nextmap;
	}

	/* No mapping found? */
	if (bvbuffmap == NULL) {
		bverror = BVERR_NONE;
		goto exit;
	}

	/* Get the info structure. */
	bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

	/* Dereference. */
	if (client)
		bvbuffmapinfo->usermap = 0;
	else
		bvbuffmapinfo->automap -= 1;

	/* Still referenced? */
	if (bvbuffmapinfo->usermap || bvbuffmapinfo->automap) {
		bverror = BVERR_NONE;
		goto exit;
	}

	/* Setup buffer unmapping. */
	gcmap.gcerror = GCERR_NONE;
	gcmap.size = buffdesc->length;
	gcmap.handle = GET_MAP_HANDLE(bvbuffmap);

	/* Remove mapping record. */
	if (prev == NULL)
		buffdesc->map = bvbuffmap->nextmap;
	else
		prev->nextmap = bvbuffmap->nextmap;

	bvbuffmap->nextmap = gccontext.vac_buffmap;
	gccontext.vac_buffmap = bvbuffmap;

	/* Unmap the buffer. */
	gc_unmap(&gcmap);

	bverror = BVERR_NONE;

exit:
	return bverror;
}

static enum bverror schedule_unmap(struct gcbatch *batch,
					struct bvbuffdesc *buffdesc)
{
	enum bverror bverror;
	struct gcschedunmap *gcschedunmap;

	if (gccontext.vac_unmap == NULL) {
		gcschedunmap = gcalloc(struct gcschedunmap,
					sizeof(struct gcschedunmap));
		if (gcschedunmap == NULL) {
			BVSETERROR(BVERR_OOM, "failed to schedule unmapping");
			goto exit;
		}
	} else {
		gcschedunmap = gccontext.vac_unmap;
		gccontext.vac_unmap = gcschedunmap->next;
	}

	gcschedunmap->bvbuffdesc = buffdesc;
	gcschedunmap->next = batch->unmap;
	batch->unmap = gcschedunmap;

	bverror = BVERR_NONE;

exit:
	return bverror;
}

static enum bverror process_scheduled_unmap(struct gcbatch *batch)
{
	enum bverror bverror = BVERR_NONE;
	struct gcschedunmap *gcschedunmap;

	while (batch->unmap != NULL) {
		gcschedunmap = batch->unmap;

		bverror = do_unmap(gcschedunmap->bvbuffdesc, 0);
		if (bverror != BVERR_NONE)
			break;

		batch->unmap = gcschedunmap->next;
		gcschedunmap->next = gccontext.vac_unmap;
		gccontext.vac_unmap = gcschedunmap;
	}

	return bverror;
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

	GCPRINT(GCDBGFILTER, GCZONE_BATCH, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	if (gccontext.vac_batches == NULL) {
		temp = gcalloc(struct gcbatch, sizeof(struct gcbatch));
		if (temp == NULL) {
			BVSETERROR(BVERR_OOM,
					"batch header allocation failed");
			goto exit;
		}

		GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
			"allocated new batch = 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);
	} else {
		temp = (struct gcbatch *) gccontext.vac_batches;
		gccontext.vac_batches = gccontext.vac_batches->next;

		GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
			"reusing batch = 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);
	}

	memset(temp, 0, sizeof(struct gcbatch));
	temp->structsize = sizeof(struct gcbatch);
	temp->batchend = do_end;

	bverror = append_buffer(temp);
	if (bverror != BVERR_NONE) {
		free_batch(temp);
		goto exit;
	}

	GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
		"batch allocated = 0x%08X\n",
		__func__, __LINE__, temp);

	*batch = temp;

exit:
	GCPRINT(GCDBGFILTER, GCZONE_BATCH, "--" GC_MOD_PREFIX
		"bverror = %d\n", __func__, __LINE__, bverror);

	return bverror;
}

static void free_batch(struct gcbatch *batch)
{
	struct gcbuffer *buffer;

	GCPRINT(GCDBGFILTER, GCZONE_BATCH, "++" GC_MOD_PREFIX
		"batch = 0x%08X\n",
		__func__, __LINE__, (unsigned int) batch);

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

	GCPRINT(GCDBGFILTER, GCZONE_BATCH, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
}

static enum bverror append_buffer(struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcbuffer *temp;

	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, "++" GC_MOD_PREFIX
		"batch = 0x%08X\n",
		__func__, __LINE__, (unsigned int) batch);

	if (gccontext.vac_buffers == NULL) {
		temp = gcalloc(struct gcbuffer, GC_BUFFER_SIZE);
		if (temp == NULL) {
			BVSETERROR(BVERR_OOM,
					"command buffer allocation failed");
			goto exit;
		}

		GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
			"allocated new buffer = 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);
	} else {
		temp = gccontext.vac_buffers;
		gccontext.vac_buffers = temp->next;

		GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
			"reusing buffer = 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);
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

	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
		"new buffer appended = 0x%08X\n",
		__func__, __LINE__, (unsigned int) temp);

	bverror = BVERR_NONE;

exit:
	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, "--" GC_MOD_PREFIX
		"bverror = %d\n", __func__, __LINE__, bverror);

	return bverror;
}

static enum bverror add_fixup(struct gcbatch *batch, unsigned int *fixup,
				unsigned int surfoffset)
{
	enum bverror bverror;
	struct gcbuffer *buffer;
	struct gcfixup *temp;

	GCPRINT(GCDBGFILTER, GCZONE_FIXUP, "++" GC_MOD_PREFIX
		"batch = 0x%08X, fixup ptr = 0x%08X\n",
		__func__, __LINE__,
		(unsigned int) batch, (unsigned int) fixup);

	buffer = batch->buftail;
	temp = buffer->fixuptail;

	GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
		"buffer = 0x%08X, fixup struct = 0x%08X\n",
		__func__, __LINE__,
		(unsigned int) buffer, (unsigned int) temp);

	if ((temp == NULL) || (temp->count == GC_FIXUP_MAX)) {
		if (gccontext.vac_fixups == NULL) {
			temp = gcalloc(struct gcfixup, sizeof(struct gcfixup));
			if (temp == NULL) {
				BVSETERROR(BVERR_OOM,
						"fixup allocation failed");
				goto exit;
			}
		} else {
			temp = gccontext.vac_fixups;
			gccontext.vac_fixups = temp->next;
		}

		temp->next = NULL;
		temp->count = 0;

		if (buffer->fixuphead == NULL)
			buffer->fixuphead = temp;
		else
			buffer->fixuptail->next = temp;
		buffer->fixuptail = temp;

		GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
			"new fixup struct allocated = 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);

	} else {
		GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
			"fixups accumulated = %d\n",
			__func__, __LINE__, temp->count);
	}

	temp->fixup[temp->count].dataoffset = fixup - buffer->head;
	temp->fixup[temp->count].surfoffset = surfoffset;
	temp->count += 1;

	GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
		"fixup offset = 0x%08X\n",
		__func__, __LINE__, fixup - buffer->head);
	GCPRINT(GCDBGFILTER, GCZONE_FIXUP, GC_MOD_PREFIX
		"surface offset = 0x%08X\n",
		__func__, __LINE__, surfoffset);

	bverror = BVERR_NONE;

exit:
	GCPRINT(GCDBGFILTER, GCZONE_FIXUP, "--" GC_MOD_PREFIX
		"bverror = %d\n", __func__, __LINE__, bverror);

	return bverror;
}

static enum bverror claim_buffer(struct gcbatch *batch,
					unsigned int size,
					void **buffer)
{
	enum bverror bverror;
	struct gcbuffer *curbuf;

	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, "++" GC_MOD_PREFIX
		"batch = 0x%08X, size = %d\n",
		__func__, __LINE__, (unsigned int) batch, size);

	/* Get the current command buffer. */
	curbuf = batch->buftail;

	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
		"buffer = 0x%08X, available = %d\n",
		__func__, __LINE__,
		(unsigned int) curbuf, curbuf->available);

	if (curbuf->available < size) {
		bverror = append_buffer(batch);
		if (bverror != BVERR_NONE)
			goto exit;

		curbuf = batch->buftail;
	}

	if (curbuf->available < size) {
		GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
			"requested size is too large.\n",
			__func__, __LINE__);

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
	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, "--" GC_MOD_PREFIX
		"bverror = %d\n", __func__, __LINE__, bverror);

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

static struct bvformatxlate formatxlate[] = {
	/* BITS=12 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X4R4G4B4, RGBA,
		BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4, 4), BVALPHA(0, 0)),

	/* BITS=12 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X4R4G4B4, ARGB,
		BVRED(8, 4), BVGREEN(4, 4), BVBLUE(0, 4), BVALPHA(12, 0)),

	/* BITS=12 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X4R4G4B4, BGRA,
		BVRED(4, 4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0, 0)),

	/* BITS=12 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X4R4G4B4, ABGR,
		BVRED(0, 4), BVGREEN(4, 4), BVBLUE(8, 4), BVALPHA(12, 0)),

	/* BITS=12 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A4R4G4B4, RGBA,
		BVRED(12, 4), BVGREEN(8, 4), BVBLUE(4, 4), BVALPHA(0, 4)),

	/* BITS=12 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A4R4G4B4, ARGB,
		BVRED(8, 4), BVGREEN(4, 4), BVBLUE(0, 4), BVALPHA(12, 4)),

	/* BITS=12 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A4R4G4B4, BGRA,
		BVRED(4, 4), BVGREEN(8, 4), BVBLUE(12, 4), BVALPHA(0, 4)),

	/* BITS=12 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A4R4G4B4, ABGR,
		BVRED(0, 4), BVGREEN(4, 4), BVBLUE(8, 4), BVALPHA(12, 4)),

	/***********************************************/

	/* BITS=15 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X1R5G5B5, RGBA,
		BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1, 5), BVALPHA(0, 0)),

	/* BITS=15 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X1R5G5B5, ARGB,
		BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0, 5), BVALPHA(15, 0)),

	/* BITS=15 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, X1R5G5B5, BGRA,
		BVRED(1, 5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* BITS=15 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, X1R5G5B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 0)),

	/* BITS=15 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A1R5G5B5, RGBA,
		BVRED(11, 5), BVGREEN(6, 5), BVBLUE(1, 5), BVALPHA(0, 1)),

	/* BITS=15 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A1R5G5B5, ARGB,
		BVRED(10, 5), BVGREEN(5, 5), BVBLUE(0, 5), BVALPHA(15, 1)),

	/* BITS=15 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, A1R5G5B5, BGRA,
		BVRED(1, 5), BVGREEN(6, 5), BVBLUE(11, 5), BVALPHA(0, 1)),

	/* BITS=15 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, A1R5G5B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 5), BVBLUE(10, 5), BVALPHA(15, 1)),

	/***********************************************/

	/* BITS=16 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, R5G6B5, ARGB,
		BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0, 5), BVALPHA(0, 0)),

	/* BITS=16 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, R5G6B5, ARGB,
		BVRED(11, 5), BVGREEN(5, 6), BVBLUE(0, 5), BVALPHA(0, 0)),

	/* BITS=16 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(16, R5G6B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* BITS=16 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(16, R5G6B5, ABGR,
		BVRED(0, 5), BVGREEN(5, 6), BVBLUE(11, 5), BVALPHA(0, 0)),

	/* BITS=16 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATINVALID,

	/* BITS=16 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATINVALID,

	/* BITS=16 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATINVALID,

	/* BITS=16 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATINVALID,

	/***********************************************/

	/* BITS=24 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, X8R8G8B8, RGBA,
		BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8, 8), BVALPHA(0, 0)),

	/* BITS=24 ALPHA=0 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, X8R8G8B8, ARGB,
		BVRED(16, 8), BVGREEN(8, 8), BVBLUE(0, 8), BVALPHA(0, 0)),

	/* BITS=24 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, X8R8G8B8, BGRA,
		BVRED(8, 8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0, 0)),

	/* BITS=24 ALPHA=0 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, X8R8G8B8, ABGR,
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(0, 0)),

	/* BITS=24 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, A8R8G8B8, RGBA,
		BVRED(24, 8), BVGREEN(16, 8), BVBLUE(8, 8), BVALPHA(0, 8)),

	/* BITS=24 ALPHA=1 REVERSED=0 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, A8R8G8B8, ARGB,
		BVRED(16, 8), BVGREEN(8, 8), BVBLUE(0, 8), BVALPHA(24, 8)),

	/* BITS=24 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=0 */
	BVFORMATRGBA(32, A8R8G8B8, BGRA,
		BVRED(8, 8), BVGREEN(16, 8), BVBLUE(24, 8), BVALPHA(0, 8)),

	/* BITS=24 ALPHA=1 REVERSED=1 LEFT_JUSTIFIED=1 */
	BVFORMATRGBA(32, A8R8G8B8, ABGR,
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(24, 8)),
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

	GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
		"ocdformat = 0x%08X\n",
		__func__, __LINE__, ocdformat);

	cs = (ocdformat & OCDFMTDEF_CS_MASK) >> OCDFMTDEF_CS_SHIFT;
	bits = (ocdformat & OCDFMTDEF_COMPONENTSIZEMINUS1_MASK)
		>> OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT;
	cont = (ocdformat & OCDFMTDEF_CONTAINER_MASK)
		>> OCDFMTDEF_CONTAINER_SHIFT;

	switch (cs) {
	case (OCDFMTDEF_CS_RGB >> OCDFMTDEF_CS_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
			"OCDFMTDEF_CS_RGB\n",
			__func__, __LINE__);

		GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
			"bits = %d\n",
			__func__, __LINE__, bits);

		GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
			"cont = %d\n",
			__func__, __LINE__, cont);

		if ((ocdformat & OCDFMTDEF_LAYOUT_MASK) != OCDFMTDEF_PACKED)
			return 0;

		swizzle = (ocdformat & OCDFMTDEF_PLACEMENT_MASK)
			>> OCDFMTDEF_PLACEMENT_SHIFT;
		alpha = (ocdformat & OCDFMTDEF_ALPHA_MASK)
			>> OCDFMTDEF_ALPHA_SHIFT;

		GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
			"swizzle = %d\n",
			__func__, __LINE__, swizzle);

		GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
			"alpha = %d\n",
			__func__, __LINE__, alpha);

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

		GCPRINT(GCDBGFILTER, GCZONE_FORMAT, GC_MOD_PREFIX
			"index = %d\n",
			__func__, __LINE__, index);

		break;

	default:
		return 0;
	}

	if (formatxlate[index].bitspp != containers[cont])
		return 0;

	*format = &formatxlate[index];

	return 1;
}

static inline unsigned int extract_component(unsigned int pixel,
						struct bvcomponent *desc)
{
	unsigned int component;
	unsigned int component8;

	component = (pixel & desc->mask) >> desc->shift;
	GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
		"mask=0x%08X, shift=%d, component=0x%08X\n",
		__func__, __LINE__, desc->mask, desc->shift, component);

	switch (desc->size) {
	case 0:
		component8 = 0xFF;
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"component8=0x%08X\n",
			__func__, __LINE__, component8);
		break;

	case 1:
		component8 = component ? 0xFF : 0x00;
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"component8=0x%08X\n",
			__func__, __LINE__, component8);
		break;

	case 4:
		component8 = component | (component << 4);
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"component8=0x%08X\n",
			__func__, __LINE__, component8);
		break;

	case 5:
		component8 = (component << 3) | (component >> 2);
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"component8=0x%08X\n",
			__func__, __LINE__, component8);
		break;

	case 6:
		component8 = (component << 2) | (component >> 4);
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"component8=0x%08X\n",
			__func__, __LINE__, component8);
		break;

	default:
		component8 = component;
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"component8=0x%08X\n",
			__func__, __LINE__, component8);
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
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"srcpixel=0x%08X\n",
			__func__, __LINE__, srcpixel);
		break;

	case 32:
		srcpixel = *(unsigned int *) ptr;
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"srcpixel=0x%08X\n",
			__func__, __LINE__, srcpixel);
		break;

	default:
		srcpixel = 0;
		GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
			"srcpixel=0x%08X\n",
			__func__, __LINE__, srcpixel);
	}

	r = extract_component(srcpixel, &format->rgba.r);
	g = extract_component(srcpixel, &format->rgba.g);
	b = extract_component(srcpixel, &format->rgba.b);
	a = extract_component(srcpixel, &format->rgba.a);

	GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
		"(r,g,b,a)=0x%02X,0x%02X,0x%02X,0x%02X\n",
		__func__, __LINE__, r, g, b, a);

	dstpixel = (a << 24) | (r << 16) | (g <<  8) | b;

	GCPRINT(GCDBGFILTER, GCZONE_COLOR, GC_MOD_PREFIX
		"dstpixel=0x%08X\n",
		__func__, __LINE__, dstpixel);

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

	GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
		"blend = 0x%08X (%s)\n",
		__func__, __LINE__, blend, bv_blend_name(blend));

	if ((blend & BVBLENDDEF_REMOTE) != 0) {
		BVSETBLTERROR(BVERR_BLEND, "remote alpha not supported");
		goto exit;
	}

	global = (blend & BVBLENDDEF_GLOBAL_MASK) >> BVBLENDDEF_GLOBAL_SHIFT;

	switch (global) {
	case (BVBLENDDEF_GLOBAL_NONE >> BVBLENDDEF_GLOBAL_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
			"BVBLENDDEF_GLOBAL_NONE\n",
			__func__, __LINE__);

		gca->src_global_color =
		gca->dst_global_color = 0;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		break;

	case (BVBLENDDEF_GLOBAL_UCHAR >> BVBLENDDEF_GLOBAL_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
			"BVBLENDDEF_GLOBAL_UCHAR\n",
			__func__, __LINE__);

		gca->src_global_color =
		gca->dst_global_color =
			((unsigned int) bltparams->globalalpha.size8) << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;

	case (BVBLENDDEF_GLOBAL_FLOAT >> BVBLENDDEF_GLOBAL_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
			"BVBLENDDEF_GLOBAL_FLOAT\n",
			__func__, __LINE__);

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

	GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
		"k1 = %d\n",
		__func__, __LINE__, k1);

	GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
		"k2 = %d\n",
		__func__, __LINE__, k2);

	GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
		"k3 = %d\n",
		__func__, __LINE__, k3);

	GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
		"k4 = %d\n",
		__func__, __LINE__, k4);

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
 * Surface validation.
 */

static int verify_surface(unsigned int tile,
				union bvinbuff *surf,
				struct bvsurfgeom *geom,
				struct bvrect *rect)
{
	if (tile) {
		if (surf->tileparams == NULL)
			return GCBVERR_TILE;

		if (surf->tileparams->structsize <
		    STRUCTSIZE(surf->tileparams, srcheight))
			return GCBVERR_TILE_VERS;

#if 0
		if (surf->tileparams->virtaddr == NULL)
			return GCBVERR_TILE_VIRTADDR;
#endif
		/* FIXME/TODO */
		return GCBVERR_TILE;
	} else {
		if (surf->desc == NULL)
			return GCBVERR_DESC;

		if (surf->desc->structsize < STRUCTSIZE(surf->desc, map))
			return GCBVERR_DESC_VERS;

#if 0
		if (surf->desc->virtaddr == NULL)
			return GCBVERR_DESC_VIRTADDR;
#endif
	}

	if (geom == NULL)
		return GCBVERR_GEOM;

	if (geom->structsize < STRUCTSIZE(geom, palette))
		return GCBVERR_GEOM_VERS;

	{
		struct bvformatxlate *format;
		if (parse_format(geom->format, &format)) {
			unsigned int geomsize;
			unsigned int rectsize;

			geomsize
				= (geom->width
				*  geom->height
				*  format->bitspp) / 8;

			rectsize
				= (rect->top + rect->height - 1)
				* geom->virtstride
				+ ((rect->left + rect->width)
				* format->bitspp) / 8;

			if ((geomsize > surf->desc->length) ||
				(rectsize > surf->desc->length)) {
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"*** INVALID SURFACE PARAMETERS\n",
					__func__, __LINE__);
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"    dimensions = %dx%d\n",
					__func__, __LINE__,
					geom->width, geom->height);
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"    rectangle = (%d,%d %dx%d)\n",
					__func__, __LINE__,
					rect->left, rect->top,
					rect->width, rect->height);
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"    size based on dimensions = %d\n",
					__func__, __LINE__,
					geomsize);
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"    size based on rectangle = %d\n",
					__func__, __LINE__,
					rectsize);
				GCPRINT(NULL, 0, GC_MOD_PREFIX
					"    size specified = %d\n",
					__func__, __LINE__,
					surf->desc->length);

				return GCBVERR_GEOM;
			}
		}
	}

	return -1;
}

#if GCDEBUG_ENABLE
#	define VERIFYBATCH(changeflags, prevrect, currrect) \
		verify_batch(__func__, __LINE__, \
				changeflags, prevrect, currrect)

static void verify_batch(const char *function, int line,
				unsigned int changeflags,
				struct bvrect *prevrect,
				struct bvrect *currrect)
{
	if ((changeflags & 1) == 0) {
		/* Origin did not change. */
		if ((prevrect->left != currrect->left) ||
			(prevrect->top != currrect->top)) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"ERROR: origin changed\n",
				function, line);
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"  previous = %d,%d\n",
				function, line,
				prevrect->left,
				prevrect->top);
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"  current = %d,%d\n",
				function, line,
				currrect->left,
				currrect->top);
		}
	}

	if ((changeflags & 2) == 0) {
		/* Size did not change. */
		if ((prevrect->width != currrect->width) ||
			(prevrect->height != currrect->height)) {
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"ERROR: size changed\n",
				function, line);
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"  previous = %dx%d\n",
				function, line,
				prevrect->width,
				prevrect->height);
			GCPRINT(NULL, 0, GC_MOD_PREFIX
				"  current = %dx%d\n",
				function, line,
				currrect->width,
				currrect->height);
		}
	}

	prevrect->left = currrect->left;
	prevrect->top = currrect->top;
	prevrect->width = currrect->width;
	prevrect->height = currrect->height;
}
#else
#	define VERIFYBATCH(changeflags, prevrect, currrect)
#endif

/*******************************************************************************
 * Primitive renderers.
 */

static enum bverror set_dst(struct bvbltparams *bltparams,
				struct gcbatch *batch,
				struct bvbuffmap *dstmap)
{
	enum bverror bverror;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct bvrect *dstrect = &bltparams->dstrect;
	struct bvrect *cliprect = &bltparams->cliprect;
	int destleft, desttop, destright, destbottom;
	int clipleft, cliptop, clipright, clipbottom;
	int clippedleft, clippedtop, clippedright, clippedbottom;
	struct gcmodst *gcmodst;

	GCPRINT(GCDBGFILTER, GCZONE_DEST, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

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

	/* Set the destination rectangle. */
	batch->gcblit.rect.left = clippedleft;
	batch->gcblit.rect.top = clippedtop;
	batch->gcblit.rect.right = clippedright;
	batch->gcblit.rect.bottom = clippedbottom;

	/* Parse the destination format. */
	if (!parse_format(dstgeom->format, &batch->gcblit.format)) {
		BVSETBLTERRORARG(BVERR_DSTGEOM_FORMAT,
				"invalid destination format (%d)",
				dstgeom->format);
		goto exit;
	}

	/* Allocate command buffer. */
	bverror = claim_buffer(batch, sizeof(struct gcmodst),
				(void **) &gcmodst);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM,
				"failed to allocate command buffer");
		goto exit;
	}

	memset(gcmodst, 0, sizeof(struct gcmodst));

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"allocated %d of commmand buffer\n",
		__func__, __LINE__, sizeof(struct gcmodst));

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"destination surface:\n",
		__func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  dstvirtaddr = 0x%08X\n",
		__func__, __LINE__,
		(unsigned int) bltparams->dstdesc->virtaddr);

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  dstaddr = 0x%08X\n",
		__func__, __LINE__,
		GET_MAP_HANDLE(dstmap));

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  dstsurf = %dx%d, stride = %ld\n",
		__func__, __LINE__,
		dstgeom->width,
		dstgeom->height,
		dstgeom->virtstride);

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		__func__, __LINE__,
		destleft, desttop, destright, destbottom,
		destright - destleft, destbottom - desttop);

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  clipping rect = (%d,%d)-(%d,%d), %dx%d\n",
		__func__, __LINE__,
		clipleft, cliptop, clipright, clipbottom,
		clipright - clipleft, clipbottom - cliptop);

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  clipping delta = (%d,%d)-(%d,%d)\n",
		__func__, __LINE__,
		batch->deltaleft, batch->deltatop,
		batch->deltaright, batch->deltabottom);

	GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
		"  clipped dstrect = (%d,%d)-(%d,%d), %dx%d\n",
		__func__, __LINE__,
		clippedleft, clippedtop, clippedright, clippedbottom,
		clippedright - clippedleft, clippedbottom - clippedtop);

	/* Add the address fixup. */
	add_fixup(batch, &gcmodst->address, 0);

	/* Set surface parameters. */
	gcmodst->address_ldst = gcmodst_address_ldst;
	gcmodst->address = GET_MAP_HANDLE(dstmap);
	gcmodst->stride = dstgeom->virtstride;

	/* Set surface width and height. */
	gcmodst->rotation.reg.surf_width = dstgeom->width;
	gcmodst->rotationheight_ldst = gcmodst_rotationheight_ldst;
	gcmodst->rotationheight.reg.height = dstgeom->height;

	/* Set clipping. */
	gcmodst->clip.lt_ldst = gcmoclip_lt_ldst;
	gcmodst->clip.lt.reg.left = clipleft;
	gcmodst->clip.lt.reg.top = cliptop;
	gcmodst->clip.rb.reg.right = clipright;
	gcmodst->clip.rb.reg.bottom = clipbottom;

exit:
	GCPRINT(GCDBGFILTER, GCZONE_DEST, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return bverror;
}

static enum bverror do_fill(struct bvbltparams *bltparams,
				struct gcbatch *batch,
				struct srcdesc *srcdesc)
{
	enum bverror bverror;
	enum bverror unmap_bverror;
	struct gcmofill *gcmofill;
	unsigned char *fillcolorptr;
	struct bvformatxlate *srcformat;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct bvbuffmap *dstmap = NULL;

	GCPRINT(GCDBGFILTER, GCZONE_DO_FILL, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* Finish previous batch if any. */
	bverror = batch->batchend(bltparams, batch);
	if (bverror != BVERR_NONE)
		goto exit;

	/* Did destination change? */
	if (batch->dstchanged) {
		GCPRINT(GCDBGFILTER, GCZONE_DO_FILL, GC_MOD_PREFIX
			"destination changed, applying new parameters\n",
			__func__, __LINE__);

		/* Map the destination. */
		bverror = do_map(bltparams->dstdesc, 0, &dstmap);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		/* Set the new destination. */
		bverror = set_dst(bltparams, batch, dstmap);
		if (bverror != BVERR_NONE)
			goto exit;
	}

	/* Parse the source format. */
	if (!parse_format(srcdesc->geom->format, &srcformat)) {
		BVSETBLTERRORARG((srcdesc->index == 0)
					? BVERR_SRC1GEOM_FORMAT
					: BVERR_SRC2GEOM_FORMAT,
				"invalid source format (%d)",
				srcdesc->geom->format);
		goto exit;
	}

	bverror = claim_buffer(batch, sizeof(struct gcmofill),
				(void **) &gcmofill);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM, "failed to allocate command buffer");
		goto exit;
	}

	memset(gcmofill, 0, sizeof(struct gcmofill));

	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
		"allocated %d of commmand buffer\n",
		__func__, __LINE__, sizeof(struct gcmofill));

	/***********************************************************************
	** Set source.
	*/

	/* Set surface dummy width and height. */
	gcmofill->src.rotation_ldst = gcmofillsrc_rotation_ldst;
	gcmofill->src.rotation.reg.surf_width = dstgeom->width;
	gcmofill->src.rotationheight_ldst = gcmofillsrc_rotationheight_ldst;
	gcmofill->src.rotationheight.reg.height = dstgeom->height;

	/* Disable alpha blending. */
	gcmofill->src.alphacontrol_ldst = gcmofillsrc_alphacontrol_ldst;
	gcmofill->src.alphacontrol.reg.enable = GCREG_ALPHA_CONTROL_ENABLE_OFF;

	/***********************************************************************
	** Set fill color.
	*/

	fillcolorptr
		= (unsigned char *) srcdesc->buf.desc->virtaddr
		+ srcdesc->rect->top * srcdesc->geom->virtstride
		+ srcdesc->rect->left * srcformat->bitspp / 8;

	gcmofill->clearcolor_ldst = gcmofill_clearcolor_ldst;
	gcmofill->clearcolor.raw = getinternalcolor(fillcolorptr, srcformat);

	/***********************************************************************
	** Configure and start fill.
	*/

	/* Set destination configuration. */
	gcmofill->start.config_ldst = gcmostart_config_ldst;
	gcmofill->start.config.reg.swizzle = batch->gcblit.format->swizzle;
	gcmofill->start.config.reg.format = batch->gcblit.format->format;
	gcmofill->start.config.reg.command = GCREG_DEST_CONFIG_COMMAND_CLEAR;

	/* Set ROP3. */
	gcmofill->start.rop_ldst = gcmostart_rop_ldst;
	gcmofill->start.rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmofill->start.rop.reg.fg = (unsigned char) bltparams->op.rop;

	/* Set START_DE command. */
	gcmofill->start.startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmofill->start.rect = batch->gcblit.rect;

	/* Flush PE cache. */
	gcmofill->start.flush.flush_ldst = gcmoflush_flush_ldst;
	gcmofill->start.flush.flush.reg = gcregflush_pe2D;

exit:
	if (dstmap != NULL) {
		unmap_bverror = schedule_unmap(batch, bltparams->dstdesc);
		if ((unmap_bverror != BVERR_NONE) && (bverror == BVERR_NONE)) {
			bltparams->errdesc = gccontext.bverrorstr;
			bverror = unmap_bverror;
		}
	}

	GCPRINT(GCDBGFILTER, GCZONE_DO_FILL, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return bverror;
}

static enum bverror do_blit_end(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcmomultisrc *gcmomultisrc;
	struct gcmostart *gcmostart;
	unsigned int buffersize;

	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
		"finalizing the blit, scrcount = %d\n",
		__func__, __LINE__, batch->gcblit.srccount);

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

	memset(gcmomultisrc, 0, buffersize);

	GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
		"allocated %d of commmand buffer\n",
		__func__, __LINE__, buffersize);

	/* Configure multi-source control. */
	gcmomultisrc->control_ldst = gcmomultisrc_control_ldst;
	gcmomultisrc->control.reg.srccount = batch->gcblit.srccount - 1;
	gcmomultisrc->control.reg.horblock
		= GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128;

	/* Skip to the next structure. */
	gcmostart = (struct gcmostart *) (gcmomultisrc + 1);

	/* Set destination configuration. */
	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, GC_MOD_PREFIX
		"format entry = 0x%08X\n",
		__func__, __LINE__, batch->gcblit.format);

	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, GC_MOD_PREFIX
		"  swizzle code = %d\n",
		__func__, __LINE__, batch->gcblit.format->swizzle);

	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, GC_MOD_PREFIX
		"  format code = %d\n",
		__func__, __LINE__, batch->gcblit.format->format);

	gcmostart->config_ldst = gcmostart_config_ldst;
	gcmostart->config.reg.swizzle = batch->gcblit.format->swizzle;
	gcmostart->config.reg.format = batch->gcblit.format->format;
	gcmostart->config.reg.command = batch->gcblit.multisrc
		? GCREG_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT
		: GCREG_DEST_CONFIG_COMMAND_BIT_BLT;

	/* Set ROP. */
	gcmostart->rop_ldst = gcmostart_rop_ldst;
	gcmostart->rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmostart->rop.reg.fg = (unsigned char) batch->gcblit.rop;

	/* Set START_DE command. */
	gcmostart->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmostart->rect = batch->gcblit.rect;

	/* Flush PE cache. */
	gcmostart->flush.flush_ldst = gcmoflush_flush_ldst;
	gcmostart->flush.flush.reg = gcregflush_pe2D;

	/* Reset the finalizer. */
	batch->batchend = do_end;

	gc_debug_blt(batch->gcblit.srccount,
		     abs(batch->gcblit.rect.right - batch->gcblit.rect.left),
		     abs(batch->gcblit.rect.top   - batch->gcblit.rect.bottom));

exit:
	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return bverror;
}

static enum bverror do_blit(struct bvbltparams *bltparams,
				struct gcbatch *batch,
				struct srcdesc *srcdesc,
				unsigned int srccount,
				struct gcalpha *gca)
{
	enum bverror bverror = BVERR_NONE;
	enum bverror unmap_bverror;

	struct gcmosrc *gcmosrc;

	unsigned int i, index;

	struct bvbuffmap *dstmap = NULL;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct bvrect *dstrect = &bltparams->dstrect;

	struct bvformatxlate *srcformat;
	struct bvbuffmap *srcmap[2] = { NULL, NULL };
	struct bvsurfgeom *srcgeom;
	struct bvrect *srcrect;
	int srcleft, srctop;
	int srcsurfleft, srcsurftop;
	int srcshift, srcadjust, srcalign;
	int multisrc;

	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* Did destination change? */
	if (batch->dstchanged) {
		GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, GC_MOD_PREFIX
			"destination changed, applying new parameters\n",
			__func__, __LINE__);

		/* Finalize the previous blit if any. */
		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Map the destination. */
		bverror = do_map(bltparams->dstdesc, 0, &dstmap);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		/* Set the new destination. */
		bverror = set_dst(bltparams, batch, dstmap);
		if (bverror != BVERR_NONE)
			goto exit;
	}

	for (i = 0; i < srccount; i += 1) {
		srcgeom = srcdesc[i].geom;
		srcrect = srcdesc[i].rect;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"source surface %d:\n",
			__func__, __LINE__, i);

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  srcaddr[%d] = 0x%08X\n",
			__func__, __LINE__,
			i, (unsigned int) srcdesc[i].buf.desc->virtaddr);

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  srcsurf = %dx%d, stride = %ld\n",
			__func__, __LINE__,
			srcgeom->width, srcgeom->height,
			srcgeom->virtstride);

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  srcrect = (%d,%d)-(%d,%d), %dx%d\n",
			__func__, __LINE__,
			srcrect->left, srcrect->top,
			srcrect->left + srcrect->width,
			srcrect->top  + srcrect->height,
			srcrect->width, srcrect->height);

		/***************************************************************
		** Parse the source format.
		*/

		/* Parse the source format. */
		if (!parse_format(srcgeom->format, &srcformat)) {
			BVSETBLTERRORARG((srcdesc[i].index == 0)
						? BVERR_SRC1GEOM_FORMAT
						: BVERR_SRC2GEOM_FORMAT,
					"invalid source format (%d)",
					srcgeom->format);
			goto exit;
		}

		/***************************************************************
		** Determine source placement.
		*/

		srcsurfleft = srcrect->left - dstrect->left;
		srcsurftop = srcrect->top - dstrect->top;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  source surfaceorigin = %d,%d\n",
			__func__, __LINE__, srcsurfleft, srcsurftop);

		/* (srcsurfleft,srcsurftop) are the coordinates of the source
		   surface origin. PE requires 16 byte alignment of the base
		   address. Compute the alignment requirement in pixels. */
		srcalign = 16 * 8 / srcformat->bitspp;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  srcalign = %d\n",
			__func__, __LINE__, srcalign);

		/* Compute the number of pixels the origin has to be
		   aligned by. */
		srcadjust = srcsurfleft % srcalign;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  srcadjust = %d\n",
			__func__, __LINE__, srcadjust);

		multisrc = (srcadjust == 0);

		GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, GC_MOD_PREFIX
			"  multisrc = %d\n",
			__func__, __LINE__, multisrc);

		/* Adjust the origin. */
		srcsurfleft -= srcadjust;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  adjusted source surface origin = %d,%d\n",
			__func__, __LINE__, srcsurfleft, srcsurftop);

		/* Adjust the source rectangle for the single source walker. */
		srcleft = dstrect->left + batch->deltaleft + srcadjust;
		srctop = dstrect->top + batch->deltatop;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  source %d rectangle origin = %d,%d\n",
			__func__, __LINE__, i, srcleft, srctop);

		/* Compute the surface offset in bytes. */
		srcshift
			= srcsurftop * (int) srcgeom->virtstride
			+ srcsurfleft * (int) srcformat->bitspp / 8;

		GCPRINT(GCDBGFILTER, GCZONE_SURF, GC_MOD_PREFIX
			"  srcshift = 0x%08X\n",
			__func__, __LINE__, srcshift);

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

		bverror = do_map(srcdesc[i].buf.desc, 0, &srcmap[i]);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		/***************************************************************
		** Allocate command buffer.
		*/

		bverror = claim_buffer(batch, sizeof(struct gcmosrc),
					(void **) &gcmosrc);
		if (bverror != BVERR_NONE) {
			BVSETBLTERROR(BVERR_OOM,
				"failed to allocate command buffer");
			goto exit;
		}

		memset(gcmosrc, 0, sizeof(struct gcmosrc));

		GCPRINT(GCDBGFILTER, GCZONE_BUFFER, GC_MOD_PREFIX
			"allocated %d of commmand buffer\n",
			__func__, __LINE__, sizeof(struct gcmosrc));

		/***************************************************************
		** Configure source.
		*/

		/* Shortcut to the register index. */
		index = batch->gcblit.srccount;

		add_fixup(batch, &gcmosrc->address, srcshift);

		/* Set surface parameters. */
		gcmosrc->address_ldst = gcmosrc_address_ldst[index];
		gcmosrc->address = GET_MAP_HANDLE(srcmap[i]);

		gcmosrc->stride_ldst = gcmosrc_stride_ldst[index];
		gcmosrc->stride = srcgeom->virtstride;

		gcmosrc->rotation_ldst = gcmosrc_rotation_ldst[index];
		gcmosrc->rotation.reg.surf_width
			= dstrect->left + srcgeom->width;

		gcmosrc->config_ldst = gcmosrc_config_ldst[index];
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
		gcmosrc->rop.reg.type = GCREG_ROP_TYPE_ROP3;
		gcmosrc->rop.reg.fg = (unsigned char) batch->gcblit.rop;

		gcmosrc->mult_ldst = gcmosrc_mult_ldst[index];
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

		gcmosrc->alphacontrol_ldst = gcmosrc_alphacontrol_ldst[index];
		gcmosrc->alphamodes_ldst = gcmosrc_alphamodes_ldst[index];
		gcmosrc->srcglobal_ldst = gcmosrc_srcglobal_ldst[index];
		gcmosrc->dstglobal_ldst = gcmosrc_dstglobal_ldst[index];

		if ((gca != NULL) && ((srccount == 1) || (i > 0))) {
			gcmosrc->alphacontrol.reg.enable
				= GCREG_ALPHA_CONTROL_ENABLE_ON;

			gcmosrc->alphamodes.reg.src_global_alpha
				= gca->src_global_alpha_mode;
			gcmosrc->alphamodes.reg.dst_global_alpha
				= gca->dst_global_alpha_mode;

			if (srccount == 1) {
				GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
					"k1 sets src blend.\n",
					__func__, __LINE__);
				GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
					"k2 sets dst blend.\n",
					__func__, __LINE__);

				gcmosrc->alphamodes.reg.src_blend
					= gca->k1->factor_mode;
				gcmosrc->alphamodes.reg.src_color_reverse
					= gca->k1->color_reverse;

				gcmosrc->alphamodes.reg.dst_blend
					= gca->k2->factor_mode;
				gcmosrc->alphamodes.reg.dst_color_reverse
					= gca->k2->color_reverse;
			} else {
				GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
					"k1 sets dst blend.\n",
					__func__, __LINE__);
				GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
					"k2 sets src blend.\n",
					__func__, __LINE__);

				gcmosrc->alphamodes.reg.src_blend
					= gca->k2->factor_mode;
				gcmosrc->alphamodes.reg.src_color_reverse
					= gca->k2->color_reverse;

				gcmosrc->alphamodes.reg.dst_blend
					= gca->k1->factor_mode;
				gcmosrc->alphamodes.reg.dst_color_reverse
					= gca->k1->color_reverse;
			}

			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"dst blend:\n",
				__func__, __LINE__);
			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"  factor = %d\n",
				__func__, __LINE__,
				gcmosrc->alphamodes.reg.dst_blend);
			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"  inverse = %d\n",
				__func__, __LINE__,
				gcmosrc->alphamodes.reg.dst_color_reverse);

			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"src blend:\n",
				__func__, __LINE__);
			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"  factor = %d\n",
				__func__, __LINE__,
				gcmosrc->alphamodes.reg.src_blend);
			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"  inverse = %d\n",
				__func__, __LINE__,
				gcmosrc->alphamodes.reg.src_color_reverse);

			gcmosrc->srcglobal.raw = gca->src_global_color;
			gcmosrc->dstglobal.raw = gca->dst_global_color;
		} else {
			GCPRINT(GCDBGFILTER, GCZONE_BLEND, GC_MOD_PREFIX
				"blending disabled.\n",
				__func__, __LINE__);

			gcmosrc->alphacontrol.reg.enable
				= GCREG_ALPHA_CONTROL_ENABLE_OFF;
		}

		batch->gcblit.srccount += 1;
	}

exit:
	for (i = 0; i < 2; i += 1)
		if (srcmap[i] != NULL) {
			unmap_bverror = schedule_unmap(batch,
							srcdesc[i].buf.desc);
			if ((unmap_bverror != BVERR_NONE) &&
				(bverror == BVERR_NONE)) {
				bltparams->errdesc = gccontext.bverrorstr;
				bverror = unmap_bverror;
			}
		}

	if (dstmap != NULL) {
		unmap_bverror = schedule_unmap(batch, bltparams->dstdesc);
		if ((unmap_bverror != BVERR_NONE) && (bverror == BVERR_NONE)) {
			bltparams->errdesc = gccontext.bverrorstr;
			bverror = unmap_bverror;
		}
	}

	GCPRINT(GCDBGFILTER, GCZONE_DO_BLIT, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
	return bverror;
}

static enum bverror do_filter(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	GCPRINT(GCDBGFILTER, GCZONE_DO_FILTER, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	BVSETBLTERROR(BVERR_SRC1_HORZSCALE, "scaling not supported");

	GCPRINT(GCDBGFILTER, GCZONE_DO_FILTER, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);
	return bverror;
}

/*******************************************************************************
 * Library constructor and destructor.
 */

void bv_init(void)
{
}

void bv_exit(void)
{
	struct bvbuffmap *bufmap;
	struct gcschedunmap *bufunmap;
	struct gcbuffer *buffer;
	struct gcfixup *fixup;
	struct gcvacbatch *batch;

	while (gccontext.vac_buffmap != NULL) {
		bufmap = gccontext.vac_buffmap;
		gccontext.vac_buffmap = bufmap->nextmap;
		gcfree(bufmap);
	}

	while (gccontext.vac_unmap != NULL) {
		bufunmap = gccontext.vac_unmap;
		gccontext.vac_unmap = bufunmap->next;
		gcfree(bufunmap);
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

enum bverror gcbv_map(struct bvbuffdesc *buffdesc)
{
	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* FIXME/TODO: add check for initialization success. */

	if (buffdesc == NULL) {
		BVSETERROR(BVERR_BUFFERDESC, "bvbuffdesc is NULL");
		goto exit;
	}

	if (buffdesc->structsize < STRUCTSIZE(buffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	bverror = do_map(buffdesc, 1, &bvbuffmap);
	if (bverror == BVERR_NONE) {
		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"virtaddr = 0x%08X\n",
			__func__, __LINE__,
			(unsigned int) buffdesc->virtaddr);

		GCPRINT(GCDBGFILTER, GCZONE_MAPPING, GC_MOD_PREFIX
			"addr = 0x%08X\n",
			__func__, __LINE__,
			GET_MAP_HANDLE(bvbuffmap));
	}

exit:
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return bverror;
}
EXPORT_SYMBOL(gcbv_map);

enum bverror gcbv_unmap(struct bvbuffdesc *buffdesc)
{
	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;

	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "++" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	/* FIXME/TODO: add check for initialization success. */

	if (buffdesc == NULL) {
		BVSETERROR(BVERR_BUFFERDESC, "bvbuffdesc is NULL");
		goto exit;
	}

	if (buffdesc->structsize < STRUCTSIZE(buffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	bvbuffmap = buffdesc->map;
	if (bvbuffmap == NULL)
		return BVERR_NONE;

	if (bvbuffmap->structsize < STRUCTSIZE(bvbuffmap, nextmap)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS,
			"unsupported bvbuffdesc version (structsize)");
		goto exit;
	}

	/* Not our mapping? */
	if (bvbuffmap->bv_unmap != gcbv_unmap) {
		bverror = bvbuffmap->bv_unmap(buffdesc);
		goto exit;
	}

	/* Get the info structure. */
	bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;

	/* Are there any existing auto-mappings? */
	if (bvbuffmapinfo->automap) {
		/* Reset user mappings if any. */
		bvbuffmapinfo->usermap = 0;

		/* Are there mappings from alternative implementations? */
		if (bvbuffmap->nextmap != NULL) {
			/* Temporarily remove the record from the mapping
			   list so that other implementations can proceeed. */
			buffdesc->map = bvbuffmap->nextmap;

			/* Call other implementations. */
			bverror = gcbv_unmap(buffdesc);

			/* Link the record back into the list. */
			bvbuffmap->nextmap = buffdesc->map;
			buffdesc->map = bvbuffmap;
		} else
			bverror = BVERR_NONE;

	/* No auto-mappings, must be user mapping. */
	} else {
		/* Unmap the buffer. */
		bverror = do_unmap(buffdesc, 1);
		if (bverror != BVERR_NONE)
			goto exit;

		/* Call other implementations. */
		bverror = gcbv_unmap(buffdesc);
	}

exit:
	GCPRINT(GCDBGFILTER, GCZONE_MAPPING, "--" GC_MOD_PREFIX
		"\n", __func__, __LINE__);

	return bverror;
}
EXPORT_SYMBOL(gcbv_unmap);

enum bverror gcbv_blt(struct bvbltparams *bltparams)
{
	enum bverror bverror = BVERR_NONE;
	struct gcalpha *gca = NULL;
	struct gcalpha _gca;
	unsigned int op, type;
	unsigned int batchflags;
	unsigned int batchexec = 0;
	bool nop = false;
	struct gcbatch *batch;
	int src1used, src2used, maskused;
	struct srcdesc srcdesc[2];
	unsigned short rop, blend, format;
	struct gccommit gccommit;
	int srccount, res;

	GCPRINT(GCDBGFILTER, GCZONE_BLIT, "++" GC_MOD_PREFIX
		"bltparams = 0x%08X\n",
		__func__, __LINE__, (unsigned int) bltparams);

	/* FIXME/TODO: add check for initialization success. */

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
	res = verify_surface(0, (union bvinbuff *) &bltparams->dstdesc,
				bltparams->dstgeom, &bltparams->dstrect);
	if (res != -1) {
		BVSETBLTSURFERROR(res, g_destsurferr);
		goto exit;
	}

	/* Extract the operation flags. */
	op = (bltparams->flags & BVFLAG_OP_MASK) >> BVFLAG_OP_SHIFT;
	type = (bltparams->flags & BVFLAG_BATCH_MASK) >> BVFLAG_BATCH_SHIFT;

	switch (type) {
	case (BVFLAG_BATCH_NONE >> BVFLAG_BATCH_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
			"BVFLAG_BATCH_NONE\n",
			__func__, __LINE__);
		bverror = allocate_batch(&batch);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		batchexec = 1;
		batchflags = 0x7FFFFFFF;
		break;

	case (BVFLAG_BATCH_BEGIN >> BVFLAG_BATCH_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
			"BVFLAG_BATCH_BEGIN\n",
			__func__, __LINE__);
		bverror = allocate_batch(&batch);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}

		bltparams->batch = (struct bvbatch *) batch;

		batchexec = 0;
		bltparams->batchflags =
		batchflags = 0x7FFFFFFF;
		break;

	case (BVFLAG_BATCH_CONTINUE >> BVFLAG_BATCH_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
			"BVFLAG_BATCH_CONTINUE\n",
			__func__, __LINE__);
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
		batchflags = bltparams->batchflags;
		break;

	case (BVFLAG_BATCH_END >> BVFLAG_BATCH_SHIFT):
		GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
			"BVFLAG_BATCH_END\n",
			__func__, __LINE__);
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
		batchflags = bltparams->batchflags;
		break;

	default:
		BVSETBLTERROR(BVERR_BATCH, "unrecognized batch type");
		goto exit;
	}

	GCPRINT(GCDBGFILTER, GCZONE_BATCH, GC_MOD_PREFIX
		"batchflags=0x%08X\n",
		__func__, __LINE__, batchflags);

	if (!nop) {
		/* Determine whether the destination has changed. */
		batch->dstchanged = batchflags
			& (BVBATCH_DST
			| BVBATCH_DSTRECT_ORIGIN
			| BVBATCH_DSTRECT_SIZE
			| BVBATCH_CLIPRECT_ORIGIN
			| BVBATCH_CLIPRECT_SIZE);

		GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
			"dstchanged=%d\n",
			__func__, __LINE__, (batch->dstchanged != 0));

		/* Verify the batch change flags. */
		VERIFYBATCH(batchflags >> 12, &g_prevdstrect,
				&bltparams->dstrect);

		switch (op) {
		case (BVFLAG_ROP >> BVFLAG_OP_SHIFT):
			GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
				"BVFLAG_ROP\n",
				__func__, __LINE__);
			rop = bltparams->op.rop;
			src1used = ((rop & 0xCCCC) >> 2)
					^  (rop & 0x3333);
			src2used = ((rop & 0xF0F0) >> 4)
					^  (rop & 0x0F0F);
			maskused = ((rop & 0xFF00) >> 8)
					^  (rop & 0x00FF);
			break;

		case (BVFLAG_BLEND >> BVFLAG_OP_SHIFT):
			GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
				"BVFLAG_BLEND\n",
				__func__, __LINE__);
			blend = bltparams->op.blend;
			format = (blend & BVBLENDDEF_FORMAT_MASK)
				>> BVBLENDDEF_FORMAT_SHIFT;

			bverror = parse_blend(bltparams,
						bltparams->op.blend, &_gca);
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
			GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
				"BVFLAG_FILTER\n",
				__func__, __LINE__);
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
			GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
				"src1used\n",
				__func__, __LINE__);

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_SRC1,
				&bltparams->src1, bltparams->src1geom,
				&bltparams->src1rect);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src1surferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			VERIFYBATCH(batchflags >> 14, &batch->prevsrc1rect,
					&bltparams->src1rect);

			/* Same as the destination? */
			if ((bltparams->src1.desc
				== bltparams->dstdesc) &&
				(bltparams->src1geom
				== bltparams->dstgeom) &&
				EQ_ORIGIN(bltparams->src1rect,
						bltparams->dstrect) &&
				EQ_SIZE(bltparams->src1rect,
						bltparams->dstrect)) {
				GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
					"src1 is the same as dst\n",
					__func__, __LINE__);
			} else {
				srcdesc[srccount].index = 0;
				srcdesc[srccount].buf = bltparams->src1;
				srcdesc[srccount].geom = bltparams->src1geom;
				srcdesc[srccount].rect = &bltparams->src1rect;

				srccount += 1;
			}
		}

		/* Verify the src2 parameters structure. */
		if (src2used) {
			GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
				"src2used\n",
				__func__, __LINE__);

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_SRC2,
				&bltparams->src2, bltparams->src2geom,
				&bltparams->src2rect);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src2surferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			VERIFYBATCH(batchflags >> 16, &batch->prevsrc2rect,
					&bltparams->src2rect);

			/* Same as the destination? */
			if ((bltparams->src2.desc
				== bltparams->dstdesc) &&
				(bltparams->src2geom
				== bltparams->dstgeom) &&
				EQ_ORIGIN(bltparams->src2rect,
						bltparams->dstrect) &&
				EQ_SIZE(bltparams->src2rect,
						bltparams->dstrect)) {
				GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
					"src2 is the same as dst\n",
					__func__, __LINE__);
			} else {
				srcdesc[srccount].index = 1;
				srcdesc[srccount].buf = bltparams->src2;
				srcdesc[srccount].geom = bltparams->src2geom;
				srcdesc[srccount].rect = &bltparams->src2rect;

				srccount += 1;
			}
		}

		/* Verify the mask parameters structure. */
		if (maskused) {
			GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
				"maskused\n",
				__func__, __LINE__);
			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_MASK,
				&bltparams->mask, bltparams->maskgeom,
				&bltparams->maskrect);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_masksurferr);
				goto exit;
			}

			/* Verify the batch change flags. */
			VERIFYBATCH(batchflags >> 18, &batch->prevmaskrect,
					&bltparams->maskrect);

			BVSETERROR(BVERR_OP,
				"operation with mask not supported");
			goto exit;
		}

		GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
			"srccount = %d\n",
			__func__, __LINE__, srccount);

		switch (srccount) {
		case 0:
			bverror = do_blit(bltparams, batch, NULL, 0, gca);
			break;

		case 1:
			if (EQ_SIZE((*srcdesc[0].rect), bltparams->dstrect))
				bverror = do_blit(bltparams, batch,
						srcdesc, 1, gca);
			else if ((srcdesc[0].rect->width == 1) &&
				(srcdesc[0].rect->height == 1))
				bverror = do_fill(bltparams, batch, srcdesc);
			else
				bverror = do_filter(bltparams, batch);
			break;

		case 2:
			if (EQ_SIZE((*srcdesc[0].rect), bltparams->dstrect))
				if (EQ_SIZE((*srcdesc[1].rect),
					bltparams->dstrect))
					bverror = do_blit(bltparams, batch,
							srcdesc, 2, gca);
				else
					BVSETBLTERROR(BVERR_SRC2_HORZSCALE,
						"scaling not supported");
			else
				if (EQ_SIZE((*srcdesc[1].rect),
					bltparams->dstrect))
					BVSETBLTERROR(BVERR_SRC1_HORZSCALE,
						"scaling not supported");
				else
					BVSETBLTERROR(BVERR_SRC1_HORZSCALE,
						"scaling not supported");
		}
	}

	if (batchexec) {
		/* Finalize the current operation. */
		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		GCDUMPBATCH(batch);

		gccommit.gcerror = GCERR_NONE;
		gccommit.buffer = batch->bufhead;

		gc_commit_wrapper(&gccommit);
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

		GCPRINT(GCDBGFILTER, GCZONE_BLIT, GC_MOD_PREFIX
			"batch is submitted.\n",
			__func__, __LINE__);
	}

exit:
	if ((batch != NULL) && batchexec) {
		process_scheduled_unmap(batch);
		free_batch(batch);
		bltparams->batch = NULL;
	}

	GCPRINT(GCDBGFILTER, GCZONE_BLIT, "--" GC_MOD_PREFIX
		"bverror = %d\n", __func__, __LINE__, bverror);

	return bverror;
}
EXPORT_SYMBOL(gcbv_blt);
