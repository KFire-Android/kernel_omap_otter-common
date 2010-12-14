/*
 * ispresizer.c
 *
 * Driver Library for Resizer module in TI's OMAP3 Camera ISP
 *
 * Copyright (C)2009 Texas Instruments, Inc.
 *
 * Contributors:
 * 	Sameer Venkatraman <sameerv@ti.com>
 * 	Mohit Jalori
 *	Sergio Aguirre <saaguirre@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/device.h>

#include "isp.h"
#include "ispreg.h"
#include "ispresizer.h"
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
#include "ispresizer_dfs.h"
#endif

/*
 * Resizer constants
 * "OMAP3430 TRM ES3.1"
 */
#define MIN_IN_WIDTH			32
#define MIN_IN_HEIGHT			32
#define MAX_IN_WIDTH_MEMORY_MODE	4095

#define MAX_IN_WIDTH_ONTHEFLY_MODE	1280
#define MAX_IN_WIDTH_ONTHEFLY_MODE_ES2	4095
#define MAX_IN_WIDTH_ONTHEFLY_MODE_ES3	4095
#define MAX_IN_HEIGHT			4095
#define MIN_RESIZE_VALUE		64
#define MAX_RESIZE_VALUE		1024
#define MID_RESIZE_VALUE		512
#define MAX_SCALE_FACTOR		4

/*
 * Constants for coarse input pointer calculations
 */
#define CIP_4PHASE		16	/* in 1/8 pixel precision */
#define CIP_7PHASE		32	/* in 1/4 pixel precision */

/*
 * Resizer Use Constraints
 * "TRM ES3.1, table 12-46"
 */
#define MIN_OUT_WIDTH			16
#define MAX_4TAP_OUT_WIDTH		1280
#define MAX_7TAP_OUT_WIDTH		640
#define MAX_4TAP_OUT_WIDTH_ES2		3312
#define MAX_7TAP_OUT_WIDTH_ES2		1650
#define MAX_4TAP_OUT_WIDTH_ES3		4095
#define MAX_7TAP_OUT_WIDTH_ES3		2048
#define MIN_OUT_HEIGHT			2
#define MAX_OUT_HEIGHT			4095

#define DEFAULTSTPHASE			1
#define RESIZECONSTANT			256
#define TAP7				7
#define TAP4				4

#define OUT_WIDTH_ALIGN			2
#define OUT_WIDTH_ALIGN_UP		16
#define OUT_HEIGHT_ALIGN		2

#define PHY_ADDRESS_ALIGN		32

/* Default configuration of resizer,filter coefficients,yenh for camera isp */
static struct isprsz_coef ispreszdefcoef = {
	/* For 8-phase 4-tap horizontal filter: */
	{
		0x0000, 0x0100, 0x0000, 0x0000, 0x03FA, 0x00F6, 0x0010, 0x0000,
		0x03F9, 0x00DB, 0x002C, 0x0000, 0x03FB, 0x00B3, 0x0053, 0x03FF,
		0x03FD, 0x0082, 0x0084, 0x03FD, 0x03FF, 0x0053, 0x00B3, 0x03FB,
		0x0000, 0x002C, 0x00DB, 0x03F9, 0x0000, 0x0010, 0x00F6, 0x03FA
	},
	/* For 8-phase 4-tap vertical filter: */
	{
		0x0000, 0x0100, 0x0000, 0x0000, 0x03FA, 0x00F6, 0x0010, 0x0000,
		0x03F9, 0x00DB, 0x002C, 0x0000, 0x03FB, 0x00B3, 0x0053, 0x03FF,
		0x03FD, 0x0082, 0x0084, 0x03FD, 0x03FF, 0x0053, 0x00B3, 0x03FB,
		0x0000, 0x002C, 0x00DB, 0x03F9, 0x0000, 0x0010, 0x00F6, 0x03FA
	},
	/* For 4-phase 7-tap horizontal filter: */
	#define UNUSED 0
	{
		0x0004, 0x0023, 0x005A, 0x0058, 0x0023, 0x0004, 0x0000, UNUSED,
		0x0002, 0x0018, 0x004d, 0x0060, 0x0031, 0x0008, 0x0000, UNUSED,
		0x0001, 0x000f, 0x003f, 0x0062, 0x003f, 0x000f, 0x0001, UNUSED,
		0x0000, 0x0008, 0x0031, 0x0060, 0x004d, 0x0018, 0x0002, UNUSED
	},
	/* For 4-phase 7-tap vertical filter: */
	{
		0x0004, 0x0023, 0x005A, 0x0058, 0x0023, 0x0004, 0x0000, UNUSED,
		0x0002, 0x0018, 0x004d, 0x0060, 0x0031, 0x0008, 0x0000, UNUSED,
		0x0001, 0x000f, 0x003f, 0x0062, 0x003f, 0x000f, 0x0001, UNUSED,
		0x0000, 0x0008, 0x0031, 0x0060, 0x004d, 0x0018, 0x0002, UNUSED
	}
	#undef UNUSED
};

/**
 * ispresizer_set_coeffs - Setup default coefficents for polyphase filter.
 * @dst: Target registers map structure.
 * @coeffs: filter coefficients structure or NULL for default values
 * @h_ratio: Horizontal resizing value.
 * @v_ratio: Vertical resizing value.
 */
void ispresizer_set_coeffs(struct device *dev, struct isprsz_coef *coeffs,
			   unsigned h_ratio, unsigned v_ratio)
{
	const u16 *cf_ptr;
	int ix;
	u32 rval;

	/* Init horizontal filter coefficients */
	if (h_ratio > MID_RESIZE_VALUE) {
		if (coeffs != NULL)
			cf_ptr = coeffs->h_filter_coef_7tap;
		else
			cf_ptr = ispreszdefcoef.h_filter_coef_7tap;
	} else {
		if (coeffs != NULL)
			cf_ptr = coeffs->h_filter_coef_4tap;
		else
			cf_ptr = ispreszdefcoef.h_filter_coef_4tap;
	}

	for (ix = ISPRSZ_HFILT10; ix <= ISPRSZ_HFILT3130; ix += sizeof(u32)) {
		rval  = (*cf_ptr++ & ISPRSZ_HFILT_COEF0_MASK) <<
			ISPRSZ_HFILT_COEF0_SHIFT;
		rval |= (*cf_ptr++ & ISPRSZ_HFILT_COEF1_MASK) <<
			ISPRSZ_HFILT_COEF1_SHIFT;
		isp_reg_writel(dev, rval, OMAP3_ISP_IOMEM_RESZ, ix);
	}

	/* Init vertical filter coefficients */
	if (v_ratio > MID_RESIZE_VALUE) {
		if (coeffs != NULL)
			cf_ptr = coeffs->v_filter_coef_7tap;
		else
			cf_ptr = ispreszdefcoef.v_filter_coef_7tap;
	} else {
		if (coeffs != NULL)
			cf_ptr = coeffs->v_filter_coef_4tap;
		else
			cf_ptr = ispreszdefcoef.v_filter_coef_4tap;
	}

	for (ix = ISPRSZ_VFILT10; ix <= ISPRSZ_VFILT3130; ix += sizeof(u32)) {
		rval  = (*cf_ptr++ & ISPRSZ_VFILT_COEF0_MASK) <<
			ISPRSZ_VFILT_COEF0_SHIFT;
		rval |= (*cf_ptr++ & ISPRSZ_VFILT_COEF1_MASK) <<
			ISPRSZ_VFILT_COEF1_SHIFT;
		isp_reg_writel(dev, rval, OMAP3_ISP_IOMEM_RESZ, ix);
	}
}

/**
 * ispresizer_try_fmt - Try format.
 */
static int ispresizer_try_fmt(struct isp_node *pipe, bool memory)
{
	s16 rsz;
	u32 sph = DEFAULTSTPHASE;
	int max_in_otf, max_out_4tap, max_out_7tap;

	if (pipe->out.image.width < MIN_OUT_WIDTH)
		return -EINVAL;
	if (pipe->out.image.height < MIN_OUT_HEIGHT)
		return -EINVAL;

	pipe->out.image.height = min_t(u32, pipe->out.image.height,
				       MAX_OUT_HEIGHT);

	switch (omap_rev()) {
	case OMAP3430_REV_ES1_0:
		max_in_otf = MAX_IN_WIDTH_ONTHEFLY_MODE;
		max_out_4tap = MAX_4TAP_OUT_WIDTH;
		max_out_7tap = MAX_7TAP_OUT_WIDTH;
		break;
	case OMAP3630_REV_ES1_0:
	case OMAP3630_REV_ES1_1:
	case OMAP3630_REV_ES1_2:
		max_in_otf = MAX_IN_WIDTH_ONTHEFLY_MODE_ES3;
		max_out_4tap = MAX_4TAP_OUT_WIDTH_ES3;
		max_out_7tap = MAX_7TAP_OUT_WIDTH_ES3;
		break;
	default:
		max_in_otf = MAX_IN_WIDTH_ONTHEFLY_MODE_ES2;
		max_out_4tap = MAX_4TAP_OUT_WIDTH_ES2;
		max_out_7tap = MAX_7TAP_OUT_WIDTH_ES2;
	}

	if (pipe->in.image.width < MIN_IN_WIDTH)
		return -EINVAL;

	if (pipe->in.image.height < MIN_IN_HEIGHT)
		return -EINVAL;

	if (memory) {
		if (pipe->in.image.width > MAX_IN_WIDTH_MEMORY_MODE)
			return -EINVAL;
	} else {
		if (pipe->in.image.width > max_in_otf)
			return -EINVAL;
	}

	/**
	 * Calculate vertical ratio.
	 *  See OMAP34XX Technical Refernce Manual.
	 */
	pipe->out.image.height = ALIGN(pipe->out.image.height,
				       OUT_HEIGHT_ALIGN);

	/* Set default crop, if not defined */
	if (pipe->in.crop.height <= 0 || pipe->in.crop.top < 0 ||
	    pipe->in.crop.height + pipe->in.crop.top > pipe->in.image.height) {
		pipe->in.crop.height = pipe->in.image.height;
		pipe->in.crop.top = 0;
	}
	/**
	 * Calculate vertical ratio.
	 *  See OMAP34XX Technical Refernce Manual.
	 */
	rsz = pipe->in.crop.height * RESIZECONSTANT / pipe->out.image.height;
	if (rsz > MID_RESIZE_VALUE) {
		if (pipe->out.image.width > max_out_7tap) {
			pipe->out.image.width = max_out_7tap &
						~(OUT_WIDTH_ALIGN - 1);
			/* update output crop also */
			pipe->out.crop.width = pipe->out.image.width;
			pipe->out.crop.left = 0;
		}
		rsz = ((pipe->in.crop.height - TAP7)
			* RESIZECONSTANT - sph * 64 - CIP_7PHASE)
			/ (pipe->out.image.height - 1);
	} else {
		if (pipe->out.image.width > max_out_4tap) {
			pipe->out.image.width = max_out_4tap &
						~(OUT_WIDTH_ALIGN - 1);
			/* update output crop also */
			pipe->out.crop.width = pipe->out.image.width;
			pipe->out.crop.left = 0;
		}
		rsz = ((pipe->in.crop.height - TAP4)
			* RESIZECONSTANT - sph * 32 - CIP_4PHASE)
			/ (pipe->out.image.height - 1);
	}

	/* To preserve range of vertical ratio */
	if (rsz < MIN_RESIZE_VALUE || rsz > MAX_RESIZE_VALUE) {
		rsz = clamp_t(s16, rsz, MIN_RESIZE_VALUE, MAX_RESIZE_VALUE);
		pipe->out.image.height = pipe->in.crop.height *
					 RESIZECONSTANT / rsz;
		if (rsz == MIN_RESIZE_VALUE)
			pipe->out.image.height &= ~(OUT_HEIGHT_ALIGN - 1);
		else
			pipe->out.image.height = ALIGN(pipe->out.image.height,
						       OUT_HEIGHT_ALIGN);
		/* update output crop also */
		pipe->out.crop.height = pipe->out.image.height;
		pipe->out.crop.top = 0;
	}

	/**
	 * Calculate horizontal ratio.
	 *  See OMAP34XX Technical Refernce Manual.
	 */
	rsz = pipe->in.crop.width * RESIZECONSTANT / pipe->out.image.width;
	if (rsz > RESIZECONSTANT)
		pipe->out.image.width = ALIGN(pipe->out.image.width,
					      OUT_WIDTH_ALIGN);
	else
		pipe->out.image.width = ALIGN(pipe->out.image.width,
					      OUT_WIDTH_ALIGN_UP);

	/* Set default crop, if not defined */
	if (pipe->in.crop.width <= 0 || pipe->in.crop.left < 0 ||
	    pipe->in.crop.width + pipe->in.crop.left > pipe->in.image.width) {
		pipe->in.crop.width = pipe->in.image.width;
		pipe->in.crop.left = 0;
	}

	/**
	 * Calculate horizontal ratio.
	 *  See OMAP34XX Technical Refernce Manual.
	 */
	rsz = pipe->in.crop.width * RESIZECONSTANT / pipe->out.image.width;
	if (rsz > MID_RESIZE_VALUE) {
		rsz = ((pipe->in.crop.width - TAP7)
			* RESIZECONSTANT - sph * 64 - CIP_7PHASE)
			/ (pipe->out.image.width - 1);
	} else {
		rsz = ((pipe->in.crop.width - TAP4)
			* RESIZECONSTANT - sph * 32 - CIP_4PHASE)
			/ (pipe->out.image.width - 1);
	}

	/* To preserve range of horizontal ratio */
	if (rsz < MIN_RESIZE_VALUE || rsz > MAX_RESIZE_VALUE) {
		rsz = clamp_t(s16, rsz, MIN_RESIZE_VALUE, MAX_RESIZE_VALUE);
		pipe->out.image.width = pipe->in.crop.width *
					RESIZECONSTANT / rsz;
		if (rsz == MIN_RESIZE_VALUE)
			pipe->out.image.width &= ~(OUT_WIDTH_ALIGN_UP - 1);
		else
			pipe->out.image.width = ALIGN(pipe->out.image.width,
						      OUT_WIDTH_ALIGN_UP);
		/* update output crop also */
		pipe->out.crop.width = pipe->out.image.width;
		pipe->out.crop.left = 0;
	}

	return 0;
}

/**
 * ispresizer_try_ratio - Calculate resize values, input and output size.
 */
static int ispresizer_try_ratio(struct device *dev, struct isp_node *pipe,
				struct v4l2_rect *phy_rect,
				u16 *h_ratio, u16 *v_ratio)
{
	u16 h_rsz, v_rsz;
	u32 sph = DEFAULTSTPHASE;
	struct v4l2_rect try_rect;

	/* Check range of requested vertical crop */
	if (pipe->in.crop.height < MIN_IN_HEIGHT || pipe->in.crop.top < 0 ||
	    pipe->in.crop.height + pipe->in.crop.top > pipe->in.image.height) {
		dev_err(dev, "Vertical crop is out of range Crop: T=%d H=%d "
			"Image: Height=%d\n", pipe->in.crop.top,
			pipe->in.crop.height, pipe->in.image.height);
		pipe->in.crop.height = pipe->in.image.height;
		pipe->in.crop.top = 0;
	}

	/**
	 * Calculate vertical ratio.
	 *  See OMAP34XX Technical Refernce Manual.
	 */
	v_rsz = pipe->in.crop.height * RESIZECONSTANT / pipe->out.image.height;
	if (v_rsz > MID_RESIZE_VALUE) {
		v_rsz = ((pipe->in.crop.height - TAP7)
			* RESIZECONSTANT - sph * 64 - CIP_7PHASE)
			/ (pipe->out.image.height - 1);
	} else {
		v_rsz = ((pipe->in.crop.height - TAP4)
			* RESIZECONSTANT - sph * 32 - CIP_4PHASE)
			/ (pipe->out.image.height - 1);
	}

	if (v_rsz < MIN_RESIZE_VALUE || v_rsz > MAX_RESIZE_VALUE) {
		dev_err(dev, "Vertical ratio is out of range: %d", v_rsz);
		v_rsz = clamp_t(u16, v_rsz, MIN_RESIZE_VALUE, MAX_RESIZE_VALUE);
		pipe->in.crop.height = pipe->out.image.height * v_rsz /
				       RESIZECONSTANT;
	}

	/**
	 * Recalculate input. See OMAP34XX TRM.
	 *  4-phase, 7-tap mode
	 *     ih = (64 * spv + (oh - 1) * vrsz + 32) >> 8 + 7
	 *  8-phase, 4-tap mode
	 *     ih = (32 * spv + (oh - 1) * vrsz + 16) >> 8 + 4
	 */
	if (v_rsz > MID_RESIZE_VALUE) {
		try_rect.height = (64 * sph + (pipe->out.image.height - 1) *
				   v_rsz + CIP_7PHASE) / RESIZECONSTANT + TAP7;
	} else {
		try_rect.height = (32 * sph + (pipe->out.image.height - 1) *
				   v_rsz + CIP_4PHASE) / RESIZECONSTANT + TAP4;
	}
	/* First storing user requested offset */
	try_rect.top = pipe->in.crop.top;
	/* Compensation of image centering after resize */
	try_rect.top += (pipe->in.crop.height - try_rect.height) / 2;
	/* Validate range of physical dimension of crop */
	try_rect.top = clamp_t(u16, try_rect.top, 0, pipe->in.image.height -
			       pipe->in.crop.height);
	pipe->in.crop.height = clamp_t(u16, pipe->in.crop.height, MIN_IN_HEIGHT,
				       pipe->in.image.height);

	/* Check range of requested horizontal crop */
	if (pipe->in.crop.width < MIN_IN_WIDTH || pipe->in.crop.left < 0 ||
	    pipe->in.crop.width + pipe->in.crop.left > pipe->in.image.width) {
		dev_err(dev, "Horizontal crop is out of range Crop: L=%d W=%d "
			"Image: Width=%d\n", pipe->in.crop.left,
			pipe->in.crop.width, pipe->in.image.width);
		pipe->in.crop.width = pipe->in.image.width;
		pipe->in.crop.left = 0;
	}

	/**
	 * Calculate horizontal ratio.
	 *  See OMAP34XX Technical Refernce Manual.
	 */
	h_rsz = pipe->in.crop.width * RESIZECONSTANT / pipe->out.image.width;
	if (h_rsz > MID_RESIZE_VALUE) {
		h_rsz = ((pipe->in.crop.width - TAP7)
			* RESIZECONSTANT - sph * 64 - CIP_7PHASE)
			/ (pipe->out.image.width - 1);
	} else {
		h_rsz = ((pipe->in.crop.width - TAP4)
			* RESIZECONSTANT - sph * 32 - CIP_4PHASE)
			/ (pipe->out.image.width - 1);
	}

	if (h_rsz < MIN_RESIZE_VALUE || h_rsz > MAX_RESIZE_VALUE) {
		dev_err(dev, "Horizontal ratio is out of range: %d", h_rsz);
		h_rsz = clamp_t(u16, h_rsz, MIN_RESIZE_VALUE, MAX_RESIZE_VALUE);
		pipe->in.crop.height = pipe->out.image.height * v_rsz /
				       RESIZECONSTANT;
	}

	/**
	 * Recalculate input. See OMAP34XX TRM.
	 *  4-phase, 7-tap mode
	 *     iw = (64 * sph + (ow - 1) * hrsz + 32) >> 8 + 7
	 *  8-phase, 4-tap mode
	 *     iw = (32 * sph + (ow - 1) * hrsz + 16) >> 8 + 7
	 */
	if (h_rsz > MID_RESIZE_VALUE) {
		try_rect.width = (64 * sph + (pipe->out.image.width - 1) *
				  h_rsz + CIP_7PHASE) / RESIZECONSTANT + TAP7;
	} else {
		try_rect.width = (32 * sph + (pipe->out.image.width - 1) *
				  h_rsz + CIP_4PHASE) / RESIZECONSTANT + TAP4;
	}
	/* First storing user requested offset */
	try_rect.left = pipe->in.crop.left;
	/* Compensation of image centering after resize */
	try_rect.left += (pipe->in.crop.width - try_rect.width) / 2;
	/* Validate range of physical dimension of crop */
	try_rect.left = clamp_t(u16, try_rect.left, 0, pipe->in.image.width -
				pipe->in.crop.width);
	pipe->in.crop.width = clamp_t(u16, pipe->in.crop.width, MIN_IN_WIDTH,
				      pipe->in.image.width);

	/* Storing parameters for engine crop */
	*phy_rect = try_rect;
	/* Storing calculated ratio parameters */
	*v_ratio = v_rsz;
	*h_ratio = h_rsz;

	return 0;
}

/**
 * ispresizer_is_upscale - Return operation type up/down scale.
 *
 * Returns true for up-scale operation and false for down-scale.
 */
bool ispresizer_is_upscale(struct isp_node *pipe)
{
	if (pipe->in.image.width * pipe->in.image.height <
	    pipe->out.image.width * pipe->out.image.height)
		return true;
	else
		return false;
}

/**
 * ispresizer_set_ratio - Setup horizontal and vertical resizing value
 * @dev: Device context.
 * @h_ratio: Horizontal ratio.
 * @v_ratio: Vertical ratio.
 *
 * Resizing range from 64 to 1024
 */
static inline void ispresizer_set_ratio(struct device *dev, u16 h_ratio,
					u16 v_ratio)
{
	u32 rgval;

	rgval = isp_reg_readl(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT) &
			      ~(ISPRSZ_CNT_HRSZ_MASK | ISPRSZ_CNT_VRSZ_MASK);
	rgval |= ((h_ratio - 1) << ISPRSZ_CNT_HRSZ_SHIFT)
		  & ISPRSZ_CNT_HRSZ_MASK;
	rgval |= ((v_ratio - 1) << ISPRSZ_CNT_VRSZ_SHIFT)
		  & ISPRSZ_CNT_VRSZ_MASK;
	isp_reg_writel(dev, rgval, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT);

	dev_dbg(dev, "%s: Ratio: H=%d V=%d\n", __func__, h_ratio, v_ratio);
}

/**
 * ispresizer_set_start - Setup vertical and horizontal start position
 * @dev: Device context.
 * @left: Horizontal start position.
 * @top: Vertical start position.
 *
 * Vertical start line:
 *  This field makes sense only when the resizer obtains its input
 *  from the preview engine/CCDC
 *
 * Horizontal start pixel:
 *  Pixels are coded on 16 bits for YUV and 8 bits for color separate data.
 *  When the resizer gets its input from SDRAM, this field must be set
 *  to <= 15 for YUV 16-bit data and <= 31 for 8-bit color separate data
 */
static inline void ispresizer_set_start(struct device *dev, u32 left, u32 top)
{
	u32 rgval;

	rgval = (left << ISPRSZ_IN_START_HORZ_ST_SHIFT)
		& ISPRSZ_IN_START_HORZ_ST_MASK;
	rgval |= (top << ISPRSZ_IN_START_VERT_ST_SHIFT)
		 & ISPRSZ_IN_START_VERT_ST_MASK;

	isp_reg_writel(dev, rgval, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_IN_START);

	dev_dbg(dev, "%s: Start: H=%d V=%d\n", __func__, left, top);
}

/**
 * ispresizer_set_input_size - Setup the input size
 * @dev: Device context.
 * @width: The range is 0 to 4095 pixels
 * @height: The range is 0 to 4095 lines
 */
static inline void ispresizer_set_input_size(struct device *dev, u32 width,
					     u32 height)
{
	u32 rgval;

	rgval = (width << ISPRSZ_IN_SIZE_HORZ_SHIFT)
		& ISPRSZ_IN_SIZE_HORZ_MASK;
	rgval |= (height << ISPRSZ_IN_SIZE_VERT_SHIFT)
		 & ISPRSZ_IN_SIZE_VERT_MASK;

	isp_reg_writel(dev, rgval, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_IN_SIZE);

	dev_dbg(dev, "%s: In size: W=%d H=%d\n", __func__, width, height);
}

/**
 * ispresizer_set_output_size - Setup the output height and width
 * @dev: Device context.
 * @width: Output width.
 * @height: Output height.
 *
 * Width :
 *  The value must be EVEN.
 *
 * Height:
 *  The number of bytes written to SDRAM must be
 *  a multiple of 16-bytes if the vertical resizing factor
 *  is greater than 1x (upsizing)
 */
static inline void ispresizer_set_output_size(struct device *dev, u32 width,
					      u32 height)
{
	u32 rgval;

	rgval  = (width << ISPRSZ_OUT_SIZE_HORZ_SHIFT)
		 & ISPRSZ_OUT_SIZE_HORZ_MASK;
	rgval |= (height << ISPRSZ_OUT_SIZE_VERT_SHIFT)
		 & ISPRSZ_OUT_SIZE_VERT_MASK;
	isp_reg_writel(dev, rgval, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_OUT_SIZE);

	dev_dbg(dev, "%s: Out size: W=%d H=%d\n", __func__, width, height);
}

/**
 * ispresizer_set_source - Input source select
 * @isp_res: Device context.
 * @source: Input source type
 *
 * If this field is set to RSZ_OTFLY_YUV, the resizer input is fed from
 * Preview/CCDC engine, otherwise from memory.
 */
static inline void ispresizer_set_source(struct isp_res_device *isp_res,
					 enum resizer_input source)
{
	struct device *dev = to_device(isp_res);

	if (source != RSZ_OTFLY_YUV)
		isp_reg_or(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
			   ISPRSZ_CNT_INPSRC);
	else
		isp_reg_and(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
			    ~ISPRSZ_CNT_INPSRC);

	dev_dbg(dev, "%s: In source: %u\n", __func__, source);
}

/**
 * ispresizer_set_intype - Input type select
 * @isp_res: Device context.
 * @type: Pixel format type.
 */
static inline void ispresizer_set_intype(struct isp_res_device *isp_res,
					 enum resizer_input type)
{
	struct device *dev = to_device(isp_res);

	if (type == RSZ_MEM_COL8)
		isp_reg_or(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
			   ISPRSZ_CNT_INPTYP);
	else
		isp_reg_and(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
			    ~ISPRSZ_CNT_INPTYP);

	dev_dbg(dev, "%s: In type: %u\n", __func__, type);
}

/**
 * ispresizer_set_in_offset - Configures the read address line offset.
 * @offset: Line Offset for the input image.
 *
 * Returns 0 if successful, or -EINVAL if offset is not 32 bits aligned.
 **/
static inline int ispresizer_set_in_offset(struct isp_res_device *isp_res,
					   u32 offset)
{
	struct device *dev = to_device(isp_res);

	if (offset % 32)
		return -EINVAL;
	isp_reg_writel(dev, offset << ISPRSZ_SDR_INOFF_OFFSET_SHIFT,
		       OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_INOFF);

	dev_dbg(dev, "%s: In offset: 0x%04X\n", __func__, offset);

	return 0;
}

/**
 * ispresizer_set_out_offset - Configures the write address line offset.
 * @offset: Line offset for the preview output.
 *
 * Returns 0 if successful, or -EINVAL if address is not 32 bits aligned.
 **/
int ispresizer_set_out_offset(struct isp_res_device *isp_res, u32 offset)
{
	struct device *dev = to_device(isp_res);

	if (offset % 32)
		return -EINVAL;
	isp_reg_writel(dev, offset << ISPRSZ_SDR_OUTOFF_OFFSET_SHIFT,
		       OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_OUTOFF);

	dev_dbg(dev, "%s: Out offset: 0x%04X\n", __func__, offset);

	return 0;
}

/* Structure for saving/restoring resizer module registers */
static struct isp_reg isprsz_reg_list[] = {
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_OUT_SIZE, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_IN_START, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_IN_SIZE, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_INADD, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_INOFF, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_OUTADD, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_OUTOFF, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT10, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT32, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT54, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT76, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT98, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT1110, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT1312, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT1514, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT1716, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT1918, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT2120, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT2322, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT2524, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT2726, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT2928, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_HFILT3130, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT10, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT32, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT54, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT76, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT98, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT1110, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT1312, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT1514, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT1716, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT1918, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT2120, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT2322, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT2524, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT2726, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT2928, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_VFILT3130, 0x0000},
	{OMAP3_ISP_IOMEM_RESZ, ISPRSZ_YENH, 0x0000},
	{0, ISP_TOK_TERM, 0x0000}
};

/**
 * ispresizer_applycrop - Apply crop to input image.
 **/
void ispresizer_applycrop(struct isp_res_device *isp_res)
{
	struct isp_device *isp = to_isp_device(isp_res);
	struct isp_node *pipe = &isp->pipeline.rsz;
	int bpp = ISP_BYTES_PER_PIXEL;

	if (!isp_res->applycrop)
		return;

	ispresizer_set_ratio(isp->dev, isp_res->h_resz, isp_res->v_resz);
	ispresizer_set_coeffs(isp->dev, NULL, isp_res->h_resz, isp_res->v_resz);

	/* Switch filter, releated to up/down scale */
	if (ispresizer_is_upscale(pipe))
		ispresizer_enable_cbilin(isp_res, 1);
	else
		ispresizer_enable_cbilin(isp_res, 0);

	/* Set input and output size */
	ispresizer_set_input_size(isp->dev, isp_res->phy_rect.width,
				  isp_res->phy_rect.height);
	ispresizer_set_output_size(isp->dev, pipe->out.image.width,
				   pipe->out.image.height);

	/* Set input address and line offset address */
	if (pipe->in.path != RSZ_OTFLY_YUV) {
		/* Set the input address, plus calculated crop offset */
		ispresizer_set_inaddr(isp_res, isp_res->in_buff_addr, pipe);
		/* Set the input line offset/length */
		ispresizer_set_in_offset(isp_res, pipe->in.image.bytesperline);
	} else {
		/* Set the input address.*/
		ispresizer_set_inaddr(isp_res, 0, NULL);
		/* Set the starting pixel offset */
		ispresizer_set_start(isp->dev, isp_res->phy_rect.left * bpp,
				     isp_res->phy_rect.top);
		ispresizer_set_in_offset(isp_res, 0);
	}

	/* Set output line offset */
	ispresizer_set_out_offset(isp_res, pipe->out.image.bytesperline);

	isp_res->applycrop = 0;
}

/**
 * ispresizer_config_shadow_registers - Configure shadow registers.
 **/
void ispresizer_config_shadow_registers(struct isp_res_device *isp_res)
{
	ispresizer_applycrop(isp_res);

	return;
}

int ispresizer_config_crop(struct isp_res_device *isp_res,
			   struct isp_node *pipe, struct v4l2_crop *a)
{
	struct isp_device *isp = to_isp_device(isp_res);
	int rval = 0;

	pipe->in.crop = a->c;

	rval = ispresizer_try_ratio(to_device(isp_res), pipe,
				    &isp_res->phy_rect, &isp_res->h_resz,
				    &isp_res->v_resz);

	a->c = pipe->in.crop;

	isp_res->applycrop = 1;

	if (isp->running == ISP_STOPPED)
		ispresizer_applycrop(isp_res);

	return rval;
}

/**
 * ispresizer_request - Reserves the Resizer module.
 *
 * Allows only one user at a time.
 *
 * Returns 0 if successful, or -EBUSY if resizer module was already requested.
 **/
int ispresizer_request(struct isp_res_device *isp_res)
{
	struct device *dev = to_device(isp_res);

	mutex_lock(&isp_res->ispres_mutex);
	if (!isp_res->res_inuse) {
		isp_res->res_inuse = 1;
		mutex_unlock(&isp_res->ispres_mutex);
		isp_reg_writel(dev,
			       isp_reg_readl(dev,
					     OMAP3_ISP_IOMEM_MAIN, ISP_CTRL) |
			       ISPCTRL_SBL_WR0_RAM_EN |
			       ISPCTRL_RSZ_CLK_EN,
			       OMAP3_ISP_IOMEM_MAIN, ISP_CTRL);
		return 0;
	} else {
		mutex_unlock(&isp_res->ispres_mutex);
		dev_err(dev, "resizer: Module Busy\n");
		return -EBUSY;
	}
}

/**
 * ispresizer_free - Makes Resizer module free.
 *
 * Returns 0 if successful, or -EINVAL if resizer module was already freed.
 **/
int ispresizer_free(struct isp_res_device *isp_res)
{
	struct device *dev = to_device(isp_res);

	mutex_lock(&isp_res->ispres_mutex);
	if (isp_res->res_inuse) {
		isp_res->res_inuse = 0;
		mutex_unlock(&isp_res->ispres_mutex);
		isp_reg_and(dev, OMAP3_ISP_IOMEM_MAIN, ISP_CTRL,
			    ~(ISPCTRL_RSZ_CLK_EN | ISPCTRL_SBL_WR0_RAM_EN));
		return 0;
	} else {
		mutex_unlock(&isp_res->ispres_mutex);
		dev_err(dev, "Resizer Module already freed\n");
		return -EINVAL;
	}
}

/**
 * ispresizer_try_size - Validates input and output images size.
 * @pipe: Resizer data path parameters.
 *
 * Calculates the horizontal and vertical resize ratio, number of pixels to
 * be cropped in the resizer module and checks the validity of various
 * parameters. Formula used for calculation is:-
 *
 * 8-phase 4-tap mode :-
 * inputwidth = (32 * sph + (ow - 1) * hrsz + 16) >> 8 + 7
 * inputheight = (32 * spv + (oh - 1) * vrsz + 16) >> 8 + 4
 * endpahse for width = ((32 * sph + (ow - 1) * hrsz + 16) >> 5) % 8
 * endphase for height = ((32 * sph + (oh - 1) * hrsz + 16) >> 5) % 8
 *
 * 4-phase 7-tap mode :-
 * inputwidth = (64 * sph + (ow - 1) * hrsz + 32) >> 8 + 7
 * inputheight = (64 * spv + (oh - 1) * vrsz + 32) >> 8 + 7
 * endpahse for width = ((64 * sph + (ow - 1) * hrsz + 32) >> 6) % 4
 * endphase for height = ((64 * sph + (oh - 1) * hrsz + 32) >> 6) % 4
 *
 */
int ispresizer_try_pipeline(struct isp_res_device *isp_res,
			    struct isp_node *pipe)
{
	if (pipe->in.image.pixelformat == V4L2_PIX_FMT_YUYV ||
	    pipe->in.image.pixelformat == V4L2_PIX_FMT_UYVY) {
		pipe->in.image.colorspace = V4L2_COLORSPACE_JPEG;
	} else {
		pipe->in.image.colorspace = V4L2_COLORSPACE_SRGB;
	}

	if (ispresizer_try_fmt(pipe, pipe->in.path != RSZ_OTFLY_YUV))
		return -EINVAL;

	if (ispresizer_try_ratio(to_device(isp_res), pipe, &isp_res->phy_rect,
				 &isp_res->h_resz, &isp_res->v_resz))
		return -EINVAL;

	if (pipe->in.path == RSZ_MEM_COL8) {
		pipe->in.image.bytesperline = ALIGN(pipe->in.image.width,
						   PHY_ADDRESS_ALIGN);
		pipe->out.image.bytesperline = ALIGN(pipe->out.image.width,
						    PHY_ADDRESS_ALIGN);
	} else {
		pipe->in.image.bytesperline = ALIGN(pipe->in.image.width * 2,
						   PHY_ADDRESS_ALIGN);
		pipe->out.image.bytesperline = ALIGN(pipe->out.image.width * 2,
						    PHY_ADDRESS_ALIGN);
	}
	pipe->in.image.field = V4L2_FIELD_NONE;
	pipe->in.image.sizeimage = pipe->in.image.bytesperline *
				  pipe->in.image.height;

	pipe->out.image.field = pipe->in.image.field;
	pipe->out.image.colorspace = pipe->in.image.colorspace;
	pipe->out.image.pixelformat = pipe->in.image.pixelformat;
	pipe->out.image.sizeimage = pipe->out.image.bytesperline *
				   pipe->out.image.height;

	return 0;
}

/**
 * ispresizer_config_size - Configures input and output image size.
 * @pipe: Resizer data path parameters.
 *
 * Configures the appropriate values stored in the isp_res structure in the
 * resizer registers.
 *
 * Returns 0 if successful, or -EINVAL if passed values haven't been verified
 * with ispresizer_try_size() previously.
 **/
int ispresizer_s_pipeline(struct isp_res_device *isp_res,
			  struct isp_node *pipe)
{
	struct device *dev = to_device(isp_res);
	int bpp = ISP_BYTES_PER_PIXEL;

	ispresizer_set_source(isp_res, pipe->in.path);
	ispresizer_set_intype(isp_res, pipe->in.path);
	ispresizer_set_start_phase(dev, NULL);
	ispresizer_set_luma_enhance(dev, NULL);

	if (pipe->in.image.pixelformat == V4L2_PIX_FMT_YUYV)
		ispresizer_config_ycpos(isp_res, 1);
	else
		ispresizer_config_ycpos(isp_res, 0);

	ispresizer_try_pipeline(isp_res, pipe);

	ispresizer_set_ratio(dev, isp_res->h_resz, isp_res->v_resz);
	ispresizer_set_coeffs(dev, NULL, isp_res->h_resz, isp_res->v_resz);

	/* Switch filter, releated to up/down scale */
	if (ispresizer_is_upscale(pipe))
		ispresizer_enable_cbilin(isp_res, 1);
	else
		ispresizer_enable_cbilin(isp_res, 0);

	/* Set input and output size */
	ispresizer_set_input_size(dev, isp_res->phy_rect.width,
				  isp_res->phy_rect.height);
	ispresizer_set_output_size(dev, pipe->out.image.width,
				   pipe->out.image.height);

	/* Set input address and line offset address */
	if (pipe->in.path != RSZ_OTFLY_YUV) {
		/* Set the input address, plus calculated crop offset */
		ispresizer_set_inaddr(isp_res, isp_res->in_buff_addr, pipe);
		/* Set the input line offset/length */
		ispresizer_set_in_offset(isp_res, pipe->in.image.bytesperline);
	} else {
		/* Set the input address.*/
		ispresizer_set_inaddr(isp_res, 0, NULL);
		/* Set the starting pixel offset */
		ispresizer_set_start(dev, isp_res->phy_rect.left * bpp,
				     isp_res->phy_rect.top);
		ispresizer_set_in_offset(isp_res, 0);
	}

	/* Set output line offset */
	ispresizer_set_out_offset(isp_res, pipe->out.image.bytesperline);

	return 0;
}

/**
 * ispresizer_enable - Enables the resizer module.
 * @enable: 1 - Enable, 0 - Disable
 *
 * Client should configure all the sub modules in resizer before this.
 **/
void ispresizer_enable(struct isp_res_device *isp_res, int enable)
{
	struct device *dev = to_device(isp_res);
	int val;

	if (enable) {
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
		struct isp_device *isp = to_isp_device(isp_res);
		if (isp->dfs_resz)
			ispresz_dfs_dump(isp);
#endif
		val = (isp_reg_readl(dev, OMAP3_ISP_IOMEM_RESZ,
			ISPRSZ_PCR) & ISPRSZ_PCR_ONESHOT) |
			ISPRSZ_PCR_ENABLE;
	} else {
		val = isp_reg_readl(dev,
				    OMAP3_ISP_IOMEM_RESZ, ISPRSZ_PCR) &
			~ISPRSZ_PCR_ENABLE;
	}
	isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_PCR);
}

/**
 * ispresizer_busy - Checks if ISP resizer is busy.
 *
 * Returns busy field from ISPRSZ_PCR register.
 **/
int ispresizer_busy(struct isp_res_device *isp_res)
{
	struct device *dev = to_device(isp_res);

	return isp_reg_readl(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_PCR) &
		ISPRSZ_PCR_BUSY;
}

/**
 * ispresizer_is_enabled - Checks if ISP resizer is enable.
 *
 * Returns busy field from ISPRSZ_PCR register.
 **/
int ispresizer_is_enabled(struct isp_res_device *isp_res)
{
	struct device *dev = to_device(isp_res);

	return isp_reg_readl(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_PCR) &
		ISPRSZ_PCR_ENABLE;
}
/**
 * ispresizer_set_start_phase - Sets the horizontal and vertical start phase.
 * @phase: horizontal and vertical start phase (0 - 7).
 *
 * This API just updates the isp_res struct. Actual register write happens in
 * ispresizer_config_size.
 **/
void ispresizer_set_start_phase(struct device *dev, struct isprsz_phase *phase)
{
	/* clear bits */
	isp_reg_and(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
		    ~(ISPRSZ_CNT_HSTPH_MASK | ISPRSZ_CNT_VSTPH_MASK));

	if (phase != NULL) {
		isp_reg_or(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
			   ((phase->h_phase << ISPRSZ_CNT_HSTPH_SHIFT)
			     & ISPRSZ_CNT_HSTPH_MASK) |
			   ((phase->v_phase << ISPRSZ_CNT_VSTPH_SHIFT)
			     & ISPRSZ_CNT_VSTPH_MASK));
	} else {
		isp_reg_or(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
			   ((DEFAULTSTPHASE << ISPRSZ_CNT_HSTPH_SHIFT)
			     & ISPRSZ_CNT_HSTPH_MASK) |
			   ((DEFAULTSTPHASE  << ISPRSZ_CNT_VSTPH_SHIFT)
			     & ISPRSZ_CNT_VSTPH_MASK));
	}
}

/**
 * ispresizer_config_ycpos - Specifies if output should be in YC or CY format.
 * @yc: 0 - YC format, 1 - CY format
 **/
void ispresizer_config_ycpos(struct isp_res_device *isp_res, u8 yc)
{
	struct device *dev = to_device(isp_res);

	isp_reg_and_or(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
		       ~ISPRSZ_CNT_YCPOS, (yc ? ISPRSZ_CNT_YCPOS : 0));
}

/**
 * Sets the chrominance algorithm
 * @cbilin: 0 - chrominance uses same processing as luminance,
 *          1 - bilinear interpolation processing
 **/
void ispresizer_enable_cbilin(struct isp_res_device *isp_res, u8 enable)
{
	struct device *dev = to_device(isp_res);

	isp_reg_and_or(dev, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_CNT,
		       ~ISPRSZ_CNT_CBILIN, (enable ? ISPRSZ_CNT_CBILIN : 0));
}

/**
 * ispresizer_set_luma_enhance - Set luminance enhancer parameters.
 * @yenh: Pointer to structure containing desired values or NULL
 * to user default values for core, slope, gain and algo parameters.
 **/
void ispresizer_set_luma_enhance(struct device *dev, struct isprsz_yenh *yenh)
{
	if (yenh != NULL) {
		isp_reg_writel(dev, (yenh->algo << ISPRSZ_YENH_ALGO_SHIFT) |
				    (yenh->gain << ISPRSZ_YENH_GAIN_SHIFT) |
				    (yenh->slope << ISPRSZ_YENH_SLOP_SHIFT) |
				    (yenh->coreoffset <<
				     ISPRSZ_YENH_CORE_SHIFT),
				    OMAP3_ISP_IOMEM_RESZ, ISPRSZ_YENH);
	} else {
		isp_reg_writel(dev, 0, OMAP3_ISP_IOMEM_RESZ, ISPRSZ_YENH);
	}
}

/**
 * ispresizer_set_inaddr - Sets the memory address of the input frame.
 * @addr: 32bit memory address aligned on 32byte boundary.
 * @offset: Starting offset.
 *
 * Returns 0 if successful, or -EINVAL if address is not 32 bits aligned.
 **/
int ispresizer_set_inaddr(struct isp_res_device *isp_res, u32 addr,
			  struct isp_node *pipe)
{
	struct device *dev = to_device(isp_res);
	u32 in_buf_plus_offs = 0;

	if (addr % 32)
		return -EINVAL;

	isp_res->in_buff_addr = addr;
	if (pipe != NULL) {
		dev_dbg(dev, "%s: In crop top: %d[%d] left: %d[%d]\n", __func__,
			isp_res->phy_rect.top, pipe->in.crop.top,
			isp_res->phy_rect.left, pipe->in.crop.left);
		/* Calculate additional part to prepare crop offsets */
		in_buf_plus_offs = ((isp_res->phy_rect.top *
				     (pipe->in.image.bytesperline / 2) +
				     (isp_res->phy_rect.left & ~0xf)) *
				    ISP_BYTES_PER_PIXEL);

		/* Set the fractional part of the crop */
		ispresizer_set_start(dev, isp_res->phy_rect.left & 0xf, 0);

		dev_dbg(dev, "%s: In address offs: 0x%08X\n", __func__,
			in_buf_plus_offs);
	}

	isp_reg_writel(dev, isp_res->in_buff_addr + in_buf_plus_offs,
		       OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_INADD);

	dev_dbg(dev, "%s: In address base: 0x%08X\n", __func__, addr);

	return 0;
}

/**
 * Configures the memory address to which the output frame is written.
 * @addr: 32bit memory address aligned on 32byte boundary.
 **/
int ispresizer_set_outaddr(struct isp_res_device *isp_res, u32 addr)
{
	struct device *dev = to_device(isp_res);

	if (addr % 32)
		return -EINVAL;
	isp_reg_writel(dev, addr << ISPRSZ_SDR_OUTADD_ADDR_SHIFT,
		       OMAP3_ISP_IOMEM_RESZ, ISPRSZ_SDR_OUTADD);

	dev_dbg(dev, "%s: Out address: 0x%08X\n", __func__, addr);

	return 0;
}

/**
 * ispresizer_save_context - Saves the values of the resizer module registers.
 **/
void ispresizer_save_context(struct device *dev)
{
	isp_save_context(dev, isprsz_reg_list);
}

/**
 * ispresizer_restore_context - Restores resizer module register values.
 **/
void ispresizer_restore_context(struct device *dev)
{
	isp_restore_context(dev, isprsz_reg_list);
}

/**
 * isp_resizer_init - Module Initialisation.
 *
 * Always returns 0.
 **/
int __init isp_resizer_init(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_res_device *isp_res = &isp->isp_res;

	mutex_init(&isp_res->ispres_mutex);
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	ispresz_dfs_setup(isp);
#endif
	return 0;
}

/**
 * isp_resizer_cleanup - Module Cleanup.
 **/
void isp_resizer_cleanup(struct device *dev)
{
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	struct isp_device *isp = dev_get_drvdata(dev);

	ispresz_dfs_shutdown(isp);
#endif
}
