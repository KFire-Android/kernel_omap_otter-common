/*
 * drivers/media/video/isp/omap_prev2resz.c
 *
 * Wrapper for Preview module in TI's OMAP3430 ISP
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Contributors:
 * 	Atanas Filipov <afilipov@mm-sol.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <plat/omap-pm.h>
#include "isp.h"
#include <linux/omap_prev2resz.h>

#define OMAP_PREV2RESZ_NAME	"omap-prev2resz"

static struct device 		*p2r_device;
static struct class		*p2r_class;
static int			 p2r_major = -1;

static struct isp_interface_config p2r_interface = {
	.ccdc_par_ser		= ISP_NONE,
	.dataline_shift		= 0,
	.hsvs_syncdetect	= ISPCTRL_SYNC_DETECT_VSRISE,
	.strobe			= 0,
	.prestrobe		= 0,
	.shutter		= 0,
	.wait_hs_vs		= 0,
};

/*
 * Structure keeps the global information for device
 */
static struct {
	unsigned char		opened;		/* device state */
	struct prev2resz_status	status;		/* engines status */
	struct completion	resz_complete;	/* completion for interrupt */

	/*
	 * Videobuf queue operations
	 */
	struct videobuf_queue_ops	vbq_ops;
} p2r_ctx;

/*
 * File handle data structure
 */
struct prev2resz_fhdl {
	struct isp_node pipe;	/* User pipe configuration */
	struct isp_node prev;	/* Preview pipe node */
	struct isp_node resz;	/* Resizer pipe node */

	int src_buff_index;
	u32 src_buff_addr;	/* Input address */
	int dst_buff_index;
	u32 dst_buff_addr;	/* Output address */

	struct device		*isp;
	struct isp_device	*isp_dev;
	struct isp_prev_device	*isp_prev;
	struct isp_res_device	*isp_resz;

	struct videobuf_queue	src_vbq;
	spinlock_t		src_vbq_lock; /* spinlock for input queues. */
	struct videobuf_queue	dst_vbq;
	spinlock_t		dst_vbq_lock; /* spinlock for output queues. */
};

/*
 * prev2resz_isp_callback - Interrupt Service Routine for Resizer
 *
 *	@status: ISP IRQ0STATUS register value
 *	@arg1: Currently not used
 *	@arg2: Currently not used
 *
 */
static void prev2resz_resz_callback(unsigned long status,
				    isp_vbq_callback_ptr arg1, void *arg2)
{
	if ((status & RESZ_DONE) != RESZ_DONE)
		return;
	complete(&p2r_ctx.resz_complete);
}

static int prev2resz_ioc_set_config(struct prev2resz_fhdl *fh)
{
	struct isp_node *pipe = &fh->pipe;
	int rval, rest;

	if (pipe->in.image.pixelformat != V4L2_PIX_FMT_SBGGR16 &&
	    pipe->in.image.pixelformat != V4L2_PIX_FMT_SGRBG10DPCM8) {
		pipe->in.image.bytesperline = (pipe->in.image.width * 2) & ~63;
		rest = (pipe->in.image.width * 2) % 64;
	} else {
		pipe->in.image.bytesperline =  pipe->in.image.width & ~63;
		rest = pipe->in.image.width % 64;
	}
	/* ISP Previewer memory input must be aligned of 64 bytes */
	pipe->in.crop.left		+= rest / 2;
	pipe->in.crop.width		-= rest / 2;

	/* Setup parameters for Previewer input */
	fh->prev.in			= pipe->in;
	fh->prev.in.path		= PRV_RAW_MEM;
	/* Setup parameters for Previewer output */
	fh->prev.out			= pipe->in;
	fh->prev.out.path		= PREVIEW_RSZ;
	fh->prev.out.image.pixelformat	= pipe->out.image.pixelformat;
	/* Previewer output is 16 bits data and must be aligned of 32 bytes */
	fh->prev.out.image.bytesperline =
				(fh->prev.out.image.width * 2) & ~31;
	rest = (fh->prev.out.image.width * 2) % 32;
	fh->prev.out.crop.left		+= rest / 2;
	fh->prev.out.crop.width		-= rest / 2;

	/* Try Previwer input and output size */
	rval = isppreview_try_pipeline(fh->isp_prev, &fh->prev);
	if (rval != 0)
		return rval;
	/* Update return parameters */
	pipe->in.crop = fh->prev.out.crop;

	/* Setup parameters for Resizer input */
	fh->resz.in			= fh->prev.out;
	fh->resz.in.path			= RSZ_OTFLY_YUV;
	fh->resz.in.image.width		= fh->prev.out.crop.width;
	fh->resz.in.image.height	= fh->prev.out.crop.height;
	fh->resz.in.image.pixelformat	= pipe->out.image.pixelformat;
	/* Setup parameters for Resizer output */
	fh->resz.out			= pipe->out;
	fh->resz.out.path		= RSZ_MEM_YUV;
	fh->resz.out.image.pixelformat	= pipe->out.image.pixelformat;

	/* Try Resizer input and output size */
	rval = ispresizer_try_pipeline(fh->isp_resz, &fh->resz);
	if (rval != 0)
		return rval;
	/* Update return parameters */
	pipe->out = fh->resz.out;

	return 0;
}

static int prev2resz_ioc_run_engine(struct prev2resz_fhdl *fh)
{
	struct isp_freq_devider *fdiv;
	int rval;

	rval = isppreview_s_pipeline(fh->isp_prev, &fh->prev);
	if (rval != 0)
		return rval;

	rval = isppreview_set_inaddr(fh->isp_prev, fh->src_buff_addr);
	if (rval != 0)
		return rval;

	rval = isppreview_config_inlineoffset(fh->isp_prev,
				fh->prev.in.image.bytesperline);
	if (rval != 0)
		return rval;

	rval = isppreview_set_outaddr(fh->isp_prev, fh->dst_buff_addr);
	if (rval != 0)
		return rval;

	rval = isppreview_config_features(fh->isp_prev, &fh->isp_prev->params);
	if (rval != 0)
		return rval;

	isppreview_set_size(fh->isp_prev, fh->prev.in.image.width,
			    fh->prev.in.image.height);

	/* Set resizer input and output size */
	rval = ispresizer_s_pipeline(fh->isp_resz, &fh->resz);
	if (rval != 0)
		return rval;

	rval = ispresizer_set_outaddr(fh->isp_resz, fh->dst_buff_addr);
	if (rval != 0)
		return rval;

	isp_configure_interface(fh->isp, &p2r_interface);
	/*
	 * Through-put requirement:
	 * Set max OCP freq for 3630 is 200 MHz through-put
	 * is in KByte/s so 200000 KHz * 4 = 800000 KByte/s
	 */
	omap_pm_set_min_bus_tput(fh->isp, OCP_INITIATOR_AGENT, 800000);

	/* Reduces memory bandwidth */
	fdiv = isp_get_upscale_ratio(fh->pipe.in.image.width,
				     fh->pipe.in.image.height,
				     fh->pipe.out.image.width,
				     fh->pipe.out.image.height);
	dev_dbg(p2r_device, "Set the REQ_EXP register = %d.\n",
		fdiv->prev_exp);
	isp_reg_and_or(fh->isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
		       ~(ISPSBL_SDR_REQ_PRV_EXP_MASK |
		         ISPSBL_SDR_REQ_RSZ_EXP_MASK),
		         fdiv->prev_exp << ISPSBL_SDR_REQ_PRV_EXP_SHIFT);
	isp_start(fh->isp);

	init_completion(&p2r_ctx.resz_complete);
	rval = isp_set_callback(fh->isp, CBK_RESZ_DONE, prev2resz_resz_callback,
			       (void *) NULL, (void *) NULL);
	if (rval) {
		dev_err(p2r_device, "%s: setting resizer callback failed\n",
			__func__);
		return rval;
	}

	ispresizer_enable(fh->isp_resz, 1);
	isppreview_enable(fh->isp_prev, 1);
	rval = wait_for_completion_interruptible_timeout(
			&p2r_ctx.resz_complete, msecs_to_jiffies(1000));
	if (rval == 0)
		dev_crit(p2r_device, "Resizer interrupt timeout exit\n");

	isp_unset_callback(fh->isp, CBK_RESZ_DONE);

	/* Reset Through-put requirement */
	omap_pm_set_min_bus_tput(fh->isp, OCP_INITIATOR_AGENT, 0);

	/* This will flushes the queue */
	if (&fh->src_vbq)
		videobuf_queue_cancel(&fh->src_vbq);
	if (&fh->dst_vbq)
		videobuf_queue_cancel(&fh->dst_vbq);

	return 0;
}

/**
 * prev2resz_vbq_setup - Sets up the videobuffer size and validates count.
 */
static int prev2resz_vbq_setup(struct videobuf_queue *q, unsigned int *cnt,
			       unsigned int *size)
{
	struct prev2resz_fhdl *fhdl = q->priv_data;

	if (*cnt <= 0)
		*cnt = 1;
	if (*cnt > VIDEO_MAX_FRAME)
		*cnt = VIDEO_MAX_FRAME;

	if (!fhdl->pipe.in.image.width || !fhdl->pipe.in.image.height ||
	    !fhdl->pipe.out.image.width || !fhdl->pipe.out.image.height) {
		dev_err(p2r_device, "%s: Can't setup buffers "
			"size\n", __func__);
		return -EINVAL;
	}

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
		*size = fhdl->pipe.out.image.bytesperline *
			fhdl->pipe.out.image.height;
	else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
		*size = fhdl->pipe.in.image.bytesperline *
			fhdl->pipe.in.image.height;
	else
		return -EINVAL;

	return 0;
}
/**
 * prev2resz_vbq_release - Videobuffer queue release
 */
static void prev2resz_vbq_release(struct videobuf_queue *q,
				  struct videobuf_buffer *vb)
{
	struct prev2resz_fhdl *fhdl = q->priv_data;

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		ispmmu_vunmap(fhdl->isp, fhdl->dst_buff_addr);
		fhdl->dst_buff_addr = 0;
		spin_lock(&fhdl->dst_vbq_lock);
		vb->state = VIDEOBUF_NEEDS_INIT;
		spin_unlock(&fhdl->dst_vbq_lock);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		ispmmu_vunmap(fhdl->isp, fhdl->src_buff_addr);
		fhdl->src_buff_addr = 0;
		spin_lock(&fhdl->src_vbq_lock);
		vb->state = VIDEOBUF_NEEDS_INIT;
		spin_unlock(&fhdl->src_vbq_lock);
	}

	if (vb->memory != V4L2_MEMORY_MMAP) {
		videobuf_dma_unmap(q, videobuf_to_dma(vb));
		videobuf_dma_free(videobuf_to_dma(vb));
	}
}

/**
 * prev2resz_vbq_prepare - Videobuffer is prepared and mmapped.
 */
static int prev2resz_vbq_prepare(struct videobuf_queue *q,
				 struct videobuf_buffer *vb,
				 enum v4l2_field field)
{
	struct prev2resz_fhdl *fhdl = q->priv_data;
	struct videobuf_dmabuf *dma  = videobuf_to_dma(vb);
	dma_addr_t isp_addr;
	int err = 0;

	if (!vb->baddr) {
		dev_err(p2r_device, "%s: No user buffer allocated\n",
			__func__);
		return -EINVAL;
	}

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		spin_lock(&fhdl->dst_vbq_lock);
		vb->size = fhdl->pipe.out.image.bytesperline *
			   fhdl->pipe.out.image.height;
		vb->width = fhdl->pipe.out.image.width;
		vb->height = fhdl->pipe.out.image.height;
		vb->bsize = vb->size;
		vb->field = field;
		spin_unlock(&fhdl->dst_vbq_lock);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		spin_lock(&fhdl->src_vbq_lock);
		vb->size = fhdl->pipe.in.image.bytesperline *
			   fhdl->pipe.in.image.height;
		vb->width = fhdl->pipe.in.image.width;
		vb->height = fhdl->pipe.in.image.height;
		vb->bsize = vb->size;
		vb->field = field;
		spin_unlock(&fhdl->src_vbq_lock);
	} else {
		return -EINVAL;
	}

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		dev_dbg(p2r_device, "baddr = %08x\n", (int)vb->baddr);
		err = videobuf_iolock(q, vb, NULL);
		if (!err) {
			isp_addr = ispmmu_vmap(fhdl->isp, dma->sglist,
					       dma->sglen);
			if (!isp_addr) {
				err = -EIO;
			} else {
				if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
					fhdl->dst_buff_addr = isp_addr;
				else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
					fhdl->src_buff_addr = isp_addr;
				else
					return -EINVAL;
			}
		}
	}

	if (!err) {
		if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			spin_lock(&fhdl->dst_vbq_lock);
			vb->state = VIDEOBUF_PREPARED;
			spin_unlock(&fhdl->dst_vbq_lock);
		} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			spin_lock(&fhdl->src_vbq_lock);
			vb->state = VIDEOBUF_PREPARED;
			spin_unlock(&fhdl->src_vbq_lock);
		}
	} else {
		prev2resz_vbq_release(q, vb);
	}

	return err;
}

/**
 * prev2resz_vbq_queue - Videobuffer is in the queue.
 */
static void prev2resz_vbq_queue(struct videobuf_queue *q,
				struct videobuf_buffer *vb)
{
}

/**
 * prev2resz_open - Initializes and opens the device
 */
static int prev2resz_open(struct inode *inode, struct file *fp)
{
	struct prev2resz_fhdl *fh;
	int ret = 0;

	fh = kzalloc(sizeof(struct prev2resz_fhdl), GFP_KERNEL);
	if (NULL == fh)
		return -ENOMEM;

	fh->isp = isp_get();
	if (fh->isp == NULL) {
		dev_err(p2r_device, "Can't get ISP device\n");
		ret = -EBUSY;
		goto err_isp;
	}
	fh->isp_dev = dev_get_drvdata(fh->isp);
	if (fh->isp_dev == NULL) {
		dev_err(p2r_device, "Can't get ISP driver data\n");
		ret = -EBUSY;
		goto err_isp;
	}
	fh->isp_prev = &fh->isp_dev->isp_prev;
	fh->isp_resz = &fh->isp_dev->isp_res;

	fh->src_vbq.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fh->dst_vbq.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	p2r_ctx.status.prv_busy = PREV2RESZ_FREE;
	p2r_ctx.status.rsz_busy = PREV2RESZ_FREE;

	init_completion(&p2r_ctx.resz_complete);

	p2r_ctx.opened++;

	fp->private_data = fh;

	videobuf_queue_sg_init(&fh->src_vbq, &p2r_ctx.vbq_ops, NULL,
			       &fh->src_vbq_lock, fh->src_vbq.type,
			       V4L2_FIELD_NONE, sizeof(struct videobuf_buffer),
			       fh);
	spin_lock_init(&fh->src_vbq_lock);

	videobuf_queue_sg_init(&fh->dst_vbq, &p2r_ctx.vbq_ops, NULL,
			       &fh->dst_vbq_lock, fh->dst_vbq.type,
			       V4L2_FIELD_NONE, sizeof(struct videobuf_buffer),
			       fh);
	spin_lock_init(&fh->dst_vbq_lock);

	return 0;

err_isp:
	isp_put();
	kfree(fh);

	return ret;
}

/**
 * prev2resz_ioctl - I/O control function for device
 */
static int prev2resz_ioctl(struct inode *inode, struct file *file,
			   unsigned int cmd, unsigned long arg)
{
	struct prev2resz_fhdl *fh = file->private_data;
	long rval = 0;

	if ((_IOC_TYPE(cmd) != PREV2RESZ_IOC_BASE) && (_IOC_TYPE(cmd) != 'M') &&
	    (cmd < BASE_VIDIOC_PRIVATE)) {
		dev_err(p2r_device, "Bad command value.\n");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		rval = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		rval = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));

	if (rval) {
		dev_err(p2r_device, "Access denied\n");
		return -EFAULT;
	}

	switch (cmd) {
	case PREV2RESZ_REQBUF:
	{
		struct v4l2_requestbuffers v4l2_req;
		if (copy_from_user(&v4l2_req, (void *)arg,
				   sizeof(struct v4l2_requestbuffers)))
			return -EIO;
		if (v4l2_req.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			if (videobuf_reqbufs(&fh->src_vbq, &v4l2_req) < 0)
				return -EINVAL;
		} else if (v4l2_req.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			if (videobuf_reqbufs(&fh->dst_vbq, &v4l2_req) < 0)
				return -EINVAL;
		}
		if (copy_to_user((void *)arg, &v4l2_req,
				 sizeof(struct v4l2_requestbuffers)))
			return -EIO;
		break;
	}
	case PREV2RESZ_QUERYBUF:
	{
		struct v4l2_buffer v4l2_buf;
		if (copy_from_user(&v4l2_buf, (void *)arg,
				   sizeof(struct v4l2_buffer)))
			return -EIO;
		if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			if (videobuf_querybuf(&fh->src_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		} else if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			if (videobuf_querybuf(&fh->dst_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		}
		if (copy_to_user((void *)arg, &v4l2_buf,
				 sizeof(struct v4l2_buffer)))
			return -EIO;
		break;
	}
	case PREV2RESZ_QUEUEBUF:
	{
		struct v4l2_buffer v4l2_buf;
		if (copy_from_user(&v4l2_buf, (void *)arg,
				   sizeof(struct v4l2_buffer)))
			return -EIO;
		if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			if (videobuf_qbuf(&fh->src_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		} else if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			if (videobuf_qbuf(&fh->dst_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		}
		if (copy_to_user((void *)arg, &v4l2_buf,
				 sizeof(struct v4l2_buffer)))
			return -EIO;
		break;
	}
	case PREV2RESZ_SET_CONFIG:
		if (copy_from_user(&fh->pipe, (void *)arg,
				   sizeof(fh->pipe)))
			return -EIO;
		if (prev2resz_ioc_set_config(fh) < 0)
			return -EINVAL;
		if (copy_to_user((void *)arg, &fh->pipe,
				 sizeof(fh->pipe)))
			return -EIO;
		break;

	case PREV2RESZ_GET_CONFIG:
		if (copy_to_user((void *)arg, &fh->pipe,
				 sizeof(fh->pipe)))
			return -EIO;
		break;

	case PREV2RESZ_RUN_ENGINE:
		if (file->f_flags & O_NONBLOCK) {
			if (isppreview_busy(fh->isp_prev))
				return -EBUSY;
			if (ispresizer_busy(fh->isp_resz))
				return -EBUSY;
		}
		if (prev2resz_ioc_run_engine(fh) < 0)
			return -EINVAL;
		break;

	case PREV2RESZ_GET_STATUS:
	{
		struct prev2resz_status *status =
			(struct prev2resz_status *)arg;
		status->prv_busy = isppreview_busy(fh->isp_prev);
		status->rsz_busy = ispresizer_busy(fh->isp_resz);
		break;
	}
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability v4l2_cap;
		if (copy_from_user(&v4l2_cap, (void *)arg,
				   sizeof(struct v4l2_capability)))
			return -EIO;

		strcpy(v4l2_cap.driver, "omap3wrapper");
		strcpy(v4l2_cap.card, "omap3wrapper/prev-resz");
		v4l2_cap.version	= 1.0;;
		v4l2_cap.capabilities	= V4L2_CAP_VIDEO_CAPTURE |
					  V4L2_CAP_READWRITE;

		if (copy_to_user((void *)arg, &v4l2_cap,
				 sizeof(struct v4l2_capability)))
			return -EIO;
		break;
	}
	case VIDIOC_PRIVATE_ISP_PRV_CFG:
		if (isppreview_config(fh->isp_prev, (void *)arg))
			return -EIO;
		break;

	default:
		dev_err(p2r_device, "IOC: Invalid Command Value!\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * prev2resz_release - Releases device resources
 */
static int prev2resz_release(struct inode *inode, struct file *fp)
{
	struct prev2resz_fhdl *fh = fp->private_data;
	u32 timeout = 0;

	while ((p2r_ctx.status.prv_busy != PREV2RESZ_FREE ||
		p2r_ctx.status.rsz_busy != PREV2RESZ_FREE) &&
		(timeout < 20)) {
		timeout++;
		schedule();
	}
	p2r_ctx.status.prv_busy = PREV2RESZ_FREE;
	p2r_ctx.status.rsz_busy = PREV2RESZ_FREE;
	p2r_ctx.opened--;


	/* This will Free memory allocated to the buffers,
	 * and flushes the queue
	 */
	if (&fh->src_vbq)
		videobuf_queue_cancel(&fh->src_vbq);
	if (&fh->dst_vbq)
		videobuf_queue_cancel(&fh->dst_vbq);

	fp->private_data = NULL;

	isp_stop(fh->isp);

	kfree(fh);

	isp_put();

	return 0;
}

static const struct file_operations dev_fops = {
	.owner		= THIS_MODULE,
	.open		= prev2resz_open,
	.ioctl		= prev2resz_ioctl,
	.release	= prev2resz_release
};

/**
 * prev2resz_probe - Checks for device presence
 */
static int prev2resz_driver_probe(struct platform_device *device)
{
	return 0;
}

/**
 * prev2resz_driver_remove - Handles the removal of the driver
 */
static int prev2resz_driver_remove(struct platform_device *device)
{
	return 0;
}

static struct platform_driver omap_prev2resz_driver = {
	.probe	= prev2resz_driver_probe,
	.remove	= prev2resz_driver_remove,
	.driver	= {
		.bus	= &platform_bus_type,
		.name	= OMAP_PREV2RESZ_NAME,
	},
};

/**
 * prev2resz_device_release - Acts when Reference count is zero
 */
static void prev2resz_device_release(struct device *device)
{
}

static struct platform_device omap_prev2resz_device = {
	.name	= OMAP_PREV2RESZ_NAME,
	.id	= 2,
	.dev	= {
		.release = prev2resz_device_release,
	}
};

/**
 * omap_prev2resz_init - Initialization of device
 */
static int __init omap_prev2resz_init(void)
{
	int ret;

	p2r_major = register_chrdev(0, OMAP_PREV2RESZ_NAME, &dev_fops);

	/* register driver as a platform driver */
	ret = platform_driver_register(&omap_prev2resz_driver);
	if (ret) {
		dev_crit(p2r_device, "failed to register platform driver!\n");
		goto drv_fail;
	}

	/* Register the drive as a platform device */
	ret = platform_device_register(&omap_prev2resz_device);
	if (ret) {
		dev_crit(p2r_device, "failed to register platform device!\n");
		goto dev_fail;
	}

	p2r_class = class_create(THIS_MODULE, OMAP_PREV2RESZ_NAME);
	if (!p2r_class) {
		dev_crit(p2r_device, "Failed to create class!\n");
		goto cls_fail;
	}

	/* make entry in the devfs */
	p2r_device = device_create(p2r_class, p2r_device,
				    MKDEV(p2r_major, 0), NULL,
				    OMAP_PREV2RESZ_NAME);

	p2r_ctx.vbq_ops.buf_setup	= prev2resz_vbq_setup;
	p2r_ctx.vbq_ops.buf_prepare	= prev2resz_vbq_prepare;
	p2r_ctx.vbq_ops.buf_release	= prev2resz_vbq_release;
	p2r_ctx.vbq_ops.buf_queue	= prev2resz_vbq_queue;

	p2r_ctx.opened = 0;

	return 0;

cls_fail:
	platform_device_unregister(&omap_prev2resz_device);
dev_fail:
	platform_driver_unregister(&omap_prev2resz_driver);
drv_fail:
	unregister_chrdev(p2r_major, OMAP_PREV2RESZ_NAME);

	return ret;
}

/**
 * omap_prev2resz_exit - Close the device
 */
static void __exit omap_prev2resz_exit(void)
{
	device_destroy(p2r_class, MKDEV(p2r_major, 0));
	class_destroy(p2r_class);
	platform_device_unregister(&omap_prev2resz_device);
	platform_driver_unregister(&omap_prev2resz_driver);
	unregister_chrdev(p2r_major, OMAP_PREV2RESZ_NAME);
	p2r_major = -1;
}

module_init(omap_prev2resz_init);
module_exit(omap_prev2resz_exit);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("OMAP ISP Previewer and Resizer pipe");
MODULE_LICENSE("GPL");
