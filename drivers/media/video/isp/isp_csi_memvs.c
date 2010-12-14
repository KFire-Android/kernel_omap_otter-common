/*
 * isp_csi_memvs.c - ISP CSI Virtual sensor
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Leverage imx046.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

#include <linux/mm.h>
#include <linux/slab.h>

#include <media/v4l2-int-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>

#include <asm/pgalloc.h>

#include "../omap34xxcam.h"
#include "isp.h"
#include "isp_csi.h"

#define PHY_ADDRESS_ALIGN		32

#define ISP_CSI_MEMVS_DRIVER_NAME  "omap3"
#define ISP_CSI_MEMVS_CARD_NAME  "omap3/virtual//"
#define ISP_CSI_MEMVS_MOD_NAME "ISP_CSI_MEMVS: "

#define VSENSOR_OUT_WIDTH	3280
#define VSENSOR_OUT_HEIGHT	2464

#define ISP_CSI_MEMVS_BIGGEST_FRAME_BYTE_SIZE \
		PAGE_ALIGN(VSENSOR_OUT_WIDTH * VSENSOR_OUT_HEIGHT * 2)

#define to_omap34xxcam_fh(vfh)				\
	container_of(vfh, struct omap34xxcam_fh, vfh)

struct isp_csi_memvs_buf {
	dma_addr_t isp_addr;
	struct videobuf_buffer *vb;
	u32 vb_state;
};

struct isp_csi_memvs_bufs {
	dma_addr_t isp_addr_capture[VIDEO_MAX_FRAME];
	struct isp_csi_memvs_buf buf[VIDEO_MAX_FRAME];
	int queue;
	int done;
	int wait_hs_vs;
};

/**
 * struct isp_csi_memvs_sensor - main structure for sensor information
 * @v4l2_int_device: V4L2 device structure structure
 * @in_pix: V4L2 pixel format information structure
 * @out_pix: V4L2 pixel format information structure
 */
struct isp_csi_memvs_sensor {
	struct mutex mutex; /* serialises access to this structure */

	struct v4l2_int_device	*v4l2_int_device;
	struct video_device	*out_vfd;
	struct v4l2_pix_format	in_pix;
	struct v4l2_pix_format	out_pix;
	struct v4l2_streamparm	sparams;
	struct v4l2_fract	tpframe;
	bool opened;

	struct device *isp;
	struct isp_csi_memvs_bufs bufs;
};

struct isp_csi_memvs_fh {
	struct isp_csi_memvs_sensor *sensor;

	spinlock_t vbq_lock;		/* spinlock for videobuf queues */
	struct videobuf_queue vbq;
};

/* list of image formats supported by isp_csi_memvs sensor */
const static struct v4l2_fmtdesc isp_csi_memvs_formats[] = {
	{
		.description	= "Bayer10 (GR/BG)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG10,
	},
	{
		.description	= "Bayer10 (RG/GB)",
		.pixelformat	= V4L2_PIX_FMT_SRGGB10,
	},
	{
		.description	= "Bayer10 (BG/GR)",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
	},
	{
		.description	= "Bayer10 (GB/RG)",
		.pixelformat	= V4L2_PIX_FMT_SGBRG10,
	}
};

#define NUM_CAPTURE_FORMATS ARRAY_SIZE(isp_csi_memvs_formats)

struct isp_csi_memvs_sensor *isp_csi_memvs;

/**
 * ioctl_enum_fmt_cap - Implement the CAPTURE buffer VIDIOC_ENUM_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @fmt: standard V4L2 VIDIOC_ENUM_FMT ioctl structure
 *
 * Implement the VIDIOC_ENUM_FMT ioctl for the CAPTURE buffer type.
 */
static int ioctl_enum_fmt_cap(struct v4l2_int_device *s,
			      struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	memset(fmt, 0, sizeof(*fmt));
	fmt->index = index;
	fmt->type = type;

	switch (fmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		if (index >= NUM_CAPTURE_FORMATS)
			return -EINVAL;
	break;
	default:
		return -EINVAL;
	}

	fmt->flags = isp_csi_memvs_formats[index].flags;
	strlcpy(fmt->description, isp_csi_memvs_formats[index].description,
					sizeof(fmt->description));
	fmt->pixelformat = isp_csi_memvs_formats[index].pixelformat;

	return 0;
}

/**
 * ioctl_try_fmt_cap - Implement the CAPTURE buffer VIDIOC_TRY_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_TRY_FMT ioctl structure
 *
 * Implement the VIDIOC_TRY_FMT ioctl for the CAPTURE buffer type.  This
 * ioctl is used to negotiate the image capture size and pixel format
 * without actually making it take effect.
 */
static int ioctl_try_fmt_cap(struct v4l2_int_device *s,
			     struct v4l2_format *f)
{
	struct v4l2_pix_format *pix = &f->fmt.pix;
	struct isp_csi_memvs_sensor *sensor = s->priv;

	if (pix->pixelformat != sensor->in_pix.pixelformat)
		return -EINVAL;

	*pix = sensor->in_pix;
	pix->field = V4L2_FIELD_NONE;
	pix->bytesperline = ALIGN(pix->width * ISP_BYTES_PER_PIXEL,
				  PHY_ADDRESS_ALIGN);
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
	pix->colorspace = V4L2_COLORSPACE_SRGB;
	return 0;
}

/**
 * ioctl_s_fmt_cap - V4L2 sensor interface handler for VIDIOC_S_FMT ioctl
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 VIDIOC_S_FMT ioctl structure
 *
 * If the requested format is supported, configures the HW to use that
 * format, returns error code if format not supported or HW can't be
 * correctly configured.
 */
static int ioctl_s_fmt_cap(struct v4l2_int_device *s,
				struct v4l2_format *f)
{
	struct isp_csi_memvs_sensor *sensor = s->priv;
	struct v4l2_pix_format *pix = &f->fmt.pix;
	int rval;

	rval = ioctl_try_fmt_cap(s, f);
	if (!rval)
		sensor->out_pix = *pix;

	return rval;
}

/**
 * ioctl_g_fmt_cap - V4L2 sensor interface handler for ioctl_g_fmt_cap
 * @s: pointer to standard V4L2 device structure
 * @f: pointer to standard V4L2 v4l2_format structure
 *
 * Returns the sensor's current pixel format in the v4l2_format
 * parameter.
 */
static int ioctl_g_fmt_cap(struct v4l2_int_device *s,
				struct v4l2_format *f)
{
	struct isp_csi_memvs_sensor *sensor = s->priv;
	f->fmt.pix = sensor->out_pix;

	return 0;
}

/**
 * ioctl_g_pixclk - V4L2 sensor interface handler for ioctl_g_pixclk
 * @s: pointer to standard V4L2 device structure
 * @pixclk: pointer to unsigned 32 var to store pixelclk in HZ
 *
 * Returns the sensor's current pixel clock in HZ
 */
static int ioctl_priv_g_pixclk(struct v4l2_int_device *s, u32 *pixclk)
{
	*pixclk = 0;

	return 0;
}

/**
 * ioctl_g_activesize - V4L2 sensor interface handler for ioctl_g_activesize
 * @s: pointer to standard V4L2 device structure
 * @pix: pointer to standard V4L2 v4l2_rect structure
 *
 * Returns the sensor's current active image basesize.
 */
static int ioctl_priv_g_activesize(struct v4l2_int_device *s,
			      struct v4l2_rect *pix)
{
	struct isp_csi_memvs_sensor *sensor = s->priv;

	pix->top = 0;
	pix->left = 0;
	pix->width = sensor->out_pix.width;
	pix->height = sensor->out_pix.height;

	return 0;
}

/**
 * ioctl_g_fullsize - V4L2 sensor interface handler for ioctl_g_fullsize
 * @s: pointer to standard V4L2 device structure
 * @pix: pointer to standard V4L2 v4l2_rect structure
 *
 * Returns the sensor's biggest image basesize.
 */
static int ioctl_priv_g_fullsize(struct v4l2_int_device *s,
			    struct v4l2_rect *pix)
{
	struct isp_csi_memvs_sensor *sensor = s->priv;

	pix->top = 0;
	pix->left = 0;
	pix->width = sensor->out_pix.width;
	pix->height = sensor->out_pix.height;

	return 0;
}

/**
 * ioctl_g_pixelsize - V4L2 sensor interface handler for ioctl_g_pixelsize
 * @s: pointer to standard V4L2 device structure
 * @pix: pointer to standard V4L2 v4l2_pix_format structure
 *
 * Returns the sensor's configure pixel size.
 */
static int ioctl_priv_g_pixelsize(struct v4l2_int_device *s,
			    struct v4l2_rect *pix)
{
	pix->left	= 0;
	pix->top	= 0;
	pix->width	= 1;
	pix->height	= 1;

	return 0;
}

/**
 * ioctl_g_parm - V4L2 sensor interface handler for VIDIOC_G_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_G_PARM ioctl structure
 *
 * Returns the sensor's video CAPTURE parameters.
 */
static int ioctl_g_parm(struct v4l2_int_device *s,
			     struct v4l2_streamparm *a)
{
	struct v4l2_captureparm *cparm = &a->parm.capture;
	struct isp_csi_memvs_sensor *sensor = s->priv;

	if (a->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	/* Return stored or default values */
	*a = sensor->sparams;
	cparm->capability = V4L2_CAP_TIMEPERFRAME;
	cparm->timeperframe = sensor->tpframe;

	return 0;
}

/**
 * ioctl_s_parm - V4L2 sensor interface handler for VIDIOC_S_PARM ioctl
 * @s: pointer to standard V4L2 device structure
 * @a: pointer to standard V4L2 VIDIOC_S_PARM ioctl structure
 *
 * Configures the sensor to use the input parameters, if possible.  If
 * not possible, reverts to the old parameters and returns the
 * appropriate error code.
 */
static int ioctl_s_parm(struct v4l2_int_device *s,
			struct v4l2_streamparm *a)
{
	struct v4l2_captureparm *cparm = &a->parm.capture;
	struct isp_csi_memvs_sensor *sensor = s->priv;

	sensor->sparams = *a;
	sensor->tpframe = cparm->timeperframe;

	return 0;
}

static struct omap34xxcam_sensor_config isp_csi_memvs_hwc = {
	.sensor_isp  = 0,
	.capture_mem = ISP_CSI_MEMVS_BIGGEST_FRAME_BYTE_SIZE * 2,
	.ival_default	= { 1, 10 },
};

#define ISP_DUMMY_MCLK		216000000
static struct isp_interface_config isp_csi_memvs_if_config = {
	.ccdc_par_ser 		= ISP_CSIB,
	.dataline_shift 	= 0x0,
	.hsvs_syncdetect 	= ISPCTRL_SYNC_DETECT_VSRISE,
	.strobe 		= 0x0,
	.prestrobe 		= 0x0,
	.shutter 		= 0x0,
	.wenlog 		= ISPCCDC_CFG_WENLOG_AND,
	.wait_hs_vs		= 0,
	.cam_mclk		= ISP_DUMMY_MCLK,
	.u.csi.use_mem_read	= 0x1,
	.u.csi.crc 		= 0x0,
	.u.csi.mode 		= 0x0,
	.u.csi.edge 		= 0x0,
	.u.csi.signalling 	= 0x0,
	.u.csi.strobe_clock_inv = 0x0,
	.u.csi.vs_edge 		= 0x0,
	.u.csi.channel 		= 0x0,
	.u.csi.vpclk 		= 0x1,
	.u.csi.data_start 	= 0x0,
	.u.csi.data_size 	= 0x0,
	.u.csi.format 		= V4L2_PIX_FMT_SGRBG10,
};

/**
 * ioctl_g_priv - V4L2 sensor interface handler for vidioc_int_g_priv_num
 * @s: pointer to standard V4L2 device structure
 * @p: void pointer to hold sensor's private data address
 *
 * Returns device's (sensor's) private data area address in p parameter
 */
static int ioctl_g_priv(struct v4l2_int_device *s, void *p)
{
	struct omap34xxcam_hw_config *hwc = p;

	hwc->u.sensor		= isp_csi_memvs_hwc;
	hwc->dev_index		= 3;
	hwc->dev_minor		= 6;
	hwc->dev_type		= OMAP34XXCAM_SLAVE_SENSOR;

	return 0;
}

/**
 * ioctl_s_power - V4L2 sensor interface handler for vidioc_int_s_power_num
 * @s: pointer to standard V4L2 device structure
 * @on: power state to which device is to be set
 *
 * Sets devices power state to requrested state, if possible.
 */
static int ioctl_s_power(struct v4l2_int_device *s, enum v4l2_power on)
{
	struct omap34xxcam_videodev *vdev = s->u.slave->master->priv;
	struct device *dev = vdev->cam->isp;
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_csi_memvs_sensor *sensor = s->priv;
	struct v4l2_rect src_rect;

	switch (on) {
	case V4L2_POWER_ON:
		/* Configure interface memory dimensions */
		src_rect.top = 0;
		src_rect.left = 0;
		src_rect.width = sensor->in_pix.width;
		src_rect.height = sensor->in_pix.height;

		isp_csi_memvs_if_config.u.csi.mem_src_rect = src_rect;

		isp->isp_csi.lcm_src_addr = sensor->bufs.isp_addr_capture[0];
		isp->isp_csi.lcm_src_ofst = sensor->in_pix.width * 2;

		isp_configure_interface(dev, &isp_csi_memvs_if_config);
		break;
	case V4L2_POWER_OFF:
		/* Hmmm... Not sure here */
		break;
	case V4L2_POWER_STANDBY:
		break;
	}

	return 0;
}

/**
 * ioctl_dev_exit - V4L2 sensor interface handler for vidioc_int_dev_exit_num
 * @s: pointer to standard V4L2 device structure
 *
 * Delinitialise the dev. at slave detach.  The complement of ioctl_dev_init.
 */
static int ioctl_dev_exit(struct v4l2_int_device *s)
{
	return 0;
}

/**
 * ioctl_dev_init - V4L2 sensor interface handler for vidioc_int_dev_init_num
 * @s: pointer to standard V4L2 device structure
 *
 * Initialise the device when slave attaches to the master.  Returns 0 if
 * isp_csi_memvs device could be found, otherwise returns appropriate error.
 */
static int ioctl_dev_init(struct v4l2_int_device *s)
{
	/* NOTE: Shall we init something here? */

	printk(KERN_INFO ISP_CSI_MEMVS_DRIVER_NAME
		 " omap3isp CSI memvs initted\n");
	return 0;
}

/**
 * ioctl_enum_framesizes - V4L2 sensor if handler for vidioc_int_enum_framesizes
 * @s: pointer to standard V4L2 device structure
 * @frms: pointer to standard V4L2 framesizes enumeration structure
 *
 * Returns possible framesizes depending on choosen pixel format
 **/
static int ioctl_enum_framesizes(struct v4l2_int_device *s,
				 struct v4l2_frmsizeenum *frms)
{
	struct isp_csi_memvs_sensor *sensor = s->priv;

	if (frms->pixel_format != sensor->in_pix.pixelformat)
		return -EINVAL;

	/* Check that the index we are being asked for is not
	   out of bounds. */
	if (frms->index != 0)
		return -EINVAL;

	frms->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	frms->discrete.width = sensor->in_pix.width;
	frms->discrete.height = sensor->in_pix.height;

	return 0;
}

const struct v4l2_fract isp_csi_memvs_frameintervals[] = {
	{ .numerator = 1, .denominator = 10 },
};

static int ioctl_enum_frameintervals(struct v4l2_int_device *s,
					struct v4l2_frmivalenum *frmi)
{
	struct isp_csi_memvs_sensor *sensor = s->priv;

	if (frmi->pixel_format != sensor->in_pix.pixelformat)
		return -EINVAL;

	/* Check that the index we are being asked for is not
	   out of bounds. */
	if (frmi->index >= ARRAY_SIZE(isp_csi_memvs_frameintervals))
		return -EINVAL;

	frmi->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	frmi->discrete.numerator =
			isp_csi_memvs_frameintervals[frmi->index].numerator;
	frmi->discrete.denominator =
			isp_csi_memvs_frameintervals[frmi->index].denominator;

	return 0;
}

static struct v4l2_int_ioctl_desc isp_csi_memvs_int_ioctl_desc[] = {
	{ .num = vidioc_int_enum_framesizes_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_enum_framesizes},
	{ .num = vidioc_int_enum_frameintervals_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_enum_frameintervals},
	{ .num = vidioc_int_dev_init_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_dev_init},
	{ .num = vidioc_int_dev_exit_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_dev_exit},
	{ .num = vidioc_int_s_power_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_power },
	{ .num = vidioc_int_g_priv_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_priv },
	{ .num = vidioc_int_enum_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_enum_fmt_cap },
	{ .num = vidioc_int_try_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_try_fmt_cap },
	{ .num = vidioc_int_g_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_fmt_cap },
	{ .num = vidioc_int_s_fmt_cap_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_fmt_cap },
	{ .num = vidioc_int_g_parm_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_g_parm },
	{ .num = vidioc_int_s_parm_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_s_parm },
	{ .num = vidioc_int_priv_g_pixclk_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_priv_g_pixclk },
	{ .num = vidioc_int_priv_g_activesize_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_priv_g_activesize },
	{ .num = vidioc_int_priv_g_fullsize_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_priv_g_fullsize },
	{ .num = vidioc_int_priv_g_pixelsize_num,
	  .func = (v4l2_int_ioctl_func *)ioctl_priv_g_pixelsize },
};

static struct v4l2_int_slave isp_csi_memvs_slave = {
	.ioctls = isp_csi_memvs_int_ioctl_desc,
	.num_ioctls = ARRAY_SIZE(isp_csi_memvs_int_ioctl_desc),
};

static struct v4l2_int_device isp_csi_memvs_int_device = {
	.module = THIS_MODULE,
	.name = ISP_CSI_MEMVS_DRIVER_NAME,
	.type = v4l2_int_type_slave,
	.u = {
		.slave = &isp_csi_memvs_slave,
	},
};

/* "Output" device IOCTLs */
static int vidioc_querycap(struct file *file, void *fh,
		struct v4l2_capability *cap)
{
	strlcpy(cap->driver, ISP_CSI_MEMVS_DRIVER_NAME, sizeof(cap->driver));
	strlcpy(cap->card, ISP_CSI_MEMVS_CARD_NAME, sizeof(cap->card));
	cap->bus_info[0] = '\0';
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;

	return 0;
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *fh,
				   struct v4l2_fmtdesc *f)
{
	int index = f->index;
	enum v4l2_buf_type type = f->type;

	if (index >= NUM_CAPTURE_FORMATS)
		return -EINVAL;

	memset(f, 0, sizeof(*f));
	f->index = index;
	f->type = type;

	f->flags = isp_csi_memvs_formats[index].flags;
	strlcpy(f->description, isp_csi_memvs_formats[index].description,
			sizeof(f->description));
	f->pixelformat = isp_csi_memvs_formats[index].pixelformat;

	return 0;
}

static int vidioc_g_fmt_vid_out(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct isp_csi_memvs_fh *ofh = fh;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;

	f->fmt.pix = sensor->in_pix;
	return 0;
}

static int vidioc_try_fmt_vid_out(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct isp_csi_memvs_fh *ofh = fh;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;
	struct isp_device *isp = dev_get_drvdata(sensor->isp);
	struct v4l2_pix_format *in_pix_user = &f->fmt.pix;
	struct v4l2_rect try_rect;
	int ifmt, ret = 0;

	for (ifmt = 0; ifmt < NUM_CAPTURE_FORMATS; ifmt++) {
		if (in_pix_user->pixelformat ==
		    isp_csi_memvs_formats[ifmt].pixelformat)
			break;
	}
	if (ifmt == NUM_CAPTURE_FORMATS)
		return -EINVAL;

	/* Check size constraints */
	try_rect.width = in_pix_user->width;
	try_rect.height = in_pix_user->height;
	try_rect.left = 0;
	try_rect.top = 0;

	ret = isp_csi_lcm_validate_src_region(&isp->isp_csi, try_rect);
	if (ret)
		return -EINVAL;

	/* Set bytes per line aligned to 32 bytes, User must be aware must
	 * add padding bytes to every line in input buffer */
	in_pix_user->bytesperline = ALIGN(in_pix_user->width *
					  ISP_BYTES_PER_PIXEL,
					  PHY_ADDRESS_ALIGN);
	in_pix_user->field = V4L2_FIELD_NONE;
	in_pix_user->sizeimage = PAGE_ALIGN(in_pix_user->bytesperline *
					    in_pix_user->height);
	in_pix_user->colorspace = V4L2_COLORSPACE_SRGB;

	return 0;
}

static int vidioc_s_fmt_vid_out(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct isp_csi_memvs_fh *ofh = fh;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;
	int ret = 0;

	ret = vidioc_try_fmt_vid_out(file, fh, f);
	if (!ret)
		sensor->in_pix = f->fmt.pix;

	return ret;
}

static int vidioc_reqbufs(struct file *file, void *fh,
			  struct v4l2_requestbuffers *b)
{
	struct isp_csi_memvs_fh *ofh = fh;

	if ((b->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) || (b->count < 0))
		return -EINVAL;

	return videobuf_reqbufs(&ofh->vbq, b);
}

static int vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct isp_csi_memvs_fh *ofh = fh;

	return videobuf_querybuf(&ofh->vbq, b);
}

static int vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct isp_csi_memvs_fh *ofh = fh;

	return videobuf_qbuf(&ofh->vbq, b);
}

static int vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct isp_csi_memvs_fh *ofh = fh;

	return videobuf_dqbuf(&ofh->vbq, (struct v4l2_buffer *)b,
			      (file->f_flags & O_NONBLOCK) ? 1 : 0);
}

static int vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct isp_csi_memvs_fh *ofh = fh;

	return videobuf_streamon(&ofh->vbq);;
}

static int vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct isp_csi_memvs_fh *ofh = fh;

	return videobuf_streamoff(&ofh->vbq);
}

/**
 * vidioc_default - private IOCTL handler
 * @file: ptr. to system file structure
 * @fh: ptr to hold address of omap34xxcam_fh struct (per-filehandle data)
 * @cmd: ioctl cmd value
 * @arg: ioctl arg value
 *
 */
static long vidioc_default(struct file *file, void *_fh, int cmd, void *arg)
{
	struct isp_csi_memvs_sensor *sensor = video_drvdata(file);
	struct v4l2_int_device *vdev = sensor->v4l2_int_device;
	int rval = -EINVAL;

	if (cmd == VIDIOC_PRIVATE_OMAP34XXCAM_SENSOR_INFO) {
		u32 pixclk;
		struct v4l2_rect active_size, full_size, pixel_size;
		struct omap34xxcam_sensor_info *ret_sensor_info;

		ret_sensor_info = (struct omap34xxcam_sensor_info *)arg;
		mutex_lock(&sensor->mutex);
		rval = ioctl_priv_g_pixclk(vdev, &pixclk);
		mutex_unlock(&sensor->mutex);
		if (rval)
			goto out;
		mutex_lock(&sensor->mutex);
		rval = ioctl_priv_g_activesize(vdev, &active_size);
		mutex_unlock(&sensor->mutex);
		if (rval)
			goto out;
		mutex_lock(&sensor->mutex);
		rval = ioctl_priv_g_fullsize(vdev, &full_size);
		mutex_unlock(&sensor->mutex);
		if (rval)
			goto out;
		mutex_lock(&sensor->mutex);
		rval = ioctl_priv_g_pixelsize(vdev, &pixel_size);
		mutex_unlock(&sensor->mutex);
		if (rval)
			goto out;
		ret_sensor_info->current_xclk = pixclk;
		memcpy(&ret_sensor_info->active_size, &active_size,
			sizeof(struct v4l2_rect));
		memcpy(&ret_sensor_info->full_size, &full_size,
			sizeof(struct v4l2_rect));
		memcpy(&ret_sensor_info->pixel_size, &pixel_size,
			sizeof(struct v4l2_rect));
		rval = 0;
		goto out;
	}

out:
	return rval;
}


static const struct v4l2_ioctl_ops isp_csi_memvs_vout_ioctl_ops = {
	.vidioc_querycap      			= vidioc_querycap,
	.vidioc_enum_fmt_vid_out 		= vidioc_enum_fmt_vid_out,
	.vidioc_g_fmt_vid_out			= vidioc_g_fmt_vid_out,
	.vidioc_try_fmt_vid_out			= vidioc_try_fmt_vid_out,
	.vidioc_s_fmt_vid_out			= vidioc_s_fmt_vid_out,
	.vidioc_reqbufs				= vidioc_reqbufs,
	.vidioc_querybuf			= vidioc_querybuf,
	.vidioc_qbuf				= vidioc_qbuf,
	.vidioc_dqbuf				= vidioc_dqbuf,
	.vidioc_streamon			= vidioc_streamon,
	.vidioc_streamoff			= vidioc_streamoff,
	.vidioc_default				= vidioc_default,
};

static int isp_csi_memvs_vout_buf_setup(struct videobuf_queue *q,
					unsigned int *count,
					unsigned int *size)
{
	struct isp_csi_memvs_fh *ofh = q->priv_data;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;

	if (*count <= 0)
		*count = 1;

	if (*count > VIDEO_MAX_FRAME)
		*count = VIDEO_MAX_FRAME;

	*size = sensor->in_pix.sizeimage;

	return 0;
}

static int isp_csi_memvs_vout_vb_lock_vma(struct videobuf_buffer *vb, int lock)
{
	unsigned long start, end;
	struct vm_area_struct *vma;
	int rval = 0;

	if (vb->memory == V4L2_MEMORY_MMAP)
		return 0;

	if (!current || !current->mm) {
		/**
		 * We are called from interrupt or workqueue context.
		 *
		 * For locking, we return error.
		 * For unlocking, the subsequent release of
		 * buffer should set things right
		 */
		if (lock)
			return -EINVAL;
		else
			return 0;
	}

	end = vb->baddr + vb->bsize;

	down_write(&current->mm->mmap_sem);
	spin_lock(&current->mm->page_table_lock);

	for (start = vb->baddr; ; ) {
		unsigned int newflags;

		vma = find_vma(current->mm, start);
		if (!vma || vma->vm_start > start) {
				rval = -ENOMEM;
			goto out;
		}

		newflags = vma->vm_flags | VM_LOCKED;
		if (!lock)
			newflags &= ~VM_LOCKED;

		vma->vm_flags = newflags;

		if (vma->vm_end >= end)
			break;

		start = vma->vm_end;
	}

out:
	spin_unlock(&current->mm->page_table_lock);
	up_write(&current->mm->mmap_sem);
	return rval;
}

static void isp_csi_memvs_vout_buf_release(struct videobuf_queue *q,
					   struct videobuf_buffer *vb)
{
	struct isp_csi_memvs_fh *ofh = q->priv_data;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;
	struct device *dev = sensor->isp;
	struct isp_csi_memvs_bufs *bufs = &sensor->bufs;

	ispmmu_vunmap(dev, bufs->isp_addr_capture[vb->i]);
	bufs->isp_addr_capture[vb->i] = (dma_addr_t)NULL;
	isp_csi_memvs_vout_vb_lock_vma(vb, 0);
	videobuf_dma_unmap(q, videobuf_to_dma(vb));
	videobuf_dma_free(videobuf_to_dma(vb));
	vb->state = VIDEOBUF_NEEDS_INIT;
}

static int isp_csi_memvs_vout_buf_prepare(struct videobuf_queue *q,
					  struct videobuf_buffer *vb,
					  enum v4l2_field field)
{
	struct isp_csi_memvs_fh *ofh = q->priv_data;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;
	struct device *dev = sensor->isp;
	struct isp_csi_memvs_bufs *bufs = &sensor->bufs;
	struct videobuf_dmabuf *vdma;
	unsigned int isp_addr;
	int err = 0;

	/*
	 * Accessing pix here is okay since it's constant while
	 * streaming is on (and we only get called then).
	 */
	if (vb->baddr) {
		/* This is a userspace buffer. */
		if (sensor->in_pix.sizeimage > vb->bsize)
			/* The buffer isn't big enough. */
			return -EINVAL;
	} else {
		if (vb->state != VIDEOBUF_NEEDS_INIT
		    && sensor->in_pix.sizeimage > vb->bsize)
			/*
			 * We have a kernel bounce buffer that has
			 * already been allocated.
			 */
			isp_csi_memvs_vout_buf_release(q, vb);
	}

	vb->size = sensor->in_pix.bytesperline * sensor->in_pix.height;
	vb->width = sensor->in_pix.width;
	vb->height = sensor->in_pix.height;
	vb->field = field;

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		err = isp_csi_memvs_vout_vb_lock_vma(vb, 1);
		if (err)
			goto buf_init_err;

		err = videobuf_iolock(q, vb, NULL);
		if (err)
			goto buf_init_err;

		vdma = videobuf_to_dma(vb);

		isp_addr = ispmmu_vmap(dev, vdma->sglist, vdma->sglen);

		if (IS_ERR_VALUE(isp_addr))
			err = -EIO;
		else
			bufs->isp_addr_capture[vb->i] = isp_addr;

		return err;
	}

buf_init_err:
	if (!err)
		vb->state = VIDEOBUF_PREPARED;
	else
		isp_csi_memvs_vout_buf_release(q, vb);

	return err;
}

static void isp_csi_memvs_vout_buf_queue(struct videobuf_queue *q,
					 struct videobuf_buffer *vb)
{
	struct isp_csi_memvs_fh *ofh = q->priv_data;
	struct isp_csi_memvs_sensor *sensor = ofh->sensor;
	struct isp_csi_memvs_bufs *bufs = &sensor->bufs;
	struct isp_csi_memvs_buf *buf;

	BUG_ON(ISP_BUFS_IS_FULL(bufs));

	buf = ISP_BUF_QUEUE(bufs);

	buf->isp_addr = bufs->isp_addr_capture[vb->i];
	buf->vb_state = VIDEOBUF_DONE;
	buf->vb->state = VIDEOBUF_ACTIVE;

	ISP_BUF_MARK_QUEUED(bufs);
}

static struct videobuf_queue_ops isp_csi_memvs_vout_vbq_ops = {
	.buf_setup = isp_csi_memvs_vout_buf_setup,
	.buf_prepare = isp_csi_memvs_vout_buf_prepare,
	.buf_release = isp_csi_memvs_vout_buf_release,
	.buf_queue = isp_csi_memvs_vout_buf_queue,
};

static int isp_csi_memvs_vout_open(struct file *file)
{
	struct isp_csi_memvs_sensor *sensor = video_drvdata(file);
	struct video_device *vfd = sensor->out_vfd;
	struct isp_csi_memvs_fh *ofh;
	struct device *isp;
	int ret = 0;

	mutex_lock(&sensor->mutex);
	if (sensor->opened) {
		dev_err(&vfd->dev, "Device already opened!\n");
		ret = -EBUSY;
		goto out_err;
	}

	ofh = kzalloc(sizeof(*ofh), GFP_KERNEL);
	if (ofh == NULL) {
		ret = -ENOMEM;
		goto out_err;
	}

	ofh->sensor = sensor;

	isp = isp_get();
	if (!isp) {
		dev_err(&vfd->dev, "can't get isp\n");
		ret = -EBUSY;
		goto out_isp_get;
	}

	sensor->isp = isp;

	file->private_data = ofh;

	spin_lock_init(&ofh->vbq_lock);

	videobuf_queue_sg_init(&ofh->vbq, &isp_csi_memvs_vout_vbq_ops, NULL,
			       &ofh->vbq_lock, V4L2_BUF_TYPE_VIDEO_OUTPUT,
			       V4L2_FIELD_NONE, sizeof(struct videobuf_buffer),
			       ofh);

	sensor->opened = true;

	mutex_unlock(&sensor->mutex);
	return 0;

out_isp_get:
	kfree(ofh);

out_err:
	mutex_unlock(&sensor->mutex);
	return ret;
}

static int isp_csi_memvs_vout_release(struct file *file)
{
	struct isp_csi_memvs_sensor *sensor = video_drvdata(file);
	struct video_device *vfd = sensor->out_vfd;
	struct isp_csi_memvs_fh *ofh = file->private_data;

	mutex_lock(&sensor->mutex);
	if (!sensor->opened) {
		mutex_unlock(&sensor->mutex);
		dev_err(&vfd->dev, "Device wasn't opened!\n");
		return -EINVAL;
	}
	mutex_unlock(&sensor->mutex);

	kfree(ofh);

	sensor->opened = false;

	isp_put();
	sensor->isp = NULL;

	return 0;
}

static const struct v4l2_file_operations isp_csi_memvs_vout_fops = {
	.owner 		= THIS_MODULE,
	.unlocked_ioctl	= video_ioctl2,
	.open 		= isp_csi_memvs_vout_open,
	.release 	= isp_csi_memvs_vout_release,
};

/**
 * isp_csi_memvs_init - sensor driver module_init handler
 *
 * Registers driver as a v4l2 int slave driver.
 */
static int __init isp_csi_memvs_init(void)
{
	struct isp_csi_memvs_sensor *sensor;
	struct video_device *vfd;
	int err = 0;


	sensor = kmalloc(sizeof(struct isp_csi_memvs_sensor), GFP_KERNEL);
	if (!sensor) {
		printk(KERN_ERR ISP_CSI_MEMVS_DRIVER_NAME
				": could not allocate memory\n");
		return -ENOMEM;
	}

	mutex_init(&sensor->mutex);

	/* Allocate resources for output device, for receiving frames from mem
	 * to the CCP2 read port
	 */
	vfd = video_device_alloc();
	if (!vfd) {
		printk(KERN_ERR ISP_CSI_MEMVS_DRIVER_NAME " could not allocate"
		       " video device struct\n");
		err = -ENOMEM;
		goto error1;
	}
	vfd->release = video_device_release;
	vfd->ioctl_ops = &isp_csi_memvs_vout_ioctl_ops;

	strlcpy(vfd->name, ISP_CSI_MEMVS_DRIVER_NAME, sizeof(vfd->name));
	vfd->vfl_type = VFL_TYPE_GRABBER;
	vfd->fops = &isp_csi_memvs_vout_fops;
	vfd->minor = -1;

	/* FIXME: Hardcode to register as /dev/video10, must be
	 *        board-dependent
	 */
	if (video_register_device(vfd, VFL_TYPE_GRABBER, 10) < 0) {
		printk(KERN_ERR ISP_CSI_MEMVS_DRIVER_NAME
		       ": could not register Video for Linux device\n");
		vfd->minor = -1;
		err = -ENODEV;
		goto error2;
	}

	/* Register slave with omap34xxcam master */
	sensor->v4l2_int_device = &isp_csi_memvs_int_device;
	sensor->v4l2_int_device->priv = sensor;

	/* Init pixel in/out information */
	sensor->out_pix.width = VSENSOR_OUT_WIDTH;
	sensor->out_pix.height = VSENSOR_OUT_HEIGHT;
	sensor->out_pix.pixelformat = isp_csi_memvs_formats[0].pixelformat;
	sensor->out_pix.bytesperline = ALIGN(sensor->out_pix.width *
					     ISP_BYTES_PER_PIXEL,
					     PHY_ADDRESS_ALIGN);
	sensor->out_pix.field = V4L2_FIELD_NONE;
	sensor->out_pix.sizeimage = PAGE_ALIGN(sensor->out_pix.bytesperline *
					       sensor->out_pix.height);
	sensor->out_pix.colorspace = V4L2_COLORSPACE_SRGB;

	sensor->in_pix = sensor->out_pix;

	sensor->sparams.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	sensor->tpframe.numerator = 1;
	sensor->tpframe.numerator = 10;

	err = v4l2_int_device_register(sensor->v4l2_int_device);

	if (err)
		return err;

	sensor->out_vfd = vfd;
	sensor->opened = false;

	video_set_drvdata(vfd, sensor);

	isp_csi_memvs = sensor;
	return 0;

error2:
	video_device_release(vfd);

error1:
	kfree(sensor);

	return err;
}
late_initcall(isp_csi_memvs_init);

/**
 * isp_csi_memvs_cleanup - sensor driver module_exit handler
 *
 * Unregisters/deletes v4l2 int slave driver.
 * Complement of isp_csi_memvs_init.
 */
static void __exit isp_csi_memvs_cleanup(void)
{
	v4l2_int_device_unregister(isp_csi_memvs->v4l2_int_device);

	kfree(isp_csi_memvs);
}
module_exit(isp_csi_memvs_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("isp_csi_memvs driver");
