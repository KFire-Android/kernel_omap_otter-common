/*
 * ti_hdmi_4xxx_ip.c
 *
 * HDMI TI81xx, TI38xx, TI OMAP4 etc IP driver Library
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com/
 * Authors: Yong Zhi
 *	Mythri pk <mythripk@ti.com>
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/omapfb.h>
#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
#include <sound/asoundef.h>
#endif
#include "ti_hdmi_4xxx_ip.h"
#include "dss.h"
#include "dss_features.h"

#define CEC_MAX_NUM_OPERANDS   14
#define HDMI_CORE_CEC_RETRY    200
#define HDMI_CEC_TX_CMD_RETRY  400

#define HDMI_WP_SYSCONFIG_SMART_IDLE 0x3

static inline void hdmi_write_reg(void __iomem *base_addr,
				const u16 idx, u32 val)
{
	__raw_writel(val, base_addr + idx);
}

static inline u32 hdmi_read_reg(void __iomem *base_addr,
				const u16 idx)
{
	return __raw_readl(base_addr + idx);
}

static inline void __iomem *hdmi_wp_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp;
}

static inline void __iomem *hdmi_phy_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp + ip_data->phy_offset;
}

static inline void __iomem *hdmi_pll_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp + ip_data->pll_offset;
}

static inline void __iomem *hdmi_av_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp + ip_data->core_av_offset;
}

static inline void __iomem *hdmi_core_sys_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp + ip_data->core_sys_offset;
}

static inline void __iomem *hdmi_core_cec_base(struct hdmi_ip_data *ip_data)
{
	return ip_data->base_wp + ip_data->cec_offset;
}


static inline int hdmi_wait_for_bit_change(void __iomem *base_addr,
				const u16 idx,
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

static int hdmi_pll_init(struct hdmi_ip_data *ip_data)
{
	u32 r;
	void __iomem *pll_base = hdmi_pll_base(ip_data);
	struct hdmi_pll_info *fmt = &ip_data->pll_data;

	/* PLL start always use manual mode */
	REG_FLD_MOD(pll_base, PLLCTRL_PLL_CONTROL, 0x0, 0, 0);

	r = hdmi_read_reg(pll_base, PLLCTRL_CFG1);
	r = FLD_MOD(r, fmt->regm, 20, 9); /* CFG1_PLL_REGM */
	r = FLD_MOD(r, fmt->regn - 1, 8, 1);  /* CFG1_PLL_REGN */

	hdmi_write_reg(pll_base, PLLCTRL_CFG1, r);

	r = hdmi_read_reg(pll_base, PLLCTRL_CFG2);

	r = FLD_MOD(r, 0x0, 12, 12); /* PLL_HIGHFREQ divide by 2 */
	r = FLD_MOD(r, 0x1, 13, 13); /* PLL_REFEN */
	r = FLD_MOD(r, 0x0, 14, 14); /* PHY_CLKINEN de-assert during locking */
	r = FLD_MOD(r, fmt->refsel, 22, 21); /* REFSEL */

	if (fmt->dcofreq) {
		/* divider programming for frequency beyond 1000Mhz */
		REG_FLD_MOD(pll_base, PLLCTRL_CFG3, fmt->regsd, 17, 10);
		r = FLD_MOD(r, 0x4, 3, 1); /* 1000MHz and 2000MHz */
	} else {
		r = FLD_MOD(r, 0x2, 3, 1); /* 500MHz and 1000MHz */
	}

	hdmi_write_reg(pll_base, PLLCTRL_CFG2, r);

	r = hdmi_read_reg(pll_base, PLLCTRL_CFG4);
	r = FLD_MOD(r, fmt->regm2, 24, 18);
	r = FLD_MOD(r, fmt->regmf, 17, 0);

	hdmi_write_reg(pll_base, PLLCTRL_CFG4, r);

	/* go now */
	REG_FLD_MOD(pll_base, PLLCTRL_PLL_GO, 0x1, 0, 0);

	/* wait for bit change */
	if (hdmi_wait_for_bit_change(pll_base, PLLCTRL_PLL_GO,
							0, 0, 1) != 1) {
		pr_err("PLL GO bit not set\n");
		return -ETIMEDOUT;
	}

	/* Wait till the lock bit is set in PLL status */
	if (hdmi_wait_for_bit_change(pll_base,
				PLLCTRL_PLL_STATUS, 1, 1, 1) != 1) {
		pr_err("cannot lock PLL\n");
		pr_err("CFG1 0x%x\n",
			hdmi_read_reg(pll_base, PLLCTRL_CFG1));
		pr_err("CFG2 0x%x\n",
			hdmi_read_reg(pll_base, PLLCTRL_CFG2));
		pr_err("CFG4 0x%x\n",
			hdmi_read_reg(pll_base, PLLCTRL_CFG4));
		return -ETIMEDOUT;
	}

	pr_debug("PLL locked!\n");

	return 0;
}

static int hdmi_wait_for_audio_stop(struct hdmi_ip_data *ip_data)
{
	int count = 0;

	/* wait for audio to stop before powering off the phy*/
	while (REG_GET(hdmi_wp_base(ip_data),
			HDMI_WP_AUDIO_CTRL, 31, 31) != 0) {
		msleep(100);
		if (count++ > 100) {
			pr_err("Audio is not turned off "
					"even after 10 seconds\n");
			return -ETIMEDOUT;
		}
	}
	return 0;
}

/* PHY_PWR_CMD */
static int hdmi_set_phy_pwr(struct hdmi_ip_data *ip_data, enum hdmi_phy_pwr val)
{
	if (val == HDMI_PHYPWRCMD_OFF && !(ip_data->set_mode))
		hdmi_wait_for_audio_stop(ip_data);

	/* Command for power control of HDMI PHY */
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_PWR_CTRL, val, 7, 6);

	/* Status of the power control of HDMI PHY */
	if (hdmi_wait_for_bit_change(hdmi_wp_base(ip_data),
				HDMI_WP_PWR_CTRL, 5, 4, val) != val) {
		pr_err("Failed to set PHY power mode to %d\n", val);
		return -ETIMEDOUT;
	}

	return 0;
}

/* PLL_PWR_CMD */
static int hdmi_set_pll_pwr(struct hdmi_ip_data *ip_data, enum hdmi_pll_pwr val)
{
	/* Command for power control of HDMI PLL */
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_PWR_CTRL, val, 3, 2);

	/* wait till PHY_PWR_STATUS is set */
	if (hdmi_wait_for_bit_change(hdmi_wp_base(ip_data), HDMI_WP_PWR_CTRL,
						1, 0, val) != val) {
		pr_err("Failed to set PLL_PWR_STATUS\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int hdmi_pll_reset(struct hdmi_ip_data *ip_data)
{
	/* SYSRESET  controlled by power FSM */
	if (cpu_is_omap44xx())
		REG_FLD_MOD(hdmi_pll_base(ip_data),
				PLLCTRL_PLL_CONTROL, 0x0, 3, 3);
	else
		REG_FLD_MOD(hdmi_pll_base(ip_data),
				PLLCTRL_PLL_CONTROL, 0x1, 3, 3);

	/* READ 0x0 reset is in progress */
	if (hdmi_wait_for_bit_change(hdmi_pll_base(ip_data),
				PLLCTRL_PLL_STATUS, 0, 0, 1) != 1) {
		pr_err("Failed to sysreset PLL\n");
		return -ETIMEDOUT;
	}

	return 0;
}

int ti_hdmi_4xxx_pll_enable(struct hdmi_ip_data *ip_data)
{
	u16 r = 0;

	r = hdmi_set_pll_pwr(ip_data, HDMI_PLLPWRCMD_ALLOFF);
	if (r)
		return r;

	r = hdmi_set_pll_pwr(ip_data, HDMI_PLLPWRCMD_BOTHON_ALLCLKS);
	if (r)
		return r;

	r = hdmi_pll_reset(ip_data);
	if (r)
		return r;

	r = hdmi_pll_init(ip_data);
	if (r)
		return r;

	return 0;
}

void ti_hdmi_4xxx_pll_disable(struct hdmi_ip_data *ip_data)
{
	hdmi_set_pll_pwr(ip_data, HDMI_PLLPWRCMD_ALLOFF);
}

int ti_hdmi_4xxx_set_phy_on_hpd(struct hdmi_ip_data *ip_data, bool hpd)
{
	unsigned long flags;
	int r;
	/* this should be in ti_hdmi_4xxx_ip private data */
	static DEFINE_SPINLOCK(phy_tx_lock);

	spin_lock_irqsave(&phy_tx_lock, flags);


	if (hpd == ip_data->phy_tx_enabled) {
		spin_unlock_irqrestore(&phy_tx_lock, flags);
		return 0;
	}

	ip_data->set_mode = false;
	if (hpd)
		r = hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_TXON);
	else
		r = hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_LDOON);
	/*
	 * HDMI_WP_PWR_CTRL doesn't seem to reflect the change in power
	 * states, ignore the error for now
	 */
#if 0
	if (r) {
		DSSERR("Failed to %s PHY TX power\n",
				hpd ? "enable" : "disable");
		goto err;
	}
#endif
	ip_data->phy_tx_enabled = hpd;
#if 0
err:
#endif
	spin_unlock_irqrestore(&phy_tx_lock, flags);
	return r;
}

int ti_hdmi_4xxx_phy_enable(struct hdmi_ip_data *ip_data)
{
	u16 r = 0;
	void __iomem *phy_base = hdmi_phy_base(ip_data);
	unsigned long pclk = ip_data->cfg.timings.pixclock;
	u16 freqout = 1;
	bool hpd;
	pclk = pclk ? PICOS2KHZ(pclk) : 0;

	r = hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_LDOON);

	/*
	 * HDMI_WP_PWR_CTRL doesn't seem to reflect the change in power
	 * states, ignore the error for now
	 */
#if 0
	if (r)
		return r;
#endif

	/*
	 * Read address 0 in order to get the SCP reset done completed
	 * Dummy access performed to make sure reset is done
	 */
	hdmi_read_reg(phy_base, HDMI_TXPHY_TX_CTRL);

	/*
	 * Write to phy address 0 to configure the clock
	 * use HFBITCLK write HDMI_TXPHY_TX_CONTROL_FREQOUT field
	 */
	if (cpu_is_omap44xx()) {
		REG_FLD_MOD(phy_base, HDMI_TXPHY_TX_CTRL, 0x1, 31, 30);
	} else if (cpu_is_omap54xx()) {
		if (pclk < 62500) {
			freqout = 0;
		} else if ((pclk >= 62500) && (pclk < 185000)) {
			freqout = 1;
		} else {
			/* clock frequency > 185MHz */
			freqout = 2;
		}
		REG_FLD_MOD(phy_base, HDMI_TXPHY_TX_CTRL, freqout, 31, 30);
	}

	/* Write to phy address 1 to start HDMI line (TXVALID and TMDSCLKEN) */
	hdmi_write_reg(phy_base, HDMI_TXPHY_DIGITAL_CTRL, 0xF0000000);

	/* Setup max LDO voltage */
	/* This register need not be set for OMAP5 as it is unused*/
	if (cpu_is_omap44xx())
		REG_FLD_MOD(phy_base, HDMI_TXPHY_POWER_CTRL, 0xB, 3, 0);

	/* Write to phy address 3 to change the polarity control */
	REG_FLD_MOD(phy_base, HDMI_TXPHY_PAD_CFG_CTRL, 0x1, 27, 27);

	hpd = gpio_get_value(ip_data->hpd_gpio);

	r = ti_hdmi_4xxx_set_phy_on_hpd(ip_data, hpd);
	if (r) {
		hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_OFF);
		return r;
	}

	/* enable divby2 */
	if (cpu_is_omap54xx())
		REG_FLD_MOD(phy_base, HDMI_TXPHY_BIST_CONTROL, 1, 11, 11);

	return 0;
}

void ti_hdmi_4xxx_phy_disable(struct hdmi_ip_data *ip_data)
{

	hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_OFF);
	ip_data->phy_tx_enabled = false;
}

int hdmi_ti_4xxx_set_wait_soft_reset(struct hdmi_ip_data *ip_data)
{

	/* reset Wrapper */
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_SYSCONFIG, 0x1, 0, 0);

	/* wait till SOFTRESET == 0 */
	if (hdmi_wait_for_bit_change(hdmi_wp_base(ip_data),
					HDMI_WP_SYSCONFIG, 0, 0, 0) != 0) {
		pr_err("SYSCONFIG[SOFTRESET] bit not set to 0\n");
		return -ETIMEDOUT;
	}

	/* Make madule smart and wakeup capable*/
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_SYSCONFIG,
		HDMI_WP_SYSCONFIG_SMART_IDLE, 3, 2);

	return 0;
}

int hdmi_ti_4xxx_wp_get_video_state(struct hdmi_ip_data *ip_data)
{
	return REG_GET(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG, 31, 31);
}

static int hdmi_core_ddc_init(struct hdmi_ip_data *ip_data)
{
	void __iomem *base = hdmi_core_sys_base(ip_data);

	/* Turn on CLK for DDC */
	REG_FLD_MOD(base, HDMI_CORE_AV_DPD, 0x7, 2, 0);

	/* IN_PROG */
	if (REG_GET(base, HDMI_CORE_DDC_STATUS, 4, 4) == 1) {
		/* Abort transaction */
		REG_FLD_MOD(base, HDMI_CORE_DDC_CMD, 0xf, 3, 0);
		/* IN_PROG */
		if (hdmi_wait_for_bit_change(base, HDMI_CORE_DDC_STATUS,
					4, 4, 0) != 0) {
			DSSERR("Timeout aborting DDC transaction\n");
			return -ETIMEDOUT;
		}
	}

	/* Clk SCL Devices */
	REG_FLD_MOD(base, HDMI_CORE_DDC_CMD, 0xA, 3, 0);

	/* HDMI_CORE_DDC_STATUS_IN_PROG */
	if (hdmi_wait_for_bit_change(base, HDMI_CORE_DDC_STATUS,
				4, 4, 0) != 0) {
		DSSERR("Timeout starting SCL clock\n");
		return -ETIMEDOUT;
	}

	/* Clear FIFO */
	REG_FLD_MOD(base, HDMI_CORE_DDC_CMD, 0x9, 3, 0);

	/* HDMI_CORE_DDC_STATUS_IN_PROG */
	if (hdmi_wait_for_bit_change(base, HDMI_CORE_DDC_STATUS,
				4, 4, 0) != 0) {
		DSSERR("Timeout clearing DDC fifo\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int hdmi_core_ddc_edid(struct hdmi_ip_data *ip_data,
		u8 *pedid, int ext)
{
	void __iomem *base = hdmi_core_sys_base(ip_data);
	u32 i;
	char checksum;
	u32 offset = 0;

	/* HDMI_CORE_DDC_STATUS_IN_PROG */
	if (hdmi_wait_for_bit_change(base, HDMI_CORE_DDC_STATUS,
				4, 4, 0) != 0) {
		DSSERR("Timeout waiting DDC to be ready\n");
		return -ETIMEDOUT;
	}

	if (ext % 2 != 0)
		offset = 0x80;

	/* Load Segment Address Register */
	REG_FLD_MOD(base, HDMI_CORE_DDC_SEGM, ext / 2, 7, 0);

	/* Load Slave Address Register */
	REG_FLD_MOD(base, HDMI_CORE_DDC_ADDR, 0xA0 >> 1, 7, 1);

	/* Load Offset Address Register */
	REG_FLD_MOD(base, HDMI_CORE_DDC_OFFSET, offset, 7, 0);

	/* Load Byte Count */
	REG_FLD_MOD(base, HDMI_CORE_DDC_COUNT1, 0x80, 7, 0);
	REG_FLD_MOD(base, HDMI_CORE_DDC_COUNT2, 0x0, 1, 0);

	/* Set DDC_CMD */
	if (ext)
		REG_FLD_MOD(base, HDMI_CORE_DDC_CMD, 0x4, 3, 0);
	else
		REG_FLD_MOD(base, HDMI_CORE_DDC_CMD, 0x2, 3, 0);

	/* HDMI_CORE_DDC_STATUS_BUS_LOW */
	if (REG_GET(base, HDMI_CORE_DDC_STATUS, 6, 6) == 1) {
		pr_err("I2C Bus Low?\n");
		return -EIO;
	}
	/* HDMI_CORE_DDC_STATUS_NO_ACK */
	if (REG_GET(base, HDMI_CORE_DDC_STATUS, 5, 5) == 1) {
		pr_err("I2C No Ack\n");
		return -EIO;
	}

	for (i = 0; i < 0x80; ++i) {
		int t;

		/* IN_PROG */
		if (REG_GET(base, HDMI_CORE_DDC_STATUS, 4, 4) == 0) {
			DSSERR("operation stopped when reading edid\n");
			return -EIO;
		}

		t = 0;
		/* FIFO_EMPTY */
		while (REG_GET(base, HDMI_CORE_DDC_STATUS, 2, 2) == 1) {
			if (t++ > 10000) {
				DSSERR("timeout reading edid\n");
				return -ETIMEDOUT;
			}
			udelay(1);
		}

		pedid[i] = REG_GET(base, HDMI_CORE_DDC_DATA, 7, 0);
	}

	checksum = 0;
	for (i = 0; i < 0x80; ++i)
		checksum += pedid[i];

	if (checksum != 0) {
		pr_err("E-EDID checksum failed!!\n");
		return -EIO;
	}

	return 0;
}

int ti_hdmi_4xxx_read_edid(struct hdmi_ip_data *ip_data,
				u8 *edid, int len)
{
	int r, l;

	if (len < 128)
		return -EINVAL;

	r = hdmi_core_ddc_init(ip_data);
	if (r)
		return r;

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

	return l;
}

bool ti_hdmi_4xxx_detect(struct hdmi_ip_data *ip_data)
{
	return gpio_get_value(ip_data->hpd_gpio);
}

static void hdmi_core_init(struct hdmi_core_video_config *video_cfg,
			struct hdmi_core_infoframe_avi *avi_cfg,
			struct hdmi_core_packet_enable_repeat *repeat_cfg)
{
	pr_debug("Enter hdmi_core_init\n");

	/* video core */
	video_cfg->ip_bus_width = HDMI_INPUT_8BIT;
	video_cfg->op_dither_truc = HDMI_OUTPUTTRUNCATION_8BIT;
	video_cfg->deep_color_pkt = HDMI_DEEPCOLORPACKECTDISABLE;
	video_cfg->pkt_mode = HDMI_PACKETMODERESERVEDVALUE;
	video_cfg->hdmi_dvi = HDMI_DVI;
	video_cfg->tclk_sel_clkmult = HDMI_FPLL10IDCK;

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

	/* packet enable and repeat */
	repeat_cfg->audio_pkt = 0;
	repeat_cfg->audio_pkt_repeat = 0;
	repeat_cfg->avi_infoframe = 0;
	repeat_cfg->avi_infoframe_repeat = 0;
	repeat_cfg->gen_cntrl_pkt = 0;
	repeat_cfg->gen_cntrl_pkt_repeat = 0;
	repeat_cfg->generic_pkt = 0;
	repeat_cfg->generic_pkt_repeat = 0;
}

static void hdmi_core_powerdown_disable(struct hdmi_ip_data *ip_data)
{
	pr_debug("Enter hdmi_core_powerdown_disable\n");
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_CTRL1, 0x0, 0, 0);
}

static void hdmi_core_swreset_release(struct hdmi_ip_data *ip_data)
{
	pr_debug("Enter hdmi_core_swreset_release\n");
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_SYS_SRST, 0x0, 0, 0);
}

static void hdmi_core_swreset_assert(struct hdmi_ip_data *ip_data)
{
	pr_debug("Enter hdmi_core_swreset_assert\n");
	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_SYS_SRST, 0x1, 0, 0);
}

/* HDMI_CORE_VIDEO_CONFIG */
static void hdmi_core_video_config(struct hdmi_ip_data *ip_data,
				struct hdmi_core_video_config *cfg)
{
	u32 r = 0;
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);

	/* sys_ctrl1 default configuration not tunable */
	r = hdmi_read_reg(core_sys_base, HDMI_CORE_CTRL1);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_VEN_FOLLOWVSYNC, 5, 5);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_HEN_FOLLOWHSYNC, 4, 4);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_BSEL_24BITBUS, 2, 2);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_EDGE_RISINGEDGE, 1, 1);
	hdmi_write_reg(core_sys_base, HDMI_CORE_CTRL1, r);

	REG_FLD_MOD(core_sys_base,
			HDMI_CORE_SYS_VID_ACEN, cfg->ip_bus_width, 7, 6);

	/* Vid_Mode */
	r = hdmi_read_reg(core_sys_base, HDMI_CORE_SYS_VID_MODE);

	/* dither truncation configuration */
	if (cfg->op_dither_truc > HDMI_OUTPUTTRUNCATION_12BIT) {
		r = FLD_MOD(r, cfg->op_dither_truc - 3, 7, 6);
		r = FLD_MOD(r, 1, 5, 5);
	} else {
		r = FLD_MOD(r, cfg->op_dither_truc, 7, 6);
		r = FLD_MOD(r, 0, 5, 5);
	}
	hdmi_write_reg(core_sys_base, HDMI_CORE_SYS_VID_MODE, r);

	/* HDMI_Ctrl */
	r = hdmi_read_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_HDMI_CTRL);
	r = FLD_MOD(r, cfg->deep_color_pkt, 6, 6);
	r = FLD_MOD(r, cfg->pkt_mode, 5, 3);
	r = FLD_MOD(r, cfg->hdmi_dvi, 0, 0);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_HDMI_CTRL, r);

	/* TMDS_CTRL */
	REG_FLD_MOD(core_sys_base,
			HDMI_CORE_SYS_TMDS_CTRL, cfg->tclk_sel_clkmult, 6, 5);
}

static void hdmi_core_aux_infoframe_avi_config(struct hdmi_ip_data *ip_data)
{
	u32 val;
	char sum = 0, checksum = 0;
	void __iomem *av_base = hdmi_av_base(ip_data);
	struct hdmi_core_infoframe_avi info_avi = ip_data->avi_cfg;

	sum += 0x82 + 0x002 + 0x00D;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_TYPE, 0x082);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_VERS, 0x002);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_LEN, 0x00D);

	val = (info_avi.db1_format << 5) |
		(info_avi.db1_active_info << 4) |
		(info_avi.db1_bar_info_dv << 2) |
		(info_avi.db1_scan_info);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(0), val);
	sum += val;

	val = (info_avi.db2_colorimetry << 6) |
		(info_avi.db2_aspect_ratio << 4) |
		(info_avi.db2_active_fmt_ar);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(1), val);
	sum += val;

	val = (info_avi.db3_itc << 7) |
		(info_avi.db3_ec << 4) |
		(info_avi.db3_q_range << 2) |
		(info_avi.db3_nup_scaling);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(2), val);
	sum += val;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(3),
					info_avi.db4_videocode);
	sum += info_avi.db4_videocode;

	val = info_avi.db5_pixel_repeat;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(4), val);
	sum += val;

	val = info_avi.db6_7_line_eoftop & 0x00FF;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(5), val);
	sum += val;

	val = ((info_avi.db6_7_line_eoftop >> 8) & 0x00FF);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(6), val);
	sum += val;

	val = info_avi.db8_9_line_sofbottom & 0x00FF;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(7), val);
	sum += val;

	val = ((info_avi.db8_9_line_sofbottom >> 8) & 0x00FF);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(8), val);
	sum += val;

	val = info_avi.db10_11_pixel_eofleft & 0x00FF;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(9), val);
	sum += val;

	val = ((info_avi.db10_11_pixel_eofleft >> 8) & 0x00FF);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(10), val);
	sum += val;

	val = info_avi.db12_13_pixel_sofright & 0x00FF;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(11), val);
	sum += val;

	val = ((info_avi.db12_13_pixel_sofright >> 8) & 0x00FF);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_DBYTE(12), val);
	sum += val;

	checksum = 0x100 - sum;
	hdmi_write_reg(av_base, HDMI_CORE_AV_AVI_CHSUM, checksum);
}

static void hdmi_core_av_packet_config(struct hdmi_ip_data *ip_data,
		struct hdmi_core_packet_enable_repeat repeat_cfg)
{
	/* enable/repeat the infoframe */
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_PB_CTRL1,
		(repeat_cfg.audio_pkt << 5) |
		(repeat_cfg.audio_pkt_repeat << 4) |
		(repeat_cfg.avi_infoframe << 1) |
		(repeat_cfg.avi_infoframe_repeat));

	/* enable/repeat the packet */
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_PB_CTRL2,
		(repeat_cfg.gen_cntrl_pkt << 3) |
		(repeat_cfg.gen_cntrl_pkt_repeat << 2) |
		(repeat_cfg.generic_pkt << 1) |
		(repeat_cfg.generic_pkt_repeat));
}

void hdmi_wp_init(struct omap_video_timings *timings,
			struct hdmi_video_format *video_fmt,
			struct hdmi_irq_vector *irq_enable)
{
	pr_debug("Enter hdmi_wp_init\n");

	timings->hbp = 0;
	timings->hfp = 0;
	timings->hsw = 0;
	timings->vbp = 0;
	timings->vfp = 0;
	timings->vsw = 0;

	video_fmt->packing_mode = HDMI_PACK_10b_RGB_YUV444;
	video_fmt->y_res = 0;
	video_fmt->x_res = 0;

	irq_enable->pll_recal = 0;
	irq_enable->pll_unlock = 0;
	irq_enable->pll_lock = 0;
	irq_enable->phy_disconnect = 0;
	irq_enable->phy_connect = 0;
	irq_enable->phy_short_5v = 0;
	irq_enable->video_end_fr = 0;
	irq_enable->video_vsync = 0;
	irq_enable->fifo_sample_req = 0;
	irq_enable->fifo_overflow = 0;
	irq_enable->fifo_underflow = 0;
	irq_enable->ocp_timeout = 0;
	irq_enable->core = 0;
}

void ti_hdmi_4xxx_wp_video_start(struct hdmi_ip_data *ip_data, bool start)
{
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG, start, 31, 31);
}

void hdmi_wp_video_init_format(struct hdmi_video_format *video_fmt,
	struct omap_video_timings *timings, struct hdmi_config *param)
{
	pr_debug("Enter hdmi_wp_video_init_format\n");

	video_fmt->y_res = param->timings.yres;
	if (param->timings.vmode & FB_VMODE_INTERLACED)
		video_fmt->y_res /= 2;
	video_fmt->x_res = param->timings.xres;
	omapfb_fb2dss_timings(&param->timings, timings);
}

void hdmi_wp_video_config_format(struct hdmi_ip_data *ip_data,
		struct hdmi_video_format *video_fmt)
{
	u32 l = 0;

	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG,
			video_fmt->packing_mode, 10, 8);

	l |= FLD_VAL(video_fmt->y_res, 31, 16);
	l |= FLD_VAL(video_fmt->x_res, 15, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_SIZE, l);
}

void hdmi_wp_video_config_interface(struct hdmi_ip_data *ip_data,
	struct hdmi_video_interface *video_int)
{
	u32 r;
	pr_debug("Enter hdmi_wp_video_config_interface\n");

	r = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG);
	r = FLD_MOD(r, video_int->vsp, 7, 7);
	r = FLD_MOD(r, video_int->hsp, 6, 6);
	r = FLD_MOD(r, video_int->interlacing, 3, 3);
	r = FLD_MOD(r, video_int->tm, 1, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG, r);
}

void hdmi_wp_video_config_timing(struct hdmi_ip_data *ip_data,
		struct omap_video_timings *timings)
{
	u32 timing_h = 0;
	u32 timing_v = 0;

	pr_debug("Enter hdmi_wp_video_config_timing\n");

	timing_h |= FLD_VAL(timings->hbp, 31, 20);
	timing_h |= FLD_VAL(timings->hfp, 19, 8);
	timing_h |= FLD_VAL(timings->hsw, 7, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_TIMING_H, timing_h);

	timing_v |= FLD_VAL(timings->vbp, 31, 20);
	timing_v |= FLD_VAL(timings->vfp, 19, 8);
	timing_v |= FLD_VAL(timings->vsw, 7, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_TIMING_V, timing_v);
}

void hdmi_wp_irq_enable(struct hdmi_ip_data *ip_data,
			struct hdmi_irq_vector *irq_enable)
{
	u32 r = 0;
	r = ((irq_enable->pll_recal << 31) |
		(irq_enable->pll_unlock << 30) |
		(irq_enable->pll_lock << 29) |
		(irq_enable->phy_disconnect << 26) |
		(irq_enable->phy_connect << 25) |
		(irq_enable->phy_short_5v << 24) |
		(irq_enable->video_end_fr << 17) |
		(irq_enable->video_vsync << 16) |
		(irq_enable->fifo_sample_req << 10) |
		(irq_enable->fifo_overflow << 9) |
		(irq_enable->fifo_underflow << 8) |
		(irq_enable->ocp_timeout << 4) |
		(irq_enable->core << 0));

	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_IRQENABLE_SET, r);
}

int ti_hdmi_4xxx_irq_handler(struct hdmi_ip_data *ip_data)
{
	u32 val = 0, r = 0;
	u32 core_state = 0;
	u32 intr4 = 0;
	void __iomem *wp_base = hdmi_wp_base(ip_data);
	void __iomem *core_base = hdmi_core_sys_base(ip_data);

	pr_debug("Enter hdmi_ti_4xxx_irq_handler\n");

	val = hdmi_read_reg(wp_base, HDMI_WP_IRQSTATUS);
	if (val & HDMI_WP_IRQSTATUS_CORE) {
		pr_debug("HDMI_WP_IRQSTATUS = 0x%x\n", val);
		core_state = hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR_STATE);
		if (core_state & 0x1) {
			intr4 = hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR4);
			hdmi_write_reg(core_base, HDMI_CORE_SYS_INTR4, intr4);
		}
	}

	if (intr4 & HDMI_CORE_SYSTEM_INTR4_CEC)
		r |= HDMI_CEC_INT;

	/* Ack other interrupts if any */
	hdmi_write_reg(wp_base, HDMI_WP_IRQSTATUS, val);
	/* flush posted write */
	hdmi_read_reg(wp_base, HDMI_WP_IRQSTATUS);

	return r;
}

void ti_hdmi_4xxx_basic_configure(struct hdmi_ip_data *ip_data)
{
	/* HDMI */
	struct omap_video_timings video_timing;
	struct hdmi_video_format video_format;
	struct hdmi_video_interface video_interface;
	/* HDMI core */
	struct hdmi_core_infoframe_avi *avi_cfg = &ip_data->avi_cfg;
	struct hdmi_core_video_config v_core_cfg;
	struct hdmi_core_packet_enable_repeat repeat_cfg;
	struct hdmi_config *cfg = &ip_data->cfg;
	struct hdmi_irq_vector irq_enable;

	hdmi_wp_init(&video_timing, &video_format, &irq_enable);

	hdmi_core_init(&v_core_cfg, avi_cfg, &repeat_cfg);

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
	video_interface.tm = 1 ; /* HDMI_TIMING_MASTER_24BIT */

	hdmi_wp_video_config_interface(ip_data, &video_interface);

	/*
	 * configure core video part
	 * set software reset in the core
	 */
	hdmi_core_swreset_assert(ip_data);

	/* power down off */
	hdmi_core_powerdown_disable(ip_data);

	v_core_cfg.pkt_mode = HDMI_PACKETMODE24BITPERPIXEL;
	v_core_cfg.hdmi_dvi = cfg->cm.mode;

	hdmi_core_video_config(ip_data, &v_core_cfg);

	/* release software reset in the core */
	hdmi_core_swreset_release(ip_data);

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

	/* enable/repeat the infoframe */
	repeat_cfg.avi_infoframe = HDMI_PACKETENABLE;
	repeat_cfg.avi_infoframe_repeat = HDMI_PACKETREPEATON;
	/* wakeup */
	repeat_cfg.audio_pkt = HDMI_PACKETENABLE;
	repeat_cfg.audio_pkt_repeat = HDMI_PACKETREPEATON;
	hdmi_core_av_packet_config(ip_data, repeat_cfg);
}

int ti_hdmi_4xxx_cec_read_rx_cmd(struct hdmi_ip_data *ip_data,
		struct cec_rx_data *rx_data)
{
	int rx_byte_cnt;
	int temp;
	int i;
	int cec_cmd_cnt = 0;
	int r = 0;

	cec_cmd_cnt = REG_GET(hdmi_core_cec_base(ip_data),
			HDMI_CEC_RX_COUNT, 6, 4);

	if (cec_cmd_cnt > 0) {
		/*TODO:Check the RX error*/
		/*Get the initiator and destination id*/
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_RX_CMD_HEADER);
		rx_data->init_device_id = FLD_GET(temp, 7, 4);
		rx_data->dest_device_id = FLD_GET(temp, 3, 0);

		/*get the command*/
		r = ti_hdmi_4xxx_cec_get_rx_cmd(ip_data, &rx_data->rx_cmd);
		if (r) {
			pr_err(KERN_ERR "RX Error in reading cmd\n");
			goto error_exit;
		}

		/*Get the rx command operands*/
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_RX_COUNT);
		rx_byte_cnt = FLD_GET(temp, 3, 0);
		rx_data->rx_count = rx_byte_cnt;
		if (rx_data->rx_count > CEC_MAX_NUM_OPERANDS) {
			pr_err(KERN_ERR "RX wrong num of operands\n");
			r = -EINVAL;
		} else {
			for (i = 0; i < rx_byte_cnt; i++) {
				temp = RD_REG_32(hdmi_core_cec_base(ip_data),
					HDMI_CEC_RX_OPERAND + (i * 4));
				rx_data->rx_operand[i] = FLD_GET(temp, 7, 0);
			}
		}

		/* Clear the just read command */
		REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_RX_CONTROL,
			1, 0, 0);

		} else {
			/*No cmd in the FIFO return error*/
			r = -EINVAL   ;
		}

error_exit:
	return r;

}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_read_rx_cmd);

int ti_hdmi_4xxx_cec_get_rx_cmd(struct hdmi_ip_data *ip_data,
		char *rx_cmd)
{
	int temp;
	int cec_cmd_cnt = 0;
	int r = 0;

	if (!ip_data)
		return -ENODEV;

	cec_cmd_cnt = REG_GET(hdmi_core_cec_base(ip_data),
			HDMI_CEC_RX_COUNT, 6, 4);

	if (cec_cmd_cnt > 0) {
		/*get the command*/
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_RX_COMMAND);
		*rx_cmd = FLD_GET(temp, 7, 0);
		r = 0;
	} else
		r = -EINVAL;

	return r;

}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_get_rx_cmd);

int ti_hdmi_4xxx_cec_transmit_cmd(struct hdmi_ip_data *ip_data,
		struct cec_tx_data *data, int *cmd_acked)
{
	int r = -EINVAL;
	u32 retry = HDMI_CORE_CEC_RETRY;
	u32 temp, i = 0;

	/* 1. Flush TX FIFO - required as change of initiator ID / destination
	ID while TX is in progress could result in courrupted message.
	2. Clear interrupt status registers for TX.
	3. Set initiator Address, set retry count
	4. Set Destination Address
	5. Clear TX interrupt flags - if required
	6. Set the command
	7. Transmit
	8. Check for NACK / ACK - report the same. */


	/* Clear TX FIFO */
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_DBG_3, 1, 7, 7);

	while (retry) {
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_DBG_3);
		if (FLD_GET(temp, 7, 7) == 0)
			break;
		udelay(10);
		retry--;
	}
	if (retry == 0x0) {
		pr_err(KERN_ERR "Could not clear TX FIFO");
		pr_err(KERN_ERR "\n FIFO Reset - retry  : %d - was %d\n",
			retry, HDMI_CORE_CEC_RETRY);
		goto error_exit;
	}

	/* Clear TX interrupts */
	hdmi_write_reg(hdmi_core_cec_base(ip_data), HDMI_CEC_INT_STATUS_0,
		HDMI_CEC_TX_FIFO_INT_MASK);

	hdmi_write_reg(hdmi_core_cec_base(ip_data), HDMI_CEC_INT_STATUS_1,
		HDMI_CEC_RETRANSMIT_CNT_INT_MASK);

	/* Set the initiator addresses */
	temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_TX_INIT);
	temp = FLD_MOD(temp, data->initiator_device_id, 3, 0);
	hdmi_write_reg(hdmi_core_cec_base(ip_data), HDMI_CEC_TX_INIT,
		temp);

	/*Set destination id*/
	temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_TX_DEST);
	temp = FLD_MOD(temp, data->dest_device_id, 3, 0);
	hdmi_write_reg(hdmi_core_cec_base(ip_data), HDMI_CEC_TX_DEST,
		temp);

	/* Set the retry count */
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_DBG_3,
		data->retry_count, 6, 4);

	if (data->send_ping)
		goto send_ping;

	/* Setup command and arguments for the command */
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_TX_COMMAND,
		data->tx_cmd, 7, 0);

	for (i = 0; i < data->tx_count; i++) {
		temp = RD_REG_32(hdmi_core_cec_base(ip_data),
			(HDMI_CEC_TX_OPERAND + (i * 4)));
		temp = FLD_MOD(temp, data->tx_operand[i], 7, 0);
		WR_REG_32(hdmi_core_cec_base(ip_data),
			(HDMI_CEC_TX_OPERAND + (i * 4)), temp);
	}

	/* Operand count */
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_TRANSMIT_DATA,
		data->tx_count, 3, 0);
	/* Transmit */
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_TRANSMIT_DATA,
		0x1, 4, 4);

	goto wait_for_ack_nack;

send_ping:

	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_TX_DEST, 0x1, 7, 7);
	/*Wait for reset*/
	retry = HDMI_CORE_CEC_RETRY;
	while (retry) {
		if (!REG_GET(hdmi_core_cec_base(ip_data), HDMI_CEC_TX_DEST,
			7, 7))
			break;
		udelay(10);
		retry--;
	}
	if (retry == 0) {
		pr_err(KERN_ERR "\nCould not send ping\n");
		goto error_exit;
	}

wait_for_ack_nack:
	pr_debug("cec_transmit_cmd wait for ack\n");
	retry = HDMI_CEC_TX_CMD_RETRY;
	*cmd_acked = -1;
	do {
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_INT_STATUS_0);
		/* Look for TX change event */
		if (FLD_GET(temp, 5, 5) != 0) {
			*cmd_acked = 1;
			/* Clear interrupt status */
			REG_FLD_MOD(hdmi_core_cec_base(ip_data),
				HDMI_CEC_INT_STATUS_0, 0x1, 5, 5);
			if (FLD_GET(temp, 2, 2) != 0) {
				/* Clear interrupt status */
				REG_FLD_MOD(hdmi_core_cec_base(ip_data),
					HDMI_CEC_INT_STATUS_0, 0x1, 2, 2);
			}

			r = 0;
			break;
		}
		/* Wait for re-transmits to expire */
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_INT_STATUS_1);
		if (FLD_GET(temp, 1, 1) == 0) {
			/* Wait for 7 mSecs - As per CEC protocol
			nominal bit period is ~3 msec
			delay of >= 3 bit period before next attempt
			*/
			mdelay(10);

		} else {
			/* Nacked ensure to clear the status */
			temp = FLD_MOD(0x0, 1, 1, 1);
			hdmi_write_reg(hdmi_core_cec_base(ip_data),
				HDMI_CEC_INT_STATUS_1, temp);
			*cmd_acked = 0;
			r = 0;
			break;
		}
		retry--;
	} while (retry);

	if (retry == 0x0) {
		pr_err(KERN_ERR "\nCould not send\n");
		pr_err(KERN_ERR "\nNo ack / nack sensed\n");
		pr_err(KERN_ERR "\nResend did not complete in : %d\n",
			((HDMI_CEC_TX_CMD_RETRY - retry) * 10));
	}

error_exit:

	return r;
}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_transmit_cmd);

int ti_hdmi_4xxx_power_on_cec(struct hdmi_ip_data *ip_data)
{
	int temp;
	int r = 0;
	int retry = HDMI_CORE_CEC_RETRY;

	/*Clear TX FIFO*/
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_DBG_3, 0x1, 7, 7);
	while (retry) {
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_DBG_3);
		if (FLD_GET(temp, 7, 7) == 0)
			break;
		retry--;
	}
	if (retry == 0x0) {
		pr_err(KERN_ERR "Could not clear TX FIFO");
		r = -EBUSY;
		goto error_exit;
	}

	/*Clear RX FIFO*/
	hdmi_write_reg(hdmi_core_cec_base(ip_data), HDMI_CEC_RX_CONTROL, 0x3);
	retry = HDMI_CORE_CEC_RETRY;
	while (retry) {
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_RX_CONTROL);
		if (FLD_GET(temp, 1, 0) == 0)
			break;
		retry--;
	}
	if (retry == 0x0) {
		pr_err(KERN_ERR "Could not clear RX FIFO");
		r = -EBUSY;
		goto error_exit;
	}

	/*Clear CEC interrupts*/
	hdmi_write_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_INT_STATUS_1,
	hdmi_read_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_INT_STATUS_1));
	hdmi_write_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_INT_STATUS_0,
	hdmi_read_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_INT_STATUS_0));

	/*Enable HDMI core interrupts*/

	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_IRQENABLE_SET, 0x1,
		0, 0);


	REG_FLD_MOD(hdmi_core_sys_base(ip_data), HDMI_CORE_SYS_UMASK4, 0x1, 3,
		3);

	/*Enable CEC interrupts*/
	/*command being received event*/
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_INT_ENABLE_0, 0x1, 0,
		0);

	/*RX fifo not empty event*/
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_INT_ENABLE_0, 0x1, 1,
		1);


	/*Initialize CEC clock divider*/
	/*CEC needs 2MHz clock hence set the devider to 24 to get
	48/24=2MHz clock*/
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_WP_CLK, 0x18, 5, 0);

	/*Remove BYpass mode*/

	temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
		HDMI_CEC_SETUP);
	if (FLD_GET(temp, 4, 4) != 0) {
		temp = FLD_MOD(temp, 0, 4, 4);
		hdmi_write_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_SETUP, temp);

		/* If we enabled CEC in middle of a CEC messages on CEC n/w,
		we could have start bit irregularity and/or short
		pulse event. Clear them now */
		temp = hdmi_read_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_INT_STATUS_1);
		temp = FLD_MOD(0x0, 0x5, 2, 0);
		hdmi_write_reg(hdmi_core_cec_base(ip_data),
			HDMI_CEC_INT_STATUS_1, temp);
	}
	return 0;
error_exit:
	return r;
}
EXPORT_SYMBOL(ti_hdmi_4xxx_power_on_cec);

int ti_hdmi_4xxx_power_off_cec(struct hdmi_ip_data *ip_data)
{
	return 0;
}
EXPORT_SYMBOL(ti_hdmi_4xxx_power_off_cec);

int ti_hdmi_4xxx_cec_int_handler(struct hdmi_ip_data *ip_data)
{
	u32 cec_rx = 0;

	cec_rx = REG_GET(hdmi_core_cec_base(ip_data),
		HDMI_CEC_INT_STATUS_0, 1, 0);
	return cec_rx;
}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_int_handler);

int ti_hdmi_4xxx_cec_clr_rx_int(struct hdmi_ip_data *ip_data, int cec_rx)
{
	/*clear CEC RX interrupts*/
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_INT_STATUS_0, cec_rx,
		1, 0);
	return 0;
}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_clr_rx_int);

int ti_hdmi_4xxx_cec_get_reg_device_list(struct hdmi_ip_data *ip_data)
{
	int dev_mask = 0;

	/*Store current device listning ids*/
	dev_mask = RD_REG_32(hdmi_core_cec_base(ip_data),
		HDMI_CEC_CA_15_8);
	dev_mask <<= 8;
	dev_mask |= RD_REG_32(hdmi_core_cec_base(ip_data),
		HDMI_CEC_CA_7_0);
	return dev_mask;

}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_get_reg_device_list);

int ti_hdmi_4xxx_cec_add_reg_device(struct hdmi_ip_data *ip_data,
		int device_id, int clear)
{
	u32 temp, regis_reg, shift_cnt;

	/* Register to receive messages intended for this device
	and broad cast messages */
	regis_reg = HDMI_CEC_CA_7_0;
	shift_cnt = device_id;
	temp = 0;
	if (device_id > 0x7) {
		regis_reg = HDMI_CEC_CA_15_8;
		shift_cnt -= 0x7;
	}
	if (clear == 0x1) {
		WR_REG_32(hdmi_core_cec_base(ip_data),
			HDMI_CEC_CA_7_0, 0);
		WR_REG_32(hdmi_core_cec_base(ip_data),
			HDMI_CEC_CA_15_8, 0);
	} else {
		temp = RD_REG_32(hdmi_core_cec_base(ip_data), regis_reg);
	}
	temp |= 0x1 << shift_cnt;
	WR_REG_32(hdmi_core_cec_base(ip_data), regis_reg,
		temp);
	return 0;

}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_add_reg_device);

int ti_hdmi_4xxx_cec_set_reg_device_list(struct hdmi_ip_data *ip_data, int mask)
{
	WR_REG_32(hdmi_core_cec_base(ip_data),
		HDMI_CEC_CA_15_8, (mask >> 8) & 0xff);
	WR_REG_32(hdmi_core_cec_base(ip_data),
		HDMI_CEC_CA_7_0, mask & 0xff);
	return 0;
}
EXPORT_SYMBOL(ti_hdmi_4xxx_cec_set_reg_device_list);


void ti_hdmi_4xxx_wp_dump(struct hdmi_ip_data *ip_data, struct seq_file *s)
{
#define DUMPREG(r) seq_printf(s, "%-35s %08x\n", #r,\
		hdmi_read_reg(hdmi_wp_base(ip_data), r))

	DUMPREG(HDMI_WP_REVISION);
	DUMPREG(HDMI_WP_SYSCONFIG);
	DUMPREG(HDMI_WP_IRQSTATUS_RAW);
	DUMPREG(HDMI_WP_IRQSTATUS);
	DUMPREG(HDMI_WP_PWR_CTRL);
	DUMPREG(HDMI_WP_IRQENABLE_SET);
	DUMPREG(HDMI_WP_VIDEO_CFG);
	DUMPREG(HDMI_WP_VIDEO_SIZE);
	DUMPREG(HDMI_WP_VIDEO_TIMING_H);
	DUMPREG(HDMI_WP_VIDEO_TIMING_V);
	DUMPREG(HDMI_WP_WP_CLK);
	DUMPREG(HDMI_WP_AUDIO_CFG);
	DUMPREG(HDMI_WP_AUDIO_CFG2);
	DUMPREG(HDMI_WP_AUDIO_CTRL);
	DUMPREG(HDMI_WP_AUDIO_DATA);
}

void ti_hdmi_4xxx_pll_dump(struct hdmi_ip_data *ip_data, struct seq_file *s)
{
#define DUMPPLL(r) seq_printf(s, "%-35s %08x\n", #r,\
		hdmi_read_reg(hdmi_pll_base(ip_data), r))

	DUMPPLL(PLLCTRL_PLL_CONTROL);
	DUMPPLL(PLLCTRL_PLL_STATUS);
	DUMPPLL(PLLCTRL_PLL_GO);
	DUMPPLL(PLLCTRL_CFG1);
	DUMPPLL(PLLCTRL_CFG2);
	DUMPPLL(PLLCTRL_CFG3);
	DUMPPLL(PLLCTRL_CFG4);
}

void ti_hdmi_4xxx_core_dump(struct hdmi_ip_data *ip_data, struct seq_file *s)
{
	int i;

#define CORE_REG(i, name) name(i)
#define DUMPCORE(r) seq_printf(s, "%-35s %08x\n", #r,\
		hdmi_read_reg(hdmi_pll_base(ip_data), r))
#define DUMPCOREAV(i, r) seq_printf(s, "%s[%d]%*s %08x\n", #r, i, \
		(i < 10) ? 32 - strlen(#r) : 31 - strlen(#r), " ", \
		hdmi_read_reg(hdmi_pll_base(ip_data), CORE_REG(i, r)))

	DUMPCORE(HDMI_CORE_SYS_VND_IDL);
	DUMPCORE(HDMI_CORE_SYS_DEV_IDL);
	DUMPCORE(HDMI_CORE_SYS_DEV_IDH);
	DUMPCORE(HDMI_CORE_SYS_DEV_REV);
	DUMPCORE(HDMI_CORE_SYS_SRST);
	DUMPCORE(HDMI_CORE_CTRL1);
	DUMPCORE(HDMI_CORE_SYS_SYS_STAT);
	DUMPCORE(HDMI_CORE_SYS_VID_ACEN);
	DUMPCORE(HDMI_CORE_SYS_VID_MODE);
	DUMPCORE(HDMI_CORE_SYS_INTR_STATE);
	DUMPCORE(HDMI_CORE_SYS_INTR1);
	DUMPCORE(HDMI_CORE_SYS_INTR2);
	DUMPCORE(HDMI_CORE_SYS_INTR3);
	DUMPCORE(HDMI_CORE_SYS_INTR4);
	DUMPCORE(HDMI_CORE_SYS_UMASK1);
	DUMPCORE(HDMI_CORE_SYS_TMDS_CTRL);
	DUMPCORE(HDMI_CORE_SYS_DE_DLY);
	DUMPCORE(HDMI_CORE_SYS_DE_CTRL);
	DUMPCORE(HDMI_CORE_SYS_DE_TOP);
	DUMPCORE(HDMI_CORE_SYS_DE_CNTL);
	DUMPCORE(HDMI_CORE_SYS_DE_CNTH);
	DUMPCORE(HDMI_CORE_SYS_DE_LINL);
	DUMPCORE(HDMI_CORE_SYS_DE_LINH_1);

	DUMPCORE(HDMI_CORE_DDC_CMD);
	DUMPCORE(HDMI_CORE_DDC_STATUS);
	DUMPCORE(HDMI_CORE_DDC_ADDR);
	DUMPCORE(HDMI_CORE_DDC_OFFSET);
	DUMPCORE(HDMI_CORE_DDC_COUNT1);
	DUMPCORE(HDMI_CORE_DDC_COUNT2);
	DUMPCORE(HDMI_CORE_DDC_DATA);
	DUMPCORE(HDMI_CORE_DDC_SEGM);

	DUMPCORE(HDMI_CORE_AV_HDMI_CTRL);
	DUMPCORE(HDMI_CORE_AV_DPD);
	DUMPCORE(HDMI_CORE_AV_PB_CTRL1);
	DUMPCORE(HDMI_CORE_AV_PB_CTRL2);
	DUMPCORE(HDMI_CORE_AV_AVI_TYPE);
	DUMPCORE(HDMI_CORE_AV_AVI_VERS);
	DUMPCORE(HDMI_CORE_AV_AVI_LEN);
	DUMPCORE(HDMI_CORE_AV_AVI_CHSUM);

	for (i = 0; i < HDMI_CORE_AV_AVI_DBYTE_NELEMS; i++)
		DUMPCOREAV(i, HDMI_CORE_AV_AVI_DBYTE);

	for (i = 0; i < HDMI_CORE_AV_SPD_DBYTE_NELEMS; i++)
		DUMPCOREAV(i, HDMI_CORE_AV_SPD_DBYTE);

	for (i = 0; i < HDMI_CORE_AV_AUD_DBYTE_NELEMS; i++)
		DUMPCOREAV(i, HDMI_CORE_AV_AUD_DBYTE);

	for (i = 0; i < HDMI_CORE_AV_MPEG_DBYTE_NELEMS; i++)
		DUMPCOREAV(i, HDMI_CORE_AV_MPEG_DBYTE);

	for (i = 0; i < HDMI_CORE_AV_GEN_DBYTE_NELEMS; i++)
		DUMPCOREAV(i, HDMI_CORE_AV_GEN_DBYTE);

	for (i = 0; i < HDMI_CORE_AV_GEN2_DBYTE_NELEMS; i++)
		DUMPCOREAV(i, HDMI_CORE_AV_GEN2_DBYTE);

	DUMPCORE(HDMI_CORE_AV_ACR_CTRL);
	DUMPCORE(HDMI_CORE_AV_FREQ_SVAL);
	DUMPCORE(HDMI_CORE_AV_N_SVAL1);
	DUMPCORE(HDMI_CORE_AV_N_SVAL2);
	DUMPCORE(HDMI_CORE_AV_N_SVAL3);
	DUMPCORE(HDMI_CORE_AV_CTS_SVAL1);
	DUMPCORE(HDMI_CORE_AV_CTS_SVAL2);
	DUMPCORE(HDMI_CORE_AV_CTS_SVAL3);
	DUMPCORE(HDMI_CORE_AV_CTS_HVAL1);
	DUMPCORE(HDMI_CORE_AV_CTS_HVAL2);
	DUMPCORE(HDMI_CORE_AV_CTS_HVAL3);
	DUMPCORE(HDMI_CORE_AV_AUD_MODE);
	DUMPCORE(HDMI_CORE_AV_SPDIF_CTRL);
	DUMPCORE(HDMI_CORE_AV_HW_SPDIF_FS);
	DUMPCORE(HDMI_CORE_AV_SWAP_I2S);
	DUMPCORE(HDMI_CORE_AV_SPDIF_ERTH);
	DUMPCORE(HDMI_CORE_AV_I2S_IN_MAP);
	DUMPCORE(HDMI_CORE_AV_I2S_IN_CTRL);
	DUMPCORE(HDMI_CORE_AV_I2S_CHST0);
	DUMPCORE(HDMI_CORE_AV_I2S_CHST1);
	DUMPCORE(HDMI_CORE_AV_I2S_CHST2);
	DUMPCORE(HDMI_CORE_AV_I2S_CHST4);
	DUMPCORE(HDMI_CORE_AV_I2S_CHST5);
	DUMPCORE(HDMI_CORE_AV_ASRC);
	DUMPCORE(HDMI_CORE_AV_I2S_IN_LEN);
	DUMPCORE(HDMI_CORE_AV_HDMI_CTRL);
	DUMPCORE(HDMI_CORE_AV_AUDO_TXSTAT);
	DUMPCORE(HDMI_CORE_AV_AUD_PAR_BUSCLK_1);
	DUMPCORE(HDMI_CORE_AV_AUD_PAR_BUSCLK_2);
	DUMPCORE(HDMI_CORE_AV_AUD_PAR_BUSCLK_3);
	DUMPCORE(HDMI_CORE_AV_TEST_TXCTRL);
	DUMPCORE(HDMI_CORE_AV_DPD);
	DUMPCORE(HDMI_CORE_AV_PB_CTRL1);
	DUMPCORE(HDMI_CORE_AV_PB_CTRL2);
	DUMPCORE(HDMI_CORE_AV_AVI_TYPE);
	DUMPCORE(HDMI_CORE_AV_AVI_VERS);
	DUMPCORE(HDMI_CORE_AV_AVI_LEN);
	DUMPCORE(HDMI_CORE_AV_AVI_CHSUM);
	DUMPCORE(HDMI_CORE_AV_SPD_TYPE);
	DUMPCORE(HDMI_CORE_AV_SPD_VERS);
	DUMPCORE(HDMI_CORE_AV_SPD_LEN);
	DUMPCORE(HDMI_CORE_AV_SPD_CHSUM);
	DUMPCORE(HDMI_CORE_AV_AUDIO_TYPE);
	DUMPCORE(HDMI_CORE_AV_AUDIO_VERS);
	DUMPCORE(HDMI_CORE_AV_AUDIO_LEN);
	DUMPCORE(HDMI_CORE_AV_AUDIO_CHSUM);
	DUMPCORE(HDMI_CORE_AV_MPEG_TYPE);
	DUMPCORE(HDMI_CORE_AV_MPEG_VERS);
	DUMPCORE(HDMI_CORE_AV_MPEG_LEN);
	DUMPCORE(HDMI_CORE_AV_MPEG_CHSUM);
	DUMPCORE(HDMI_CORE_AV_CP_BYTE1);
	DUMPCORE(HDMI_CORE_AV_CEC_ADDR_ID);
}

void ti_hdmi_4xxx_phy_dump(struct hdmi_ip_data *ip_data, struct seq_file *s)
{
#define DUMPPHY(r) seq_printf(s, "%-35s %08x\n", #r,\
		hdmi_read_reg(hdmi_phy_base(ip_data), r))

	DUMPPHY(HDMI_TXPHY_TX_CTRL);
	DUMPPHY(HDMI_TXPHY_DIGITAL_CTRL);
	DUMPPHY(HDMI_TXPHY_POWER_CTRL);
	DUMPPHY(HDMI_TXPHY_PAD_CFG_CTRL);
}

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO) || \
	defined(CONFIG_OMAP5_DSS_HDMI_AUDIO)
void ti_hdmi_4xxx_wp_audio_config_dma(struct hdmi_ip_data *ip_data,
					struct hdmi_audio_dma *aud_dma)
{
	u32 r;

	DSSDBG("Enter hdmi_wp_audio_config_dma\n");

	r = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CFG2);
	r = FLD_MOD(r, aud_dma->transfer_size, 15, 8);
	r = FLD_MOD(r, aud_dma->block_size, 7, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CFG2, r);

	r = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CTRL);
	r = FLD_MOD(r, aud_dma->mode, 9, 9);
	r = FLD_MOD(r, aud_dma->fifo_threshold, 8, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CTRL, r);
}
#endif

#if defined(CONFIG_OMAP4_DSS_HDMI_AUDIO)
static void ti_hdmi_4xxx_wp_audio_config_format(struct hdmi_ip_data *ip_data,
					struct hdmi_audio_format *aud_fmt)
{
	u32 r;

	DSSDBG("Enter hdmi_wp_audio_config_format\n");

	r = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CFG);
	r = FLD_MOD(r, aud_fmt->stereo_channels, 26, 24);
	r = FLD_MOD(r, aud_fmt->active_chnnls_msk, 23, 16);
	r = FLD_MOD(r, aud_fmt->en_sig_blk_strt_end, 5, 5);
	r = FLD_MOD(r, aud_fmt->type, 4, 4);
	r = FLD_MOD(r, aud_fmt->justification, 3, 3);
	r = FLD_MOD(r, aud_fmt->sample_order, 2, 2);
	r = FLD_MOD(r, aud_fmt->samples_per_word, 1, 1);
	r = FLD_MOD(r, aud_fmt->sample_size, 0, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CFG, r);
}

static void ti_hdmi_4xxx_core_audio_config(struct hdmi_ip_data *ip_data,
					struct hdmi_core_audio_config *cfg)
{
	u32 r;
	void __iomem *av_base = hdmi_av_base(ip_data);

	/*
	 * Parameters for generation of Audio Clock Recovery packets
	 */
	REG_FLD_MOD(av_base, HDMI_CORE_AV_N_SVAL1, cfg->n, 7, 0);
	REG_FLD_MOD(av_base, HDMI_CORE_AV_N_SVAL2, cfg->n >> 8, 7, 0);
	REG_FLD_MOD(av_base, HDMI_CORE_AV_N_SVAL3, cfg->n >> 16, 7, 0);

	if (cfg->cts_mode == HDMI_AUDIO_CTS_MODE_SW) {
		REG_FLD_MOD(av_base, HDMI_CORE_AV_CTS_SVAL1, cfg->cts, 7, 0);
		REG_FLD_MOD(av_base,
				HDMI_CORE_AV_CTS_SVAL2, cfg->cts >> 8, 7, 0);
		REG_FLD_MOD(av_base,
				HDMI_CORE_AV_CTS_SVAL3, cfg->cts >> 16, 7, 0);
	} else {
		REG_FLD_MOD(av_base, HDMI_CORE_AV_AUD_PAR_BUSCLK_1,
				cfg->aud_par_busclk, 7, 0);
		REG_FLD_MOD(av_base, HDMI_CORE_AV_AUD_PAR_BUSCLK_2,
				(cfg->aud_par_busclk >> 8), 7, 0);
		REG_FLD_MOD(av_base, HDMI_CORE_AV_AUD_PAR_BUSCLK_3,
				(cfg->aud_par_busclk >> 16), 7, 0);
	}

	/* Set ACR clock divisor */
	REG_FLD_MOD(av_base,
			HDMI_CORE_AV_FREQ_SVAL, cfg->mclk_mode, 2, 0);

	r = hdmi_read_reg(av_base, HDMI_CORE_AV_ACR_CTRL);
	/*
	 * Use TMDS clock for ACR packets. For devices that use
	 * the MCLK, this is the first part of the MCLK initialization.
	 */
	r = FLD_MOD(r, 0, 2, 2);

	r = FLD_MOD(r, cfg->en_acr_pkt, 1, 1);
	r = FLD_MOD(r, cfg->cts_mode, 0, 0);
	hdmi_write_reg(av_base, HDMI_CORE_AV_ACR_CTRL, r);

	/* For devices using MCLK, this completes its initialization. */
	if (cfg->use_mclk)
		REG_FLD_MOD(av_base, HDMI_CORE_AV_ACR_CTRL, 1, 2, 2);

	/* Override of SPDIF sample frequency with value in I2S_CHST4 */
	REG_FLD_MOD(av_base, HDMI_CORE_AV_SPDIF_CTRL,
						cfg->fs_override, 1, 1);

	/*
	 * Set IEC-60958-3 channel status word. It is passed to the IP
	 * just as it is received. The user of the driver is responsible
	 * for its contents.
	 */
	hdmi_write_reg(av_base, HDMI_CORE_AV_I2S_CHST0,
		       cfg->iec60958_cfg->status[0]);
	hdmi_write_reg(av_base, HDMI_CORE_AV_I2S_CHST1,
		       cfg->iec60958_cfg->status[1]);
	hdmi_write_reg(av_base, HDMI_CORE_AV_I2S_CHST2,
		       cfg->iec60958_cfg->status[2]);
	/* yes, this is correct: status[3] goes to CHST4 register */
	hdmi_write_reg(av_base, HDMI_CORE_AV_I2S_CHST4,
		       cfg->iec60958_cfg->status[3]);
	/* yes, this is correct: status[4] goes to CHST5 register */
	hdmi_write_reg(av_base, HDMI_CORE_AV_I2S_CHST5,
		       cfg->iec60958_cfg->status[4]);

	/* set I2S parameters */
	r = hdmi_read_reg(av_base, HDMI_CORE_AV_I2S_IN_CTRL);
	r = FLD_MOD(r, cfg->en_high_bitrate_aud, 7, 7);
	r = FLD_MOD(r, cfg->i2s_cfg.sck_edge_mode, 6, 6);
	r = FLD_MOD(r, cfg->i2s_cfg.cbit_order, 5, 5);
	r = FLD_MOD(r, cfg->i2s_cfg.vbit, 4, 4);
	r = FLD_MOD(r, cfg->i2s_cfg.ws_polarity, 3, 3);
	r = FLD_MOD(r, cfg->i2s_cfg.justification, 2, 2);
	r = FLD_MOD(r, cfg->i2s_cfg.direction, 1, 1);
	r = FLD_MOD(r, cfg->i2s_cfg.shift, 0, 0);
	hdmi_write_reg(av_base, HDMI_CORE_AV_I2S_IN_CTRL, r);

	REG_FLD_MOD(av_base, HDMI_CORE_AV_I2S_IN_LEN,
			cfg->i2s_cfg.in_length_bits, 3, 0);

	/* Audio channels and mode parameters */
	REG_FLD_MOD(av_base, HDMI_CORE_AV_HDMI_CTRL, cfg->layout, 2, 1);
	r = hdmi_read_reg(av_base, HDMI_CORE_AV_AUD_MODE);
	r = FLD_MOD(r, cfg->i2s_cfg.active_sds, 7, 4);
	r = FLD_MOD(r, cfg->en_dsd_audio, 3, 3);
	r = FLD_MOD(r, cfg->en_parallel_aud_input, 2, 2);
	r = FLD_MOD(r, cfg->en_spdif, 1, 1);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_MODE, r);
}

static void ti_hdmi_4xxx_core_audio_infoframe_cfg(struct hdmi_ip_data *ip_data,
		struct snd_cea_861_aud_if *info_aud)
{
	u8 sum = 0, checksum = 0;
	void __iomem *av_base = hdmi_av_base(ip_data);

	/*
	 * Set audio info frame type, version and length as
	 * described in HDMI 1.4a Section 8.2.2 specification.
	 * Checksum calculation is defined in Section 5.3.5.
	 */
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUDIO_TYPE, 0x84);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUDIO_VERS, 0x01);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUDIO_LEN, 0x0a);
	sum += 0x84 + 0x001 + 0x00a;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(0),
		       info_aud->db1_ct_cc);
	sum += info_aud->db1_ct_cc;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(1),
		       info_aud->db2_sf_ss);
	sum += info_aud->db2_sf_ss;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(2), info_aud->db3);
	sum += info_aud->db3;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(3), info_aud->db4_ca);
	sum += info_aud->db4_ca;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(4),
		       info_aud->db5_dminh_lsv);
	sum += info_aud->db5_dminh_lsv;

	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(5), 0x00);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(6), 0x00);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(7), 0x00);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(8), 0x00);
	hdmi_write_reg(av_base, HDMI_CORE_AV_AUD_DBYTE(9), 0x00);

	checksum = 0x100 - sum;
	hdmi_write_reg(av_base,
					HDMI_CORE_AV_AUDIO_CHSUM, checksum);

	/*
	 * TODO: Add MPEG and SPD enable and repeat cfg when EDID parsing
	 * is available.
	 */
}

int ti_hdmi_4xxx_audio_config(struct hdmi_ip_data *ip_data,
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
	/*
	 * In the IEC-60958 status word, check if the audio sample word length
	 * is 16-bit as several optimizations can be performed in such case.
	 */
	if (!(iec->status[4] & IEC958_AES4_CON_MAX_WORDLEN_24))
		if (iec->status[4] & IEC958_AES4_CON_WORDLEN_20_16)
			word_length_16b = true;

	/* I2S configuration. See Phillips' specification */
	if (word_length_16b)
		core.i2s_cfg.justification = HDMI_AUDIO_JUSTIFY_LEFT;
	else
		core.i2s_cfg.justification = HDMI_AUDIO_JUSTIFY_RIGHT;
	/*
	 * The I2S input word length is twice the lenght given in the IEC-60958
	 * status word. If the word size is greater than
	 * 20 bits, increment by one.
	 */
	core.i2s_cfg.in_length_bits = iec->status[4] & IEC958_AES4_CON_WORDLEN;
	if (iec->status[4] & IEC958_AES4_CON_MAX_WORDLEN_24)
		core.i2s_cfg.in_length_bits++;
	core.i2s_cfg.sck_edge_mode = HDMI_AUDIO_I2S_SCK_EDGE_RISING;
	core.i2s_cfg.cbit_order = false;
	core.i2s_cfg.vbit = HDMI_AUDIO_I2S_VBIT_FOR_PCM;
	core.i2s_cfg.ws_polarity = HDMI_AUDIO_I2S_WS_POLARITY_LOW_IS_LEFT;
	core.i2s_cfg.direction = HDMI_AUDIO_I2S_MSB_SHIFTED_FIRST;
	core.i2s_cfg.shift = HDMI_AUDIO_I2S_FIRST_BIT_SHIFT;

	/* convert sample frequency to a number */
	switch (iec->status[3] & IEC958_AES3_CON_FS) {
	case IEC958_AES3_CON_FS_32000:
		fs_nr = 32000;
		break;
	case IEC958_AES3_CON_FS_44100:
		fs_nr = 44100;
		break;
	case IEC958_AES3_CON_FS_48000:
		fs_nr = 48000;
		break;
	case IEC958_AES3_CON_FS_88200:
		fs_nr = 88200;
		break;
	case IEC958_AES3_CON_FS_96000:
		fs_nr = 96000;
		break;
	case IEC958_AES3_CON_FS_176400:
		fs_nr = 176400;
		break;
	case IEC958_AES3_CON_FS_192000:
		fs_nr = 192000;
		break;
	default:
		return -EINVAL;
	}

	err = hdmi_compute_acr(fs_nr, (u32 *)&n, (u32 *)&cts);

	/* Audio clock regeneration settings */
	core.n = n;
	core.cts = cts;
	if (dss_has_feature(FEAT_HDMI_CTS_SWMODE)) {
		core.aud_par_busclk = 0;
		core.cts_mode = HDMI_AUDIO_CTS_MODE_SW;
		core.use_mclk = dss_has_feature(FEAT_HDMI_AUDIO_USE_MCLK);
	} else {
		core.aud_par_busclk = (((128 * 31) - 1) << 8);
		core.cts_mode = HDMI_AUDIO_CTS_MODE_HW;
		core.use_mclk = true;
	}

	if (core.use_mclk)
		core.mclk_mode = HDMI_AUDIO_MCLK_128FS;

	/* Audio channels settings */
	channel_count = (aud_if->db1_ct_cc & CEA861_AUDIO_INFOFRAME_DB1CC) + 1;

	switch (channel_count) {
	case 2:
		audio_format.active_chnnls_msk = 0x03;
		break;
	case 3:
		audio_format.active_chnnls_msk = 0x07;
		break;
	case 4:
		audio_format.active_chnnls_msk = 0x0f;
		break;
	case 5:
		audio_format.active_chnnls_msk = 0x1f;
		break;
	case 6:
		audio_format.active_chnnls_msk = 0x3f;
		break;
	case 7:
		audio_format.active_chnnls_msk = 0x7f;
		break;
	case 8:
		audio_format.active_chnnls_msk = 0xff;
		break;
	default:
		return -EINVAL;
	}

	/*
	 * the HDMI IP needs to enable four stereo channels when transmitting
	 * more than 2 audio channels
	 */
	if (channel_count == 2) {
		audio_format.stereo_channels = HDMI_AUDIO_STEREO_ONECHANNEL;
		core.i2s_cfg.active_sds = HDMI_AUDIO_I2S_SD0_EN;
		core.layout = HDMI_AUDIO_LAYOUT_2CH;
	} else {
		audio_format.stereo_channels = HDMI_AUDIO_STEREO_FOURCHANNELS;
		core.i2s_cfg.active_sds = HDMI_AUDIO_I2S_SD0_EN |
				HDMI_AUDIO_I2S_SD1_EN | HDMI_AUDIO_I2S_SD2_EN |
				HDMI_AUDIO_I2S_SD3_EN;
		core.layout = HDMI_AUDIO_LAYOUT_8CH;
	}

	core.en_spdif = false;
	/* use sample frequency from channel status word */
	core.fs_override = true;
	/* enable ACR packets */
	core.en_acr_pkt = true;
	/* disable direct streaming digital audio */
	core.en_dsd_audio = false;
	/* use parallel audio interface */
	core.en_parallel_aud_input = true;
	/* disable high bit-rate audio */
	core.en_high_bitrate_aud = false;

	/* DMA settings */
	if (word_length_16b)
		audio_dma.transfer_size = 0x10;
	else
		audio_dma.transfer_size = 0x20;
	audio_dma.block_size = 0xC0;
	audio_dma.mode = HDMI_AUDIO_TRANSF_DMA;
	audio_dma.fifo_threshold = 0x20; /* in number of samples */

	/* audio FIFO format settings */
	if (word_length_16b) {
		audio_format.samples_per_word = HDMI_AUDIO_ONEWORD_TWOSAMPLES;
		audio_format.sample_size = HDMI_AUDIO_SAMPLE_16BITS;
		audio_format.justification = HDMI_AUDIO_JUSTIFY_LEFT;
	} else {
		audio_format.samples_per_word = HDMI_AUDIO_ONEWORD_ONESAMPLE;
		audio_format.sample_size = HDMI_AUDIO_SAMPLE_24BITS;
		audio_format.justification = HDMI_AUDIO_JUSTIFY_RIGHT;
	}
	audio_format.type = HDMI_AUDIO_TYPE_LPCM;
	audio_format.sample_order = HDMI_AUDIO_SAMPLE_LEFT_FIRST;
	/* disable start/stop signals of IEC 60958 blocks */
	audio_format.en_sig_blk_strt_end = HDMI_AUDIO_BLOCK_SIG_STARTEND_ON;

	/* configure DMA and audio FIFO format*/
	ti_hdmi_4xxx_wp_audio_config_dma(ip_data, &audio_dma);
	ti_hdmi_4xxx_wp_audio_config_format(ip_data, &audio_format);

	/* configure the core*/
	ti_hdmi_4xxx_core_audio_config(ip_data, &core);

	/* configure CEA 861 audio infoframe*/
	ti_hdmi_4xxx_core_audio_infoframe_cfg(ip_data, aud_if);

	/* TODO: check video dependency (HDMI 1.4a, table 7-5) */

	return 0;
}

void ti_hdmi_4xxx_wp_audio_enable(struct hdmi_ip_data *ip_data, bool enable)
{
	REG_FLD_MOD(hdmi_wp_base(ip_data),
		    HDMI_WP_AUDIO_CTRL, enable, 31, 31);
}

void ti_hdmi_4xxx_audio_start(struct hdmi_ip_data *ip_data, bool enable)
{
	REG_FLD_MOD(hdmi_av_base(ip_data),
		    HDMI_CORE_AV_AUD_MODE, enable, 0, 0);
	REG_FLD_MOD(hdmi_wp_base(ip_data),
		    HDMI_WP_AUDIO_CTRL, enable, 30, 30);
}
#endif
