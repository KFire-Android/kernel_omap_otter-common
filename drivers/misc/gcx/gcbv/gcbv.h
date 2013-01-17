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

#define GCBV_BATCH_FINALIZE_SRCCOUNT       (1 << 0)
#define GCBV_BATCH_FINALIZE_MULTISRC       (1 << 1)
#define GCBV_BATCH_FINALIZE_ALIGN          (1 << 2)
#define GCBV_BATCH_FINALIZE_FLAGS_DST      (1 << 3)
#define GCBV_BATCH_FINALIZE_FLAGS_DESTRECT (1 << 4)
#define GCBV_BATCH_FINALIZE_FLAGS_CLIPRECT (1 << 5)
#define GCBV_BATCH_FINALIZE_OPERATION      (1 << 6)

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

struct gccaps {
	bool l2cachefor420;
	unsigned int maxsource;
	bool strictalign;
	bool swizzlefixed;
};

struct gccontext {
	/* Last generated error message. */
	char bverrorstr[128];

	/* Capabilities and characteristics. */
	unsigned int gcmodel;
	unsigned int gcrevision;
	unsigned int gcdate;
	unsigned int gctime;
	struct gccaps gccaps;

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
	unsigned int endian;
	bool premultiplied;
	bool zerofill;

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
	bool globalcolorenable;
	unsigned int globalcolor;

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

struct gcrectset {
	/* Render rectangle as specified by the client. */
	struct gcrect orig;

	/* Clipped rectangle. */
	struct gcrect clip;

	/* Clipped rectangle adjusted for base address misalignment. */
	struct gcrect adj;
};

struct gcsurface {
	/* Surface change flags. */
	bool surfdirty;
	bool rectdirty;
	bool destrectdirty;
	bool cliprectdirty;

	/* BLTsville source index (-1 for dst, 0 for src1 and 1 for src2). */
	int index;

	/* Surface buffer descriptor. */
	union bvinbuff buf;

	/* Geometry size as specified by the client. */
	unsigned int width;
	unsigned int height;

	/* Geometry size adjusted for base address misalignment. */
	unsigned int adjwidth;
	unsigned int adjheight;

	/* Physical size of the surface (adjusted and 0 degree rotated). */
	unsigned int physwidth;
	unsigned int physheight;

	/* Plane strides. */
	long stride1;
	long stride2;
	long stride3;

	/* Base address alignment in pixels. */
	int xpixalign;
	int ypixalign;

	/* Base address alignment in bytes. */
	int bytealign1;
	int bytealign2;
	int bytealign3;

	/* Surface format. */
	struct bvformatxlate format;

	/* Rotation angle. */
	int angle;
	int adjangle;

	/* Render rectangles. */
	struct gcrectset rect;

	/* Aux render rectangles. */
	bool haveaux;
	struct gcrectset auxrect;

	/* Mirror setting. */
	unsigned int mirror;

	/* ROP. */
	unsigned short rop;

	/* Blending info. */
	struct gcalpha *gca;
	bool globalcolorenable;
	unsigned int globalcolor;
	unsigned char srcglobalpremul;
	unsigned char srcglobalmode;
	unsigned char dstglobalmode;
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
	bool multisrc;

	/* Computed destination rectangle coordinates; in multi-source
	 * setup can be modified to match new destination and source
	 * geometry. */
	struct gcrect dstrect;

	/* Block walker enable. */
	bool blockenable;

	/* Destination format and swizzle. */
	unsigned int format;
	unsigned int swizzle;
	unsigned int endian;
};

/* Filter states. */
struct gcfilter {
	/* Kernel size. */
	unsigned int horkernelsize;
	unsigned int verkernelsize;

	/* Scale factors. */
	unsigned int horscalefactor;
	unsigned int verscalefactor;
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
	struct gcsurface dstinfo;

	/* Clipping deltas; used to correct the source coordinates for
	 * single source blits. */
	struct gcrect clipdelta;

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
bool null_rect(struct gcrect *gcrect);
bool valid_rect(struct gcsurface *gcsurface, struct gcrect *gcrect);

/* Rotation processing. */
void rotate_rect(int angle,
		 struct gcsurface *gcsurface, struct gcrect *rect);
void rotate_geom(int angle, struct gcsurface *gcsurface);
void adjust_angle(struct gcsurface *srcinfo, struct gcsurface *dstinfo);
void process_rotation(struct gcsurface *gcsurface);

/* Parsers. */
enum bverror parse_format(struct bvbltparams *bvbltparams,
			  enum ocdformat ocdformat,
			  struct bvformatxlate *format);
enum bverror parse_blend(struct bvbltparams *bvbltparams,
			 enum bvblend blend,
			 struct gcalpha *gca);
enum bverror parse_destination(struct bvbltparams *bvbltparams,
			       struct gcbatch *gcbatch);
enum bverror parse_source(struct bvbltparams *bvbltparams,
			  struct gcbatch *batch,
			  struct gcsurface *srcinfo,
			  unsigned int index,
			  unsigned short rop);
enum bverror parse_scalemode(struct bvbltparams *bvbltparams,
			     struct gcbatch *batch);

/* Return surface alignment offset. */
int get_pixel_offset(struct gcsurface *gcsurface, int offset);

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
			  struct gcsurface *srcinfo);
enum bverror set_blending_index(struct bvbltparams *bvbltparams,
				struct gcbatch *batch,
				struct gcsurface *srcinfo,
				unsigned int index);

/* Program YUV source. */
void set_computeyuv(struct gcsurface *srcinfo, int x, int y);
enum bverror set_yuvsrc(struct bvbltparams *bvbltparams,
			struct gcbatch *batch,
			struct gcsurface *srcinfo,
			struct bvbuffmap *srcmap);
enum bverror set_yuvsrc_index(struct bvbltparams *bvbltparams,
			      struct gcbatch *batch,
			      struct gcsurface *srcinfo,
			      struct bvbuffmap *srcmap,
			      unsigned int index);

/* Rendering entry points. */
enum bverror do_fill(struct bvbltparams *bltparams,
		     struct gcbatch *gcbatch,
		     struct gcsurface *srcinfo);
enum bverror do_blit(struct bvbltparams *bltparams,
		     struct gcbatch *gcbatch,
		     struct gcsurface *srcinfo);
enum bverror do_filter(struct bvbltparams *bvbltparams,
		       struct gcbatch *gcbatch,
		       struct gcsurface *srcinfo);

#endif
