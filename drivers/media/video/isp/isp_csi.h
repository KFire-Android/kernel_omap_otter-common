/*
 * isp_csi.h
 *
 * Copyright (C) 2009 Texas Instruments.
 *
 * Contributors:
 * 	Sergio Aguirre <saaguirre@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef OMAP_ISP_CSI_H
#define OMAP_ISP_CSI_H

#include <linux/videodev2.h>

struct isp_csi_interface_cfg {
	unsigned use_mem_read:1;
	unsigned crc:1;
	unsigned mode:1;
	unsigned edge:1;
	unsigned signalling:1;
	unsigned strobe_clock_inv:1;
	unsigned vs_edge:1;
	unsigned channel:3;
	unsigned vpclk:2;
	unsigned int data_start;
	unsigned int data_size;
	u32 format;
	struct v4l2_rect mem_src_rect;
};

/**
 * struct isp_csi_vp_cfg - ISP CSI1/CCP2 videoport config
 * @no_ocp: Exclusively use VP (no OCP output)
 * @divider: VP frequency divider, OCPCLK/(divider + 1)
 * @write_polarity: 0 - Write data to VP on PCLK falling edge.
 * 		    1 - Write data to VP on PCLK rising edge.
 */
struct isp_csi_vp_cfg {
	bool no_ocp;
	u8 divider;
	u8 write_polarity;
};

/**
 * struct isp_csi_device - ISP CSI1/CCP2 device structure
 * @if_device: Interface enable.
 * @vp_cfg: Structure with current videoport configuration.
 */
struct isp_csi_device {
	bool if_enabled;
	bool lcm_enabled;
	struct isp_csi_vp_cfg vp_cfg;
	u32 lcm_src_addr;
	u32 lcm_src_ofst;
	struct v4l2_rect lcm_src_rect;
	u32 lcm_src_fmt;
	u32 lcm_dst_fmt;
};

void isp_csi_if_enable(struct isp_csi_device *isp_csi, u8 enable);
int isp_csi_set_vp_freq(struct isp_csi_device *isp_csi, u16 vp_freq);
int isp_csi_configure_interface(struct isp_csi_device *isp_csi,
				struct isp_csi_interface_cfg *config);
int isp_csi_lcm_s_src_ofst(struct isp_csi_device *isp_csi, u32 offset);
int isp_csi_lcm_s_src_addr(struct isp_csi_device *isp_csi, u32 addr);
int isp_csi_lcm_validate_src_region(struct isp_csi_device *isp_csi,
				    struct v4l2_rect rect);
int isp_csi_lcm_readport_enable(struct isp_csi_device *isp_csi, bool enable);

#endif	/* OMAP_ISP_CSI_H */

