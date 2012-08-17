
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
#include <linux/i2c.h>
#include <drm/drm_edid.h>
#if defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
#include <sound/asoundef.h>
#endif

#include "ti_hdmi_5xxx_ip.h"
#include "dss.h"

#define CEC_RETRY_COUNT 5
#define CEC_TX_WAIT_COUNT 10 /*wait max for 100ms*/
#define CEC_MAX_NUM_OPERANDS 14
#define CEC_HEADER_CMD_COUNT 2

#define HDMI_CORE_PRODUCT_ID1_HDCP 0xC1
#define HDMI_CORE_A_HDCPCFG0_HDCP_EN_MASK 0x4

#define RETRY_CNT		3
#define OMAP2_L4_IO_OFFSET	0xb2000000
#define CONTROL_CORE_PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA \
				(OMAP2_L4_IO_OFFSET + 0x4A002940)

#define HDMI_CORE_A_HDCPOBS0_HDCPENABLED (1 << 0)

static const struct csc_table csc_table_deepcolor[4] = {
	/* HDMI_DEEP_COLOR_24BIT */
	[0] = { 7036, 0, 0, 32,
		0, 7036, 0, 32,
		0, 0, 7036, 32 },
	/* HDMI_DEEP_COLOR_30BIT */
	[1] = { 7015, 0, 0, 128,
		0, 7015, 0, 128,
		0, 0, 7015, 128 },
	/* HDMI_DEEP_COLOR_36BIT */
	[2] = { 7010, 0, 0, 512,
		0, 7010, 0, 512,
		0, 0, 7010, 512},
	/* FULL RANGE */
	[3] = { 8192, 0, 0, 0,
		0, 8192, 0, 0,
		0, 0, 8192, 0},
};

static int panel_hdmi_i2c_read(struct i2c_adapter *adapter,
	unsigned char *buf, u16 count, u8 ext)
{
	int r, retries;
	u8 segptr = ext / 2;
	u8 offset = ext * EDID_LENGTH;

	for (retries = RETRY_CNT; retries > 0; retries--) {

		if (ext >= 2) {
			struct i2c_msg msgs_extended[] = {
				{
					.addr   = 0x30,
					.flags  = 0,
					.len    = 1,
					.buf    = &segptr,
				},
				{
					.addr   = DDC_ADDR,
					.flags  = 0,
					.len    = 1,
					.buf    = &offset,
				},
				{
					.addr   = DDC_ADDR,
					.flags  = I2C_M_RD,
					.len    = count,
					.buf    = buf,
				}
			};

			r = i2c_transfer(adapter, msgs_extended, 3);
			if (r == 3)
				return 0;

		} else {
			struct i2c_msg msgs[] = {
				{
					.addr   = DDC_ADDR,
					.flags  = 0,
					.len    = 1,
					.buf    = &offset,
				},
				{
					.addr   = DDC_ADDR,
					.flags  = I2C_M_RD,
					.len    = count,
					.buf    = buf,
				}
			};

			r = i2c_transfer(adapter, msgs, 2);
			if (r == 2)
				return 0;
		}

		if (r != -EAGAIN)
			break;
	}

	return r < 0 ? r : -EIO;
}

static int panel_hdmi_read_edid(u8 *edid, int len)
{
	struct i2c_adapter *adapter;
	int r = 0, n = 0, i = 0;
	int max_ext_blocks = (len / 128) - 1;

	adapter = i2c_get_adapter(0);
	if (!adapter) {
		pr_err("panel_dvi_read_edid: Failed to get I2C adapter, bus 0\n");
		return -EINVAL;
	}

	r = panel_hdmi_i2c_read(adapter, edid, EDID_LENGTH, 0);
	if (r) {
		return r;
	} else {
		/*Multiblock read*/
		n = edid[0x7e];

		if (n > max_ext_blocks)
			n = max_ext_blocks;
		for (i = 1; i <= n; i++) {
			r = panel_hdmi_i2c_read(adapter, edid + i*EDID_LENGTH,
				EDID_LENGTH, i);
			if (r)
				return r;
		}
	}

	return r;
}

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

static inline void __iomem *hdmi_wp_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp;
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

static inline void hdmi_core_ddc_req_addr(struct hdmi_ip_data *ip_data,
						u8 addr, int ext)
{
	u8 seg_ptr = ext / 2;
	u8 edidaddr = ((ext % 2) * 0x80) + addr;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_ADDRESS, edidaddr, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SEGPTR, seg_ptr, 7, 0);

	if (seg_ptr)
		REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_OPERATION, 1, 1, 1);
	else
		REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_OPERATION, 1, 0, 0);
}

static void hdmi_core_ddc_init(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	/*Mask the interrupts*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_CTLINT, 0x0, 2, 2);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_CTLINT, 0x0, 6, 6);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_INT, 0x0, 2, 2);

	/* Master clock division */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_DIV, 0x5, 3, 0);

	/* Standard speed counter */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SS_SCL_HCNT_1_ADDR, 0x0, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SS_SCL_HCNT_0_ADDR, 0x79, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SS_SCL_LCNT_1_ADDR, 0x0, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SS_SCL_LCNT_0_ADDR, 0x91, 7, 0);

	/* Fast speed counter*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_FS_SCL_HCNT_1_ADDR, 0x0, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_FS_SCL_HCNT_0_ADDR, 0x0F, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_FS_SCL_LCNT_1_ADDR, 0x0, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_FS_SCL_LCNT_0_ADDR, 0x21, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SLAVE, 0x50, 6, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_SEGADDR, 0x30, 6, 0);
}

static int hdmi_core_ddc_edid(struct hdmi_ip_data *ip_data,
					u8 *pedid, int ext)
{
	u8 cur_addr = 0;
	char checksum = 0;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	hdmi_core_ddc_req_addr(ip_data, cur_addr, ext);

	/* Unmask the interrupts*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_CTLINT, 0x1, 2, 2);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_CTLINT, 0x1, 6, 6);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2CM_INT, 0x1, 2, 2);

	/* FIXME:This is a hack to  read only 128 bytes data with a mdelay
	 * Ideally the read has to be based on the done interrupt and
	 * status which is not received thus it is ignored for now
	 */
	while (cur_addr < 128) {
	#if 0
		if (hdmi_wait_for_bit_change(HDMI_CORE_I2CM_INT,
						0, 0, 1) != 1) {
			DSSERR("Failed to recieve done interrupt\n");
			return -ETIMEDOUT;
		}
	#endif
		mdelay(1);
		pedid[cur_addr] = REG_GET(core_sys_base,
					HDMI_CORE_I2CM_DATAI, 7, 0);
		DSSDBG("pedid[%d] = %d", cur_addr, pedid[cur_addr]);
		checksum += pedid[cur_addr++];
		hdmi_core_ddc_req_addr(ip_data, cur_addr, ext);
	}

	return 0;

}

static inline void ctrl_core_hdmi_write_reg(u32 val)
{
	__raw_writel(val, (void __iomem *)CONTROL_CORE_PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA);
}

int ti_hdmi_5xxx_read_edid(struct hdmi_ip_data *ip_data,
				u8 *edid, int len)
{
	int r, l;

	if (len < 128)
		return -EINVAL;

	if ((omap_rev() == OMAP5430_REV_ES1_0) ||
		(omap_rev() == OMAP5432_REV_ES1_0)) {
		/*
		 * Set PAD0_HDMI_DDC_SCL_PAD1_HDMI_DDC_SDA:
		 * - Use GPIO7_194 and GPIO7_195
		 * - pull-up/down enabled
		 *   - pull-up selected
		 */
		ctrl_core_hdmi_write_reg((u32)0x011E011E);
		r = panel_hdmi_read_edid(edid, len);
		/* Use hdmi_ddc_scl and hdmi_ddc_sda for hdcp */
		ctrl_core_hdmi_write_reg((u32)0x01000100);

		return r;
	} else {

		hdmi_core_ddc_init(ip_data);

		r = hdmi_core_ddc_edid(ip_data, edid, 0);
		if (r)
			return r;

		l = 128;

		if (len >= 128 * 2 && edid[0x7e] > 0) {
			r = hdmi_core_ddc_edid(ip_data, edid + 0x80, 1);
			if (r)
				return r;
			l += 128;
		}
		return 0;
	}
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
	video_cfg->v_fc_config.timings = cfg->timings;
	video_cfg->hblank = cfg->timings.right_margin +
			cfg->timings.left_margin + cfg->timings.hsync_len;

	video_cfg->vblank_osc = 0; /* Always 0 - need to confirm */
	video_cfg->vblank = cfg->timings.vsync_len +
			cfg->timings.lower_margin + cfg->timings.upper_margin;
	video_cfg->v_fc_config.cm.mode = cfg->cm.mode;
	video_cfg->v_fc_config.interlace = cfg->interlace;

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

static void hdmi_core_infoframe_vsi_config(struct hdmi_ip_data *ip_data,
				struct hdmi_s3d_info info_s3d)
{
	int length;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	/* For side-by-side(HALF) we need to specify subsampling
	 * in 3D_ext_data
	 */
	length = info_s3d.frame_struct == HDMI_S3D_SIDE_BY_SIDE_HALF ? 6 : 5;
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_DATAUTO0, 0, 3, 3);
	/* LSB 24bit IEEE Registration Identifier */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDIEEEID0, 0x03);
	/* Mid 24bit IEEE Registration Identifier */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDIEEEID1, 0x0c);
	/* MSB 24bit IEEE Registration Identifier */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDIEEEID2, 0x00);

	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDSIZE, length);

	/* HDMI Video Format => 3D Format indication present
	 * 3d structure and potentially 3D_Ext_Data
	 */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDPAYLOAD(0), 0x40);
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDPAYLOAD(1),
						info_s3d.frame_struct << 4);
	if (info_s3d.frame_struct == HDMI_S3D_SIDE_BY_SIDE_HALF)
		hdmi_write_reg(core_sys_base, HDMI_CORE_FC_VSDPAYLOAD(2),
						info_s3d.subsamp_pos << 4);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_DATAUTO0, 1, 3, 3);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_DATAUTO1, 0, 3, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_DATAUTO2, 1, 7, 4);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_DATAUTO2, 0, 3, 0);
}

/* DSS_HDMI_CORE_VIDEO_CONFIG */
static void hdmi_core_video_config(struct hdmi_ip_data *ip_data,
				struct hdmi_core_vid_config *cfg)
{
	unsigned char r = 0;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	/* Set hsync, vsync and data-enable polarity  */
	r = hdmi_read_reg(core_sys_base, HDMI_CORE_FC_INVIDCONF);

	r = FLD_MOD(r, !!(cfg->v_fc_config.timings.sync &
						FB_SYNC_VERT_HIGH_ACT), 6, 6);
	r = FLD_MOD(r, !!(cfg->v_fc_config.timings.sync &
						FB_SYNC_HOR_HIGH_ACT), 5, 5);
	r = FLD_MOD(r, cfg->hdcp_keepout, 7, 7);
	r = FLD_MOD(r, cfg->data_enable_pol, 4, 4);
	r = FLD_MOD(r, cfg->vblank_osc, 1, 1);
	r = FLD_MOD(r, cfg->v_fc_config.interlace, 0, 0);
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_INVIDCONF, r);

	/* set x resolution */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHACTIV1,
			(cfg->v_fc_config.timings.xres >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHACTIV0,
			(cfg->v_fc_config.timings.xres & 0xFF), 7, 0);

	/* set y resolution */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVACTIV1,
			(cfg->v_fc_config.timings.yres >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVACTIV0,
			(cfg->v_fc_config.timings.yres & 0xFF), 7, 0);

	/* set horizontal blanking pixels */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHBLANK1,
			(cfg->hblank >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INHBLANK0,
			(cfg->hblank & 0xFF), 7, 0);

	/* set vertial blanking pixels */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVBLANK, cfg->vblank, 7, 0);

	/* set horizontal sync offset */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINDELAY1,
			(cfg->v_fc_config.timings.right_margin >> 8), 4, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINDELAY0,
			(cfg->v_fc_config.timings.right_margin & 0xFF), 7, 0);

	/* set vertical sync offset */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_VSYNCINDELAY,
			cfg->v_fc_config.timings.lower_margin, 7, 0);

	/* set horizontal sync pulse width */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINWIDTH1,
			(cfg->v_fc_config.timings.hsync_len >> 8), 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_HSYNCINWIDTH0,
			(cfg->v_fc_config.timings.hsync_len & 0xFF), 7, 0);

	/*  set vertical sync pulse width */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_VSYNCINWIDTH,
			cfg->v_fc_config.timings.vsync_len, 5, 0);

	/* select DVI mode */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVIDCONF,
		cfg->v_fc_config.cm.mode, 3, 3);
}

static void hdmi_core_config_video_packetizer(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	struct hdmi_config *cfg = &ip_data->cfg;
	int clr_depth;

	switch (cfg->deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		clr_depth = 5;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		clr_depth = 6;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		clr_depth = 0;
		break;
	}

	/* COLOR_DEPTH */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_PR_CD, clr_depth, 7, 4);

	/* BYPASS_EN */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_CONF, clr_depth ? 0 : 1, 6, 6);

	/* PP_EN */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_CONF, clr_depth ? 1 : 0, 5, 5);

	/* YCC422_EN */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_CONF, 0, 3, 3);

	/* PP_STUFFING */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_STUFF, clr_depth ? 1 : 0, 1, 1);

	/* YCC422_STUFFING */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_STUFF, 1, 2, 2);

	/* OUTPUT_SELECTOR */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_CONF, clr_depth ? 0 : 2, 1, 0);
}

static void hdmi_core_config_csc(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	struct hdmi_config *cfg = &ip_data->cfg;
	int clr_depth;

	switch (cfg->deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		clr_depth = 5;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		clr_depth = 6;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		clr_depth = 0;
		break;
	}

	/* CSC_COLORDEPTH */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_SCALE, clr_depth, 7, 4);
}

int ti_hdmi_5xxx_hdcp_init(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	DSSDBG("hdcp_initialize\n");

	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0, 0, 2, 2);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_VIDPOLCFG, 1, 4, 4);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG1, 1, 1, 1);

	return 0;
}

int ti_hdmi_5xxx_hdcp_enable(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	struct hdmi_config *cfg = &ip_data->cfg;

	/* Read product ID1 */
	if (hdmi_read_reg(core_sys_base, HDMI_CORE_PRODUCT_ID1) !=
		HDMI_CORE_PRODUCT_ID1_HDCP) {
		DSSDBG("HDCP is not present.\n");
		return -EACCES;
	}

	if (hdmi_read_reg(core_sys_base, HDMI_CORE_A_HDCPCFG0) &
		HDMI_CORE_A_HDCPCFG0_HDCP_EN_MASK) {
		DSSDBG("HDCP already enabled\n");
		return 0;
	}

	/* Select DVI or HDMI */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			ip_data->cfg.cm.mode, 0, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_INVIDCONF, 1, 7, 7);
	/* Set data enable, Hsync and Vsync polarity */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_VIDPOLCFG,
			1, 4, 4);  /* dataen pol */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_VIDPOLCFG,
		!!(cfg->timings.sync & FB_SYNC_VERT_HIGH_ACT), 3, 3);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_VIDPOLCFG,
		!!(cfg->timings.sync & FB_SYNC_HOR_HIGH_ACT), 1, 1);

	/* HDCP only */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			0, 1, 1); /* Disable 1.1 */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			1, 4, 4); /* Ri check */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			0, 6, 6); /* I2C fast mode */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			0, 7, 7); /* Enhanced Link verification */

	/* fixed */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0, 0, 3, 3); /* Av Mute */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			0, 5, 5); /* By pass encryption */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG1,
			0, 1, 1); /* disable encryption */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_VIDPOLCFG,
			0x00, 5, 6); /* video color encryption */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_OESSWCFG,
			64 , 7, 0); /* video color encryption */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG1,
			1, 2, 2); /* Encode packet header */

	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG1,
			0, 3, 3); /* SHA1 KSV */
	/* Reset HDCP engine */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG1,
			0, 0, 0); /* swreset */

	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0,
			1, 2, 2); /* rxdetect */
	return 0;

}

int ti_hdmi_5xxx_hdcp_disable(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_HDCPCFG0, 0, 2, 2);

	return 0;

}

int ti_hdmi_5xxx_hdcp_int_handler(struct hdmi_ip_data *ip_data)
{
	u32 intr;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	intr = hdmi_read_reg(core_sys_base, HDMI_CORE_A_APIINTSTAT);

	if (intr & KSVSHA1CALCINT) {
		/* request memory access */
		REG_FLD_MOD(core_sys_base, HDMI_CORE_A_KSVMEMCTRL, 1, 0, 0);
		REG_FLD_MOD(core_sys_base, HDMI_CORE_A_APIINTCLR, 1, 1, 1);
	} else if (intr & KSVACCESSINT) {
		/* request granted */
		REG_FLD_MOD(core_sys_base, HDMI_CORE_A_APIINTCLR, 1, 0, 0);
	} else {
		REG_FLD_MOD(core_sys_base, HDMI_CORE_A_APIINTCLR, intr, 7, 0);
	}

	if (intr)
		DSSDBG("HDCP Interrupt : 0x%x\n", intr);

	return intr;
}

static void hdmi_core_config_video_sampler(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	struct hdmi_config *cfg = &ip_data->cfg;
	int video_mapping;

	switch (cfg->deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		video_mapping = 3;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		video_mapping = 5;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		video_mapping = 1;
		break;
	}

	/* VIDEO_MAPPING */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_TX_INVID0, video_mapping, 4, 0);
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

static void hdmi_core_csc_config(struct hdmi_ip_data *ip_data,
				struct csc_table csc_coeff)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A1_MSB, csc_coeff.a1 >> 8 , 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A1_LSB, csc_coeff.a1, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A2_MSB, csc_coeff.a2 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A2_LSB, csc_coeff.a2, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A3_MSB, csc_coeff.a3 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A3_LSB, csc_coeff.a3, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A4_MSB, csc_coeff.a4 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_A4_LSB, csc_coeff.a4, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B1_MSB, csc_coeff.b1 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B1_LSB, csc_coeff.b1, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B2_MSB, csc_coeff.b2 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B2_LSB, csc_coeff.b2, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B3_MSB, csc_coeff.b3 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B3_LSB, csc_coeff.b3, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B4_MSB, csc_coeff.b4 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_B4_LSB, csc_coeff.b4, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C1_MSB, csc_coeff.c1 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C1_LSB, csc_coeff.c1, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C2_MSB, csc_coeff.c2 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C2_LSB, csc_coeff.c2, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C3_MSB, csc_coeff.c3 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C3_LSB, csc_coeff.c3, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C4_MSB, csc_coeff.c4 >> 8, 6, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_CSC_COEF_C4_LSB, csc_coeff.c4, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_MC_FLOWCTRL, 0x1, 0, 0);

}

int ti_hdmi_5xxx_configure_range(struct hdmi_ip_data *ip_data)
{
	struct csc_table csc_coeff = {0};

	switch (ip_data->cfg.range) {
	case HDMI_LIMITED_RANGE:
		csc_coeff =  csc_table_deepcolor[ip_data->cfg.deep_color];
		ip_data->avi_cfg.db3_q_range = HDMI_INFOFRAME_AVI_DB3Q_LR;
		break;
	case HDMI_FULL_RANGE:
		csc_coeff =  csc_table_deepcolor[3];  /* full range */
		ip_data->avi_cfg.db3_q_range = HDMI_INFOFRAME_AVI_DB3Q_FR;
		break;
	}
	hdmi_core_csc_config(ip_data, csc_coeff);
	hdmi_core_aux_infoframe_avi_config(ip_data);
	return 0;
}

static void hdmi_enable_video_path(struct hdmi_ip_data *ip_data)
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

static void hdmi_core_mask_interrupts(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_VP_MASK, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_MASK0, 0xe7, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_MASK1, 0xfb, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_MASK2, 0x3, 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_PHY_MASK0, 0xf3, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_PHY_I2CM_INT_ADDR, 0xc, 3, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_PHY_I2CM_CTLINT_ADDR, 0xcc, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_INT, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_CC08, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_D010, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_CEC_MASK, 0x7f, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_GP_MASK, 0x3, 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_HDCP_MASK, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_CEC_MAGIC_MASK, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2C1_MASK, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_I2C2_MASK, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_APIINTMSK, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_INT, 0xf, 7, 0);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_FC_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_FC_STAT1, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_FC_STAT2, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_AS_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_PHY_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_I2CM_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_CEC_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_VP_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_I2CMPHY_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_MUTE, 0x3, 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_APIINTCLR, 0xff, 7, 0);
}

static void hdmi_core_enable_interrupts(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	/* Unmute interrupts */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_MUTE, 0x0, 1, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_A_APIINTMSK, 0x00, 7, 0);
}

int ti_hdmi_5xxx_irq_handler(struct hdmi_ip_data *ip_data)
{
	u32 val = 0, r = 0;
	u32 temp = 0;
	void __iomem *wp_base = hdmi_wp_base(ip_data);

	val = hdmi_read_reg(wp_base, HDMI_WP_IRQSTATUS);
	if (val & HDMI_WP_IRQSTATUS_CORE) {
		temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
			HDMI_CORE_IH_CEC_STAT0);

		if (temp)
			r |= HDMI_CEC_INT;

		temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
			HDMI_CORE_A_APIINTSTAT);

		if (temp)
			r |= HDMI_HDCP_INT;

	}
	/* Ack other interrupts if any */
	hdmi_write_reg(wp_base, HDMI_WP_IRQSTATUS, val);
	/* flush posted write */
	hdmi_read_reg(wp_base, HDMI_WP_IRQSTATUS);

	return r;

}

int ti_hdmi_5xxx_hdcp_status(struct hdmi_ip_data *ip_data)
{
	int status = HDMI_HDCP_FAILED;
	int val = 0;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	if (!core_sys_base) {
		DSSERR("null pointer hit while getting hdmi base address\n");
		return status;
	}

	val = hdmi_read_reg(core_sys_base, HDMI_CORE_A_HDCPOBS0);

	if (val & HDMI_CORE_A_HDCPOBS0_HDCPENABLED)
		status = HDMI_HDCP_ENABLED;

	return status;
}

int ti_hdmi_5xxx_irq_process(struct hdmi_ip_data *ip_data)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_FC_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_FC_STAT1, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_FC_STAT2, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_AS_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_PHY_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_I2CM_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_VP_STAT0, 0xff, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_IH_I2CMPHY_STAT0, 0xff, 7, 0);

	return 0;
}

void ti_hdmi_5xxx_basic_configure(struct hdmi_ip_data *ip_data)
{
	/* HDMI */
	struct omap_video_timings video_timing;
	struct hdmi_video_format video_format;
	struct hdmi_video_interface video_interface;
	/* HDMI core */
	struct hdmi_core_vid_config v_core_cfg;
	struct hdmi_core_infoframe_avi *avi_cfg = &ip_data->avi_cfg;
	struct hdmi_config *cfg = &ip_data->cfg;
	struct hdmi_irq_vector irq_enable;

	hdmi_core_mask_interrupts(ip_data);

	hdmi_wp_init(&video_timing, &video_format, &irq_enable);

	hdmi_core_init(&v_core_cfg, avi_cfg, cfg);

	hdmi_wp_video_init_format(&video_format, &video_timing, cfg);

	hdmi_wp_video_config_timing(ip_data, &video_timing);

	/* video config */
	video_format.packing_mode = HDMI_PACK_24b_RGB_YUV444_YUV422;

	hdmi_wp_video_config_format(ip_data, &video_format);

	video_interface.vsp =
		!!(cfg->timings.sync & FB_SYNC_VERT_HIGH_ACT);
	video_interface.hsp =
		!!(cfg->timings.sync & FB_SYNC_HOR_HIGH_ACT);
	video_interface.interlacing =
		!!(cfg->timings.vmode & FB_VMODE_INTERLACED);
	video_interface.tm = cfg->deep_color + 1;

	hdmi_wp_video_config_interface(ip_data, &video_interface);


	if (ip_data->cfg.cm.mode == HDMI_DVI ||
	(ip_data->cfg.cm.code == 1 && ip_data->cfg.cm.mode == HDMI_HDMI)) {
		ip_data->cfg.range = HDMI_FULL_RANGE;
		ti_hdmi_5xxx_configure_range(ip_data);
	} else {
		ip_data->cfg.range = HDMI_LIMITED_RANGE;
		ti_hdmi_5xxx_configure_range(ip_data);
	}

	if (ip_data->cfg.cm.mode == HDMI_DVI ||
	(ip_data->cfg.cm.code == 1 && ip_data->cfg.cm.mode == HDMI_HDMI)) {
		ip_data->cfg.range = HDMI_FULL_RANGE;
		ti_hdmi_5xxx_configure_range(ip_data);
	} else {
		ip_data->cfg.range = HDMI_LIMITED_RANGE;
		ti_hdmi_5xxx_configure_range(ip_data);
	}

	/* Enable pll and core interrupts */
	irq_enable.pll_recal = 1;
	irq_enable.pll_unlock = 1;
	irq_enable.pll_lock = 1;
	irq_enable.phy_disconnect = 1;
	irq_enable.phy_connect = 1;
	irq_enable.phy_short_5v = 1;
	irq_enable.video_end_fr = 1;
	/* irq_enable.video_vsync = 1; */
	irq_enable.fifo_sample_req = 1;
	irq_enable.fifo_overflow = 1;
	irq_enable.fifo_underflow = 1;
	irq_enable.ocp_timeout = 1;
	irq_enable.core = 1;
	/* enable IRQ */
	hdmi_wp_irq_enable(ip_data, &irq_enable);
	/*
	 * configure core video part
	 * set software reset in the core
	 */
	v_core_cfg.packet_mode = HDMI_PACKETMODE24BITPERPIXEL;
	v_core_cfg.hdcp_keepout = 1;

	hdmi_core_video_config(ip_data, &v_core_cfg);

	hdmi_core_config_video_packetizer(ip_data);
	hdmi_core_config_csc(ip_data);
	hdmi_core_config_video_sampler(ip_data);

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
	avi_cfg->db3_nup_scaling = HDMI_INFOFRAME_AVI_DB3SC_NO;
	avi_cfg->db4_videocode = cfg->cm.code;
	avi_cfg->db5_pixel_repeat = HDMI_INFOFRAME_AVI_DB5PR_NO;
	avi_cfg->db6_7_line_eoftop = 0;
	avi_cfg->db8_9_line_sofbottom = 0;
	avi_cfg->db10_11_pixel_eofleft = 0;
	avi_cfg->db12_13_pixel_sofright = 0;

	hdmi_core_aux_infoframe_avi_config(ip_data);

	if (ip_data->cfg.s3d_info.vsi_enabled)
		hdmi_core_infoframe_vsi_config(ip_data, ip_data->cfg.s3d_info);

	hdmi_enable_video_path(ip_data);

	hdmi_core_enable_interrupts(ip_data);
}

int ti_hdmi_5xxx_cec_get_rx_cmd(struct hdmi_ip_data *ip_data,
	char *rx_cmd)
{
	int i = 1;
	int temp = 0;
	temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
		(HDMI_CORE_CEC_RX_DATA_0 + (i*4)));
	*rx_cmd = FLD_GET(temp, 7, 0);

	return 0;
}
int ti_hdmi_5xxx_cec_read_rx_cmd(struct hdmi_ip_data *ip_data,
	struct cec_rx_data *rx_data)
{
	int rx_byte_cnt;
	int temp;
	int i, j;
	int r = -EINVAL;

	temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
		HDMI_CORE_CEC_CTRL);

	if (FLD_GET(temp, 0, 0)) {
		pr_err("%s In transmission state\n", __func__);
		return -EBUSY;
	}
	/* TODO: Check follower id to the registerd device list */
	/* Check if frame is present*/
	temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
		HDMI_CORE_CEC_LOCK);
	if (temp) {
		/* Read total number of bytes received */
		rx_byte_cnt = hdmi_read_reg(hdmi_core_sys_base(ip_data),
				HDMI_CORE_CEC_RX_CNT);
		if (rx_byte_cnt) {
			if (rx_byte_cnt > (CEC_MAX_NUM_OPERANDS + CEC_HEADER_CMD_COUNT)) {
				pr_err(KERN_ERR "RX wrong num of operands\n");
				r = -EINVAL;
				goto error_exit;
			}
			/* Read the header */
			temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
				HDMI_CORE_CEC_RX_DATA_0);
			rx_data->init_device_id = FLD_GET(temp, 7, 4);
			rx_data->dest_device_id = FLD_GET(temp, 3, 0);
			/* Get the RX command */
			r = ti_hdmi_5xxx_cec_get_rx_cmd(ip_data,
				&rx_data->rx_cmd);
			if (r) {
				pr_err(KERN_ERR "RX Error in reading cmd\n");
				goto error_exit;
			}

			/*Get RX operands*/
			rx_data->rx_count = rx_byte_cnt - CEC_HEADER_CMD_COUNT;
			for (i = 2, j = 0; i < rx_byte_cnt; i++, j++) {
				temp = RD_REG_32(hdmi_core_sys_base(ip_data),
					HDMI_CORE_CEC_RX_DATA_0 + (i * 4));
				rx_data->rx_operand[j] = FLD_GET(temp, 7, 0);
			}
			hdmi_write_reg(hdmi_core_sys_base(ip_data),
				HDMI_CORE_CEC_RX_CNT, 0x0);
			r = 0;
		}
		/* Clear the LOCK register*/
		hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_LOCK,
			0x0);
	}
error_exit:
	return r;
}
int ti_hdmi_5xxx_cec_transmit_cmd(struct hdmi_ip_data *ip_data,
	struct cec_tx_data *data, int *cmd_acked)
{
	int r = -EINVAL;
	u32 temp, i = 0, j = 0, retry = 0;
	char count = 0;
	int cec_send_time = 0;
	int tx_state = 0;

	/*1.Clear interrupt status registers for TX.
	2. Set initiator Address, Set Destination Address
	3. Set the command, TX count
	4. Transmit
	5. Check for NACK / ACK - report the same. */

	do {
		/* Clear TX interrupt status */
		/* Clear DONE */
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_IH_CEC_STAT0, 0x1, 0, 0);
		/* Clear NACK */
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_IH_CEC_STAT0, 0x1, 2, 2);
		/* Clear ERR INITIATOR*/
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_IH_CEC_STAT0, 0x1, 4, 4);
		/* Write TX CMD HEADER*/
		/* Initiator ID*/
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_CEC_TX_DATA_0, data->initiator_device_id,
			7, 4);
		temp = data->initiator_device_id << 4;
		temp |= data->dest_device_id & 0xF;
		/* Destination ID*/
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_CEC_TX_DATA_0, temp, 7, 0);
		count = 1;/* Add header count to total TX count*/

		if (data->send_ping)
			goto send_ping;
		/*Write opcode*/
		i = 1;
		hdmi_write_reg(hdmi_core_sys_base(ip_data),
			(HDMI_CORE_CEC_TX_DATA_0 + (i * 4)), data->tx_cmd);
		count++;
		/*Write operands*/
		i++;
		for (j = 0; j < data->tx_count; j++, i++) {
			hdmi_write_reg(hdmi_core_sys_base(ip_data),
				(HDMI_CORE_CEC_TX_DATA_0 + (i * 4)),
				data->tx_operand[j]);
			count++;
		}
send_ping:
		/* Write TX count*/
		REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_TX_CNT,
			count, 4, 0);
		ti_hdmi_5xxx_cec_add_reg_device(ip_data,
			data->initiator_device_id, false);


		ip_data->cec_int = 0;
		/*Send CEC command*/
		REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_CTRL,
			0x03, 7, 0);
		/* Start bit ~4.6ms rounded to 5ms
		*  Header 8bit + EOM+ACK = 8 *2.4 + 2.4 + 2.4
		*  rounded to 10*2.5 = 25ms
		*  opcode + params = count*25ms */
		cec_send_time = 5 + 25 * (1 + count);
		/* Wait for CEC to send the message*/
		tx_state = wait_event_interruptible_timeout(ip_data->tx_complete,
			ip_data->cec_int & 0x1,
			msecs_to_jiffies(cec_send_time));

		if (tx_state && (tx_state != -ERESTARTSYS)) {
				/* Check for ACK/NACK */
				if (FLD_GET(ip_data->cec_int, 2, 2)) {
					*cmd_acked = 0;/* Retransmit */
					/* Clear NACK */
					REG_FLD_MOD(hdmi_core_sys_base(ip_data),
						HDMI_CORE_IH_CEC_STAT0, 0x1, 2,
						2);
				} else {
					*cmd_acked = 1;
					/* Exit TX loop*/
				}
				break;
		} else {
			/* Retransmit*/
			retry++;
		}
	} while (retry < data->retry_count);

	if (retry == data->retry_count)
		return r;
	else
		return 0;
}
int ti_hdmi_5xxx_power_on_cec(struct hdmi_ip_data *ip_data)
{
	 struct hdmi_irq_vector irq_enable;
	/* Initialize the CEC to STANDBY, respond only to ping */
	/* NACK broadcast messages */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_CTRL, 0x3, 4, 3);

	/* Clear All CEC interrupts */
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_IH_CEC_STAT0,
		0xFF);

	/* Set CEC interrupt mask */
	/* Enable WAKEUP,Follower line error,initiator error,
	ARD lost interrupt, NACK interrupt, EOM interrupt, TX done interrupt */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_MASK, 0x0, 7, 0);
	/*Unmute CEC interrupts*/
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_IH_MUTE, 0x0, 1, 0);
	/* On power on reset the registerd device to unregisterd*/
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_ADDR_L, 0x0);
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_ADDR_H, 0x80);

	/* Set TX count to zero */
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_TX_CNT, 0x0);

	/*Set Wake up for all supported opcode*/
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_WKUPCTRL,
		0xFF);

	/*Clear read LOCK*/
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CEC_LOCK, 0x0);

	/*Enable HDMI core interrupts*/
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_IRQENABLE_SET, 0x1,
		0, 0);
	irq_enable.core = 1;
	hdmi_wp_irq_enable(ip_data, &irq_enable);

	init_waitqueue_head(&ip_data->tx_complete);

	return 0;
}


int ti_hdmi_5xxx_power_off_cec(struct hdmi_ip_data *ip_data)
{
	/*disable CEC interrupts*/
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_IH_MUTE, 0x3, 1, 0);
	return 0;
}
EXPORT_SYMBOL(ti_hdmi_5xxx_power_off_cec);

int ti_hdmi_5xxx_cec_int_handler(struct hdmi_ip_data *ip_data)
{
	int temp = 0;
	int lock_val;
	int r = 0;

	temp = hdmi_read_reg(hdmi_core_sys_base(ip_data),
		HDMI_CORE_IH_CEC_STAT0);
	/*check wakeup*/
	if (FLD_GET(temp, 6, 6))
		r = FLD_GET(temp, 6, 6);
	/*check err follower*/
	if (FLD_GET(temp, 5, 5)) {
		/*Check the LOCK status*/
		lock_val = hdmi_read_reg(hdmi_core_sys_base(ip_data),
			HDMI_CORE_CEC_LOCK);
		if (lock_val)
			hdmi_write_reg(hdmi_core_sys_base(ip_data),
				HDMI_CORE_CEC_LOCK, 0x0);
	}
	if (FLD_GET(temp, 0, 0)) {
		ip_data->cec_int = temp;
		wake_up_interruptible(&ip_data->tx_complete);
	}
	if (FLD_GET(temp, 2, 2)) {
		ip_data->cec_int = temp;
		wake_up_interruptible(&ip_data->tx_complete);
	}
	/* Get EOM status*/
	r |= FLD_GET(temp, 1, 1);
	/*Clear all the CEC interrupts*/
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_IH_CEC_STAT0,
		0xff, 7, 0);

	return r;
}
int ti_hdmi_5xxx_cec_clr_rx_int(struct hdmi_ip_data *ip_data, int cec_rx)
{
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_IH_CEC_STAT0, 0x1,
		1, 1);
	return 0;
}
int ti_hdmi_5xxx_cec_get_reg_device_list(struct hdmi_ip_data *ip_data)
{
	int dev_mask = 0;

	/*Store current device listning ids*/
	dev_mask = RD_REG_32(hdmi_core_sys_base(ip_data),
		HDMI_CORE_CEC_ADDR_H);
	dev_mask <<= 8;
	dev_mask |= RD_REG_32(hdmi_core_sys_base(ip_data),
		HDMI_CORE_CEC_ADDR_L);
	return dev_mask;

}
int ti_hdmi_5xxx_cec_add_reg_device(struct hdmi_ip_data *ip_data,
	int device_id, int clear)
{
	u32 temp, regis_reg, shift_cnt;

	/* Register to receive messages intended for this device
	and broad cast messages */
	regis_reg = HDMI_CORE_CEC_ADDR_L;
	shift_cnt = device_id;
	temp = 0;
	if (device_id > 0x7) {
		regis_reg = HDMI_CORE_CEC_ADDR_H;
		shift_cnt -= 0x7;
	}
	if (clear == 0x1) {
		WR_REG_32(hdmi_core_sys_base(ip_data),
			HDMI_CORE_CEC_ADDR_L, 0);
		WR_REG_32(hdmi_core_sys_base(ip_data),
			HDMI_CORE_CEC_ADDR_H, 0);
	} else {
		temp = RD_REG_32(hdmi_core_sys_base(ip_data), regis_reg);
	}
	temp |= 0x1 << shift_cnt;
	WR_REG_32(hdmi_core_sys_base(ip_data), regis_reg,
		temp);
	return 0;

}
int ti_hdmi_5xxx_cec_set_reg_device_list(struct hdmi_ip_data *ip_data,
	int mask)
{
	WR_REG_32(hdmi_core_sys_base(ip_data),
		HDMI_CORE_CEC_ADDR_H, (mask >> 8) & 0xff);
	WR_REG_32(hdmi_core_sys_base(ip_data),
		HDMI_CORE_CEC_ADDR_L, mask & 0xff);
	return 0;
}

#if defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
static void ti_hdmi_5xxx_wp_audio_config_format(struct hdmi_ip_data *ip_data,
					struct hdmi_audio_format *aud_fmt)
{
	u32 r;

	DSSDBG("Enter hdmi_wp_audio_config_format\n");
	r = hdmi_read_reg(ip_data->base_wp, HDMI_WP_AUDIO_CFG);
	r = FLD_MOD(r, aud_fmt->en_sig_blk_strt_end, 5, 5);
	r = FLD_MOD(r, aud_fmt->type, 4, 4);
	r = FLD_MOD(r, aud_fmt->justification, 3, 3);
	r = FLD_MOD(r, aud_fmt->samples_per_word, 1, 1);
	r = FLD_MOD(r, aud_fmt->sample_size, 0, 0);
	hdmi_write_reg(ip_data->base_wp, HDMI_WP_AUDIO_CFG, r);
}

static void ti_hdmi_5xxx_core_audio_config(struct hdmi_ip_data *ip_data,
					struct hdmi_core_audio_config *cfg)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	u8 val;

	/* Mute audio before configuring */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCONF, 0xf, 7, 4);

	/* Set the N parameter */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_N1, cfg->n, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_N2, cfg->n >> 8, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_N3, cfg->n >> 16, 3, 0);

	/*
	 * CTS manual mode. Automatic mode is not supported
	 * when using audio parallel interface.
	 */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_CTS3, 1, 4, 4);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_CTS1, cfg->cts, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_CTS2, cfg->cts >> 8, 7, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_CTS3,
		cfg->cts >> 16, 3, 0);

	/* Layout of Audio Sample Packets: 2-channel */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCONF, cfg->layout, 0, 0);

	/* Configure IEC-609580 Validity bits */
	/* Channel 0 is valid */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 0, 0, 0);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 0, 4, 4);
	/* Channels 1, 2, 3 are not valid */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 1, 1, 1);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 1, 5, 5);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 1, 2, 2);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 1, 6, 6);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 1, 3, 3);
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSV, 1, 7, 7);

	/* Configure IEC-60958 User bits */
	/* TODO: should be set by user. */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSU, 0, 7, 0);

	/* Configure IEC-60958 Channel Status word */
	/* CGMSA */
	val = cfg->iec60958_cfg->status[5] & IEC958_AES5_CON_CGMSA;
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(0), val, 5, 4);

	/* Copyright */
	val = (cfg->iec60958_cfg->status[0] &
			IEC958_AES0_CON_NOT_COPYRIGHT)>>2;
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(0), val, 0, 0);

	/* Category */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(1),
		cfg->iec60958_cfg->status[1]);

	/* PCM audio mode */
	val = (cfg->iec60958_cfg->status[0] & IEC958_AES0_CON_MODE)>>6;
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(2), val, 6, 4);

	/* Source number */
	val = cfg->iec60958_cfg->status[2] & IEC958_AES2_CON_SOURCE;
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(2), val, 3, 4);

	/* Channel number right 0  */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(3), 2, 3, 0);
	/* Channel number right 1*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(3), 4, 7, 4);
	/* Channel number right 2  */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(4), 6, 3, 0);
	/* Channel number right 3*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(4), 8, 7, 4);
	/* Channel number left 0  */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(5), 1, 3, 0);
	/* Channel number left 1*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(5), 3, 7, 4);
	/* Channel number left 2  */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(6), 5, 3, 0);
	/* Channel number left 3*/
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(6), 7, 7, 4);

	/* Clock accuracy and sample rate */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(7),
		cfg->iec60958_cfg->status[3]);

	/* Original sample rate and word length */
	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDSCHNLS(8),
		cfg->iec60958_cfg->status[4]);

	/* Enable FIFO empty and full interrupts */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_INT, 3, 3, 2);

	/* Configure GPA */
	/* select HBR/SPDIF interfaces */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_CONF0, 0, 5, 5);
	/* enable two channels in GPA */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_GP_CONF1, 3, 7, 0);
	/* disable HBR */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_GP_CONF2, 0, 0, 0);
	/* Enable GPA FIFO full and empty mask */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_GP_MASK, 3, 1, 0);
	/* Set polarity of GPA FIFO empty interrupts */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_AUD_GB_POL, 1, 0, 0);

	/*Unmute audio */
	REG_FLD_MOD(core_sys_base, HDMI_CORE_FC_AUDSCONF, 0, 7, 4);
}

static void ti_hdmi_5xxx_core_audio_infoframe_cfg
	(struct hdmi_ip_data *ip_data,
	 struct snd_cea_861_aud_if *info_aud)
{
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDICONF0,
		info_aud->db1_ct_cc);

	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDICONF1,
		info_aud->db2_sf_ss);

	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDICONF2,
		info_aud->db4_ca);

	hdmi_write_reg(core_sys_base, HDMI_CORE_FC_AUDICONF3,
		info_aud->db5_dminh_lsv);
}

int ti_hdmi_5xxx_audio_config(struct hdmi_ip_data *ip_data,
		struct snd_aes_iec958 *iec, struct snd_cea_861_aud_if *aud_if)
{
	struct hdmi_audio_format audio_format;
	struct hdmi_audio_dma audio_dma;
	struct hdmi_core_audio_config core;
	int err, n, cts, channel_count;
	unsigned int fs_nr;
	bool word_length_16b = false;

	if (!iec || !aud_if || !ip_data)
		return -EINVAL;

	core.iec60958_cfg = iec;

	if (!(iec->status[4] & IEC958_AES4_CON_MAX_WORDLEN_24))
		if (iec->status[4] & IEC958_AES4_CON_WORDLEN_20_16)
			word_length_16b = true;

	/* only 16-bit word length supported atm */
	if (!word_length_16b)
		return -EINVAL;

	/* only 44.1kHz supported atm */
	switch (iec->status[3] & IEC958_AES3_CON_FS) {
	case IEC958_AES3_CON_FS_44100:
		fs_nr = 44100;
		break;
	default:
		return -EINVAL;
	}

	err = hdmi_compute_acr(fs_nr, &n, &cts);
	core.n = n;
	core.cts = cts;

	/* Audio channels settings */
	channel_count = (aud_if->db1_ct_cc & CEA861_AUDIO_INFOFRAME_DB1CC) + 1;

	/* only 2 channels supported atm */
	if (channel_count != 2)
		return -EINVAL;

	core.layout = HDMI_AUDIO_LAYOUT_2CH;

	/* DMA settings */
	if (word_length_16b)
		audio_dma.transfer_size = 0x10;
	else
		audio_dma.transfer_size = 0x20;
	audio_dma.block_size = 0xC0;
	audio_dma.mode = HDMI_AUDIO_TRANSF_DMA;
	audio_dma.fifo_threshold = 0x20; /* in number of samples */

	/* audio FIFO format settings for 16-bit samples*/
	audio_format.samples_per_word = HDMI_AUDIO_ONEWORD_TWOSAMPLES;
	audio_format.sample_size = HDMI_AUDIO_SAMPLE_16BITS;
	audio_format.justification = HDMI_AUDIO_JUSTIFY_LEFT;

	/* only LPCM atm */
	audio_format.type = HDMI_AUDIO_TYPE_LPCM;

	/* disable start/stop signals of IEC 60958 blocks */
	audio_format.en_sig_blk_strt_end = HDMI_AUDIO_BLOCK_SIG_STARTEND_ON;

	/* configure DMA and audio FIFO format*/
	ti_hdmi_4xxx_wp_audio_config_dma(ip_data, &audio_dma);
	ti_hdmi_5xxx_wp_audio_config_format(ip_data, &audio_format);

	/* configure the core*/
	ti_hdmi_5xxx_core_audio_config(ip_data, &core);

	/* configure CEA 861 audio infoframe*/
	ti_hdmi_5xxx_core_audio_infoframe_cfg(ip_data, aud_if);

	return 0;
}

void ti_hdmi_5xxx_wp_audio_enable(struct hdmi_ip_data *ip_data, bool enable)
{
	REG_FLD_MOD(ip_data->base_wp,
		HDMI_WP_AUDIO_CTRL, enable, 31, 31);
}

void ti_hdmi_5xxx_audio_start(struct hdmi_ip_data *ip_data, bool enable)
{
	REG_FLD_MOD(ip_data->base_wp,
		HDMI_WP_AUDIO_CTRL, enable, 30, 30);
}

#endif
