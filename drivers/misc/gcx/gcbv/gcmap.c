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

GCDBG_FILTERDEF(map, GCZONE_NONE,
		"mapping")


/*******************************************************************************
 * Memory management.
 */

enum bverror do_map(struct bvbuffdesc *bvbuffdesc,
		    struct gcbatch *batch,
		    struct bvbuffmap **map)
{
	static const int mapsize
		= sizeof(struct bvbuffmap)
		+ sizeof(struct bvbuffmapinfo);

	enum bverror bverror;
	struct gccontext *gccontext = get_context();
	struct bvbuffmap *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;
	struct bvphysdesc *bvphysdesc;
	bool mappedbyothers;
	struct gcimap gcimap;
	struct gcschedunmap *gcschedunmap;

	GCENTERARG(GCZONE_MAPPING, "bvbuffdesc = 0x%08X\n",
		   (unsigned int) bvbuffdesc);

	/* Lock access to the mapping list. */
	GCLOCK(&gccontext->maplock);

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
		if (gccontext->buffmapvac == NULL) {
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
			bvbuffmap = gccontext->buffmapvac;
			gccontext->buffmapvac = bvbuffmap->nextmap;
		}

		/* Setup buffer mapping. Here we need to check and make sure
		 * that the buffer starts at a location that is supported by
		 * the hw. If it is not, offset is computed and the buffer is
		 * extended by the value of the offset. */
		gcimap.gcerror = GCERR_NONE;
		gcimap.handle = 0;

		if (bvbuffdesc->auxtype == BVAT_PHYSDESC) {
			bvphysdesc = (struct bvphysdesc *) bvbuffdesc->auxptr;

			if (bvphysdesc->structsize <
			    STRUCTSIZE(bvphysdesc, pageoffset)) {
				BVSETERROR(BVERR_BUFFERDESC_VERS,
					   "unsupported bvphysdesc version");
				goto fail;
			}

			gcimap.buf.offset = bvphysdesc->pageoffset;
			gcimap.pagesize = bvphysdesc->pagesize;
			gcimap.pagearray = bvphysdesc->pagearray;
			gcimap.size = bvbuffdesc->length;

			GCDBG(GCZONE_MAPPING, "new mapping (%s):\n",
			      (batch == NULL) ? "explicit" : "implicit");
			GCDBG(GCZONE_MAPPING, "pagesize = %lu\n",
			      bvphysdesc->pagesize);
			GCDBG(GCZONE_MAPPING, "pagearray = 0x%08X\n",
			      (unsigned int) bvphysdesc->pagearray);
			GCDBG(GCZONE_MAPPING, "pageoffset = %lu\n",
			      bvphysdesc->pageoffset);
			GCDBG(GCZONE_MAPPING, "mapping size = %d\n",
			      gcimap.size);
		} else {
			gcimap.buf.logical = bvbuffdesc->virtaddr;
			gcimap.pagesize = 0;
			gcimap.pagearray = NULL;
			gcimap.size = bvbuffdesc->length;

			GCDBG(GCZONE_MAPPING, "new mapping (%s):\n",
			      (batch == NULL) ? "explicit" : "implicit");
			GCDBG(GCZONE_MAPPING, "specified virtaddr = 0x%08X\n",
			      (unsigned int) bvbuffdesc->virtaddr);
			GCDBG(GCZONE_MAPPING, "aligned virtaddr = 0x%08X\n",
			      (unsigned int) gcimap.buf.logical);
			GCDBG(GCZONE_MAPPING, "mapping size = %d\n",
			      gcimap.size);
		}

		gc_map_wrapper(&gcimap);
		if (gcimap.gcerror != GCERR_NONE) {
			BVSETERROR(BVERR_OOM,
				   "unable to allocate gccore memory");
			goto fail;
		}

		/* Set map handle. */
		bvbuffmapinfo = (struct bvbuffmapinfo *) bvbuffmap->handle;
		bvbuffmapinfo->handle = gcimap.handle;

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
		if (list_empty(&gccontext->unmapvac)) {
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
			head = gccontext->unmapvac.next;
			gcschedunmap = list_entry(head,
						  struct gcschedunmap,
						  link);
			list_move(&gcschedunmap->link, &batch->unmap);
		}

		gcschedunmap->handle = (unsigned long) bvbuffdesc;

		GCDBG(GCZONE_MAPPING, "scheduled for unmapping.\n");
	}

	/* Set the map pointer. */
	*map = bvbuffmap;

	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext->maplock);

	GCEXITARG(GCZONE_MAPPING, "handle = 0x%08X\n",
		  bvbuffmapinfo->handle);
	return BVERR_NONE;

fail:
	if (bvbuffmap != NULL) {
		bvbuffmap->nextmap = gccontext->buffmapvac;
		gccontext->buffmapvac = bvbuffmap;
	}

	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext->maplock);

	GCEXITARG(GCZONE_MAPPING, "bverror = %d\n", bverror);
	return bverror;
}

void do_unmap_implicit(struct gcbatch *batch)
{
	struct gccontext *gccontext = get_context();
	struct list_head *head, *temphead;
	struct gcschedunmap *gcschedunmap;
	struct bvbuffdesc *bvbuffdesc;
	struct bvbuffmap *prev, *bvbuffmap;
	struct bvbuffmapinfo *bvbuffmapinfo;

	GCENTER(GCZONE_MAPPING);

	/* Lock access to the mapping list. */
	GCLOCK(&gccontext->maplock);

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
			list_move(head, &gccontext->unmapvac);
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
			list_move(head, &gccontext->unmapvac);
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
		bvbuffmap->nextmap = gccontext->buffmapvac;
		gccontext->buffmapvac = bvbuffmap;
	}

	/* Unlock access to the mapping list. */
	GCUNLOCK(&gccontext->maplock);

	GCEXIT(GCZONE_MAPPING);
}
