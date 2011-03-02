/*
 * omap_s3d_overlay.c
 *
 * Copyright (C) 2010 Texas Instruments.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * Leveraged code from V4L2 2D OMAP overlay driver
 *
 * Author: Alberto Aguirre (a-aguirre@ti.com)
 *
 * Copyright (C) 2004 MontaVista Software, Inc.
 * Copyright (C) 2010 Texas Instruments.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/videodev2.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/irq.h>

#include <media/videobuf-dma-contig.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>

#include <asm/processor.h>
#include <asm/cacheflush.h>
#include <plat/dma.h>
#include <plat/vram.h>
#include <plat/vrfb.h>
#include <plat/display.h>

#include "omap_voutlib.h"
#include "omap_s3d_overlaydef.h"

#ifdef CONFIG_TILER_OMAP
#include <mach/tiler.h>
#define TILER_ALLOCATE_V4L2
#endif

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("OMAP V4L2 S3D overlay driver");
MODULE_LICENSE("GPL");

#define QQVGA_WIDTH 160
#define QQVGA_HEIGHT 120

/* configuration macros */
#define S3D_OVL_DEV_NAME "s3d_overlay"

#define VID_MAX_WIDTH 2048
#define VID_MAX_HEIGHT 2048

/* Mimimum requirement is 2x2 for DSS */
#define VID_MIN_WIDTH 2
#define VID_MIN_HEIGHT 2

#define S3DERR(format, ...) \
	printk(KERN_ERR S3D_OVL_DEV_NAME ": " format, \
	## __VA_ARGS__)
#ifdef DEBUG
#define S3DINFO(format, ...) \
	printk(KERN_INFO S3D_OVL_DEV_NAME ": " format, \
	## __VA_ARGS__)

#define S3DWARN(format, ...) \
	printk(KERN_WARNING S3D_OVL_DEV_NAME ": " format, \
	## __VA_ARGS__)
#else
#define S3DINFO(format, ...)

#define S3DWARN(format, ...)
#endif

static int def_disp_id = -1;
module_param_named(disp_id, def_disp_id, int, S_IRUGO| S_IWUSR);

static struct s3d_ovl_device *s3d_overlay_dev;

/* list of image formats supported by OMAP4 video pipelines */
static const struct v4l2_fmtdesc omap_formats[] = {
	{
	 /* Note:  V4L2 defines RGB565 as:
	  *
	  *      Byte 0                    Byte 1
	  *      g2 g1 g0 r4 r3 r2 r1 r0   b4 b3 b2 b1 b0 g5 g4 g3
	  *
	  * We interpret RGB565 as:
	  *
	  *      Byte 0                    Byte 1
	  *      g2 g1 g0 b4 b3 b2 b1 b0   r4 r3 r2 r1 r0 g5 g4 g3
	  */
	 .description = "RGB565, le",
	 .pixelformat = V4L2_PIX_FMT_RGB565,
	 },
	{
	 /* Note:  V4L2 defines RGB32 as: RGB-8-8-8-8  we use
	  *  this for RGB24 unpack mode, the last 8 bits are ignored
	  * */
	 .description = "RGB32, le",
	 .pixelformat = V4L2_PIX_FMT_RGB32,
	 },
	{
	 /* Note:  V4L2 defines RGB24 as: RGB-8-8-8  we use
	  *        this for RGB24 packed mode
	  *
	  */
	 .description = "RGB24, le",
	 .pixelformat = V4L2_PIX_FMT_RGB24,
	 },
	{
	 .description = "YUYV (YUV 4:2:2), packed",
	 .pixelformat = V4L2_PIX_FMT_YUYV,
	 },
	{
	 .description = "UYVY, packed",
	 .pixelformat = V4L2_PIX_FMT_UYVY,
	 },
	{
	 .description = "NV12 - YUV420 format",
	 .pixelformat = V4L2_PIX_FMT_NV12,
	 },

};

#define NUM_OUTPUT_FORMATS (ARRAY_SIZE(omap_formats))

/* list of OMAP S3D private (non-standard) V4L2 driver controls */
static const struct v4l2_queryctrl s3d_private_controls[] = {
	{
	 .id = V4L2_CID_PRIVATE_DISPLAY_ID,
	 .type = V4L2_CTRL_TYPE_MENU,
	 .name = "Display",
	 .minimum = 0,
	 /* this is dynamically determined at driver init time */
	 .maximum = 0,
	 .step = 1,
	 /* this is dynamically determined at driver init time */
	 .default_value = 0,
	 .flags = 0,
	 .reserved = {0, 0},
	 },
	{
	 .id = V4L2_CID_PRIVATE_ANAGLYPH_TYPE,
	 .type = V4L2_CTRL_TYPE_MENU,
	 .name = "Anaglyph Type",
	 .minimum = 0,
	 .maximum = V4L2_ANAGLYPH_MAX - 1,
	 .step = 1,
	 .default_value = V4L2_ANAGLYPH_RED_CYAN,
	 .flags = 0,
	 .reserved = {0, 0},
	 },
	{
	 .id = V4L2_CID_PRIVATE_S3D_MODE,
	 .type = V4L2_CTRL_TYPE_MENU,
	 .name = "Mode Select",
	 .minimum = 0,
	 .maximum = V4L2_S3D_MODE_MAX - 1,
	 .step = 1,
	 .default_value = V4L2_S3D_MODE_OFF,
	 .flags = 0,
	 .reserved = {0, 0},
	 },
};

/* list of OMAP V4L2 driver control menus */
static const struct v4l2_querymenu s3d_querymenus[] = {
	{
	 .id = V4L2_CID_PRIVATE_ANAGLYPH_TYPE,
	 .index = V4L2_ANAGLYPH_RED_CYAN,
	 .name = "Red-Cyan",
	 .reserved = 0,
	 },
	{
	 .id = V4L2_CID_PRIVATE_ANAGLYPH_TYPE,
	 .index = V4L2_ANAGLYPH_RED_BLUE,
	 .name = "Red-Blue",
	 .reserved = 0,
	 },
	{
	 .id = V4L2_CID_PRIVATE_ANAGLYPH_TYPE,
	 .index = V4L2_ANAGLYPH_GR_MAG,
	 .name = "Green-Magenta",
	 .reserved = 0,
	 },
	{
	 .id = V4L2_CID_PRIVATE_S3D_MODE,
	 .index = V4L2_S3D_MODE_OFF,
	 .name = "2D Mode",
	 .reserved = 0,
	 },
	{
	 .id = V4L2_CID_PRIVATE_S3D_MODE,
	 .index = V4L2_S3D_MODE_ON,
	 .name = "Display-native 3D Mode",
	 .reserved = 0,
	 },
	{
	 .id = V4L2_CID_PRIVATE_S3D_MODE,
	 .index = V4L2_S3D_MODE_ANAGLYPH,
	 .name = "Anaglyph 3D Mode",
	 .reserved = 0,
	 },
};

static struct {
	/* displays count (determined at driver init time) */
	unsigned int count;
	/* list of available display names (used in vidioc_querymenu)
		allocated at driver init time */
	const char **name;
} display_list;

/*-------------------------------------------------------------------------
*Local function prototypes
*--------------------------------------------------------------------------
*/
static int alloc_buffers(struct s3d_ovl_device *dev,
			 struct s3d_buffer_queue *q, unsigned int *count);
static void free_buffers(struct s3d_buffer_queue *q);
static int vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i);

/*-------------------------------------------------------------------------
* Helper functions
*--------------------------------------------------------------------------
*/
static inline int rotation_90_or_270(const struct s3d_overlay *ovl)
{
	return (ovl->rotation == OMAP_DSS_ROT_90 ||
		ovl->rotation == OMAP_DSS_ROT_270);
}

static inline int rotation_enabled(const struct s3d_overlay *ovl)
{
	return ovl->rotation;
}

static inline bool anaglyph_enabled(const struct s3d_ovl_device *dev)
{
	return dev->s3d_mode == V4L2_S3D_MODE_ANAGLYPH;
}

static inline bool s3d_enabled(const struct s3d_ovl_device *dev)
{
	return dev->s3d_mode == V4L2_S3D_MODE_ON;
}

static int v4l2_rot_to_dss_rot(const unsigned int v4l2_rotation,
				enum omap_dss_rotation_angle *rotation)
{
	switch (v4l2_rotation) {
	case 90:
		*rotation = OMAP_DSS_ROT_90;
		break;
	case 180:
		*rotation = OMAP_DSS_ROT_180;
		break;
	case 270:
		*rotation = OMAP_DSS_ROT_270;
		break;
	case 0:
		*rotation = OMAP_DSS_ROT_0;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum omap_color_mode v4l2_pix_to_dss_color(struct v4l2_pix_format *pix)
{

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_NV12:
		return OMAP_DSS_COLOR_NV12;
	case 0:
		break;
	case V4L2_PIX_FMT_YUYV:
		return OMAP_DSS_COLOR_YUV2;
	case V4L2_PIX_FMT_UYVY:
		return OMAP_DSS_COLOR_UYVY;
	case V4L2_PIX_FMT_RGB565:
		return OMAP_DSS_COLOR_RGB16;
	case V4L2_PIX_FMT_RGB24:
		return OMAP_DSS_COLOR_RGB24P;
	case V4L2_PIX_FMT_RGB32:
		return OMAP_DSS_COLOR_ARGB32;
	case V4L2_PIX_FMT_BGR32:
		return OMAP_DSS_COLOR_RGBX32;

	default:
		return -EINVAL;
	}
	return -EINVAL;
}

static int try_format(struct v4l2_pix_format *pix)
{
	int ifmt, bpp = 0;

	pix->height = clamp(pix->height, (u32) VID_MIN_HEIGHT,
			    (u32) VID_MAX_HEIGHT);

	pix->width =
	    clamp(pix->width, (u32) VID_MIN_WIDTH, (u32) VID_MAX_WIDTH);

	for (ifmt = 0; ifmt < NUM_OUTPUT_FORMATS; ifmt++) {
		if (pix->pixelformat == omap_formats[ifmt].pixelformat)
			break;
	}

	if (ifmt == NUM_OUTPUT_FORMATS)
		ifmt = 0;

	pix->pixelformat = omap_formats[ifmt].pixelformat;
	pix->field = V4L2_FIELD_ANY;
	pix->priv = 0;

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	default:
		pix->colorspace = V4L2_COLORSPACE_JPEG;
		bpp = YUYV_BPP;
		break;
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		bpp = RGB565_BPP;
		break;
	case V4L2_PIX_FMT_RGB24:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		bpp = RGB24_BPP;
		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		bpp = RGB32_BPP;
		break;
	case V4L2_PIX_FMT_NV12:
		pix->colorspace = V4L2_COLORSPACE_JPEG;
		bpp = 1;
		break;
	}

	pix->bytesperline = pix->width * bpp;
	/*Round to the next page size for TILER buffers,
	* this will be the stride/vert pitch*/
	pix->bytesperline = (pix->bytesperline + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	pix->sizeimage = pix->bytesperline * pix->height;
	if (V4L2_PIX_FMT_NV12 == pix->pixelformat)
		pix->sizeimage += pix->sizeimage >> 1;

	return bpp;
}

static void set_default_crop(struct s3d_ovl_device *dev,
				struct v4l2_pix_format *pix,
				struct v4l2_rect *crop)
{
	crop->width = pix->width;
	crop->height = pix->height;
	crop->width &= ~1;
	crop->height &= ~1;
	crop->left = 0;
	crop->top = 0;
}

static void set_default_window(struct s3d_ovl_device *dev,
				struct v4l2_window *win)
{
	struct omap_video_timings *timings = &dev->cur_disp->panel.timings;
	win->w.width = timings->x_res;
	win->w.height = timings->y_res;
	win->w.top = 0;
	win->w.left = 0;
}

static int calculate_offset(const struct v4l2_rect *crop,
				struct s3d_buffer_queue *q, const int idx)
{
	unsigned long addr = 0, uv_addr = 0;

	addr = tiler_get_natural_addr((void *)q->phy_addr[idx]);
	q->offset[idx] = tiler_stride(addr) * crop->top + crop->left * q->bpp;
	if (OMAP_DSS_COLOR_NV12 == q->color_mode) {
		uv_addr = tiler_get_natural_addr((void *)q->phy_uv_addr[idx]);
		q->uv_offset[idx] = tiler_stride(uv_addr) * (crop->top >> 1)
		    + (crop->left & ~1);
	}
	/*TODO: use the s3d_offsets */

	return 0;
}

static inline enum s3d_view get_next_view(const struct s3d_ovl_device *dev,
						struct s3d_overlay *ovl)
{
	enum s3d_view view;

	/*For frame sequential panels we need to toggle the view
	 *fetched by the overlay.
	 *For multiple writeback passes, assumption is each pass
	 *only fetches one view, so it needs to be toggled as well */

	if (dev->fter_config.alt_view && ovl->state != OVL_ST_FETCH_ALL) {
		view = (ovl->state == OVL_ST_FETCH_L) ? S3D_VIEW_R : S3D_VIEW_L;
		ovl->state = (ovl->state == OVL_ST_FETCH_L) ?
					OVL_ST_FETCH_R : OVL_ST_FETCH_L;

	} else if (ovl->role == OVL_ROLE_L) {
		view = S3D_VIEW_L;
	} else if (ovl->role == OVL_ROLE_R) {
		view = S3D_VIEW_R;
	} else {
		view = S3D_VIEW_BASE;
	}

	return view;

}

static int get_view_address(const struct s3d_buffer_queue *q,
				const enum s3d_view view,
				const unsigned int idx,
				unsigned long *p_phy_addr,
				unsigned long *p_phy_uv_addr)
{
	unsigned long addr = q->phy_addr[idx]+q->offset[idx];
	unsigned long uv_addr = q->phy_uv_addr[idx]+q->uv_offset[idx];
	unsigned long stride =
	    (q->uses_tiler) ? tiler_stride(addr) : (q->width * q->bpp);
	unsigned long uv_stride =
	    (q->uses_tiler) ? tiler_stride(uv_addr) : (q->width * q->bpp);

	/*TODO: take into account custom LR offsets */
	if ((q->frame_packing.order == V4L2_FPACK_ORDER_LF &&
	     view == S3D_VIEW_R) ||
	    (q->frame_packing.order == V4L2_FPACK_ORDER_RF &&
	     view == S3D_VIEW_L)) {
		switch (q->frame_packing.type) {
		case V4L2_FPACK_OVERUNDER:
			/*height specifies the height of the two views */
			addr += (q->crop_h >> 1) * stride;
			if (uv_addr)
				uv_addr += (q->crop_h >> 2) * uv_stride;
			break;
		case V4L2_FPACK_SIDEBYSIDE:
			addr += (q->crop_w * q->bpp) >> 1;
			/*UV's are packed so they are the same width as the Y */
			if (uv_addr)
				uv_addr += (q->crop_w * q->bpp) >> 1;
			break;
		case V4L2_FPACK_ROW_IL:
			addr += stride;
			if (uv_addr)
				uv_addr += uv_stride;
			break;
		case V4L2_FPACK_COL_IL:
			addr += q->bpp;
			/*Bad idea to store S3D in column interleaved YUV422
			 * and 420 formats as it bleeds chroma between views,
			 * since uv are shared among left and right
			 */
			if (uv_addr)
				uv_addr += q->bpp;
			break;
		case V4L2_FPACK_NONE:
		default:
			break;
		}
	}

	*p_phy_addr = addr;
	*p_phy_uv_addr = uv_addr;
	S3DINFO("get_view_address - stride:%ld, q:0x%lx, addr:0x%lx\n, "
		"q.type:%d, view:%d, order:%d",
		stride, (unsigned long)q, addr, q->frame_packing.type, view,
		q->frame_packing.order);
	return 0;

}

static void s3d_update_disp_work(struct work_struct *work)
{
	struct s3d_disp_upd_work *w = container_of(work, typeof(*w), work);
	struct omap_dss_device *disp = w->disp;

	if (disp->driver->sched_update)
		disp->driver->sched_update(disp, 0, 0,
			disp->panel.timings.x_res,
			disp->panel.timings.y_res);
	else if (disp->driver->update)
		disp->driver->update(disp, 0, 0,
			disp->panel.timings.x_res,
			disp->panel.timings.y_res);
	kfree(w);
}

static void s3d_sched_disp_update(struct s3d_ovl_device *dev)
{
	struct s3d_disp_upd_work *w;

	/* queue process frame */
	w = kzalloc(sizeof(*w), GFP_ATOMIC);
	if (w) {
		w->disp = dev->cur_disp;
		INIT_WORK(&w->work, s3d_update_disp_work);
		queue_work(dev->workqueue, &w->work);
	} else
		S3DWARN("Failed to queue display update work\n");
}

static int apply(const struct s3d_ovl_device *dev,
			const struct s3d_overlay *ovl)
{
	struct omap_overlay *dssovl = ovl->dssovl;
	int r;

	if (!dssovl->manager || !dssovl->manager->device) {
		S3DERR("overlay manager or display device is not valid\n");
		return -EINVAL;
	}

	if (ovl->role != OVL_ROLE_WB)
		r = dssovl->manager->apply(dssovl->manager);
	else {
		struct omap_writeback *wb = omap_dss_get_wb(0);
		r = omap_dss_wb_apply(dssovl->manager, wb);
	}

	return r;
}

static int apply_changes(struct s3d_ovl_device *dev,
				const struct list_head *overlays)
{
	struct list_head *pos;
	struct s3d_overlay *ovl;
	bool update_disp = false;
	int r;

	list_for_each(pos, overlays) {
		ovl = list_entry(pos, struct s3d_overlay, list);
		if (ovl->update_dss) {
			r = apply(dev, ovl);
			if (r) {
				S3DERR("failed to apply changes(%d)\n", r);
				return r;
			}
			ovl->update_dss = false;
			if (ovl->role & OVL_ROLE_DISP)
				update_disp = true;
		}
	}

	/* if manual update mode display -> schedule update */
	if (update_disp && dssdev_manually_updated(dev->cur_disp))
		s3d_sched_disp_update(dev);

	return 0;
}

static int enable_overlay(const struct s3d_ovl_device *dev,
				struct s3d_overlay *ovl,
				bool enable)
{
	struct omap_overlay_info info;
	struct omap_overlay *dssovl = ovl->dssovl;
	int r;

	dssovl->get_overlay_info(ovl->dssovl, &info);
	if (info.enabled == enable)
		return 0;

	info.enabled = enable;
	r = dssovl->set_overlay_info(dssovl, &info);
	if (r) {
		S3DERR("could not set overlay info!\n");
		return r;
	}

	if (ovl->role == OVL_ROLE_WB && !enable) {
		struct omap_writeback *wb;
		struct omap_writeback_info wb_info;

		wb = omap_dss_get_wb(0);

		if (!wb) {
			S3DERR("could not obtain WB device\n");
			r = -EINVAL;
			return r;
		}

		wb->get_wb_info(wb, &wb_info);
		wb_info.enabled = false;
		r = wb->set_wb_info(wb, &wb_info);
	}
	ovl->update_dss = true;
	return 0;

}

static struct omap_overlay *find_free_overlay(struct s3d_ovl_device *dev)
{
	unsigned int i;
	struct omap_overlay *ovl;

	for (i = 1; i < omap_dss_get_num_overlays(); i++) {
		ovl = omap_dss_get_overlay(i);
		if ((ovl->supported_modes & dev->supported_modes) &&
			(ovl->caps & OMAP_DSS_OVL_CAP_SCALE)) {
			struct omap_overlay_info info;
			mutex_lock(&ovl->lock);
			ovl->get_overlay_info(ovl, &info);
			if (!ovl->in_use && !info.enabled) {
				ovl->in_use = true;
				mutex_unlock(&ovl->lock);
				return ovl;
			}
			mutex_unlock(&ovl->lock);
		}
	}
	return NULL;

}

static struct omap_dss_device *get_display(const unsigned int idx)
{
	struct omap_dss_device *dssdev = NULL;
	struct omap_dss_device *tmp = NULL;
	unsigned int count = 0;
	for_each_dss_dev(tmp) {
		if (count == idx) {
			dssdev = tmp;
			break;
		}
		++count;
	}
	return dssdev;
}

/* Caller holds the dev mutex */
static int change_display(struct s3d_ovl_device *dev,
				struct omap_dss_device *display)
{
	struct omap_overlay_manager *mgr;
	int r;

	if (!display) {
		S3DERR("null display\n");
		return -EINVAL;
	}

	if (dev->cur_disp == display)
		return 0;

	mgr = display->manager;
	if (!mgr) {
		int i = 0;

		S3DINFO("display '%s' has no manager\n", display->name);

		while ((mgr = omap_dss_get_overlay_manager(i++)))
			if (mgr->id == display->channel)
				break;

		if (mgr)
			S3DINFO("binding to manager '%s'\n", mgr->name);
		else {
			S3DERR("could not get manager\n");
			return -EINVAL;
		}
	}

	if (mgr->device != display) {
		omapdss_display_disable(display);
		if (mgr->device) {
			omapdss_display_disable(mgr->device);
			r = mgr->unset_device(mgr);
			if (r || mgr->device) {
				S3DERR("failed to unbind display from manager\n");
				return r;
			}
		}

		r = mgr->set_device(mgr, display);
		if (r) {
			S3DERR("failed to bind display to manager\n");
			return r;
		}

		r = mgr->apply(mgr);
		if (r) {
			S3DERR("failed to apply manager changes\n");
			return r;
		}
	}

	r = omapdss_display_enable(display);
	if (r) {
		S3DERR("failed to enable display '%s'\n", display->name);
		return r;
	}

	/*TODO: recalculate resources based on display type and allocate them */
	/* needs to be properly synchronized with ISR, assuring no lost videobufs */

	omap_dss_put_device(dev->cur_disp);
	dev->cur_disp = display;
	return 0;
}

static inline int bind_ovl2display(struct s3d_ovl_device *dev,
				   struct s3d_overlay *ovl,
				   struct omap_dss_device *display)
{
	struct omap_overlay_manager *mgr;
	int r;
	mgr = ovl->dssovl->manager;

	if (mgr == display->manager)
		return 0;

	r = enable_overlay(dev, ovl, false);
	if (r) {
		S3DERR("couldn't disable ovl\n");
		return r;
	}

	if (mgr) {
		r = ovl->dssovl->unset_manager(ovl->dssovl);
		if (r) {
			S3DERR("couldn't unbind manager from ovl\n");
			return r;
		}
	}

	r = ovl->dssovl->set_manager(ovl->dssovl, display->manager);
	if (r) {
		S3DERR("couldn't bind to manager\n");
		return r;
	}
	display->manager->apply(display->manager);
	return 0;

}

static inline enum s3d_disp_type s3d_disp_type(enum v4l2_frame_pack_type type)
{
	switch (type) {
	case V4L2_FPACK_NONE:
		return S3D_DISP_NONE;
	case V4L2_FPACK_OVERUNDER:
		return S3D_DISP_OVERUNDER;
	case V4L2_FPACK_SIDEBYSIDE:
		return S3D_DISP_SIDEBYSIDE;
	case V4L2_FPACK_ROW_IL:
		return S3D_DISP_ROW_IL;
	case V4L2_FPACK_COL_IL:
		return S3D_DISP_COL_IL;
	case V4L2_FPACK_PIX_IL:
		return S3D_DISP_PIX_IL;
	case V4L2_FPACK_CHECKB:
		return S3D_DISP_CHECKB;
	case V4L2_FPACK_FRM_SEQ:
		return S3D_DISP_FRAME_SEQ;
	default:
		return -EINVAL;
	}
}

static inline enum s3d_disp_order s3d_disp_order(enum v4l2_frame_pack_order order)
{
	switch (order) {
	case V4L2_FPACK_ORDER_LF:
		return S3D_DISP_ORDER_L;
	case V4L2_FPACK_ORDER_RF:
		return S3D_DISP_ORDER_R;
	default:
		return -EINVAL;
	}
}

static inline enum s3d_disp_sub_sampling s3d_disp_ss(
					enum v4l2_frame_pack_sub_sample ss)
{
	switch (ss) {
	case V4L2_FPACK_SS_NONE:
		return S3D_DISP_SUB_SAMPLE_NONE;
	case V4L2_FPACK_SS_VERT:
		return S3D_DISP_SUB_SAMPLE_V;
	case V4L2_FPACK_SS_HOR:
		return S3D_DISP_SUB_SAMPLE_H;
	default:
		return -EINVAL;
	}
}

static inline enum v4l2_frame_pack_type v4l2_s3d_type(enum s3d_disp_type type)
{
	switch (type) {
	case S3D_DISP_NONE:
		return V4L2_FPACK_NONE;
	case S3D_DISP_OVERUNDER:
		return V4L2_FPACK_OVERUNDER;
	case S3D_DISP_SIDEBYSIDE:
		return V4L2_FPACK_SIDEBYSIDE;
	case S3D_DISP_ROW_IL:
		return V4L2_FPACK_ROW_IL;
	case S3D_DISP_COL_IL:
		return V4L2_FPACK_COL_IL;
	case S3D_DISP_PIX_IL:
		return V4L2_FPACK_PIX_IL;
	case S3D_DISP_CHECKB:
		return V4L2_FPACK_CHECKB;
	case S3D_DISP_FRAME_SEQ:
		return V4L2_FPACK_FRM_SEQ;
	default:
		return -EINVAL;
	}
}

static inline enum v4l2_frame_pack_order v4l2_s3d_order(enum s3d_disp_order order)
{
	switch (order) {
	case S3D_DISP_ORDER_L:
		return V4L2_FPACK_ORDER_LF;
	case S3D_DISP_ORDER_R:
		return V4L2_FPACK_ORDER_RF;
	default:
		return -EINVAL;
	}
}

static inline enum v4l2_frame_pack_sub_sample v4l2_s3d_ss(
					enum s3d_disp_sub_sampling ss)
{
	switch (ss) {
	case S3D_DISP_SUB_SAMPLE_NONE:
		return V4L2_FPACK_SS_NONE;
	case S3D_DISP_SUB_SAMPLE_V:
		return V4L2_FPACK_SS_VERT;
	case S3D_DISP_SUB_SAMPLE_H:
		return V4L2_FPACK_SS_HOR;
	default:
		return -EINVAL;
	}
}



static int v4l2_fpack_to_disp_fpack(struct v4l2_frame_packing *frame_packing,
				struct s3d_disp_info *display_info)
{
	display_info->type = s3d_disp_type(frame_packing->type);
	display_info->order = s3d_disp_order(frame_packing->order);
	display_info->sub_samp = s3d_disp_ss(frame_packing->order);
	/*TODO: match gap to input*/
	display_info->gap = 0;
	return 0;
}

static int disp_fpack_to_v4l2_fpack(struct s3d_disp_info *display_info,
				struct v4l2_frame_packing *frame_packing)
{
	frame_packing->type = v4l2_s3d_type(display_info->type);
	frame_packing->order = v4l2_s3d_order(display_info->order);
	frame_packing->sub_samp = v4l2_s3d_ss(display_info->sub_samp);
	return 0;
}


static inline bool order_match(struct v4l2_frame_packing *frame_packing,
				struct s3d_disp_info *disp_info)
{
	return s3d_disp_order(frame_packing->order) == disp_info->order;
}

static bool fpack_match(struct v4l2_frame_packing *frame_packing,
				struct s3d_disp_info *disp_info)
{
	struct s3d_disp_info fpack;
	v4l2_fpack_to_disp_fpack(frame_packing, &fpack);

	return (fpack.type == disp_info->type) &&
		(fpack.order == disp_info->order) &&
		(fpack.sub_samp == disp_info->sub_samp) &&
		(fpack.gap == disp_info->gap);
}

static int fill_src(struct s3d_ovl_device *dev,
			struct s3d_formatter_config *info,
			struct v4l2_frame_packing *fpack,
			unsigned int src_width,
			unsigned int src_height)
{
	switch (fpack->type) {
	case V4L2_FPACK_NONE:
	case V4L2_FPACK_FRM_SEQ:
		info->src_w = src_width;
		info->src_h = src_height;
		break;
	case V4L2_FPACK_OVERUNDER:
	case V4L2_FPACK_ROW_IL:
		info->src_w = src_width;
		info->src_h = src_height/2;
		break;
	case V4L2_FPACK_COL_IL:
	case V4L2_FPACK_SIDEBYSIDE:
	case V4L2_FPACK_CHECKB:
		info->src_w = src_width/2;
		info->src_h = src_height;
		break;
	case V4L2_FPACK_PIX_IL:
	default:
		return -EINVAL;
	}

	return 0;
}
static int fill_dst(struct s3d_ovl_device *dev,
			struct s3d_formatter_config *info,
			struct s3d_disp_info *disp_info,
			unsigned int dst_width,
			unsigned int dst_height)
{
	unsigned int pos_x = dev->win.w.left, pos_y = dev->win.w.top;

	info->r_pos_x = pos_x;
	info->r_pos_y = pos_y;
	info->l_pos_x = pos_x;
	info->l_pos_y = pos_y;

	switch (disp_info->type) {
	case S3D_DISP_NONE:
	case S3D_DISP_FRAME_SEQ:
		info->dst_h = dst_height;
		info->dst_w = dst_width;
		info->disp_ovls = 1;
		info->alt_view = (disp_info->type == S3D_DISP_FRAME_SEQ);
		break;
	case S3D_DISP_ROW_IL:
	case S3D_DISP_COL_IL:
		info->dst_w = dst_width;
		info->dst_h = dst_height/2;
		info->wb_dst_w = info->dst_w;
		info->wb_dst_h = info->dst_h;
		info->use_wb = true;
		info->wb_skip_lines = true;
		info->wb_needs_tiler = true;
		info->wb_passes = 2;
		info->alt_view = true;
		info->disp_ovls = 1;
		info->wb_ovls = 1;
		break;
	case S3D_DISP_OVERUNDER:
		info->dst_w = dst_width;
		info->dst_h = (dst_height - disp_info->gap) / 2;
		pos_y += (dst_height + disp_info->gap) / 2;
		info->disp_ovls = 2;
		info->a_view_per_ovl = true;
		break;
	case S3D_DISP_SIDEBYSIDE:
		info->dst_w = (dst_width - disp_info->gap) / 2;
		info->dst_h = dst_height;
		pos_x += (dst_width + disp_info->gap) / 2;
		info->disp_ovls = 2;
		info->a_view_per_ovl = true;
		break;
	case S3D_DISP_PIX_IL:
	case S3D_DISP_CHECKB:
	default:
		return -EINVAL;
	}

	if (disp_info->order == S3D_DISP_ORDER_L) {
		info->r_pos_x = pos_x;
		info->r_pos_y = pos_y;
	} else {
		info->l_pos_x = pos_x;
		info->l_pos_y = pos_y;
	}

	return 0;

}

static int fill_formatter_config(struct s3d_ovl_device *dev,
				struct s3d_formatter_config *info,
				struct v4l2_frame_packing *fpack,
				struct s3d_disp_info *disp_info,
				enum v4l2_s3d_mode s3d_mode)
{
	unsigned int src_width;
	unsigned int src_height;
	unsigned int dst_width = dev->win.w.width;
	unsigned int dst_height = dev->win.w.height;
	bool anaglyph = anaglyph_enabled(dev);
	int r;

	memset(info, 0, sizeof(*info));

	if (dev->rotation == 90 || dev->rotation == 270) {
		src_width = dev->crop.height;
		src_height = dev->crop.width;
	} else {
		src_width = dev->crop.width;
		src_height = dev->crop.height;
	}

	S3DINFO("formatter config - crop.w:%d, crop.h:%d, win.w:%d, win.h:%d\n",
		src_width, src_height, dst_width, dst_height);

	if (anaglyph && fpack->type == V4L2_FPACK_NONE) {
		S3DERR("Anaglyph is not supported with 2D content");
		return -EINVAL;
	}

	info->disp_w = dst_width;
	info->disp_h = dst_height;
	info->in_rotation = dev->rotation;
	info->out_rotation = 0;
	info->r_pos_x = dev->win.w.left;
	info->r_pos_y = dev->win.w.top;
	info->l_pos_x = dev->win.w.left;
	info->l_pos_y = dev->win.w.top;

	/*If the input matches what the display expects just feed it, no
	 * special processing is needed */
	if (fpack_match(fpack, disp_info) && dev->rotation == 0) {
		info->disp_ovls = 1;
		info->src_h = src_height;
		info->src_w = src_width;
		info->dst_h = dst_height;
		info->dst_w = dst_width;
		return 0;
	}

	/*Corner cases where there's an optimal configuration*/
	if (disp_info->type == S3D_DISP_ROW_IL && !anaglyph) {
		bool same_order = order_match(fpack, disp_info);
		bool sbs = fpack->type == V4L2_FPACK_SIDEBYSIDE;
		bool ovu = fpack->type == V4L2_FPACK_OVERUNDER;

		if ((same_order && sbs && dev->rotation == 0) ||
			(same_order && ovu && dev->rotation == 90) ||
			(!same_order && ovu && dev->rotation == 270)) {

			info->src_w = src_width;
			info->src_h = src_height;
			info->dst_w = dst_width*2;
			info->dst_h = dst_height/2;
			info->wb_dst_w = info->dst_w;
			info->wb_dst_h = info->dst_h;
			info->disp_ovls = 1;
			info->wb_ovls = 1;
			info->wb_passes = 1;
			info->use_wb = true;
			return 0;
		}

	}

	if (disp_info->type == S3D_DISP_COL_IL && !anaglyph) {
		switch (dev->rotation) {
		case 0:
			info->in_rotation = 90;
			info->out_rotation = 270;
			break;
		case 90:
			info->in_rotation = 0;
			info->out_rotation = 90;
			break;
		case 270:
			info->in_rotation = 0;
			info->out_rotation = 270;
			break;
		case 180:
			info->in_rotation = 90;
			info->out_rotation = 90;
			break;
		}
	}

	r = fill_src(dev, info, fpack, dev->crop.width, dev->crop.height);
	if (info->in_rotation == 90 || info->in_rotation == 270) {
		unsigned int tmp = info->src_w;
		info->src_w = info->src_h;
		info->src_h = tmp;
	}

	if (anaglyph) {
		info->disp_ovls = 2;
		info->a_view_per_ovl = true;
		info->dst_h = dst_height;
		info->dst_w = dst_width;
		return 0;
	}

	if (info->out_rotation == 90 || info->out_rotation == 270) {
		dst_width = dev->win.w.height;
		dst_height = dev->win.w.width;
	}

	r = fill_dst(dev, info, disp_info, dst_width, dst_height);

	disp_fpack_to_v4l2_fpack(disp_info, &dev->out_q.frame_packing);

	/*Special case, for column interleaving we technically row interleave
	  and use input/output rotation to appear as column interleaving*/
	if (disp_info->type == S3D_DISP_COL_IL)
		dev->out_q.frame_packing.type = V4L2_FPACK_ROW_IL;
	return r;

}

static int init_overlay(struct s3d_ovl_device *dev, struct s3d_overlay *ovl)
{
	struct omap_overlay_info *info;

	info = &ovl->dssovl->info;

	if (anaglyph_enabled(dev)) {
		if (!info->yuv2rgb_conv.weight_coef) {
			info->yuv2rgb_conv.weight_coef =
				kzalloc(sizeof(struct \
					omap_dss_color_weight_coef),
					GFP_KERNEL);
			if (!info->yuv2rgb_conv.weight_coef) {
				S3DERR("could not allocate memory for color "
					"weight coef\n");
				return -ENOMEM;
			}
		}
		memset(info->yuv2rgb_conv.weight_coef, 0,
			sizeof(*info->yuv2rgb_conv.weight_coef));
	} else if (info->yuv2rgb_conv.weight_coef) {
		kfree(info->yuv2rgb_conv.weight_coef);
		info->yuv2rgb_conv.weight_coef = NULL;
	}

	ovl->alpha = dev->win.global_alpha;
	ovl->vflip = dev->vflip;
	if (ovl->role == OVL_ROLE_WB ||
		((ovl->role & OVL_ROLE_DISP) && !dev->fter_config.use_wb)) {
		/*TODO: account for custom s3d offsets (gap between l r) */
		v4l2_rot_to_dss_rot(dev->fter_config.in_rotation, &ovl->rotation);
		ovl->queue = &dev->in_q;
		ovl->wb_queue = &dev->out_q;

		ovl->src.w = dev->fter_config.src_w;
		ovl->src.h = dev->fter_config.src_h;
		ovl->dst.w = dev->fter_config.dst_w;
		ovl->dst.h = dev->fter_config.dst_h;

		if (ovl->role == OVL_ROLE_R) {
			ovl->dst.x = dev->fter_config.r_pos_x;
			ovl->dst.y = dev->fter_config.r_pos_y;
			ovl->state = OVL_ST_FETCH_R;

			if (anaglyph_enabled(dev)) {
				info->zorder = OMAP_DSS_OVL_ZORDER_2;
				ovl->alpha = dev->win.global_alpha / 2;
				switch (dev->anaglyph_type) {
				case V4L2_ANAGLYPH_GR_MAG:
					info->yuv2rgb_conv.weight_coef->rr =
						1000;
					info->yuv2rgb_conv.weight_coef->bb =
						1000;
					break;
				case V4L2_ANAGLYPH_RED_BLUE:
					info->yuv2rgb_conv.weight_coef->bb =
						1000;
					break;
				default:
					info->yuv2rgb_conv.weight_coef->gg =
						1000;
					info->yuv2rgb_conv.weight_coef->bb =
						1000;
				}
			}
		} else {/*Left or LR*/
			ovl->dst.x = dev->fter_config.l_pos_x;
			ovl->dst.y = dev->fter_config.l_pos_y;
		}

		if (ovl->role == OVL_ROLE_L) {
			ovl->state = OVL_ST_FETCH_L;

			if (anaglyph_enabled(dev)) {
				info->zorder = OMAP_DSS_OVL_ZORDER_1;
				switch (dev->anaglyph_type) {
				case V4L2_ANAGLYPH_GR_MAG:
					info->yuv2rgb_conv.weight_coef->gg =
						1000;
					break;
				case V4L2_ANAGLYPH_RED_CYAN:
				case V4L2_ANAGLYPH_RED_BLUE:
				default:
					info->yuv2rgb_conv.weight_coef->rr =
						1000;
				}
			}
		} else if (ovl->role == OVL_ROLE_DISP) {
			/*If we are flipping views, we have to start with the
			*expected view by the panel. Here it's setup oppposite
			*since get_next_view will flip it*/
			enum s3d_ovl_state state;
			state = dev->cur_disp->panel.s3d_info.order ==
				S3D_DISP_ORDER_L ? OVL_ST_FETCH_R : OVL_ST_FETCH_L;
			ovl->state = dev->fter_config.alt_view ? state : OVL_ST_FETCH_ALL;
		}

	} else if ((ovl->role & OVL_ROLE_DISP) && dev->fter_config.use_wb) {
		v4l2_rot_to_dss_rot(dev->fter_config.out_rotation, &ovl->rotation);
		ovl->queue = &dev->out_q;
		ovl->src.w = dev->fter_config.disp_w;
		ovl->src.h = dev->fter_config.disp_h;
		ovl->dst.w = ovl->src.w;
		ovl->dst.h = ovl->src.h;
		ovl->dst.x = dev->fter_config.r_pos_x;
		ovl->dst.y = dev->fter_config.r_pos_y;
		ovl->state = OVL_ST_FETCH_ALL;
	}

	info->yuv2rgb_conv.dirty = true;

	S3DINFO("init_ovl - role:0x%x, src.w:%d, src.h:%d, dst.w:%d, dst.h:%d "
		"dst.x:%d, dst.y:%d\n", ovl->role, ovl->src.w, ovl->src.h,
		ovl->dst.w, ovl->dst.h, ovl->dst.x, ovl->dst.y);

	return bind_ovl2display(dev, ovl, dev->cur_disp);
}

static int init_overlays(struct s3d_ovl_device *dev,
				const struct list_head *overlays)
{
	int r;
	struct list_head *pos;
	struct s3d_overlay *ovl;

	list_for_each(pos, overlays) {
		ovl = list_entry(pos, struct s3d_overlay, list);
		r = init_overlay(dev, ovl);
		if (r)
			return r;
	}
	return 0;

}

static int enable_overlays(const struct s3d_ovl_device *dev,
				const struct list_head *overlays,
				enum s3d_overlay_role role, bool enable)
{
	int r = 0;
	struct list_head *pos;
	struct s3d_overlay *ovl;

	list_for_each(pos, overlays) {
		ovl = list_entry(pos, struct s3d_overlay, list);
		if (!(role & ovl->role))
			continue;
		r = enable_overlay(dev, ovl, enable);
		if (r && enable) {
			S3DERR("failed to enable overlay\n");
			return r;
		} else if (r && !enable) {
			/*When disabling, walk through the whole list */
			S3DERR("failed to disable overlay\n");
			continue;
		} else {
			r = apply(dev, ovl);
			if (r)
				S3DERR("failed to apply configuration\n");
		}

	}

	return r;
}

static int conf_overlay_info(const struct s3d_ovl_device *dev,
				struct s3d_overlay *ovl,
				const unsigned int buf_idx)
{
	int r;
	struct omap_overlay *dssovl;
	struct omap_overlay_info info;
	unsigned long addr;
	unsigned long uv_addr;
	enum s3d_disp_view view;

	dssovl = ovl->dssovl;

	if (((ovl->dssovl->caps & OMAP_DSS_OVL_CAP_SCALE) == 0) &&
	    ((ovl->src.w != ovl->dst.w) || (ovl->src.h != ovl->dst.h))) {
		S3DERR("overlay does not support scaling\n");
		return -EINVAL;
	}

	view = get_next_view(dev, ovl);
	get_view_address(ovl->queue, view, buf_idx, &addr, &uv_addr);

	S3DINFO("conf - role:%d, addr: 0x%lx, uv_addr:0x%lx\n",
		ovl->role, addr, uv_addr);

	dssovl->get_overlay_info(ovl->dssovl, &info);

	info.enabled = true;
	info.paddr = addr;
	info.p_uv_addr = uv_addr;

	info.rotation = ovl->rotation;
	info.rotation_type = ovl->queue->uses_tiler ?
				OMAP_DSS_ROT_TILER : OMAP_DSS_ROT_DMA;

	info.vaddr = NULL;
	info.width = ovl->src.w;
	info.height = ovl->src.h;
	info.color_mode = ovl->queue->color_mode;
	info.mirror = ovl->vflip;
	info.pos_x = ovl->dst.x;
	info.pos_y = ovl->dst.y;
	info.out_width = ovl->dst.w;
	info.out_height = ovl->dst.h;
	info.global_alpha = ovl->alpha;
	info.out_wb = (ovl->role == OVL_ROLE_WB);
	info.screen_width =
	    rotation_90_or_270(ovl) ? ovl->queue->height : ovl->queue->width;

	S3DINFO("conf - width:%d, height:%d, out_w:%d, out_h:%d, "
		"x:%d, y:%d, rot:%d\n",
		info.width, info.height, info.out_width, info.out_height,
		info.pos_x, info.pos_y, info.rotation);

	r = dssovl->set_overlay_info(dssovl, &info);
	if (r) {
		S3DERR("could not set overlay info!\n");
		return r;
	}
	if (ovl->role == OVL_ROLE_WB) {
		struct omap_writeback *wb;
		struct omap_writeback_info wb_info;
		unsigned long wb_addr;
		unsigned long wb_uv_addr;

		wb = omap_dss_get_wb(0);
		if (!wb) {
			S3DERR("could not obtain WB device\n");
			r = -EINVAL;
			return r;
		}

		wb->get_wb_info(wb, &wb_info);
		get_view_address(ovl->wb_queue, view, buf_idx, &wb_addr, &wb_uv_addr);
		S3DINFO("conf - wb_addr: 0x%lx uv:0x%lx\n", wb_addr, wb_uv_addr);
		wb_info.enabled = true;
		wb_info.capturemode = OMAP_WB_CAPTURE_ALL;
		wb_info.dss_mode = ovl->wb_queue->color_mode;
		wb_info.height = ovl->dst.h;
		wb_info.width = ovl->dst.w;
		wb_info.out_height = dev->fter_config.wb_dst_h;
		wb_info.out_width = dev->fter_config.wb_dst_w;
		wb_info.source = ovl->dssovl->id + 3;
		wb_info.source_type = OMAP_WB_SOURCE_OVERLAY;
		wb_info.paddr = wb_addr;
		wb_info.puv_addr = wb_uv_addr;
		wb_info.line_skip = dev->fter_config.wb_skip_lines ? 1 : 0;

		S3DINFO("conf - WB width:%ld, height:%ld, out_w:%ld, out_h:%ld, "
			"skip_lines:%d\n",
			wb_info.width, wb_info.height, wb_info.out_width,
			wb_info.out_height, wb_info.line_skip);

		r = wb->set_wb_info(wb, &wb_info);
	}
	ovl->update_dss = true;
	return r;

}

static int conf_overlays(const struct s3d_ovl_device *dev,
				const struct list_head *overlays,
				const unsigned buf_idx,
				enum s3d_overlay_role role)
{
	int r;
	struct list_head *pos;
	struct s3d_overlay *ovl;

	list_for_each(pos, overlays) {
		ovl = list_entry(pos, struct s3d_overlay, list);
		if (!(role & ovl->role))
			continue;
		r = conf_overlay_info(dev, ovl, buf_idx);
		if (r) {
			S3DERR("failed to configure overlay\n");
			return r;
		}
	}
	return 0;
}

static void assign_role_to_overlays(struct s3d_ovl_device *dev,
					struct list_head *overlays)
{
	struct list_head *pos;
	struct s3d_overlay *ovl;

	unsigned int num_wb = dev->fter_config.wb_ovls;
	bool one_ovly_per_view = dev->fter_config.a_view_per_ovl;
	enum s3d_overlay_role prev_role = OVL_ROLE_NONE;

	list_for_each(pos, overlays) {
		ovl = list_entry(pos, struct s3d_overlay, list);
		if (num_wb > 0) {
			ovl->role = OVL_ROLE_WB;
			num_wb--;
			continue;
		}

		if (one_ovly_per_view && prev_role == OVL_ROLE_NONE)
			ovl->role = OVL_ROLE_L;
		else if (one_ovly_per_view && prev_role != OVL_ROLE_NONE)
			ovl->role = OVL_ROLE_R;
		else
			ovl->role = OVL_ROLE_DISP;

		prev_role = ovl->role;

	}
}

static int free_overlays(const struct s3d_ovl_device *dev,
				struct list_head *overlays)
{
	struct list_head *pos, *n;
	struct s3d_overlay *ovl;
	struct omap_overlay *dssovl;
	struct omap_overlay_info info;

	list_for_each_safe(pos, n, overlays) {
		ovl = list_entry(pos, struct s3d_overlay, list);
		list_del(pos);
		mutex_lock(&ovl->dssovl->lock);
		dssovl = ovl->dssovl;
		dssovl->get_overlay_info(dssovl, &info);
		if (info.yuv2rgb_conv.weight_coef) {
			kfree(info.yuv2rgb_conv.weight_coef);
			info.yuv2rgb_conv.weight_coef = NULL;
		}
		info.yuv2rgb_conv.dirty = true;
		info.global_alpha = 255;
		dssovl->in_use = false;
		dssovl->set_overlay_info(dssovl, &info);
		mutex_unlock(&ovl->dssovl->lock);
		kfree(ovl);
	}

	return 0;
}

static int allocate_overlays(struct s3d_ovl_device *dev,
				struct list_head *overlays,
				const unsigned int count)
{
	unsigned int i;
	struct s3d_overlay *s3d_ovl;
	for (i = 0; i < count; i++) {
		s3d_ovl = kzalloc(sizeof(*s3d_ovl), GFP_KERNEL);
		if (!s3d_ovl) {
			free_overlays(dev, &dev->overlays);
			return -ENOMEM;
		}
		s3d_ovl->dssovl = find_free_overlay(dev);
		if (s3d_ovl->dssovl == NULL) {
			S3DERR("failed to allocate overlay\n");
			kfree(s3d_ovl);
			free_overlays(dev, &dev->overlays);
			return -ENOMEM;
		}
		list_add_tail(&s3d_ovl->list, overlays);

	}
	return 0;
}

static int reallocate_resources(struct s3d_ovl_device *dev)
{
	/*TODO: intelligently browse through what has been allocated
	   already and reconfigure it */
	return 0;
}

/*Caller holds dev mutex*/
static int allocate_resources(struct s3d_ovl_device *dev)
{
	unsigned int num_ovls;
	struct s3d_disp_info *disp_info;

	memset(&dev->out_q, 0, sizeof(dev->out_q));

	disp_info = dev->override_s3d_disp ?
			&dev->s3d_disp_info : &dev->cur_disp->panel.s3d_info;

	if (fill_formatter_config(dev, &dev->fter_config,
					&dev->in_q.frame_packing,
					disp_info,
					dev->s3d_mode)) {
		S3DERR("failed to fill formatting info\n");
		return -EINVAL;
	}

	if (dev->fter_config.use_wb) {
		unsigned int disp_w;
		unsigned int disp_h;
		if (dev->fter_config.out_rotation == 90 ||
			dev->fter_config.out_rotation == 270) {
			disp_w = dev->fter_config.disp_h;
			disp_h = dev->fter_config.disp_w;
		} else {
			disp_w = dev->fter_config.disp_w;
			disp_h = dev->fter_config.disp_h;
		}
		dev->out_q.bpp = RGB32_BPP;
		dev->out_q.color_mode = OMAP_DSS_COLOR_RGBA32;
		dev->out_q.width = disp_w;
		dev->out_q.height = disp_h;
		dev->out_q.crop_w = dev->out_q.width;
		dev->out_q.crop_h = dev->out_q.height;
		dev->out_q.uses_tiler = dev->fter_config.wb_needs_tiler;
		if (dev->out_q.n_alloc > 0) {
			S3DWARN("internal buffers exist\n");
			free_buffers(&dev->out_q);
		}
		if (alloc_buffers(dev, &dev->out_q, &dev->in_q.n_alloc)) {
			S3DERR("failed to allocate internal buffers\n");
			return -ENOMEM;
		}
	}

	num_ovls = dev->fter_config.wb_ovls + dev->fter_config.disp_ovls;
	if (allocate_overlays(dev, &dev->overlays, num_ovls)) {
		S3DERR("failed to allocate overlays(%d)\n", num_ovls);
		return -ENOMEM;
	}

	assign_role_to_overlays(dev, &dev->overlays);

	return 0;

}

static int free_resources(struct s3d_ovl_device *dev)
{
	free_overlays(dev, &dev->overlays);
	free_buffers(&dev->out_q);

	return 0;
}

static int toggle_driver_view(struct s3d_ovl_device *dev)
{
	struct s3d_overlay *ovl;
	struct omap_dss_device *disp = dev->cur_disp;
	enum s3d_disp_view view;

	if (!disp->driver->set_s3d_view)
		return -EINVAL;

	list_for_each_entry(ovl, &dev->overlays, list) {
		if (!(ovl->role & OVL_ROLE_DISP))
			continue;
		view = (ovl->state == OVL_ST_FETCH_L) ?
				S3D_DISP_VIEW_L : S3D_DISP_VIEW_R;
		disp->driver->set_s3d_view(disp, view);
		return 0;
	}
	return -EINVAL;
}
/* Caller holds dev mutex*/
static int change_s3d_mode(struct s3d_ovl_device *dev,
				enum v4l2_s3d_mode mode)
{
	struct omap_dss_device *disp;
	bool enable_s3d;
	int r = 0;

	/*TODO: synchronize correctly with queuing and ISR */
	if (dev->streaming) {

		if (reallocate_resources(dev)) {
			S3DERR("failed to reallocate resources\n");
			return -EINVAL;
		}
	}

	disp = dev->cur_disp;
	enable_s3d = (mode == V4L2_S3D_MODE_ON) && !dev->override_s3d_disp;
	if (!disp) {
		WARN_ON(1);
		S3DERR("NULL display pointer!\n");
		return -EFAULT;
	}
	if (disp->driver && disp->driver->enable_s3d) {
		r = disp->driver->enable_s3d(dev->cur_disp, enable_s3d);
		if (enable_s3d && r) {
			S3DWARN("failed to enable S3D display\n");
			/*fallback to anaglyph mode*/
			mode = dev->s3d_mode = V4L2_S3D_MODE_ANAGLYPH;
		}
	}

	if (disp->panel.s3d_info.type == S3D_DISP_NONE &&
		mode == V4L2_S3D_MODE_ON &&
		!dev->override_s3d_disp)
		mode = dev->s3d_mode = V4L2_S3D_MODE_ANAGLYPH;

	if (mode == V4L2_S3D_MODE_ANAGLYPH) {
		struct omap_overlay_manager_info info;
		struct omap_overlay_manager *mgr = disp->manager;
		if (mgr && mgr->get_manager_info && mgr->set_manager_info) {
			mgr->get_manager_info(mgr, &info);
			info.alpha_enabled = false;
			r = mgr->set_manager_info(mgr, &info);
			if (r)
				S3DERR("failed to set alpha blender\n");
		} else {
			S3DERR("invalid manager");
			r = -EINVAL;
		}
	}

	return r;
}

static int wb_process_buffer(struct s3d_ovl_device *dev,
				struct videobuf_buffer *buf)
{
	int r;
	dev->fter_info.pend_wb_passes = dev->fter_config.wb_passes;
	dev->fter_info.wb_cur_buf = buf;
	dev->fter_info.wb_state = WB_BUSY;
	S3DINFO("WB process buffer:%d\n", buf->i);
	r = conf_overlays(dev, &dev->overlays, buf->i, OVL_ROLE_WB);
	if (r) {
		S3DERR("failed to configure WB overlay\n");
		return r;
	}
	r = apply_changes(dev, &dev->overlays);
	if (r)
		S3DERR("failed to apply changes to WB path\n");

	return r;
}

static int wb_kick(struct s3d_ovl_device *dev)
{
	struct wb_buffer *buf;
	if (list_empty(&dev->fter_info.wb_videobuf_q)) {
		S3DINFO("WB: nothing to kick\n");
		dev->fter_info.wb_state = WB_IDLE;
		dev->fter_info.wb_cur_buf = NULL;
		return 0;
	}
	buf = list_entry(dev->fter_info.wb_videobuf_q.next,
				struct wb_buffer, queue);
	list_del(&buf->queue);
	S3DINFO("WB: kicking %d\n", buf->videobuf->i);
	return wb_process_buffer(dev, buf->videobuf);
}
static int activate_resources(struct s3d_ovl_device *dev)
{
	int r;

	r = init_overlays(dev, &dev->overlays);
	if (r) {
		S3DERR("failed to init overlays\n");
		return r;
	}

	if (dev->fter_config.use_wb) {
		INIT_LIST_HEAD(&dev->videobuf_q);
		dev->next_buf = dev->cur_buf = NULL;
		dev->streaming = true;
		return wb_kick(dev);
	}

	dev->next_buf = dev->cur_buf = list_entry(dev->videobuf_q.next,
						struct videobuf_buffer,
						queue);
	list_del(&dev->cur_buf->queue);
	dev->cur_buf->state = VIDEOBUF_ACTIVE;

	r = conf_overlays(dev, &dev->overlays, dev->cur_buf->i, OVL_ROLE_DISP);
	if (r) {
		S3DERR("failed to configure overlays\n");
		return r;
	}

	r = apply_changes(dev, &dev->overlays);
	if (r) {
		S3DERR("failed to apply overlay configuration\n");
		return r;
	}
	dev->streaming = true;
	return 0;
}

static void s3d_wb_isr(void *arg, u32 irqstatus)
{
	struct s3d_ovl_device *dev = (struct s3d_ovl_device *)arg;
	struct s3d_formatter_info *info;
	unsigned long flags;

	spin_lock_irqsave(&dev->vbq_lock, flags);

	info = &dev->fter_info;

	if (info->pend_wb_passes == 0) {
		S3DERR("wb got IRQ but no pending passes\n");
		spin_unlock_irqrestore(&dev->vbq_lock, flags);
		return;
	}

	info->pend_wb_passes--;
	if (info->pend_wb_passes == 0) {
		S3DINFO("wb done, idx:%d\n", info->wb_cur_buf->i);
		/*This is the first buffer processed, we can now display it*/
		if (dev->cur_buf == NULL) {
			dev->next_buf = dev->cur_buf = info->wb_cur_buf;
			info->wb_cur_buf->state = VIDEOBUF_ACTIVE;
			conf_overlays(dev, &dev->overlays, info->wb_cur_buf->i,
				      OVL_ROLE_DISP);
			apply_changes(dev, &dev->overlays);
		} else {
			/*Add to displayable list*/
			info->wb_cur_buf->state = VIDEOBUF_QUEUED;
			list_add_tail(&info->wb_cur_buf->queue,
					&dev->videobuf_q);
		}
		wb_kick(dev);
	} else {
		S3DINFO("wb pending:%d, idx:%d\n",
			dev->fter_info.pend_wb_passes,
			info->wb_cur_buf->i);
		/*Do next WB Pass */
		conf_overlays(dev, &dev->overlays, info->wb_cur_buf->i, OVL_ROLE_WB);
		apply_changes(dev, &dev->overlays);
	}

	spin_unlock_irqrestore(&dev->vbq_lock, flags);

}

static void s3d_overlay_isr(void *arg, u32 irqstatus)
{
	struct timeval timevalue = { 0 };
	struct s3d_ovl_device *dev = (struct s3d_ovl_device *)arg;
	unsigned long flags;

	if (!dev->streaming)
		return;

	/* Filter erroneous VSYNC handling */
	if (!dssdev_manually_updated(dev->cur_disp) && dispc_is_vsync_fake())
		return;

	spin_lock_irqsave(&dev->vbq_lock, flags);
	if (dev->cur_buf == NULL) {
		spin_unlock_irqrestore(&dev->vbq_lock, flags);
		return;
	}

	do_gettimeofday(&timevalue);

	if (dev->cur_buf != dev->next_buf) {
		S3DINFO("vsync ISR: buffer done - idx:%d\n", dev->cur_buf->i);
		dev->cur_buf->ts = timevalue;
		dev->cur_buf->state = VIDEOBUF_DONE;
		wake_up_interruptible(&dev->cur_buf->done);
		dev->cur_buf = dev->next_buf;
	}

	if (dev->cur_disp->panel.s3d_info.type == S3D_DISP_FRAME_SEQ)
		toggle_driver_view(dev);

	if (list_empty(&dev->videobuf_q)) {
		if (dev->cur_disp->panel.s3d_info.type == S3D_DISP_FRAME_SEQ) {
			/*This will toggle the current view */
			conf_overlays(dev, &dev->overlays,
				      dev->cur_buf->i, OVL_ROLE_DISP);
			apply_changes(dev, &dev->overlays);
		}
		spin_unlock_irqrestore(&dev->vbq_lock, flags);
		return;
	}

	/*TODO: for framesequential displays, make sure we don't get
	 *the next buffer before completing a full S3D sequence
	 *to avoid situations like L1R1L1R2
	 */
	dev->next_buf = list_entry(dev->videobuf_q.next,
					struct videobuf_buffer, queue);
	list_del(&dev->next_buf->queue);
	dev->next_buf->state = VIDEOBUF_ACTIVE;

	S3DINFO("vysnc ISR: next buffer idx:%d\n", dev->next_buf->i);

	conf_overlays(dev, &dev->overlays, dev->next_buf->i, OVL_ROLE_DISP);
	apply_changes(dev, &dev->overlays);

	spin_unlock_irqrestore(&dev->vbq_lock, flags);

}

/*-------------------------------------------------------------------------
* Buffer allocation functions
*-------------------------------------------------------------------------
*/
static void *alloc_buffer(size_t size, unsigned long *phys_addr)
{
	void *virt_addr;
	unsigned long addr;

	virt_addr = alloc_pages_exact(size, GFP_KERNEL | GFP_DMA);
	addr = (unsigned long)virt_addr;
	*phys_addr = virt_to_phys(virt_addr);
	return virt_addr;
}

static inline void free_buffer(void *virtaddr, size_t size)
{
	free_pages_exact(virtaddr, size);
}

static void free_buffers(struct s3d_buffer_queue *q)
{
	unsigned int i;
	if (q->uses_tiler) {
		for (i = 0; i < q->n_alloc; i++) {
			if (q->tiler_alloc_ptr[i])
				tiler_free((u32) q->tiler_alloc_ptr[i]);
			if (q->tiler_alloc_uv_ptr[i])
				tiler_free((u32) q->tiler_alloc_uv_ptr[i]);
			q->phy_addr[i] = 0;
			q->tiler_alloc_ptr[i] = 0;
			q->phy_uv_addr[i] = 0;
			q->tiler_alloc_uv_ptr[i] = 0;
		}
	} else {
		for (i = 0; i < q->n_alloc; i++) {
			free_buffer(q->virt_addr[i], q->buf_siz);
			q->virt_addr[i] = 0;
		}
	}

	q->n_alloc = 0;
}

static int alloc_buffers(struct s3d_ovl_device *dev,
				struct s3d_buffer_queue *q,
				unsigned int *count)
{
	s32 n_alloc;

	if (q->uses_tiler) {
		int aligned = 1;
		enum tiler_fmt fmt;
		size_t tiler_block_size = PAGE_ALIGN(q->width * q->bpp);

		n_alloc = *count;
		/* special allocation scheme for NV12 format */
		if (OMAP_DSS_COLOR_NV12 == q->color_mode) {
			tiler_alloc_packed_nv12(&n_alloc, ALIGN(q->width, 128),
						q->height,
						(void **)q->phy_addr,
						(void **)q->phy_uv_addr,
						(void **)q->tiler_alloc_ptr,
						(void **)q->tiler_alloc_uv_ptr,
						aligned);
			q->buf_siz = (q->height * 3 / 2 * tiler_block_size);
		} else {
			unsigned int width = q->width;
			unsigned int bpp = q->bpp;

			switch (q->color_mode) {
			case OMAP_DSS_COLOR_UYVY:
			case OMAP_DSS_COLOR_YUV2:
				width /= 2;
				bpp = 4;
				break;
			default:
				break;
			}

			/* Only bpp of 1, 2, and 4 is supported by tiler */
			fmt = (bpp == 1 ? TILFMT_8BIT :
				bpp == 2 ? TILFMT_16BIT :
				bpp == 4 ? TILFMT_32BIT : TILFMT_INVALID);
			if (fmt == TILFMT_INVALID) {
				S3DERR("invalid tiler format\n");
				return -ENOMEM;
			}

			tiler_alloc_packed(&n_alloc, fmt,
						ALIGN(width, 128 / bpp),
						q->height, (void **)q->phy_addr,
						(void **)q->tiler_alloc_ptr,
						aligned);
			q->buf_siz = (q->height * tiler_block_size);
		}

	} else {
		unsigned int i;
		unsigned long phy_addr = 0;
		void *virt_addr;
		size_t size;
		if (OMAP_DSS_COLOR_NV12 == q->color_mode)
			size = (q->width * q->height * 3) / 2;
		else
			size = q->width * q->height * q->bpp;

		size = PAGE_ALIGN(size);
		q->buf_siz = size;

		for (i = 0; i < *count; i++) {
			virt_addr = alloc_buffer(size, &phy_addr);
			if (virt_addr == NULL)
				break;
			q->virt_addr[i] = virt_addr;
			q->phy_addr[i] = phy_addr;
			if (OMAP_DSS_COLOR_NV12 == q->color_mode)
				q->phy_uv_addr[i] = phy_addr + (q->width * q->height);
		}
		n_alloc = i;
	}

	q->n_alloc = n_alloc;
	if (n_alloc < *count) {
		S3DERR("failed to allocate %d buffer(s),alloc:%d\n",
			*count, n_alloc);
		free_buffers(q);
		*count = 0;
		return -ENOMEM;
	}

	*count = n_alloc;
	return 0;
}

/*-------------------------------------------------------------------------
* Video buffer call backs
*-------------------------------------------------------------------------
*/

/* Buffer setup function is called by videobuf layer when REQBUF ioctl is
* called. This is used to setup buffers and return size and count of
* buffers allocated. After the call to this buffer, videobuf layer will
* setup buffer queue depending on the size and count of buffers
*/
static int buffer_setup(struct videobuf_queue *q,
			unsigned int *count, unsigned int *size)
{
	struct s3d_ovl_device *dev = q->priv_data;
	struct s3d_buffer_queue *in_q = &dev->in_q;

	if (!dev) {
		S3DERR("no device\n");
		return -EINVAL;
	}

	if (alloc_buffers(dev, in_q, count)) {
		S3DERR("failed allocating buffers\n");
		return -ENOMEM;
	}

	*size = in_q->buf_siz;
	return 0;
}

/* This function will be called when VIDIOC_QBUF ioctl is called.
* It prepare buffers before give out for the display. This function
* user space virtual address into physical address if userptr memory
* exchange mechanism is used.
*/
static int buffer_prepare(struct videobuf_queue *q,
				struct videobuf_buffer *vb,
				enum v4l2_field field)
{
	struct s3d_ovl_device *dev = q->priv_data;

	if (VIDEOBUF_NEEDS_INIT == vb->state) {
		vb->width = dev->in_q.width;
		vb->height = dev->in_q.height;
		vb->size = dev->in_q.buf_siz;
		vb->field = field;
	}
	vb->state = VIDEOBUF_PREPARED;

	return 0;
}

/* Buffer queue funtion will be called from the videobuf layer when _QBUF
* ioctl is called. It is used to enqueue buffer, which is ready to be
* displayed. */
static void buffer_queue(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct s3d_ovl_device *dev = q->priv_data;
	struct wb_buffer *wb_buf;

	/*synced with ISR through vbq_lock acquired by caller */

	wb_buf = &dev->fter_info.wb_buffers[vb->i];
	wb_buf->videobuf = vb;

	/*When we not streaming yet, add them to both queues.
	 *During stream on, it will be determined which queue to use*/
	if (!dev->streaming) {
		vb->state = VIDEOBUF_QUEUED;
		list_add_tail(&wb_buf->queue, &dev->fter_info.wb_videobuf_q);
		list_add_tail(&vb->queue, &dev->videobuf_q);
		return;
	}

	if (dev->fter_config.use_wb) {
		S3DINFO("WB queue:%d\n", vb->i);
		if (dev->fter_info.wb_state == WB_BUSY)
			list_add_tail(&wb_buf->queue,
				&dev->fter_info.wb_videobuf_q);
		else
			wb_process_buffer(dev, vb);
	} else {
		vb->state = VIDEOBUF_QUEUED;
		list_add_tail(&vb->queue, &dev->videobuf_q);
	}

}

/* Buffer release function is called from videobuf layer to release buffer
* which are already allocated */
static void buffer_release(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	vb->state = VIDEOBUF_NEEDS_INIT;
}

static struct videobuf_queue_ops vbq_ops = {
	.buf_setup = buffer_setup,
	.buf_prepare = buffer_prepare,
	.buf_release = buffer_release,
	.buf_queue = buffer_queue,
};

/*-------------------------------------------------------------------------
* File operations
*-------------------------------------------------------------------------
*/

static void vm_open(struct vm_area_struct *vma)
{
	struct s3d_ovl_device *dev = vma->vm_private_data;
	dev->in_q.mmap_count++;
}

static void vm_close(struct vm_area_struct *vma)
{
	struct s3d_ovl_device *dev = vma->vm_private_data;
	dev->in_q.mmap_count--;
}

static struct vm_operations_struct vm_ops = {
	.open = vm_open,
	.close = vm_close,
};

static int mmap(struct file *file, struct vm_area_struct *vma)
{
	struct s3d_ovl_device *dev = file->private_data;
	struct videobuf_queue *q = &dev->vbq;
	int i;
	unsigned long pos;
	int j = 0, k = 0, m = 0, p = 0, m_increment = 0;

	/* look for the buffer to map */
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		if (NULL == q->bufs[i])
			continue;
		if (V4L2_MEMORY_MMAP != q->bufs[i]->memory)
			continue;
		if (q->bufs[i]->boff == (vma->vm_pgoff << PAGE_SHIFT))
			break;
	}

	if (VIDEO_MAX_FRAME == i) {
		S3DERR("VM offset is invalid (0x%lx)\n",
		       (vma->vm_pgoff << PAGE_SHIFT));
		return -EINVAL;
	}
	q->bufs[i]->baddr = vma->vm_start;

	vma->vm_flags |= VM_RESERVED;
	if (!dev->in_q.cached) {
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		S3DINFO("using uncached buffers\n");
	}

	vma->vm_ops = &vm_ops;
	vma->vm_private_data = (void *)dev;

	/* Tiler remapping */
	pos = dev->in_q.phy_addr[i];
	/* get line width */
	/* for NV12, Y buffer is 1bpp */
	if (OMAP_DSS_COLOR_NV12 == dev->in_q.color_mode) {
		p = (dev->in_q.width + TILER_PAGE - 1) & ~(TILER_PAGE - 1);
		m_increment = 64 * TILER_WIDTH;
	} else {
		p = (dev->in_q.width * dev->in_q.bpp +
		     TILER_PAGE - 1) & ~(TILER_PAGE - 1);

		if (dev->in_q.bpp > 1)
			m_increment = 2 * 64 * TILER_WIDTH;
		else
			m_increment = 64 * TILER_WIDTH;
	}

	for (j = 0; j < dev->in_q.height; j++) {
		/* map each page of the line */
		vma->vm_pgoff = (pos + m) >> PAGE_SHIFT;

		if (remap_pfn_range(vma, vma->vm_start + k,
				    (pos + m) >> PAGE_SHIFT,
				    p, vma->vm_page_prot))
			return -EAGAIN;
		k += p;
		m += m_increment;
	}
	m = 0;

	/* UV Buffer in case of NV12 format */
	if (OMAP_DSS_COLOR_NV12 == dev->in_q.color_mode) {
		pos = dev->in_q.phy_uv_addr[i];
		/* UV buffer is 2 bpp, but half size, so p remains */
		m_increment = 2 * 64 * TILER_WIDTH;

		/* UV buffer is height / 2 */
		for (j = 0; j < dev->in_q.height / 2; j++) {
			/* map each page of the line */
			vma->vm_pgoff = (pos + m) >> PAGE_SHIFT;

			if (remap_pfn_range(vma, vma->vm_start + k,
					    (pos + m) >> PAGE_SHIFT,
					    p, vma->vm_page_prot))
				return -EAGAIN;
			k += p;
			m += m_increment;
		}
	}

	vma->vm_flags &= ~VM_IO;	/* using shared anonymous pages */
	dev->in_q.mmap_count++;

	return 0;
}

static int open(struct file *file)
{
	struct s3d_ovl_device *dev;
	struct videobuf_queue *q;

	dev = video_drvdata(file);

	if (dev == NULL)
		return -ENODEV;

	mutex_lock(&dev->lock);

	/* for now, we only support single open */
	if (dev->opened) {
		mutex_unlock(&dev->lock);
		return -EBUSY;
	}

	dev->opened++;
	file->private_data = dev;

	q = &dev->vbq;
	spin_lock_init(&dev->vbq_lock);

	videobuf_queue_dma_contig_init(q, &vbq_ops, NULL, &dev->vbq_lock,
				V4L2_BUF_TYPE_VIDEO_OUTPUT, V4L2_FIELD_NONE,
				sizeof(struct videobuf_buffer), dev);

	memset(&dev->in_q.offset, 0, sizeof(dev->in_q.offset));
	memset(&dev->in_q.uv_offset, 0, sizeof(dev->in_q.uv_offset));
	memset(&dev->out_q.offset, 0, sizeof(dev->out_q.offset));
	memset(&dev->out_q.uv_offset, 0, sizeof(dev->out_q.uv_offset));
	memset(&dev->fter_config, 0, sizeof(dev->fter_config));
	memset(&dev->fter_info, 0, sizeof(dev->fter_info));

	mutex_unlock(&dev->lock);
	return 0;
}

static int release(struct file *file)
{
	struct s3d_ovl_device *dev = file->private_data;
	struct videobuf_queue *q;

	if (dev == NULL)
		return 0;

	if (dev->streaming)
		vidioc_streamoff(file, file->private_data, V4L2_BUF_TYPE_VIDEO_OUTPUT);

	mutex_lock(&dev->lock);

	q = &dev->vbq;
	videobuf_mmap_free(q);
	free_resources(dev);
	free_buffers(&dev->in_q);

	dev->opened -= 1;
	file->private_data = NULL;

	dev->override_s3d_disp = false;
	mutex_unlock(&dev->lock);
	return 0;
}

static const struct v4l2_file_operations fops = {
	.owner = THIS_MODULE,
	.ioctl = video_ioctl2,
	.mmap = mmap,
	.open = open,
	.release = release,
};

/*-------------------------------------------------------------------------
* V4L2 IOCTLS
*-------------------------------------------------------------------------
*/

static int vidioc_querycap(struct file *file, void *fh,
			   struct v4l2_capability *cap)
{
	struct s3d_ovl_device *dev = fh;

	strlcpy(cap->driver, S3D_OVL_DEV_NAME, sizeof(cap->driver));
	strlcpy(cap->card, dev->vfd->name, sizeof(cap->card));
	cap->bus_info[0] = '\0';
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;
	return 0;
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *fh,
					struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	fmt->index = index;
	fmt->type = type;
	if (index >= NUM_OUTPUT_FORMATS)
		return -EINVAL;

	fmt->flags = omap_formats[index].flags;
	strlcpy(fmt->description, omap_formats[index].description,
		sizeof(fmt->description));
	fmt->pixelformat = omap_formats[index].pixelformat;
	return 0;
}

static int vidioc_g_fmt_vid_out(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	f->fmt.pix = dev->pix;
	return 0;

}

static int vidioc_try_fmt_vid_out(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	struct omap_video_timings *timing;

	mutex_lock(&dev->lock);

	if (dev->streaming) {
		mutex_unlock(&dev->lock);
		return -EBUSY;
	}

	if (dev->cur_disp == NULL) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	timing = &dev->cur_disp->panel.timings;

	dev->fbuf.fmt.height = timing->y_res;
	dev->fbuf.fmt.width = timing->x_res;

	try_format(&f->fmt.pix);
	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_s_fmt_vid_out(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	int bpp;
	struct omap_video_timings *timing;

	mutex_lock(&dev->lock);

	if (dev->streaming) {
		mutex_unlock(&dev->lock);
		return -EBUSY;
	}

	if (dev->cur_disp == NULL) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	timing = &dev->cur_disp->panel.timings;

	if (dev->rotation && f->fmt.pix.pixelformat == V4L2_PIX_FMT_RGB24) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	dev->fbuf.fmt.height = timing->y_res;
	dev->fbuf.fmt.width = timing->x_res;

	/* change to smaler size is OK */
	bpp = try_format(&f->fmt.pix);

	/* try & set the new output format */
	dev->pix = f->fmt.pix;

	dev->in_q.bpp = bpp;
	dev->in_q.width = dev->pix.width;
	dev->in_q.height = dev->pix.height;
	dev->in_q.crop_h = dev->in_q.height;
	dev->in_q.crop_w = dev->in_q.width;
	dev->in_q.color_mode = v4l2_pix_to_dss_color(&dev->pix);

	/* set default crop and win */
	set_default_crop(dev, &dev->pix, &dev->crop);
	set_default_window(dev, &dev->win);

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_try_fmt_vid_overlay(struct file *file, void *fh,
					struct v4l2_format *f)
{
	int err;
	struct s3d_ovl_device *dev = fh;
	struct v4l2_window *win = &f->fmt.win;

	mutex_lock(&dev->lock);

	err = omap_vout_try_window(&dev->fbuf, win);
	if (err) {
		mutex_unlock(&dev->lock);
		return err;
	}

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_s_fmt_vid_overlay(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	int err = -EINVAL;
	struct v4l2_window *win = &f->fmt.win;

	mutex_lock(&dev->lock);

	err = omap_vout_new_window(&dev->crop, &dev->win, &dev->fbuf, win);
	if (err) {
		mutex_unlock(&dev->lock);
		return err;
	}

	dev->win.global_alpha = f->fmt.win.global_alpha;
	dev->win.chromakey = f->fmt.win.chromakey;

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_enum_fmt_vid_overlay(struct file *file, void *fh,
					struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	fmt->index = index;
	fmt->type = type;
	if (index >= NUM_OUTPUT_FORMATS)
		return -EINVAL;

	fmt->flags = omap_formats[index].flags;
	strlcpy(fmt->description, omap_formats[index].description,
		sizeof(fmt->description));
	fmt->pixelformat = omap_formats[index].pixelformat;
	return 0;
}

static int vidioc_g_fmt_vid_overlay(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	struct omap_dss_device *display;
	struct omap_overlay_manager_info info;
	struct v4l2_window *win = &f->fmt.win;
	u32 key_value = 0;

	mutex_lock(&dev->lock);

	if (dev->cur_disp == NULL) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	win->w = dev->win.w;
	win->field = dev->win.field;
	win->global_alpha = dev->win.global_alpha;

	display = dev->cur_disp;
	if (display->manager && display->manager->get_manager_info) {
		display->manager->get_manager_info(display->manager, &info);
		key_value = info.trans_key;
	}
	win->chromakey = key_value;

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_try_fmt_type_private(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct v4l2_frame_packing *packing =
	    (struct v4l2_frame_packing *)&f->fmt.raw_data;

	switch (packing->type) {
	case V4L2_FPACK_PIX_IL:
	case V4L2_FPACK_CHECKB:
	case V4L2_FPACK_FRM_SEQ:
	case V4L2_FPACK_COL_IL:
	case V4L2_FPACK_ROW_IL:
		return -EINVAL;
	default:
		break;
	}
	/*TODO: Check against current display type */
	return 0;
}

static int vidioc_g_fmt_type_private(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	struct v4l2_frame_packing *packing =
	    (struct v4l2_frame_packing *)&f->fmt.raw_data;

	mutex_lock(&dev->lock);

	packing->type = dev->in_q.frame_packing.type;
	packing->order = dev->in_q.frame_packing.order;
	packing->sub_samp = dev->in_q.frame_packing.sub_samp;

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_s_fmt_type_private(struct file *file, void *fh,
					struct v4l2_format *f)
{
	struct s3d_ovl_device *dev = fh;
	struct v4l2_frame_packing *packing =
	    (struct v4l2_frame_packing *)&f->fmt.raw_data;

	switch (packing->type) {
	case V4L2_FPACK_PIX_IL:
	case V4L2_FPACK_CHECKB:
	case V4L2_FPACK_FRM_SEQ:
	case V4L2_FPACK_COL_IL:
	case V4L2_FPACK_ROW_IL:
		return -EINVAL;
	default:
		break;
	}

	mutex_lock(&dev->lock);

	if (dev->streaming) {
		S3DERR("I/O in progress\n");
		mutex_unlock(&dev->lock);
		return -EBUSY;
	}

	dev->in_q.frame_packing.type = packing->type;
	dev->in_q.frame_packing.order = packing->order;
	dev->in_q.frame_packing.sub_samp = packing->sub_samp;

	S3DINFO("type(%d), order(%d), subsampling(%d)\n",
		packing->type, packing->order, packing->sub_samp);
	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_cropcap(struct file *file, void *fh,
			  struct v4l2_cropcap *cropcap)
{
	struct s3d_ovl_device *dev = fh;
	enum v4l2_buf_type type = cropcap->type;
	struct v4l2_pix_format *pix = &dev->pix;

	cropcap->type = type;
	if (type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	/* Width and height are always even */
	cropcap->bounds.width = pix->width & ~1;
	cropcap->bounds.height = pix->height & ~1;

	omap_vout_default_crop(&dev->pix, &dev->fbuf, &cropcap->defrect);
	cropcap->pixelaspect.numerator = 1;
	cropcap->pixelaspect.denominator = 1;
	return 0;
}

static int vidioc_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct s3d_ovl_device *dev = fh;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;
	crop->c = dev->crop;
	return 0;
}

static int vidioc_s_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct s3d_ovl_device *dev = fh;
	int err = -EINVAL;
	s32 rotation;
	struct omap_video_timings *timing;

	if (crop->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
		return -EINVAL;

	mutex_lock(&dev->lock);

	if (dev->cur_disp == NULL) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	timing = &dev->cur_disp->panel.timings;
	rotation = dev->rotation;
	if (rotation == 90 || rotation == 270) {
		dev->fbuf.fmt.height = timing->x_res;
		dev->fbuf.fmt.width = timing->y_res;
	} else {
		dev->fbuf.fmt.height = timing->y_res;
		dev->fbuf.fmt.width = timing->x_res;
	}

	err = omap_vout_new_crop(&dev->pix, &dev->crop, &dev->win,
				 &dev->fbuf, &crop->c);

	dev->in_q.crop_w = dev->crop.width;
	dev->in_q.crop_h = dev->crop.height;

	mutex_unlock(&dev->lock);
	return err;

}

static int vidioc_queryctrl(struct file *file, void *fh,
				struct v4l2_queryctrl *ctrl)
{
	int i;

	if (ctrl->id & V4L2_CID_USER_BASE) {
		switch (ctrl->id) {
		case V4L2_CID_ROTATE:
			v4l2_ctrl_query_fill(ctrl, 0, 270, 90, 0);
			break;
		case V4L2_CID_BG_COLOR:
			v4l2_ctrl_query_fill(ctrl, 0, 0xFFFFFF, 1, 0);
			break;
		case V4L2_CID_VFLIP:
			v4l2_ctrl_query_fill(ctrl, 0, 1, 1, 0);
			break;
		default:
			ctrl->name[0] = '\0';
			return -EINVAL;
		}
	} else if (ctrl->id & V4L2_CID_PRIVATE_BASE) {
		for (i = 0; i < ARRAY_SIZE(s3d_private_controls); i++) {
			if (s3d_private_controls[i].id == ctrl->id) {
				memcpy(ctrl, &s3d_private_controls[i],
				       sizeof(struct v4l2_queryctrl));
				return 0;
			}
		}
		return -EINVAL;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int vidioc_querymenu(struct file *file, void *fh,
				struct v4l2_querymenu *menu)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(s3d_querymenus); i++) {
		if (s3d_querymenus[i].id == menu->id &&
		    s3d_querymenus[i].index == menu->index) {
			memcpy(menu, &s3d_querymenus[i],
			       sizeof(struct v4l2_querymenu));
			return 0;
		}
	}

	if (menu->id == V4L2_CID_PRIVATE_DISPLAY_ID &&
		menu->index < display_list.count) {
		if (display_list.name) {
			unsigned int size = strlen(display_list.name[menu->index]) + 1;

			if (size > sizeof(menu->name)) {
				size = sizeof(menu->name) - 1;
				menu->name[size] = '\0';
			}
			memcpy(&menu->name, display_list.name[menu->index], size);
		} else {
			sprintf((char *)&menu->name, "%d", menu->index);
		}
		menu->reserved = 0;
		return 0;
	}

	return -EINVAL;
}

static int vidioc_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	struct s3d_ovl_device *dev = fh;
	int ret = 0;

	mutex_lock(&dev->lock);
	switch (ctrl->id) {
	case V4L2_CID_ROTATE:
		ctrl->value = dev->rotation;
		break;
	case V4L2_CID_BG_COLOR:
		{
			struct omap_dss_device *disp = dev->cur_disp;
			struct omap_overlay_manager *mgr = disp->manager;
			if (mgr && mgr->get_manager_info) {
				struct omap_overlay_manager_info info;
				mgr->get_manager_info(mgr, &info);
				ctrl->value = info.default_color;
			} else {
				ret = -EINVAL;
			}
			break;
		}
	case V4L2_CID_VFLIP:
		ctrl->value = dev->vflip;
		break;
	case V4L2_CID_PRIVATE_DISPLAY_ID:
		ctrl->value = dev->cur_disp_idx;
		break;
	case V4L2_CID_PRIVATE_ANAGLYPH_TYPE:
		ctrl->value = dev->anaglyph_type;
		break;
	case V4L2_CID_PRIVATE_S3D_MODE:
		ctrl->value = dev->s3d_mode;
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&dev->lock);
	return ret;
}

static int vidioc_s_ctrl(struct file *file, void *fh, struct v4l2_control *a)
{
	struct s3d_ovl_device *dev = fh;

	switch (a->id) {
	case V4L2_CID_ROTATE:
		{
			int rotation = a->value;

			mutex_lock(&dev->lock);

			if (rotation && dev->pix.pixelformat == V4L2_PIX_FMT_RGB24) {
				mutex_unlock(&dev->lock);
				return -EINVAL;
			}

			dev->rotation = rotation;
			mutex_unlock(&dev->lock);
			return 0;
		}
	case V4L2_CID_BG_COLOR:
		{
			unsigned int color = a->value;
			struct omap_overlay_manager_info info;
			struct omap_dss_device *disp;
			struct omap_overlay_manager *mgr;

			mutex_lock(&dev->lock);
			disp = dev->cur_disp;
			mgr = dev->cur_disp->manager;

			if (!mgr || !mgr->get_manager_info) {
				mutex_unlock(&dev->lock);
				return -EINVAL;
			}

			mgr->get_manager_info(mgr, &info);
			info.default_color = color;
			if (mgr->set_manager_info(mgr, &info)) {
				mutex_unlock(&dev->lock);
				return -EINVAL;
			}

			mutex_unlock(&dev->lock);
			return 0;
		}
	case V4L2_CID_VFLIP:
		{
			mutex_lock(&dev->lock);

			if (a->value && dev->pix.pixelformat == V4L2_PIX_FMT_RGB24) {
				mutex_unlock(&dev->lock);
				return -EINVAL;
			}

			dev->vflip = a->value;
			mutex_unlock(&dev->lock);
			return 0;
		}
	case V4L2_CID_PRIVATE_DISPLAY_ID:
		{
			int r;
			if (dev->streaming)
				return -EBUSY;
			if (dev->cur_disp_idx == a->value)
				return 0;
			mutex_lock(&dev->lock);
			r = change_display(dev, get_display(a->value));
			if (!r)
				dev->cur_disp_idx = a->value;
			mutex_unlock(&dev->lock);

			return r;
		}
	case V4L2_CID_PRIVATE_ANAGLYPH_TYPE:
		{
			/*TODO: if already streaming should reconfigure
			 * the analyph mode dynamically */
			if (a->value < 0 || a->value >= V4L2_ANAGLYPH_MAX)
				return -EINVAL;
			mutex_lock(&dev->lock);
			dev->anaglyph_type = a->value;
			mutex_unlock(&dev->lock);
			return 0;
		}
	case V4L2_CID_PRIVATE_S3D_MODE:
		{
			if (a->value < 0 || a->value >= V4L2_S3D_MODE_MAX)
				return -EINVAL;
			mutex_lock(&dev->lock);
			if (dev->streaming && change_s3d_mode(dev, a->value)) {
				mutex_unlock(&dev->lock);
				return -EINVAL;
			}
			dev->s3d_mode = a->value;
			S3DINFO("received S3D mode:%d\n", dev->s3d_mode);
			mutex_unlock(&dev->lock);
			return 0;
		}
	default:
		return -EINVAL;
	}

}

static int vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct s3d_ovl_device *dev = fh;
	return videobuf_querybuf(&dev->vbq, b);
}

static int vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *buffer)
{
	struct s3d_ovl_device *dev = fh;
	struct videobuf_queue *q = &dev->vbq;
	int r = 0;
	bool push_buf = false;

	mutex_lock(&dev->lock);
	if ((V4L2_BUF_TYPE_VIDEO_OUTPUT != buffer->type) ||
		(buffer->index >= dev->in_q.n_alloc) ||
		(q->bufs[buffer->index]->memory != buffer->memory)) {
		r = -EINVAL;
		goto exit;
	}

	if (dev->streaming && dssdev_manually_updated(dev->cur_disp) &&
		list_empty(&dev->videobuf_q) &&
		(dev->cur_buf == dev->next_buf))
		push_buf = true;

	r = videobuf_qbuf(q, buffer);
	if (r)
		goto exit;

	if (calculate_offset(&dev->crop, &dev->in_q, buffer->index)) {
		r = -EINVAL;
		goto exit;
	}

	if (push_buf) {
		dev->next_buf = list_entry(dev->videobuf_q.next,
					struct videobuf_buffer, queue);
		list_del(&dev->next_buf->queue);
		dev->next_buf->state = VIDEOBUF_ACTIVE;

		r = conf_overlays(dev, &dev->overlays,
					dev->next_buf->i, OVL_ROLE_DISP);
		if (!r)
			r = apply_changes(dev, &dev->overlays);
	}

exit:
	mutex_unlock(&dev->lock);
	return r;
}

static int vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct s3d_ovl_device *dev = fh;
	struct videobuf_queue *q = &dev->vbq;

	return videobuf_dqbuf(q, (struct v4l2_buffer *)b,
				file->f_flags & O_NONBLOCK);
}

static int vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct s3d_ovl_device *dev = fh;
	struct videobuf_queue *q = &dev->vbq;
	int r = 0;
	u32 mask = 0;

	mutex_lock(&dev->lock);

	if (dev->streaming) {
		mutex_unlock(&dev->lock);
		return 0;
	}

	r = videobuf_streamon(q);
	if (r < 0) {
		S3DERR("videobuf_streamon failed(%d)\n", r);
		mutex_unlock(&dev->lock);
		return r;
	}

	if (list_empty(&dev->videobuf_q)) {
		S3DERR("no buffers queued\n");
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	if(def_disp_id != -1 && def_disp_id != dev->cur_disp_idx) {
		r = change_display(dev,get_display(def_disp_id));
		if (r)
			S3DERR("failed to change display(%d)\n", r);
		else
			dev->cur_disp_idx = def_disp_id;
	}

	/*Display resolution may change, so it's called before
	* resource allocation and configuration is made*/
	r = change_s3d_mode(dev, dev->s3d_mode);
	if (r) {
		S3DERR("failed to change S3D mode(%d)\n", r);
		mutex_unlock(&dev->lock);
		return r;
	}

	/*TODO: if the panel resolution changes after going to 3D
	   what to do we do with the current window setting*/
	/* set_default_window(dev, &dev->win); */

	r = allocate_resources(dev);
	if (r) {
		S3DERR("failed allocating resources(%d)\n", r);
		mutex_unlock(&dev->lock);
		return r;
	}

	if (dev->cur_disp->channel == OMAP_DSS_CHANNEL_LCD)
		mask = DISPC_IRQ_FRAMEDONE | DISPC_IRQ_VSYNC;
	else if (dev->cur_disp->channel == OMAP_DSS_CHANNEL_LCD2)
		mask = DISPC_IRQ_FRAMEDONE2 | DISPC_IRQ_VSYNC2;
	else
		mask = DISPC_IRQ_FRAMEDONE_DIG |
			DISPC_IRQ_EVSYNC_EVEN | DISPC_IRQ_EVSYNC_ODD;

	r = omap_dispc_register_isr(s3d_wb_isr, dev, DISPC_IRQ_FRAMEDONE_WB);
	if (r) {
		S3DERR("failed to register WB ISR(%d)\n", r);
		mutex_unlock(&dev->lock);
		return r;
	}

	r = omap_dispc_register_isr(s3d_overlay_isr, dev, mask);
	if (r) {
		S3DERR("failed to register vsync ISR(%d)\n", r);
		omap_dispc_unregister_isr(s3d_wb_isr, dev,
			DISPC_IRQ_FRAMEDONE_WB);
		mutex_unlock(&dev->lock);
		return r;
	}

	r = activate_resources(dev);
	if (r) {
		S3DERR("failed to activating resources(%d)\n", r);
		omap_dispc_unregister_isr(s3d_wb_isr, dev,
			DISPC_IRQ_FRAMEDONE_WB);
		omap_dispc_unregister_isr(s3d_overlay_isr, dev, mask);
		free_resources(dev);
	}

	mutex_unlock(&dev->lock);
	return r;
}

static int vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct s3d_ovl_device *dev = fh;
	struct videobuf_buffer *buf;
	u32 mask = 0;
	int r;

	mutex_lock(&dev->lock);

	if (!dev->streaming) {
		mutex_unlock(&dev->lock);
		return 0;
	}

	dev->streaming = false;

	if (dev->cur_disp->channel == OMAP_DSS_CHANNEL_LCD)
		mask = DISPC_IRQ_FRAMEDONE | DISPC_IRQ_VSYNC;
	else if (dev->cur_disp->channel == OMAP_DSS_CHANNEL_LCD2)
		mask = DISPC_IRQ_FRAMEDONE2 | DISPC_IRQ_VSYNC2;
	else
		mask = DISPC_IRQ_FRAMEDONE_DIG |
			DISPC_IRQ_EVSYNC_EVEN | DISPC_IRQ_EVSYNC_ODD;

	r = omap_dispc_unregister_isr(s3d_overlay_isr, dev, mask);
	if (r)
		S3DERR("failed to unregister vsync ISR(%d)\n", r);

	r = omap_dispc_unregister_isr(s3d_wb_isr, dev, DISPC_IRQ_FRAMEDONE_WB);
	if (r)
		S3DERR("failed to unregister wb ISR(%d)\n", r);

	enable_overlays(dev, &dev->overlays, OVL_ROLE_DISP | OVL_ROLE_WB,
			false);

	/*Flush outgoing queue*/
	list_for_each_entry(buf, &dev->videobuf_q, queue) {
		buf->state = VIDEOBUF_DONE;
		wake_up_interruptible(&buf->done);
	}

	if (dev->next_buf) {
		dev->next_buf->state = VIDEOBUF_DONE;
		wake_up_interruptible(&dev->next_buf->done);
	}
	if (dev->cur_buf && dev->cur_buf != dev->next_buf) {
		dev->cur_buf->state = VIDEOBUF_DONE;
		wake_up_interruptible(&dev->cur_buf->done);
	}
	if (dev->fter_info.wb_cur_buf) {
		dev->fter_info.wb_cur_buf->state = VIDEOBUF_DONE;
		wake_up_interruptible(&dev->fter_info.wb_cur_buf->done);
	}

	if (videobuf_streamoff(&dev->vbq))
		S3DERR("failed queue streamoff\n");

	free_resources(dev);

	change_s3d_mode(dev, V4L2_S3D_MODE_OFF);

	/* if current display is manually updated schedule update to refresh
		view when streaming is stopped */
	if (dssdev_manually_updated(dev->cur_disp))
		s3d_sched_disp_update(dev);

	INIT_LIST_HEAD(&dev->videobuf_q);
	INIT_LIST_HEAD(&dev->fter_info.wb_videobuf_q);
	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_reqbufs(struct file *file, void *fh,
				struct v4l2_requestbuffers *req)
{
	struct s3d_ovl_device *dev = fh;
	struct videobuf_queue *q = &dev->vbq;
	int ret = 0;

	if ((req->type != V4L2_BUF_TYPE_VIDEO_OUTPUT))
		return -EINVAL;

	if ((V4L2_MEMORY_MMAP != req->memory))
		return -EINVAL;

	mutex_lock(&dev->lock);

	if (dev->streaming) {
		S3DWARN("I/O is in progress\n");
		mutex_unlock(&dev->lock);
		if (req->count == 0) {
			vidioc_streamoff(file, fh, req->type);
			return 0;
		}
		return -EBUSY;
	}

	/*
	 * struct v4l2_requestbuffers reserved field used to define
	 * cacheable/non-cacheable
	 */
	dev->in_q.cached = (req->reserved[0] == 1) ? true : false;

	/* If buffers are already allocated free them */
	if (q->bufs[0] && (V4L2_MEMORY_MMAP == q->bufs[0]->memory)) {
		if (dev->in_q.mmap_count) {
			S3DWARN("some buffers are still mapped(%d)\n",
				dev->in_q.mmap_count);
			mutex_unlock(&dev->lock);
			return -EBUSY;
		}
		free_buffers(&dev->in_q);
		free_buffers(&dev->out_q);
		videobuf_mmap_free(q);
	}

	if (req->count == 0) {
		/*We have released any buffers at this point */
		return 0;
	}

	INIT_LIST_HEAD(&dev->videobuf_q);
	INIT_LIST_HEAD(&dev->fter_info.wb_videobuf_q);

	ret = videobuf_reqbufs(q, req);
	if (ret < 0) {
		S3DERR("videobuf_reqbufs failed %d\n", ret);
		mutex_unlock(&dev->lock);
		return ret;
	}

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_s_fbuf(struct file *file, void *fh,
				struct v4l2_framebuffer *a)
{
	struct s3d_ovl_device *dev = fh;
	struct omap_dss_device *disp;
	struct omap_overlay_manager *mgr;
	struct omap_overlay_manager_info info;
	enum omap_dss_trans_key_type key_type = OMAP_DSS_COLOR_KEY_GFX_DST;
	int enable = 0;

	mutex_lock(&dev->lock);

	/* OMAP DSS doesn't support Source and Destination color
	   key together */
	if ((a->flags & V4L2_FBUF_FLAG_SRC_CHROMAKEY) &&
	    (a->flags & V4L2_FBUF_FLAG_CHROMAKEY)) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}
	/* OMAP DSS Doesn't support the Destination color key
	   and alpha blending together */
	if ((a->flags & V4L2_FBUF_FLAG_CHROMAKEY) &&
	    (a->flags & V4L2_FBUF_FLAG_LOCAL_ALPHA)) {
		mutex_unlock(&dev->lock);
		return -EINVAL;
	}

	if ((a->flags & V4L2_FBUF_FLAG_SRC_CHROMAKEY)) {
		dev->fbuf.flags |= V4L2_FBUF_FLAG_SRC_CHROMAKEY;
		key_type = OMAP_DSS_COLOR_KEY_VID_SRC;
	} else
		dev->fbuf.flags &= ~V4L2_FBUF_FLAG_SRC_CHROMAKEY;

	if ((a->flags & V4L2_FBUF_FLAG_CHROMAKEY)) {
		dev->fbuf.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
		key_type = OMAP_DSS_COLOR_KEY_GFX_DST;
	} else
		dev->fbuf.flags &= ~V4L2_FBUF_FLAG_CHROMAKEY;

	if (a->flags & (V4L2_FBUF_FLAG_CHROMAKEY |
			V4L2_FBUF_FLAG_SRC_CHROMAKEY))
		enable = 1;
	else
		enable = 0;

	disp = dev->cur_disp;
	mgr = disp->manager;
	if (mgr && mgr->get_manager_info && mgr->set_manager_info) {
		mgr->get_manager_info(mgr, &info);
		info.trans_enabled = enable;
		info.trans_key_type = key_type;
		info.trans_key = dev->win.chromakey;

		if (a->flags & V4L2_FBUF_FLAG_LOCAL_ALPHA) {
			dev->fbuf.flags |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
			enable = 1;
		} else {
			dev->fbuf.flags &= ~V4L2_FBUF_FLAG_LOCAL_ALPHA;
			enable = 0;
		}

		info.alpha_enabled = enable;
		if (mgr->set_manager_info(mgr, &info)) {
			mutex_unlock(&dev->lock);
			return -EINVAL;
		}

	}

	mutex_unlock(&dev->lock);
	return 0;
}

static int vidioc_g_fbuf(struct file *file, void *fh,
				struct v4l2_framebuffer *a)
{
	struct s3d_ovl_device *dev = fh;
	struct omap_dss_device *display;
	struct omap_overlay_manager_info info;

	a->flags = 0x0;

	a->capability = V4L2_FBUF_CAP_LOCAL_ALPHA | V4L2_FBUF_CAP_CHROMAKEY
	    | V4L2_FBUF_CAP_SRC_CHROMAKEY;

	mutex_lock(&dev->lock);

	display = dev->cur_disp;
	if (display->manager && display->manager->get_manager_info) {
		display->manager->get_manager_info(display->manager, &info);
		if (info.trans_key_type == OMAP_DSS_COLOR_KEY_VID_SRC)
			a->flags |= V4L2_FBUF_FLAG_SRC_CHROMAKEY;
		if (info.trans_key_type == OMAP_DSS_COLOR_KEY_GFX_DST)
			a->flags |= V4L2_FBUF_FLAG_CHROMAKEY;
		if (info.alpha_enabled)
			a->flags |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
	}

	mutex_unlock(&dev->lock);
	return 0;
}

static long vidioc_default(struct file *file, void *fh, int cmd, void *arg)
{
	struct s3d_ovl_device *dev = (struct s3d_ovl_device *)fh;
	int ret = 0;
	switch (cmd) {
	case VIDIOC_PRIVATE_S3D_S_OFFS:
	{
		struct v4l2_s3d_offsets *offsets = arg;

		/*Saved offsets get applied at the next QBUF */
		mutex_lock(&dev->lock);
		dev->offsets.l = offsets->l;
		dev->offsets.r = offsets->r;
		dev->offsets.w = offsets->w;
		dev->offsets.h = offsets->h;
		mutex_unlock(&dev->lock);
		break;
	}
	case VIDIOC_PRIVATE_S3D_S_OUT_FMT:
	{
		struct v4l2_frame_packing *fpack = arg;
		/*Overrides S3D caps for the output display*/
		mutex_lock(&dev->lock);
		dev->override_s3d_disp = true;
		v4l2_fpack_to_disp_fpack(fpack, &dev->s3d_disp_info);
		mutex_unlock(&dev->lock);
		break;
	}
	default:
		ret = -EINVAL;
	}
	return ret;
}

static const struct v4l2_ioctl_ops vout_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out = vidioc_g_fmt_vid_out,
	.vidioc_try_fmt_vid_out = vidioc_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out = vidioc_s_fmt_vid_out,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_querymenu = vidioc_querymenu,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_fbuf = vidioc_s_fbuf,
	.vidioc_g_fbuf = vidioc_g_fbuf,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_try_fmt_vid_overlay = vidioc_try_fmt_vid_overlay,
	.vidioc_s_fmt_vid_overlay = vidioc_s_fmt_vid_overlay,
	.vidioc_enum_fmt_vid_overlay = vidioc_enum_fmt_vid_overlay,
	.vidioc_g_fmt_vid_overlay = vidioc_g_fmt_vid_overlay,
	.vidioc_try_fmt_type_private = vidioc_try_fmt_type_private,
	.vidioc_s_fmt_type_private = vidioc_s_fmt_type_private,
	.vidioc_g_fmt_type_private = vidioc_g_fmt_type_private,
	.vidioc_cropcap = vidioc_cropcap,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_s_crop = vidioc_s_crop,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_default = vidioc_default,
};

/*-------------------------------------------------------------------------
* Driver init functions
*-------------------------------------------------------------------------
*/

static int get_def_disp(struct s3d_ovl_device *dev)
{
	struct omap_dss_device *dssdev;
	struct omap_dss_device *tmp;
	struct omap_overlay *ovl;
	unsigned int count;

	/*Ugly since we assume GFX pipeline is the first in the list
	 *and it's associated display is the default. We need a better
	 *mechanism to get the default display device
	 */
	ovl = omap_dss_get_overlay(0);
	if (ovl && ovl->manager)
		dssdev = ovl->manager->device;
	else
		return -ENODEV;

	tmp = NULL;
	count = 0;
	for_each_dss_dev(tmp) {
		if ( (tmp == dssdev && def_disp_id == -1) ||
			def_disp_id == count) {
			dssdev = tmp;
			dev->cur_disp_idx = count;
			dev->cur_disp = dssdev;
		}
		count++;
	}

	display_list.count = count;
	display_list.name = kzalloc(count * sizeof(const char *), GFP_KERNEL);
	if (display_list.name == NULL) {
		S3DERR("could not allocate display names array\n");
	} else {
		tmp = NULL;
		count = 0;
		/* getting display names */
		for_each_dss_dev(tmp) {
			display_list.name[count++] = tmp->name;
		}
	}

	/* locate V4L2_CID_PRIVATE_DISPLAY_ID queryctrl struct within
	 * s3d_private_controls and dynamically adjust maximum param */
	for (count = 0; count < ARRAY_SIZE(s3d_private_controls); count++) {
		if (s3d_private_controls[count].id == V4L2_CID_PRIVATE_DISPLAY_ID) {
			int *ptr;

			ptr = (int *)&s3d_private_controls[count].maximum;
			*ptr = display_list.count - 1;
			ptr = (int *)&s3d_private_controls[count].default_value;
			*ptr = dev->cur_disp_idx;
			break;
		}
	}

	return 0;
}

static int __init s3d_ovl_dev_init(struct s3d_ovl_device *dev)
{
	struct v4l2_pix_format *pix;
	struct video_device *vfd;
	struct omap_dss_device *display;

	if (get_def_disp(dev)) {
		S3DERR("could not find default display!\n");
		return -ENODEV;
	}

	INIT_LIST_HEAD(&dev->overlays);
	display = dev->cur_disp;

	/* set the default pix */
	pix = &dev->pix;

	/* Set the default picture of QVGA  */
	pix->width = QQVGA_WIDTH;
	pix->height = QQVGA_HEIGHT;

	/* Default pixel format is RGB 5-6-5 */
	pix->pixelformat = V4L2_PIX_FMT_RGB565;
	pix->field = V4L2_FIELD_ANY;
	pix->bytesperline = pix->width * 2;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
	pix->colorspace = V4L2_COLORSPACE_JPEG;

	dev->fbuf.fmt.width = display->panel.timings.x_res;
	dev->fbuf.fmt.height = display->panel.timings.y_res;

	/* Set the data structures for the overlay parameters */
	dev->win.global_alpha = 255;
	dev->fbuf.flags = 0;
	dev->fbuf.capability = V4L2_FBUF_CAP_LOCAL_ALPHA |
	    V4L2_FBUF_CAP_SRC_CHROMAKEY | V4L2_FBUF_CAP_CHROMAKEY;
	dev->win.chromakey = 0;

	dev->supported_modes = OMAP_DSS_COLOR_NV12 | OMAP_DSS_COLOR_YUV2 |
	    OMAP_DSS_COLOR_UYVY | OMAP_DSS_COLOR_RGB16 |
	    OMAP_DSS_COLOR_RGB24P | OMAP_DSS_COLOR_ARGB32 |
	    OMAP_DSS_COLOR_RGBX32;

	dev->in_q.bpp = RGB565_BPP;
	dev->in_q.width = QQVGA_WIDTH;
	dev->in_q.height = QQVGA_HEIGHT;
	dev->in_q.color_mode = OMAP_DSS_COLOR_RGB16;
	dev->in_q.uses_tiler = true;
	dev->in_q.cached = false;

	dev->vflip = 0;
	dev->rotation = 0;
	dev->s3d_mode = V4L2_S3D_MODE_ON;
	dev->anaglyph_type = V4L2_ANAGLYPH_RED_CYAN;

	set_default_crop(dev, &dev->pix, &dev->crop);
	set_default_window(dev, &dev->win);

	/* initialize the video_device struct */
	vfd = dev->vfd = video_device_alloc();

	if (!vfd) {
		S3DERR("could not allocate video device struct\n");
		return -ENOMEM;
	}
	vfd->release = video_device_release;
	vfd->ioctl_ops = &vout_ioctl_ops;

	strlcpy(vfd->name, S3D_OVL_DEV_NAME, sizeof(vfd->name));
	vfd->vfl_type = VFL_TYPE_GRABBER;

	vfd->fops = &fops;
	mutex_init(&dev->lock);

	return 0;

}

static void s3d_ovl_dev_release(struct s3d_ovl_device *dev)
{
	flush_workqueue(dev->workqueue);
	destroy_workqueue(dev->workqueue);

	video_unregister_device(dev->vfd);
	v4l2_device_unregister(&dev->v4l2_dev);

	kfree(display_list.name);
	kfree(dev);
	s3d_overlay_dev = NULL;
}

static int __init s3d_ovl_driver_init(void)
{
	int r = 0;
	struct s3d_ovl_device *dev;
	struct video_device *vfd = NULL;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		S3DERR("could not allocate internal dev struct\n");
		return -ENOMEM;
	}

	snprintf(dev->v4l2_dev.name, sizeof(dev->v4l2_dev.name),
		 "%s", S3D_OVL_DEV_NAME);
	if (v4l2_device_register(NULL, &dev->v4l2_dev) < 0) {
		S3DERR("v4l2_device_register failed\n");
		r = -ENODEV;
		goto error;
	}

	if (s3d_ovl_dev_init(dev) != 0) {
		r = -ENODEV;
		goto error1;
	}

	dev->workqueue = create_singlethread_workqueue("s3d_workqueue");
	if (!dev->workqueue) {
		r = -ENOMEM;
		goto error2;
	}

	vfd = dev->vfd;
	if (video_register_device(vfd, VFL_TYPE_GRABBER, -1) < 0) {
		r = -ENODEV;
		goto error3;
	}
	video_set_drvdata(vfd, dev);

	s3d_overlay_dev = dev;

	return 0;

error3:
	destroy_workqueue(dev->workqueue);
error2:
	video_device_release(vfd);
error1:
	v4l2_device_unregister(&dev->v4l2_dev);
	if (display_list.name)
		kfree(display_list.name);
error:
	kfree(dev);
	s3d_overlay_dev = NULL;
	return r;
}

static void __exit s3d_ovl_driver_exit(void)
{
	if (s3d_overlay_dev)
		s3d_ovl_dev_release(s3d_overlay_dev);
}

late_initcall(s3d_ovl_driver_init);
module_exit(s3d_ovl_driver_exit);
