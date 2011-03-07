/*
 * hdmi_lib.c
 *
 * HDMI library support functions for TI OMAP processors.
 *
 * Copyright (C) 2010 Texas Instruments
 * Author: Yong Zhi <y-zhi@ti.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* Rev history:
 * Yong Zhi <y-zhi@ti.com>	changed SiVal macros
 *				added PLL/PHY code
 *				added EDID code
 *				moved PLL/PHY code to hdmi panel driver
 *				cleanup 2/08/10
 * MythriPk <mythripk@ti.com>	Apr 2010 Modified to read extended EDID
 *				partition and handle checksum with and without
 *				extension
 *				May 2010 Added support for Hot Plug Detect.
 * Munish <munish@ti.com>	Sep 2010 Added VS Infoframe for S3D support
 *
 */

#define DSS_SUBSYS_NAME "HDMI"

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <plat/hdmi_lib.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/hrtimer.h>

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
#include <syslink/ipc.h>
#include <syslink/notify.h>
#include <syslink/notify_driver.h>
#include <syslink/notifydefs.h>
#include <syslink/notify_driverdefs.h>
#include <linux/sched.h>

#define SYS_M3 2
#define HDMI_AUDIO_WA_EVENT 5
#define HDMI_AUDIO_WA_EVENT_ACK 5
#endif

/* HDMI PHY */
#define HDMI_TXPHY_TX_CTRL			0x0ul
#define HDMI_TXPHY_DIGITAL_CTRL			0x4ul
#define HDMI_TXPHY_POWER_CTRL			0x8ul

/* HDMI Wrapper */
#define HDMI_WP_REVISION			0x0ul
#define HDMI_WP_SYSCONFIG			0x10ul
#define HDMI_WP_IRQSTATUS_RAW			0x24ul
#define HDMI_WP_IRQSTATUS			0x28ul
#define HDMI_WP_IRQWAKEEN			0x34ul
#define HDMI_WP_PWR_CTRL			0x40ul
#define HDMI_WP_DEBOUNCE			0x44ul
#define HDMI_WP_IRQENABLE_SET			0x2Cul
#define HDMI_WP_VIDEO_CFG			0x50ul
#define HDMI_WP_VIDEO_SIZE			0x60ul
#define HDMI_WP_VIDEO_TIMING_H			0x68ul
#define HDMI_WP_VIDEO_TIMING_V			0x6Cul
#define HDMI_WP_WP_CLK				0x70ul
#define HDMI_WP_WP_DEBUG_CFG			0x90ul
#define HDMI_WP_WP_DEBUG_DATA			0x94ul


/* HDMI IP Core System */
#define HDMI_CORE_SYS__VND_IDL			0x0ul
#define HDMI_CORE_SYS__DEV_IDL			0x8ul
#define HDMI_CORE_SYS__DEV_IDH			0xCul
#define HDMI_CORE_SYS__DEV_REV			0x10ul
#define HDMI_CORE_SYS__SRST			0x14ul
#define HDMI_CORE_CTRL1				0x20ul
#define HDMI_CORE_SYS__SYS_STAT			0x24ul
#define HDMI_CORE_SYS__VID_ACEN			0x124ul
#define HDMI_CORE_SYS__VID_MODE			0x128ul
#define HDMI_CORE_SYS__VID_CTRL			0x120ul
#define HDMI_CORE_SYS__INTR_STATE		0x1C0ul
#define HDMI_CORE_SYS__INTR1			0x1C4ul
#define HDMI_CORE_SYS__INTR2			0x1C8ul
#define HDMI_CORE_SYS__INTR3			0x1CCul
#define HDMI_CORE_SYS__INTR4			0x1D0ul
#define HDMI_CORE_SYS__UMASK1			0x1D4ul
#define HDMI_CORE_SYS__TMDS_CTRL		0x208ul
#define HDMI_CORE_CTRL1_VEN__FOLLOWVSYNC	0x1ul
#define HDMI_CORE_CTRL1_HEN__FOLLOWHSYNC	0x1ul
#define HDMI_CORE_CTRL1_BSEL__24BITBUS		0x1ul
#define HDMI_CORE_CTRL1_EDGE__RISINGEDGE	0x1ul

#define HDMI_CORE_SYS__DE_DLY			0xC8ul
#define HDMI_CORE_SYS__DE_CTRL			0xCCul
#define HDMI_CORE_SYS__DE_TOP			0xD0ul
#define HDMI_CORE_SYS__DE_CNTL			0xD8ul
#define HDMI_CORE_SYS__DE_CNTH			0xDCul
#define HDMI_CORE_SYS__DE_LINL			0xE0ul
#define HDMI_CORE_SYS__DE_LINH__1		0xE4ul

/* HDMI IP Core Audio Video */
#define HDMI_CORE_AV_HDMI_CTRL			0xBCul
#define HDMI_CORE_AV_DPD			0xF4ul
#define HDMI_CORE_AV_PB_CTRL1			0xF8ul
#define HDMI_CORE_AV_PB_CTRL2			0xFCul
#define HDMI_CORE_AV_AVI_TYPE			0x100ul
#define HDMI_CORE_AV_AVI_VERS			0x104ul
#define HDMI_CORE_AV_AVI_LEN			0x108ul
#define HDMI_CORE_AV_AVI_CHSUM			0x10Cul
#define HDMI_CORE_AV_AVI_DBYTE			0x110ul
#define HDMI_CORE_AV_AVI_DBYTE__ELSIZE		0x4ul

/* HDMI DDC E-DID */
#define HDMI_CORE_DDC_CMD			0x3CCul
#define HDMI_CORE_DDC_STATUS			0x3C8ul
#define HDMI_CORE_DDC_ADDR			0x3B4ul
#define HDMI_CORE_DDC_OFFSET			0x3BCul
#define HDMI_CORE_DDC_COUNT1			0x3C0ul
#define HDMI_CORE_DDC_COUNT2			0x3C4ul
#define HDMI_CORE_DDC_DATA			0x3D0ul
#define HDMI_CORE_DDC_SEGM			0x3B8ul

#define HDMI_WP_AUDIO_CFG			0x80ul
#define HDMI_WP_AUDIO_CFG2			0x84ul
#define HDMI_WP_AUDIO_CTRL			0x88ul
#define HDMI_WP_AUDIO_DATA			0x8Cul

#define HDMI_CORE_AV__AVI_DBYTE			0x110ul
#define HDMI_CORE_AV__AVI_DBYTE__ELSIZE		0x4ul
#define HDMI_IP_CORE_AV__AVI_DBYTE__NELEMS	15
#define HDMI_CORE_AV__SPD_DBYTE			0x190ul
#define HDMI_CORE_AV__SPD_DBYTE__ELSIZE		0x4ul
#define HDMI_CORE_AV__SPD_DBYTE__NELEMS		27
#define HDMI_CORE_AV__AUDIO_DBYTE		0x210ul
#define HDMI_CORE_AV__AUDIO_DBYTE__ELSIZE	0x4ul
#define HDMI_CORE_AV__AUDIO_DBYTE__NELEMS	10
#define HDMI_CORE_AV__MPEG_DBYTE		0x290ul
#define HDMI_CORE_AV__MPEG_DBYTE__ELSIZE	0x4ul
#define HDMI_CORE_AV__MPEG_DBYTE__NELEMS	27
#define HDMI_CORE_AV__GEN_DBYTE			0x300ul
#define HDMI_CORE_AV__GEN_DBYTE__ELSIZE		0x4ul
#define HDMI_CORE_AV__GEN_DBYTE__NELEMS		31
#define HDMI_CORE_AV__GEN2_DBYTE		0x380ul
#define HDMI_CORE_AV__GEN2_DBYTE__ELSIZE	0x4ul
#define HDMI_CORE_AV__GEN2_DBYTE__NELEMS	31
#define HDMI_CORE_AV__ACR_CTRL			0x4ul
#define HDMI_CORE_AV__FREQ_SVAL			0x8ul
#define HDMI_CORE_AV__N_SVAL1			0xCul
#define HDMI_CORE_AV__N_SVAL2			0x10ul
#define HDMI_CORE_AV__N_SVAL3			0x14ul
#define HDMI_CORE_AV__CTS_SVAL1			0x18ul
#define HDMI_CORE_AV__CTS_SVAL2			0x1Cul
#define HDMI_CORE_AV__CTS_SVAL3			0x20ul
#define HDMI_CORE_AV__CTS_HVAL1			0x24ul
#define HDMI_CORE_AV__CTS_HVAL2			0x28ul
#define HDMI_CORE_AV__CTS_HVAL3			0x2Cul
#define HDMI_CORE_AV__AUD_MODE			0x50ul
#define HDMI_CORE_AV__SPDIF_CTRL		0x54ul
#define HDMI_CORE_AV__HW_SPDIF_FS		0x60ul
#define HDMI_CORE_AV__SWAP_I2S			0x64ul
#define HDMI_CORE_AV__SPDIF_ERTH		0x6Cul
#define HDMI_CORE_AV__I2S_IN_MAP		0x70ul
#define HDMI_CORE_AV__I2S_IN_CTRL		0x74ul
#define HDMI_CORE_AV__I2S_CHST0			0x78ul
#define HDMI_CORE_AV__I2S_CHST1			0x7Cul
#define HDMI_CORE_AV__I2S_CHST2			0x80ul
#define HDMI_CORE_AV__I2S_CHST4			0x84ul
#define HDMI_CORE_AV__I2S_CHST5			0x88ul
#define HDMI_CORE_AV__ASRC			0x8Cul
#define HDMI_CORE_AV__I2S_IN_LEN		0x90ul
#define HDMI_CORE_AV__HDMI_CTRL			0xBCul
#define HDMI_CORE_AV__AUDO_TXSTAT		0xC0ul
#define HDMI_CORE_AV__AUD_PAR_BUSCLK_1		0xCCul
#define HDMI_CORE_AV__AUD_PAR_BUSCLK_2		0xD0ul
#define HDMI_CORE_AV__AUD_PAR_BUSCLK_3		0xD4ul
#define HDMI_CORE_AV__TEST_TXCTRL		0xF0ul
#define HDMI_CORE_AV__DPD			0xF4ul
#define HDMI_CORE_AV__PB_CTRL1			0xF8ul
#define HDMI_CORE_AV__PB_CTRL2			0xFCul
#define HDMI_CORE_AV__AVI_TYPE			0x100ul
#define HDMI_CORE_AV__AVI_VERS			0x104ul
#define HDMI_CORE_AV__AVI_LEN			0x108ul
#define HDMI_CORE_AV__AVI_CHSUM			0x10Cul
#define HDMI_CORE_AV__SPD_TYPE			0x180ul
#define HDMI_CORE_AV__SPD_VERS			0x184ul
#define HDMI_CORE_AV__SPD_LEN			0x188ul
#define HDMI_CORE_AV__SPD_CHSUM			0x18Cul
#define HDMI_CORE_AV__AUDIO_TYPE		0x200ul
#define HDMI_CORE_AV__AUDIO_VERS		0x204ul
#define HDMI_CORE_AV__AUDIO_LEN			0x208ul
#define HDMI_CORE_AV__AUDIO_CHSUM		0x20Cul
#define HDMI_CORE_AV__MPEG_TYPE			0x280ul
#define HDMI_CORE_AV__MPEG_VERS			0x284ul
#define HDMI_CORE_AV__MPEG_LEN			0x288ul
#define HDMI_CORE_AV__MPEG_CHSUM		0x28Cul
#define HDMI_CORE_AV__CP_BYTE1			0x37Cul
#define HDMI_CORE_AV__CEC_ADDR_ID		0x3FCul

#define HDMI_WP_IRQSTATUS_CORE 0x00000001
#define HDMI_WP_IRQSTATUS_PHYCONNECT 0x02000000
#define HDMI_WP_IRQSTATUS_PHYDISCONNECT 0x04000000
#define HDMI_WP_IRQSTATUS_OCPTIMEOUT 0x00000010

#define HDMI_CORE_SYS__SYS_STAT_HPD 0x02

static struct {
	void __iomem *base_core;	/* 0 */
	void __iomem *base_core_av;	/* 1 */
	void __iomem *base_wp;		/* 2 */
	struct hdmi_core_infoframe_avi avi_param;
	struct list_head notifier_head;
	struct hdmi_audio_format audio_fmt;
	struct hdmi_audio_dma audio_dma;
	struct hdmi_core_audio_config audio_core_cfg;
#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	u32 notify_event_reg;
	u32 cts_interval;
	struct omap_chip_id audio_wa_chip_ids;
	struct task_struct *wa_task;
	u32 ack_payload;
#endif
	u32 pixel_clock;
} hdmi;

static DEFINE_MUTEX(hdmi_mutex);

static inline void hdmi_write_reg(u32 base, u16 idx, u32 val)
{
	void __iomem *b;

	switch (base) {
	case HDMI_CORE_SYS:
		b = hdmi.base_core;
		break;
	case HDMI_CORE_AV:
		b = hdmi.base_core_av;
		break;
	case HDMI_WP:
		b = hdmi.base_wp;
		break;
	default:
		BUG();
	}
	__raw_writel(val, b + idx);
	/* DBG("write = 0x%x idx =0x%x\r\n", val, idx); */
}

static inline u32 hdmi_read_reg(u32 base, u16 idx)
{
	void __iomem *b;
	u32 l;

	switch (base) {
	case HDMI_CORE_SYS:
		b = hdmi.base_core;
		break;
	case HDMI_CORE_AV:
		b = hdmi.base_core_av;
		break;
	case HDMI_WP:
		b = hdmi.base_wp;
		break;
	default:
		BUG();
	}
	l = __raw_readl(b + idx);

	/* DBG("addr = 0x%p rd = 0x%x idx = 0x%x\r\n", (b+idx), l, idx); */
	return l;
}

void hdmi_dump_regs(struct seq_file *s)
{
#define DUMPREG(g, r) seq_printf(s, "%-35s %08x\n", #r, hdmi_read_reg(g, r))

	/* wrapper registers */
	DUMPREG(HDMI_WP, HDMI_WP_REVISION);
	DUMPREG(HDMI_WP, HDMI_WP_SYSCONFIG);
	DUMPREG(HDMI_WP, HDMI_WP_IRQSTATUS_RAW);
	DUMPREG(HDMI_WP, HDMI_WP_IRQSTATUS);
	DUMPREG(HDMI_WP, HDMI_WP_PWR_CTRL);
	DUMPREG(HDMI_WP, HDMI_WP_IRQENABLE_SET);
	DUMPREG(HDMI_WP, HDMI_WP_VIDEO_SIZE);
	DUMPREG(HDMI_WP, HDMI_WP_VIDEO_TIMING_H);
	DUMPREG(HDMI_WP, HDMI_WP_VIDEO_TIMING_V);
	DUMPREG(HDMI_WP, HDMI_WP_WP_CLK);

	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__VND_IDL);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DEV_IDL);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DEV_IDH);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DEV_REV);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__SRST);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__SYS_STAT);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_ACEN);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_MODE);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR_STATE);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR1);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR2);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR3);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR4);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__UMASK1);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__TMDS_CTRL);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_CTRL1_VEN__FOLLOWVSYNC);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_CTRL1_HEN__FOLLOWHSYNC);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_CTRL1_BSEL__24BITBUS);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_CTRL1_EDGE__RISINGEDGE);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DE_CTRL);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DE_TOP);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DE_CNTH);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DE_LINL);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DE_LINH__1);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_SYS__DE_DLY);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_CMD);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_STATUS);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_ADDR);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_OFFSET);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_COUNT1);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_COUNT2);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_DATA);
	DUMPREG(HDMI_CORE_SYS, HDMI_CORE_DDC_SEGM);

	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_DBYTE);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__ACR_CTRL);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_TYPE);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_VERS);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_LEN);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_CHSUM);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__HDMI_CTRL);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_DBYTE);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_DBYTE);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_DBYTE);
	DUMPREG(HDMI_CORE_AV, HDMI_CORE_AV__AVI_DBYTE);
}

#define FLD_MASK(start, end)	(((1 << ((start) - (end) + 1)) - 1) << (end))
#define FLD_VAL(val, start, end) (((val) << (end)) & FLD_MASK(start, end))
#define FLD_GET(val, start, end) (((val) & FLD_MASK(start, end)) >> (end))
#define FLD_MOD(orig, val, start, end) \
	(((orig) & ~FLD_MASK(start, end)) | FLD_VAL(val, start, end))

#define REG_FLD_MOD(base, idx, val, start, end) \
	hdmi_write_reg(base, idx, \
		FLD_MOD(hdmi_read_reg(base, idx), val, start, end))

#define RD_REG_32(COMP, REG)            hdmi_read_reg(COMP, REG)
#define WR_REG_32(COMP, REG, VAL)       hdmi_write_reg(COMP, REG, (u32)(VAL))

int hdmi_get_pixel_append_position(void)
{
	printk(KERN_WARNING "This is yet to be implemented");
	return 0;
}
EXPORT_SYMBOL(hdmi_get_pixel_append_position);

int hdmi_core_ddc_edid(u8 *pEDID, int ext)
{
	u32 i, j, l;
	char checksum = 0;
	u32 sts = HDMI_CORE_DDC_STATUS;
	u32 ins = HDMI_CORE_SYS;
	u32 offset = 0;

	/* Turn on CLK for DDC */
	REG_FLD_MOD(HDMI_CORE_AV, HDMI_CORE_AV_DPD, 0x7, 2, 0);

	/* Wait */
	mdelay(10);

	if (!ext) {
		/* Clk SCL Devices */
		REG_FLD_MOD(ins, HDMI_CORE_DDC_CMD, 0xA, 3, 0);

		/* HDMI_CORE_DDC_STATUS__IN_PROG */
		while (FLD_GET(hdmi_read_reg(ins, sts), 4, 4) == 1)
			;

		/* Clear FIFO */
		REG_FLD_MOD(ins, HDMI_CORE_DDC_CMD, 0x9, 3, 0);

		/* HDMI_CORE_DDC_STATUS__IN_PROG */
		while (FLD_GET(hdmi_read_reg(ins, sts), 4, 4) == 1)
			;
	} else {
		if (ext%2 != 0)
			offset = 0x80;
	}

	/* Load Segment Address Register */
	REG_FLD_MOD(ins, HDMI_CORE_DDC_SEGM, ext/2, 7, 0);

	/* Load Slave Address Register */
	REG_FLD_MOD(ins, HDMI_CORE_DDC_ADDR, 0xA0 >> 1, 7, 1);

	/* Load Offset Address Register */
	REG_FLD_MOD(ins, HDMI_CORE_DDC_OFFSET, offset, 7, 0);
	/* Load Byte Count */
	REG_FLD_MOD(ins, HDMI_CORE_DDC_COUNT1, 0x80, 7, 0);
	REG_FLD_MOD(ins, HDMI_CORE_DDC_COUNT2, 0x0, 1, 0);
	/* Set DDC_CMD */

	if (ext)
		REG_FLD_MOD(ins, HDMI_CORE_DDC_CMD, 0x4, 3, 0);
	else
	REG_FLD_MOD(ins, HDMI_CORE_DDC_CMD, 0x2, 3, 0);

	/*
	 * Yong: do not optimize this part of the code, seems
	 * DDC bus needs some time to get stabilized
	 */
	l = hdmi_read_reg(ins, sts);

	/* HDMI_CORE_DDC_STATUS__BUS_LOW */
	if (FLD_GET(l, 6, 6) == 1) {
		printk("I2C Bus Low?\n\r");
		return -1;
	}
	/* HDMI_CORE_DDC_STATUS__NO_ACK */
	if (FLD_GET(l, 5, 5) == 1) {
		printk("I2C No Ack\n\r");
		return -1;
	}

	i = ext * 128;
	j = 0;
	while (((FLD_GET(hdmi_read_reg(ins, sts), 4, 4) == 1) ||
		(FLD_GET(hdmi_read_reg(ins, sts), 2, 2) == 0)) && j < 128) {
		if (FLD_GET(hdmi_read_reg(ins, sts), 2, 2) == 0) {
			/* FIFO not empty */
			pEDID[i++] = FLD_GET(
				hdmi_read_reg(ins, HDMI_CORE_DDC_DATA), 7, 0);
			j++;
		}
	}

	for (j = 0; j < 128; j++)
		checksum += pEDID[j];

	if (checksum != 0) {
		printk("E-EDID checksum failed!!");
		return -1;
	}
	return 0;
}

int read_edid(u8 *pEDID, u16 max_length)
{
	int r = 0, n = 0, i = 0;
	int max_ext_blocks = (max_length / 128) - 1;

	r = hdmi_core_ddc_edid(pEDID, 0);
	if (r) {
		return -1;
	} else {
		n = pEDID[0x7e];

		/*
		 * README: need to comply with max_length set by the caller.
		 * Better implementation should be to allocate necessary
		 * memory to store EDID according to nb_block field found
		 * in first block
		 */

		if (n > max_ext_blocks)
			n = max_ext_blocks;

		for (i = 1; i <= n; i++) {
			r = hdmi_core_ddc_edid(pEDID, i);
			if (r)
				return -1;
		}
	}
	return 0;
}

static void hdmi_core_init(enum hdmi_deep_mode deep_color,
	struct hdmi_core_video_config_t *v_cfg,
	struct hdmi_core_audio_config *audio_cfg,
	struct hdmi_core_infoframe_avi *avi,
	struct hdmi_core_packet_enable_repeat *r_p)
{
	DBG("Enter HDMI_Core_GlobalInitVars()\n");

	/* video core */
	switch (deep_color) {
	case HDMI_DEEP_COLOR_30BIT:
		v_cfg->CoreInputBusWide = HDMI_INPUT_10BIT;
		v_cfg->CoreOutputDitherTruncation = HDMI_OUTPUTTRUNCATION_10BIT;
		v_cfg->CoreDeepColorPacketED = HDMI_DEEPCOLORPACKECTENABLE;
		v_cfg->CorePacketMode = HDMI_PACKETMODE30BITPERPIXEL;
		break;
	case HDMI_DEEP_COLOR_36BIT:
		v_cfg->CoreInputBusWide = HDMI_INPUT_12BIT;
		v_cfg->CoreOutputDitherTruncation = HDMI_OUTPUTTRUNCATION_12BIT;
		v_cfg->CoreDeepColorPacketED = HDMI_DEEPCOLORPACKECTENABLE;
		v_cfg->CorePacketMode = HDMI_PACKETMODE36BITPERPIXEL;
		break;
	case HDMI_DEEP_COLOR_24BIT:
	default:
		v_cfg->CoreInputBusWide = HDMI_INPUT_8BIT;
		v_cfg->CoreOutputDitherTruncation = HDMI_OUTPUTTRUNCATION_8BIT;
		v_cfg->CoreDeepColorPacketED = HDMI_DEEPCOLORPACKECTDISABLE;
		v_cfg->CorePacketMode = HDMI_PACKETMODERESERVEDVALUE;
		break;
	}

	v_cfg->CoreHdmiDvi = HDMI_DVI;
	v_cfg->CoreTclkSelClkMult = FPLL10IDCK;
	/* audio core */
	audio_cfg->fs = FS_48000;
	audio_cfg->n = 6144;
	audio_cfg->layout = LAYOUT_2CH;
	audio_cfg->if_fs = IF_FS_NO;
	audio_cfg->if_channel_number = 2;
	audio_cfg->if_sample_size = IF_NO_PER_SAMPLE;
	audio_cfg->if_audio_channel_location = HDMI_CEA_CODE_00;
	audio_cfg->i2schst_max_word_length = I2S_CHST_WORD_MAX_20;
	audio_cfg->i2schst_word_length = I2S_CHST_WORD_16_BITS;
	audio_cfg->i2s_in_bit_length = I2S_IN_LENGTH_16;
	audio_cfg->i2s_justify = HDMI_AUDIO_JUSTIFY_LEFT;
	audio_cfg->aud_par_busclk = 0;
	audio_cfg->cts_mode = CTS_MODE_SW;

	if (omap_rev() == OMAP4430_REV_ES1_0) {
		audio_cfg->aud_par_busclk = (((128 * 31) - 1) << 8);
		audio_cfg->cts_mode = CTS_MODE_HW;
	}

	/* info frame */
	avi->db1y_rgb_yuv422_yuv444 = 0;
	avi->db1a_active_format_off_on = 0;
	avi->db1b_no_vert_hori_verthori = 0;
	avi->db1s_0_1_2 = 0;
	avi->db2c_no_itu601_itu709_extented = 0;
	avi->db2m_no_43_169 = 0;
	avi->db2r_same_43_169_149 = 0;
	avi->db3itc_no_yes = 0;
	avi->db3ec_xvyuv601_xvyuv709 = 0;
	avi->db3q_default_lr_fr = 0;
	avi->db3sc_no_hori_vert_horivert = 0;
	avi->db4vic_videocode = 0;
	avi->db5pr_no_2_3_4_5_6_7_8_9_10 = 0;
	avi->db6_7_lineendoftop = 0 ;
	avi->db8_9_linestartofbottom = 0;
	avi->db10_11_pixelendofleft = 0;
	avi->db12_13_pixelstartofright = 0;

	/* packet enable and repeat */
	r_p->AudioPacketED = 0;
	r_p->AudioPacketRepeat = 0;
	r_p->AVIInfoFrameED = 0;
	r_p->AVIInfoFrameRepeat = 0;
	r_p->GeneralcontrolPacketED = 0;
	r_p->GeneralcontrolPacketRepeat = 0;
	r_p->GenericPacketED = 0;
	r_p->GenericPacketRepeat = 0;
	r_p->MPEGInfoFrameED = 0;
	r_p->MPEGInfoFrameRepeat = 0;
	r_p->SPDInfoFrameED = 0;
	r_p->SPDInfoFrameRepeat = 0;
}

static void hdmi_core_powerdown_disable(void)
{
	DBG("Enter DSS_HDMI_CORE_POWER_DOWN_DISABLE()\n");
	REG_FLD_MOD(HDMI_CORE_SYS, HDMI_CORE_CTRL1, 0x0, 0, 0);
}

/* todo: power off the core */
static __attribute__ ((unused)) void hdmi_core_powerdown_enable(void)
{
	REG_FLD_MOD(HDMI_CORE_SYS, HDMI_CORE_CTRL1, 0x1, 0, 0);
}

static void hdmi_core_swreset_release(void)
{
	DBG("Enter DSS_HDMI_CORE_SW_RESET_RELEASE()\n");
	REG_FLD_MOD(HDMI_CORE_SYS, HDMI_CORE_SYS__SRST, 0x0, 0, 0);
}

static void hdmi_core_swreset_assert(void)
{
	DBG("Enter DSS_HDMI_CORE_SW_RESET_ASSERT ()\n");
	REG_FLD_MOD(HDMI_CORE_SYS, HDMI_CORE_SYS__SRST, 0x1, 0, 0);
}

/* DSS_HDMI_CORE_VIDEO_CONFIG */
static int hdmi_core_video_config(struct hdmi_core_video_config_t *cfg)
{
	u32 name = HDMI_CORE_SYS;
	u32 av_name = HDMI_CORE_AV;
	u32 r = 0;

	/* sys_ctrl1 default configuration not tunable */
	u32 ven;
	u32 hen;
	u32 bsel;
	u32 edge;

	/* sys_ctrl1 default configuration not tunable */
	ven = HDMI_CORE_CTRL1_VEN__FOLLOWVSYNC;
	hen = HDMI_CORE_CTRL1_HEN__FOLLOWHSYNC;
	bsel = HDMI_CORE_CTRL1_BSEL__24BITBUS;
	edge = HDMI_CORE_CTRL1_EDGE__RISINGEDGE;

	/* sys_ctrl1 default configuration not tunable */
	r = hdmi_read_reg(name, HDMI_CORE_CTRL1);
	r = FLD_MOD(r, ven, 5, 5);
	r = FLD_MOD(r, hen, 4, 4);
	r = FLD_MOD(r, bsel, 2, 2);
	r = FLD_MOD(r, edge, 1, 1);
	/* PD bit has to be written to recieve the interrupts */
	r = FLD_MOD(r, 1, 0, 0);
	hdmi_write_reg(name, HDMI_CORE_CTRL1, r);

	REG_FLD_MOD(name, HDMI_CORE_SYS__VID_ACEN, cfg->CoreInputBusWide, 7, 6);

	/* Vid_Mode */
	r = hdmi_read_reg(name, HDMI_CORE_SYS__VID_MODE);
	/* dither truncation configuration */
	if (cfg->CoreOutputDitherTruncation > HDMI_OUTPUTTRUNCATION_12BIT) {
		r = FLD_MOD(r, cfg->CoreOutputDitherTruncation - 3, 7, 6);
		r = FLD_MOD(r, 1, 5, 5);
	} else {
		r = FLD_MOD(r, cfg->CoreOutputDitherTruncation, 7, 6);
		r = FLD_MOD(r, 0, 5, 5);
	}
	hdmi_write_reg(name, HDMI_CORE_SYS__VID_MODE, r);

	/* HDMI_CTRL */
	r = hdmi_read_reg(av_name, HDMI_CORE_AV_HDMI_CTRL);
	r = FLD_MOD(r, cfg->CoreDeepColorPacketED, 6, 6);
	r = FLD_MOD(r, cfg->CorePacketMode, 5, 3);
	r = FLD_MOD(r, cfg->CoreHdmiDvi, 0, 0);
	hdmi_write_reg(av_name, HDMI_CORE_AV_HDMI_CTRL, r);

	/* TMDS_CTRL */
	REG_FLD_MOD(name, HDMI_CORE_SYS__TMDS_CTRL,
		cfg->CoreTclkSelClkMult, 6, 5);

	return 0;
}

static int hdmi_core_audio_mode_enable(u32  instanceName)
{
	REG_FLD_MOD(instanceName, HDMI_CORE_AV__AUD_MODE, 1, 0, 0);
	return 0;
}

static void hdmi_core_audio_config(u32 name,
		struct hdmi_core_audio_config *audio_cfg)
{
	u32 SD3_EN = 0, SD2_EN = 0, SD1_EN = 0, SD0_EN = 0, r;
	u8 DBYTE1, DBYTE2, DBYTE4, CHSUM;
	u8 size1;
	u16 size0;
	u8 acr_en;

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	if (omap_chip_is(hdmi.audio_wa_chip_ids))
		acr_en = 0;
	else
		acr_en = 1;
#else
	acr_en = 1;
#endif

	/* CTS_MODE */
	WR_REG_32(name, HDMI_CORE_AV__ACR_CTRL,
		/* MCLK_EN (0: Mclk is not used) */
		(0x0 << 2) |
		/* Set ACR packets while audio is not present */
		(acr_en << 1) |
		/* CTS Source Select (1:SW, 0:HW) */
		(audio_cfg->cts_mode << 0));

	REG_FLD_MOD(name, HDMI_CORE_AV__FREQ_SVAL, 0, 2, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__N_SVAL1, audio_cfg->n, 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__N_SVAL2, audio_cfg->n >> 8, 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__N_SVAL3, audio_cfg->n >> 16, 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__CTS_SVAL1, audio_cfg->cts, 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__CTS_SVAL2, audio_cfg->cts >> 8, 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__CTS_SVAL3, audio_cfg->cts >> 16, 7, 0);

	/* number of channel */
	REG_FLD_MOD(name, HDMI_CORE_AV__HDMI_CTRL, audio_cfg->layout, 2, 1);
	REG_FLD_MOD(name, HDMI_CORE_AV__AUD_PAR_BUSCLK_1,
				audio_cfg->aud_par_busclk, 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__AUD_PAR_BUSCLK_2,
				(audio_cfg->aud_par_busclk >> 8), 7, 0);
	REG_FLD_MOD(name, HDMI_CORE_AV__AUD_PAR_BUSCLK_3,
				(audio_cfg->aud_par_busclk >> 16), 7, 0);
	/* FS_OVERRIDE = 1 because // input is used */
	REG_FLD_MOD(name, HDMI_CORE_AV__SPDIF_CTRL, 1, 1, 1);
	/* refer to table171 p122 in func core spec*/
	REG_FLD_MOD(name, HDMI_CORE_AV__I2S_CHST4, audio_cfg->fs, 3, 0);

	/*
	 * audio config is mainly due to wrapper hardware connection
	 * and so are fixed (hardware) I2S deserializer is by-passed
	 * so I2S configuration is not needed (I2S don't care).
	 * Wrapper is directly connected at the I2S deserialiser
	 * output level so some registers call I2S and need to be
	 * programmed to configure this parallel bus, there configuration
	 * is also fixed and due to the hardware connection (I2S hardware)
	 */
	WR_REG_32(name, HDMI_CORE_AV__I2S_IN_CTRL,
		(0 << 7) |	/* HBRA_ON */
		(1 << 6) |	/* SCK_EDGE Sample clock is rising */
		(0 << 5) |	/* CBIT_ORDER */
		(0 << 4) |	/* VBit, 0x0=PCM, 0x1=compressed */
		(0 << 3) |	/* I2S_WS, 0xdon't care */
		(audio_cfg->i2s_justify << 2) | /* I2S_JUST*/
		(0 << 1) |	/* I2S_DIR, 0xdon't care */
		(0));		/* I2S_SHIFT, 0x0 don't care */

	r = hdmi_read_reg(name, HDMI_CORE_AV__I2S_CHST5);
	r = FLD_MOD(r, audio_cfg->fs, 7, 4); /* FS_ORIG */
	r = FLD_MOD(r, audio_cfg->i2schst_word_length, 3, 1);
	r = FLD_MOD(r, audio_cfg->i2schst_max_word_length, 0, 0);
	WR_REG_32(name, HDMI_CORE_AV__I2S_CHST5, r);

	REG_FLD_MOD(name, HDMI_CORE_AV__I2S_IN_LEN,
			audio_cfg->i2s_in_bit_length, 3, 0);

	/* channel enable depend of the layout */
	if (audio_cfg->layout == LAYOUT_2CH) {
		SD3_EN = 0x0;
		SD2_EN = 0x0;
		SD1_EN = 0x0;
		SD0_EN = 0x1;
	} else if (audio_cfg->layout == LAYOUT_8CH) {
		SD3_EN = 0x1;
		SD2_EN = 0x1;
		SD1_EN = 0x1;
		SD0_EN = 0x1;
	}

	r = hdmi_read_reg(name, HDMI_CORE_AV__AUD_MODE);
	r = FLD_MOD(r, SD3_EN, 7, 7); /* SD3_EN */
	r = FLD_MOD(r, SD2_EN, 6, 6); /* SD2_EN */
	r = FLD_MOD(r, SD1_EN, 5, 5); /* SD1_EN */
	r = FLD_MOD(r, SD0_EN, 4, 4); /* SD0_EN */
	r = FLD_MOD(r, 0, 3, 3); /* DSD_EN */
	r = FLD_MOD(r, 1, 2, 2); /* AUD_PAR_EN */
	r = FLD_MOD(r, 0, 1, 1); /* SPDIF_EN */
	WR_REG_32(name, HDMI_CORE_AV__AUD_MODE, r);

	/* Audio info frame setting refer to CEA-861-d spec p75 */
	/* 0x0 because on HDMI CT must be = 0 / -1 because 1 is for 2 channel */
	DBYTE1 = 0x0 + (audio_cfg->if_channel_number - 1);
	DBYTE2 = (audio_cfg->if_fs << 2) + audio_cfg->if_sample_size;
	/* channel location according to CEA spec */
	DBYTE4 = audio_cfg->if_audio_channel_location;

	CHSUM = 0x100-0x84-0x01-0x0A-DBYTE1-DBYTE2-DBYTE4;

	WR_REG_32(name, HDMI_CORE_AV__AUDIO_TYPE, 0x084);
	WR_REG_32(name, HDMI_CORE_AV__AUDIO_VERS, 0x001);
	WR_REG_32(name, HDMI_CORE_AV__AUDIO_LEN, 0x00A);
	/* don't care on VMP */
	WR_REG_32(name, HDMI_CORE_AV__AUDIO_CHSUM, CHSUM);

	size0 = HDMI_CORE_AV__AUDIO_DBYTE;
	size1 = HDMI_CORE_AV__AUDIO_DBYTE__ELSIZE;
	hdmi_write_reg(name, (size0 + 0 * size1), DBYTE1);
	hdmi_write_reg(name, (size0 + 1 * size1), DBYTE2);
	hdmi_write_reg(name, (size0 + 2 * size1), 0x000);
	hdmi_write_reg(name, (size0 + 3 * size1), DBYTE4);
	hdmi_write_reg(name, (size0 + 4 * size1), 0x000);
	hdmi_write_reg(name, (size0 + 5 * size1), 0x000);
	hdmi_write_reg(name, (size0 + 6 * size1), 0x000);
	hdmi_write_reg(name, (size0 + 7 * size1), 0x000);
	hdmi_write_reg(name, (size0 + 8 * size1), 0x000);
	hdmi_write_reg(name, (size0 + 9 * size1), 0x000);

	/* ISCR1 and ACP setting */
	WR_REG_32(name, HDMI_CORE_AV__SPD_TYPE, 0x04);
	WR_REG_32(name, HDMI_CORE_AV__SPD_VERS, 0x0);
	WR_REG_32(name, HDMI_CORE_AV__SPD_LEN, 0x0);
	WR_REG_32(name, HDMI_CORE_AV__SPD_CHSUM, 0x0);

	WR_REG_32(name, HDMI_CORE_AV__MPEG_TYPE, 0x05);
	WR_REG_32(name, HDMI_CORE_AV__MPEG_VERS, 0x0);
	WR_REG_32(name, HDMI_CORE_AV__MPEG_LEN, 0x0);
	WR_REG_32(name, HDMI_CORE_AV__MPEG_CHSUM, 0x0);
}

int hdmi_core_read_avi_infoframe(struct hdmi_core_infoframe_avi *info_avi)
{
	info_avi->db1y_rgb_yuv422_yuv444 =
		hdmi.avi_param.db1y_rgb_yuv422_yuv444;
	info_avi->db1a_active_format_off_on =
		hdmi.avi_param.db1a_active_format_off_on;
	info_avi->db1b_no_vert_hori_verthori =
		hdmi.avi_param.db1b_no_vert_hori_verthori;
	info_avi->db1s_0_1_2 = hdmi.avi_param.db1s_0_1_2;
	info_avi->db2c_no_itu601_itu709_extented =
		hdmi.avi_param.db2c_no_itu601_itu709_extented;
	info_avi->db2m_no_43_169 = hdmi.avi_param.db2m_no_43_169;
	info_avi->db2r_same_43_169_149 = hdmi.avi_param.db2r_same_43_169_149;
	info_avi->db3itc_no_yes = hdmi.avi_param.db3itc_no_yes;
	info_avi->db3ec_xvyuv601_xvyuv709 =
		hdmi.avi_param.db3ec_xvyuv601_xvyuv709;
	info_avi->db3q_default_lr_fr = hdmi.avi_param.db3q_default_lr_fr;
	info_avi->db3sc_no_hori_vert_horivert =
		hdmi.avi_param.db3sc_no_hori_vert_horivert;
	info_avi->db4vic_videocode = hdmi.avi_param.db4vic_videocode;
	info_avi->db5pr_no_2_3_4_5_6_7_8_9_10 =
		hdmi.avi_param.db5pr_no_2_3_4_5_6_7_8_9_10;
	info_avi->db6_7_lineendoftop = hdmi.avi_param.db6_7_lineendoftop;
	info_avi->db8_9_linestartofbottom =
		hdmi.avi_param.db8_9_linestartofbottom;
	info_avi->db10_11_pixelendofleft =
		hdmi.avi_param.db10_11_pixelendofleft;
	info_avi->db12_13_pixelstartofright =
		hdmi.avi_param.db12_13_pixelstartofright;

	return 0;
}

static int hdmi_core_audio_infoframe_avi(
				struct hdmi_core_infoframe_avi info_avi)
{
	u16 offset;
	int dbyte, dbyte_size;
	u32 val;
	char sum = 0, checksum = 0;

	dbyte = HDMI_CORE_AV_AVI_DBYTE;
	dbyte_size = HDMI_CORE_AV_AVI_DBYTE__ELSIZE;
	/* info frame video */
	sum += 0x82 + 0x002 + 0x00D;
	hdmi_write_reg(HDMI_CORE_AV, HDMI_CORE_AV_AVI_TYPE, 0x082);
	hdmi_write_reg(HDMI_CORE_AV, HDMI_CORE_AV_AVI_VERS, 0x002);
	hdmi_write_reg(HDMI_CORE_AV, HDMI_CORE_AV_AVI_LEN, 0x00D);

	offset = dbyte + (0 * dbyte_size);
	val = (info_avi.db1y_rgb_yuv422_yuv444 << 5) |
		(info_avi.db1a_active_format_off_on << 4) |
		(info_avi.db1b_no_vert_hori_verthori << 2) |
		(info_avi.db1s_0_1_2);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (1 * dbyte_size);
	val = (info_avi.db2c_no_itu601_itu709_extented << 6) |
		(info_avi.db2m_no_43_169 << 4) |
		(info_avi.db2r_same_43_169_149);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (2 * dbyte_size);
	val = (info_avi.db3itc_no_yes << 7) |
		(info_avi.db3ec_xvyuv601_xvyuv709 << 4) |
		(info_avi.db3q_default_lr_fr << 2) |
		(info_avi.db3sc_no_hori_vert_horivert);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (3 * dbyte_size);
	hdmi_write_reg(HDMI_CORE_AV, offset, info_avi.db4vic_videocode);
	sum += info_avi.db4vic_videocode;

	offset = dbyte + (4 * dbyte_size);
	val = info_avi.db5pr_no_2_3_4_5_6_7_8_9_10;
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (5 * dbyte_size);
	val = info_avi.db6_7_lineendoftop & 0x00FF;
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (6 * dbyte_size);
	val = ((info_avi.db6_7_lineendoftop >> 8) & 0x00FF);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (7 * dbyte_size);
	val = info_avi.db8_9_linestartofbottom & 0x00FF;
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (8 * dbyte_size);
	val = ((info_avi.db8_9_linestartofbottom >> 8) & 0x00FF);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (9 * dbyte_size);
	val = info_avi.db10_11_pixelendofleft & 0x00FF;
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (10 * dbyte_size);
	val = ((info_avi.db10_11_pixelendofleft >> 8) & 0x00FF);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	offset = dbyte + (11 * dbyte_size);
	val = info_avi.db12_13_pixelstartofright & 0x00FF;
	hdmi_write_reg(HDMI_CORE_AV, offset , val);
	sum += val;

	offset = dbyte + (12 * dbyte_size);
	val = ((info_avi.db12_13_pixelstartofright >> 8) & 0x00FF);
	hdmi_write_reg(HDMI_CORE_AV, offset, val);
	sum += val;

	checksum = 0x100 - sum;
	hdmi_write_reg(HDMI_CORE_AV, HDMI_CORE_AV_AVI_CHSUM, checksum);

	return 0;
}

int hdmi_configure_csc(enum hdmi_core_av_csc csc)
{
	int var;
	switch (csc) {
	/*
	 * Setting the AVI infroframe to respective color mode
	 * As the quantization is in default mode ie it selects
	 * full range for RGB (except for VGA ) and limited range
	 * for YUV we dont have to make any changes for this
	 */
	case RGB:
		hdmi.avi_param.db1y_rgb_yuv422_yuv444 = INFOFRAME_AVI_DB1Y_RGB;
		hdmi_core_audio_infoframe_avi(hdmi.avi_param);
		var = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_ACEN);
		var = FLD_MOD(var, 0, 2, 2);
		hdmi_write_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_ACEN, var);
		break;
	case RGB_TO_YUV:
		hdmi.avi_param.db1y_rgb_yuv422_yuv444 =
						INFOFRAME_AVI_DB1Y_YUV422;
		hdmi_core_audio_infoframe_avi(hdmi.avi_param);
		var = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_ACEN);
		var = FLD_MOD(var, 1, 2, 2);
		hdmi_write_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_ACEN, var);
		break;
	case YUV_TO_RGB:
		hdmi.avi_param.db1y_rgb_yuv422_yuv444 = INFOFRAME_AVI_DB1Y_RGB;
		hdmi_core_audio_infoframe_avi(hdmi.avi_param);
		var = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_MODE);
		var = FLD_MOD(var, 1, 3, 3);
		hdmi_write_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__VID_MODE, var);
		break;
	default:
		break;
	}
	return 0;
}

int hdmi_configure_lrfr(enum hdmi_range lr_fr, int force_set)
{
	int var;
	switch (lr_fr) {
	/*
	 * Setting the AVI infroframe to respective limited range
	 * 0 if for limited range 1 for full range
	 */
	case HDMI_LIMITED_RANGE:
		hdmi.avi_param.db3q_default_lr_fr = INFOFRAME_AVI_DB3Q_LR;
		hdmi_core_audio_infoframe_avi(hdmi.avi_param);
		if (force_set) {
			var = hdmi_read_reg(HDMI_CORE_SYS,
						HDMI_CORE_SYS__VID_ACEN);
			var = FLD_MOD(var, 1, 1, 1);
			hdmi_write_reg(HDMI_CORE_SYS,
						HDMI_CORE_SYS__VID_ACEN, var);
		}
		break;
	case HDMI_FULL_RANGE:
		if (hdmi.avi_param.db1y_rgb_yuv422_yuv444 ==
						INFOFRAME_AVI_DB1Y_YUV422) {
			printk(KERN_ERR"It is only limited range for YUV");
			return -1;
		}
		hdmi.avi_param.db3q_default_lr_fr = INFOFRAME_AVI_DB3Q_FR;
		hdmi_core_audio_infoframe_avi(hdmi.avi_param);
		if (force_set) {
			var = hdmi_read_reg(HDMI_CORE_SYS,
						HDMI_CORE_SYS__VID_MODE);
			var = FLD_MOD(var, 1, 4, 4);
			hdmi_write_reg(HDMI_CORE_SYS,
						HDMI_CORE_SYS__VID_MODE, var);
		}
		break;
	default:
		break;
	}
	return 0;
}

static int hdmi_core_vsi_infoframe(u32 name,
	struct hdmi_s3d_config info_s3d)
{
	u16 offset;
	u8 sum = 0, i;
	/*For side-by-side(HALF) we need to specify subsampling in 3D_ext_data*/
	int length = info_s3d.structure > 0x07 ? 6 : 5;
	u8 info_frame_packet[] = {
		0x81, /*Vendor-Specific InfoFrame*/
		0x01, /*InfoFrame version number per CEA-861-D*/
		length, /*InfoFrame length, excluding checksum and header*/
		0x00, /*Checksum*/
		0x03, 0x0C, 0x00, /*24-bit IEEE Registration Ident*/
		0x40, /*3D format indication preset, 3D_Struct follows*/
		info_s3d.structure << 4, /*3D_Struct, no 3D_Meta*/
		info_s3d.s3d_ext_data << 4,/*3D_Ext_Data*/
		};

	/*Adding packet header and checksum length*/
	length += 4;

	/*Checksum is packet_header+checksum+infoframe_length = 0*/
	for (i = 0; i < length; i++)
		sum += info_frame_packet[i];
	info_frame_packet[3] = 0x100-sum;

	offset = HDMI_CORE_AV__GEN_DBYTE;
	for (i = 0; i < length; i++) {
		hdmi_write_reg(name, offset, info_frame_packet[i]);
		offset += HDMI_CORE_AV__GEN_DBYTE__ELSIZE;
	}

	return 0;
}

static int hdmi_core_av_packet_config(u32 name,
	struct hdmi_core_packet_enable_repeat r_p)
{

	/* enable/repeat the infoframe */
	hdmi_write_reg(name, HDMI_CORE_AV_PB_CTRL1,
		(r_p.MPEGInfoFrameED << 7) |
		(r_p.MPEGInfoFrameRepeat << 6) |
		(r_p.AudioPacketED << 5) |
		(r_p.AudioPacketRepeat << 4) |
		(r_p.SPDInfoFrameED << 3) |
		(r_p.SPDInfoFrameRepeat << 2) |
		(r_p.AVIInfoFrameED << 1) |
		(r_p.AVIInfoFrameRepeat));

	/* enable/repeat the packet */
	hdmi_write_reg(name, HDMI_CORE_AV_PB_CTRL2,
		(r_p.GeneralcontrolPacketED << 3) |
		(r_p.GeneralcontrolPacketRepeat << 2) |
		(r_p.GenericPacketED << 1) |
		(r_p.GenericPacketRepeat));
	return 0;
}

static void hdmi_w1_init(struct hdmi_video_timing *t_p,
	struct hdmi_video_format *f_p,
	struct hdmi_video_interface *i_p,
	struct hdmi_irq_vector *pIrqVectorEnable,
	struct hdmi_audio_format *audio_fmt,
	struct hdmi_audio_dma *audio_dma)
{
	DBG("Enter HDMI_W1_GlobalInitVars()\n");

	t_p->horizontalBackPorch = 0;
	t_p->horizontalFrontPorch = 0;
	t_p->horizontalSyncPulse = 0;
	t_p->verticalBackPorch = 0;
	t_p->verticalFrontPorch = 0;
	t_p->verticalSyncPulse = 0;

	f_p->packingMode = HDMI_PACK_10b_RGB_YUV444;
	f_p->linePerPanel = 0;
	f_p->pixelPerLine = 0;

	i_p->vSyncPolarity = 0;
	i_p->hSyncPolarity = 0;

	i_p->interlacing = 0;
	i_p->timingMode = 0; /* HDMI_TIMING_SLAVE */

	pIrqVectorEnable->pllRecal = 0;
	pIrqVectorEnable->pllUnlock = 0;
	pIrqVectorEnable->pllLock = 0;
	pIrqVectorEnable->phyDisconnect = 1;
	pIrqVectorEnable->phyConnect = 1;
	pIrqVectorEnable->phyShort5v = 0;
	pIrqVectorEnable->videoEndFrame = 0;
	pIrqVectorEnable->videoVsync = 0;
	pIrqVectorEnable->fifoSampleRequest = 0;
	pIrqVectorEnable->fifoOverflow = 0;
	pIrqVectorEnable->fifoUnderflow = 0;
	pIrqVectorEnable->ocpTimeOut = 1;
	pIrqVectorEnable->core = 1;

	audio_fmt->stereo_channel_enable = HDMI_STEREO_ONECHANNELS;
	audio_fmt->audio_channel_location = HDMI_CEA_CODE_03;
	audio_fmt->iec = HDMI_AUDIO_FORMAT_LPCM;
	audio_fmt->justify = HDMI_AUDIO_JUSTIFY_LEFT;
	audio_fmt->left_before = HDMI_SAMPLE_LEFT_FIRST;
	audio_fmt->sample_number = HDMI_ONEWORD_TWO_SAMPLES;
	audio_fmt->sample_size = HDMI_SAMPLE_16BITS;
	audio_fmt->block_start_end = HDMI_BLOCK_STARTEND_ON;

	audio_dma->dma_transfer = 0x20;
	audio_dma->block_size = 0xC0;
	audio_dma->dma_or_irq = HDMI_THRESHOLD_DMA;
	audio_dma->threshold_value = 0x20;
}


static void hdmi_w1_irq_enable(struct hdmi_irq_vector *pIrqVectorEnable)
{
	u32 r = 0;

	r = ((pIrqVectorEnable->pllRecal << 31) |
		(pIrqVectorEnable->pllUnlock << 30) |
		(pIrqVectorEnable->pllLock << 29) |
		(pIrqVectorEnable->phyDisconnect << 26) |
		(pIrqVectorEnable->phyConnect << 25) |
		(pIrqVectorEnable->phyShort5v << 24) |
		(pIrqVectorEnable->videoEndFrame << 17) |
		(pIrqVectorEnable->videoVsync << 16) |
		(pIrqVectorEnable->fifoSampleRequest << 10) |
		(pIrqVectorEnable->fifoOverflow << 9) |
		(pIrqVectorEnable->fifoUnderflow << 8) |
		(pIrqVectorEnable->ocpTimeOut << 4) |
		(pIrqVectorEnable->core << 0));

	hdmi_write_reg(HDMI_WP, HDMI_WP_IRQENABLE_SET, r);
}

static void hdmi_w1_irq_wakeup_enable(struct hdmi_irq_vector *pIrqVectorEnable)
{
	u32 r = 0;
	/* make the irq wakeup enable */
	r = ((pIrqVectorEnable->phyDisconnect << 26) |
		(pIrqVectorEnable->phyConnect << 25) |
		(pIrqVectorEnable->core << 0));
	hdmi_write_reg(HDMI_WP, HDMI_WP_IRQWAKEEN, r);
}

static inline int hdmi_w1_wait_for_bit_change(const u32 ins,
	u32 idx, int b2, int b1, int val)
{
	int t = 0;
	while (val != FLD_GET(hdmi_read_reg(ins, idx), b2, b1)) {
		udelay(1);
		if (t++ > 1000)
			return !val;
	}
	return val;
}

/* todo: add timeout value */
int hdmi_w1_set_wait_srest(void)
{
	/* reset W1 */
	REG_FLD_MOD(HDMI_WP, HDMI_WP_SYSCONFIG, 0x1, 0, 0);

	/* wait till SOFTRESET == 0 */
	while (FLD_GET(hdmi_read_reg(HDMI_WP, HDMI_WP_SYSCONFIG), 0, 0))
		;
	return 0;
}

/* PHY_PWR_CMD */
int hdmi_w1_set_wait_phy_pwr(HDMI_PhyPwr_t val)
{
	DBG("*** Set PHY power mode to %d\n", val);
	REG_FLD_MOD(HDMI_WP, HDMI_WP_PWR_CTRL, val, 7, 6);

	if (hdmi_w1_wait_for_bit_change(HDMI_WP, HDMI_WP_PWR_CTRL, 5, 4, val)
	    != val) {
		ERR("Failed to set PHY power mode to %d\n", val);
		return -ENODEV;
	}

	/* Set module to smart idle to allow DSS to go into OFF mode*/
	if (val == HDMI_PHYPWRCMD_OFF)
		REG_FLD_MOD(HDMI_WP, HDMI_WP_SYSCONFIG, 0x2, 3, 2);

	return 0;
}

/* PLL_PWR_CMD */
int hdmi_w1_set_wait_pll_pwr(HDMI_PllPwr_t val)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_PWR_CTRL, val, 3, 2);

	/* wait till PHY_PWR_STATUS=ON */
	if (hdmi_w1_wait_for_bit_change(HDMI_WP, HDMI_WP_PWR_CTRL, 1, 0, val)
	    != val) {
		ERR("Failed to set PHY_PWR_STATUS to ON\n");
		return -ENODEV;
	}

	return 0;
}

void hdmi_w1_video_stop(void)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_VIDEO_CFG, 0, 31, 31);
}

void hdmi_w1_video_start(void)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_VIDEO_CFG, (u32)0x1, 31, 31);
}

int hdmi_w1_get_video_state(void)
{
	uint32_t status = hdmi_read_reg(HDMI_WP, HDMI_WP_VIDEO_CFG);

	return (status & 0x80000000) ? 1 : 0;
}

static void hdmi_w1_video_init_format(struct hdmi_video_format *f_p,
	struct hdmi_video_timing *t_p, struct hdmi_config *param)
{
	DBG("Enter HDMI_W1_ConfigVideoResolutionTiming()\n");

	f_p->linePerPanel = param->lpp;
	f_p->pixelPerLine = param->ppl;

	t_p->horizontalBackPorch = param->hbp;
	t_p->horizontalFrontPorch = param->hfp;
	t_p->horizontalSyncPulse = param->hsw;
	t_p->verticalBackPorch = param->vbp;
	t_p->verticalFrontPorch = param->vfp;
	t_p->verticalSyncPulse = param->vsw;
}

static void hdmi_w1_video_config_format(
	struct hdmi_video_format *f_p)
{
	u32 l = 0;

	REG_FLD_MOD(HDMI_WP, HDMI_WP_VIDEO_CFG, f_p->packingMode, 10, 8);

	l |= FLD_VAL(f_p->linePerPanel, 31, 16);
	l |= FLD_VAL(f_p->pixelPerLine, 15, 0);
	hdmi_write_reg(HDMI_WP, HDMI_WP_VIDEO_SIZE, l);
}

static void hdmi_w1_video_config_interface(
	struct hdmi_video_interface *i_p)
{
	u32 r;
	DBG("Enter HDMI_W1_ConfigVideoInterface()\n");

	r = hdmi_read_reg(HDMI_WP, HDMI_WP_VIDEO_CFG);
	r = FLD_MOD(r, i_p->vSyncPolarity, 7, 7);
	r = FLD_MOD(r, i_p->hSyncPolarity, 6, 6);
	r = FLD_MOD(r, i_p->interlacing, 3, 3);
	r = FLD_MOD(r, i_p->timingMode, 1, 0);
	hdmi_write_reg(HDMI_WP, HDMI_WP_VIDEO_CFG, r);
}

static void hdmi_w1_video_config_timing(
	struct hdmi_video_timing *t_p)
{
	u32 timing_h = 0;
	u32 timing_v = 0;

	DBG("Enter HDMI_W1_ConfigVideoTiming ()\n");

	timing_h |= FLD_VAL(t_p->horizontalBackPorch, 31, 20);
	timing_h |= FLD_VAL(t_p->horizontalFrontPorch, 19, 8);
	timing_h |= FLD_VAL(t_p->horizontalSyncPulse, 7, 0);
	hdmi_write_reg(HDMI_WP, HDMI_WP_VIDEO_TIMING_H, timing_h);

	timing_v |= FLD_VAL(t_p->verticalBackPorch, 31, 20);
	timing_v |= FLD_VAL(t_p->verticalFrontPorch, 19, 8);
	timing_v |= FLD_VAL(t_p->verticalSyncPulse, 7, 0);
	hdmi_write_reg(HDMI_WP, HDMI_WP_VIDEO_TIMING_V, timing_v);
}

static void hdmi_w1_audio_config_format(u32 name,
			struct hdmi_audio_format *audio_fmt)
{
	u32 value = 0;

	value = hdmi_read_reg(name, HDMI_WP_AUDIO_CFG);
	value &= 0xfffffff7;
	value |= ((audio_fmt->justify) << 3);;
	value &= 0xfffffffb;
	value |= ((audio_fmt->left_before) << 2);
	value &= 0xfffffffd;
	value |= ((audio_fmt->sample_number) << 1);
	value &= 0xfffffffe;
	value |= ((audio_fmt->sample_size));
	value &= 0xf8ffffff;
	value |= ((audio_fmt->stereo_channel_enable) << 24);
	value &= 0xff00ffff;
	value |= ((audio_fmt->audio_channel_location) << 16);
	value &= 0xffffffef;
	value |= ((audio_fmt->iec) << 4);
	value &= 0xffffffdf;
	value |= ((audio_fmt->block_start_end) << 5);
	hdmi_write_reg(name, HDMI_WP_AUDIO_CFG, value);
	DBG("HDMI_WP_AUDIO_CFG = 0x%x\n", value);

}

static void hdmi_w1_audio_config_dma(u32 name, struct hdmi_audio_dma *audio_dma)
{
	u32 value = 0;

	value = hdmi_read_reg(name, HDMI_WP_AUDIO_CFG2);
	value &= 0xffffff00;
	value |= audio_dma->block_size;
	value &= 0xffff00ff;
	value |= audio_dma->dma_transfer << 8;
	hdmi_write_reg(name, HDMI_WP_AUDIO_CFG2, value);
	DBG("HDMI_WP_AUDIO_CFG2 = 0x%x\n", value);

	value = hdmi_read_reg(name, HDMI_WP_AUDIO_CTRL);
	value &= 0xfffffdff;
	value |= audio_dma->dma_or_irq << 9;
	value &= 0xfffffe00;
	value |= audio_dma->threshold_value;
	hdmi_write_reg(name, HDMI_WP_AUDIO_CTRL, value);
	DBG("HDMI_WP_AUDIO_CTRL = 0x%x\n", value);

}

static int hdmi_configure_acr(u32 pclk)
{
	u32 r, deep_color = 0, fs, n, cts;
#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	u32 cts_interval_qtt, cts_interval_res;
#endif

	/* Deep color mode */
	if (omap_rev() == OMAP4430_REV_ES1_0)
		deep_color = 100;
	else {
		r = hdmi_read_reg(HDMI_WP, HDMI_WP_VIDEO_CFG);
		switch (r & 0x03) {
		case 1:
			deep_color = 100;
			break;
		case 2:
			deep_color = 125;
			break;
		case 3:
			deep_color = 150;
			break;
		default:
			return -EINVAL;
		}
	}
	switch (hdmi.audio_core_cfg.fs) {
	case FS_32000:
		fs = 32000;
		if ((deep_color == 125) && ((pclk == 54054)
				|| (pclk == 74250)))
			n = 8192;
		else
			n = 4096;
		break;
	case FS_44100:
		fs = 44100;
		n = 6272;
		break;
	case FS_48000:
		fs = 48000;
		if ((deep_color == 125) && ((pclk == 54054)
				|| (pclk == 74250)))
			n = 8192;
		else
			n = 6144;
		break;
	case FS_88200:
	case FS_96000:
	case FS_176400:
	case FS_192000:
	case FS_NOT_INDICATED:
	default:
		return -EINVAL;
	}
	/* Process CTS */
	cts = pclk*(n/128)*deep_color / (fs/10);

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	if (omap_chip_is(hdmi.audio_wa_chip_ids)) {
		if (pclk && deep_color) {
			cts_interval_qtt = 1000000 /
				((pclk * deep_color) / 100);
			cts_interval_res = 1000000 %
				((pclk * deep_color) / 100);
			hdmi.cts_interval = cts_interval_res*n/
				((pclk * deep_color) / 100);
			hdmi.cts_interval += cts_interval_qtt*n;
		} else
			hdmi.cts_interval = 0;
	}
#endif

	hdmi.audio_core_cfg.n = n;
	hdmi.audio_core_cfg.cts = cts;

	return 0;
}

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
int hdmi_lib_acr_wa_send_event(u32 payload)
{
	long tout;
	if (omap_chip_is(hdmi.audio_wa_chip_ids)) {
		if (hdmi.notify_event_reg == HDMI_NOTIFY_EVENT_REG) {
			notify_send_event(SYS_M3, 0, HDMI_AUDIO_WA_EVENT,
					payload, 0);
			if (signal_pending(current))
				return -ERESTARTSYS;
			hdmi.wa_task = current;
			set_current_state(TASK_INTERRUPTIBLE);
			tout = schedule_timeout(msecs_to_jiffies(5000));
			if (!tout)
				return -EIO;

			/*
			 * Enable this code when the following patch
			 * from Ducati MM is released:
			 * hdmiwa: Reseting properly the hdmi status variable
			 * It ensures that hdmi_status variable is reset for
			 * each time a notification is received.
			 */

#if 0
			if (payload != hdmi.ack_payload)
				return -EBADE;
#endif
			return 0;
		}
		return -ENODEV;
	}
	return 0;
}
int hdmi_lib_start_acr_wa(void)
{
	return hdmi_lib_acr_wa_send_event(hdmi.cts_interval);
}
int hdmi_lib_stop_acr_wa(void)
{
	return hdmi_lib_acr_wa_send_event(0);
}

void hdmi_notify_event_ack_func(u16 proc_id, u16 line_id, u32 event_id,
							u32 *arg, u32 payload)
{
	hdmi.ack_payload = payload;
	if (WARN_ON(!hdmi.wa_task))
		return;

	wake_up_process(hdmi.wa_task);
}

static int hdmi_syslink_notifier_call(struct notifier_block *nb,
						unsigned long val, void *v)
{
	int status = 0;
	u16 *proc_id = (u16 *)v;

	switch ((int)val) {
	case IPC_START:
		if (*proc_id == multiproc_get_id("SysM3")) {
			status = notify_register_event(SYS_M3, 0,
				HDMI_AUDIO_WA_EVENT_ACK, (notify_fn_notify_cbck)
				hdmi_notify_event_ack_func,	(void *)NULL);
			if (status == NOTIFY_S_SUCCESS)
				hdmi.notify_event_reg = HDMI_NOTIFY_EVENT_REG;
		}
		return status;
	case IPC_STOP:
		if (*proc_id == multiproc_get_id("SysM3")) {
			status = notify_unregister_event(SYS_M3, 0,
				HDMI_AUDIO_WA_EVENT_ACK, (notify_fn_notify_cbck)
				hdmi_notify_event_ack_func,	(void *)NULL);
			if (status == NOTIFY_S_SUCCESS)
				hdmi.notify_event_reg =
					HDMI_NOTIFY_EVENT_NOTREG;
		}
		return status;
	case IPC_CLOSE:
	default:
		return status;
	}
}

static struct notifier_block hdmi_syslink_notify_block = {
	.notifier_call = hdmi_syslink_notifier_call,
};
#endif

static void hdmi_w1_audio_enable(void)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_AUDIO_CTRL, 1, 31, 31);
}

static void hdmi_w1_audio_disable(void)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_AUDIO_CTRL, 0, 31, 31);
}

static void hdmi_w1_audio_start(void)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_AUDIO_CTRL, 1, 30, 30);
}

static void hdmi_w1_audio_stop(void)
{
	REG_FLD_MOD(HDMI_WP, HDMI_WP_AUDIO_CTRL, 0, 30, 30);
}

int hdmi_lib_enable(struct hdmi_config *cfg)
{
	u32 r;
	u32 av_name = HDMI_CORE_AV;

	/* HDMI */
	struct hdmi_video_timing VideoTimingParam;
	struct hdmi_video_format VideoFormatParam;
	struct hdmi_video_interface VideoInterfaceParam;
	struct hdmi_irq_vector IrqHdmiVectorEnable;
	struct hdmi_s3d_config s3d_param;

	/* HDMI core */
	struct hdmi_core_video_config_t v_core_cfg;
	struct hdmi_core_packet_enable_repeat repeat_param;

	hdmi_w1_init(&VideoTimingParam, &VideoFormatParam,
		&VideoInterfaceParam, &IrqHdmiVectorEnable,
		&hdmi.audio_fmt, &hdmi.audio_dma);

	hdmi_core_init(cfg->deep_color, &v_core_cfg,
		&hdmi.audio_core_cfg,
		&hdmi.avi_param,
		&repeat_param);

	/*
	   Currently we are seeing OCP timeout interrupt.
	   It happens if l3 is idle. we need to set the SCP
	   clock to proper value to make sure good performance
	   is met.
	*/
	hdmi_write_reg(HDMI_WP, HDMI_WP_WP_CLK, 0x100);

	/* Enable PLL Lock and UnLock intrerrupts */
	IrqHdmiVectorEnable.pllUnlock = 1;
	IrqHdmiVectorEnable.pllLock = 1;
	IrqHdmiVectorEnable.core = 1;

	/***************** init DSS register **********************/
	hdmi_w1_irq_enable(&IrqHdmiVectorEnable);

	/* Enable wakeup for connect & core at start but on disconnect */
	IrqHdmiVectorEnable.phyDisconnect = 0;
	hdmi_w1_irq_wakeup_enable(&IrqHdmiVectorEnable);

	hdmi_w1_video_init_format(&VideoFormatParam,
			&VideoTimingParam, cfg);

	hdmi_w1_video_config_timing(&VideoTimingParam);

	/* video config */
	switch (cfg->deep_color) {
	case 0:
		VideoFormatParam.packingMode = HDMI_PACK_24b_RGB_YUV444_YUV422;
		VideoInterfaceParam.timingMode = HDMI_TIMING_MASTER_24BIT;
		break;
	case 1:
		VideoFormatParam.packingMode = HDMI_PACK_10b_RGB_YUV444;
		VideoInterfaceParam.timingMode = HDMI_TIMING_MASTER_30BIT;
		break;
	case 2:
		VideoFormatParam.packingMode = HDMI_PACK_ALREADYPACKED;
		VideoInterfaceParam.timingMode = HDMI_TIMING_MASTER_36BIT;
		break;
	}

	hdmi_w1_video_config_format(&VideoFormatParam);

	/* FIXME */
	VideoInterfaceParam.vSyncPolarity = cfg->v_pol;
	VideoInterfaceParam.hSyncPolarity = cfg->h_pol;
	VideoInterfaceParam.interlacing = cfg->interlace;

	hdmi_w1_video_config_interface(&VideoInterfaceParam);

#if 0
	/* hnagalla */
	val = hdmi_read_reg(HDMI_WP, HDMI_WP_VIDEO_SIZE);

	val &= 0x0FFFFFFF;
	val |= ((0x1f) << 27); /* wakeup */
	hdmi_write_reg(HDMI_WP, HDMI_WP_VIDEO_SIZE, val);
#endif
	hdmi_w1_audio_config_format(HDMI_WP, &hdmi.audio_fmt);
	hdmi_w1_audio_config_dma(HDMI_WP, &hdmi.audio_dma);


	/****************************** CORE *******************************/
	/************* configure core video part ********************************/
	/* set software reset in the core */
	hdmi_core_swreset_assert();

	/* power down off */
	hdmi_core_powerdown_disable();

	v_core_cfg.CoreHdmiDvi = cfg->hdmi_dvi;

	hdmi.pixel_clock = cfg->pixel_clock;
	hdmi_configure_acr(hdmi.pixel_clock);

	r = hdmi_core_video_config(&v_core_cfg);

	hdmi_core_audio_config(av_name, &hdmi.audio_core_cfg);
	hdmi_core_audio_mode_enable(av_name);

	/* release software reset in the core */
	hdmi_core_swreset_release();

	/* configure packet */
	/* info frame video see doc CEA861-D page 65 */
	hdmi.avi_param.db1y_rgb_yuv422_yuv444 = INFOFRAME_AVI_DB1Y_RGB;
	hdmi.avi_param.db1a_active_format_off_on =
		INFOFRAME_AVI_DB1A_ACTIVE_FORMAT_OFF;
	hdmi.avi_param.db1b_no_vert_hori_verthori = INFOFRAME_AVI_DB1B_NO;
	hdmi.avi_param.db1s_0_1_2 = INFOFRAME_AVI_DB1S_0;
	hdmi.avi_param.db2c_no_itu601_itu709_extented = INFOFRAME_AVI_DB2C_NO;
	hdmi.avi_param.db2m_no_43_169 = INFOFRAME_AVI_DB2M_NO;
	hdmi.avi_param.db2r_same_43_169_149 = INFOFRAME_AVI_DB2R_SAME;
	hdmi.avi_param.db3itc_no_yes = INFOFRAME_AVI_DB3ITC_NO;
	hdmi.avi_param.db3ec_xvyuv601_xvyuv709 = INFOFRAME_AVI_DB3EC_XVYUV601;
	hdmi.avi_param.db3q_default_lr_fr = INFOFRAME_AVI_DB3Q_DEFAULT;
	hdmi.avi_param.db3sc_no_hori_vert_horivert = INFOFRAME_AVI_DB3SC_NO;
	hdmi.avi_param.db4vic_videocode = cfg->video_format;
	hdmi.avi_param.db5pr_no_2_3_4_5_6_7_8_9_10 = INFOFRAME_AVI_DB5PR_NO;
	hdmi.avi_param.db6_7_lineendoftop = 0;
	hdmi.avi_param.db8_9_linestartofbottom = 0;
	hdmi.avi_param.db10_11_pixelendofleft = 0;
	hdmi.avi_param.db12_13_pixelstartofright = 0;

	r = hdmi_core_audio_infoframe_avi(hdmi.avi_param);

	if (cfg->vsi_enabled) {
		s3d_param.structure = cfg->s3d_structure;
		s3d_param.s3d_ext_data = cfg->subsamp_pos;
		hdmi_core_vsi_infoframe(av_name, s3d_param);

		/*enable/repeat the vendor specific infoframe*/
		repeat_param.GenericPacketED = PACKETENABLE;
		repeat_param.GenericPacketRepeat = PACKETREPEATON;
	}

	/* enable/repeat the infoframe */
	repeat_param.AVIInfoFrameED = PACKETENABLE;
	repeat_param.AVIInfoFrameRepeat = PACKETREPEATON;
	/* wakeup */
	repeat_param.AudioPacketED = PACKETENABLE;
	repeat_param.AudioPacketRepeat = PACKETREPEATON;
	/* ISCR1 transmission */
	repeat_param.MPEGInfoFrameED = PACKETDISABLE;
	repeat_param.MPEGInfoFrameRepeat = PACKETREPEATOFF;
	/* ACP transmission */
	repeat_param.SPDInfoFrameED = cfg->supports_ai;
	repeat_param.SPDInfoFrameRepeat = cfg->supports_ai;

	r = hdmi_core_av_packet_config(av_name, repeat_param);

	REG_FLD_MOD(av_name, HDMI_CORE_AV__HDMI_CTRL, cfg->hdmi_dvi, 0, 0);

#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	if (omap_chip_is(hdmi.audio_wa_chip_ids)) {
		if (hdmi.notify_event_reg == HDMI_NOTIFY_EVENT_NOTREG) {
			r = ipc_register_notifier(&hdmi_syslink_notify_block);
			hdmi.notify_event_reg = HDMI_NOTIFY_WAIT_FOR_IPC;
		}
	}
#endif
	return r;
}

/**
 * hdmi_lib_init - Initializes hdmi library
 *
 * hdmi_lib_init is expected to be called by any user of the hdmi library (e.g.
 * HDMI video driver, HDMI audio driver). This means that it is not safe to add
 * anything in this function that requires the DSS clocks to be enabled.
 */
int hdmi_lib_init(void)
{
	static u8 initialized;

	mutex_lock(&hdmi_mutex);
	if (initialized) {
		printk(KERN_INFO "hdmi_lib already initialized\n");
		mutex_unlock(&hdmi_mutex);
		return 0;
	}
	initialized = 1;
	mutex_unlock(&hdmi_mutex);

	hdmi.base_wp = ioremap(HDMI_WP, (HDMI_HDCP - HDMI_WP));

	if (!hdmi.base_wp) {
		ERR("can't ioremap WP\n");
		return -ENOMEM;
	}

	hdmi.base_core = hdmi.base_wp + 0x400;
	hdmi.base_core_av = hdmi.base_wp + 0x900;
#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	hdmi.notify_event_reg = HDMI_NOTIFY_EVENT_NOTREG;
	hdmi.audio_wa_chip_ids.oc = CHIP_IS_OMAP4430ES2 |
			CHIP_IS_OMAP4430ES2_1 | CHIP_IS_OMAP4430ES2_2;
#endif

	INIT_LIST_HEAD(&hdmi.notifier_head);

	return 0;
}

void hdmi_lib_exit(void){
	iounmap(hdmi.base_wp);
#ifdef CONFIG_OMAP_HDMI_AUDIO_WA
	if (omap_chip_is(hdmi.audio_wa_chip_ids))
		ipc_unregister_notifier(&hdmi_syslink_notify_block);
#endif
}

int hdmi_set_irqs(int i)
{
	u32 r = 0 , hpd = 0;
	struct hdmi_irq_vector pIrqVectorEnable;

	if (!i) {
		pIrqVectorEnable.pllRecal = 0;
		pIrqVectorEnable.phyShort5v = 0;
		pIrqVectorEnable.videoEndFrame = 0;
		pIrqVectorEnable.videoVsync = 0;
		pIrqVectorEnable.fifoSampleRequest = 0;
		pIrqVectorEnable.fifoOverflow = 0;
		pIrqVectorEnable.fifoUnderflow = 0;
		pIrqVectorEnable.ocpTimeOut = 0;
		pIrqVectorEnable.pllUnlock = 1;
		pIrqVectorEnable.pllLock = 1;
		pIrqVectorEnable.phyDisconnect = 1;
		pIrqVectorEnable.phyConnect = 1;
		pIrqVectorEnable.core = 1;

		hdmi_w1_irq_enable(&pIrqVectorEnable);

		r = hdmi_read_reg(HDMI_WP, HDMI_WP_IRQENABLE_SET);
		DBG("Irqenable %x\n", r);
	}
	r = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_CTRL1);
	/* PD bit has to be written to recieve the interrupts */
	r = FLD_MOD(r, !i, 0, 0);
	hdmi_write_reg(HDMI_CORE_SYS, HDMI_CORE_CTRL1, r);

	hdmi_write_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__UMASK1, i ? 0x00 : 0x40);
	hpd = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__UMASK1);
	DBG("%x hpd\n", hpd);

	return 0;
}

/* Interrupt handler */
void HDMI_W1_HPD_handler(int *r)
{
	u32 val, set = 0, hpd_intr = 0, core_state = 0, time_in_ms;
	static bool first_hpd, dirty;
	static ktime_t ts_hpd_low, ts_hpd_high;
	//mdelay(30);
	DBG("-------------DEBUG-------------------");
	DBG("%x hdmi_wp_irqstatus\n", \
		hdmi_read_reg(HDMI_WP, HDMI_WP_IRQSTATUS));
	DBG("%x hdmi_core_intr_state\n", \
		hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR_STATE));
	DBG("%x hdmi_core_irqstate\n", \
		hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR1));
	DBG("%x hdmi_core_sys_sys_stat\n", \
		hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__SYS_STAT));
	DBG("-------------DEBUG-------------------");

	val = hdmi_read_reg(HDMI_WP, HDMI_WP_IRQSTATUS);
	if (val & HDMI_WP_IRQSTATUS_CORE) {
		core_state = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR_STATE);
		if (core_state & 0x1) {
			set = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__SYS_STAT);
			hpd_intr = hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR1);

			hdmi_write_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR1,
				hpd_intr);

			/* Read to flush */
			hdmi_read_reg(HDMI_CORE_SYS, HDMI_CORE_SYS__INTR1);
		}
	}

	if (val & HDMI_WP_IRQSTATUS_PHYCONNECT) {
		DBG("connect, ");
		*r |= HDMI_CONNECT;
	}

	if ((val & HDMI_WP_IRQSTATUS_CORE) && (core_state & 0x1)) {
		if (hpd_intr & 0x40) {
			if  (set & HDMI_CORE_SYS__SYS_STAT_HPD) {
			if ((first_hpd == 0) && (dirty == 0)) {
					*r |= HDMI_FIRST_HPD;
				first_hpd++;
				DBG("first hpd");
				} else {
					ts_hpd_high = ktime_get();
					if (dirty) {
						time_in_ms =
						(int)ktime_to_us(ktime_sub\
						(ts_hpd_high, ts_hpd_low)) / 1000;
				if (time_in_ms >= 100)
							*r |= HDMI_HPD_MODIFY;
				else
							*r |= HDMI_HPD_HIGH;
				dirty = 0;
			}
				}
		} else {
				ts_hpd_low = ktime_get();
			dirty = 1;
				*r |= HDMI_HPD_LOW;
			}
		}
	}

	/* We can get connect / disconnect simultaneously due to glitch	*/
	if (val & HDMI_WP_IRQSTATUS_PHYDISCONNECT) {
		DBG("Disconnect");
		dirty = 0;
		first_hpd = 0;
		*r |= HDMI_DISCONNECT;
	}

	/*
	   Handle ocp timeout in correct way. SCP clock will reset the divder
	   to 7 automatically. We need to speed up the clock to avoid performance
	   degradation.
	*/
	if(val & HDMI_WP_IRQSTATUS_OCPTIMEOUT) {
		hdmi_write_reg(HDMI_WP, HDMI_WP_WP_CLK, 0x100);
		DBG("OCP timeout");
	}

	/* Ack other interrupts if any */
	hdmi_write_reg(HDMI_WP, HDMI_WP_IRQSTATUS, val);
	/* flush posted write */
	hdmi_read_reg(HDMI_WP, HDMI_WP_IRQSTATUS);

}

int hdmi_rxdet(void)
{
	int state = 0;
	int loop = 0, val1, val2, val3, val4;
	struct hdmi_irq_vector IrqHdmiVectorEnable;

	hdmi_write_reg(HDMI_WP, HDMI_WP_WP_DEBUG_CFG, 4);

	do {
		val1 = hdmi_read_reg(HDMI_WP, HDMI_WP_WP_DEBUG_DATA);
		udelay(5);
		val2 = hdmi_read_reg(HDMI_WP, HDMI_WP_WP_DEBUG_DATA);
		udelay(5);
		val3 = hdmi_read_reg(HDMI_WP, HDMI_WP_WP_DEBUG_DATA);
		udelay(5);
		val4 = hdmi_read_reg(HDMI_WP, HDMI_WP_WP_DEBUG_DATA);
	} while ((val1 != val2 || val2 != val3 || val3 != val4)
		&& (loop < 100));

	hdmi_write_reg(HDMI_WP, HDMI_WP_WP_DEBUG_CFG, 0);

	if (loop == 100)
		state = -1;
	else
		state = (val1 & 1);

	/* Turn on the wakeup capability of the interrupts
	It is recommended to turn on opposite interrupt wake
	up capability in connected and disconnected state.
	This is to avoid race condition in interrupts.
	*/
	IrqHdmiVectorEnable.core = 1;
	if(state){
		//printk("Connected ...");
		IrqHdmiVectorEnable.phyDisconnect = 1;
		IrqHdmiVectorEnable.phyConnect = 0;
		hdmi_w1_irq_wakeup_enable(&IrqHdmiVectorEnable);
	}
	else {
		//printk("DisConnected ...");
		IrqHdmiVectorEnable.phyDisconnect = 0;
		IrqHdmiVectorEnable.phyConnect = 1;
		hdmi_w1_irq_wakeup_enable(&IrqHdmiVectorEnable);
	}

	return state;
}

/* wrapper functions to be used until L24.5 release */
int HDMI_CORE_DDC_READEDID(u32 name, u8 *p, u16 max_length)
{
	int r = read_edid(p, max_length);
	return r;
}

int HDMI_W1_StopVideoFrame(u32 name)
{
	DBG("Enter HDMI_W1_StopVideoFrame()\n");
	hdmi_w1_video_stop();
	return 0;
}

int HDMI_W1_StartVideoFrame(u32 name)
{
	DBG("Enter HDMI_W1_StartVideoFrame  ()\n");
	hdmi_w1_video_start();
	return 0;
}

/* PHY_PWR_CMD */
int HDMI_W1_SetWaitPhyPwrState(u32 name,
	HDMI_PhyPwr_t param)
{
	int r = hdmi_w1_set_wait_phy_pwr(param);
	return r;
}

/* PLL_PWR_CMD */
int HDMI_W1_SetWaitPllPwrState(u32 name,
		HDMI_PllPwr_t param)
{
	int r = hdmi_w1_set_wait_pll_pwr(param);
	return r;
}

int HDMI_W1_SetWaitSoftReset(void)
{
	/* reset W1 */
	REG_FLD_MOD(HDMI_WP, HDMI_WP_SYSCONFIG, 0x1, 0, 0);

	/* wait till SOFTRESET == 0 */
	while (FLD_GET(hdmi_read_reg(HDMI_WP, HDMI_WP_SYSCONFIG), 0, 0));

	/* Make madule smart and wakeup capable*/
	REG_FLD_MOD(HDMI_WP, HDMI_WP_SYSCONFIG, 0x3, 3, 2);

	/* Add debounce as less as possible */
	hdmi_write_reg(HDMI_WP, HDMI_WP_DEBOUNCE, 0x0101);

	return 0;
}

int hdmi_w1_wrapper_enable(u32 instanceName)
{
	printk(KERN_INFO "Wrapper Enabled...\n");
	hdmi_w1_audio_enable();
	return 0;
}

int hdmi_w1_wrapper_disable(u32 instanceName)
{
	hdmi_w1_audio_disable();
	printk(KERN_INFO "Wrapper disabled...\n");
	return 0;
}

int hdmi_w1_stop_audio_transfer(u32 instanceName)
{
	hdmi_w1_audio_stop();
	/* if audio is not used, switch to smart-idle & wakeup capable*/
	REG_FLD_MOD(HDMI_WP, HDMI_WP_SYSCONFIG, 0x3, 3, 2);
	return 0;
}

int hdmi_w1_start_audio_transfer(u32 instanceName)
{
	/*
	 * during audio use-case, switch to no-idle to avoid
	 * DSS_L3_ICLK clock to be shutdown (as per TRM)
	 */
	REG_FLD_MOD(HDMI_WP, HDMI_WP_SYSCONFIG, 0x1, 3, 2);
	hdmi_w1_audio_start();
	printk(KERN_INFO "Start audio transfer...\n");
	return 0;
}

void hdmi_add_notifier(struct hdmi_notifier *notifier)
{
	mutex_lock(&hdmi_mutex);
	list_add_tail(&notifier->list, &hdmi.notifier_head);
	mutex_unlock(&hdmi_mutex);
}

void hdmi_remove_notifier(struct hdmi_notifier *notifier)
{
	struct hdmi_notifier *cur, *next;

	list_for_each_entry_safe(cur, next, &hdmi.notifier_head, list) {
		if (cur == notifier) {
			mutex_lock(&hdmi_mutex);
			list_del(&cur->list);
			mutex_unlock(&hdmi_mutex);
		}
	}
}

void hdmi_notify_hpd(int state)
{
	struct hdmi_notifier *cur, *next;

	list_for_each_entry_safe(cur, next, &hdmi.notifier_head, list) {
		if (cur->hpd_notifier)
			cur->hpd_notifier(state, cur->private_data);
	}
}

void hdmi_notify_pwrchange(int state)
{
	struct hdmi_notifier *cur, *next;

	list_for_each_entry_safe(cur, next, &hdmi.notifier_head, list) {
		if (cur->pwrchange_notifier)
			cur->pwrchange_notifier(state, cur->private_data);
	}
}

int hdmi_configure_audio_sample_freq(u32 sample_freq)
{
	int err = 0;

	switch (sample_freq) {
	case 48000:
		hdmi.audio_core_cfg.fs = FS_48000;
		break;
	case 44100:
		hdmi.audio_core_cfg.fs = FS_44100;
		break;
	case 32000:
		hdmi.audio_core_cfg.fs = FS_32000;
		break;
	default:
		return -EINVAL;
	}
	err = hdmi_configure_acr(hdmi.pixel_clock);
	if (err)
		return err;

	hdmi_core_audio_config(HDMI_CORE_AV, &hdmi.audio_core_cfg);

	return err;
}

int hdmi_configure_audio_sample_size(u32 sample_size)
{
	u32 r;
	hdmi.audio_core_cfg.if_sample_size = IF_NO_PER_SAMPLE;
	hdmi.audio_fmt.left_before = HDMI_SAMPLE_LEFT_FIRST;

	switch (sample_size) {
	case HDMI_SAMPLE_16BITS:
		hdmi.audio_core_cfg.i2schst_max_word_length =
			I2S_CHST_WORD_MAX_20;
		hdmi.audio_core_cfg.i2schst_word_length = I2S_CHST_WORD_16_BITS;
		hdmi.audio_core_cfg.i2s_in_bit_length = I2S_IN_LENGTH_16;
		hdmi.audio_core_cfg.i2s_justify = HDMI_AUDIO_JUSTIFY_LEFT;
		hdmi.audio_fmt.sample_number = HDMI_ONEWORD_TWO_SAMPLES;
		hdmi.audio_fmt.sample_size = HDMI_SAMPLE_16BITS;
		hdmi.audio_fmt.justify = HDMI_AUDIO_JUSTIFY_LEFT;
		break;
	case HDMI_SAMPLE_24BITS:
		hdmi.audio_core_cfg.i2schst_max_word_length =
			I2S_CHST_WORD_MAX_24;
		hdmi.audio_core_cfg.i2schst_word_length = I2S_CHST_WORD_24_BITS;
		hdmi.audio_core_cfg.i2s_in_bit_length = I2S_IN_LENGTH_24;
		hdmi.audio_core_cfg.i2s_justify = HDMI_AUDIO_JUSTIFY_RIGHT;
		hdmi.audio_fmt.sample_number = HDMI_ONEWORD_ONE_SAMPLE;
		hdmi.audio_fmt.sample_size = HDMI_SAMPLE_24BITS;
		hdmi.audio_fmt.justify = HDMI_AUDIO_JUSTIFY_RIGHT;
		break;
	default:
		return -EINVAL;
	}
	hdmi_core_audio_config(HDMI_CORE_AV, &hdmi.audio_core_cfg);
	r = hdmi_read_reg(HDMI_WP, HDMI_WP_AUDIO_CTRL);
	if (r & 0x80000000)
		REG_FLD_MOD(HDMI_WP, HDMI_WP_AUDIO_CTRL, 0, 31, 31);
	hdmi_w1_audio_config_format(HDMI_WP, &hdmi.audio_fmt);
	if (r & 0x80000000)
		REG_FLD_MOD(HDMI_WP, HDMI_WP_AUDIO_CTRL, 1, 31, 31);
	return 0;
}
