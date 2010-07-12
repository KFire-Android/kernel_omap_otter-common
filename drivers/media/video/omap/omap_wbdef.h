/*
 * drivers/media/video/omap/omap_wbdef.h
 *
 * Copyright (C) 2009 Texas Instruments.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <plat/display.h>
#include <linux/videodev2.h>

#define YUYV_BPP        2
#define RGB565_BPP      2
#define RGB24_BPP       3
#define RGB32_BPP       4
#define TILE_SIZE       32

struct omap2wb_device {
	struct mutex  mtx;

	int state;
	struct v4l2_device v4l2_dev;
	struct omap_wb_device *wb;
};

struct omap_wb_device {

	struct video_device *vfd;
	int wid;
	int opened;
	struct omap2wb_device *wb_dev;
	/* we don't allow to change image fmt/size once buffer has
	 * been allocated
	 */
	int buffer_allocated;
	/* allow to reuse previously allocated buffer which is big enough */
	int buffer_size;
	/* keep buffer info across opens */
	unsigned long buf_virt_addr[VIDEO_MAX_FRAME];
	unsigned long buf_phy_addr[VIDEO_MAX_FRAME];
	/* keep which buffers we actually allocated (via tiler) */
	unsigned long buf_phy_uv_addr_alloced[VIDEO_MAX_FRAME];
	unsigned long buf_phy_addr_alloced[VIDEO_MAX_FRAME];

	/* NV12 support*/
	unsigned long buf_phy_uv_addr[VIDEO_MAX_FRAME];
	u8 *queued_buf_uv_addr[VIDEO_MAX_FRAME];

	enum omap_color_mode dss_mode;

	/* we don't allow to request new buffer when old buffers are
	 * still mmaped
	 */
	int mmap_count;

	spinlock_t vbq_lock;		/* spinlock for videobuf queues */
	unsigned long field_count;	/* field counter for videobuf_buffer */

	bool enabled;
	/* non-NULL means streaming is in progress. */
	bool streaming;

	struct v4l2_pix_format pix;
	struct v4l2_window win;

	/* Lock to protect the shared data structures in ioctl */
	struct mutex lock;

	int bpp; /* bytes per pixel */

	int ps, vr_ps, line_length, first_int, field_id;
	enum v4l2_memory memory;
	struct videobuf_buffer *cur_frm, *next_frm;
	struct list_head dma_queue;
	u8 *queued_buf_addr[VIDEO_MAX_FRAME];
	void *isr_handle;

	/* Buffer queue variables */
	struct omap_wb_device *wb;
	enum v4l2_buf_type type;
	struct videobuf_queue vbq;
	int io_allowed;

	/*write back specific*/
	enum v4l2_writeback_source			source;
	enum v4l2_writeback_source_type		source_type;
	enum v4l2_writeback_capturemode		capturemode;
	bool buf_empty;
};
