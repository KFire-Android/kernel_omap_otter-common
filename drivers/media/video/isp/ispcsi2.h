/*
 * ispcsi2.h
 *
 * Copyright (C) 2009 Texas Instruments.
 *
 * Contributors:
 * 	Sergio Aguirre <saaguirre@ti.com>
 * 	Dominic Curran <dcurran@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef OMAP_ISP_CSI2_API_H
#define OMAP_ISP_CSI2_API_H
#include <linux/videodev2.h>

#define ISP_CSI2_LANE_OFF 	0
#define ISP_CSI2_LANE_ON 	1

enum isp_csi2_output {
	CSI2_MEM,
	CSI2_VP,
	CSI2_MEM_VP, /* UNTESTED! */
};

enum isp_csi2_irqevents {
	OCP_ERR_IRQ = 0x4000,
	SHORT_PACKET_IRQ = 0x2000,
	ECC_CORRECTION_IRQ = 0x1000,
	ECC_NO_CORRECTION_IRQ = 0x800,
	COMPLEXIO2_ERR_IRQ = 0x400,
	COMPLEXIO1_ERR_IRQ = 0x200,
	FIFO_OVF_IRQ = 0x100,
	CONTEXT7 = 0x80,
	CONTEXT6 = 0x40,
	CONTEXT5 = 0x20,
	CONTEXT4 = 0x10,
	CONTEXT3 = 0x8,
	CONTEXT2 = 0x4,
	CONTEXT1 = 0x2,
	CONTEXT0 = 0x1,
};

enum isp_csi2_ctx_irqevents {
	CTX_ECC_CORRECTION = 0x100,
	CTX_LINE_NUMBER = 0x80,
	CTX_FRAME_NUMBER = 0x40,
	CTX_CS = 0x20,
	CTX_LE = 0x8,
	CTX_LS = 0x4,
	CTX_FE = 0x2,
	CTX_FS = 0x1,
};

enum isp_csi2_power_cmds {
	ISP_CSI2_POWER_OFF,
	ISP_CSI2_POWER_ON,
	ISP_CSI2_POWER_ULPW,
};

enum isp_csi2_frame_mode {
	ISP_CSI2_FRAME_IMMEDIATE,
	ISP_CSI2_FRAME_AFTERFEC,
};

struct csi2_lanecfg {
	u8 pos;
	u8 pol;
	u8 state; 	/*Current state - 1-Used  0-Unused */
};

struct isp_csi2_lanes_cfg {
	struct csi2_lanecfg data[4];
	struct csi2_lanecfg clk;
};

struct isp_csi2_lanes_cfg_update {
	bool data[4];
	bool clk;
};

struct isp_csi2_phy_cfg {
	u8 ths_term;
	u8 ths_settle;
	u8 tclk_term;
	unsigned tclk_miss:1;
	u8 tclk_settle;
};

struct isp_csi2_phy_cfg_update {
	bool ths_term;
	bool ths_settle;
	bool tclk_term;
	bool tclk_miss;
	bool tclk_settle;
};

struct isp_csi2_ctx_cfg {
	u8 virtual_id;
	u8 frame_count;
	struct v4l2_pix_format format;
	u16 alpha;
	u16 data_offset;
	u32 ping_addr;
	u32 pong_addr;
	bool eof_enabled;
	bool eol_enabled;
	bool checksum_enabled;
	bool enabled;
};

struct isp_csi2_ctx_cfg_update {
	bool virtual_id;
	bool frame_count;
	bool format;
	bool alpha;
	bool data_offset;
	bool ping_addr;
	bool pong_addr;
	bool eof_enabled;
	bool eol_enabled;
	bool checksum_enabled;
	bool enabled;
};

struct isp_csi2_timings_cfg {
	bool force_rx_mode;
	bool stop_state_16x;
	bool stop_state_4x;
	u16 stop_state_counter;
};

struct isp_csi2_timings_cfg_update {
	bool force_rx_mode;
	bool stop_state_16x;
	bool stop_state_4x;
	bool stop_state_counter;
};

struct isp_csi2_ctrl_cfg {
	bool vp_clk_enable;
	bool vp_only_enable;
	u8 vp_out_ctrl;
	bool debug_enable;
	u8 burst_size;
	enum isp_csi2_frame_mode frame_mode;
	bool ecc_enable;
	bool secure_mode;
	bool if_enable;
};

struct isp_csi2_ctrl_cfg_update {
	bool vp_clk_enable;
	bool vp_only_enable;
	bool vp_out_ctrl;
	bool debug_enable;
	bool burst_size;
	bool frame_mode;
	bool ecc_enable;
	bool secure_mode;
	bool if_enable;
};

struct isp_csi2_cfg {
	struct isp_csi2_lanes_cfg lanes;
	struct isp_csi2_phy_cfg phy;
	struct isp_csi2_ctx_cfg contexts[8];
	struct isp_csi2_timings_cfg timings[2];
	struct isp_csi2_ctrl_cfg ctrl;
};

struct isp_csi2_cfg_update {
	struct isp_csi2_lanes_cfg_update lanes;
	struct isp_csi2_phy_cfg_update phy;
	struct isp_csi2_ctx_cfg_update contexts[8];
	struct isp_csi2_timings_cfg_update timings[2];
	struct isp_csi2_ctrl_cfg_update ctrl;
};

struct isp_csi2_device {
	struct device *dev;
	struct isp_csi2_cfg current_cfg;
	struct isp_csi2_cfg_update current_cfg_update;
	bool update_complexio_cfg1;
	bool update_phy_cfg0;
	bool update_phy_cfg1;
	bool update_ctx_ctrl1[8];
	bool update_ctx_ctrl2[8];
	bool update_ctx_ctrl3[8];
	bool update_timing;
	bool update_ctrl;
	bool uses_videoport;
	bool force_mem_out;
};

int isp_csi2_complexio_lanes_config(struct isp_csi2_device *isp_csi2,
				    struct isp_csi2_lanes_cfg *reqcfg);
int isp_csi2_complexio_lanes_update(struct isp_csi2_device *isp_csi2,
				    bool force_update);
int isp_csi2_complexio_lanes_count(struct isp_csi2_device *isp_csi2, int cnt);
int isp_csi2_complexio_lanes_get(struct isp_csi2_device *isp_csi2);
int isp_csi2_complexio_power_autoswitch(struct isp_csi2_device *isp_csi2,
					bool enable);
int isp_csi2_complexio_power(struct isp_csi2_device *isp_csi2,
			     enum isp_csi2_power_cmds power_cmd);
int isp_csi2_ctrl_config_frame_mode(struct isp_csi2_device *isp_csi2,
				    enum isp_csi2_frame_mode frame_mode);
int isp_csi2_ctrl_config_vp_clk_enable(struct isp_csi2_device *isp_csi2,
				       bool vp_clk_enable);
int isp_csi2_ctrl_config_vp_only_enable(struct isp_csi2_device *isp_csi2,
					bool vp_only_enable);
int isp_csi2_ctrl_config_debug_enable(struct isp_csi2_device *isp_csi2,
				      bool debug_enable);
int isp_csi2_ctrl_config_burst_size(struct isp_csi2_device *isp_csi2,
				    u8 burst_size);
int isp_csi2_ctrl_config_ecc_enable(struct isp_csi2_device *isp_csi2,
				    bool ecc_enable);
int isp_csi2_ctrl_config_secure_mode(struct isp_csi2_device *isp_csi2,
				     bool secure_mode);
int isp_csi2_ctrl_config_if_enable(struct isp_csi2_device *isp_csi2,
				   bool if_enable);
int isp_csi2_ctrl_config_vp_out_ctrl(struct isp_csi2_device *isp_csi2,
				     u8 vp_out_ctrl);
int isp_csi2_ctrl_update(struct isp_csi2_device *isp_csi2, bool force_update);
int isp_csi2_ctrl_get(struct isp_csi2_device *isp_csi2);
int isp_csi2_ctx_config_virtual_id(struct isp_csi2_device *isp_csi2,
				   u8 ctxnum, u8 virtual_id);
int isp_csi2_ctx_config_frame_count(struct isp_csi2_device *isp_csi2,
				    u8 ctxnum, u8 frame_count);
int isp_csi2_ctx_config_format(struct isp_csi2_device *isp_csi2,
			       u8 ctxnum, u32 pixformat);
int isp_csi2_ctx_config_alpha(struct isp_csi2_device *isp_csi2,
			      u8 ctxnum, u16 alpha);
int isp_csi2_ctx_config_data_offset(struct isp_csi2_device *isp_csi2,
				    u8 ctxnum, u16 data_offset);
int isp_csi2_ctx_config_ping_addr(struct isp_csi2_device *isp_csi2,
				  u8 ctxnum, u32 ping_addr);
int isp_csi2_ctx_config_pong_addr(struct isp_csi2_device *isp_csi2,
				  u8 ctxnum, u32 pong_addr);
int isp_csi2_ctx_config_eof_enabled(struct isp_csi2_device *isp_csi2,
				    u8 ctxnum, bool eof_enabled);
int isp_csi2_ctx_config_eol_enabled(struct isp_csi2_device *isp_csi2,
				    u8 ctxnum, bool eol_enabled);
int isp_csi2_ctx_config_checksum_enabled(struct isp_csi2_device *isp_csi2,
					 u8 ctxnum, bool checksum_enabled);
int isp_csi2_ctx_config_enabled(struct isp_csi2_device *isp_csi2,
				u8 ctxnum, bool enabled);
int isp_csi2_ctx_update(struct isp_csi2_device *isp_csi2,
			u8 ctxnum, bool force_update);
int isp_csi2_ctx_get(struct isp_csi2_device *isp_csi2, u8 ctxnum);
int isp_csi2_ctx_update_all(struct isp_csi2_device *isp_csi2,
			    bool force_update);
int isp_csi2_ctx_get_all(struct isp_csi2_device *isp_csi2);
int isp_csi2_phy_config(struct isp_csi2_device *isp_csi2,
			struct isp_csi2_phy_cfg *desiredphyconfig);
int isp_csi2_calc_phy_cfg0(struct isp_csi2_device *isp_csi2,
			   u32 mipiclk, u32 lbound_hs_settle,
			   u32 ubound_hs_settle);
int isp_csi2_phy_update(struct isp_csi2_device *isp_csi2,
			bool force_update);
int isp_csi2_phy_get(struct isp_csi2_device *isp_csi2);
int isp_csi2_timings_config_forcerxmode(struct isp_csi2_device *isp_csi2,
					u8 io, bool force_rx_mode);
int isp_csi2_timings_config_stopstate_16x(struct isp_csi2_device *isp_csi2,
					  u8 io, bool stop_state_16x);
int isp_csi2_timings_config_stopstate_4x(struct isp_csi2_device *isp_csi2,
					 u8 io, bool stop_state_4x);
int isp_csi2_timings_config_stopstate_cnt(struct isp_csi2_device *isp_csi2,
					  u8 io, u16 stop_state_counter);
int isp_csi2_timings_update(struct isp_csi2_device *isp_csi2,
			    u8 io, bool force_update);
int isp_csi2_timings_get(struct isp_csi2_device *isp_csi2, u8 io);
int isp_csi2_timings_update_all(struct isp_csi2_device *isp_csi2, bool force_update);
int isp_csi2_timings_get_all(struct isp_csi2_device *isp_csi2);
void isp_csi2_irq_complexio1_set(struct isp_csi2_device *isp_csi2, int enable);
void isp_csi2_irq_ctx_set(struct isp_csi2_device *isp_csi2, int enable);
void isp_csi2_irq_status_set(struct isp_csi2_device *isp_csi2, int enable);
void isp_csi2_irq_all_set(struct isp_csi2_device *isp_csi2, int enable);

int isp_csi2_try_pipeline(struct isp_csi2_device *isp_csi2,
			  struct isp_pipeline *pipe);

int isp_csi2_isr(struct isp_csi2_device *isp_csi2);
int isp_csi2_reset(struct isp_csi2_device *isp_csi2);
void isp_csi2_enable(struct isp_csi2_device *isp_csi2, int enable);
void isp_csi2_regdump(struct isp_csi2_device *isp_csi2);

void ispcsi2_save_context(struct device *dev);
void ispcsi2_restore_context(struct device *dev);

#endif	/* OMAP_ISP_CSI2_H */

