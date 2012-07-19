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

#ifndef GCBV_H
#define GCBV_H

#include "gcmain.h"

/*******************************************************************************
 * Miscellaneous defines and macros.
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

#define BVSETERROR(error, message, ...) \
do { \
	struct gccontext *tmpcontext = get_context(); \
	snprintf(tmpcontext->bverrorstr, sizeof(tmpcontext->bverrorstr), \
		 message, ##__VA_ARGS__); \
	GCDUMPSTRING("%s(%d): [ERROR] %s\n", __func__, __LINE__, \
		     tmpcontext->bverrorstr); \
	bverror = error; \
} while (0)

#define BVSETBLTERROR(error, message, ...) \
do { \
	struct gccontext *tmpcontext = get_context(); \
	snprintf(tmpcontext->bverrorstr, sizeof(tmpcontext->bverrorstr), \
		 message, ##__VA_ARGS__); \
	GCDUMPSTRING("%s(%d): [ERROR] %s\n", __func__, __LINE__, \
		     tmpcontext->bverrorstr); \
	bvbltparams->errdesc = tmpcontext->bverrorstr; \
	bverror = error; \
} while (0)


/*******************************************************************************
 * Global data structure.
 */

struct gccontext {
	/* Last generated error message. */
	char bverrorstr[128];

	/* Dynamically allocated structure cache. */
	struct bvbuffmap *buffmapvac;		/* bvbuffmap */
	struct list_head unmapvac;		/* gcschedunmap */
	struct list_head buffervac;		/* gcbuffer */
	struct list_head fixupvac;		/* gcfixup */
	struct list_head batchvac;		/* gcbatch */

	/* Callback lists. */
	struct list_head callbacklist;		/* gccallbackinfo */
	struct list_head callbackvac;		/* gccallbackinfo */

	/* Access locks. */
	GCLOCK_TYPE batchlock;
	GCLOCK_TYPE bufferlock;
	GCLOCK_TYPE fixuplock;
	GCLOCK_TYPE maplock;
	GCLOCK_TYPE callbacklock;
};


/*******************************************************************************
 * Mapping structures.
 */

/* bvbuffmap struct attachment. */
struct bvbuffmapinfo {
	/* Mapped handle for the buffer. */
	unsigned long handle;

	/* Number of times the client explicitly mapped this buffer. */
	int usermap;

	/* Number of times implicit mapping happened. */
	int automap;
};


/*******************************************************************************
 * Color format.
 */

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


/*******************************************************************************
 * Alpha blending.
 */

/* Alpha blending hardware configuration. */
struct gcblendconfig {
	unsigned char factor_mode;
	unsigned char color_reverse;

	bool src1used;
	bool src2used;
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

	bool src1used;
	bool src2used;
};


/*******************************************************************************
 * Rotation and mirror defines.
 */

#define GCREG_ROT_ANGLE_ROT0	0x0
#define GCREG_ROT_ANGLE_ROT90	0x4
#define GCREG_ROT_ANGLE_ROT180	0x5
#define GCREG_ROT_ANGLE_ROT270	0x6

#define ROT_ANGLE_INVALID	-1
#define ROT_ANGLE_0		0
#define ROT_ANGLE_90		1
#define ROT_ANGLE_180		2
#define ROT_ANGLE_270		3

#define GCREG_MIRROR_NONE	0x0
#define GCREG_MIRROR_X		0x1
#define GCREG_MIRROR_Y		0x2
#define GCREG_MIRROR_XY		0x3

extern const unsigned int rotencoding[];


/*******************************************************************************
 * Batch structures.
 */

/* Operation finalization call. */
struct gcbatch;
typedef enum bverror (*gcbatchend) (struct bvbltparams *bvbltparams,
				    struct gcbatch *gcbatch);

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

	/* Command buffer list (gcbuffer). */
	struct list_head buffer;

	/* Scheduled implicit unmappings (gcschedunmap). */
	struct list_head unmap;

	/* Batch linked list (gcbatch). */
	struct list_head link;
};


/*******************************************************************************
 * srcinfo is used by blitters to define an array of valid sources.
 */

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


/*******************************************************************************
 * Internal API entries.
 */

/* Get the pointer to the context. */
struct gccontext *get_context(void);

/* Parsers. */
enum bverror parse_format(struct bvbltparams *bvbltparams,
			  enum ocdformat ocdformat,
			  struct bvformatxlate **format);
enum bverror parse_blend(struct bvbltparams *bvbltparams,
			 enum bvblend blend,
			 struct gcalpha *gca);
enum bverror parse_destination(struct bvbltparams *bvbltparams,
			       struct gcbatch *gcbatch);
enum bverror parse_source(struct bvbltparams *bvbltparams,
			  struct gcbatch *gcbatch,
			  struct srcinfo *srcinfo);

/* Return surface alignment offset. */
int get_pixel_offset(struct bvbuffdesc *bvbuffdesc,
		     struct bvformatxlate *format,
		     int offset);

/* Buffer mapping. */
enum bverror do_map(struct bvbuffdesc *bvbuffdesc,
		    struct gcbatch *gcbatch,
		    struct bvbuffmap **map);
void do_unmap_implicit(struct gcbatch *gcbatch);

/* Batch/command buffer management. */
enum bverror do_end(struct bvbltparams *bvbltparams,
		    struct gcbatch *gcbatch);
enum bverror allocate_batch(struct bvbltparams *bvbltparams,
			    struct gcbatch **gcbatch);
void free_batch(struct gcbatch *gcbatch);
enum bverror append_buffer(struct bvbltparams *bvbltparams,
			   struct gcbatch *gcbatch,
			   struct gcbuffer **gcbuffer);

enum bverror add_fixup(struct bvbltparams *bvbltparams,
		       struct gcbatch *gcbatch,
		       unsigned int *fixup,
		       unsigned int surfoffset);
enum bverror claim_buffer(struct bvbltparams *bvbltparams,
			  struct gcbatch *gcbatch,
			  unsigned int size,
			  void **buffer);

/* Program the destination. */
enum bverror set_dst(struct bvbltparams *bltparams,
		     struct gcbatch *batch,
		     struct bvbuffmap *dstmap);

/* Rendering entry points. */
enum bverror do_fill(struct bvbltparams *bltparams,
		     struct gcbatch *gcbatch,
		     struct srcinfo *srcinfo);
enum bverror do_blit(struct bvbltparams *bltparams,
		     struct gcbatch *gcbatch,
		     struct srcinfo *srcinfo);
enum bverror do_filter(struct bvbltparams *bvbltparams,
		       struct gcbatch *gcbatch,
		       struct srcinfo *srcinfo);

#endif
