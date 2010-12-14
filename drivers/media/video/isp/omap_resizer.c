/*
 * drivers/media/video/isp/omap_resizer.c
 *
 * Driver for Resizer module in TI's OMAP3430 ISP
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Contributors:
 *      Sergio Aguirre <saaguirre@ti.com>
 *      Troy Laramy <t-laramy@ti.com>
 *      Atanas Filipov <afilipov@mm-sol.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <media/v4l2-dev.h>
#include <asm/cacheflush.h>
#include <plat/omap-pm.h>

#include "isp.h"
#include "ispreg.h"
#include "ispresizer.h"

#include <linux/omap_resizer.h>

#define OMAP_RESIZER_NAME		"omap-resizer"

static DECLARE_MUTEX(rsz_hardware_mutex);

enum rsz_config {
	STATE_NOTDEFINED,	/* Resizer driver not configured */
	STATE_CONFIGURED	/* Resizer driver configured. */
};

enum rsz_status {
	CHANNEL_FREE,
	CHANNEL_BUSY
};

/*
 * Global structure keeps track of global information
 */
struct device_params {
	unsigned char opened;			/* state of the device */
	struct completion isr_complete;		/* Completion for interrupt */
	struct videobuf_queue_ops vbq_ops;	/* videobuf queue operations */
};

static struct device 		*rsz_device;
static struct class		*rsz_class;
static struct device_params	*rsz_params;
static int			 rsz_major = -1;

static struct isp_interface_config rsz_interface = {
	.ccdc_par_ser		= ISP_NONE,
	.dataline_shift		= 0,
	.hsvs_syncdetect	= ISPCTRL_SYNC_DETECT_VSRISE,
	.strobe			= 0,
	.prestrobe		= 0,
	.shutter		= 0,
	.wait_hs_vs		= 0,
};

/*
 * Filehandle data structure
 */
struct rsz_fhdl {
	struct isp_node pipe;
	int src_buff_index;
	u32 rsz_sdr_inadd;	/* Input address */
	int dst_buff_index;
	u32 rsz_sdr_outadd;	/* Output address */
	int rsz_buff_count;	/* used buffers for resizing */

	struct device_params	*device;
	struct device		*isp;
	struct isp_device	*isp_dev;

	struct videobuf_queue	src_vbq;
	spinlock_t		src_vbq_lock; /* spinlock for input queues. */
	struct videobuf_queue	dst_vbq;
	spinlock_t		dst_vbq_lock; /* spinlock for output queues. */

	enum rsz_status status;	/* Channel status: busy or not */
	enum rsz_config config;	/* Configuration state */
};

/*
 * rsz_isp_callback - Interrupt Service Routine for Resizer
 *
 *	@status: ISP IRQ0STATUS register value
 *	@arg1: Currently not used
 *	@arg2: Currently not used
 *
 */
static void rsz_isp_callback(unsigned long status, isp_vbq_callback_ptr arg1,
			     void *arg2)
{
	if ((status & RESZ_DONE) != RESZ_DONE)
		return;
	complete(&rsz_params->isr_complete);
}

/*
 * rsz_ioc_run_engine - Enables Resizer
 *
 *	@fhdl: Structure containing ISP resizer global information
 *
 *	Submits a resizing task specified by the structure.
 *	The call can either be blocked until the task is completed
 *	or returned immediately based on the value of the blocking argument.
 *	If it is blocking, the status of the task can be checked.
 *
 *	Returns 0 if successful, or -EINVAL otherwise.
 */
int rsz_ioc_run_engine(struct rsz_fhdl *fhdl)
{
	int ret;
	struct videobuf_queue *sq = &fhdl->src_vbq;
	struct videobuf_queue *dq = &fhdl->dst_vbq;
	struct isp_freq_devider *fdiv;

	if (fhdl->config != STATE_CONFIGURED) {
		dev_err(rsz_device, "State not configured \n");
		return -EINVAL;
	}

	fhdl->status = CHANNEL_BUSY;

	down(&rsz_hardware_mutex);

	if (ispresizer_s_pipeline(&fhdl->isp_dev->isp_res, &fhdl->pipe) != 0)
		return -EINVAL;
	if (ispresizer_set_inaddr(&fhdl->isp_dev->isp_res,
				  fhdl->rsz_sdr_inadd, &fhdl->pipe) != 0)
		return -EINVAL;
	if (ispresizer_set_outaddr(&fhdl->isp_dev->isp_res,
				   fhdl->rsz_sdr_outadd) != 0)
		return -EINVAL;

	/* Reduces memory bandwidth */
	fdiv = isp_get_upscale_ratio(fhdl->pipe.in.image.width,
				     fhdl->pipe.in.image.height,
				     fhdl->pipe.out.image.width,
				     fhdl->pipe.out.image.height);
	dev_dbg(rsz_device, "Set the REQ_EXP register = %d.\n",
		fdiv->resz_exp);
	isp_reg_and_or(fhdl->isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
		       ~ISPSBL_SDR_REQ_RSZ_EXP_MASK,
		       fdiv->resz_exp << ISPSBL_SDR_REQ_RSZ_EXP_SHIFT);

	init_completion(&rsz_params->isr_complete);

	if (isp_configure_interface(fhdl->isp, &rsz_interface) != 0) {
		dev_err(rsz_device, "Can not configure interface\n");
		return -EINVAL;
	}

	if (isp_set_callback(fhdl->isp, CBK_RESZ_DONE, rsz_isp_callback,
			     (void *) NULL, (void *)NULL)) {
		dev_err(rsz_device, "Can not set callback for resizer\n");
		return -EINVAL;
	}
	/*
	 * Through-put requirement:
	 * Set max OCP freq for 3630 is 200 MHz through-put
	 * is in KByte/s so 200000 KHz * 4 = 800000 KByte/s
	 */
	omap_pm_set_min_bus_tput(fhdl->isp, OCP_INITIATOR_AGENT, 800000);
	isp_start(fhdl->isp);
	ispresizer_enable(&fhdl->isp_dev->isp_res, 1);

	ret = wait_for_completion_interruptible_timeout(
			&rsz_params->isr_complete, msecs_to_jiffies(1000));
	if (ret == 0)
		dev_crit(rsz_device, "\nTimeout exit from"
				     " wait_for_completion\n");

	up(&rsz_hardware_mutex);

	fhdl->status = CHANNEL_FREE;
	fhdl->config = STATE_NOTDEFINED;

	sq->bufs[fhdl->src_buff_index]->state = VIDEOBUF_DONE;
	dq->bufs[fhdl->dst_buff_index]->state = VIDEOBUF_DONE;

	if (fhdl->rsz_sdr_inadd) {
		ispmmu_vunmap(fhdl->isp, fhdl->rsz_sdr_inadd);
		fhdl->rsz_sdr_inadd = 0;
	}

	if (fhdl->rsz_sdr_outadd) {
		ispmmu_vunmap(fhdl->isp, fhdl->rsz_sdr_outadd);
		fhdl->rsz_sdr_outadd = 0;
	}

	/* Unmap and free the memory allocated for buffers */
	if (sq->bufs[fhdl->src_buff_index] != NULL) {
		videobuf_dma_unmap(sq, videobuf_to_dma(
				   sq->bufs[fhdl->src_buff_index]));
		videobuf_dma_free(videobuf_to_dma(
				  sq->bufs[fhdl->src_buff_index]));
		dev_dbg(rsz_device, "Unmap source buffer.\n");
	}

	if (dq->bufs[fhdl->dst_buff_index] != NULL) {
		videobuf_dma_unmap(dq, videobuf_to_dma(
				   dq->bufs[fhdl->dst_buff_index]));
		videobuf_dma_free(videobuf_to_dma(
				  dq->bufs[fhdl->dst_buff_index]));
		dev_dbg(rsz_device, "Unmap destination buffer.\n");
	}
	isp_unset_callback(fhdl->isp, CBK_RESZ_DONE);

	/* Reset Through-put requirement */
	omap_pm_set_min_bus_tput(fhdl->isp, OCP_INITIATOR_AGENT, 0);

	/* This will flushes the queue */
	if (&fhdl->src_vbq)
		videobuf_queue_cancel(&fhdl->src_vbq);
	if (&fhdl->dst_vbq)
		videobuf_queue_cancel(&fhdl->dst_vbq);

	return 0;
}

/*
 * rsz_ioc_set_params - Set parameters for resizer
 *
 *	@dst: Target registers map structure.
 *	@src: Structure containing all configuration parameters.
 *
 *	Returns 0 if successful, -1 otherwise.
 */
static int rsz_ioc_set_params(struct rsz_fhdl *fhdl)
{
	/* the source always is memory */
	if (fhdl->pipe.in.image.pixelformat == V4L2_PIX_FMT_YUYV ||
	    fhdl->pipe.in.image.pixelformat == V4L2_PIX_FMT_UYVY) {
		fhdl->pipe.in.path = RSZ_MEM_YUV;
	} else {
		fhdl->pipe.in.path = RSZ_MEM_COL8;
	}

	if (ispresizer_try_pipeline(&fhdl->isp_dev->isp_res, &fhdl->pipe) != 0)
		return -EINVAL;

	/* Ready to set hardware and start operation */
	fhdl->config = STATE_CONFIGURED;

	dev_dbg(rsz_device, "Setup parameters.\n");

	return 0;
}

/*
 * rsz_ioc_get_params - Gets the parameter values
 *
 *	@src: Structure containing all configuration parameters.
 *
 *	Returns 0 if successful, -1 otherwise.
 */
static int rsz_ioc_get_params(const struct rsz_fhdl *fhdl)
{
	if (fhdl->config != STATE_CONFIGURED) {
		dev_err(rsz_device, "state not configured\n");
		return -EINVAL;
	}
	return 0;
}

/*
 * rsz_vbq_setup - Sets up the videobuffer size and validates count.
 *
 *	@q: Structure containing the videobuffer queue file handle.
 *	@cnt: Number of buffers requested
 *	@size: Size in bytes of the buffer used for result
 *
 *	Returns 0 if successful, -1 otherwise.
 */
static int rsz_vbq_setup(struct videobuf_queue *q, unsigned int *cnt,
			 unsigned int *size)
{
	struct rsz_fhdl *fhdl = q->priv_data;

	if (*cnt <= 0)
		*cnt = 1;
	if (*cnt > VIDEO_MAX_FRAME)
		*cnt = VIDEO_MAX_FRAME;

	if (!fhdl->pipe.in.image.width || !fhdl->pipe.in.image.height ||
	    !fhdl->pipe.out.image.width || !fhdl->pipe.out.image.height) {
		dev_err(rsz_device, "%s: Can't setup buffers "
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

	dev_dbg(rsz_device, "Setup video buffer type=0x%X.\n", q->type);

	return 0;
}

/*
 * rsz_vbq_release - Videobuffer queue release
 *
 *	@q: Structure containing the videobuffer queue file handle.
 *	@vb: Structure containing the videobuffer used for resizer processing.
 */
static void rsz_vbq_release(struct videobuf_queue *q,
			    struct videobuf_buffer *vb)
{
	struct rsz_fhdl *fhdl = q->priv_data;

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		ispmmu_vunmap(fhdl->isp, fhdl->rsz_sdr_outadd);
		fhdl->rsz_sdr_outadd = 0;
		spin_lock(&fhdl->dst_vbq_lock);
		vb->state = VIDEOBUF_NEEDS_INIT;
		spin_unlock(&fhdl->dst_vbq_lock);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		ispmmu_vunmap(fhdl->isp, fhdl->rsz_sdr_inadd);
		fhdl->rsz_sdr_inadd = 0;
		spin_lock(&fhdl->src_vbq_lock);
		vb->state = VIDEOBUF_NEEDS_INIT;
		spin_unlock(&fhdl->src_vbq_lock);
	}

	if (vb->memory != V4L2_MEMORY_MMAP) {
		videobuf_dma_unmap(q, videobuf_to_dma(vb));
		videobuf_dma_free(videobuf_to_dma(vb));
	}

	dev_dbg(rsz_device, "Release video buffer type=0x%X.\n", q->type);
}

/*
 * rsz_vbq_prepare - Videobuffer is prepared and mmapped.
 *
 *	@q: Structure containing the videobuffer queue file handle.
 *	@vb: Structure containing the videobuffer used for resizer processing.
 *	@field: Type of field to set in videobuffer device.
 *
 *	Returns 0 if successful, or negative otherwise.
 */
static int rsz_vbq_prepare(struct videobuf_queue *q, struct videobuf_buffer *vb,
			   enum v4l2_field field)
{
	struct rsz_fhdl *fhdl = q->priv_data;
	struct videobuf_dmabuf *dma  = videobuf_to_dma(vb);
	dma_addr_t isp_addr;
	int err = 0;

	if (!vb->baddr) {
		dev_err(rsz_device, "%s: No user buffer allocated\n", __func__);
		return -EINVAL;
	}

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		spin_lock(&fhdl->dst_vbq_lock);
		vb->size = fhdl->pipe.out.image.bytesperline *
			   fhdl->pipe.out.image.height;
		vb->width = fhdl->pipe.out.image.width;
		vb->height = fhdl->pipe.out.image.height;
		vb->field = field;
		spin_unlock(&fhdl->dst_vbq_lock);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		spin_lock(&fhdl->src_vbq_lock);
		vb->size = fhdl->pipe.in.image.bytesperline *
			   fhdl->pipe.in.image.height;
		vb->width = fhdl->pipe.in.image.width;
		vb->height = fhdl->pipe.in.image.height;
		vb->field = field;
		spin_unlock(&fhdl->src_vbq_lock);
	} else {
		dev_dbg(rsz_device, "Wrong buffer type=0x%X.\n", q->type);
		return -EINVAL;
	}

	if (vb->state == VIDEOBUF_NEEDS_INIT) {
		dev_dbg(rsz_device, "baddr = %08x\n", (int)vb->baddr);
		err = videobuf_iolock(q, vb, NULL);
		if (!err) {
			isp_addr = ispmmu_vmap(fhdl->isp, dma->sglist,
					       dma->sglen);
			if (!isp_addr) {
				err = -EIO;
			} else {
				if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE)
					fhdl->rsz_sdr_outadd = isp_addr;
				else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT)
					fhdl->rsz_sdr_inadd = isp_addr;
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
		rsz_vbq_release(q, vb);
	}

	dev_dbg(rsz_device, "Prepare video buffer type=0x%X.\n", q->type);

	return err;
}

static void rsz_vbq_queue(struct videobuf_queue *q, struct videobuf_buffer *vb)
{
	struct rsz_fhdl *fhdl = q->priv_data;

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		spin_lock(&fhdl->dst_vbq_lock);
		vb->state = VIDEOBUF_QUEUED;
		spin_unlock(&fhdl->dst_vbq_lock);
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		spin_lock(&fhdl->src_vbq_lock);
		vb->state = VIDEOBUF_QUEUED;
		spin_unlock(&fhdl->src_vbq_lock);
	}
	dev_dbg(rsz_device, "Queue video buffer type=0x%X.\n", q->type);
}

/*
 * rsz_open - Initializes and opens the Resizer
 *
 *	@inode: Inode structure associated with the Resizer
 *	@filp: File structure associated with the Resizer
 *
 *	Returns 0 if successful,
 *		-EBUSY	if its already opened or the ISP is not available.
 *		-ENOMEM if its unable to allocate the device in kernel memory.
 */
static int rsz_open(struct inode *inode, struct file *fp)
{
	int ret = 0;
	struct rsz_fhdl *fhdl;
	struct device_params *device = rsz_params;

	fhdl = kzalloc(sizeof(struct rsz_fhdl), GFP_KERNEL);
	if (NULL == fhdl)
		return -ENOMEM;

	fhdl->isp = isp_get();
	if (!fhdl->isp) {
		dev_err(rsz_device, "\nCan't get ISP device");
		ret = -EBUSY;
		goto err_isp;
	}
	fhdl->isp_dev = dev_get_drvdata(fhdl->isp);

	device->opened++;

	fhdl->config		= STATE_NOTDEFINED;
	fhdl->status		= CHANNEL_FREE;
	fhdl->src_vbq.type	= V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fhdl->dst_vbq.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fhdl->device		= device;

	fp->private_data = fhdl;

	videobuf_queue_sg_init(&fhdl->src_vbq, &device->vbq_ops, NULL,
			       &fhdl->src_vbq_lock, fhdl->src_vbq.type,
			       V4L2_FIELD_NONE, sizeof(struct videobuf_buffer),
			       fhdl);
	spin_lock_init(&fhdl->src_vbq_lock);

	videobuf_queue_sg_init(&fhdl->dst_vbq, &device->vbq_ops, NULL,
			       &fhdl->dst_vbq_lock, fhdl->dst_vbq.type,
			       V4L2_FIELD_NONE, sizeof(struct videobuf_buffer),
			       fhdl);
	spin_lock_init(&fhdl->dst_vbq_lock);

	return 0;

err_isp:
	isp_put();

	kfree(fhdl);

	return ret;
}

/*
 * rsz_release - Releases Resizer and frees up allocated memory
 *
 *	@inode: Inode structure associated with the Resizer
 *	@filp: File structure associated with the Resizer
 *
 *	Returns 0 if successful, or -EBUSY if channel is being used.
 */
static int rsz_release(struct inode *inode, struct file *filp)
{
	u32 timeout = 0;
	struct rsz_fhdl *fhdl = filp->private_data;

	while ((fhdl->status != CHANNEL_FREE) && (timeout < 20)) {
		timeout++;
		schedule();
	}
	rsz_params->opened--;

	/* This will Free memory allocated to the buffers,
	 * and flushes the queue
	 */
	if (&fhdl->src_vbq)
		videobuf_queue_cancel(&fhdl->src_vbq);
	if (&fhdl->dst_vbq)
		videobuf_queue_cancel(&fhdl->dst_vbq);

	filp->private_data = NULL;

	isp_stop(fhdl->isp);

	isp_put();

	kfree(fhdl);

	return 0;
}

/*
 * rsz_mmap - Memory maps the Resizer module.
 *
 *	@file: File structure associated with the Resizer
 *	@vma: Virtual memory area structure.
 *
 *	Returns 0 if successful, or returned value by the map function.
 */
static int rsz_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct rsz_fhdl *fh = file->private_data;
	int rval = 0;

	rval = videobuf_mmap_mapper(&fh->src_vbq, vma);
	if (rval) {
		dev_dbg(rsz_device, "Memory map source fail!\n");
		return rval;
	}

	rval = videobuf_mmap_mapper(&fh->dst_vbq, vma);
	if (rval) {
		dev_dbg(rsz_device, "Memory map destination fail!\n");
		return rval;
	}

	return 0;
}

/*
 * rsz_ioctl - I/O control function for Resizer
 *
 *	@inode: Inode structure associated with the Resizer.
 *	@file: File structure associated with the Resizer.
 *	@cmd: Type of command to execute.
 *	@arg: Argument to send to requested command.
 *
 *	Returns 0 if successful,
 *		-EBUSY if channel is being used,
 *		-EFAULT if copy_from_user() or copy_to_user() fails.
 *		-EINVAL if parameter validation fails.
 *		-1 if bad command passed or access is denied.
 */
static long rsz_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct rsz_fhdl *fhdl = file->private_data;
	struct rsz_stat *status;

	if ((_IOC_TYPE(cmd) != RSZ_IOC_BASE) && (_IOC_TYPE(cmd) != 'V')) {
		dev_err(rsz_device, "Bad command value.\n");
		return -EFAULT;
	}

	if ((_IOC_TYPE(cmd) == RSZ_IOC_BASE) &&
	    (_IOC_NR(cmd) > RSZ_IOC_MAXNR)) {
		dev_err(rsz_device, "Command out of range.\n");
		return -EFAULT;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));

	if (ret) {
		dev_err(rsz_device, "Access denied\n");
		return -EFAULT;
	}

	switch (cmd) {

	case RSZ_S_PARAM:
		if (copy_from_user(&fhdl->pipe, (void *)arg,
				   sizeof(fhdl->pipe)))
			return -EIO;
		ret = rsz_ioc_set_params(fhdl);
		if (ret)
			return ret;
		if (copy_to_user((void *)arg, &fhdl->pipe,
				 sizeof(fhdl->pipe)))
			return -EIO;
		break;

	case RSZ_G_PARAM:
		ret = rsz_ioc_get_params(fhdl);
		if (ret)
			return ret;
		if (copy_to_user((void *)arg, &fhdl->pipe,
				 sizeof(fhdl->pipe)))
			return -EIO;
		break;

	case RSZ_G_STATUS:
		status = (struct rsz_stat *)arg;
		status->ch_busy = fhdl->status;
		status->hw_busy = ispresizer_busy(&fhdl->isp_dev->isp_res);
		break;

	case RSZ_RESIZE:
		if (file->f_flags & O_NONBLOCK) {
			if (ispresizer_busy(&fhdl->isp_dev->isp_res))
				return -EBUSY;
		}
		ret = rsz_ioc_run_engine(fhdl);
		if (ret)
			return ret;
		break;

	case RSZ_REQBUF:
	{
		struct v4l2_requestbuffers v4l2_req;
		if (copy_from_user(&v4l2_req, (void *)arg,
				   sizeof(struct v4l2_requestbuffers)))
			return -EIO;
		if (v4l2_req.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			if (videobuf_reqbufs(&fhdl->src_vbq, &v4l2_req) < 0)
				return -EINVAL;
		} else if (v4l2_req.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			if (videobuf_reqbufs(&fhdl->dst_vbq, &v4l2_req) < 0)
				return -EINVAL;
		} else {
			dev_dbg(rsz_device, "Invalid buffer type.\n");
			return -EINVAL;
		}
		if (copy_to_user((void *)arg, &v4l2_req,
				 sizeof(struct v4l2_requestbuffers)))
			return -EIO;
		break;
	}
	case RSZ_QUERYBUF:
	{
		struct v4l2_buffer v4l2_buf;
		if (copy_from_user(&v4l2_buf, (void *)arg,
				   sizeof(struct v4l2_buffer)))
			return -EIO;
		if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			if (videobuf_querybuf(&fhdl->src_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		} else if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			if (videobuf_querybuf(&fhdl->dst_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		} else {
			dev_dbg(rsz_device, "Invalid buffer type.\n");
			return -EINVAL;
		}
		if (copy_to_user((void *)arg, &v4l2_buf,
				 sizeof(struct v4l2_buffer)))
			return -EIO;
		break;
	}
	case RSZ_QUEUEBUF:
	{
		struct v4l2_buffer v4l2_buf;
		if (copy_from_user(&v4l2_buf, (void *)arg,
				   sizeof(struct v4l2_buffer)))
			return -EIO;
		if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_OUTPUT) {
			if (videobuf_qbuf(&fhdl->src_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		} else if (v4l2_buf.type == V4L2_BUF_TYPE_VIDEO_CAPTURE) {
			if (videobuf_qbuf(&fhdl->dst_vbq, &v4l2_buf) < 0)
				return -EINVAL;
		} else {
			dev_dbg(rsz_device, "Invalid buffer type.\n");
				return -EINVAL;
		}
		if (copy_to_user((void *)arg, &v4l2_buf,
				 sizeof(struct v4l2_buffer)))
			return -EIO;
		break;
	}
	case VIDIOC_QUERYCAP:
	{
		struct v4l2_capability v4l2_cap;
		if (copy_from_user(&v4l2_cap, (void *)arg,
				   sizeof(struct v4l2_capability)))
			return -EIO;

		strcpy(v4l2_cap.driver, "omap3wrapper");
		strcpy(v4l2_cap.card, "omap3wrapper/resizer");
		v4l2_cap.version	= 1.0;;
		v4l2_cap.capabilities	= V4L2_CAP_VIDEO_CAPTURE |
					  V4L2_CAP_READWRITE;

		if (copy_to_user((void *)arg, &v4l2_cap,
				 sizeof(struct v4l2_capability)))
			return -EIO;
		dev_dbg(rsz_device, "Query cap done.\n");
		break;
	}
	default:
		dev_err(rsz_device, "IOC: Invalid Command Value!\n");
		ret = -EINVAL;
	}
	return ret;
}

static const struct file_operations rsz_fops = {
	.owner		= THIS_MODULE,
	.open		= rsz_open,
	.release	= rsz_release,
	.mmap		= rsz_mmap,
	.unlocked_ioctl	= rsz_ioctl,
};

/*
 * rsz_driver_probe - Checks for device presence
 *
 *	@device: Structure containing details of the current device.
 */
static int rsz_driver_probe(struct platform_device *device)
{
	return 0;
}

/*
 * rsz_driver_remove - Handles the removal of the driver
 *
 *	@omap_resizer_device: Structure containing details for device.
 */
static int rsz_driver_remove(struct platform_device *omap_resizer_device)
{
	return 0;
}

static struct platform_driver omap_resizer_driver = {
	.probe	= rsz_driver_probe,
	.remove	= rsz_driver_remove,
	.driver	= {
		.bus	= &platform_bus_type,
		.name	= OMAP_RESIZER_NAME,
	},
};

/*
 * rsz_device_release - Acts when Reference count is zero
 *
 *	@device: Structure containing ISP resizer global information
 *
 *	This is called when the reference count goes to zero.
 */
static void rsz_device_release(struct device *device)
{
}

static struct platform_device omap_resizer_device = {
	.name	= OMAP_RESIZER_NAME,
	.id	= 2,
	.dev	= {
		.release = rsz_device_release,
	}
};

/*
 * omap_resizer_module_init - Initialization of Resizer
 *
 *	Returns 0 if successful,
 *		-ENOMEM if could not allocate memory.
 *		-ENODEV if could not register the device
 */
static int __init omap_resizer_module_init(void)
{
	int ret = 0;
	struct device_params *device;

	device = kzalloc(sizeof(struct device_params), GFP_KERNEL);
	if (!device) {
		dev_crit(rsz_device, " could not allocate memory\n");
		return -ENOMEM;
	}
	rsz_major = register_chrdev(0, OMAP_RESIZER_NAME, &rsz_fops);

	/* register driver as a platform driver */
	ret = platform_driver_register(&omap_resizer_driver);
	if (ret) {
		dev_crit(rsz_device, "failed to register platform driver!\n");
		goto fail2;
	}

	/* Register the drive as a platform device */
	ret = platform_device_register(&omap_resizer_device);
	if (ret) {
		dev_crit(rsz_device, "failed to register platform device!\n");
		goto fail3;
	}

	rsz_class = class_create(THIS_MODULE, OMAP_RESIZER_NAME);
	if (!rsz_class) {
		dev_crit(rsz_device, "Failed to create class!\n");
		goto fail4;
	}

	/* make entry in the devfs */
	rsz_device = device_create(rsz_class, rsz_device, MKDEV(rsz_major, 0),
				   NULL, OMAP_RESIZER_NAME);

	device->opened = 0;

	device->vbq_ops.buf_setup	= rsz_vbq_setup;
	device->vbq_ops.buf_prepare	= rsz_vbq_prepare;
	device->vbq_ops.buf_release	= rsz_vbq_release;
	device->vbq_ops.buf_queue	= rsz_vbq_queue;

	init_completion(&device->isr_complete);

	rsz_params = device;

	return 0;

fail4:
	platform_device_unregister(&omap_resizer_device);
fail3:
	platform_driver_unregister(&omap_resizer_driver);
fail2:
	unregister_chrdev(rsz_major, OMAP_RESIZER_NAME);

	kfree(device);

	return ret;
}

/*
 * omap_resizer_module_exit - Close of Resizer
 */
void __exit omap_resizer_module_exit(void)
{
	device_destroy(rsz_class, MKDEV(rsz_major, 0));
	class_destroy(rsz_class);
	platform_device_unregister(&omap_resizer_device);
	platform_driver_unregister(&omap_resizer_driver);
	unregister_chrdev(rsz_major, OMAP_RESIZER_NAME);
	kfree(rsz_params);
	rsz_major = -1;
}

module_init(omap_resizer_module_init)
module_exit(omap_resizer_module_exit)

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("OMAP ISP Resizer");
MODULE_LICENSE("GPL");
