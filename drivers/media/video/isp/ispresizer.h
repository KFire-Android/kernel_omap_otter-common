/*
 * ispresizer.h
 *
 * Driver header file for Resizer module in TI's OMAP3 Camera ISP
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
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

#ifndef OMAP_ISP_RESIZER_H
#define OMAP_ISP_RESIZER_H

#define COEFFS_COUNT	32

enum resizer_input {
	RSZ_OTFLY_YUV,
	RSZ_MEM_YUV,
	RSZ_MEM_COL8
};

/**
 * struct isprsz_coef - Structure for resizer filter coeffcients.
 * @h_filter_coef_4tap: Horizontal filter coefficients for 8-phase/4-tap
 *			mode (.5x-4x)
 * @v_filter_coef_4tap: Vertical filter coefficients for 8-phase/4-tap
 *			mode (.5x-4x)
 * @h_filter_coef_7tap: Horizontal filter coefficients for 4-phase/7-tap
 *			mode (.25x-.5x)
 * @v_filter_coef_7tap: Vertical filter coefficients for 4-phase/7-tap
 *			mode (.25x-.5x)
 */
struct isprsz_coef {
	u16 h_filter_coef_4tap[COEFFS_COUNT];
	u16 v_filter_coef_4tap[COEFFS_COUNT];
	u16 h_filter_coef_7tap[COEFFS_COUNT];
	u16 v_filter_coef_7tap[COEFFS_COUNT];
};

/**
 * struct isprsz_phase - Structure horizontal and vertical start phase.
 * @h_phase: horizontal start phase.
 * @v_phase: vertical start phase.
 */
struct isprsz_phase {
	u8 h_phase;
	u8 v_phase;
};

/**
 * struct isprsz_yenh - Structure for resizer luminance enhancer parameters.
 * @algo: Algorithm select.
 * @gain: Maximum gain.
 * @slope: Slope.
 * @coreoffset: Coring offset.
 */
struct isprsz_yenh {
	u8 algo;
	u8 gain;
	u8 slope;
	u8 coreoffset;
};

/**
 * struct isp_res_device - Structure for the resizer module to store its
 * 			   information.
 * @res_inuse: Indicates if resizer module has been reserved. 1 - Reserved,
 *             0 - Freed.
 * @h_startphase: Horizontal starting phase.
 * @v_startphase: Vertical starting phase.
 * @h_resz: Horizontal resizing value.
 * @v_resz: Vertical resizing value.
 * @outputwidth: Output Image Width in pixels.
 * @outputheight: Output Image Height in pixels.
 * @inputwidth: Input Image Width in pixels.
 * @inputheight: Input Image Height in pixels.
 * @algo: Algorithm select. 0 - Disable, 1 - [-1 2 -1]/2 high-pass filter,
 *        2 - [-1 -2 6 -2 -1]/4 high-pass filter.
 * @ipht_crop: Vertical start line for cropping.
 * @ipwd_crop: Horizontal start pixel for cropping.
 * @cropwidth: Crop Width.
 * @cropheight: Crop Height.
 * @resinput: Resizer input.
 * @coeflist: Register configuration for Resizer.
 * @ispres_mutex: Mutex for isp resizer.
 */
struct isp_res_device {
	u8 res_inuse;
	u16 h_resz;
	u16 v_resz;
	struct v4l2_rect phy_rect;
	struct mutex ispres_mutex; /* For checking/modifying res_inuse */
	int applycrop;
	u32 in_buff_addr;
};

bool ispresizer_is_upscale(struct isp_node *pipe);

int ispresizer_config_crop(struct isp_res_device *isp_res,
			   struct isp_node *pipe, struct v4l2_crop *a);
void ispresizer_config_shadow_registers(struct isp_res_device *isp_res);

int ispresizer_request(struct isp_res_device *isp_res);

int ispresizer_free(struct isp_res_device *isp_res);

void ispresizer_enable_cbilin(struct isp_res_device *isp_res, u8 enable);

void ispresizer_config_ycpos(struct isp_res_device *isp_res, u8 yc);

/* Sets the horizontal and vertical start phase */
void ispresizer_set_start_phase(struct device *dev, struct isprsz_phase *phase);

/* Setup coefficents for polyphase filter*/
void ispresizer_set_coeffs(struct device *dev, struct isprsz_coef *coeffs,
			   unsigned h_ratio, unsigned v_ratio);

/* Configures luminance enhancer parameters */
void ispresizer_set_luma_enhance(struct device *dev, struct isprsz_yenh *yenh);

int ispresizer_try_pipeline(struct isp_res_device *isp_res,
			    struct isp_node *pipe);

int ispresizer_s_pipeline(struct isp_res_device *isp_res,
			  struct isp_node *pipe);

int ispresizer_set_inaddr(struct isp_res_device *isp_res, u32 addr,
			  struct isp_node *pipe);

int ispresizer_set_outaddr(struct isp_res_device *isp_res, u32 addr);

int ispresizer_set_out_offset(struct isp_res_device *isp_res, u32 offset);

void ispresizer_enable(struct isp_res_device *isp_res, int enable);

int ispresizer_busy(struct isp_res_device *isp_res);

int ispresizer_is_enabled(struct isp_res_device *isp_res);

void ispresizer_save_context(struct device *dev);

void ispresizer_restore_context(struct device *dev);

#endif		/* OMAP_ISP_RESIZER_H */
