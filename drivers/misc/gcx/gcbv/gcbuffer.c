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
#define GCZONE_BATCH_ALLOC	(1 << 0)
#define GCZONE_BUFFER_ALLOC	(1 << 1)
#define GCZONE_FIXUP_ALLOC	(1 << 2)
#define GCZONE_FIXUP		(1 << 3)

GCDBG_FILTERDEF(buffer, GCZONE_NONE,
		"batchalloc",
		"bufferalloc"
		"fixupalloc",
		"fixup")


/*******************************************************************************
** Miscellaneous defines and macros.
*/

#define GC_BUFFER_INIT_SIZE \
( \
	GC_BUFFER_SIZE - max(sizeof(struct gcbuffer), GC_BUFFER_RESERVE) \
)

#define GC_BUFFER_RESERVE \
( \
	sizeof(struct gcmopipesel) + \
	sizeof(struct gcmommumaster) + \
	sizeof(struct gcmommuflush) + \
	sizeof(struct gcmosignal) + \
	sizeof(struct gccmdend) \
)


/*******************************************************************************
 * Batch/command buffer management.
 */

enum bverror do_end(struct bvbltparams *bvbltparams,
		    struct gcbatch *gcbatch)
{
	return BVERR_NONE;
}

enum bverror allocate_batch(struct bvbltparams *bvbltparams,
			    struct gcbatch **gcbatch)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gcbatch *temp;
	struct gcbuffer *gcbuffer;

	GCENTER(GCZONE_BATCH_ALLOC);

	/* Lock access to batch management. */
	GCLOCK(&gccontext->batchlock);

	if (list_empty(&gccontext->batchvac)) {
		temp = gcalloc(struct gcbatch, sizeof(struct gcbatch));
		if (temp == NULL) {
			BVSETBLTERROR(BVERR_OOM,
				      "batch header allocation failed");
			goto exit;
		}

		GCDBG(GCZONE_BATCH_ALLOC, "allocated new batch = 0x%08X\n",
		      (unsigned int) temp);
	} else {
		struct list_head *head;
		head = gccontext->batchvac.next;
		temp = list_entry(head, struct gcbatch, link);
		list_del(head);

		GCDBG(GCZONE_BATCH_ALLOC, "reusing batch = 0x%08X\n",
		      (unsigned int) temp);
	}

	memset(temp, 0, sizeof(struct gcbatch));
	temp->structsize = sizeof(struct gcbatch);
	temp->batchend = do_end;
	INIT_LIST_HEAD(&temp->buffer);
	INIT_LIST_HEAD(&temp->unmap);
	INIT_LIST_HEAD(&temp->link);

	bverror = append_buffer(bvbltparams, temp, &gcbuffer);
	if (bverror != BVERR_NONE) {
		free_batch(temp);
		goto exit;
	}

	*gcbatch = temp;

	GCDBG(GCZONE_BATCH_ALLOC, "batch allocated = 0x%08X\n",
	      (unsigned int) temp);

exit:
	/* Unlock access to batch management. */
	GCUNLOCK(&gccontext->batchlock);

	GCEXITARG(GCZONE_BATCH_ALLOC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

void free_batch(struct gcbatch *gcbatch)
{
	struct list_head *head;
	struct gccontext *gccontext = get_context();
	struct gcbuffer *gcbuffer;

	GCENTERARG(GCZONE_BATCH_ALLOC, "batch = 0x%08X\n",
		   (unsigned int) gcbatch);

	/* Lock access. */
	GCLOCK(&gccontext->batchlock);
	GCLOCK(&gccontext->bufferlock);
	GCLOCK(&gccontext->fixuplock);
	GCLOCK(&gccontext->maplock);

	/* Free implicit unmappings. */
	list_splice_init(&gcbatch->unmap, &gccontext->unmapvac);

	/* Free command buffers. */
	while (!list_empty(&gcbatch->buffer)) {
		head = gcbatch->buffer.next;
		gcbuffer = list_entry(head, struct gcbuffer, link);

		/* Free fixups. */
		list_splice_init(&gcbuffer->fixup, &gccontext->fixupvac);

		/* Free the command buffer. */
		list_move(&gcbuffer->link, &gccontext->buffervac);
	}

	/* Free the batch. */
	list_add(&gcbatch->link, &gccontext->batchvac);

	/* Unlock access. */
	GCUNLOCK(&gccontext->maplock);
	GCUNLOCK(&gccontext->fixuplock);
	GCUNLOCK(&gccontext->bufferlock);
	GCUNLOCK(&gccontext->batchlock);

	GCEXIT(GCZONE_BATCH_ALLOC);
}

enum bverror append_buffer(struct bvbltparams *bvbltparams,
			   struct gcbatch *gcbatch,
			   struct gcbuffer **gcbuffer)
{
	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct gcbuffer *temp;

	GCENTERARG(GCZONE_BUFFER_ALLOC, "batch = 0x%08X\n",
		   (unsigned int) gcbatch);

	/* Lock access to buffer management. */
	GCLOCK(&gccontext->bufferlock);

	if (list_empty(&gccontext->buffervac)) {
		temp = gcalloc(struct gcbuffer, GC_BUFFER_SIZE);
		if (temp == NULL) {
			BVSETBLTERROR(BVERR_OOM,
				      "command buffer allocation failed");
			goto exit;
		}

		list_add_tail(&temp->link, &gcbatch->buffer);

		GCDBG(GCZONE_BUFFER_ALLOC, "allocated new buffer = 0x%08X\n",
		      (unsigned int) temp);
	} else {
		struct list_head *head;
		head = gccontext->buffervac.next;
		temp = list_entry(head, struct gcbuffer, link);

		list_move_tail(&temp->link, &gcbatch->buffer);

		GCDBG(GCZONE_BUFFER_ALLOC, "reusing buffer = 0x%08X\n",
		      (unsigned int) temp);
	}

	INIT_LIST_HEAD(&temp->fixup);
	temp->pixelcount = 0;
	temp->head = temp->tail = (unsigned int *) (temp + 1);
	temp->available = GC_BUFFER_INIT_SIZE;

	GCDBG(GCZONE_BUFFER_ALLOC, "new buffer appended = 0x%08X\n",
	      (unsigned int) temp);

	*gcbuffer = temp;
	bverror = BVERR_NONE;

exit:
	/* Unlock access to buffer management. */
	GCUNLOCK(&gccontext->bufferlock);

	GCEXITARG(GCZONE_BUFFER_ALLOC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

static enum bverror allocate_fixup(struct bvbltparams *bvbltparams,
				   struct gcbuffer *gcbuffer,
				   struct gcfixup **gcfixup)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();
	struct gcfixup *temp;

	if (list_empty(&gccontext->fixupvac)) {
		temp = gcalloc(struct gcfixup, sizeof(struct gcfixup));
		if (temp == NULL) {
			BVSETBLTERROR(BVERR_OOM, "fixup allocation failed");
			goto exit;
		}

		list_add_tail(&temp->link, &gcbuffer->fixup);

		GCDBG(GCZONE_FIXUP_ALLOC,
		      "new fixup struct allocated = 0x%08X\n",
		      (unsigned int) temp);
	} else {
		struct list_head *head;
		head = gccontext->fixupvac.next;
		temp = list_entry(head, struct gcfixup, link);

		list_move_tail(&temp->link, &gcbuffer->fixup);

		GCDBG(GCZONE_FIXUP_ALLOC, "fixup struct reused = 0x%08X\n",
			(unsigned int) temp);
	}

	temp->count = 0;
	*gcfixup = temp;

exit:
	return bverror;
}

enum bverror add_fixup(struct bvbltparams *bvbltparams,
		       struct gcbatch *gcbatch,
		       unsigned int *ptr,
		       unsigned int surfoffset)
{
	enum bverror bverror = BVERR_NONE;
	struct gccontext *gccontext = get_context();
	struct list_head *head;
	struct gcbuffer *buffer;
	struct gcfixup *gcfixup;

	GCENTERARG(GCZONE_FIXUP, "batch = 0x%08X, fixup ptr = 0x%08X\n",
		   (unsigned int) gcbatch, (unsigned int) ptr);

	/* Lock access to fixup management. */
	GCLOCK(&gccontext->fixuplock);

	/* Get the current command buffer. */
	if (list_empty(&gcbatch->buffer)) {
		GCERR("no command buffers are allocated");
		goto exit;
	}
	head = gcbatch->buffer.prev;
	buffer = list_entry(head, struct gcbuffer, link);

	/* No fixups? Allocate one. */
	if (list_empty(&buffer->fixup)) {
		GCDBG(GCZONE_FIXUP_ALLOC, "no fixups allocated.\n");
		bverror = allocate_fixup(bvbltparams, buffer, &gcfixup);
		if (bverror != BVERR_NONE)
			goto exit;
	} else {
		/* Get the current fixup. */
		head = buffer->fixup.prev;
		gcfixup = list_entry(head, struct gcfixup, link);

		/* No more room? */
		if (gcfixup->count == GC_FIXUP_MAX) {
			GCDBG(GCZONE_FIXUP_ALLOC,
			      "out of room, allocating new.\n");
			bverror = allocate_fixup(bvbltparams, buffer, &gcfixup);
			if (bverror != BVERR_NONE)
				goto exit;
		}
	}

	GCDBG(GCZONE_FIXUP, "buffer = 0x%08X, fixup struct = 0x%08X\n",
	      (unsigned int) buffer, (unsigned int) gcfixup);

	gcfixup->fixup[gcfixup->count].dataoffset = ptr - buffer->head;
	gcfixup->fixup[gcfixup->count].surfoffset = surfoffset;
	gcfixup->count += 1;

	GCDBG(GCZONE_FIXUP, "fixup offset = 0x%08X\n", ptr - buffer->head);
	GCDBG(GCZONE_FIXUP, "surface offset = 0x%08X\n", surfoffset);

exit:
	/* Unlock access to fixup management. */
	GCUNLOCK(&gccontext->fixuplock);

	GCEXITARG(GCZONE_FIXUP, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}

enum bverror claim_buffer(struct bvbltparams *bvbltparams,
			  struct gcbatch *gcbatch,
			  unsigned int size,
			  void **buffer)
{
	enum bverror bverror;
	struct list_head *head;
	struct gcbuffer *gcbuffer;

	GCENTERARG(GCZONE_BUFFER_ALLOC, "batch = 0x%08X, size = %d\n",
		   (unsigned int) gcbatch, size);

	if (size > GC_BUFFER_INIT_SIZE) {
		GCERR("requested size is too big.\n");
		BVSETBLTERROR(BVERR_OOM,
			      "command buffer allocation failed");
		goto exit;
	}

	/* Get the current command buffer. */
	head = gcbatch->buffer.prev;
	gcbuffer = list_entry(head, struct gcbuffer, link);

	GCDBG(GCZONE_BUFFER_ALLOC, "buffer = 0x%08X, available = %d\n",
	      (unsigned int) gcbuffer, gcbuffer->available);

	if (gcbuffer->available < size) {
		bverror = append_buffer(bvbltparams, gcbatch, &gcbuffer);
		if (bverror != BVERR_NONE)
			goto exit;
	}

	*buffer = gcbuffer->tail;
	gcbuffer->tail = (unsigned int *)
			((unsigned char *) gcbuffer->tail + size);
	gcbuffer->available -= size;
	gcbatch->size += size;
	bverror = BVERR_NONE;

exit:
	GCEXITARG(GCZONE_BUFFER_ALLOC, "bv%s = %d\n",
		  (bverror == BVERR_NONE) ? "result" : "error", bverror);
	return bverror;
}
