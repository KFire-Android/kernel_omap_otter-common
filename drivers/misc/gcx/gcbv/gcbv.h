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

#define GC_MAX_BASE_ALIGN 64

#if !defined(BVBATCH_DESTRECT)
#define BVBATCH_DESTRECT (BVBATCH_DSTRECT_ORIGIN | BVBATCH_DSTRECT_SIZE)
#endif

#if !defined(BVBATCH_SRC1RECT)
#define BVBATCH_SRC1RECT (BVBATCH_SRC1RECT_ORIGIN | BVBATCH_SRC1RECT_SIZE)
#endif

#if !defined(BVBATCH_SRC2RECT)
#define BVBATCH_SRC2RECT (BVBATCH_SRC2RECT_ORIGIN | BVBATCH_SRC2RECT_SIZE)
#endif

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

#define GCPRINT_RECT(zone, name, rect) \
{ \
	GCDBG(zone, \
	      name " = (%d,%d)-(%d,%d), %dx%d\n", \
	      (rect)->left, (rect)->top, \
	      (rect)->right, (rect)->bottom, \
	      (rect)->right - (rect)->left, \
	      (rect)->bottom - (rect)->top); \
}


/*******************************************************************************
 * Kernel table definitions.
 */

#define GC_TAP_COUNT		9
#define GC_PHASE_BITS		5
#define GC_PHASE_MAX_COUNT	(1 << GC_PHASE_BITS)
#define GC_PHASE_LOAD_COUNT	(GC_PHASE_MAX_COUNT / 2 + 1)
#define GC_COEFFICIENT_COUNT	(GC_PHASE_LOAD_COUNT * GC_TAP_COUNT)
#define GC_FILTER_CACHE_MAX	10

enum gcfiltertype {
	GC_FILTER_SYNC,
	GC_FILTER_BLUR,

	/* Number of supported filter types. */
	GC_FILTER_COUNT
};

struct gcfilterkernel {
	enum gcfiltertype type;
	unsigned int kernelsize;
	unsigned int srcsize;
	unsigned int dstsize;
	unsigned int scalefactor;
	short kernelarray[GC_COEFFICIENT_COUNT];
	struct list_head link;
};

struct gcfiltercache {
	unsigned int count;
	struct list_head list;			/* gcfilterkernel */
};


/*******************************************************************************
 * Global data structure.
 */

struct gccontext {
	/* Last generated error message. */
	char bverrorstr[128];

	/* Capabilities and characteristics. */
	unsigned int gcmodel;
	unsigned int gcrevision;
	unsigned int gcdate;
	unsigned int gctime;
	union gcfeatures gcfeatures;
	union gcfeatures0 gcfeatures0;
	union gcfeatures1 gcfeatures1;
	union gcfeatures2 gcfeatures2;
	union gcfeatures3 gcfeatures3;

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

	/* Kernel table cache. */
	struct gcfilterkernel *loadedfilter;	/* gcfilterkernel */
	struct gcfiltercache filtercache[GC_FILTER_COUNT][GC_TAP_COUNT];

	/* Temporary buffer descriptor. */
	struct bvbuffdesc *tmpbuffdesc;
	void *tmpbuff;
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

#define BVFMT_RGB	1
#define BVFMT_YUV	2

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
	unsigned int type;
	unsigned int bitspp;
	unsigned int allocbitspp;
	unsigned int format;
	unsigned int swizzle;
	bool premultiplied;

	union {
		struct {
			const struct bvcsrgb *comp;
		} rgb;

		struct {
			unsigned int std;
			unsigned int planecount;
			unsigned int xsample;
			unsigned int ysample;
		} yuv;
	} cs;
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
 * Surface descriptor.
 */

struct surfaceinfo {
	/* BLTsville source index (-1 for dst, 0 for src1 and 1 for src2). */
	int index;

	/* Surface buffer descriptor. */
	union bvinbuff buf;

	/* Surface geometry. */
	struct bvsurfgeom *geom;
	bool newgeom;

	/* Rectangle to source from/render to. */
	struct gcrect rect;
	bool newrect;

	/* Surface format. */
	struct bvformatxlate format;

	/* Physical size of the surface (accounted for rotation). */
	unsigned int physwidth;
	unsigned int physheight;

	/* Base address alignment. */
	int xpixalign;
	int ypixalign;
	int bytealign;
	int bytealign2;
	int bytealign3;
	int stride2;
	int stride3;

	/* Rotation angle. */
	int angle;

	/* Mirror setting. */
	unsigned int mirror;

	/* ROP. */
	unsigned short rop;

	/* Blending info. */
	struct gcalpha *gca;
};


/*******************************************************************************
 * Batch structures.
 */

/* Operation finalization call. */
struct gcbatch;
typedef enum bverror (*gcbatchend) (struct bvbltparams *bvbltparams,
				    struct gcbatch *gcbatch);

/* Blit states. */
struct gcblit {
	/* Number of sources in the operation. */
	unsigned int srccount;

	/* Multi source enable flag. */
	unsigned int multisrc;

	/* Computed destination rectangle coordinates; in multi-source
	 * setup can be modified to match new destination and source
	 * geometry. */
	struct gcrect dstrect;

	/* Block walker enable. */
	int blockenable;

	/* Destination format and swizzle */
	unsigned int format;
	unsigned int swizzle;
};

/* Filter states. */
struct gcfilter {
	/* Kernel size. */
	unsigned int horkernelsize;
	unsigned int verkernelsize;

	/* Scale factors. */
	unsigned int horscalefactor;
	unsigned int verscalefactor;

	/* Destination angle. */
	bool angleoverride;
	int dstangle;

	/* Geometry size that follows angle adjustments. */
	struct bvsurfgeom dstgeom;

	/* Original source and destination rectangles adjusted
	 * by the source angle. */
	struct gcrect dstrect;
	struct gcrect dstrectaux;

	/* Clipped destination rectangle adjusted by the source angle. */
	struct gcrect dstclipped;
	struct gcrect dstclippedaux;

	/* Destination rectangles that were clipped, adjusted for
	 * the surface misalignment and the source angle. */
	struct gcrect dstadjusted;
	struct gcrect dstadjustedaux;
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
	struct {
		struct gcblit blit;
		struct gcfilter filter;
	} op;

	/* Destination surface. */
	struct surfaceinfo dstinfo;

	/* Aux rectangle present. */
	bool haveaux;
	struct gcrect dstrectaux;

	/* Clipped destination rectangle coordinates. */
	struct gcrect dstclipped;
	struct gcrect dstclippedaux;

	/* Destination rectangles that were clipped and adjusted for
	 * surface misalignment if any. */
	struct gcrect dstadjusted;
	struct gcrect dstadjustedaux;

	/* Clipping deltas; used to correct the source coordinates for
	 * single source blits. */
	struct gcrect clipdelta;

	/* Adjusted geometry size of the destination surface. */
	unsigned int dstwidth;
	unsigned int dstheight;

	/* Physical size of the source and destination surfaces. */
	unsigned int srcphyswidth;
	unsigned int srcphysheight;
	unsigned int dstphyswidth;
	unsigned int dstphysheight;

	/* Alignment byte offset for the destination surface; in multi-
	 * source setup can be modified to match new destination and source
	 * geometry. */
	int dstbyteshift;

	/* Destination rectangle adjustment offsets. */
	int dstoffsetX;
	int dstoffsetY;

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
 * Internal API entries.
 */

/* Get the pointer to the context. */
struct gccontext *get_context(void);

/* Validation. */
bool valid_rect(struct bvsurfgeom *bvsurfgeom, struct gcrect *gcrect);

/* Parsers. */
enum bverror parse_format(struct bvbltparams *bvbltparams,
			  struct surfaceinfo *surfaceinfo);
enum bverror parse_blend(struct bvbltparams *bvbltparams,
			 enum bvblend blend,
			 struct gcalpha *gca);
enum bverror parse_destination(struct bvbltparams *bvbltparams,
			       struct gcbatch *gcbatch);
enum bverror parse_source(struct bvbltparams *bvbltparams,
			  struct gcbatch *gcbatch,
			  struct bvrect *srcrect,
			  struct surfaceinfo *srcinfo);
enum bverror parse_scalemode(struct bvbltparams *bvbltparams,
			     struct gcbatch *batch);

/* Setup destination rotation parameters. */
void process_dest_rotation(struct bvbltparams *bvbltparams,
			   struct gcbatch *batch);

/* Return surface alignment offset. */
int get_pixel_offset(struct surfaceinfo *surfaceinfo, int offset);

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

/* Temporary buffer management. */
enum bverror allocate_temp(struct bvbltparams *bvbltparams,
			   unsigned int size);
enum bverror free_temp(bool schedule);

/* Program the destination. */
enum bverror set_dst(struct bvbltparams *bltparams,
		     struct gcbatch *batch,
		     struct bvbuffmap *dstmap);

/* Program blending. */
enum bverror set_blending(struct bvbltparams *bvbltparams,
			  struct gcbatch *batch,
			  struct surfaceinfo *srcinfo);
enum bverror set_blending_index(struct bvbltparams *bvbltparams,
				struct gcbatch *batch,
				struct surfaceinfo *srcinfo,
				unsigned int index);

/* Program YUV source. */
void set_computeyuv(struct surfaceinfo *srcinfo, int x, int y);
enum bverror set_yuvsrc(struct bvbltparams *bvbltparams,
			struct gcbatch *batch,
			struct surfaceinfo *srcinfo,
			struct bvbuffmap *srcmap);
enum bverror set_yuvsrc_index(struct bvbltparams *bvbltparams,
			      struct gcbatch *batch,
			      struct surfaceinfo *srcinfo,
			      struct bvbuffmap *srcmap,
			      unsigned int index);

/* Rendering entry points. */
enum bverror do_fill(struct bvbltparams *bltparams,
		     struct gcbatch *gcbatch,
		     struct surfaceinfo *srcinfo);
enum bverror do_blit(struct bvbltparams *bltparams,
		     struct gcbatch *gcbatch,
		     struct surfaceinfo *srcinfo);
enum bverror do_filter(struct bvbltparams *bvbltparams,
		       struct gcbatch *gcbatch,
		       struct surfaceinfo *srcinfo);

#endif
