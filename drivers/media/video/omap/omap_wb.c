/*
 * drivers/media/video/omap/omap_wb.c
 *
 * Copyright (C) 2005-2009 Texas Instruments.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 * Leveraged code from the OMAP2 camera driver
 * Video-for-Linux (Version 2) camera capture driver for
 * the OMAP24xx camera controller.
 *
 * Author: Andy Lowe (source@mvista.com)
 *
 * Copyright (C) 2004 MontaVista Software, Inc.
 * Copyright (C) 2009 Texas Instruments.
 *
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
#include <linux/delay.h>
#include <linux/slab.h>
#include <media/videobuf-dma-contig.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>

#include <asm/processor.h>
#include <plat/dma.h>
#include <plat/vram.h>
#include <plat/display.h>

#include "omap_wbdef.h"
#include "omap_voutlib.h"
#include <mach/tiler.h>

#define WB_NAME "omap_wb"

#define QQVGA_WIDTH		160
#define QQVGA_HEIGHT	120
#define WB_MIN_WIDTH		2
#define WB_MIN_HEIGHT		2
#define WB_MAX_WIDTH		2048
#define WB_MAX_HEIGHT		2048


struct mutex wb_lock;
static struct videobuf_queue_ops video_vbq_ops;

static int debug_wb;

module_param(debug_wb, bool, S_IRUGO);
MODULE_PARM_DESC(debug_wb, "Debug level (0-1)");

int omap_vout_try_format(struct v4l2_pix_format *pix);
enum omap_color_mode video_mode_to_dss_mode(
		struct v4l2_pix_format *pix);
void omap_wb_isr(void *arg, unsigned int irqstatus);
int	omap_dss_wb_apply(struct omap_overlay_manager *mgr, struct omap_writeback *wb);
int omap_dss_wb_flush(void);

int omap_setup_wb(struct omap_wb_device *wb_device, u32 addr, u32 uv_addr)
{
	struct omap_writeback *wb;
	struct omap_overlay_manager *mgr = NULL;
	struct omap_writeback_info wb_info;
	struct omap_overlay *ovl = NULL;
	int r = 0;
	int i = 0;
	wb_info.enabled = true;
	wb_info.info_dirty = true;
	wb_info.capturemode = wb_device->capturemode;
	wb_info.dss_mode = wb_device->dss_mode;
	wb_info.out_height = wb_device->pix.height;
	wb_info.out_width = wb_device->pix.width;
	wb_info.source = wb_device->source;
	wb_info.source_type = wb_device->source_type;
	wb_info.paddr = addr;
	wb_info.puv_addr = uv_addr;

	ovl = omap_dss_get_overlay(wb_info.source - 3);

	wb_info.height = ovl->info.out_height;
	wb_info.width = ovl->info.out_width;

	v4l2_dbg(1, debug_wb, &wb_device->wb_dev->v4l2_dev,
			"omap_write back struct contents :\n"
			"enabled = %d\n infodirty = %d\n"
			"capturemode = %d\n dss_mode = %d\n"
			"height = %ld\n width = %ld\n out_height = %ld\n"
			"out_width = %ld\n source = %d\n"
			"source_type = %d\n paddr =%lx\n puvaddr = %lx\n",
			wb_info.enabled, wb_info.info_dirty, wb_info.capturemode,
			wb_info.dss_mode, wb_info.height, wb_info.width,
			wb_info.out_height, wb_info.out_width, wb_info.source,
			wb_info.source_type, wb_info.paddr, wb_info.puv_addr);

	for (i = 0; i < omap_dss_get_num_overlay_managers(); ++i) {
			/* Fix : checking for mgr will shift to DSS2 */
			mgr = omap_dss_get_overlay_manager(i);
			if (!mgr) {
				WARN_ON(1);
				r = -EINVAL;
				goto err;
			}
			if (strcmp(mgr->name, "lcd") == 0)
				break;
	}

	wb = omap_dss_get_wb(0);

	if (!wb) {
		printk(KERN_ERR WB_NAME "couldn't get wb struct\n");
		r = -EINVAL;
		goto err;
	}
	wb->enabled = true;
	wb->info_dirty = true;

	r = wb->set_wb_info(wb, &wb_info);
	if (r) {
		printk(KERN_ERR WB_NAME "wb_info not set\n");
		goto err;
	}
	if (mgr)
		r = omap_dss_wb_apply(mgr, wb);
	else
		printk(KERN_ERR WB_NAME "mgr is NULL!\n");
err:
	return r;
}

int omap_wb_try_format(enum omap_color_mode *color_mode,
			struct v4l2_pix_format *pix)
{
	int bpp = 0;

	pix->height = clamp(pix->height, (u32)WB_MIN_HEIGHT,
			(u32)WB_MAX_HEIGHT);
	pix->width = clamp(pix->width, (u32)WB_MIN_WIDTH, (u32)WB_MAX_WIDTH);

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		*color_mode = OMAP_DSS_COLOR_RGB16;
		bpp = RGB565_BPP;
		break;
	case V4L2_PIX_FMT_RGB24:
		*color_mode = OMAP_DSS_COLOR_RGB24P;
		bpp = RGB24_BPP;
		break;
	case V4L2_PIX_FMT_BGR32:
		*color_mode = OMAP_DSS_COLOR_RGBX32;
		bpp = RGB32_BPP;
	case V4L2_PIX_FMT_YUYV:
		*color_mode = OMAP_DSS_COLOR_YUV2;
		bpp = YUYV_BPP;
		break;
	case V4L2_PIX_FMT_UYVY:
		*color_mode = OMAP_DSS_COLOR_UYVY;
		bpp = YUYV_BPP;
		break;
	case V4L2_PIX_FMT_NV12:
		*color_mode = OMAP_DSS_COLOR_NV12;
		bpp = 1; /* TODO: check this? */
		break;
	case V4L2_PIX_FMT_RGB32:
	default:
		*color_mode = OMAP_DSS_COLOR_ARGB32;
		bpp = RGB32_BPP;
		break;
	}

	pix->colorspace = V4L2_COLORSPACE_SRGB;
	pix->field = V4L2_FIELD_ANY;
	pix->priv = 0;

	pix->bytesperline = pix->width * bpp;
	pix->bytesperline = (pix->bytesperline + PAGE_SIZE - 1) &
		~(PAGE_SIZE - 1);
	pix->sizeimage = pix->bytesperline * pix->height;

	return bpp;
}

static int omap_wb_process_frame(struct omap_wb_device *wb)
{
	int ret = 0;
	u32 addr, uv_addr;
	wb->next_frm = list_entry(wb->dma_queue.next,
				struct videobuf_buffer, queue);

	list_del(&wb->next_frm->queue);

	wb->next_frm->state = VIDEOBUF_ACTIVE;

	addr = (unsigned long)wb->queued_buf_addr[wb->next_frm->i];

	uv_addr = (unsigned long)wb->queued_buf_uv_addr[
		wb->next_frm->i];

	ret = omap_setup_wb(wb, addr, uv_addr);
	if (ret)
		printk(KERN_ERR WB_NAME "error in setup_wb %d\n", ret);
	return ret;
}
/*
 *
 * IOCTL interface.
 *
 */

static int vidioc_querycap(struct file *file, void *fh,
		struct v4l2_capability *cap)
{
	struct omap_wb_device *wb = fh;

	strlcpy(cap->driver, WB_NAME,
		sizeof(cap->driver));
	strlcpy(cap->card, wb->vfd->name, sizeof(cap->card));
	cap->bus_info[0] = '\0';
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE;
	return 0;
}

static int vidioc_g_fmt_vid_cap(struct file *file, void *fh,
			struct v4l2_format *f)
{
	struct omap_wb_device *wb = fh;

	f->fmt.pix = wb->pix;
	return 0;

}

static int vidioc_s_fmt_vid_cap(struct file *file, void *fh,
			struct v4l2_format *f)
{
	struct omap_wb_device *wb = fh;
	int bpp;
	enum omap_color_mode color_mode;

	if (wb->streaming)
		return -EBUSY;

	mutex_lock(&wb->lock);

	bpp = omap_wb_try_format(&color_mode, &f->fmt.pix);

	/* try & set the new output format */
	wb->bpp = bpp;
	wb->pix = f->fmt.pix;

	wb->win.w.width = wb->pix.width;
	wb->win.w.height = wb->pix.height;

	/* we need to get curr_display dimensions for wb pipeline */
	wb->win.w.left = ((864 - wb->win.w.width) >> 1) & ~1;
	wb->win.w.top = ((480 - wb->win.w.height) >> 1) & ~1;

	wb->dss_mode = color_mode;

	mutex_unlock(&wb->lock);

	return 0;
}

static int vidioc_s_fmt_vid_overlay(struct file *file, void *fh,
			struct v4l2_format *f)
{
	int ret = 0;
	struct omap_wb_device *wb = fh;
	struct v4l2_window *win = &f->fmt.win;

	mutex_lock(&wb->lock);
	/* No boundry checks for wb window for now */
	wb->win.w.left = win->w.left;
	wb->win.w.top = win->w.top;
	wb->win.w.width = win->w.width;
	wb->win.w.height = win->w.height;
	mutex_unlock(&wb->lock);

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *fh,
			struct v4l2_requestbuffers *req)
{
	struct omap_wb_device *wb = fh;
	struct videobuf_queue *q = &wb->vbq;
	unsigned int i;
	int ret = 0;

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "entered REQbuf: \n");

	if ((req->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) || (req->count < 0))
		return -EINVAL;

	/* if memory is not mmp or userptr
	   return error */
	if ((V4L2_MEMORY_MMAP != req->memory) &&
		(V4L2_MEMORY_USERPTR != req->memory))
		return -EINVAL;

	mutex_lock(&wb->lock);

	/* Cannot be requested when streaming is on */
	if (wb->streaming) {
		mutex_unlock(&wb->lock);
		return -EBUSY;
	}

	/* If buffers are already allocated free them */
	if (q->bufs[0] && (V4L2_MEMORY_MMAP == q->bufs[0]->memory)) {
		if (wb->mmap_count) {
			mutex_unlock(&wb->lock);
			return -EBUSY;
		}

		videobuf_mmap_free(q);
	} else if (q->bufs[0] && (V4L2_MEMORY_USERPTR == q->bufs[0]->memory)) {
		if (wb->buffer_allocated) {
			videobuf_mmap_free(q);
			for (i = 0; i < wb->buffer_allocated; i++) {
				kfree(q->bufs[i]);
				q->bufs[i] = NULL;
			}
			wb->buffer_allocated = 0;
		}
	}
	/*store the memory type in data structure */
	wb->memory = req->memory;

	INIT_LIST_HEAD(&wb->dma_queue);

	/* call videobuf_reqbufs api */
	ret = videobuf_reqbufs(q, req);
	if (ret < 0) {
		mutex_unlock(&wb->lock);
		return ret;
	}

	wb->buffer_allocated = req->count;
	mutex_unlock(&wb->lock);
	return 0;
}

static int vidioc_querybuf(struct file *file, void *fh,
			struct v4l2_buffer *b)
{
	struct omap_wb_device *wb = fh;

	return videobuf_querybuf(&wb->vbq, b);
}

static int vidioc_qbuf(struct file *file, void *fh,
			struct v4l2_buffer *buffer)
{
	struct omap_wb_device *wb = fh;
	struct videobuf_queue *q = &wb->vbq;
	int ret = 0;
	int qempty = list_empty(&wb->dma_queue);

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
		"entered qbuf: buffer address: %x\n", (unsigned int) buffer);

	if ((V4L2_BUF_TYPE_VIDEO_CAPTURE != buffer->type) ||
			(buffer->index >= wb->buffer_allocated) ||

			(q->bufs[buffer->index]->memory != buffer->memory)) {
		return -EINVAL;
	}
	if (V4L2_MEMORY_USERPTR == buffer->memory) {
		if ((buffer->length < wb->pix.sizeimage) ||
			(0 == buffer->m.userptr)) {
			return -EINVAL;
		}
	}

	ret = videobuf_qbuf(q, buffer);

	if (wb->cur_frm &&
			wb->next_frm &&
			wb->cur_frm->i == wb->next_frm->i &&
			wb->cur_frm->state == VIDEOBUF_ACTIVE &&
			wb->next_frm->state == VIDEOBUF_ACTIVE &&
			qempty)
		ret = omap_wb_process_frame(wb);

	return ret;
}

static int vidioc_dqbuf(struct file *file, void *fh,
			struct v4l2_buffer *b)
{
	struct omap_wb_device *wb = fh;
	struct videobuf_queue *q = &wb->vbq;
	int ret = 0;

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
		"entered DQbuf: buffer address: %x \n", (unsigned int) b);

	if (!wb->streaming)
		return -EINVAL;

	if (file->f_flags & O_NONBLOCK)
		/* Call videobuf_dqbuf for non blocking mode */
		ret = videobuf_dqbuf(q, (struct v4l2_buffer *)b, 1);
	else
		/* Call videobuf_dqbuf for  blocking mode */
		ret = videobuf_dqbuf(q, (struct v4l2_buffer *)b, 0);
	return ret;
}

static int vidioc_streamon(struct file *file, void *fh,
			enum v4l2_buf_type i)
{
	struct omap_wb_device *wb = fh;
	struct videobuf_queue *q = &wb->vbq;
	u32 addr = 0, uv_addr = 0;
	int r = 0;
	u32 mask = 0;

	mutex_lock(&wb->lock);

	if (wb->streaming) {
		mutex_unlock(&wb->lock);
		return -EBUSY;
	}

	r = videobuf_streamon(q);
	if (r < 0) {
		mutex_unlock(&wb->lock);
		return r;
	}

	if (list_empty(&wb->dma_queue)) {
		mutex_unlock(&wb->lock);
		return -EIO;
	}
	/* Get the next frame from the buffer queue */
	wb->next_frm = wb->cur_frm = list_entry(wb->dma_queue.next,
				struct videobuf_buffer, queue);
	/* Remove buffer from the buffer queue */
	list_del(&wb->cur_frm->queue);
	/* Mark state of the current frame to active */
	wb->cur_frm->state = VIDEOBUF_ACTIVE;

	/* set flag here. Next QBUF will start DMA */
	wb->streaming = 1;

	wb->first_int = 1;

	addr = (unsigned long) wb->queued_buf_addr[wb->cur_frm->i];

	uv_addr = (unsigned long) wb->queued_buf_uv_addr[wb->cur_frm->i];

	mask = DISPC_IRQ_FRAMEDONE_WB;

	r = omap_dispc_register_isr(omap_wb_isr, wb, mask);
	if (r) {
		printk(KERN_ERR WB_NAME "Failed to register wb_isr\n");
		return r;
	}

	r = omap_setup_wb(wb, addr, uv_addr);
	if (r)
		printk(KERN_ERR WB_NAME "error in setup_wb\n");

	mutex_unlock(&wb->lock);
	return r;
}

static int vidioc_streamoff(struct file *file, void *fh,
			enum v4l2_buf_type i)
{
	struct omap_wb_device *wb = fh;
	u32 mask = 0;
	int r = 0;

	if (!wb->streaming)
		return -EINVAL;

	wb->streaming = 0;
	mask = DISPC_IRQ_FRAMEDONE_WB;

	r = omap_dispc_unregister_isr(omap_wb_isr, wb, mask);
	if (r) {
		printk(KERN_ERR WB_NAME "Failed to unregister wb_isr\n");
		return r;
	}
	omap_dss_wb_flush();

	INIT_LIST_HEAD(&wb->dma_queue);
	videobuf_streamoff(&wb->vbq);
	videobuf_queue_cancel(&wb->vbq);
	return 0;
}

static long vidioc_default(struct file *file, void *fh,
			     int cmd, void *arg)
{
	struct v4l2_writeback_ioctl_data *wb_data = NULL;
	struct omap_wb_device *wb = fh;
	wb_data  = (struct v4l2_writeback_ioctl_data *)arg;

	switch (cmd) {
	case VIDIOC_CUSTOM_S_WB:
		if (!wb_data) {
			printk(KERN_ERR WB_NAME "error in S_WB\n");
			goto err;
		}
		mutex_lock(&wb->lock);
		wb->enabled = wb_data->enabled;
		wb->source = wb_data->source;
		wb->capturemode = wb_data->capturemode;
		wb->source_type = wb_data->source_type;
		mutex_unlock(&wb->lock);

		break;

	case VIDIOC_CUSTOM_G_WB:
		if (!wb_data) {
			printk(KERN_ERR WB_NAME "error in G_WB\n");
			goto err;
		}
		mutex_lock(&wb->lock);
		wb_data->source = wb->source;
		wb_data->capturemode = wb->capturemode;
		wb_data->source_type = wb->source_type;
		mutex_unlock(&wb->lock);

		break;
	default:
		BUG();
	}
	return 0;

err:
	mutex_unlock(&wb->lock);
	return -EINVAL;
}

static int vidioc_g_fmt_vid_overlay(struct file *file, void *fh,
			struct v4l2_format *f)
{

	struct omap_wb_device *wb = fh;
	struct v4l2_window *win = &f->fmt.win;

	win->w = wb->win.w;
	win->field = wb->win.field;
	win->global_alpha = wb->win.global_alpha;

	return 0;
}

static const struct v4l2_ioctl_ops wb_ioctl_fops = {
	.vidioc_querycap	= vidioc_querycap,
	.vidioc_g_fmt_vid_cap	= vidioc_g_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap	= vidioc_s_fmt_vid_cap,
	.vidioc_reqbufs		= vidioc_reqbufs,
	.vidioc_querybuf	= vidioc_querybuf,
	.vidioc_qbuf		= vidioc_qbuf,
	.vidioc_dqbuf		= vidioc_dqbuf,
	.vidioc_streamon	= vidioc_streamon,
	.vidioc_streamoff	= vidioc_streamoff,
	.vidioc_s_fmt_vid_overlay = vidioc_s_fmt_vid_overlay,
	.vidioc_g_fmt_vid_overlay = vidioc_g_fmt_vid_overlay,
	.vidioc_default		= vidioc_default,
};

#ifdef CONFIG_TILER_OMAP
static void omap_wb_tiler_buffer_free(struct omap_wb_device *wb,
					unsigned int count,
					unsigned int startindex)
{
	int i;

	if (startindex < 0)
		startindex = 0;
	if (startindex + count > VIDEO_MAX_FRAME)
		count = VIDEO_MAX_FRAME - startindex;

	for (i = startindex; i < startindex + count; i++) {
		if (wb->buf_phy_addr_alloced[i])
			tiler_free(wb->buf_phy_addr_alloced[i]);
		if (wb->buf_phy_uv_addr_alloced[i])
			tiler_free(wb->buf_phy_uv_addr_alloced[i]);
		wb->buf_phy_addr[i] = 0;
		wb->buf_phy_addr_alloced[i] = 0;
		wb->buf_phy_uv_addr[i] = 0;
		wb->buf_phy_uv_addr_alloced[i] = 0;
	}
}

/* Allocate the buffers for  TILER space.  Ideally, the buffers will be ONLY
 in tiler space, with different rotated views available by just a convert.
 */
static int omap_wb_tiler_buffer_setup(struct omap_wb_device *wb,
					unsigned int *count, unsigned int startindex,
					struct v4l2_pix_format *pix)
{
	int i, aligned = 1;
	enum tiler_fmt fmt;

	/* normalize buffers to allocate so we stay within bounds */
	int start = (startindex < 0) ? 0 : startindex;
	int n_alloc = (start + *count > VIDEO_MAX_FRAME)
		? VIDEO_MAX_FRAME - start : *count;
	int bpp = omap_vout_try_format(pix);

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "tiler buffer alloc:\n"
		"count - %d, start -%d :\n", *count, startindex);

	/* special allocation scheme for NV12 format */
	if (OMAP_DSS_COLOR_NV12 == video_mode_to_dss_mode(pix)) {
		tiler_alloc_packed_nv12(&n_alloc, pix->width,
			pix->height,
			(void **) wb->buf_phy_addr + start,
			(void **) wb->buf_phy_uv_addr + start,
			(void **) wb->buf_phy_addr_alloced + start,
			(void **) wb->buf_phy_uv_addr_alloced + start,
			aligned);
	} else {
		unsigned int width = pix->width;
		switch (video_mode_to_dss_mode(pix)) {
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
		if (fmt == TILFMT_INVALID)
			return -ENOMEM;

		tiler_alloc_packed(&n_alloc, fmt, width,
			pix->height,
			(void **) wb->buf_phy_addr + start,
			(void **) wb->buf_phy_addr_alloced + start,
			aligned);
	}

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
			"allocated %d buffers\n", n_alloc);

	if (n_alloc < *count) {
		if (n_alloc && (startindex == -1 ||
			V4L2_MEMORY_MMAP != wb->memory)) {
			/* TODO: check this condition's logic */
			omap_wb_tiler_buffer_free(wb, n_alloc, start);
			*count = 0;
			return -ENOMEM;
		}
	}

	for (i = start; i < start + n_alloc; i++) {
		v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
				"y=%08lx (%d) uv=%08lx (%d)\n",
				wb->buf_phy_addr[i],
				wb->buf_phy_addr_alloced[i] ? 1 : 0,
				wb->buf_phy_uv_addr[i],
				wb->buf_phy_uv_addr_alloced[i] ? 1 : 0);
	}

	*count = n_alloc;

	return 0;
}

/* Free tiler buffers */
static void omap_wb_free_tiler_buffers(struct omap_wb_device *wb)
{
	omap_wb_tiler_buffer_free(wb, wb->buffer_allocated, 0);
	wb->buffer_allocated = 0;
}
#endif	/* ifdef CONFIG_TILER_OMAP */


/* Video buffer call backs */

/* Buffer setup function is called by videobuf layer when REQBUF ioctl is
 * called. This is used to setup buffers and return size and count of
 * buffers allocated. After the call to this buffer, videobuf layer will
 * setup buffer queue depending on the size and count of buffers
 */
static int omap_wb_buffer_setup(struct videobuf_queue *q, unsigned int *count,
			  unsigned int *size)
{
	struct omap_wb_device *wb = q->priv_data;
	int i;

	if (!wb)
		return -EINVAL;

	if (V4L2_BUF_TYPE_VIDEO_CAPTURE != q->type)
		return -EINVAL;

	/* Now allocated the V4L2 buffers */
	/* i is the block-width - either 4K or 8K, depending upon input width*/
	i = (wb->pix.width * wb->bpp +
		TILER_PAGE - 1) & ~(TILER_PAGE - 1);

	/* for NV12 format, buffer is height + height / 2*/
	if (OMAP_DSS_COLOR_NV12 == wb->dss_mode)
		*size = wb->buffer_size = (wb->pix.height * 3/2 * i);
	else
		*size = wb->buffer_size = (wb->pix.height * i);

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
			"\nheight=%d, size = %d, wb->buffer_sz=%d\n",
			wb->pix.height, *size, wb->buffer_size);
#ifdef CONFIG_TILER_OMAP
	if (omap_wb_tiler_buffer_setup(wb, count, 0, &wb->pix))
			return -ENOMEM;
#endif

	if (V4L2_MEMORY_MMAP != wb->memory)
		return 0;

	return 0;
}

/* This function will be called when VIDIOC_QBUF ioctl is called.
 * It prepare buffers before give out for the display. This function
 * user space virtual address into physical address if userptr memory
 * exchange mechanism is used. If rotation is enabled, it copies entire
 * buffer into VRFB memory space before giving it to the DSS.
 */
static int omap_wb_buffer_prepare(struct videobuf_queue *q,
			    struct videobuf_buffer *vb,
			    enum v4l2_field field)
{
	struct omap_wb_device *wb = q->priv_data;

	if (VIDEOBUF_NEEDS_INIT == vb->state) {
		vb->width = wb->pix.width;
		vb->height = wb->pix.height;
		vb->size = vb->width * vb->height * wb->bpp;
		vb->field = field;
	}
	vb->state = VIDEOBUF_PREPARED;

	/* Here, we need to use the physical addresses given by Tiler:
	*/
	wb->queued_buf_addr[vb->i] = (u8 *) wb->buf_phy_addr[vb->i]; 
	wb->queued_buf_uv_addr[vb->i] = (u8 *) wb->buf_phy_uv_addr[vb->i];
	return 0;
}

/* Buffer queue funtion will be called from the videobuf layer when _QBUF
 * ioctl is called. It is used to enqueue buffer, which is ready to be
 * displayed. */
static void omap_wb_buffer_queue(struct videobuf_queue *q,
			  struct videobuf_buffer *vb)
{
	struct omap_wb_device *wb = q->priv_data;

	/* Driver is also maintainig a queue. So enqueue buffer in the driver
	 * queue */
	list_add_tail(&vb->queue, &wb->dma_queue);

	vb->state = VIDEOBUF_QUEUED;
}

/* Buffer release function is called from videobuf layer to release buffer
 * which are already allocated */
static void omap_wb_buffer_release(struct videobuf_queue *q,
			    struct videobuf_buffer *vb)
{
	struct omap_wb_device *wb = q->priv_data;

	vb->state = VIDEOBUF_NEEDS_INIT;

	if (V4L2_MEMORY_MMAP != wb->memory)
		return;
}

/*
 *  File operations
 */
static void omap_wb_vm_open(struct vm_area_struct *vma)
{
	struct omap_wb_device *wb = vma->vm_private_data;

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
	"vm_open [vma=%08lx-%08lx]\n", vma->vm_start, vma->vm_end);
	wb->mmap_count++;
}

static void omap_wb_vm_close(struct vm_area_struct *vma)
{
	struct omap_wb_device *wb = vma->vm_private_data;

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
	"vm_close [vma=%08lx-%08lx]\n", vma->vm_start, vma->vm_end);
	wb->mmap_count--;
}

static struct vm_operations_struct omap_wb_vm_ops = {
	.open = omap_wb_vm_open,
	.close = omap_wb_vm_close,
};

static int omap_wb_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct omap_wb_device *wb = file->private_data;
	struct videobuf_queue *q = &wb->vbq;
	int i;
	void *pos;

	int j = 0, k = 0, m = 0, p = 0, m_increment = 0;

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
		" %s pgoff=0x%lx, start=0x%lx, end=0x%lx\n", __func__,
		vma->vm_pgoff, vma->vm_start, vma->vm_end);

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
		v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev,
		"offset invalid [offset=0x%lx]\n",
			(vma->vm_pgoff << PAGE_SHIFT));
		return -EINVAL;
	}
	q->bufs[i]->baddr = vma->vm_start;

	vma->vm_flags |= VM_RESERVED;
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	vma->vm_ops = &omap_wb_vm_ops;
	vma->vm_private_data = (void *) wb;

	pos = (void*) wb->buf_phy_addr[i];
	/* get line width */
	/* for NV12, Y buffer is 1bpp*/

	if (OMAP_DSS_COLOR_NV12 == wb->dss_mode) {
		p = (wb->pix.width +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);
		m_increment = 64 * TILER_WIDTH;
	} else {
		p = (wb->pix.width * wb->bpp +
			TILER_PAGE - 1) & ~(TILER_PAGE - 1);

		if (wb->bpp > 1)
			m_increment = 2*64*TILER_WIDTH;
		else
			m_increment = 64 * TILER_WIDTH;
	}

	for (j = 0; j < wb->pix.height; j++) {
		/* map each page of the line */

		vma->vm_pgoff =
			((unsigned long)pos + m) >> PAGE_SHIFT;

		if (remap_pfn_range(vma, vma->vm_start + k,
			((unsigned long)pos + m) >> PAGE_SHIFT,
			p, vma->vm_page_prot))
				return -EAGAIN;
		k += p;
		m += m_increment;
	}
	m = 0;

	/* UV Buffer in case of NV12 format */
	if (OMAP_DSS_COLOR_NV12 == wb->dss_mode) {
		pos = (void*) wb->buf_phy_uv_addr[i];
		/* UV buffer is 2 bpp, but half size, so p remains */
		m_increment = 2*64*TILER_WIDTH;

		/* UV buffer is height / 2*/
		for (j = 0; j < wb->pix.height / 2; j++) {
			/* map each page of the line */
			vma->vm_pgoff =
				((unsigned long)pos + m) >> PAGE_SHIFT;

			if (remap_pfn_range(vma, vma->vm_start + k,
				((unsigned long)pos + m) >> PAGE_SHIFT,
				p, vma->vm_page_prot))
				return -EAGAIN;
			k += p;
			m += m_increment;
		}
	}

	vma->vm_flags &= ~VM_IO; /* using shared anonymous pages */
	wb->mmap_count++;
	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "Exiting %s\n", __func__);
	return 0;
}

static int omap_wb_release(struct file *file)
{

	struct omap_wb_device *wb = file->private_data;
	struct videobuf_queue *q;
	unsigned int r = 0;

	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "Entering %s\n", __func__);

	if (!wb)
		return 0;

	q = &wb->vbq;

#ifdef CONFIG_TILER_OMAP
	omap_wb_free_tiler_buffers(wb);
#endif

	videobuf_mmap_free(q);

	/* Even if apply changes fails we should continue
	   freeing allocated memeory */
	if (wb->streaming) {
		u32 mask = 0;

		mask = DISPC_IRQ_FRAMEDONE_WB;

		omap_dispc_unregister_isr(omap_wb_isr, wb, mask);
		wb->streaming = 0;

		videobuf_streamoff(q);
		videobuf_queue_cancel(q);
	}

	if (wb->mmap_count != 0)
		wb->mmap_count = 0;

	wb->opened -= 1;
	file->private_data = NULL;

	if (wb->buffer_allocated)
		videobuf_mmap_free(q);
	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "Exiting %s\n", __func__);
	return r;
}

static int omap_wb_open(struct file *file)
{
	struct omap_wb_device *wb = NULL;
	struct videobuf_queue *q;

	wb = video_drvdata(file);
	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "Entering %s\n", __func__);

	if (wb == NULL)
		return -ENODEV;

	/* for now, we only support single open */
	if (wb->opened)
		return -EBUSY;

	wb->opened += 1;
	wb->enabled = true;
	file->private_data = wb;
	wb->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	q = &wb->vbq;
	video_vbq_ops.buf_setup = omap_wb_buffer_setup;
	video_vbq_ops.buf_prepare = omap_wb_buffer_prepare;
	video_vbq_ops.buf_release = omap_wb_buffer_release;
	video_vbq_ops.buf_queue = omap_wb_buffer_queue;
	spin_lock_init(&wb->vbq_lock);

	videobuf_queue_dma_contig_init(q, &video_vbq_ops, q->dev, &wb->vbq_lock,
			       wb->type, V4L2_FIELD_NONE, sizeof(struct videobuf_buffer),
				wb);	
	v4l2_dbg(1, debug_wb, &wb->wb_dev->v4l2_dev, "Exiting %s\n", __func__);
	return 0;
}

static struct v4l2_file_operations wb_fops = {
	.owner		= THIS_MODULE,
	.ioctl	 = video_ioctl2,
	.mmap	 = omap_wb_mmap,
	.open	 = omap_wb_open,
	.release = omap_wb_release,
};


/* Init functions used during driver intitalization */
/* Initial setup of video_data */
static int __init omap_wb_setup_video_data(struct omap_wb_device *wb)
{
	struct v4l2_pix_format *pix;
	struct video_device *vfd;
	/* set the default pix */
	pix = &wb->pix;

	/* Set the default picture of QVGA  */
	pix->width = QQVGA_WIDTH;
	pix->height = QQVGA_HEIGHT;

	/* Default pixel format is RGB 5-6-5 */
	pix->pixelformat = V4L2_PIX_FMT_RGB32;
	pix->field = V4L2_FIELD_ANY;
	pix->bytesperline = pix->width * 2;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
	pix->colorspace = V4L2_COLORSPACE_JPEG;

	wb->bpp = RGB32_BPP;
	wb->enabled = false;
	wb->buffer_allocated = 0;
	wb->capturemode = V4L2_WB_CAPTURE_ALL;
	wb->source = V4L2_WB_OVERLAY0;
	wb->source_type =  V4L2_WB_SOURCE_OVERLAY;
	wb->dss_mode = OMAP_DSS_COLOR_ARGB32;
	/* initialize the video_device struct */
	vfd = wb->vfd = video_device_alloc();

	if (!vfd) {
		printk(KERN_ERR WB_NAME ": could not allocate"
				" WB device struct\n");
		return -ENOMEM;
	}
	vfd->release = video_device_release;
	vfd->ioctl_ops = &wb_ioctl_fops;

	strlcpy(vfd->name, WB_NAME, sizeof(vfd->name));
	vfd->vfl_type = VFL_TYPE_GRABBER;

	/* need to register for a VID_HARDWARE_* ID in videodev.h */
	vfd->fops = &wb_fops;
	mutex_init(&wb->lock);

	vfd->minor = -1;
	return 0;
}


/* Create wb devices */
static int __init omap_wb_create_video_devices(struct platform_device *pdev)
{
	int r = 0, k;
	struct omap_wb_device *wb;
	struct video_device *vfd = NULL;
	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);

	struct omap2wb_device *wb_dev = container_of(v4l2_dev, struct
			omap2wb_device, v4l2_dev);

	for (k = 0; k < pdev->num_resources; k++) {

		wb = kmalloc(sizeof(struct omap_wb_device), GFP_KERNEL);
		if (!wb) {
			printk(KERN_ERR WB_NAME
					": could not allocate memory\n");
			return -ENOMEM;
		}

		memset(wb, 0, sizeof(struct omap_wb_device));

		wb->wid = k;
		wb->wb_dev = wb_dev;

		/* Setup the default configuration for the wb devices
		 */
		if (omap_wb_setup_video_data(wb) != 0) {
			r = -ENOMEM;
			goto error;
		}

		/* Register the Video device with V4L2
		 */
		vfd = wb->vfd;
		if (video_register_device(vfd, VFL_TYPE_GRABBER, k + 1) < 0) {
			printk(KERN_ERR WB_NAME ": could not register "
					"Video for Linux device\n");
			vfd->minor = -1;
			r = -ENODEV;
			goto error1;
		}
		video_set_drvdata(vfd, wb);

		goto success;

error1:
	video_device_release(vfd);
error:
	kfree(wb);
	return r;

success:
	printk(KERN_INFO WB_NAME ": registered and initialized "
			"wb device %d [v4l2]\n", vfd->minor);
	if (k == (pdev->num_resources - 1))
		return 0;
	}
	return -ENODEV;

}

/* Driver functions */

static void omap_wb_cleanup_device(struct omap_wb_device *wb)
{
	struct video_device *vfd;

	if (!wb)
		return;

	vfd = wb->vfd;

	if (vfd) {
		if (vfd->minor == -1) {
			/*
			 * The device was never registered, so release the
			 * video_device struct directly.
			 */
			video_device_release(vfd);
		} else {
			/*
			 * The unregister function will release the video_device
			 * struct as well as unregistering it.
			 */
			video_unregister_device(vfd);
		}
	}

#ifdef CONFIG_TILER_OMAP
	omap_wb_free_tiler_buffers(wb);
#endif

	kfree(wb);
}

static int omap_wb_remove(struct platform_device *pdev)
{

	struct v4l2_device *v4l2_dev = platform_get_drvdata(pdev);
	struct omap2wb_device *wb_dev = container_of(v4l2_dev, struct
			omap2wb_device, v4l2_dev);
	int k;

	v4l2_device_unregister(v4l2_dev);
	for (k = 0; k < pdev->num_resources; k++)
		omap_wb_cleanup_device(wb_dev->wb);

	kfree(wb_dev);

	return 0;
}

static int __init omap_wb_probe(struct platform_device *pdev)
{
	int r = 0;
	struct omap2wb_device *wb_dev = NULL;

	if (pdev->num_resources == 0) {
		dev_err(&pdev->dev, "probed for an unknown device\n");
		r = -ENODEV;
		return r;
	}

	wb_dev = kzalloc(sizeof(struct omap2wb_device), GFP_KERNEL);
	if (wb_dev == NULL) {
		r = -ENOMEM;
		return r;
	}

	if (v4l2_device_register(&pdev->dev, &wb_dev->v4l2_dev) < 0) {
		printk(KERN_ERR WB_NAME "v4l2_device_register failed\n");
		return -ENODEV;
	}

	r = omap_wb_create_video_devices(pdev);
	if (r)
		goto error0;

	return 0;

error0:
	kfree(wb_dev);
	return r;
}

static struct platform_driver omap_wb_driver = {
	.driver = {
		   .name = WB_NAME,
		   },
	.probe = omap_wb_probe,
	.remove = omap_wb_remove,
};
void omap_wb_isr(void *arg, unsigned int irqstatus)
{
	struct timeval timevalue = {0};
	struct omap_wb_device *wb = (struct omap_wb_device *) arg;
	unsigned long flags;

	spin_lock_irqsave(&wb->vbq_lock, flags);

	if (!wb->first_int && (wb->cur_frm != wb->next_frm)) {
		wb->cur_frm->ts = timevalue;
		wb->cur_frm->state = VIDEOBUF_DONE;
		wake_up_interruptible(&wb->cur_frm->done);
		wb->cur_frm = wb->next_frm;
	}

	wb->first_int = 0;

	if (list_empty(&wb->dma_queue)) {
		spin_unlock_irqrestore(&wb->vbq_lock, flags);
		return;
	}

	omap_wb_process_frame(wb);

	spin_unlock_irqrestore(&wb->vbq_lock, flags);
}

static int __init omap_wb_init(void)
{

	if (platform_driver_register(&omap_wb_driver) != 0) {
		printk(KERN_ERR WB_NAME ": could not register \
				WB driver\n");
		return -EINVAL;
	}
	mutex_init(&wb_lock);
	return 0;
}

static void omap_wb_cleanup(void)
{
	platform_driver_unregister(&omap_wb_driver);
}

late_initcall(omap_wb_init);
module_exit(omap_wb_cleanup);

