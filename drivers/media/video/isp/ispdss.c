/*
 * drivers/media/video/isp/ispdss.c
 *
 * Wrapper for Resizer module in TI's OMAP3430 ISP
 *
 * Copyright (C) 2008 Texas Instruments, Inc.
 *
 * Contributors:
 * 	Sergio Aguirre <saaguirre@ti.com>
 * 	Troy Laramy <t-laramy@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <media/v4l2-dev.h>
#include <asm/cacheflush.h>
#include <plat/iovmm.h>

#include "isp.h"
#include "ispreg.h"
#include "ispresizer.h"
#include <linux/ispdss.h>

/**
 * WA: Adjustment operation speed of ISP Resizer engine
 */
#define ISPDSS_RSZ_EXPAND_720p	16

enum config_done {
	STATE_CONFIGURED,		/* Resizer driver configured */
	STATE_NOT_CONFIGURED	/* Resizer driver not configured */
};

static struct {
	unsigned char opened;
	struct completion compl_isr;
	struct videobuf_queue_ops vbq_ops;
	ispdss_callback 	callback;
	u32 in_buf_virt_addr[32];
	u32 out_buf_virt_addr[32];
	u32 num_video_buffers;
	dma_addr_t tmp_buf;
	size_t tmp_buf_size;
	int status;
	enum config_done config_state;
	struct device *isp;
} dev_ctx;

/* ispdss_isr - Interrupt Service Routine for Resizer wrapper
 * @status: ISP IRQ0STATUS register value
 * @arg1: Currently not used
 * @arg2: Currently not used
 *
 * Interrupt Service Routine for Resizer wrapper
 **/
static void ispdss_isr(unsigned long status, isp_vbq_callback_ptr arg1,
		       void *arg2)
{
	if ((status & RESZ_DONE) != RESZ_DONE)
		return;

	complete(&(dev_ctx.compl_isr));
}

static void ispdss_tmp_buf_free(void)
{
	struct isp_device *isp_res = dev_get_drvdata(dev_ctx.isp);
	if (dev_ctx.tmp_buf) {
		iommu_vfree(isp_res->iommu, dev_ctx.tmp_buf);
		dev_ctx.tmp_buf = 0;
		dev_ctx.tmp_buf_size = 0;
	}
}

static u32 ispdss_tmp_buf_alloc(size_t size)
{
	struct isp_device *isp_res = dev_get_drvdata(dev_ctx.isp);

	ispdss_tmp_buf_free();
	dev_ctx.tmp_buf = iommu_vmalloc(isp_res->iommu, 0, size, IOMMU_FLAG);
	if (IS_ERR((void *)dev_ctx.tmp_buf)) {
		printk(KERN_ERR "ispmmu_vmap mapping failed ");
		return -ENOMEM;
	}
	dev_ctx.tmp_buf_size = size;

	return 0;
}

/**
 * ispdss_get_resource - get the ISP resource from the kernel space.
 */
int ispdss_get_resource(void)
{
	if (dev_ctx.opened) {
		dev_err(NULL, "dss: get_resource: device is already opened\n");
		return -EBUSY;
	}

	dev_ctx.isp = isp_get();
	if (!dev_ctx.isp) {
		dev_err(NULL, "dss: Can't get ISP resource\n");
		return -EACCES;
	}
	dev_ctx.opened	= 1;
	dev_ctx.config_state = STATE_NOT_CONFIGURED;

	init_completion(&dev_ctx.compl_isr);

	return 0;
}

/**
 * ispdss_put_resource - Release all the resource.
 **/
void ispdss_put_resource()
{
	struct isp_device *isp_res = dev_get_drvdata(dev_ctx.isp);
	int i = 0;

	if (dev_ctx.opened != 1)
		return;

	/* unmap output buffers if allocated */
	for (i = 0; i < dev_ctx.num_video_buffers; ++i) {
		if (dev_ctx.out_buf_virt_addr[i]) {
			iommu_kunmap(isp_res->iommu,
				     dev_ctx.out_buf_virt_addr[i]);
			dev_ctx.out_buf_virt_addr[i] = 0;
		}
		if (dev_ctx.in_buf_virt_addr[i]) {
			iommu_kunmap(isp_res->iommu,
				     dev_ctx.in_buf_virt_addr[i]);
			dev_ctx.in_buf_virt_addr[i] = 0;
		}
	}
	ispdss_tmp_buf_free();

	/* make device available */
	dev_ctx.opened = 0;

	/* stop the isp */
	isp_stop(dev_ctx.isp);

	/* release isp resource*/
	isp_put();

	return ;
}

/**
  * ispdss_configure - configure the Resizer parameters
  * @params: Structure containing the Resizer Wrapper parameters
  * @callback: callback function to call after resizing is over
  * @arg1: argument to callback function
  *
  * This function can be called from DSS to set the parameter of resizer
  * in case there is need to downsize the input image through resizer
  * before calling this function calling driver must already have called
  * ispdss_get_resource() function so that necessory initialization is over.
  * Returns 0 if successful,
  **/
int ispdss_configure(struct isp_node *pipe, ispdss_callback callback,
		  u32 num_video_buffers, void *arg1)
{
	struct isp_device *isp = dev_get_drvdata(dev_ctx.isp);
	struct isp_res_device *isp_res = &isp->isp_res;

	/* the source always is memory */
	if (pipe->in.image.pixelformat == V4L2_PIX_FMT_YUYV ||
	    pipe->in.image.pixelformat == V4L2_PIX_FMT_UYVY)
		pipe->in.path = RSZ_MEM_YUV;
	else
		pipe->in.path = RSZ_MEM_COL8;

	if (ispresizer_try_pipeline(isp_res, pipe) != 0)
		return -EINVAL;

	dev_ctx.callback = callback;
	dev_ctx.num_video_buffers = num_video_buffers;
	dev_ctx.config_state = STATE_CONFIGURED;

	return 0;
}

/** ispdss_begin - Function to be called by DSS when resizing of the input
  * image buffer is needed
  * @slot: buffer index where the input image is stored
  * @output_buffer_index: output buffer index where output of resizer will
  * be stored
  * @out_off: The line size in bytes for output buffer. as most probably
  * this will be VRFB with YUV422 data, it should come 0x2000 as input
  * @out_phy_add: physical address of the start of output memory area for this
  * @in_phy_add: physical address of the start of input memory area for this
  * @in_off:: The line size in bytes for output buffer.
  * ispdss_begin()  takes the input buffer index and output buffer index
  * to start the process of resizing. after resizing is complete,
  * the callback function will be called with the argument.
  * Indexes of the input and output buffers are used so that it is faster
  * and easier to configure the input and output address for the ISP resizer.
  * As per the current implementation, DSS uses six VRFB contexts for rotation.
  * for both input and output buffers index and physical address has been taken
  * as argument. if this buffer is not already mapped to ISP address space we
  * use physical address to map it, otherwise only the index is used.
  **/
int ispdss_begin(struct isp_node *pipe, u32 input_buffer_index,
		 int output_buffer_index, u32 out_off, u32 out_phy_add,
		 u32 in_phy_add, u32 in_off)
{
	unsigned int output_size;
	struct isp_device *isp = dev_get_drvdata(dev_ctx.isp);
	struct isp_res_device *isp_res = &isp->isp_res;

	if (output_buffer_index >= dev_ctx.num_video_buffers) {
		dev_err(dev_ctx.isp,
		"ouput buffer index is out of range %d", output_buffer_index);
		return -EINVAL;
	}

	if (dev_ctx.config_state != STATE_CONFIGURED) {
		dev_err(dev_ctx.isp, "State not configured \n");
		return -EINVAL;
	}

	dev_ctx.tmp_buf_size = PAGE_ALIGN(pipe->out.image.bytesperline *
					  pipe->out.image.height);
	ispdss_tmp_buf_alloc(dev_ctx.tmp_buf_size);

	pipe->in.path = RSZ_MEM_YUV;
	if (ispresizer_s_pipeline(isp_res, pipe) != 0)
		return -EINVAL;

	/* If this output buffer has not been mapped till now then map it */
	if (!dev_ctx.out_buf_virt_addr[output_buffer_index]) {
		output_size = pipe->out.image.height * out_off;
		dev_ctx.out_buf_virt_addr[output_buffer_index] =
			iommu_kmap(isp->iommu, 0, out_phy_add, output_size,
				   IOMMU_FLAG);
		if (IS_ERR_VALUE(
			dev_ctx.out_buf_virt_addr[output_buffer_index])) {
			dev_err(dev_ctx.isp, "Mapping of output buffer failed"
						"for index \n");
			return -ENOMEM;
		}
	}

	if (!dev_ctx.in_buf_virt_addr[input_buffer_index]) {
		dev_ctx.in_buf_virt_addr[input_buffer_index] =
			iommu_kmap(isp->iommu, 0, in_phy_add, in_off,
				   IOMMU_FLAG);
		if (IS_ERR_VALUE(
			dev_ctx.in_buf_virt_addr[input_buffer_index])) {
			dev_err(dev_ctx.isp, "Mapping of input buffer failed"
						"for index \n");
			return -ENOMEM;
		}
	}

	if (ispresizer_set_inaddr(isp_res,
				  dev_ctx.in_buf_virt_addr[input_buffer_index],
				  pipe) != 0)
		return -EINVAL;

	if (ispresizer_set_out_offset(isp_res, out_off) != 0)
		return -EINVAL;

	if (ispresizer_set_outaddr(isp_res,
		(u32)dev_ctx.out_buf_virt_addr[output_buffer_index]) != 0)
		return -EINVAL;

	/* Set ISP callback for the resizing complete even */
	if (isp_set_callback(dev_ctx.isp, CBK_RESZ_DONE, ispdss_isr,
			     (void *) NULL, (void *)NULL)) {
		dev_err(dev_ctx.isp, "No callback for RSZR\n");
		return -1;
	}

	/* All settings are done.Enable the resizer */
	init_completion(&dev_ctx.compl_isr);

	isp_start(dev_ctx.isp);

	/* WA: Slowdown ISP Resizer to reduce used memory bandwidth */
	isp_reg_and_or(dev_ctx.isp, OMAP3_ISP_IOMEM_SBL, ISPSBL_SDR_REQ_EXP,
		       ~ISPSBL_SDR_REQ_RSZ_EXP_MASK,
		       ISPDSS_RSZ_EXPAND_720p << ISPSBL_SDR_REQ_RSZ_EXP_SHIFT);

	ispresizer_enable(isp_res, 1);

	/* Wait for resizing complete event */
	if (wait_for_completion_interruptible_timeout(
				&dev_ctx.compl_isr, msecs_to_jiffies(500)) == 0)
		dev_crit(dev_ctx.isp, "\nTimeout exit from "
			 "wait_for_completion\n");

	/* Unset the ISP callback function */
	isp_unset_callback(dev_ctx.isp, CBK_RESZ_DONE);

	return 0;
}

