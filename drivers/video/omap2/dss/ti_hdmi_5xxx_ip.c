
/*
 * ti_hdmi_5xxx_ip.c
 *
 * HDMI TI OMAP5 IP driver Library
 * Copyright (C) 2011-2012 Texas Instruments Incorporated - http://www.ti.com/
 * Author: Mythri pk <mythripk@ti.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/seq_file.h>

#include "ti_hdmi_5xxx_ip.h"
#include "dss.h"

static inline void hdmi_write_reg(void __iomem *base_addr,
		const unsigned long idx, u32 val)
{
	__raw_writel(val, base_addr + idx);
}

static inline u32 hdmi_read_reg(void __iomem *base_addr,
		const unsigned long idx)
{
	return __raw_readl(base_addr + idx);
}

static inline void __iomem *hdmi_core_sys_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp + ip_data->core_sys_offset;
}

static inline int hdmi_wait_for_bit_change(void __iomem *base_addr,
			const unsigned long idx,
			int b2, int b1, u32 val)
{
	u32 t = 0;
	while (val != REG_GET(base_addr, idx, b2, b1)) {
		udelay(1);
		if (t++ > 10000)
			return !val;
	}
	return val;
}

void ti_hdmi_5xxx_core_dump(struct hdmi_ip_data *ip_data, struct seq_file *s)
{

#define DUMPCORE(r) seq_printf(s, "%-35s %08x\n", #r,\
		hdmi_read_reg(hdmi_core_sys_base(ip_data), r))

	DUMPCORE(HDMI_CORE_FC_INVIDCONF);
	DUMPCORE(HDMI_CORE_FC_INHACTIV0);
	DUMPCORE(HDMI_CORE_FC_INHACTIV1);
	DUMPCORE(HDMI_CORE_FC_INHBLANK0);
	DUMPCORE(HDMI_CORE_FC_INHBLANK1);
	DUMPCORE(HDMI_CORE_FC_INVACTIV0);
	DUMPCORE(HDMI_CORE_FC_INVACTIV1);
	DUMPCORE(HDMI_CORE_FC_INVBLANK);
	DUMPCORE(HDMI_CORE_FC_HSYNCINDELAY0);
	DUMPCORE(HDMI_CORE_FC_HSYNCINDELAY1);
	DUMPCORE(HDMI_CORE_FC_HSYNCINWIDTH0);
	DUMPCORE(HDMI_CORE_FC_HSYNCINWIDTH1);
	DUMPCORE(HDMI_CORE_FC_VSYNCINDELAY);
	DUMPCORE(HDMI_CORE_FC_VSYNCINWIDTH);
	DUMPCORE(HDMI_CORE_FC_CTRLDUR);
	DUMPCORE(HDMI_CORE_FC_EXCTRLDUR);
	DUMPCORE(HDMI_CORE_FC_EXCTRLSPAC);
	DUMPCORE(HDMI_CORE_FC_CH0PREAM);
	DUMPCORE(HDMI_CORE_FC_CH1PREAM);
	DUMPCORE(HDMI_CORE_FC_CH2PREAM);
	DUMPCORE(HDMI_CORE_FC_AVICONF0);
	DUMPCORE(HDMI_CORE_FC_AVICONF1);
	DUMPCORE(HDMI_CORE_FC_AVICONF2);
	DUMPCORE(HDMI_CORE_FC_AVIVID);
	DUMPCORE(HDMI_CORE_FC_PRCONF);

	DUMPCORE(HDMI_CORE_MC_CLKDIS);
	DUMPCORE(HDMI_CORE_MC_SWRSTZREQ);
	DUMPCORE(HDMI_CORE_MC_FLOWCTRL);
	DUMPCORE(HDMI_CORE_MC_PHYRSTZ);
	DUMPCORE(HDMI_CORE_MC_LOCKONCLOCK);

	DUMPCORE(HDMI_CORE_I2CM_SLAVE);
	DUMPCORE(HDMI_CORE_I2CM_ADDRESS);
	DUMPCORE(HDMI_CORE_I2CM_DATAO);
	DUMPCORE(HDMI_CORE_I2CM_DATAI);
	DUMPCORE(HDMI_CORE_I2CM_OPERATION);
	DUMPCORE(HDMI_CORE_I2CM_INT);
	DUMPCORE(HDMI_CORE_I2CM_CTLINT);
	DUMPCORE(HDMI_CORE_I2CM_DIV);
	DUMPCORE(HDMI_CORE_I2CM_SEGADDR);
	DUMPCORE(HDMI_CORE_I2CM_SOFTRSTZ);
	DUMPCORE(HDMI_CORE_I2CM_SEGPTR);
	DUMPCORE(HDMI_CORE_I2CM_SS_SCL_HCNT_1_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_SS_SCL_HCNT_0_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_SS_SCL_LCNT_1_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_SS_SCL_LCNT_0_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_FS_SCL_HCNT_1_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_FS_SCL_HCNT_0_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_FS_SCL_LCNT_1_ADDR);
	DUMPCORE(HDMI_CORE_I2CM_FS_SCL_LCNT_0_ADDR);
}

static void hdmi_core_init(struct hdmi_core_vid_config *video_cfg,
			struct hdmi_core_infoframe_avi *avi_cfg,
			struct hdmi_config *cfg)
{
	printk(KERN_INFO "Enter hdmi_core_init\n");

	/* video core */
	video_cfg->data_enable_pol = 1; /* It is always 1*/
	video_cfg->v_fc_config.timings.hsync_pol = cfg->timings.hsync_pol;
	video_cfg->v_fc_config.timings.x_res = cfg->timings.x_res;
	video_cfg->v_fc_config.timings.hsw = cfg->timings.hsw;
	video_cfg->v_fc_config.timings.hbp = cfg->timings.hbp;
	video_cfg->v_fc_config.timings.hfp = cfg->timings.hfp;
	video_cfg->hblank = cfg->timings.hfp +
				cfg->timings.hbp + cfg->timings.hsw;
	video_cfg->v_fc_config.timings.vsync_pol = cfg->timings.vsync_pol;
	video_cfg->v_fc_config.timings.y_res = cfg->timings.y_res;
	video_cfg->v_fc_config.timings.vsw = cfg->timings.vsw;
	video_cfg->v_fc_config.timings.vfp = cfg->timings.vfp;
	video_cfg->v_fc_config.timings.vbp = cfg->timings.vbp;
	video_cfg->vblank_osc = 0; /* Always 0 - need to confirm */
	video_cfg->vblank = cfg->timings.vsw +
				cfg->timings.vfp + cfg->timings.vbp;
	video_cfg->v_fc_config.cm.mode = cfg->cm.mode;
	video_cfg->v_fc_config.timings.interlace = cfg->timings.interlace;

	/* info frame */
	avi_cfg->db1_format = 0;
	avi_cfg->db1_active_info = 0;
	avi_cfg->db1_bar_info_dv = 0;
	avi_cfg->db1_scan_info = 0;
	avi_cfg->db2_colorimetry = 0;
	avi_cfg->db2_aspect_ratio = 0;
	avi_cfg->db2_active_fmt_ar = 0;
	avi_cfg->db3_itc = 0;
	avi_cfg->db3_ec = 0;
	avi_cfg->db3_q_range = 0;
	avi_cfg->db3_nup_scaling = 0;
	avi_cfg->db4_videocode = 0;
	avi_cfg->db5_pixel_repeat = 0;
	avi_cfg->db6_7_line_eoftop = 0 ;
	avi_cfg->db8_9_line_sofbottom = 0;
	avi_cfg->db10_11_pixel_eofleft = 0;
	avi_cfg->db12_13_pixel_sofright = 0;

}

/* DSS_HDMI_CORE_VIDEO_CONFIG */
static void hdmi_core_video_config(struct hdmi_ip_data *ip_data,
				struct hdmi_core_vid_config *cfg)
{
	unsigned char r = 0;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	/* Set hsync, vsync and data-enable polarity  */
	r = hdmi_read_reg(core_sys_base, HDMI_CORE_FC_INVIDCONF);

	r = FLD_MOD(r, cfg->v_fc_config.timings.vsync_pol, 6, 6);
	r = FLD_MOD(r, cfg->v_fc_config.timings.hsync_pol, 5, 5);
	r = FLD_MOD(r, cfg->data_enable_pol, 4, 4);
	r = FLD_MOD(r, cfg->vblank_osc, 1, 1);
	r = FLD_MOD(r, cfg->v_fc_config.timings.interlace, 0, 0);
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_INVIDCONF, r);

	/* set x resolution */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHACTIV1,
			(cfg->v_fc_config.timings.x_res >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHACTIV0,
			(cfg->v_fc_config.timings.x_res & 0xFF), 7, 0);

	/* set y resolution */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVACTIV1,
			(cfg->v_fc_config.timings.y_res >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVACTIV0,
			(cfg->v_fc_config.timings.y_res & 0xFF), 7, 0);

	/* set horizontal blanking pixels */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHBLANK1,
			(cfg->hblank >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHBLANK0,
			(cfg->hblank & 0xFF), 7, 0);

	/* set vertial blanking pixels */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVBLANK, cfg->vblank, 7, 0);

	/* set horizontal sync offset */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINDELAY1,
			(cfg->v_fc_config.timings.hfp >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINDELAY0,
			(cfg->v_fc_config.timings.hfp & 0xFF), 7, 0);

	/* set vertical sync offset */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_VSYNCINDELAY,
			cfg->v_fc_config.timings.vfp, 7, 0);

	/* set horizontal sync pulse width */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINWIDTH1,
			(cfg->v_fc_config.timings.hsw >> 8), 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINWIDTH0,
			(cfg->v_fc_config.timings.hsw & 0xFF), 7, 0);

	/*  set vertical sync pulse width */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_VSYNCINWIDTH,
			cfg->v_fc_config.timings.vsw, 5, 0);

	/* select DVI mode */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVIDCONF,
		cfg->v_fc_config.cm.mode, 3, 3);
}

static void hdmi_core_aux_infoframe_avi_config(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	struct hdmi_core_infoframe_avi info_avi = ip_data->avi_cfg;

	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF0,
				info_avi.db1_format, 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF0,
				info_avi.db1_active_info, 6, 6);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF0,
				info_avi.db1_bar_info_dv, 3, 2);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF0,
				info_avi.db1_scan_info, 5, 4);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF1,
				info_avi.db2_colorimetry, 7, 6);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF1,
				info_avi.db2_aspect_ratio, 5, 4);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF1,
				info_avi.db2_active_fmt_ar, 3, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF2,
				info_avi.db3_itc, 7, 7);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF2,
				info_avi.db3_ec, 6, 4);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF2,
				info_avi.db3_q_range, 3, 2);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVICONF2,
				info_avi.db3_nup_scaling, 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AVIVID,
				info_avi.db4_videocode, 6, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_PRCONF,
				info_avi.db5_pixel_repeat, 3, 0);
}

void hdmi_enable_video_path(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	printk(KERN_INFO "Enable video_path\n");

	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_CTRLDUR, 0x0C, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_EXCTRLDUR, 0x20, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_EXCTRLSPAC, 0x01, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_CH0PREAM, 0x0B, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_CH1PREAM, 0x16, 5, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_CH2PREAM, 0x21, 5, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_MC_CLKDIS, 0x00, 0, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_MC_CLKDIS, 0x00, 1, 1);
}

void ti_hdmi_5xxx_basic_configure(struct hdmi_ip_data *ip_data)
{
	/* HDMI */
	struct omap_video_timings video_timing;
	struct hdmi_video_format video_format;
	/* HDMI core */
	struct hdmi_core_vid_config v_core_cfg;
	struct hdmi_core_infoframe_avi *avi_cfg = &ip_data->avi_cfg;
	struct hdmi_config *cfg = &ip_data->cfg;

	hdmi_wp_init(&video_timing, &video_format);

	hdmi_core_init(&v_core_cfg, avi_cfg, cfg);

	hdmi_wp_video_init_format(&video_format, &video_timing, cfg);

	hdmi_wp_video_config_timing(ip_data, &video_timing);

	/* video config */
	video_format.packing_mode = HDMI_PACK_24b_RGB_YUV444_YUV422;

	hdmi_wp_video_config_format(ip_data, &video_format);

	hdmi_wp_video_config_interface(ip_data);

	/*
	 * configure core video part
	 * set software reset in the core
	 */
	v_core_cfg.packet_mode = HDMI_PACKETMODE24BITPERPIXEL;

	hdmi_core_video_config(ip_data, &v_core_cfg);

	/*
	 * configure packet
	 * info frame video see doc CEA861-D page 65
	 */
	avi_cfg->db1_format = HDMI_INFOFRAME_AVI_DB1Y_RGB;
	avi_cfg->db1_active_info =
			HDMI_INFOFRAME_AVI_DB1A_ACTIVE_FORMAT_OFF;
	avi_cfg->db1_bar_info_dv = HDMI_INFOFRAME_AVI_DB1B_NO;
	avi_cfg->db1_scan_info = HDMI_INFOFRAME_AVI_DB1S_0;
	avi_cfg->db2_colorimetry = HDMI_INFOFRAME_AVI_DB2C_NO;
	avi_cfg->db2_aspect_ratio = HDMI_INFOFRAME_AVI_DB2M_NO;
	avi_cfg->db2_active_fmt_ar = HDMI_INFOFRAME_AVI_DB2R_SAME;
	avi_cfg->db3_itc = HDMI_INFOFRAME_AVI_DB3ITC_NO;
	avi_cfg->db3_ec = HDMI_INFOFRAME_AVI_DB3EC_XVYUV601;
	avi_cfg->db3_q_range = HDMI_INFOFRAME_AVI_DB3Q_DEFAULT;
	avi_cfg->db3_nup_scaling = HDMI_INFOFRAME_AVI_DB3SC_NO;
	avi_cfg->db4_videocode = cfg->cm.code;
	avi_cfg->db5_pixel_repeat = HDMI_INFOFRAME_AVI_DB5PR_NO;
	avi_cfg->db6_7_line_eoftop = 0;
	avi_cfg->db8_9_line_sofbottom = 0;
	avi_cfg->db10_11_pixel_eofleft = 0;
	avi_cfg->db12_13_pixel_sofright = 0;

	hdmi_core_aux_infoframe_avi_config(ip_data);

	hdmi_enable_video_path(ip_data);
}
