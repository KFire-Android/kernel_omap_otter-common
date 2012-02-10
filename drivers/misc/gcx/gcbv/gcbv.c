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

#include <linux/gcx.h>
#include <linux/gcioctl.h>
#include <linux/gcbv.h>
#include "gcmain.h"

#ifndef GC_DUMP
#	define GC_DUMP 0
#endif

#if GC_DUMP
#	define GC_PRINT gcdump
#else
#	define GC_PRINT(...)
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

#if 0
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
	struct gccmdstartderect dstrect;
};

/* Batch header. */
struct gcbatch {
	unsigned int structsize;	/* Used to ID structure version. */

	gcbatchend batchend;		/* Pointer to the function to finilize
					   the current operation. */
	union {
		struct gcblit gcblit;
	} op;				/* States of the current operation. */

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

#if GC_DUMP
static void dumpbuffer(struct gcbatch *batch)
{
	unsigned int bufcount = 0;
	struct gcbuffer *buffer;
	unsigned int datacount, i, j, cmd;
	struct gcfixup *fixup;

	GC_PRINT(GC_INFO_MSG " BATCH DUMP (0x%08X)\n",
		__func__, __LINE__, (unsigned int) batch);

	buffer = batch->bufhead;
	while (buffer != NULL) {
		GC_PRINT(GC_INFO_MSG "   Command buffer #%d (0x%08X)\n",
			__func__, __LINE__, ++bufcount, (unsigned int) buffer);

		fixup = buffer->fixuphead;
		while (fixup != NULL) {
			GC_PRINT(GC_INFO_MSG
				"   Fixup table @ 0x%08X, count = %d:\n",
				__func__, __LINE__,
				(unsigned int) fixup, fixup->count);

			for (i = 0; i < fixup->count; i += 1) {
				GC_PRINT(GC_INFO_MSG
					"   [%02d] buffer offset = 0x%08X, "
					"surface offset = 0x%08X\n",
					__func__, __LINE__,
					i,
					fixup->fixup[i].dataoffset * 4,
					fixup->fixup[i].surfoffset);
			}

			fixup = fixup->next;
		}

		datacount = buffer->tail - buffer->head;
		for (i = 0; i < datacount;) {
			cmd = GETFIELD(buffer->head[i],
					GCREG_COMMAND_LOAD_STATE,
					COMMAND_OPCODE);

			if (cmd == REGVALUE(GCREG_COMMAND_LOAD_STATE,
						COMMAND_OPCODE, LOAD_STATE)) {
				unsigned int count, addr;

				count = GETFIELD(buffer->head[i],
					GCREG_COMMAND_LOAD_STATE,
					COMMAND_COUNT);

				addr = GETFIELD(buffer->head[i],
					GCREG_COMMAND_LOAD_STATE,
					COMMAND_ADDRESS);

				GC_PRINT(GC_INFO_MSG
					"     0x%08X: 0x%08X  STATE(0x%04X, %d)\n",
					__func__, __LINE__,
					(i << 2), buffer->head[i], addr, count);
				i += 1;

				count |= 1;
				for (j = 0; j < count; i += 1, j += 1) {
					GC_PRINT(GC_INFO_MSG " %16c0x%08X\n",
						__func__, __LINE__,
						' ', buffer->head[i]);
				}
			} else if (cmd == REGVALUE(GCREG_COMMAND_START_DE,
						COMMAND_OPCODE, START_DE)) {
				unsigned int count;
				unsigned int x1, y1, x2, y2;

				count = GETFIELD(buffer->head[i],
					GCREG_COMMAND_START_DE,
					COMMAND_COUNT);

				GC_PRINT(GC_INFO_MSG
					"     0x%08X: 0x%08X  STARTDE(%d)\n",
					__func__, __LINE__,
					(i << 2), buffer->head[i], count);
				i += 1;

				GC_PRINT(GC_INFO_MSG " %16c0x%08X\n",
					__func__, __LINE__,
					' ', buffer->head[i]);
				i += 1;

				for (j = 0; j < count; j += 1) {
					x1 = GETFIELD(buffer->head[i],
						GCREG_COMMAND_TOP_LEFT, X);
					y1 = GETFIELD(buffer->head[i],
						GCREG_COMMAND_TOP_LEFT, Y);

					GC_PRINT(GC_INFO_MSG
						" %16c0x%08X  LT(%d,%d)\n",
						__func__, __LINE__,
						' ', buffer->head[i], x1, y1);

					i += 1;

					x2 = GETFIELD(buffer->head[i],
						GCREG_COMMAND_BOTTOM_RIGHT, X);
					y2 = GETFIELD(buffer->head[i],
						GCREG_COMMAND_BOTTOM_RIGHT, Y);

					GC_PRINT(GC_INFO_MSG
						" %16c0x%08X  RB(%d,%d)\n",
						__func__, __LINE__,
						' ', buffer->head[i], x2, y2);

					i += 1;
				}
			} else {
				GC_PRINT(GC_INFO_MSG
					" unsupported command: %d\n",
					__func__, __LINE__, cmd);
			}
		}

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
	GC_PRINT(GC_ERR_MSG " %s\n", __func__, __LINE__, \
		gccontext.bverrorstr); \
} while (0)

#define SETERRORARG(message, arg) \
do { \
	snprintf(gccontext.bverrorstr, sizeof(gccontext.bverrorstr), \
		"%s(%d): " message, __func__, __LINE__, arg); \
	GC_PRINT(GC_ERR_MSG " %s\n", __func__, __LINE__, \
		gccontext.bverrorstr); \
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
	GC_PRINT(GC_ERR_MSG " %s\n", __func__, __LINE__, \
		gccontext.bverrorstr); \
	bverror = errordesc.base + g_surferr[errorid].offset; \
} while (0)

#define BVSETBLTSURFERROR(errorid, errordesc) \
do { \
	BVSETSURFERROR(errorid, errordesc); \
	bltparams->errdesc = gccontext.bverrorstr; \
} while (0)

#define BVERR_DESC		0
#define BVERR_DESC_VERS		1
#define BVERR_DESC_VIRTADDR	2
#define BVERR_TILE		3
#define BVERR_TILE_VERS		4
#define BVERR_TILE_VIRTADDR	5
#define BVERR_GEOM		6
#define BVERR_GEOM_VERS		7
#define BVERR_GEOM_FORMAT	8

struct bvsurferrorid {
	char *id;
	enum bverror base;
};

struct bvsurferror {
	unsigned int offset;
	char *message;
};

static struct bvsurferror g_surferr[] = {
	/* BVERR_DESC */
	{    0, "%s(%d): %s desc structure is not set" },

	/* BVERR_DESC_VERS */
	{  100, "%s(%d): %s desc structure has invalid size" },

	/* BVERR_DESC_VIRTADDR */
	{  200, "%s(%d): %s desc virtual pointer is not set" },

	/* BVERR_TILE: FIXME/TODO define error code */
	{    0, "%s(%d): %s tileparams structure is not set" },

	/* BVERR_TILE_VERS */
	{ 3000, "%s(%d): %s tileparams structure has invalid size" },

	/* BVERR_TILE_VIRTADDR: FIXME/TODO define error code */
	{  200, "%s(%d): %s tileparams virtual pointer is not set" },

	/* BVERR_GEOM */
	{ 1000, "%s(%d): %s geom structure is not set" },

	/* BVERR_GEOM_VERS */
	{ 1100, "%s(%d): %s geom structure has invalid size" },

	/* BVERR_GEOM_FORMAT */
	{ 1200, "%s(%d): %s invalid format specified" },
};

static struct bvsurferrorid g_destsurferr = { "dst",  BVERR_DSTDESC };
static struct bvsurferrorid g_src1surferr = { "src1", BVERR_SRC1DESC };
static struct bvsurferrorid g_src2surferr = { "src2", BVERR_SRC2DESC };
static struct bvsurferrorid g_masksurferr = { "mask", BVERR_MASKDESC };

/*******************************************************************************
 * Threads etc... TBD
 */

#if 0
#define MAX_THRD 1
static pthread_t id[MAX_THRD];

/* TODO: check that we aren't using more logic than we need */
static void sig_handler(int sig)
{
	GC_PRINT(GC_INFO_MSG " %s(%d): %lx:%d\n", __func__, __LINE__,
		(unsigned long) pthread_self(), sig);
	sleep(2); sched_yield();
}

/* TODO: check that we aren't using more logic than we need */
static void *thread(void *p)
{
	int r = 0;
	sigset_t set;
	struct sigaction sa;

	sigemptyset(&set);
	sa.sa_handler = sig_handler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);

	while (1) {
		GC_PRINT(GC_INFO_MSG " %s(%d)\n", __func__, __LINE__);
		sigwait(&set, &r);
		GC_PRINT(GC_INFO_MSG " %s(%d)\n", __func__, __LINE__);
	}

	return NULL;
}
#endif

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
	gcmap.pagecount = buffdesc->pagecount;
	gcmap.pagearray = buffdesc->pagearray;

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
	GC_PRINT(GC_INFO_MSG " dummy operation end\n", __func__, __LINE__);
	return BVERR_NONE;
}

static enum bverror allocate_batch(struct gcbatch **batch)
{
	enum bverror bverror;
	struct gcbatch *temp;

	if (gccontext.vac_batches == NULL) {
		temp = gcalloc(struct gcbatch, sizeof(struct gcbatch));
		if (temp == NULL) {
			BVSETERROR(BVERR_OOM,
					"batch header allocation failed");
			goto exit;
		}
	} else {
		temp = (struct gcbatch *) gccontext.vac_batches;
		gccontext.vac_batches = gccontext.vac_batches->next;
	}

	memset(temp, 0, sizeof(struct gcbatch));
	temp->structsize = sizeof(struct gcbatch);
	temp->batchend = do_end;

	bverror = append_buffer(temp);
	if (bverror != BVERR_NONE) {
		free_batch(temp);
		goto exit;
	}

	*batch = temp;

exit:
	return bverror;
}

static void free_batch(struct gcbatch *batch)
{
	struct gcbuffer *buffer;

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
}

static enum bverror append_buffer(struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcbuffer *temp;

	GC_PRINT(GC_INFO_MSG " batch = 0x%08X\n",
		__func__, __LINE__, (unsigned int) batch);

	if (gccontext.vac_buffers == NULL) {
		temp = gcalloc(struct gcbuffer, GC_BUFFER_SIZE);
		if (temp == NULL) {
			BVSETERROR(BVERR_OOM,
					"command buffer allocation failed");
			goto exit;
		}
	} else {
		temp = gccontext.vac_buffers;
		gccontext.vac_buffers = temp->next;
	}

	memset(temp, 0, sizeof(struct gcbuffer));
	temp->head =
	temp->tail = (unsigned int *) (temp + 1);
	temp->available = GC_BUFFER_SIZE - sizeof(struct gcbuffer);

	if (batch->bufhead == NULL)
		batch->bufhead = temp;
	else
		batch->buftail->next = temp;
	batch->buftail = temp;

	GC_PRINT(GC_INFO_MSG " new buffer appended = 0x%08X\n",
		__func__, __LINE__, (unsigned int) temp);

	bverror = BVERR_NONE;

exit:
	return bverror;
}

static enum bverror add_fixup(struct gcbatch *batch, unsigned int *fixup,
				unsigned int surfoffset)
{
	enum bverror bverror;
	struct gcbuffer *buffer;
	struct gcfixup *temp;

	GC_PRINT(GC_INFO_MSG " batch = 0x%08X, fixup ptr = 0x%08X\n",
		__func__, __LINE__, (unsigned int) batch, (unsigned int) fixup);

	buffer = batch->buftail;
	temp = buffer->fixuptail;

	GC_PRINT(GC_INFO_MSG " buffer = 0x%08X, fixup struct = 0x%08X\n",
		__func__, __LINE__, (unsigned int) buffer, (unsigned int) temp);

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

		GC_PRINT(GC_INFO_MSG " new fixup struct allocated = 0x%08X\n",
			__func__, __LINE__, (unsigned int) temp);

	} else {
		GC_PRINT(GC_INFO_MSG " fixups accumulated = %d\n",
			__func__, __LINE__, temp->count);
	}

	temp->fixup[temp->count].dataoffset = fixup - buffer->head;
	temp->fixup[temp->count].surfoffset = surfoffset;
	temp->count += 1;

	GC_PRINT(GC_INFO_MSG " fixup offset = 0x%08X\n",
		__func__, __LINE__, fixup - buffer->head);
	GC_PRINT(GC_INFO_MSG " surface offset = 0x%08X\n",
		__func__, __LINE__, surfoffset);

	bverror = BVERR_NONE;

exit:
	return bverror;
}

static enum bverror claim_buffer(struct gcbatch *batch,
					unsigned int size,
					void **buffer)
{
	enum bverror bverror;
	struct gcbuffer *curbuf;

	/* Get the current command buffer. */
	curbuf = batch->buftail;

	GC_PRINT(GC_INFO_MSG " batch = 0x%08X, buffer = 0x%08X\n",
		__func__, __LINE__,
		(unsigned int) batch, (unsigned int) curbuf);

	GC_PRINT(GC_INFO_MSG " available = %d, requested = %d\n",
		__func__, __LINE__, curbuf->available, size);

	if (curbuf->available < size) {
		bverror = append_buffer(batch);
		if (bverror != BVERR_NONE)
			goto exit;

		curbuf = batch->buftail;
	}

	*buffer = curbuf->tail;
	curbuf->tail = (unsigned int *) ((unsigned char *) curbuf->tail + size);
	curbuf->available -= size;
	batch->size += size;
	bverror = BVERR_NONE;

exit:
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
		BVRED(0, 8), BVGREEN(8, 8), BVBLUE(16, 8), BVALPHA(24, 0)),
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

	GC_PRINT(GC_INFO_MSG " ocdformat = 0x%08X\n",
		__func__, __LINE__, ocdformat);

	cs = (ocdformat & OCDFMTDEF_CS_MASK) >> OCDFMTDEF_CS_SHIFT;
	bits = (ocdformat & OCDFMTDEF_COMPONENTSIZEMINUS1_MASK)
		>> OCDFMTDEF_COMPONENTSIZEMINUS1_SHIFT;
	cont = (ocdformat & OCDFMTDEF_CONTAINER_MASK)
		>> OCDFMTDEF_CONTAINER_SHIFT;

	switch (cs) {
	case (OCDFMTDEF_CS_RGB >> OCDFMTDEF_CS_SHIFT):
		GC_PRINT(GC_INFO_MSG " OCDFMTDEF_CS_RGB\n",
			__func__, __LINE__);

		GC_PRINT(GC_INFO_MSG " bits = %d\n",
			__func__, __LINE__, bits);

		GC_PRINT(GC_INFO_MSG " cont = %d\n",
			__func__, __LINE__, cont);

		if ((ocdformat & OCDFMTDEF_LAYOUT_MASK) != OCDFMTDEF_PACKED)
			return 0;

		swizzle = (ocdformat & OCDFMTDEF_PLACEMENT_MASK)
			>> OCDFMTDEF_PLACEMENT_SHIFT;
		alpha = (ocdformat & OCDFMTDEF_ALPHA_MASK)
			>> OCDFMTDEF_ALPHA_SHIFT;

		GC_PRINT(GC_INFO_MSG " swizzle = %d\n",
			__func__, __LINE__, swizzle);

		GC_PRINT(GC_INFO_MSG " alpha = %d\n",
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

		GC_PRINT(GC_INFO_MSG " index = %d\n",
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

	switch (desc->size) {
	case 0:
		component8 = 0xFF;
		break;

	case 1:
		component8 = component ? 0xFF : 0x00;
		break;

	case 4:
		component8 = component | (component << 4);
		break;

	case 5:
		component8 = (component << 3) | (component >> 2);
		break;

	case 6:
		component8 = (component << 2) | (component >> 4);
		break;

	default:
		component8 = component;
	}

	return component8;
}

static unsigned int getinternalcolor(void *ptr, struct bvformatxlate *format)
{
	unsigned int pixel;
	unsigned int r, g, b, a;

	switch (format->bitspp) {
	case 16:
		pixel = *(unsigned short *) ptr;
		break;

	case 32:
		pixel = *(unsigned int *) ptr;
		break;

	default:
		pixel = 0;
	}

	r = extract_component(pixel, &format->rgba.r);
	g = extract_component(pixel, &format->rgba.g);
	b = extract_component(pixel, &format->rgba.b);
	a = extract_component(pixel, &format->rgba.a);

	return	(a << 24) |
		(r << 16) |
		(g <<  8) |
		 b;
}

/*******************************************************************************
 * Alpha blending parser.
 */

struct gcblendconfig {
	unsigned char factor_mode;
	unsigned char color_reverse;

	unsigned char destuse;
	unsigned char srcuse;
};

struct gcalpha {
	unsigned int src_global_color;
	unsigned int dst_global_color;

	unsigned char src_global_alpha_mode;
	unsigned char dst_global_alpha_mode;

	struct gcblendconfig *src_config;
	struct gcblendconfig *dst_config;
};

struct bvblendxlate {
	unsigned char match1;
	unsigned char match2;

	struct gcblendconfig dst;
	struct gcblendconfig src;
};

#define BVBLENDMATCH(Mode, Inverse, Normal) \
( \
	BVBLENDDEF_ ## Mode | \
	BVBLENDDEF_ ## Inverse | \
	BVBLENDDEF_ ## Normal \
)

#define BVDEST(Use) \
	Use

#define BVSRC(Use) \
	Use

#define BVBLENDUNDEFINED() \
	{ ~0, ~0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } }

static struct bvblendxlate blendxlate[64] = {
	/**********************************************************************/
	/* color factor: 00 00 00 A:(1-Cd,Cd)=zero
	   alpha factor: zero ==> 00 00 00 */
	{
		0x00,
		0x00,

		{
			/* k1 * Cd = 0 * Cd
			   k3 * Ad = 0 * Ad */
			GCREG_BLENDING_MODE_ZERO,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(0), BVSRC(0),
		},

		{
			/* k2 * Cs = 0 * Cs
			   k4 * As = 0 * As */
			GCREG_BLENDING_MODE_ZERO,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(0), BVSRC(0)
		}
	},

	/* color factor: 00 00 01 A:(1-Cd,Ad)=Ad
	   alpha factor: Ad ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			/* k1 * Cd = Ad * Cd
			   k3 * Ad = Ad * Ad */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = Ad * Cs
			   k4 * As = Ad * As */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 00 00 10 A:(1-Cd,Cs)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 00 00 11 A:(1-Cd,As)=As
	   alpha factor: As ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			/* k1 * Cd = As * Cd
			   k3 * Ad = As * Ad */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = As * Cs
			   k4 * As = As * As */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 00 01 00 A:(1-Ad,Cd)=1-Ad
	   alpha factor: 1-Ad ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			/* k1 * Cd = (1 - Ad) * Cd
			   k3 * Ad = (1 - Ad) * Ad */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = (1 - Ad) * Cs
			   k4 * As = (1 - Ad) * As */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 00 01 01 A:(1-Ad,Ad)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 00 01 10 A:(1-Ad,Cs)=1-Ad
	   alpha factor: 1-Ad ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			/* k1 * Cd = (1 - Ad) * Cd
			   k3 * Ad = (1 - Ad) * Ad */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = (1 - Ad) * Cs
			   k4 * As = (1 - Ad) * As */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 00 01 11 A:(1-Ad,As)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 00 10 00 A:(1-Cs,Cd)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 00 10 01 A:(1-Cs,Ad)=Ad
	   alpha factor: Ad ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			/* k1 * Cd = Ad * Cd
			   k3 * Ad = Ad * Ad */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = Ad * Cs
			   k4 * As = Ad * As */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 00 10 10 A:(1-Cs,Cs)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 00 10 11 A:(1-Cs,As)=As
	   alpha factor: As ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			/* k1 * Cd = As * Cd
			   k3 * Ad = As * Ad */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = As * Cs
			   k4 * As = As * As */
			GCREG_BLENDING_MODE_NORMAL,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 00 11 00 A:(1-As,Cd)=1-As
	   alpha factor: 1-As ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			/* k1 * Cd = (1 - As) * Cd
			   k3 * Ad = (1 - As) * Ad */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = (1 - As) * Cs
			   k4 * As = (1 - As) * As */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 00 11 01 A:(1-As,Ad)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 00 11 10 A:(1-As,Cs)=1-As
	   alpha factor: 1-As ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			/* k1 * Cd = (1 - As) * Cd
			   k3 * Ad = (1 - As) * Ad */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = (1 - As) * Cs
			   k4 * As = (1 - As) * As */
			GCREG_BLENDING_MODE_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 00 11 11 A:(1-As,As)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* color factor: 01 00 00 MIN:(1-Cd,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 00 01 MIN:(1-Cd,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 00 10 MIN:(1-Cd,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 00 11 MIN:(1-Cd,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 01 00 MIN:(1-Ad,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 01 01 MIN:(1-Ad,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 01 10 MIN:(1-Ad,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 01 11 MIN:(1-Ad,As)
	   alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			/* k1 * Cd = MIN:(1 - Ad, As) * Cd
			   k3 * Ad = 1 * Ad */
			GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = MIN:(1 - Ad, As) * Cs
			   k4 * As = 1 * As */
			GCREG_BLENDING_MODE_SATURATED_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 01 10 00 MIN:(1-Cs,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 10 01 MIN:(1-Cs,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 10 10 MIN:(1-Cs,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 10 11 MIN:(1-Cs,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 11 00 MIN:(1-As,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 11 01 MIN:(1-As,Ad)
	   alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			/* k1 * Cd = MIN:(1 - As, Ad) * Cd
			   k3 * Ad = 1 * Ad */
			GCREG_BLENDING_MODE_SATURATED_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = MIN:(1 - As, Ad) * Cs
			   k4 * As = 1 * As */
			GCREG_BLENDING_MODE_SATURATED_DEST_ALPHA,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 01 11 10 MIN:(1-As,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 01 11 11 MIN:(1-As,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* color factor: 10 00 00 MAX:(1-Cd,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 00 01 MAX:(1-Cd,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 00 10 MAX:(1-Cd,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 00 11 MAX:(1-Cd,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 01 00 MAX:(1-Ad,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 01 01 MAX:(1-Ad,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 01 10 MAX:(1-Ad,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 01 11 MAX:(1-Ad,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 10 00 MAX:(1-Cs,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 10 01 MAX:(1-Cs,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 10 10 MAX:(1-Cs,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 10 11 MAX:(1-Cs,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 11 00 MAX:(1-As,Cd) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 11 01 MAX:(1-As,Ad) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 11 10 MAX:(1-As,Cs) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 10 11 11 MAX:(1-As,As) ==> not supported
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/**********************************************************************/
	/* color factor: 11 00 00 C:(1-Cd,Cd)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 00 01 C:(1-Cd,Ad)=1-Cd
	   alpha factor: 1-Ad ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			/* k1 * Cd = (1 - Cd) * Cd
			   k3 * Ad = (1 - Ad) * Ad */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = (1 - Cd) * Cs
			   k4 * As = (1 - Ad) * As */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 11 00 10 C:(1-Cd,Cs)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 00 11 C:(1-Cd,As)=1-Cd
	   alpha factor: 1-Ad ==> 00 01 00 or 00 01 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A1, NORM_C2),

		{
			/* k1 * Cd = (1 - Cd) * Cd
			   k3 * Ad = (1 - Ad) * Ad */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = (1 - Cd) * Cs
			   k4 * As = (1 - Ad) * As */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 11 01 00 C:(1-Ad,Cd)=Cd
	   alpha factor: Ad ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			/* k1 * Cd = Cd * Cd
			   k3 * Ad = Ad * Ad */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = Cd * Cs
			   k4 * As = Ad * As */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 11 01 01 C:(1-Ad,Ad)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 01 10 C:(1-Ad,Cs)=Cs
	   alpha factor: As ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			/* k1 * Cd = Cs * Cd
			   k3 * Ad = As * Ad */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = Cs * Cs
			   k4 * As = As * As */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 11 01 11 C:(1-Ad,As)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 10 00 C:(1-Cs,Cd)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 10 01 C:(1-Cs,Ad)=1-Cs
	   alpha factor: 1-As ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			/* k1 * Cd = (1 - Cs) * Cd
			   k3 * Ad = (1 - As) * Ad */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = (1 - Cs) * Cs
			   k4 * As = (1 - As) * As */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 11 10 10 C:(1-Cs,Cs)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 10 11 C:(1-Cs,As)=1-Cs
	   alpha factor: 1-As ==> 00 11 00 or 00 11 10 */
	{
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C1),
		BVBLENDMATCH(ONLY_A, INV_A2, NORM_C2),

		{
			/* k1 * Cd = (1 - Cs) * Cd
			   k3 * Ad = (1 - As) * Ad */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = (1 - Cs) * Cs
			   k4 * As = (1 - As) * As */
			GCREG_BLENDING_MODE_COLOR_INVERSED,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(0)
		}
	},

	/* color factor: 11 11 00 C:(1-As,Cd)=Cd
	   alpha factor: Ad ==> 00 00 01 or 00 10 01 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A1),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A1),

		{
			/* k1 * Cd = Cd * Cd
			   k3 * Ad = Ad * Ad */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = Cd * Cs
			   k4 * As = Ad * As */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1)
		}
	},

	/* color factor: 11 11 01 C:(1-As,Ad)=undefined
	   alpha factor: N/A */
	BVBLENDUNDEFINED(),

	/* color factor: 11 11 10 C:(1-As,Cs)=Cs
	   alpha factor: As ==> 00 00 11 or 00 10 11 */
	{
		BVBLENDMATCH(ONLY_A, INV_C1, NORM_A2),
		BVBLENDMATCH(ONLY_A, INV_C2, NORM_A2),

		{
			/* k1 * Cd = Cs * Cd
			   k3 * Ad = As * Ad */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(1),
		},

		{
			/* k2 * Cs = Cs * Cs
			   k4 * As = As * As */
			GCREG_BLENDING_MODE_COLOR,
			GCREG_FACTOR_INVERSE_ENABLE,
			BVDEST(0), BVSRC(1)
		}
	},

	/* color factor: 11 11 11 C:(1-As,As)=one
	   alpha factor: one ==> 11 11 11 */
	{
		0x3F,
		0x3F,

		{
			/* k1 * Cd = 1 * Cd
			   k3 * Ad = 1 * Ad */
			GCREG_BLENDING_MODE_ONE,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(1), BVSRC(0),
		},

		{
			/* k2 * Cs = 1 * Cs
			   k4 * As = 1 * As */
			GCREG_BLENDING_MODE_ONE,
			GCREG_FACTOR_INVERSE_DISABLE,
			BVDEST(0), BVSRC(1)
		}
	}
};

static enum bverror parse_blend(struct bvbltparams *bltparams,
	enum bvblend blend, struct gcalpha *gca)
{
	enum bverror bverror;
	unsigned int global;
	unsigned int k1, k2, k3, k4;
	struct bvblendxlate *dstxlate;
	struct bvblendxlate *srcxlate;
	unsigned int alpha;

	if ((blend & BVBLENDDEF_REMOTE) != 0) {
		BVSETBLTERROR(BVERR_BLEND, "remote alpha not supported");
		goto exit;
	}

	global = (blend & BVBLENDDEF_GLOBAL_MASK) >> BVBLENDDEF_GLOBAL_SHIFT;

	switch (global) {
	case (BVBLENDDEF_GLOBAL_NONE >> BVBLENDDEF_GLOBAL_SHIFT):
		GC_PRINT(GC_INFO_MSG " BVBLENDDEF_GLOBAL_NONE\n",
			__func__, __LINE__);

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_NORMAL;

		gca->src_global_alpha_mode =
		gca->dst_global_alpha_mode = 0;
		break;

	case (BVBLENDDEF_GLOBAL_UCHAR >> BVBLENDDEF_GLOBAL_SHIFT):
		GC_PRINT(GC_INFO_MSG " BVBLENDDEF_GLOBAL_UCHAR\n",
			__func__, __LINE__);

		gca->src_global_color =
		gca->dst_global_color =
			((unsigned int) bltparams->globalalpha.size8) << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;

	case (BVBLENDDEF_GLOBAL_FLOAT >> BVBLENDDEF_GLOBAL_SHIFT):
		GC_PRINT(GC_INFO_MSG " BVBLENDDEF_GLOBAL_FLOAT\n",
			__func__, __LINE__);

		alpha = gcfp2norm8(bltparams->globalalpha.fp);

#if 0
		gca->src_global_color =
		gca->dst_global_color = alpha << 24;

		gca->src_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		gca->dst_global_alpha_mode = GCREG_GLOBAL_ALPHA_MODE_GLOBAL;
		break;
#else
		BUG();
#endif

	default:
		BVSETBLTERROR(BVERR_BLEND, "invalid global alpha mode");
		goto exit;
	}

	/*
		Co = k1 x Cd + k2 x Cs
		Ao = k3 x Ad + k4 x As
	*/

	k1 = (blend >> 18) & 0x3F;
	k2 = (blend >> 12) & 0x3F;
	k3 = (blend >>  6) & 0x3F;
	k4 =  blend        & 0x3F;

	GC_PRINT(GC_INFO_MSG " blend = 0x%08X\n", __func__, __LINE__, blend);
	GC_PRINT(GC_INFO_MSG " k1 = %d\n", __func__, __LINE__, k1);
	GC_PRINT(GC_INFO_MSG " k2 = %d\n", __func__, __LINE__, k2);
	GC_PRINT(GC_INFO_MSG " k3 = %d\n", __func__, __LINE__, k3);
	GC_PRINT(GC_INFO_MSG " k4 = %d\n", __func__, __LINE__, k4);

	dstxlate = &blendxlate[k1];
	srcxlate = &blendxlate[k2];

	if (((k3 != dstxlate->match1) && (k3 != dstxlate->match2)) ||
		((k4 != srcxlate->match1) && (k4 != srcxlate->match2))) {
		BVSETBLTERROR(BVERR_BLEND,
				"not supported coefficient combination");
		goto exit;
	}

	gca->src_config = &dstxlate->dst;
	gca->dst_config = &srcxlate->src;

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
			return BVERR_TILE;

		if (surf->tileparams->structsize != STRUCTSIZE(surf->tileparams,
								srcheight))
			return BVERR_TILE_VERS;

#if 0
		if (surf->tileparams->virtaddr == NULL)
			return BVERR_TILE_VIRTADDR;
#endif

		/* FIXME/TODO */
		return BVERR_TILE;
	} else {
		if (surf->desc == NULL)
			return BVERR_DESC;

		if (surf->desc->structsize != STRUCTSIZE(surf->desc, map))
			return BVERR_DESC_VERS;

#if 0
		if (surf->desc->virtaddr == NULL)
			return BVERR_DESC_VIRTADDR;
#endif
	}

	if (geom == NULL)
		return BVERR_GEOM;

#if 0
	if (geom->structsize != STRUCTSIZE(geom, palette))
		return BVERR_GEOM_VERS;
#endif

#if GC_DUMP
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

			if (geomsize > surf->desc->length) {
				GC_PRINT(GC_INFO_MSG
					" *** invalid geometry %dx%d\n",
					__func__, __LINE__,
					geom->width, geom->height);
			}

			if (rectsize > surf->desc->length) {
				GC_PRINT(GC_INFO_MSG
					" *** invalid rectangle "
					"(%d,%d %dx%d)\n",
					__func__, __LINE__,
					rect->left, rect->top,
					rect->width, rect->height);
			}
		}
	}
#endif

	return -1;
}

/*******************************************************************************
 * Primitive renderers.
 */

static enum bverror do_fill(struct bvbltparams *bltparams,
				struct gcbatch *batch,
				struct srcdesc *srcdesc)
{
	enum bverror bverror;
	enum bverror unmap_bverror;

	struct gcmofill *gcmofill;

	unsigned char *fillcolorptr;

	struct bvformatxlate *srcformat;
	struct bvformatxlate *dstformat;

	struct bvbuffmap *dstmap = NULL;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	struct bvrect *dstrect = &bltparams->dstrect;
	unsigned int dstoffset;

	if (!parse_format(srcdesc->geom->format, &srcformat)) {
		BVSETBLTERRORARG((srcdesc->index == 0)
					? BVERR_SRC1GEOM_FORMAT
					: BVERR_SRC2GEOM_FORMAT,
				"invalid source format (%d)",
				srcdesc->geom->format);
		goto exit;
	}

	if (!parse_format(dstgeom->format, &dstformat)) {
		BVSETBLTERRORARG(BVERR_DSTGEOM_FORMAT,
				"invalid destination format (%d)",
				dstgeom->format);
		goto exit;
	}

	bverror = do_map(bltparams->dstdesc, 0, &dstmap);
	if (bverror != BVERR_NONE) {
		bltparams->errdesc = gccontext.bverrorstr;
		goto exit;
	}

	dstoffset = 0;

	bverror = claim_buffer(batch, sizeof(struct gcmofill),
				(void **) &gcmofill);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM, "failed to allocate command buffer");
		goto exit;
	}

	memset(gcmofill, 0, sizeof(struct gcmofill));

	GC_PRINT(GC_INFO_MSG " allocated %d of commmand buffer\n",
		__func__, __LINE__, sizeof(struct gcmofill));

	/***********************************************************************
	** Set destination.
	*/

	add_fixup(batch, &gcmofill->dst.address, 0);

	/* Set surface parameters. */
	gcmofill->dst.address_ldst = gcmodst_address_ldst;
	gcmofill->dst.address = GET_MAP_HANDLE(dstmap);
	gcmofill->dst.stride = dstgeom->virtstride;
	gcmofill->dst.config.reg.swizzle = dstformat->swizzle;
	gcmofill->dst.config.reg.format = dstformat->format;

	/* Set surface width and height. */
	gcmofill->dst.rotation.reg.surf_width = dstgeom->width + dstoffset;
	gcmofill->dst.rotationheight_ldst = gcmodst_rotationheight_ldst;
	gcmofill->dst.rotationheight.reg.height = dstgeom->height;

	/* Set BLT command. */
	gcmofill->dst.config.reg.command = GCREG_DEST_CONFIG_COMMAND_CLEAR;

	/* Set clipping. */
	gcmofill->dst.clip.lt_ldst = gcmoclip_lt_ldst;

	if ((bltparams->flags & BVFLAG_CLIP) == BVFLAG_CLIP) {
		gcmofill->dst.clip.lt.reg.left
			= bltparams->cliprect.left + dstoffset;
		gcmofill->dst.clip.lt.reg.top
			= bltparams->cliprect.top;
		gcmofill->dst.clip.rb.reg.right
			= gcmofill->dst.clip.lt.reg.left
			+ bltparams->cliprect.width;
		gcmofill->dst.clip.rb.reg.bottom
			= gcmofill->dst.clip.lt.reg.top
			+ bltparams->cliprect.height;
	} else {
		gcmofill->dst.clip.lt.reg.left = GC_CLIP_RESET_LEFT;
		gcmofill->dst.clip.lt.reg.top = GC_CLIP_RESET_TOP;
		gcmofill->dst.clip.rb.reg.right = GC_CLIP_RESET_RIGHT;
		gcmofill->dst.clip.rb.reg.bottom = GC_CLIP_RESET_BOTTOM;
	}

	/***********************************************************************
	** Set source.
	*/

	/* Set surface dummy width and height. */
	gcmofill->src.rotation_ldst = gcmofillsrc_rotation_ldst;
	gcmofill->src.rotation.reg.surf_width = dstgeom->width;
	gcmofill->src.rotationheight_ldst = gcmofillsrc_rotationheight_ldst;
	gcmofill->src.rotationheight.reg.height = dstgeom->height;

	/* Set ROP3. */
	gcmofill->src.rop_ldst = gcmofillsrc_rop_ldst;
	gcmofill->src.rop.reg.type = GCREG_ROP_TYPE_ROP3;
	gcmofill->src.rop.reg.fg = (unsigned char) bltparams->op.rop;

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

	/* Set START_DE command. */
	gcmofill->start.startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmofill->start.rect.left
		= dstrect->left + dstoffset;
	gcmofill->start.rect.top
		= dstrect->top;
	gcmofill->start.rect.right
		= gcmofill->start.rect.left + dstrect->width;
	gcmofill->start.rect.bottom
		= gcmofill->start.rect.top + dstrect->height;

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

	return bverror;
}

static enum bverror do_blit_end(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	struct gcmomultisrc *gcmomultisrc;
	struct gcmostart *gcmostart;
	unsigned int buffersize;

	GC_PRINT(GC_INFO_MSG " finalizing the blit, scrcount = %d\n",
		__func__, __LINE__, batch->op.gcblit.srccount);

	buffersize
		= sizeof(struct gcmomultisrc)
		+ sizeof(struct gcmostart);

	bverror = claim_buffer(batch, buffersize, (void **) &gcmomultisrc);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM, "failed to allocate command buffer");
		goto exit;
	}

	/* Reset the finalizer. */
	batch->batchend = do_end;

	/***********************************************************************
	** Set multi-source control.
	*/

	gcmomultisrc->control_ldst = gcmomultisrc_control_ldst;
	gcmomultisrc->control.reg.srccount = batch->op.gcblit.srccount - 1;
	gcmomultisrc->control.reg.horblock
		= GCREG_DE_MULTI_SOURCE_HORIZONTAL_BLOCK_PIXEL128;

	gcmostart = (struct gcmostart *) (gcmomultisrc + 1);

	/* Set START_DE command. */
	gcmostart->startde.cmd.fld = gcfldstartde;

	/* Set destination rectangle. */
	gcmostart->rect = batch->op.gcblit.dstrect;

	/* Flush PE cache. */
	gcmostart->flush.flush_ldst = gcmoflush_flush_ldst;
	gcmostart->flush.flush.reg = gcregflush_pe2D;

exit:
	return bverror;
}

static enum bverror do_blit(struct bvbltparams *bltparams,
				struct gcbatch *batch,
				struct srcdesc *srcdesc,
				unsigned int srccount,
				struct gcalpha *gca)
{
	enum bverror bverror;
	enum bverror unmap_bverror;

	unsigned int buffersize;
	void *buffer;
	struct gcmodst *gcmodst;
	struct gcmosrc *gcmosrc;

	unsigned int i, index;
	unsigned int startblit;

	struct bvformatxlate *srcformat[2];
	struct bvbuffmap *srcmap[2] = { NULL, NULL };
	struct bvsurfgeom *srcgeom;
	struct bvrect *srcrect;
	int srcsurfleft, srcsurftop;
	int srcleft[2], srctop[2];
	int srcshift[2];
	int srcoffset;
	int srcadjust;

	struct bvformatxlate *dstformat;
	struct bvbuffmap *dstmap = NULL;
	struct bvsurfgeom *dstgeom = bltparams->dstgeom;
	unsigned int dstleft, dsttop;
	unsigned int dstright, dstbottom;
	unsigned int dstoffset;

	unsigned int multiblit = 1;

	if (!parse_format(dstgeom->format, &dstformat)) {
		BVSETBLTERRORARG(BVERR_DSTGEOM_FORMAT,
				"invalid destination format (%d)",
				dstgeom->format);
		goto exit;
	}

	bverror = do_map(bltparams->dstdesc, 0, &dstmap);
	if (bverror != BVERR_NONE) {
		bltparams->errdesc = gccontext.bverrorstr;
		goto exit;
	}

	/* Determine destination coordinates. */
	dstleft   = bltparams->dstrect.left;
	dsttop    = bltparams->dstrect.top;
	dstright  = bltparams->dstrect.width  + dstleft;
	dstbottom = bltparams->dstrect.height + dsttop;

	dstoffset = 0;

	GC_PRINT(GC_INFO_MSG " dstaddr = 0x%08X\n",
		__func__, __LINE__,
		(unsigned int) bltparams->dstdesc->virtaddr);

	GC_PRINT(GC_INFO_MSG " dstsurf = %dx%d, stride = %ld\n",
		__func__, __LINE__,
		bltparams->dstgeom->width, bltparams->dstgeom->height,
		bltparams->dstgeom->virtstride);

	GC_PRINT(GC_INFO_MSG " dstrect = (%d,%d)-(%d,%d), dstoffset = %d\n",
		__func__, __LINE__,
		dstleft, dsttop, dstright, dstbottom, dstoffset);

	GC_PRINT(GC_INFO_MSG " dstrect = %dx%d\n",
		__func__, __LINE__,
		bltparams->dstrect.width, bltparams->dstrect.height);

	dstleft  += dstoffset;
	dstright += dstoffset;

	/* Set destination coordinates. */
	batch->op.gcblit.dstrect.left   = dstleft;
	batch->op.gcblit.dstrect.top    = dsttop;
	batch->op.gcblit.dstrect.right  = dstright;
	batch->op.gcblit.dstrect.bottom = dstbottom;

	for (i = 0; i < srccount; i += 1) {
		srcgeom = srcdesc[i].geom;
		srcrect = srcdesc[i].rect;

		if (!parse_format(srcgeom->format, &srcformat[i])) {
			BVSETBLTERRORARG((srcdesc[i].index == 0)
						? BVERR_SRC1GEOM_FORMAT
						: BVERR_SRC2GEOM_FORMAT,
					"invalid source format (%d)",
					srcgeom->format);
			goto exit;
		}

		srcoffset = 0;

		GC_PRINT(GC_INFO_MSG " srcaddr[%d] = 0x%08X\n",
			__func__, __LINE__,
			i, (unsigned int) srcdesc[i].buf.desc->virtaddr);

		GC_PRINT(GC_INFO_MSG " srcsurf%d = %dx%d, stride%d = %ld\n",
			__func__, __LINE__,
			i, srcgeom->width, srcgeom->height,
			i, srcgeom->virtstride);

		GC_PRINT(GC_INFO_MSG
			" srcrect%d = (%d,%d)-(%d,%d), srcoffset%d = %d\n",
			__func__, __LINE__,
			i,
			srcrect->left, srcrect->top,
			srcrect->left + srcrect->width,
			srcrect->top  + srcrect->height,
			i,
			srcoffset);

		GC_PRINT(GC_INFO_MSG " srcrect%d = %dx%d\n",
			__func__, __LINE__,
			i, srcrect->width, srcrect->height);

		srcsurfleft = srcrect->left - dstleft + srcoffset;
		srcsurftop  = srcrect->top  - dsttop;

		GC_PRINT(GC_INFO_MSG " source %d surface origin = %d,%d\n",
			__func__, __LINE__, i, srcsurfleft, srcsurftop);

		srcadjust = srcsurfleft % 4;
		srcsurfleft -= srcadjust;
		srcleft[i] = dstleft + srcadjust;
		srctop[i]  = dsttop;

		GC_PRINT(GC_INFO_MSG " srcadjust%d = %d\n",
			__func__, __LINE__, i, srcadjust);

		GC_PRINT(GC_INFO_MSG
			" adjusted source %d surface origin = %d,%d\n",
			__func__, __LINE__, i, srcsurfleft, srcsurftop);

		GC_PRINT(GC_INFO_MSG " source %d rectangle origin = %d,%d\n",
			__func__, __LINE__, i, srcleft[i], srctop[i]);

		srcshift[i]
			= srcsurftop * (int) srcgeom->virtstride
			+ srcsurfleft * (int) srcformat[i]->bitspp / 8;

		if (srcadjust != 0)
			multiblit = 0;

		GC_PRINT(GC_INFO_MSG " srcshift[%d] = 0x%08X\n",
			__func__, __LINE__, i, srcshift[i]);

		bverror = do_map(srcdesc[i].buf.desc, 0, &srcmap[i]);
		if (bverror != BVERR_NONE) {
			bltparams->errdesc = gccontext.bverrorstr;
			goto exit;
		}
	}

	GC_PRINT(GC_INFO_MSG " multiblit = %d\n",
		__func__, __LINE__, multiblit);

	if ((batch->batchend == do_blit_end) &&
		(batch->op.gcblit.srccount < 4)) {
		GC_PRINT(GC_ERR_MSG " adding new source to the operation\n",
			__func__, __LINE__);

		startblit = 0;
		buffersize = sizeof(struct gcmosrc) * srccount;
	} else {
		if (batch->batchend == do_blit_end) {
			GC_PRINT(GC_INFO_MSG
				" maximum number of sources reached\n",
				__func__, __LINE__);
		} else {
			GC_PRINT(GC_INFO_MSG
				" another operation in progress\n",
				__func__, __LINE__);
		}

		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

		startblit = 1;
		buffersize
			= sizeof(struct gcmodst)
			+ sizeof(struct gcmosrc) * srccount;

		batch->batchend = do_blit_end;
		batch->op.gcblit.srccount = 0;
	}

	bverror = claim_buffer(batch, buffersize, &buffer);
	if (bverror != BVERR_NONE) {
		BVSETBLTERROR(BVERR_OOM, "failed to allocate command buffer");
		goto exit;
	}

	memset(buffer, 0, buffersize);

	GC_PRINT(GC_INFO_MSG " allocated %d of commmand buffer\n",
		__func__, __LINE__, buffersize);

	/***********************************************************************
	** Set destination.
	*/

	if (startblit) {
		GC_PRINT(GC_INFO_MSG " processing the destiantion\n",
			__func__, __LINE__);

		gcmodst = (struct gcmodst *) buffer;

		add_fixup(batch, &gcmodst->address, 0);

		/* Set surface parameters. */
		gcmodst->address_ldst = gcmodst_address_ldst;
		gcmodst->address = GET_MAP_HANDLE(dstmap);
		gcmodst->stride = dstgeom->virtstride;
		gcmodst->config.reg.swizzle = dstformat->swizzle;
		gcmodst->config.reg.format = dstformat->format;

		/* Set surface width and height. */
		gcmodst->rotation.reg.surf_width = dstgeom->width + dstoffset;
		gcmodst->rotationheight_ldst = gcmodst_rotationheight_ldst;
		gcmodst->rotationheight.reg.height = dstgeom->height;

		/* Set BLT command. */
		gcmodst->config.reg.command = multiblit
			? GCREG_DEST_CONFIG_COMMAND_MULTI_SOURCE_BLT
			: GCREG_DEST_CONFIG_COMMAND_BIT_BLT;

		/* Set clipping. */
		gcmodst->clip.lt_ldst = gcmoclip_lt_ldst;

		if ((bltparams->flags & BVFLAG_CLIP) == BVFLAG_CLIP) {
			gcmodst->clip.lt.reg.left
				= bltparams->cliprect.left + dstoffset;
			gcmodst->clip.lt.reg.top
				= bltparams->cliprect.top;
			gcmodst->clip.rb.reg.right
				= gcmodst->clip.lt.reg.left
				+ bltparams->cliprect.width;
			gcmodst->clip.rb.reg.bottom
				= gcmodst->clip.lt.reg.top
				+ bltparams->cliprect.height;
		} else {
			gcmodst->clip.lt.reg.left = GC_CLIP_RESET_LEFT;
			gcmodst->clip.lt.reg.top = GC_CLIP_RESET_TOP;
			gcmodst->clip.rb.reg.right = GC_CLIP_RESET_RIGHT;
			gcmodst->clip.rb.reg.bottom = GC_CLIP_RESET_BOTTOM;
		}

		/* Determine location of source states. */
		gcmosrc = (struct gcmosrc *) (gcmodst + 1);
	} else {
		GC_PRINT(GC_INFO_MSG " skipping the destiantion\n",
			__func__, __LINE__);

		/* Determine location of source states. */
		gcmosrc = (struct gcmosrc *) buffer;
	}

	/***********************************************************************
	** Set source(s).
	*/

	GC_PRINT(GC_INFO_MSG " processing %d sources\n",
		__func__, __LINE__, srccount);

	index = batch->op.gcblit.srccount;

	for (i = 0; i < srccount; i += 1, index += 1) {
		srcgeom = srcdesc[i].geom;

		add_fixup(batch, &gcmosrc->address, srcshift[i]);

		/* Set surface parameters. */
		gcmosrc->address_ldst = gcmosrc_address_ldst[index];
		gcmosrc->address = GET_MAP_HANDLE(srcmap[i]);

		gcmosrc->stride_ldst = gcmosrc_stride_ldst[index];
		gcmosrc->stride = srcgeom->virtstride;

		gcmosrc->rotation_ldst = gcmosrc_rotation_ldst[index];
		gcmosrc->rotation.reg.surf_width = dstleft + srcgeom->width;

		gcmosrc->config_ldst = gcmosrc_config_ldst[index];
		gcmosrc->config.reg.swizzle = srcformat[i]->swizzle;
		gcmosrc->config.reg.format = srcformat[i]->format;

		gcmosrc->origin_ldst = gcmosrc_origin_ldst[index];
		if (multiblit) {
			gcmosrc->origin.reg = gcregsrcorigin_min;
		} else {
			gcmosrc->origin.reg.x = srcleft[i];
			gcmosrc->origin.reg.y = srctop[i];
		}

		gcmosrc->size_ldst = gcmosrc_size_ldst[index];
		gcmosrc->size.reg = gcregsrcsize_max;

		gcmosrc->rotationheight_ldst
			= gcmosrc_rotationheight_ldst[index];
		gcmosrc->rotationheight.reg.height = dsttop + srcgeom->height;

		gcmosrc->rop_ldst = gcmosrc_rop_ldst[index];
		gcmosrc->rop.reg.type = GCREG_ROP_TYPE_ROP3;
		gcmosrc->rop.reg.fg = (gca != NULL)
			? 0xCC : (unsigned char) bltparams->op.rop;

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
			gcmosrc->alphamodes.reg.src_blend
				= gca->src_config->factor_mode;
			gcmosrc->alphamodes.reg.dst_blend
				= gca->dst_config->factor_mode;
			gcmosrc->alphamodes.reg.src_color_reverse
				= gca->src_config->color_reverse;
			gcmosrc->alphamodes.reg.dst_color_reverse
				= gca->dst_config->color_reverse;

			gcmosrc->srcglobal.raw = gca->src_global_color;
			gcmosrc->dstglobal.raw = gca->dst_global_color;
		} else
			gcmosrc->alphacontrol.reg.enable
				= GCREG_ALPHA_CONTROL_ENABLE_OFF;

		gcmosrc += 1;
		batch->op.gcblit.srccount += 1;
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

	return bverror;
}

static enum bverror do_filter(struct bvbltparams *bltparams,
				struct gcbatch *batch)
{
	enum bverror bverror;
	BVSETBLTERROR(BVERR_UNK, "FIXME/TODO");
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

	/* FIXME/TODO: add check for initialization success. */

	if (buffdesc == NULL) {
		BVSETERROR(BVERR_UNK, "invalid argument");
		goto exit;
	}

	if (buffdesc->structsize != STRUCTSIZE(buffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	bverror = do_map(buffdesc, 1, &bvbuffmap);

exit:
	return bverror;
}
EXPORT_SYMBOL(gcbv_map);

enum bverror gcbv_unmap(struct bvbuffdesc *buffdesc)
{
	enum bverror bverror;
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;

	/* FIXME/TODO: add check for initialization success. */

	if (buffdesc == NULL) {
		BVSETERROR(BVERR_UNK, "invalid argument");
		goto exit;
	}

	if (buffdesc->structsize != STRUCTSIZE(buffdesc, map)) {
		BVSETERROR(BVERR_BUFFERDESC_VERS, "argument has invalid size");
		goto exit;
	}

	bvbuffmap = buffdesc->map;
	if (bvbuffmap == NULL)
		return BVERR_NONE;

	if (bvbuffmap->structsize != STRUCTSIZE(bvbuffmap, nextmap)) {
		BVSETERROR(BVERR_UNK, "invalid map structure size");
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
	struct gcbatch *batch;
	int src1used, src2used, maskused;
	struct srcdesc srcdesc[2];
	unsigned short rop, blend, format;
	struct gccommit gccommit;
	int srccount, res;

	/* FIXME/TODO: add check for initialization success. */

	/* Verify blt parameters structure. */
	if (bltparams == NULL) {
		BVSETERROR(BVERR_UNK,
				"pointer to bvbltparams struct is expected");
		goto exit;
	}

	if (bltparams->structsize != STRUCTSIZE(bltparams, callbackdata)) {
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
		GC_PRINT(GC_INFO_MSG " BVFLAG_BATCH_NONE\n",
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
		GC_PRINT(GC_INFO_MSG " BVFLAG_BATCH_BEGIN\n",
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
		GC_PRINT(GC_INFO_MSG " BVFLAG_BATCH_CONTINUE\n",
			__func__, __LINE__);
		batch = (struct gcbatch *) bltparams->batch;
		if (batch == NULL) {
			BVSETBLTERROR(BVERR_BATCH, "batch is not initialized");
			goto exit;
		}

		if (batch->structsize != STRUCTSIZE(batch, unmap)) {
			BVSETBLTERROR(BVERR_BATCH, "invalid batch");
			goto exit;
		}

		batchexec = 0;
		batchflags = bltparams->batchflags;
		break;

	case (BVFLAG_BATCH_END >> BVFLAG_BATCH_SHIFT):
		GC_PRINT(GC_INFO_MSG " BVFLAG_BATCH_END\n",
			__func__, __LINE__);
		batch = (struct gcbatch *) bltparams->batch;
		if (batch == NULL) {
			BVSETBLTERROR(BVERR_BATCH, "batch is not initialized");
			goto exit;
		}

		if (batch->structsize != STRUCTSIZE(batch, unmap)) {
			BVSETBLTERROR(BVERR_BATCH, "invalid batch");
			goto exit;
		}

		batchexec = 1;
		batchflags = (bltparams->batchflags & BVBATCH_ENDNOP)
			? 0
			: bltparams->batchflags;
		break;

	default:
		BVSETBLTERROR(BVERR_BATCH, "unrecognized batch type");
		goto exit;
	}

	GC_PRINT(GC_INFO_MSG " batchflags=0x%08X\n",
		__func__, __LINE__, batchflags);

	if (batchflags != 0) {
		switch (op) {
		case (BVFLAG_ROP >> BVFLAG_OP_SHIFT):
			GC_PRINT(GC_INFO_MSG " BVFLAG_ROP\n",
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
			GC_PRINT(GC_INFO_MSG " BVFLAG_BLEND\n",
				__func__, __LINE__);
			blend = bltparams->op.blend;
			format = (blend & BVBLENDDEF_FORMAT_MASK)
				>> BVBLENDDEF_FORMAT_SHIFT;

			bverror = parse_blend(bltparams,
						bltparams->op.blend, &_gca);
			if (bverror != BVERR_NONE)
				goto exit;

			gca = &_gca;

			/* FIXME/TODO: logic here is incorrect */
			switch (format) {
			case (BVBLENDDEF_FORMAT_CLASSIC
				>> BVBLENDDEF_FORMAT_SHIFT):
				src1used
					= gca->dst_config->destuse
					| gca->src_config->destuse;
				src2used
					= gca->dst_config->srcuse
					| gca->src_config->srcuse;
				maskused
					= blend & BVBLENDDEF_REMOTE;
				break;

			default:
				BVSETBLTERROR(BVERR_BLEND,
						"unrecognized blend format");
				goto exit;
			}
			break;

		case (BVFLAG_FILTER >> BVFLAG_OP_SHIFT):
			GC_PRINT(GC_INFO_MSG " BVFLAG_FILTER\n",
				__func__, __LINE__);
			BVSETBLTERROR(BVERR_UNK, "FIXME/TODO");
			goto exit;

		default:
			BVSETBLTERROR(BVERR_OP, "unrecognized operation");
			goto exit;
		}

		srccount = 0;

		/* Verify the src1 parameters structure. */
		if (src1used) {
			GC_PRINT(GC_INFO_MSG " src1used\n",
				__func__, __LINE__);

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_SRC1,
				&bltparams->src1, bltparams->src1geom,
				&bltparams->src1rect);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src1surferr);
				goto exit;
			}

			/* Same as the destination? */
			if ((bltparams->src1.desc->virtaddr
				== bltparams->dstdesc->virtaddr) &&
				EQ_ORIGIN(bltparams->src1rect,
						bltparams->dstrect) &&
				EQ_SIZE(bltparams->src1rect,
						bltparams->dstrect)) {
				GC_PRINT(GC_INFO_MSG
					" src1 is the same as dst\n",
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
			GC_PRINT(GC_INFO_MSG " src2used\n",
				__func__, __LINE__);

			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_SRC2,
				&bltparams->src2, bltparams->src2geom,
				&bltparams->src2rect);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_src2surferr);
				goto exit;
			}

			/* Same as the destination? */
			if ((bltparams->src2.desc->virtaddr
				== bltparams->dstdesc->virtaddr) &&
				EQ_ORIGIN(bltparams->src2rect,
						bltparams->dstrect) &&
				EQ_SIZE(bltparams->src2rect,
						bltparams->dstrect)) {
				GC_PRINT(GC_INFO_MSG
					" src2 is the same as dst\n",
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
			GC_PRINT(GC_INFO_MSG " maskused\n",
				__func__, __LINE__);
			res = verify_surface(
				bltparams->flags & BVBATCH_TILE_MASK,
				&bltparams->mask, bltparams->maskgeom,
				&bltparams->maskrect);
			if (res != -1) {
				BVSETBLTSURFERROR(res, g_masksurferr);
				goto exit;
			}

			BVSETERROR(BVERR_UNK, "FIXME/TODO");
			goto exit;
		}

		GC_PRINT(GC_INFO_MSG " srccount = %d\n",
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
					BVSETBLTERROR(
						BVERR_UNK, "FIXME/TODO");
			else
				if (EQ_SIZE((*srcdesc[1].rect),
					bltparams->dstrect))
					BVSETBLTERROR(
						BVERR_UNK, "FIXME/TODO");
				else
					BVSETBLTERROR(
						BVERR_UNK, "FIXME/TODO");
		}
	}

	if (batchexec) {
		/* Finalize the current operation. */
		bverror = batch->batchend(bltparams, batch);
		if (bverror != BVERR_NONE)
			goto exit;

#if GC_DUMP
		dumpbuffer(batch);
#endif

		gccommit.gcerror = GCERR_NONE;
		gccommit.buffer = batch->bufhead;

		gc_commit_wrapper(&gccommit);
		if (gccommit.gcerror != GCERR_NONE) {
			BVSETBLTERRORARG(BVERR_UNK,
					"blit error occured (0x%08X)",
					gccommit.gcerror);
			goto exit;
		}
	}

exit:
	if ((batch != NULL) && batchexec) {
		process_scheduled_unmap(batch);
		free_batch(batch);
		bltparams->batch = NULL;
	}

	return bverror;
}
EXPORT_SYMBOL(gcbv_blt);
