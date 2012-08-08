/*
 * ti_hdmi.h
 *
 * HDMI driver definition for TI OMAP4, DM81xx, DM38xx  Processor.
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _TI_HDMI_H
#define _TI_HDMI_H

#include <linux/fb.h>
#include <video/cec.h>
#include <linux/wait.h>

#include "hdcp.h"

#define HDMI_CEC_INT		0x100
#define HDMI_HDCP_INT		0x200

struct hdmi_ip_data;
struct snd_aes_iec958;
struct snd_cea_861_aud_if;
struct hdmi_audio_dma;

enum hdmi_pll_pwr {
	HDMI_PLLPWRCMD_ALLOFF = 0,
	HDMI_PLLPWRCMD_PLLONLY = 1,
	HDMI_PLLPWRCMD_BOTHON_ALLCLKS = 2,
	HDMI_PLLPWRCMD_BOTHON_NOPHYCLK = 3
};

enum hdmi_core_hdmi_dvi {
	HDMI_DVI = 0,
	HDMI_HDMI = 1
};

enum hdmi_clk_refsel {
	HDMI_REFSEL_PCLK = 0,
	HDMI_REFSEL_REF1 = 1,
	HDMI_REFSEL_REF2 = 2,
	HDMI_REFSEL_SYSCLK = 3
};

enum hdmi_deep_color_mode {
	HDMI_DEEP_COLOR_24BIT = 0,
	HDMI_DEEP_COLOR_30BIT = 1,
	HDMI_DEEP_COLOR_36BIT = 2,
};

enum hdmi_range {
	HDMI_LIMITED_RANGE = 0,
	HDMI_FULL_RANGE,
};

enum hdmi_s3d_frame_structure {
	HDMI_S3D_FRAME_PACKING          = 0,
	HDMI_S3D_FIELD_ALTERNATIVE      = 1,
	HDMI_S3D_LINE_ALTERNATIVE       = 2,
	HDMI_S3D_SIDE_BY_SIDE_FULL      = 3,
	HDMI_S3D_L_DEPTH                = 4,
	HDMI_S3D_L_DEPTH_GP_GP_DEPTH    = 5,
	HDMI_S3D_SIDE_BY_SIDE_HALF      = 8
};

/* Subsampling types used for Stereoscopic 3D over HDMI. Below HOR
stands for Horizontal, QUI for Quinxcunx Subsampling, O for odd fields,
E for Even fields, L for left view and R for Right view*/
enum hdmi_s3d_subsampling_type {
	HDMI_S3D_HOR_OL_OR = 0,
	HDMI_S3D_HOR_OL_ER = 1,
	HDMI_S3D_HOR_EL_OR = 2,
	HDMI_S3D_HOR_EL_ER = 3,
	HDMI_S3D_QUI_OL_OR = 4,
	HDMI_S3D_QUI_OL_ER = 5,
	HDMI_S3D_QUI_EL_OR = 6,
	HDMI_S3D_QUI_EL_ER = 7
};

struct hdmi_s3d_info {
	bool subsamp;
	enum hdmi_s3d_frame_structure  frame_struct;
	enum hdmi_s3d_subsampling_type  subsamp_pos;
	bool vsi_enabled;
};

struct hdmi_cm {
	int	code;
	int	mode;
};

struct hdmi_config {
	struct fb_videomode timings;
	u16 interlace;
	struct hdmi_cm cm;
	bool s3d_enabled;
	struct hdmi_s3d_info s3d_info;
	enum hdmi_deep_color_mode deep_color;
	enum hdmi_range range;
};

/* HDMI PLL structure */
struct hdmi_pll_info {
	u16 regn;
	u16 regm;
	u32 regmf;
	u16 regm2;
	u16 regsd;
	u16 dcofreq;
	enum hdmi_clk_refsel refsel;
};

struct hdmi_irq_vector {
	u8      pll_recal;
	u8      pll_unlock;
	u8      pll_lock;
	u8      phy_disconnect;
	u8      phy_connect;
	u8      phy_short_5v;
	u8      video_end_fr;
	u8      video_vsync;
	u8      fifo_sample_req;
	u8      fifo_overflow;
	u8      fifo_underflow;
	u8      ocp_timeout;
	u8      core;
};

struct ti_hdmi_ip_ops {

	void (*video_configure)(struct hdmi_ip_data *ip_data);

	int (*phy_enable)(struct hdmi_ip_data *ip_data);

	void (*phy_disable)(struct hdmi_ip_data *ip_data);

	int (*read_edid)(struct hdmi_ip_data *ip_data, u8 *edid, int len);

	bool (*detect)(struct hdmi_ip_data *ip_data);

	int (*pll_enable)(struct hdmi_ip_data *ip_data);

	void (*pll_disable)(struct hdmi_ip_data *ip_data);

	void (*video_enable)(struct hdmi_ip_data *ip_data, bool start);

	void (*dump_wrapper)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	void (*dump_core)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	void (*dump_pll)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	void (*dump_phy)(struct hdmi_ip_data *ip_data, struct seq_file *s);

	int (*set_phy)(struct hdmi_ip_data *ip_data, bool hpd);

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
	void (*audio_enable)(struct hdmi_ip_data *ip_data, bool start);

	void (*audio_start)(struct hdmi_ip_data *ip_data, bool start);

	int (*audio_config)(struct hdmi_ip_data *ip_data,
		struct snd_aes_iec958 *iec, struct snd_cea_861_aud_if *aud_if);
#endif

	int (*irq_handler) (struct hdmi_ip_data *ip_data);

	int (*irq_process) (struct hdmi_ip_data *ip_data);

	int (*configure_range)(struct hdmi_ip_data *ip_data);

	int (*cec_get_rx_cmd)(struct hdmi_ip_data *ip_data, char *rx_cmd);

	int (*cec_read_rx_cmd)(struct hdmi_ip_data *ip_data,
		struct cec_rx_data *rx_data);

	int (*cec_transmit_cmd)(struct hdmi_ip_data *ip_data,
		struct cec_tx_data *data, int *cmd_acked);

	int (*power_on_cec)(struct hdmi_ip_data *ip_data);

	int (*power_off_cec)(struct hdmi_ip_data *ip_data);

	int (*cec_int_handler)(struct hdmi_ip_data *ip_data);

	int (*cec_clr_rx_int)(struct hdmi_ip_data *ip_data, int cec_rx);

	int (*cec_get_reg_device_list)(struct hdmi_ip_data *ip_data);

	int (*cec_add_reg_device)(struct hdmi_ip_data *ip_data,
		int device_id, int clear);

	int (*cec_set_reg_device_list)(struct hdmi_ip_data *ip_data,
		int mask);

	int (*hdcp_init)(struct hdmi_ip_data *ip_data);

	int (*hdcp_enable) (struct hdmi_ip_data *ip_data);

	int (*hdcp_disable)(struct hdmi_ip_data *ip_data);

	int (*hdcp_status)(struct hdmi_ip_data *ip_data);

	int (*hdcp_int_handler)(struct hdmi_ip_data *ip_data);

};

/*
 * Refer to section 8.2 in HDMI 1.3 specification for
 * details about infoframe databytes
 */
struct hdmi_core_infoframe_avi {
	/* Y0, Y1 rgb,yCbCr */
	u8	db1_format;
	/* A0  Active information Present */
	u8	db1_active_info;
	/* B0, B1 Bar info data valid */
	u8	db1_bar_info_dv;
	/* S0, S1 scan information */
	u8	db1_scan_info;
	/* C0, C1 colorimetry */
	u8	db2_colorimetry;
	/* M0, M1 Aspect ratio (4:3, 16:9) */
	u8	db2_aspect_ratio;
	/* R0...R3 Active format aspect ratio */
	u8	db2_active_fmt_ar;
	/* ITC IT content. */
	u8	db3_itc;
	/* EC0, EC1, EC2 Extended colorimetry */
	u8	db3_ec;
	/* Q1, Q0 Quantization range */
	u8	db3_q_range;
	/* SC1, SC0 Non-uniform picture scaling */
	u8	db3_nup_scaling;
	/* VIC0..6 Video format identification */
	u8	db4_videocode;
	/* PR0..PR3 Pixel repetition factor */
	u8	db5_pixel_repeat;
	/* Line number end of top bar */
	u16	db6_7_line_eoftop;
	/* Line number start of bottom bar */
	u16	db8_9_line_sofbottom;
	/* Pixel number end of left bar */
	u16	db10_11_pixel_eofleft;
	/* Pixel number start of right bar */
	u16	db12_13_pixel_sofright;
};

struct hdmi_ip_data {
	void __iomem	*base_wp;	/* HDMI wrapper */
	unsigned long	core_sys_offset;
	unsigned long	core_av_offset;
	unsigned long	pll_offset;
	unsigned long	phy_offset;
	unsigned long	cec_offset;
	const struct ti_hdmi_ip_ops *ops;
	struct hdmi_config cfg;
	struct hdmi_pll_info pll_data;
	struct hdmi_core_infoframe_avi avi_cfg;

	/* ti_hdmi_4xxx_ip private data. These should be in a separate struct */
	int hpd_gpio;
	bool phy_tx_enabled;

	bool set_mode;
	wait_queue_head_t tx_complete;/*ti signal TX complete*/
	int cec_int;
};
int ti_hdmi_4xxx_phy_enable(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_phy_disable(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_read_edid(struct hdmi_ip_data *ip_data, u8 *edid, int len);
bool ti_hdmi_4xxx_detect(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_wp_video_start(struct hdmi_ip_data *ip_data, bool start);
int ti_hdmi_4xxx_pll_enable(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_pll_disable(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_irq_handler(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_basic_configure(struct hdmi_ip_data *ip_data);
void ti_hdmi_4xxx_wp_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
void ti_hdmi_4xxx_pll_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
void ti_hdmi_4xxx_core_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
void ti_hdmi_4xxx_phy_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
int ti_hdmi_4xxx_set_phy_on_hpd(struct hdmi_ip_data *ip_data, bool hpd);
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
int hdmi_compute_acr(u32 sample_freq, u32 *n, u32 *cts);
void ti_hdmi_4xxx_wp_audio_config_dma(struct hdmi_ip_data *ip_data,
					struct hdmi_audio_dma *aud_dma);
#endif
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
void ti_hdmi_4xxx_wp_audio_enable(struct hdmi_ip_data *ip_data, bool enable);
void ti_hdmi_4xxx_audio_start(struct hdmi_ip_data *ip_data, bool enable);
int ti_hdmi_4xxx_audio_config(struct hdmi_ip_data *ip_data,
		struct snd_aes_iec958 *iec, struct snd_cea_861_aud_if *aud_if);
#endif
#if defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
void ti_hdmi_5xxx_wp_audio_enable(struct hdmi_ip_data *ip_data, bool enable);
void ti_hdmi_5xxx_audio_start(struct hdmi_ip_data *ip_data, bool enable);
int ti_hdmi_5xxx_audio_config(struct hdmi_ip_data *ip_data,
		struct snd_aes_iec958 *iec, struct snd_cea_861_aud_if *aud_if);
#endif
int hdmi_get_ipdata(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_notify_hpd(struct hdmi_ip_data *ip_data, bool hpd_state);
int ti_hdmi_4xxx_cec_get_rx_cmd(struct hdmi_ip_data *ip_data,
	char *rx_cmd);
int ti_hdmi_4xxx_cec_read_rx_cmd(struct hdmi_ip_data *ip_data,
	struct cec_rx_data *rx_data);
int ti_hdmi_4xxx_cec_transmit_cmd(struct hdmi_ip_data *ip_data,
	struct cec_tx_data *data, int *cmd_acked);
int ti_hdmi_4xxx_power_on_cec(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_power_off_cec(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_cec_int_handler(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_cec_clr_rx_int(struct hdmi_ip_data *ip_data, int cec_rx);
int ti_hdmi_4xxx_cec_get_reg_device_list(struct hdmi_ip_data *ip_data);
int ti_hdmi_4xxx_cec_add_reg_device(struct hdmi_ip_data *ip_data,
	int device_id, int clear);
int ti_hdmi_4xxx_cec_set_reg_device_list(struct hdmi_ip_data *ip_data,
	int mask);
int hdmi_ti_4xxx_wp_get_video_state(struct hdmi_ip_data *ip_data);
int hdmi_ti_4xxx_set_wait_soft_reset(struct hdmi_ip_data *ip_data);
void ti_hdmi_5xxx_basic_configure(struct hdmi_ip_data *ip_data);
void ti_hdmi_5xxx_core_dump(struct hdmi_ip_data *ip_data, struct seq_file *s);
int ti_hdmi_5xxx_read_edid(struct hdmi_ip_data *ip_data,
				u8 *edid, int len);
int ti_hdmi_5xxx_irq_handler(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_irq_process(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_hdcp_status(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_configure_range(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_cec_get_rx_cmd(struct hdmi_ip_data *ip_data,
	char *rx_cmd);
int ti_hdmi_5xxx_cec_read_rx_cmd(struct hdmi_ip_data *ip_data,
	struct cec_rx_data *rx_data);
int ti_hdmi_5xxx_cec_transmit_cmd(struct hdmi_ip_data *ip_data,
	struct cec_tx_data *data, int *cmd_acked);
int ti_hdmi_5xxx_power_on_cec(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_power_off_cec(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_cec_int_handler(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_cec_clr_rx_int(struct hdmi_ip_data *ip_data, int cec_rx);
int ti_hdmi_5xxx_cec_get_reg_device_list(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_cec_add_reg_device(struct hdmi_ip_data *ip_data,
	int device_id, int clear);
int ti_hdmi_5xxx_cec_set_reg_device_list(struct hdmi_ip_data *ip_data,
	int mask);
int ti_hdmi_5xxx_hdcp_init(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_hdcp_enable(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_hdcp_disable(struct hdmi_ip_data *ip_data);
int ti_hdmi_5xxx_hdcp_int_handler(struct hdmi_ip_data *ip_data);

#endif
