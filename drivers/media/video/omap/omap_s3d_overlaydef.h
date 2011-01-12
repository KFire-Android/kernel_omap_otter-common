/*
 * drivers/media/video/omap/omap_s3d_overlaydef.h
 *
 * Copyright (C) 2010 Texas Instruments.
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#ifndef OMAP_S3D_VOUTDEF_H
#define OMAP_S3D_VOUTDEF_H

#include <plat/display.h>

#define YUYV_BPP        2
#define RGB565_BPP      2
#define RGB24_BPP       3
#define RGB32_BPP       4
#define TILE_SIZE       32

#define VIDIOC_PRIVATE_S3D_S_OFFS \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct v4l2_s3d_offsets)
#define VIDIOC_PRIVATE_S3D_S_OUT_FMT \
		_IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct v4l2_frame_packing)


#define V4L2_CID_PRIVATE_DISPLAY_ID (V4L2_CID_PRIVATE_BASE+1)
#define V4L2_CID_PRIVATE_ANAGLYPH_TYPE (V4L2_CID_PRIVATE_BASE+2)
#define V4L2_CID_PRIVATE_S3D_MODE (V4L2_CID_PRIVATE_BASE+3)
#define V4L2_CID_PRIVATE_LASTP1 (V4L2_CID_PRIVATE_BASE+4)

/*TODO: add all the other anaglyph modes*/
enum v4l2_anaglyph_type {
	/*Left view Red, right view Cyan */
	V4L2_ANAGLYPH_RED_CYAN = 0,
	/*Left view Red, right view Blue */
	V4L2_ANAGLYPH_RED_BLUE = 1,
	/*Left view Green, right view Magenta */
	V4L2_ANAGLYPH_GR_MAG,
	V4L2_ANAGLYPH_MAX
};

enum v4l2_s3d_mode {
	V4L2_S3D_MODE_OFF = 0,
	V4L2_S3D_MODE_ON,
	V4L2_S3D_MODE_ANAGLYPH,
	V4L2_S3D_MODE_MAX
};

enum v4l2_frame_pack_type {
	V4L2_FPACK_NONE = 0,
	V4L2_FPACK_OVERUNDER,
	V4L2_FPACK_SIDEBYSIDE,
	V4L2_FPACK_ROW_IL,
	V4L2_FPACK_COL_IL,
	V4L2_FPACK_PIX_IL,
	V4L2_FPACK_CHECKB,
	V4L2_FPACK_FRM_SEQ,
};

enum v4l2_frame_pack_order {
	V4L2_FPACK_ORDER_LF = 0,
	V4L2_FPACK_ORDER_RF,
};

enum v4l2_frame_pack_sub_sample {
	V4L2_FPACK_SS_NONE = 0,
	V4L2_FPACK_SS_HOR,
	V4L2_FPACK_SS_VERT,
};

struct v4l2_frame_packing {
	enum v4l2_frame_pack_type type;
	enum v4l2_frame_pack_order order;
	enum v4l2_frame_pack_sub_sample sub_samp;
};

struct v4l2_s3d_offsets {
	/*Offset from base address to active left view start */
	unsigned long l;
	/*Offset from base address to active right view start */
	unsigned long r;
	/*Cropping w and h */
	unsigned int w;
	unsigned int h;
};

enum s3d_view {
	S3D_VIEW_L = 0,
	S3D_VIEW_R,
	S3D_VIEW_BASE,
};

/*Describes if the overlay is only pulling the left or right view
* or all of the input.
* USING_ALL state is only valid for ROLE_NORMAL and ROLE_WB
*/
enum s3d_ovl_state {
	OVL_ST_INIT = 0,
	OVL_ST_FETCH_L,
	OVL_ST_FETCH_R,
	OVL_ST_FETCH_ALL,
};

/*The formatter state when using WB pipeline*/
enum omap_wb_state {
	WB_INIT = 0,
	WB_BUSY,
	WB_IDLE,
};

enum s3d_overlay_role {
	OVL_ROLE_NONE = 0,
	OVL_ROLE_WB = 1 << 0,
	OVL_ROLE_L = 1 << 1,
	OVL_ROLE_R = 1 << 2,
	OVL_ROLE_DISP = OVL_ROLE_L | OVL_ROLE_R,
};

struct s3d_overlay {
	struct list_head list;

	struct omap_overlay *dssovl;
	enum omap_dss_rotation_angle rotation;
	u8 alpha;
	bool vflip;
	/*specified after rotation and cropping */
	struct {
		unsigned int w;
		unsigned int h;
	} src;

	/*Specified after rotation */
	struct {
		unsigned int x;
		unsigned int y;
		unsigned int w;
		unsigned int h;
	} dst;

	struct s3d_buffer_queue *queue;
	/*ignored unless role= ROLE_WB */
	struct s3d_buffer_queue *wb_queue;

	enum s3d_overlay_role role;
	enum s3d_ovl_state state;

	bool update_dss;

};

struct s3d_buffer_queue {
	unsigned int bpp;
	unsigned int width;
	unsigned int height;
	unsigned int crop_w;
	unsigned int crop_h;

	enum omap_color_mode color_mode;
	struct v4l2_frame_packing frame_packing;

	bool uses_tiler;
	bool cached;

	unsigned int mmap_count;
	unsigned int n_alloc;
	size_t buf_siz;

	void *virt_addr[VIDEO_MAX_FRAME];
	unsigned long phy_addr[VIDEO_MAX_FRAME];
	unsigned long phy_uv_addr[VIDEO_MAX_FRAME];

	unsigned long offset[VIDEO_MAX_FRAME];
	unsigned long uv_offset[VIDEO_MAX_FRAME];

	/* tiler specific */
	void *tiler_alloc_ptr[VIDEO_MAX_FRAME];
	void *tiler_alloc_uv_ptr[VIDEO_MAX_FRAME];

};

struct wb_buffer {
	struct videobuf_buffer *videobuf;
	struct list_head queue;
};

struct s3d_formatter_info {
	enum omap_wb_state wb_state;
	unsigned int pend_wb_passes;

	struct videobuf_buffer *wb_cur_buf;
	struct list_head wb_videobuf_q;
	struct wb_buffer wb_buffers[VIDEO_MAX_FRAME];
};

struct s3d_formatter_config {
	/*Flip between views for each WB pass or overlay update */
	bool alt_view;
	/*Skips every other line when fetching input */
	bool in_skip_lines;
	bool use_wb;
	bool wb_needs_tiler;
	/*Skips every other line when writing */
	bool wb_skip_lines;
	bool a_view_per_ovl;

	unsigned int in_rotation;
	unsigned int out_rotation;

	unsigned int wb_passes;
	unsigned int wb_ovls;
	unsigned int disp_ovls;

	/*Input and output sizes for displayable and WB overlays */
	unsigned int src_w;
	unsigned int src_h;
	unsigned int dst_w;
	unsigned int dst_h;

	/*WB src are implied to be dst_w and dst_h */
	/*wb_dst w/h specify additional scaling */
	unsigned int wb_dst_w;
	unsigned int wb_dst_h;

	/*Ignored if not using writeback, when used
	 *they specify the size for the displayable overlay*/
	unsigned int disp_w;
	unsigned int disp_h;

	/*Specifies top-left coordinate, if only one display overlay is used
	 * r_pos_x and r_pos_y are ignored*/
	unsigned int l_pos_x;
	unsigned int l_pos_y;
	unsigned int r_pos_x;
	unsigned int r_pos_y;

};

struct s3d_ovl_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
	struct mutex lock;
	unsigned int opened;
	bool streaming;
	enum omap_color_mode supported_modes;

	bool vflip;
	bool hflip;
	unsigned int rotation;
	enum v4l2_s3d_mode s3d_mode;
	enum v4l2_anaglyph_type anaglyph_type;

	/* list of s3d_overlay pointers */
	struct list_head overlays;

	unsigned int cur_disp_idx;
	struct omap_dss_device *cur_disp;

	struct s3d_formatter_config fter_config;
	struct s3d_formatter_info fter_info;

	struct videobuf_buffer *cur_buf, *next_buf;
	struct list_head videobuf_q;
	struct videobuf_queue vbq;
	spinlock_t vbq_lock;
	struct s3d_buffer_queue in_q;
	struct s3d_buffer_queue out_q;

	struct v4l2_pix_format pix;
	struct v4l2_rect crop;
	struct v4l2_window win;
	struct v4l2_framebuffer fbuf;
	struct v4l2_s3d_offsets offsets;

	bool override_s3d_disp;
	struct s3d_disp_info s3d_disp_info;

	/* workqueue for manual update */
	struct workqueue_struct *workqueue;
};

/* manual display update work */
struct s3d_disp_upd_work {
	struct omap_dss_device *disp;
	struct work_struct work;
};

/*External function prototypes*/
int omap_dss_wb_apply(struct omap_overlay_manager *mgr,
		      struct omap_writeback *wb);
int omap_dss_wb_flush(void);

#endif /* ifndef OMAP_S3D_VOUTDEF_H */
