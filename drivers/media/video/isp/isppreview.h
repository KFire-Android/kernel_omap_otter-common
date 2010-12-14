/*
 * isppreview.h
 *
 * Driver header file for Preview module in TI's OMAP3 Camera ISP
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Contributors:
 *	Senthilvadivu Guruswamy <svadivu@ti.com>
 *	Pallavi Kulkarni <p-kulkarni@ti.com>
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

#ifndef OMAP_ISP_PREVIEW_H
#define OMAP_ISP_PREVIEW_H

#include <mach/isp_user.h>

/**
 * struct isp_prev_device - Structure for storing ISP Preview module information
 * @prevout_w: Preview output width.
 * @prevout_h: Preview output height.
 * @previn_w: Preview input width.
 * @previn_h: Preview input height.
 * @prev_inpfmt: Preview input format.
 * @prev_outfmt: Preview output format.
 * @hmed_en: Horizontal median filter enable.
 * @nf_en: Noise filter enable.
 * @dcor_en: Defect correction enable.
 * @cfa_en: Color Filter Array (CFA) interpolation enable.
 * @csup_en: Chrominance suppression enable.
 * @yenh_en: Luma enhancement enable.
 * @fmtavg: Number of horizontal pixels to average in input formatter. The
 *          input width should be a multiple of this number.
 * @brightness: Brightness in preview module.
 * @contrast: Contrast in preview module.
 * @color: Color effect in preview module.
 * @cfafmt: Color Filter Array (CFA) Format.
 * @wbal_update: Update digital and colour gains in Previewer
 *
 * This structure is used to store the OMAP ISP Preview module Information.
 */
struct isp_prev_device {
	int update_color_matrix;
	u8 update_rgb_blending;
	u8 update_rgb_to_ycbcr;
	u8 hmed_en;
	u8 nf_en;
	u8 dcor_en;
	u8 cfa_en;
	u8 csup_en;
	u8 yenh_en;
	u8 rg_update;
	u8 gg_update;
	u8 bg_update;
	u8 cfa_update;
	u8 nf_update;
	u8 wbal_update;
	u8 fmtavg;
	u8 brightness;
	u8 contrast;
	enum v4l2_colorfx color;
	enum cfa_fmt cfafmt;
	struct ispprev_nf prev_nf_t;
	struct prev_params params;
	int shadow_update;
	spinlock_t lock;
	int fmt_avg;
};

void isppreview_config_shadow_registers(struct isp_prev_device *isp_prev);

int isppreview_request(struct isp_prev_device *isp_prev);

void isppreview_free(struct isp_prev_device *isp_prev);

int isppreview_config_datapath(struct isp_prev_device *isp_prev,
			       struct isp_node *pipe);

void isppreview_config_ycpos(struct isp_prev_device *isp_prev,
			     enum preview_ycpos_mode mode);

void isppreview_config_averager(struct isp_prev_device *isp_prev, u8 average);

void isppreview_enable_invalaw(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_enable_drkframe(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_enable_shadcomp(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_config_drkf_shadcomp(struct isp_prev_device *isp_prev,
				     u8 scomp_shtval);

void isppreview_enable_gammabypass(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_enable_hmed(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_config_hmed(struct isp_prev_device *isp_prev,
			    struct ispprev_hmed);

void isppreview_enable_noisefilter(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_config_noisefilter(struct isp_prev_device *isp_prev,
				   struct ispprev_nf prev_nf);

void isppreview_enable_dcor(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_config_dcor(struct isp_prev_device *isp_prev,
			    struct ispprev_dcor prev_dcor);


void isppreview_config_cfa(struct isp_prev_device *isp_prev,
			   struct ispprev_cfa);

void isppreview_config_gammacorrn(struct isp_prev_device *isp_prev,
				  struct ispprev_gtable);

void isppreview_config_chroma_suppression(struct isp_prev_device *isp_prev,
					  struct ispprev_csup csup);

void isppreview_enable_cfa(struct isp_prev_device *isp_prev, u8 enable);

void isppreview_config_luma_enhancement(struct isp_prev_device *isp_prev,
					u32 *ytable);

void isppreview_enable_luma_enhancement(struct isp_prev_device *isp_prev,
					u8 enable);

void isppreview_enable_chroma_suppression(struct isp_prev_device *isp_prev,
					  u8 enable);

void isppreview_config_whitebalance(struct isp_prev_device *isp_prev,
				    struct ispprev_wbal);

void isppreview_config_blkadj(struct isp_prev_device *isp_prev,
			      struct ispprev_blkadj);

void isppreview_config_rgb_blending(struct isp_prev_device *isp_prev,
				    struct ispprev_rgbtorgb);

void isppreview_config_rgb_to_ycbcr(struct isp_prev_device *isp_prev,
				    struct ispprev_csc);

void isppreview_update_contrast(struct isp_prev_device *isp_prev, u8 *contrast);

void isppreview_query_contrast(struct isp_prev_device *isp_prev, u8 *contrast);

void isppreview_config_contrast(struct isp_prev_device *isp_prev, u8 contrast);

void isppreview_get_contrast_range(u8 *min_contrast, u8 *max_contrast);

void isppreview_update_brightness(struct isp_prev_device *isp_prev,
				  u8 *brightness);

void isppreview_config_brightness(struct isp_prev_device *isp_prev,
				  u8 brightness);

void isppreview_get_brightness_range(u8 *min_brightness, u8 *max_brightness);

/* Set input width and height in previewer registers */
void isppreview_set_size(struct isp_prev_device *isp_prev, u32 input_w,
			 u32 input_h);

void isppreview_set_color(struct isp_prev_device *isp_prev, u8 *mode);

void isppreview_get_color(struct isp_prev_device *isp_prev, u8 *mode);

void isppreview_query_brightness(struct isp_prev_device *isp_prev,
				 u8 *brightness);

void isppreview_config_yc_range(struct isp_prev_device *isp_prev,
				struct ispprev_yclimit yclimit);

int isppreview_try_pipeline(struct isp_prev_device *isp_prev,
			    struct isp_node *pipe);

int isppreview_s_pipeline(struct isp_prev_device *isp_prev,
			  struct isp_node *pipe);

int isppreview_config_inlineoffset(struct isp_prev_device *isp_prev,
				   u32 offset);

int isppreview_set_inaddr(struct isp_prev_device *isp_prev, u32 addr);

int isppreview_config_outlineoffset(struct isp_prev_device *isp_prev,
				    u32 offset);

int isppreview_set_outaddr(struct isp_prev_device *isp_prev, u32 addr);

int isppreview_config_darklineoffset(struct isp_prev_device *isp_prev,
				     u32 offset);

int isppreview_set_darkaddr(struct isp_prev_device *isp_prev, u32 addr);

void isppreview_enable(struct isp_prev_device *isp_prev, int enable);

int isppreview_busy(struct isp_prev_device *isp_prev);

int isppreview_is_enabled(struct isp_prev_device *isp_prev);

void isppreview_save_context(struct device *dev);

void isppreview_restore_context(struct device *dev);

int isppreview_config(struct isp_prev_device *isp_prev, void *userspace_add);

int isppreview_config_features(struct isp_prev_device *isp_prev,
			       struct prev_params *config);

#endif/* OMAP_ISP_PREVIEW_H */
