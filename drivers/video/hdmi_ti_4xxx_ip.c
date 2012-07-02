/*
 * hdmi_ti_4xxx_ip.c
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
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/omapfb.h>
#if defined(CONFIG_OMAP_REMOTE_PROC_IPU) && defined(CONFIG_RPMSG)
#include <linux/rpmsg.h>
#include <linux/remoteproc.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif
#include "hdmi_ti_4xxx_ip.h"

#define CEC_MAX_NUM_OPERANDS   14
#define HDMI_CORE_CEC_RETRY    200
#define HDMI_CEC_TX_CMD_RETRY  400

#if defined(CONFIG_OMAP_REMOTE_PROC_IPU) && defined(CONFIG_RPMSG)
static bool hdmi_acrwa_registered;
struct omap_chip_id audio_must_use_tclk;

struct payload_data {
	u32 cts_interval;
	u32 acr_rate;
	u32 sys_ck_rate;
	u32 trigger;
} hdmi_payload;

static void hdmi_acrwa_cb(struct rpmsg_channel *rpdev, void *data, int len,
			void *priv, u32 src)
{
	struct rproc *rproc;
	struct payload_data *payload = data;
	int err = 0;

	if  (!payload)
		pr_err("HDMI ACRWA: No payload received (src: 0x%x)\n", src);

	pr_info("HDMI ACRWA: ACRrate %d, CTSInterval %d, sys_clk %d,"
			"(src: 0x%x)\n", payload->acr_rate,
			payload->cts_interval, payload->sys_ck_rate, src);

	if (payload && payload->cts_interval == hdmi_payload.cts_interval &&
		payload->acr_rate == hdmi_payload.acr_rate &&
		payload->sys_ck_rate == hdmi_payload.sys_ck_rate &&
		payload->acr_rate && payload->sys_ck_rate &&
		payload->cts_interval) {

		hdmi_payload.trigger = 1;
		err = rpmsg_send(rpdev, &hdmi_payload, sizeof(hdmi_payload));
		if (err) {
			pr_err("HDMI ACRWA: rpmsg trigger start"
						"send failed: %d\n", err);
			hdmi_payload.trigger = 0;
			return;
		}
		/* Disable hibernation before Start HDMI ACRWA */
		rproc = rproc_get("ipu");
		pm_runtime_disable(rproc->dev);
		rproc_put(rproc);

	} else {
		pr_err("HDMI ACRWA: Wrong payload received\n");
	}
}

static int hdmi_acrwa_probe(struct rpmsg_channel *rpdev)
{
	int err = 0;
	struct clk *sys_ck;

	/* Sys clk rate is require to calculate cts_interval in ticks */
	sys_ck = clk_get(NULL, "sys_clkin_ck");

	if (IS_ERR(sys_ck)) {
		pr_err("HDMI ACRWA: Not able to obtain sys_clk\n");
		return -EINVAL;
	}

	hdmi_payload.sys_ck_rate = clk_get_rate(sys_ck);
	hdmi_payload.trigger = 0;

	/* send a message to our remote processor */
	pr_info("HDMI ACRWA: Send START msg from:0x%x to:0x%x\n",
					rpdev->src, rpdev->dst);
	err = rpmsg_send(rpdev, &hdmi_payload, sizeof(hdmi_payload));
	if (err)
		pr_err("HDMI ACRWA: rpmsg payload send failed: %d\n", err);

	return err;
}

static void __devexit hdmi_acrwa_remove(struct rpmsg_channel *rpdev)
{
	struct rproc *rproc;

	if (hdmi_payload.trigger) {
		hdmi_payload.cts_interval = 0;
		hdmi_payload.acr_rate = 0;
		hdmi_payload.sys_ck_rate = 0;
		hdmi_payload.trigger = 0;

		pr_info("HDMI ACRWA:Send STOP msg from:0x%x to:0x%x\n",
				rpdev->src, rpdev->dst);
		/* send a message to our remote processor */
		rpmsg_send(rpdev, &hdmi_payload, sizeof(hdmi_payload));

		/* Reenable hibernation after HDMI ACRWA stopped */
		rproc = rproc_get("ipu");
		pm_runtime_enable(rproc->dev);
		rproc_put(rproc);
	}
}

static struct rpmsg_device_id hdmi_acrwa_id_table[] = {
	{
		.name = "rpmsg-hdmiwa"
	},
	{ },
};
MODULE_DEVICE_TABLE(platform, hdmi_acrwa_id_table);

static struct rpmsg_driver hdmi_acrwa_driver = {
	.drv.name       = KBUILD_MODNAME,
	.drv.owner      = THIS_MODULE,
	.id_table = hdmi_acrwa_id_table,
	.probe    = hdmi_acrwa_probe,
	.callback = hdmi_acrwa_cb,
	.remove   = __devexit_p(hdmi_acrwa_remove),
};

int hdmi_lib_start_acr_wa(void)
{
	int ret = 0;

	if (omap_chip_is(audio_must_use_tclk)) {
		if (!hdmi_acrwa_registered) {
			ret = register_rpmsg_driver(&hdmi_acrwa_driver);
			if (ret) {
				pr_err("Error creating hdmi_acrwa driver\n");
				return ret;
			}

			hdmi_acrwa_registered = true;
		}
	}
	return ret;
}
void hdmi_lib_stop_acr_wa(void)
{
	if (omap_chip_is(audio_must_use_tclk)) {
		if (hdmi_acrwa_registered) {
			unregister_rpmsg_driver(&hdmi_acrwa_driver);
			hdmi_acrwa_registered = false;
		}
	}
}
#else
int hdmi_lib_start_acr_wa(void) { return 0; }
void hdmi_lib_stop_acr_wa(void) { }
#endif
static inline void hdmi_write_reg(void __iomem *base_addr,
				const struct hdmi_reg idx, u32 val)
{
	__raw_writel(val, base_addr + idx.idx);
}

static inline u32 hdmi_read_reg(void __iomem *base_addr,
				const struct hdmi_reg idx)
{
	return __raw_readl(base_addr + idx.idx);
}

static inline void __iomem *hdmi_wp_base(struct hdmi_ip_data *ip_data)
{
	return (void __iomem *) (ip_data->base_wp);
}

static inline void __iomem *hdmi_phy_base(struct hdmi_ip_data *ip_data)
{
	return (void __iomem *) (ip_data->base_wp + ip_data->hdmi_phy_offset);
}

static inline void __iomem *hdmi_pll_base(struct hdmi_ip_data *ip_data)
{
	return (void __iomem *)	(ip_data->base_wp + ip_data->hdmi_pll_offset);
}

static inline void __iomem *hdmi_av_base(struct hdmi_ip_data *ip_data)
{
	return (void __iomem *)
			(ip_data->base_wp + ip_data->hdmi_core_av_offset);
}

static inline void __iomem *hdmi_core_sys_base(struct hdmi_ip_data *ip_data)
{
	return (void __iomem *)
			(ip_data->base_wp + ip_data->hdmi_core_sys_offset);
}

static inline void __iomem *hdmi_core_cec_base(struct hdmi_ip_data *ip_data)
{
	return (void __iomem *)
			(ip_data->base_wp + ip_data->hdmi_cec_offset);
}

static inline int hdmi_wait_for_bit_change(void __iomem *base_addr,
				const struct hdmi_reg idx,
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

static int hdmi_pll_init(struct hdmi_ip_data *ip_data,
		enum hdmi_clk_refsel refsel, int dcofreq,
		struct hdmi_pll_info *fmt, u16 sd)
{
	u32 r;

	/* PLL start always use manual mode */
	REG_FLD_MOD(hdmi_pll_base(ip_data), PLLCTRL_PLL_CONTROL, 0x0, 0, 0);

	r = hdmi_read_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG1);
	r = FLD_MOD(r, fmt->regm, 20, 9); /* CFG1_PLL_REGM */
	r = FLD_MOD(r, fmt->regn, 8, 1);  /* CFG1_PLL_REGN */

	hdmi_write_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG1, r);

	r = hdmi_read_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG2);

	r = FLD_MOD(r, 0x0, 12, 12); /* PLL_HIGHFREQ divide by 2 */
	r = FLD_MOD(r, 0x1, 13, 13); /* PLL_REFEN */
	r = FLD_MOD(r, 0x0, 14, 14); /* PHY_CLKINEN de-assert during locking */

	if (dcofreq) {
		/* divider programming for frequency beyond 1000Mhz */
		REG_FLD_MOD(hdmi_pll_base(ip_data), PLLCTRL_CFG3, sd, 17, 10);
		r = FLD_MOD(r, 0x4, 3, 1); /* 1000MHz and 2000MHz */
	} else {
		r = FLD_MOD(r, 0x2, 3, 1); /* 500MHz and 1000MHz */
	}

	hdmi_write_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG2, r);

	r = hdmi_read_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG4);
	r = FLD_MOD(r, fmt->regm2, 24, 18);
	r = FLD_MOD(r, fmt->regmf, 17, 0);

	hdmi_write_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG4, r);

	/* go now */
	REG_FLD_MOD(hdmi_pll_base(ip_data), PLLCTRL_PLL_GO, 0x1, 0, 0);

	/* wait for PLL opertation to be over */
	if (hdmi_wait_for_bit_change(hdmi_pll_base(ip_data), PLLCTRL_PLL_GO,
							0, 0, 0)) {
		pr_err("PLL GO bit not set\n");
		return -ETIMEDOUT;
	}

	/* Wait till the lock bit is set in PLL status */
	if (hdmi_wait_for_bit_change(hdmi_pll_base(ip_data),
				PLLCTRL_PLL_STATUS, 1, 1, 1) != 1) {
		pr_err("cannot lock PLL\n");
		pr_err("CFG1 0x%x\n",
			hdmi_read_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG1));
		pr_err("CFG2 0x%x\n",
			hdmi_read_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG2));
		pr_err("CFG4 0x%x\n",
			hdmi_read_reg(hdmi_pll_base(ip_data), PLLCTRL_CFG4));
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
static int hdmi_set_phy_pwr(struct hdmi_ip_data *ip_data,
				enum hdmi_phy_pwr val,
				enum hdmi_pwrchg_reasons reason)
{
	/* FIXME audio driver should have already stopped, but not yet */
	bool wait_for_audio_stop = !(reason &
		(HDMI_PWRCHG_MODE_CHANGE | HDMI_PWRCHG_RESYNC));

	if (val == HDMI_PHYPWRCMD_OFF && wait_for_audio_stop)
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
int hdmi_ti_4xxx_set_pll_pwr(struct hdmi_ip_data *ip_data, enum hdmi_pll_pwr val)
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
EXPORT_SYMBOL(hdmi_ti_4xxx_set_pll_pwr);

static int hdmi_pll_reset(struct hdmi_ip_data *ip_data)
{
	/* SYSRESET  controlled by power FSM */
	REG_FLD_MOD(hdmi_pll_base(ip_data), PLLCTRL_PLL_CONTROL, 0x0, 3, 3);

	/* READ 0x0 reset is in progress */
	if (hdmi_wait_for_bit_change(hdmi_pll_base(ip_data),
				PLLCTRL_PLL_STATUS, 0, 0, 1) != 1) {
		pr_err("Failed to sysreset PLL\n");
		return -ETIMEDOUT;
	}

	return 0;
}

int hdmi_ti_4xxx_pll_program(struct hdmi_ip_data *ip_data,
				struct hdmi_pll_info *fmt)
{
	u16 r = 0;
	enum hdmi_clk_refsel refsel;

	r = hdmi_ti_4xxx_set_pll_pwr(ip_data, HDMI_PLLPWRCMD_ALLOFF);
	if (r)
		return r;

	r = hdmi_ti_4xxx_set_pll_pwr(ip_data, HDMI_PLLPWRCMD_BOTHON_ALLCLKS);
	if (r)
		return r;

	r = hdmi_pll_reset(ip_data);
	if (r)
		return r;

	refsel = HDMI_REFSEL_SYSCLK;

	r = hdmi_pll_init(ip_data, refsel, fmt->dcofreq, fmt, fmt->regsd);
	if (r)
		return r;

	return 0;
}

int hdmi_ti_4xxx_phy_init(struct hdmi_ip_data *ip_data, int phy)
{
	u16 r = 0;

	r = hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_LDOON,
			HDMI_PWRCHG_DEFAULT);
	if (r)
		return r;

	r = hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_TXON, HDMI_PWRCHG_DEFAULT);
	if (r)
		return r;

	/*
	 * Read address 0 in order to get the SCP reset done completed
	 * Dummy access performed to make sure reset is done
	 */
	hdmi_read_reg(hdmi_phy_base(ip_data), HDMI_TXPHY_TX_CTRL);

	/*
	 * Write to phy address 0 to configure the clock
	 * use HFBITCLK write HDMI_TXPHY_TX_CONTROL_FREQOUT field
	 */
	if (phy <= 50000)
		REG_FLD_MOD(hdmi_phy_base(ip_data), HDMI_TXPHY_TX_CTRL, 0x0, 31,
				30);
	else if ((50000 < phy) && (phy <= 100000))
		REG_FLD_MOD(hdmi_phy_base(ip_data), HDMI_TXPHY_TX_CTRL, 0x1, 31,
				30);
	else
		REG_FLD_MOD(hdmi_phy_base(ip_data), HDMI_TXPHY_TX_CTRL, 0x2, 31,
				30);

	/* Write to phy address 1 to start HDMI line (TXVALID and TMDSCLKEN) */
	hdmi_write_reg(hdmi_phy_base(ip_data),
					HDMI_TXPHY_DIGITAL_CTRL, 0xF0000000);

	/* Write to phy address 3 to change the polarity control */
	REG_FLD_MOD(hdmi_phy_base(ip_data),
					HDMI_TXPHY_PAD_CFG_CTRL, 0x1, 27, 27);

	return 0;
}

void hdmi_ti_4xxx_phy_off(struct hdmi_ip_data *ip_data,
			enum hdmi_pwrchg_reasons reason)
{
	hdmi_lib_stop_acr_wa();
	hdmi_set_phy_pwr(ip_data, HDMI_PHYPWRCMD_OFF, reason);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_phy_init);
EXPORT_SYMBOL(hdmi_ti_4xxx_phy_off);

static int hdmi_core_ddc_edid(struct hdmi_ip_data *ip_data,
						u8 *pedid, int ext)
{
	u32 i, j;
	char checksum = 0;
	u32 offset = 0;

	/* Turn on CLK for DDC */
	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_DPD, 0x7, 2, 0);

	/*
	 * SW HACK : Without the Delay DDC(i2c bus) reads 0 values /
	 * right shifted values( The behavior is not consistent and seen only
	 * with some TV's)
	 */
	msleep(300);

	if (!ext) {
		/* Clk SCL Devices */
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
						HDMI_CORE_DDC_CMD, 0xA, 3, 0);

		/* HDMI_CORE_DDC_STATUS_IN_PROG */
		if (hdmi_wait_for_bit_change(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_STATUS, 4, 4, 0) != 0) {
			pr_err("Failed to program DDC\n");
			return -ETIMEDOUT;
		}

		/* Clear FIFO */
		REG_FLD_MOD(hdmi_core_sys_base(ip_data)
						, HDMI_CORE_DDC_CMD, 0x9, 3, 0);

		/* HDMI_CORE_DDC_STATUS_IN_PROG */
		if (hdmi_wait_for_bit_change(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_STATUS, 4, 4, 0) != 0) {
			pr_err("Failed to program DDC\n");
			return -ETIMEDOUT;
		}

	} else {
		if (ext % 2 != 0)
			offset = 0x80;
	}

	/* Load Segment Address Register */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_SEGM, ext/2, 7, 0);

	/* Load Slave Address Register */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_ADDR, 0xA0 >> 1, 7, 1);

	/* Load Offset Address Register */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_OFFSET, offset, 7, 0);

	/* Load Byte Count */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_COUNT1, 0x80, 7, 0);
	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_COUNT2, 0x0, 1, 0);

	/* Set DDC_CMD */
	if (ext)
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_CMD, 0x4, 3, 0);
	else
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_CMD, 0x2, 3, 0);

	/* HDMI_CORE_DDC_STATUS_BUS_LOW */
	if (REG_GET(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_STATUS, 6, 6) == 1) {
		pr_err("I2C Bus Low?\n");
		return -EIO;
	}
	/* HDMI_CORE_DDC_STATUS_NO_ACK */
	if (REG_GET(hdmi_core_sys_base(ip_data),
					HDMI_CORE_DDC_STATUS, 5, 5) == 1) {
		pr_err("I2C No Ack\n");
		return -EIO;
	}

	i = ext * 128;
	j = 0;
	while (((REG_GET(hdmi_core_sys_base(ip_data),
			HDMI_CORE_DDC_STATUS, 4, 4) == 1) ||
			(REG_GET(hdmi_core_sys_base(ip_data),
			HDMI_CORE_DDC_STATUS, 2, 2) == 0)) && j < 128) {

		if (REG_GET(hdmi_core_sys_base(ip_data)
					, HDMI_CORE_DDC_STATUS, 2, 2) == 0) {
			/* FIFO not empty */
			pedid[i++] = REG_GET(hdmi_core_sys_base(ip_data),
						HDMI_CORE_DDC_DATA, 7, 0);
			j++;
		}
	}

	for (j = 0; j < 128; j++)
		checksum += pedid[j];

	if (checksum != 0) {
		pr_err("E-EDID checksum failed!!\n");
		return -EIO;
	}

	return 0;
}

int read_ti_4xxx_edid(struct hdmi_ip_data *ip_data, u8 *pedid, u16 max_length)
{
	int r = 0, n = 0, i = 0;
	int max_ext_blocks = (max_length / 128) - 1;

	r = hdmi_core_ddc_edid(ip_data, pedid, 0);
	if (r) {
		return r;
	} else {
		n = pedid[0x7e];

		/*
		 * README: need to comply with max_length set by the caller.
		 * Better implementation should be to allocate necessary
		 * memory to store EDID according to nb_block field found
		 * in first block
		 */
		if (n > max_ext_blocks)
			n = max_ext_blocks;

		for (i = 1; i <= n; i++) {
			r = hdmi_core_ddc_edid(ip_data, pedid, i);
			if (r)
				return r;
		}
	}
	return 0;
}
EXPORT_SYMBOL(read_ti_4xxx_edid);

static void hdmi_core_init(enum hdmi_deep_color_mode deep_color,
			struct hdmi_core_video_config *video_cfg,
			struct hdmi_core_infoframe_avi *avi_cfg,
			struct hdmi_core_packet_enable_repeat *repeat_cfg)
{
	pr_debug("Enter hdmi_core_init\n");

	/* video core */
	switch (deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		video_cfg->ip_bus_width = HDMI_INPUT_10BIT;
		video_cfg->op_dither_truc = HDMI_OUTPUTTRUNCATION_10BIT;
		video_cfg->deep_color_pkt = HDMI_DEEPCOLORPACKECTENABLE;
		video_cfg->pkt_mode = HDMI_PACKETMODE30BITPERPIXEL;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		video_cfg->ip_bus_width = HDMI_INPUT_12BIT;
		video_cfg->op_dither_truc = HDMI_OUTPUTTRUNCATION_12BIT;
		video_cfg->deep_color_pkt = HDMI_DEEPCOLORPACKECTENABLE;
		video_cfg->pkt_mode = HDMI_PACKETMODE36BITPERPIXEL;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		video_cfg->ip_bus_width = HDMI_INPUT_8BIT;
		video_cfg->op_dither_truc = HDMI_OUTPUTTRUNCATION_8BIT;
		video_cfg->deep_color_pkt = HDMI_DEEPCOLORPACKECTDISABLE;
		video_cfg->pkt_mode = HDMI_PACKETMODERESERVEDVALUE;
		break;
	}

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

	/* sys_ctrl1 default configuration not tunable */
	r = hdmi_read_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CTRL1);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_VEN_FOLLOWVSYNC, 5, 5);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_HEN_FOLLOWHSYNC, 4, 4);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_BSEL_24BITBUS, 2, 2);
	r = FLD_MOD(r, HDMI_CORE_CTRL1_EDGE_RISINGEDGE, 1, 1);
	/* PD bit has to be written to recieve the interrupts */
	r = FLD_MOD(r, HDMI_CORE_CTRL1_POWER_DOWN, 0, 0);
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_CTRL1, r);

	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_SYS_VID_ACEN, cfg->ip_bus_width, 7, 6);

	/* Vid_Mode */
	r = hdmi_read_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_SYS_VID_MODE);

	/* dither truncation configuration */
	if (cfg->op_dither_truc > HDMI_OUTPUTTRUNCATION_12BIT) {
		r = FLD_MOD(r, cfg->op_dither_truc - 3, 7, 6);
		r = FLD_MOD(r, 1, 5, 5);
	} else {
		r = FLD_MOD(r, cfg->op_dither_truc, 7, 6);
		r = FLD_MOD(r, 0, 5, 5);
	}
	hdmi_write_reg(hdmi_core_sys_base(ip_data), HDMI_CORE_SYS_VID_MODE, r);

	/* HDMI_Ctrl */
	r = hdmi_read_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_HDMI_CTRL);
	r = FLD_MOD(r, cfg->deep_color_pkt, 6, 6);
	r = FLD_MOD(r, cfg->pkt_mode, 5, 3);
	r = FLD_MOD(r, cfg->hdmi_dvi, 0, 0);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_HDMI_CTRL, r);

	/* TMDS_CTRL */
	REG_FLD_MOD(hdmi_core_sys_base(ip_data),
			HDMI_CORE_SYS_TMDS_CTRL, cfg->tclk_sel_clkmult, 6, 5);
}

static void hdmi_core_aux_infoframe_avi_config(struct hdmi_ip_data *ip_data,
		struct hdmi_core_infoframe_avi info_avi)
{
	u32 val;
	char sum = 0, checksum = 0;

	sum += 0x82 + 0x002 + 0x00D;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_TYPE, 0x082);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_VERS, 0x002);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_LEN, 0x00D);

	val = (info_avi.db1_format << 5) |
		(info_avi.db1_active_info << 4) |
		(info_avi.db1_bar_info_dv << 2) |
		(info_avi.db1_scan_info);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(0), val);
	sum += val;

	val = (info_avi.db2_colorimetry << 6) |
		(info_avi.db2_aspect_ratio << 4) |
		(info_avi.db2_active_fmt_ar);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(1), val);
	sum += val;

	val = (info_avi.db3_itc << 7) |
		(info_avi.db3_ec << 4) |
		(info_avi.db3_q_range << 2) |
		(info_avi.db3_nup_scaling);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(2), val);
	sum += val;

	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(3),
					info_avi.db4_videocode);
	sum += info_avi.db4_videocode;

	val = info_avi.db5_pixel_repeat;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(4), val);
	sum += val;

	val = info_avi.db6_7_line_eoftop & 0x00FF;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(5), val);
	sum += val;

	val = ((info_avi.db6_7_line_eoftop >> 8) & 0x00FF);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(6), val);
	sum += val;

	val = info_avi.db8_9_line_sofbottom & 0x00FF;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(7), val);
	sum += val;

	val = ((info_avi.db8_9_line_sofbottom >> 8) & 0x00FF);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(8), val);
	sum += val;

	val = info_avi.db10_11_pixel_eofleft & 0x00FF;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(9), val);
	sum += val;

	val = ((info_avi.db10_11_pixel_eofleft >> 8) & 0x00FF);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(10), val);
	sum += val;

	val = info_avi.db12_13_pixel_sofright & 0x00FF;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(11), val);
	sum += val;

	val = ((info_avi.db12_13_pixel_sofright >> 8) & 0x00FF);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_DBYTE(12), val);
	sum += val;

	checksum = 0x100 - sum;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AVI_CHSUM, checksum);
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

void hdmi_core_vsi_config(struct hdmi_ip_data *ip_data,
		struct hdmi_core_vendor_specific_infoframe *config)
{
	u8 sum = 0, i;
	/*For side-by-side(HALF) we need to specify subsampling in 3D_ext_data*/
	int length = config->s3d_structure > 0x07 ? 6 : 5;
	u8 info_frame_packet[] = {
		0x81, /*Vendor-Specific InfoFrame*/
		0x01, /*InfoFrame version number per CEA-861-D*/
		length, /*InfoFrame length, excluding checksum and header*/
		0x00, /*Checksum*/
		0x03, 0x0C, 0x00, /*24-bit IEEE Registration Ident*/
		0x40, /*3D format indication preset, 3D_Struct follows*/
		config->s3d_structure << 4, /*3D_Struct, no 3D_Meta*/
		config->s3d_ext_data << 4,/*3D_Ext_Data*/
	};

	if (!config->enable) {
		REG_FLD_MOD(hdmi_av_base(ip_data),
			HDMI_CORE_AV_PB_CTRL2, 0, 1, 0);
		return;
	}

	/*Adding packet header and checksum length*/
	length += 4;

	/*Checksum is packet_header+checksum+infoframe_length = 0*/
	for (i = 0; i < length; i++)
		sum += info_frame_packet[i];
	info_frame_packet[3] = 0x100-sum;

	for (i = 0; i < length; i++)
		hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_GEN_DBYTE(i),
						info_frame_packet[i]);

	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_PB_CTRL2, 0x3, 1, 0);
	return;
}
EXPORT_SYMBOL(hdmi_core_vsi_config);


static void hdmi_wp_init(struct omap_video_timings *timings,
			struct hdmi_video_format *video_fmt,
			struct hdmi_video_interface *video_int)
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

	video_int->vsp = 0;
	video_int->hsp = 0;

	video_int->interlacing = 0;
	video_int->tm = 0; /* HDMI_TIMING_SLAVE */

}

void hdmi_ti_4xxx_wp_video_start(struct hdmi_ip_data *ip_data, bool start)
{
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG, start, 31, 31);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_wp_video_start);

int hdmi_ti_4xxx_wp_get_video_state(struct hdmi_ip_data *ip_data)
{
	u32 status = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG);

	return (status & 0x80000000) ? 1 : 0;
}

int hdmi_ti_4xxx_set_wait_soft_reset(struct hdmi_ip_data *ip_data)
{
	u8 count = 0;

	/* reset W1 */
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_SYSCONFIG, 0x1, 0, 0);

	/* wait till SOFTRESET == 0 */
	while (hdmi_wait_for_bit_change(hdmi_wp_base(ip_data),
					HDMI_WP_SYSCONFIG, 0, 0, 0) != 0) {
		if (count++ > 10) {
			pr_err("SYSCONFIG[SOFTRESET] bit not set to 0\n");
			return -ETIMEDOUT;
		}
	}

	/* Make madule smart and wakeup capable*/
	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_SYSCONFIG, 0x3, 3, 2);

	return 0;
}


static void hdmi_wp_video_init_format(struct hdmi_video_format *video_fmt,
	struct omap_video_timings *timings, struct hdmi_config *param)
{
	pr_debug("Enter hdmi_wp_video_init_format\n");

	video_fmt->y_res = param->timings.yres;
	if (param->timings.vmode & FB_VMODE_INTERLACED)
		video_fmt->y_res /= 2;
	video_fmt->x_res = param->timings.xres;

	omapfb_fb2dss_timings(&param->timings, timings);
}

static void hdmi_wp_video_config_format(struct hdmi_ip_data *ip_data,
		struct hdmi_video_format *video_fmt)
{
	u32 l = 0;

	REG_FLD_MOD(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG,
			video_fmt->packing_mode, 10, 8);

	l |= FLD_VAL(video_fmt->y_res, 31, 16);
	l |= FLD_VAL(video_fmt->x_res, 15, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_SIZE, l);
}

static void hdmi_wp_video_config_interface(struct hdmi_ip_data *ip_data,
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

static void hdmi_wp_video_config_timing(struct hdmi_ip_data *ip_data,
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

static void hdmi_wp_core_interrupt_set(struct hdmi_ip_data *ip_data, u32 val)
{
	u32	irqStatus;
	irqStatus = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_IRQENABLE_SET);
	pr_debug("[HDMI] WP_IRQENABLE_SET..currently reads as:%x\n", irqStatus);
	irqStatus = irqStatus | val;
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_IRQENABLE_SET, irqStatus);
	pr_debug("[HDMI]WP_IRQENABLE_SET..changed to :%x\n", irqStatus);
}

void hdmi_ti_4xxx_basic_configure(struct hdmi_ip_data *ip_data,
			struct hdmi_config *cfg)
{
	/* HDMI */
	struct omap_video_timings video_timing;
	struct hdmi_video_format video_format;
	struct hdmi_video_interface video_interface;
	/* HDMI core */
	struct hdmi_core_infoframe_avi avi_cfg;
	struct hdmi_core_video_config v_core_cfg;
	struct hdmi_core_packet_enable_repeat repeat_cfg;

	hdmi_wp_init(&video_timing, &video_format,
		&video_interface);

	hdmi_core_init(cfg->deep_color, &v_core_cfg,
		&avi_cfg,
		&repeat_cfg);

	hdmi_wp_core_interrupt_set(ip_data, HDMI_WP_IRQENABLE_CORE |
				HDMI_WP_AUDIO_FIFO_UNDERFLOW);

	hdmi_wp_video_init_format(&video_format, &video_timing, cfg);

	hdmi_wp_video_config_timing(ip_data, &video_timing);

	/* video config */
	video_format.packing_mode = HDMI_PACK_24b_RGB_YUV444_YUV422;

	hdmi_wp_video_config_format(ip_data, &video_format);

	video_interface.vsp = !!(cfg->timings.sync & FB_SYNC_VERT_HIGH_ACT);
	video_interface.hsp = !!(cfg->timings.sync & FB_SYNC_HOR_HIGH_ACT);
	video_interface.interlacing = cfg->timings.vmode & FB_VMODE_INTERLACED;
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
	avi_cfg.db1_format = HDMI_INFOFRAME_AVI_DB1Y_RGB;
	avi_cfg.db1_active_info =
		HDMI_INFOFRAME_AVI_DB1A_ACTIVE_FORMAT_OFF;
	avi_cfg.db1_bar_info_dv = HDMI_INFOFRAME_AVI_DB1B_NO;
	avi_cfg.db1_scan_info = HDMI_INFOFRAME_AVI_DB1S_0;
	avi_cfg.db2_colorimetry = HDMI_INFOFRAME_AVI_DB2C_NO;
	avi_cfg.db2_aspect_ratio = HDMI_INFOFRAME_AVI_DB2M_NO;
	if (cfg->cm.mode == HDMI_HDMI && cfg->cm.code < CEA_MODEDB_SIZE) {
		if (cea_modes[cfg->cm.code].flag & FB_FLAG_RATIO_16_9)
			avi_cfg.db2_aspect_ratio = HDMI_INFOFRAME_AVI_DB2M_169;
		else if (cea_modes[cfg->cm.code].flag & FB_FLAG_RATIO_4_3)
			avi_cfg.db2_aspect_ratio = HDMI_INFOFRAME_AVI_DB2M_43;
	}
	avi_cfg.db2_active_fmt_ar = HDMI_INFOFRAME_AVI_DB2R_SAME;
	avi_cfg.db3_itc = HDMI_INFOFRAME_AVI_DB3ITC_NO;
	avi_cfg.db3_ec = HDMI_INFOFRAME_AVI_DB3EC_XVYUV601;

	if (cfg->cm.mode == HDMI_DVI ||
	(cfg->cm.code == 1 && cfg->cm.mode == HDMI_HDMI)) {
		/* setting for FULL RANGE MODE */
		pr_debug("infoframe avi full range\n");
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
				HDMI_CORE_SYS_VID_MODE, 1, 4, 4);
		avi_cfg.db3_q_range = HDMI_INFOFRAME_AVI_DB3Q_FR;
	} else {
		/* setting for LIMITED RANGE MODE */
		pr_debug("infoframe avi limited range\n");
		REG_FLD_MOD(hdmi_core_sys_base(ip_data),
				HDMI_CORE_SYS_VID_ACEN, 1, 1, 1);
		avi_cfg.db3_q_range = HDMI_INFOFRAME_AVI_DB3Q_LR;
	}

	avi_cfg.db3_nup_scaling = HDMI_INFOFRAME_AVI_DB3SC_NO;
	avi_cfg.db4_videocode = cfg->cm.code;
	avi_cfg.db5_pixel_repeat = HDMI_INFOFRAME_AVI_DB5PR_NO;
	avi_cfg.db6_7_line_eoftop = 0;
	avi_cfg.db8_9_line_sofbottom = 0;
	avi_cfg.db10_11_pixel_eofleft = 0;
	avi_cfg.db12_13_pixel_sofright = 0;

	hdmi_core_aux_infoframe_avi_config(ip_data, avi_cfg);

	/* enable/repeat the infoframe */
	repeat_cfg.avi_infoframe = HDMI_PACKETENABLE;
	repeat_cfg.avi_infoframe_repeat = HDMI_PACKETREPEATON;
	/* wakeup */
	repeat_cfg.audio_pkt = HDMI_PACKETENABLE;
	repeat_cfg.audio_pkt_repeat = HDMI_PACKETREPEATON;
	hdmi_core_av_packet_config(ip_data, repeat_cfg);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_basic_configure);

u32 hdmi_ti_4xxx_irq_handler(struct hdmi_ip_data *ip_data)
{
	u32 val, sys_stat = 0, core_state = 0;
	u32 intr2 = 0, intr3 = 0, intr4 = 0, r = 0;
	void __iomem *wp_base = hdmi_wp_base(ip_data);
	void __iomem *core_base = hdmi_core_sys_base(ip_data);

	pr_debug("Enter hdmi_ti_4xxx_irq_handler\n");

	val = hdmi_read_reg(wp_base, HDMI_WP_IRQSTATUS);
	if (val & HDMI_WP_IRQSTATUS_CORE) {
		core_state = hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR_STATE);
		if (core_state & 0x1) {
			sys_stat = hdmi_read_reg(core_base,
						 HDMI_CORE_SYS_SYS_STAT);
			intr2 = hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR2);
			intr3 = hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR3);
			intr4 = hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR4);

			pr_debug("HDMI_CORE_SYS_SYS_STAT = 0x%x\n", sys_stat);
			pr_debug("HDMI_CORE_SYS_INTR2 = 0x%x\n", intr2);
			pr_debug("HDMI_CORE_SYS_INTR3 = 0x%x\n", intr3);

			hdmi_write_reg(core_base, HDMI_CORE_SYS_INTR2, intr2);
			hdmi_write_reg(core_base, HDMI_CORE_SYS_INTR3, intr3);
			hdmi_write_reg(core_base, HDMI_CORE_SYS_INTR4, intr4);

			hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR2);
			hdmi_read_reg(core_base, HDMI_CORE_SYS_INTR3);
		}
	}

	if (val & HDMI_WP_AUDIO_FIFO_UNDERFLOW)
		pr_err("HDMI_WP_AUDIO_FIFO_UNDERFLOW\n");

	pr_debug("HDMI_WP_IRQSTATUS = 0x%x\n", val);
	pr_debug("HDMI_CORE_SYS_INTR_STATE = 0x%x\n", core_state);

	if (intr2 & HDMI_CORE_SYSTEM_INTR2__BCAP)
		r |= HDMI_BCAP;

	if (intr3 & HDMI_CORE_SYSTEM_INTR3__RI_ERR)
		r |= HDMI_RI_ERR;

	if (intr4 & HDMI_CORE_SYSTEM_INTR4_CEC)
		r |= HDMI_CEC_INT;

	/* Ack other interrupts if any */
	hdmi_write_reg(wp_base, HDMI_WP_IRQSTATUS, val);
	/* flush posted write */
	hdmi_read_reg(wp_base, HDMI_WP_IRQSTATUS);
	return r;
}
EXPORT_SYMBOL(hdmi_ti_4xxx_irq_handler);

void hdmi_ti_4xxx_dump_regs(struct hdmi_ip_data *ip_data, struct seq_file *s)
{
#define DUMPREG(g, r) seq_printf(s, "%-35s %08x\n", #r, hdmi_read_reg(g, r))

	void __iomem *wp_base = hdmi_wp_base(ip_data);
	void __iomem *core_sys_base = hdmi_core_sys_base(ip_data);
	void __iomem *phy_base = hdmi_phy_base(ip_data);
	void __iomem *pll_base = hdmi_pll_base(ip_data);
	void __iomem *av_base = hdmi_av_base(ip_data);

	/* wrapper registers */
	DUMPREG(wp_base, HDMI_WP_REVISION);
	DUMPREG(wp_base, HDMI_WP_SYSCONFIG);
	DUMPREG(wp_base, HDMI_WP_IRQSTATUS_RAW);
	DUMPREG(wp_base, HDMI_WP_IRQSTATUS);
	DUMPREG(wp_base, HDMI_WP_PWR_CTRL);
	DUMPREG(wp_base, HDMI_WP_IRQENABLE_SET);
	DUMPREG(wp_base, HDMI_WP_VIDEO_SIZE);
	DUMPREG(wp_base, HDMI_WP_VIDEO_TIMING_H);
	DUMPREG(wp_base, HDMI_WP_VIDEO_TIMING_V);
	DUMPREG(wp_base, HDMI_WP_WP_CLK);

	DUMPREG(core_sys_base, HDMI_CORE_SYS_VND_IDL);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DEV_IDL);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DEV_IDH);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DEV_REV);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_SRST);
	DUMPREG(core_sys_base, HDMI_CORE_CTRL1);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_SYS_STAT);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_VID_ACEN);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_VID_MODE);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_INTR_STATE);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_INTR1);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_INTR2);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_INTR3);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_INTR4);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_UMASK1);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_TMDS_CTRL);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_DLY);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_CTRL);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_TOP);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_CNTL);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_CNTH);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_LINL);
	DUMPREG(core_sys_base, HDMI_CORE_SYS_DE_LINH_1);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_CMD);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_STATUS);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_ADDR);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_OFFSET);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_COUNT1);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_COUNT2);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_DATA);
	DUMPREG(core_sys_base, HDMI_CORE_DDC_SEGM);

	DUMPREG(av_base, HDMI_CORE_AV_HDMI_CTRL);
	DUMPREG(av_base, HDMI_CORE_AV_AVI_DBYTE_NELEMS);
	DUMPREG(av_base, HDMI_CORE_AV_SPD_DBYTE);
	DUMPREG(av_base, HDMI_CORE_AV_SPD_DBYTE_NELEMS);
	DUMPREG(av_base, HDMI_CORE_AV_AUD_DBYTE_NELEMS);
	DUMPREG(av_base, HDMI_CORE_AV_MPEG_DBYTE);
	DUMPREG(av_base, HDMI_CORE_AV_MPEG_DBYTE_NELEMS);
	DUMPREG(av_base, HDMI_CORE_AV_GEN_DBYTE_NELEMS);
	DUMPREG(av_base, HDMI_CORE_AV_GEN2_DBYTE);
	DUMPREG(av_base, HDMI_CORE_AV_GEN2_DBYTE_NELEMS);
	DUMPREG(av_base, HDMI_CORE_AV_ACR_CTRL);
	DUMPREG(av_base, HDMI_CORE_AV_FREQ_SVAL);
	DUMPREG(av_base, HDMI_CORE_AV_N_SVAL1);
	DUMPREG(av_base, HDMI_CORE_AV_N_SVAL2);
	DUMPREG(av_base, HDMI_CORE_AV_N_SVAL3);
	DUMPREG(av_base, HDMI_CORE_AV_CTS_SVAL1);
	DUMPREG(av_base, HDMI_CORE_AV_CTS_SVAL2);
	DUMPREG(av_base, HDMI_CORE_AV_CTS_SVAL3);
	DUMPREG(av_base, HDMI_CORE_AV_CTS_HVAL1);
	DUMPREG(av_base, HDMI_CORE_AV_CTS_HVAL2);
	DUMPREG(av_base, HDMI_CORE_AV_CTS_HVAL3);
	DUMPREG(av_base, HDMI_CORE_AV_AUD_MODE);
	DUMPREG(av_base, HDMI_CORE_AV_SPDIF_CTRL);
	DUMPREG(av_base, HDMI_CORE_AV_HW_SPDIF_FS);
	DUMPREG(av_base, HDMI_CORE_AV_SWAP_I2S);
	DUMPREG(av_base, HDMI_CORE_AV_SPDIF_ERTH);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_IN_MAP);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_IN_CTRL);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_CHST0);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_CHST1);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_CHST2);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_CHST4);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_CHST5);
	DUMPREG(av_base, HDMI_CORE_AV_ASRC);
	DUMPREG(av_base, HDMI_CORE_AV_I2S_IN_LEN);
	DUMPREG(av_base, HDMI_CORE_AV_AUDO_TXSTAT);
	DUMPREG(av_base, HDMI_CORE_AV_AUD_PAR_BUSCLK_1);
	DUMPREG(av_base, HDMI_CORE_AV_AUD_PAR_BUSCLK_2);
	DUMPREG(av_base, HDMI_CORE_AV_AUD_PAR_BUSCLK_3);
	DUMPREG(av_base, HDMI_CORE_AV_TEST_TXCTRL);

	DUMPREG(av_base, HDMI_CORE_AV_DPD);
	DUMPREG(av_base, HDMI_CORE_AV_PB_CTRL1);
	DUMPREG(av_base, HDMI_CORE_AV_PB_CTRL2);
	DUMPREG(av_base, HDMI_CORE_AV_AVI_TYPE);
	DUMPREG(av_base, HDMI_CORE_AV_AVI_VERS);
	DUMPREG(av_base, HDMI_CORE_AV_AVI_LEN);
	DUMPREG(av_base, HDMI_CORE_AV_AVI_CHSUM);
	DUMPREG(av_base, HDMI_CORE_AV_SPD_TYPE);
	DUMPREG(av_base, HDMI_CORE_AV_SPD_VERS);
	DUMPREG(av_base, HDMI_CORE_AV_SPD_LEN);
	DUMPREG(av_base, HDMI_CORE_AV_SPD_CHSUM);
	DUMPREG(av_base, HDMI_CORE_AV_AUDIO_TYPE);
	DUMPREG(av_base, HDMI_CORE_AV_AUDIO_VERS);
	DUMPREG(av_base, HDMI_CORE_AV_AUDIO_LEN);
	DUMPREG(av_base, HDMI_CORE_AV_AUDIO_CHSUM);
	DUMPREG(av_base, HDMI_CORE_AV_MPEG_TYPE);
	DUMPREG(av_base, HDMI_CORE_AV_MPEG_VERS);
	DUMPREG(av_base, HDMI_CORE_AV_MPEG_LEN);
	DUMPREG(av_base, HDMI_CORE_AV_MPEG_CHSUM);
	DUMPREG(av_base, HDMI_CORE_AV_CP_BYTE1);
	DUMPREG(av_base, HDMI_CORE_AV_CEC_ADDR_ID);

	DUMPREG(pll_base, PLLCTRL_PLL_CONTROL);
	DUMPREG(pll_base, PLLCTRL_PLL_STATUS);
	DUMPREG(pll_base, PLLCTRL_PLL_GO);
	DUMPREG(pll_base, PLLCTRL_CFG1);
	DUMPREG(pll_base, PLLCTRL_CFG2);
	DUMPREG(pll_base, PLLCTRL_CFG3);
	DUMPREG(pll_base, PLLCTRL_CFG4);

	DUMPREG(phy_base, HDMI_TXPHY_TX_CTRL);
	DUMPREG(phy_base, HDMI_TXPHY_DIGITAL_CTRL);
	DUMPREG(phy_base, HDMI_TXPHY_POWER_CTRL);
	DUMPREG(phy_base, HDMI_TXPHY_PAD_CFG_CTRL);

#undef DUMPREG
}
EXPORT_SYMBOL(hdmi_ti_4xxx_dump_regs);

int hdmi_ti_4xxx_config_audio_acr(struct hdmi_ip_data *ip_data,
				u32 sample_freq, u32 *n, u32 *cts, u32 pclk)
{
	u32 r;
	u32 deep_color = 0;
#if defined(CONFIG_OMAP_REMOTE_PROC_IPU) && defined(CONFIG_RPMSG)
	u32 cts_interval_qtt, cts_interval_res, n_val, cts_interval;
#endif
	if (n == NULL || cts == NULL)
		return -EINVAL;
	/*
	 * Obtain current deep color configuration. This needed
	 * to calculate the TMDS clock based on the pixel clock.
	 */
	r = REG_GET(hdmi_wp_base(ip_data), HDMI_WP_VIDEO_CFG, 1, 0);


	switch (r) {
	case 1: /* No deep color selected */
		deep_color = 100;
		break;
	case 2: /* 10-bit deep color selected */
		deep_color = 125;
		break;
	case 3: /* 12-bit deep color selected */
		deep_color = 150;
		break;
	default:
		return -EINVAL;
	}

	switch (sample_freq) {
	case 32000:
		if ((deep_color == 125) && ((pclk == 54054)
				|| (pclk == 74250)))
			*n = 8192;
		else
			*n = 4096;
		break;
	case 44100:
		*n = 6272;
		break;
	case 48000:
		if ((deep_color == 125) && ((pclk == 54054)
				|| (pclk == 74250)))
			*n = 8192;
		else
			*n = 6144;
		break;
	default:
		*n = 0;
		return -EINVAL;
	}

	/* Calculate CTS. See HDMI 1.3a or 1.4a specifications */
	*cts = pclk * (*n / 128) * deep_color / (sample_freq / 10);

#if defined(CONFIG_OMAP_REMOTE_PROC_IPU) && defined(CONFIG_RPMSG)
	if (omap_chip_is(audio_must_use_tclk)) {
		n_val = *n;
		cts_interval = 0;
		if (pclk && deep_color) {
			cts_interval_qtt = 1000000 /
				((pclk * deep_color) / 100);
			cts_interval_res = 1000000 %
				((pclk * deep_color) / 100);
			cts_interval = (cts_interval_res * n_val) /
					((pclk * deep_color) / 100);
			cts_interval += cts_interval_qtt * n_val;
		}
		hdmi_payload.cts_interval = cts_interval;
		hdmi_payload.acr_rate = 128 * sample_freq / n_val;
	}
#endif
	return 0;
}
EXPORT_SYMBOL(hdmi_ti_4xxx_config_audio_acr);


void hdmi_ti_4xxx_wp_audio_config_format(struct hdmi_ip_data *ip_data,
					struct hdmi_audio_format *aud_fmt)
{
	u32 r, reset_wp;

	pr_debug("Enter hdmi_wp_audio_config_format\n");

	reset_wp = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CTRL);
	/* Reset HDMI wrapper */
	if (reset_wp & 0x80000000)
		REG_FLD_MOD(hdmi_wp_base(ip_data),
		HDMI_WP_AUDIO_CTRL, 0, 31, 31);

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

	if (r & 0x80000000)
		REG_FLD_MOD(hdmi_wp_base(ip_data),
		HDMI_WP_AUDIO_CTRL, 1, 31, 31);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_wp_audio_config_format);

void hdmi_ti_4xxx_wp_audio_config_dma(struct hdmi_ip_data *ip_data,
					struct hdmi_audio_dma *aud_dma)
{
	u32 r;

	pr_debug("Enter hdmi_wp_audio_config_dma\n");

	r = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CFG2);
	r = FLD_MOD(r, aud_dma->transfer_size, 15, 8);
	r = FLD_MOD(r, aud_dma->block_size, 7, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CFG2, r);

	r = hdmi_read_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CTRL);
	r = FLD_MOD(r, aud_dma->mode, 9, 9);
	r = FLD_MOD(r, aud_dma->fifo_threshold, 8, 0);
	hdmi_write_reg(hdmi_wp_base(ip_data), HDMI_WP_AUDIO_CTRL, r);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_wp_audio_config_dma);


void hdmi_ti_4xxx_core_audio_config(struct hdmi_ip_data *ip_data,
					struct hdmi_core_audio_config *cfg)
{
	u32 r;

	/* audio clock recovery parameters */
	r = hdmi_read_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_ACR_CTRL);
	/*
	 * MCLK_EN: use TCLK for ACR packets. For devices that use
	 * the MCLK, this is the first part of the MCLK initialization
	 */
	r = FLD_MOD(r, 0, 2, 2);
	r = FLD_MOD(r, cfg->en_acr_pkt, 1, 1);
	r = FLD_MOD(r, cfg->cts_mode, 0, 0);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_ACR_CTRL, r);

	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_N_SVAL1, cfg->n, 7, 0);
	REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_N_SVAL2, cfg->n >> 8, 7, 0);
	REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_N_SVAL3, cfg->n >> 16, 7, 0);

	if (cfg->use_mclk)
		REG_FLD_MOD(hdmi_av_base(ip_data),
			HDMI_CORE_AV_FREQ_SVAL, cfg->mclk_mode, 2, 0);

	if (cfg->cts_mode == HDMI_AUDIO_CTS_MODE_SW) {
		REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_CTS_SVAL1, cfg->cts, 7, 0);
		REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_CTS_SVAL2, cfg->cts >> 8, 7, 0);
		REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_CTS_SVAL3, cfg->cts >> 16, 7, 0);
	} else {
		/* Configure clock for audio packets */
		REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_AUD_PAR_BUSCLK_1,
				cfg->aud_par_busclk, 7, 0);
		REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_AUD_PAR_BUSCLK_2,
				(cfg->aud_par_busclk >> 8), 7, 0);
		REG_FLD_MOD(hdmi_av_base(ip_data),
				HDMI_CORE_AV_AUD_PAR_BUSCLK_3,
				(cfg->aud_par_busclk >> 16), 7, 0);
	}

	/* For devices using MCLK, this completes its initialization. */
	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_ACR_CTRL,
							cfg->use_mclk, 2, 2);

	/* Override of SPDIF sample frequency with value in I2S_CHST4 */
	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_SPDIF_CTRL,
						cfg->fs_override, 1, 1);

	/* I2S parameters */
	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_I2S_CHST4,
						cfg->freq_sample, 3, 0);

	r = hdmi_read_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_I2S_IN_CTRL);
	r = FLD_MOD(r, cfg->i2s_cfg.en_high_bitrate_aud, 7, 7);
	r = FLD_MOD(r, cfg->i2s_cfg.sck_edge_mode, 6, 6);
	r = FLD_MOD(r, cfg->i2s_cfg.cbit_order, 5, 5);
	r = FLD_MOD(r, cfg->i2s_cfg.vbit, 4, 4);
	r = FLD_MOD(r, cfg->i2s_cfg.ws_polarity, 3, 3);
	r = FLD_MOD(r, cfg->i2s_cfg.justification, 2, 2);
	r = FLD_MOD(r, cfg->i2s_cfg.direction, 1, 1);
	r = FLD_MOD(r, cfg->i2s_cfg.shift, 0, 0);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_I2S_IN_CTRL, r);

	r = hdmi_read_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_I2S_CHST5);
	r = FLD_MOD(r, cfg->freq_sample, 7, 4);
	r = FLD_MOD(r, cfg->i2s_cfg.word_length, 3, 1);
	r = FLD_MOD(r, cfg->i2s_cfg.word_max_length, 0, 0);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_I2S_CHST5, r);

	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_I2S_IN_LEN,
					cfg->i2s_cfg.in_length_bits, 3, 0);

	/* Audio channels and mode parameters */
	REG_FLD_MOD(hdmi_av_base(ip_data), HDMI_CORE_AV_HDMI_CTRL,
							cfg->layout, 2, 1);
	r = hdmi_read_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_MODE);
	r = FLD_MOD(r, cfg->i2s_cfg.active_sds, 7, 4);
	r = FLD_MOD(r, cfg->en_dsd_audio, 3, 3);
	r = FLD_MOD(r, cfg->en_parallel_aud_input, 2, 2);
	r = FLD_MOD(r, cfg->en_spdif, 1, 1);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_MODE, r);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_core_audio_config);

void hdmi_ti_4xxx_core_audio_infoframe_config(struct hdmi_ip_data *ip_data,
		struct hdmi_core_infoframe_audio *info_aud)
{
	u8 val;
	u8 sum = 0, checksum = 0;

	/*
	 * Set audio info frame type, version and length as
	 * described in HDMI 1.4a Section 8.2.2 specification.
	 * Checksum calculation is defined in Section 5.3.5.
	 */
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUDIO_TYPE, 0x84);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUDIO_VERS, 0x01);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUDIO_LEN, 0x0a);
	sum += 0x84 + 0x001 + 0x00a;

	val = (info_aud->db1_coding_type << 4)
			| (info_aud->db1_channel_count - 1);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(0), val);
	sum += val;

	val = (info_aud->db2_sample_freq << 2) | info_aud->db2_sample_size;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(1), val);
	sum += val;

	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(2), 0x00);

	val = info_aud->db4_channel_alloc;
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(3), val);
	sum += val;

	val = (info_aud->db5_downmix_inh << 7) | (info_aud->db5_lsv << 3);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(4), val);
	sum += val;

	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(5), 0x00);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(6), 0x00);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(7), 0x00);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(8), 0x00);
	hdmi_write_reg(hdmi_av_base(ip_data), HDMI_CORE_AV_AUD_DBYTE(9), 0x00);

	checksum = 0x100 - sum;
	hdmi_write_reg(hdmi_av_base(ip_data),
					HDMI_CORE_AV_AUDIO_CHSUM, checksum);

	/*
	 * TODO: Add MPEG and SPD enable and repeat cfg when EDID parsing
	 * is available.
	 */
}
EXPORT_SYMBOL(hdmi_ti_4xxx_core_audio_infoframe_config);

void hdmi_ti_4xxx_audio_transfer_en(struct hdmi_ip_data *ip_data,
						bool enable)
{
	REG_FLD_MOD(hdmi_wp_base(ip_data),
			HDMI_WP_AUDIO_CTRL, enable, 30, 30);
	REG_FLD_MOD(hdmi_av_base(ip_data),
			HDMI_CORE_AV_AUD_MODE, enable, 0, 0);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_audio_transfer_en);


void hdmi_ti_4xxx_wp_audio_enable(struct hdmi_ip_data *ip_data, bool enable)
{
	REG_FLD_MOD(hdmi_wp_base(ip_data),
			HDMI_WP_AUDIO_CTRL, enable, 31, 31);
}
EXPORT_SYMBOL(hdmi_ti_4xxx_wp_audio_enable);

int hdmi_ti_4xx_check_aksv_data(struct hdmi_ip_data *ip_data)
{
	u32 aksv_data[5];
	int i, j, ret;
	int one = 0, zero = 0;
	/* check if HDCP AKSV registers are populated.
	 * If not load the keys and reset the wrapper.
	 */
	for (i = 0; i < 5; i++) {
		aksv_data[i] = hdmi_read_reg(hdmi_core_sys_base(ip_data),
					     HDMI_CORE_AKSV(i));
		/* Count number of zero / one */
		for (j = 0; j < 8; j++)
			(aksv_data[i] & (0x01 << j)) ? one++ : zero++;
		pr_debug("%x ", aksv_data[i] & 0xFF);
	}

	if (one != zero)
		pr_warn("HDCP: invalid AKSV\n");

	ret = (one == zero) ? HDMI_AKSV_VALID :
		(one == 0) ? HDMI_AKSV_ZERO : HDMI_AKSV_ERROR;

	return ret;

}
EXPORT_SYMBOL(hdmi_ti_4xx_check_aksv_data);

int hdmi_ti_4xx_cec_read_rx_cmd(struct hdmi_ip_data *ip_data,
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
		r = hdmi_ti_4xx_cec_get_rx_cmd(ip_data, &rx_data->rx_cmd);
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
EXPORT_SYMBOL(hdmi_ti_4xx_cec_read_rx_cmd);

int hdmi_ti_4xx_cec_get_rx_cmd(struct hdmi_ip_data *ip_data,
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
EXPORT_SYMBOL(hdmi_ti_4xx_cec_get_rx_cmd);
int hdmi_ti_4xx_cec_transmit_cmd(struct hdmi_ip_data *ip_data,
	struct cec_tx_data *data, int *cmd_acked)
{
	int r = EINVAL;
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
EXPORT_SYMBOL(hdmi_ti_4xx_cec_transmit_cmd);

int hdmi_ti_4xxx_power_on_cec(struct hdmi_ip_data *ip_data)
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
EXPORT_SYMBOL(hdmi_ti_4xxx_power_on_cec);

int hdmi_ti_4xxx_cec_get_rx_int(struct hdmi_ip_data *ip_data)
{
	u32 cec_rx = 0;

	cec_rx = REG_GET(hdmi_core_cec_base(ip_data),
		HDMI_CEC_INT_STATUS_0, 1, 0);
	return cec_rx;
}
EXPORT_SYMBOL(hdmi_ti_4xxx_cec_get_rx_int);

int hdmi_ti_4xxx_cec_clr_rx_int(struct hdmi_ip_data *ip_data, int cec_rx)
{
	/*clear CEC RX interrupts*/
	REG_FLD_MOD(hdmi_core_cec_base(ip_data), HDMI_CEC_INT_STATUS_0, cec_rx,
		1, 0);
	return 0;
}
EXPORT_SYMBOL(hdmi_ti_4xxx_cec_clr_rx_int);

int hdmi_ti_4xxx_cec_get_listening_mask(struct hdmi_ip_data *ip_data)
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
EXPORT_SYMBOL(hdmi_ti_4xxx_cec_get_listening_mask);

int hdmi_ti_4xxx_cec_add_listening_device(struct hdmi_ip_data *ip_data,
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
EXPORT_SYMBOL(hdmi_ti_4xxx_cec_add_listening_device);

int hdmi_ti_4xxx_cec_set_listening_mask(struct hdmi_ip_data *ip_data, int mask)
{
	WR_REG_32(hdmi_core_cec_base(ip_data),
		HDMI_CEC_CA_15_8, (mask >> 8) & 0xff);
	WR_REG_32(hdmi_core_cec_base(ip_data),
		HDMI_CEC_CA_7_0, mask & 0xff);
	return 0;
}
EXPORT_SYMBOL(hdmi_ti_4xxx_cec_set_listening_mask);

static int __init hdmi_ti_4xxx_init(void)
{
#if defined(CONFIG_OMAP_REMOTE_PROC_IPU) && defined(CONFIG_RPMSG)
	audio_must_use_tclk.oc = CHIP_IS_OMAP4430ES2 |
			CHIP_IS_OMAP4430ES2_1 | CHIP_IS_OMAP4430ES2_2;
	hdmi_acrwa_registered = false;
#endif
	return 0;
}

static void __exit hdmi_ti_4xxx_exit(void)
{

}

module_init(hdmi_ti_4xxx_init);
module_exit(hdmi_ti_4xxx_exit);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("hdmi_ti_4xxx_ip module");
MODULE_LICENSE("GPL");
