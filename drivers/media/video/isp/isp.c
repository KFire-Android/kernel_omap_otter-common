/*
 * isp.c
 *
 * Driver Library for ISP Control module in TI's OMAP3 Camera ISP
 * ISP interface and IRQ related APIs are defined here.
 *
 * Copyright (C) 2009 Texas Instruments.
 * Copyright (C) 2009 Nokia.
 *
 * Contributors:
 * 	Sameer Venkatraman <sameerv@ti.com>
 * 	Mohit Jalori <mjalori@ti.com>
 * 	Sergio Aguirre <saaguirre@ti.com>
 * 	Sakari Ailus <sakari.ailus@nokia.com>
 * 	Tuukka Toivonen <tuukka.o.toivonen@nokia.com>
 *	Toni Leinonen <toni.leinonen@nokia.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <asm/cacheflush.h>

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/device.h>

#include "isp.h"
#include "ispreg.h"
#include "ispccdc.h"
#include "isph3a.h"
#include "isphist.h"
#include "isp_af.h"
#include "isppreview.h"
#include "ispresizer.h"
#include "ispcsi2.h"
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
#include "isp_dfs.h"
#endif

static struct platform_device *omap3isp_pdev;
static int isp_complete_reset = 1;

static void isp_save_ctx(struct device *dev);

static void isp_restore_ctx(struct device *dev);

static void isp_buf_init(struct device *dev);

/* List of image formats supported via OMAP ISP */
const static struct v4l2_fmtdesc isp_formats[] = {
	{
		.description = "UYVY, packed",
		.pixelformat = V4L2_PIX_FMT_UYVY,
	},
	{
		.description = "YUYV (YUV 4:2:2), packed",
		.pixelformat = V4L2_PIX_FMT_YUYV,
	},
	{
		.description = "Bayer10 (GrR/BGb)",
		.pixelformat = V4L2_PIX_FMT_SGRBG10,
	},
	{
		.description = "Bayer10 (GrR/BGb)",
		.pixelformat = V4L2_PIX_FMT_SRGGB10,
	},
	{
		.description = "Bayer10 (GrR/BGb)",
		.pixelformat = V4L2_PIX_FMT_SGBRG10,
	},
	{
		.description = "Bayer10 (GrR/BGb)",
		.pixelformat = V4L2_PIX_FMT_SBGGR10,
	},
};

/**
 * struct vcontrol - Video control structure.
 * @qc: V4L2 Query control structure.
 * @current_value: Current value of the control.
 */
static struct vcontrol {
	struct v4l2_queryctrl qc;
	int current_value;
} video_control[] = {
	{
		{
			.id = V4L2_CID_BRIGHTNESS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Brightness",
			.minimum = ISPPRV_BRIGHT_LOW,
			.maximum = ISPPRV_BRIGHT_HIGH,
			.step = ISPPRV_BRIGHT_STEP,
			.default_value = ISPPRV_BRIGHT_DEF,
		},
		.current_value = ISPPRV_BRIGHT_DEF,
	},
	{
		{
			.id = V4L2_CID_CONTRAST,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Contrast",
			.minimum = ISPPRV_CONTRAST_LOW,
			.maximum = ISPPRV_CONTRAST_HIGH,
			.step = ISPPRV_CONTRAST_STEP,
			.default_value = ISPPRV_CONTRAST_DEF,
		},
		.current_value = ISPPRV_CONTRAST_DEF,
	},
	{
		{
			.id = V4L2_CID_COLORFX,
			.type = V4L2_CTRL_TYPE_MENU,
			.name = "Color Effects",
			.minimum = V4L2_COLORFX_NONE,
			.maximum = V4L2_COLORFX_SEPIA,
			.step = 1,
			.default_value = V4L2_COLORFX_NONE,
		},
		.current_value = V4L2_COLORFX_NONE,
	},
	{
		{
			.id = V4L2_CID_PRIVATE_OMAP3ISP_CSI2MEM,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "omap3isp: Force CSI2->MEM path",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.current_value = 0,
	},
};

static struct v4l2_querymenu video_menu[] = {
	{
		.id = V4L2_CID_COLORFX,
		.index = 0,
		.name = "None",
	},
	{
		.id = V4L2_CID_COLORFX,
		.index = 1,
		.name = "B&W",
	},
	{
		.id = V4L2_CID_COLORFX,
		.index = 2,
		.name = "Sepia",
	},
};

/* Structure for saving/restoring ISP module registers */
static struct isp_reg isp_reg_list[] = {
	{OMAP3_ISP_IOMEM_MAIN, ISP_SYSCONFIG, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_GRESET_LENGTH, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_PSTRB_REPLAY, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_CTRL, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_CTRL, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_FRAME, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_PSTRB_DELAY, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_STRB_DELAY, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_SHUT_DELAY, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_PSTRB_LENGTH, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_STRB_LENGTH, 0},
	{OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_SHUT_LENGTH, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF_SYSCONFIG, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF_IRQENABLE, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF0_CTRL, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF1_CTRL, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF0_START, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF1_START, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF0_END, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF1_END, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF0_WINDOWSIZE, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF1_WINDOWSIZE, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF0_THRESHOLD, 0},
	{OMAP3_ISP_IOMEM_CBUFF, ISP_CBUFF1_THRESHOLD, 0},
	{0, ISP_TOK_TERM, 0}
};

/*
 * Defined constants for management of pipeline bandwidth constraints.
 */
#define REQ_EXP_MULTIPLIER 32
static struct isp_freq_devider isp_ratio_factor[] = {
	{  0, 32768, 0 * REQ_EXP_MULTIPLIER, 0 * REQ_EXP_MULTIPLIER },
	{  2, 16384, 1 * REQ_EXP_MULTIPLIER, 1 * REQ_EXP_MULTIPLIER },
	{  6,  8192, 2 * REQ_EXP_MULTIPLIER, 2 * REQ_EXP_MULTIPLIER },
	{ 14,  4096, 3 * REQ_EXP_MULTIPLIER, 3 * REQ_EXP_MULTIPLIER },
	{ 30,  2048, 4 * REQ_EXP_MULTIPLIER, 4 * REQ_EXP_MULTIPLIER }
};

/**
 * isp_validate_errata_i421 - Check errata i421
 **/
static int isp_validate_errata_i421(struct device *dev,
				struct isph3a_aewb_config *aewb_cfg,
				struct af_paxel *af_cfg)
{
	int i, num_cycles;
	int aewb_width, aewb_cnt, af_width, af_cnt;

	aewb_cnt = aewb_cfg->hor_win_count;
	aewb_width = aewb_cfg->win_width;

	/* Restore interface differences with AEWB interface */
	af_cnt = af_cfg->hz_cnt + 1;
	af_width = (af_cfg->width + 1) * 2;

	for (i = 0; i <= af_cnt; i++) {
		num_cycles = ((i + 1) * af_width) + 2;
		num_cycles += af_cfg->hz_start - aewb_cfg->hor_win_start;

		if ((num_cycles % aewb_width) == 0) {
			dev_err(dev, "Preventing errata i421..."
				     " Invalid AF paxel size, index %d\n", i);
			return -EINVAL;
		}

		if ((num_cycles / aewb_width) >= aewb_cnt)
			break;
	}

	return 0;
}

/**
 * isp_get_upscale_ratio - Return ratio releated to the current crop.
 * @dev: Device pointer specific to the OMAP3 ISP.
 */
struct isp_freq_devider *isp_get_upscale_ratio(int in_w, int in_h, int out_w,
					       int out_h)
{
	int in_size, out_size, ratio = 0;

	if (in_w > 0 && in_h > 0 && out_w > 0 && out_h > 0) {
		in_size = in_w * in_h;
		out_size = out_w * out_h;
		if (out_size > in_size) {
			ratio = int_sqrt(out_size / in_size);
			clamp_t(int, ratio, 0,
				ARRAY_SIZE(isp_ratio_factor) - 1);
		}
	}

	return &isp_ratio_factor[ratio];
}
EXPORT_SYMBOL(isp_get_upscale_ratio);

/**
 * isp_flush - Post pending L3 bus writes by doing a register readback
 * @dev: Device pointer specific to the OMAP3 ISP.
 *
 * In order to force posting of pending writes, we need to write and
 * readback the same register, in this case the revision register.
 *
 * See this link for reference:
 *   http://www.mail-archive.com/linux-omap@vger.kernel.org/msg08149.html
 **/
void isp_flush(struct device *dev)
{
	isp_reg_writel(dev, 0, OMAP3_ISP_IOMEM_MAIN, ISP_REVISION);
	isp_reg_readl(dev, OMAP3_ISP_IOMEM_MAIN, ISP_REVISION);
}

/*
 *
 * V4L2 Handling
 *
 */

/**
 * find_vctrl - Return the index of the ctrl array of the requested ctrl ID.
 * @id: Requested control ID.
 *
 * Returns 0 if successful, -EINVAL if not found, or -EDOM if its out of
 * domain.
 **/
static int find_vctrl(int id)
{
	int i;

	if (id < V4L2_CID_BASE)
		return -EDOM;

	for (i = (ARRAY_SIZE(video_control) - 1); i >= 0; i--)
		if (video_control[i].qc.id == id)
			break;

	if (i < 0)
		i = -EINVAL;

	return i;
}

/**
 * find_next_vctrl - Return next v4l2 ctrl ID available after the specified ID
 * @id: Reference V4L2 control ID.
 *
 * Returns 0 if successful, or -EINVAL if not found.
 **/
static int find_next_vctrl(int id)
{
	int i;
	u32 best = (u32)-1;

	for (i = 0; i < ARRAY_SIZE(video_control); i++) {
		if (video_control[i].qc.id > id &&
		    (best == (u32)-1 ||
		     video_control[i].qc.id <
		     video_control[best].qc.id)) {
			best = i;
		}
	}

	if (best == (u32)-1)
		return -EINVAL;

	return best;
}

/**
 * find_vmenu - Return index of the menu array of the requested ctrl option.
 * @id: Requested control ID.
 * @index: Requested menu option index.
 *
 * Returns 0 if successful, -EINVAL if not found, or -EDOM if its out of
 * domain.
 **/
static int find_vmenu(int id, int index)
{
	int i;

	if (id < V4L2_CID_BASE)
		return -EDOM;

	for (i = (ARRAY_SIZE(video_menu) - 1); i >= 0; i--) {
		if (video_menu[i].id != id || video_menu[i].index != index)
			continue;
		return i;
	}

	return -EINVAL;
}

/**
 * isp_release_resources - Free all currently requested ISP submodules.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_release_resources(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	if (isp->pipeline.modules & OMAP_ISP_CCDC)
		ispccdc_free(&isp->isp_ccdc);

	if (isp->pipeline.modules & OMAP_ISP_PREVIEW)
		isppreview_free(&isp->isp_prev);

	if (isp->pipeline.modules & OMAP_ISP_RESIZER)
		ispresizer_free(&isp->isp_res);
	return;
}

/**
 * isp_wait - Wait for idle or busy state transition with a time limit
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @busy: Function pointer which determines if submodule is busy.
 * @wait_for_busy: If 0, waits for idle state, if 1, waits for busy state.
 * @max_wait: Max retry count in us for wait for idle/busy transition.
 * @priv: Function parameter to send to busy check function.
 **/
static int isp_wait(struct device *dev, int (*busy)(void *), int wait_for_busy,
		    int max_wait, void *priv)
{
	int wait = 0;

	if (max_wait == 0)
		max_wait = 10000; /* 10 ms */

	while ((wait_for_busy && !busy(priv))
	       || (!wait_for_busy && busy(priv))) {
		rmb();
		udelay(1);
		wait++;
		if (wait > max_wait)
			return -EBUSY;
	}
	DPRINTK_ISPCTRL(KERN_ALERT "%s: wait %d\n", __func__, wait);

	return 0;
}

/**
 * ispccdc_sbl_wait_idle - Wrapper to wait for ccdc sbl status bits to be idle.
 * @isp_ccdc: Pointer to ISP CCDC device.
 * @max_wait: Max retry count for wait for idle transition of the CCDC SBL bits
 **/
static int ispccdc_sbl_wait_idle(struct isp_ccdc_device *isp_ccdc, int max_wait)
{
	struct device *dev = to_device(isp_ccdc);

	return isp_wait(dev, ispccdc_sbl_busy, 0, max_wait, isp_ccdc);
}

/**
 * isp_adjust_bandwidth - Adjust ISP clock to get maximum bandwidth.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_adjust_bandwidth(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	/* If we are using memory read chanel make adjustment depending of input
	 * And output size */

	if (isp->config->u.csi.use_mem_read) {
		struct isp_freq_devider *deviders;
		u32 in_width, in_height, out_width, out_height;

		in_width = isp->pipeline.in_pix.width;
		in_height = isp->pipeline.in_pix.height;
		out_width = isp->pipeline.out_pix.width;
		out_height = isp->pipeline.out_pix.height;

		/* If Resizer is in the take the crop in calculation */
		if (isp->pipeline.modules & OMAP_ISP_RESIZER) {
			in_width = isp->pipeline.rsz.in.crop.width;
			in_height = isp->pipeline.rsz.in.crop.height;
		}

		deviders = isp_get_upscale_ratio(in_width, in_height,
						 out_width, out_height);

		isp_csi_set_vp_freq(&isp->isp_csi, deviders->csi2_div);

		ispccdc_config_vp_freq(&isp->isp_ccdc, deviders->ccdc_div);
	} else {
		/* Make adjustment depending of the pixel clock */
		unsigned long pixelclk = isp->ccdc_clk;
		unsigned long l3_ick = clk_get_rate(isp->l3_ick);
		unsigned long div = 2;

		/* Calculate devider and clamp result between 0 and 64 */
		if (l3_ick && pixelclk) {
			div = clamp_t(unsigned long, l3_ick / pixelclk, 2,
				      (ISPCCDC_FMTCFG_VPIF_FRQ_MASK >>
				      ISPCCDC_FMTCFG_VPIF_FRQ_SHIFT) + 1);

			/* Feedback control */
			if ((div > 2) && ((l3_ick / div) < pixelclk))
				div--;
		}

		/* The range of register value is a 2..62 */
		ispccdc_config_vp_freq(&isp->isp_ccdc, div - 2);
	}

	return;
}

/**
 * isp_enable_interrupts - Enable ISP interrupts.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_enable_interrupts(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	u32 irq0enable;

	irq0enable = IRQ0ENABLE_CCDC_VD0_IRQ
		| IRQ0ENABLE_CSIA_IRQ | IRQ0ENABLE_CSIB_LCM_IRQ
		| IRQ0ENABLE_CSIB_LC0_IRQ | IRQ0ENABLE_HIST_DONE_IRQ
		| IRQ0ENABLE_H3A_AWB_DONE_IRQ | IRQ0ENABLE_H3A_AF_DONE_IRQ
		| IRQ0ENABLE_HS_VS_IRQ
		| isp->interrupts;

	if (CCDC_PREV_CAPTURE(isp))
		irq0enable |= IRQ0ENABLE_PRV_DONE_IRQ;

	if (CCDC_PREV_RESZ_CAPTURE(isp))
		irq0enable |= IRQ0ENABLE_PRV_DONE_IRQ | IRQ0ENABLE_RSZ_DONE_IRQ;

	isp_reg_writel(dev, -1, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0STATUS);
	isp_complete_reset = 0;

	isp_reg_or(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE, irq0enable);

	return;
}

/**
 * isp_disable_interrupts - Disable all ISP interrupts.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_disable_interrupts(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	u32 irq0enable;

	irq0enable = ~(IRQ0ENABLE_CCDC_VD0_IRQ
		| IRQ0ENABLE_CSIA_IRQ | IRQ0ENABLE_CSIB_LCM_IRQ
		| IRQ0ENABLE_CSIB_LC0_IRQ | IRQ0ENABLE_HIST_DONE_IRQ
		| IRQ0ENABLE_H3A_AWB_DONE_IRQ | IRQ0ENABLE_H3A_AF_DONE_IRQ
		| IRQ0ENABLE_HS_VS_IRQ
		| isp->interrupts);

	if (CCDC_PREV_CAPTURE(isp))
		irq0enable &= ~IRQ0ENABLE_PRV_DONE_IRQ;

	if (CCDC_PREV_RESZ_CAPTURE(isp))
		irq0enable &= ~(IRQ0ENABLE_PRV_DONE_IRQ
			| IRQ0ENABLE_RSZ_DONE_IRQ);

	isp_reg_and(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE, irq0enable);
}

/**
 * isp_set_callback - Set an external callback for an ISP interrupt.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @type: Type of the event for which callback is requested.
 * @callback: Method to be called as callback in the ISR context.
 * @arg1: First argument to be passed when callback is called in ISR.
 * @arg2: Second argument to be passed when callback is called in ISR.
 *
 * This function sets a callback function for a done event in the ISP
 * module, and enables the corresponding interrupt.
 **/
int isp_set_callback(struct device *dev, enum isp_callback_type type,
		     isp_callback_t callback, isp_vbq_callback_ptr arg1,
		     void *arg2)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	unsigned long irqflags = 0;

	if (callback == NULL) {
		DPRINTK_ISPCTRL("ISP_ERR : Null Callback\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&isp->lock, irqflags);
	isp->irq.isp_callbk[type] = callback;
	isp->irq.isp_callbk_arg1[type] = arg1;
	isp->irq.isp_callbk_arg2[type] = arg2;
	spin_unlock_irqrestore(&isp->lock, irqflags);

	switch (type) {
	case CBK_PREV_DONE:
		isp_reg_writel(dev, IRQ0ENABLE_PRV_DONE_IRQ,
			       OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0STATUS);
		isp_reg_or(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE,
			   IRQ0ENABLE_PRV_DONE_IRQ);
		break;
	case CBK_RESZ_DONE:
		isp_reg_writel(dev, IRQ0ENABLE_RSZ_DONE_IRQ,
			       OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0STATUS);
		isp_reg_or(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE,
			   IRQ0ENABLE_RSZ_DONE_IRQ);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(isp_set_callback);

/**
 * isp_unset_callback - Clears the callback for the ISP module done events.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @type: Type of the event for which callback to be cleared.
 *
 * This function clears a callback function for a done event in the ISP
 * module, and disables the corresponding interrupt.
 **/
int isp_unset_callback(struct device *dev, enum isp_callback_type type)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	unsigned long irqflags = 0;

	spin_lock_irqsave(&isp->lock, irqflags);
	isp->irq.isp_callbk[type] = NULL;
	isp->irq.isp_callbk_arg1[type] = NULL;
	isp->irq.isp_callbk_arg2[type] = NULL;
	spin_unlock_irqrestore(&isp->lock, irqflags);

	switch (type) {
	case CBK_PREV_DONE:
		isp_reg_and(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE,
			    ~IRQ0ENABLE_PRV_DONE_IRQ);
		break;
	case CBK_RESZ_DONE:
		isp_reg_and(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE,
			    ~IRQ0ENABLE_RSZ_DONE_IRQ);
		break;
	default:
		break;
	}

	return 0;
}
EXPORT_SYMBOL(isp_unset_callback);

/**
 * isp_set_xclk - Configures the specified cam_xclk to the desired frequency.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @xclk: Desired frequency of the clock in Hz.
 * @xclksel: XCLK to configure (0 = A, 1 = B).
 *
 * Configures the specified MCLK divisor in the ISP timing control register
 * (TCTRL_CTRL) to generate the desired xclk clock value.
 *
 * Divisor = mclk / xclk
 *
 * Returns the final frequency that is actually being generated
 **/
u32 isp_set_xclk(struct device *dev, u32 xclk, u8 xclksel)
{
	u32 divisor;
	u32 currentxclk;
	struct isp_device *isp = dev_get_drvdata(dev);

	if (xclk >= isp->mclk) {
		divisor = ISPTCTRL_CTRL_DIV_BYPASS;
		currentxclk = isp->mclk;
	} else if (xclk >= 2) {
		divisor = isp->mclk / xclk;
		if (divisor >= ISPTCTRL_CTRL_DIV_BYPASS)
			divisor = ISPTCTRL_CTRL_DIV_BYPASS - 1;
		currentxclk = isp->mclk / divisor;
	} else {
		divisor = xclk;
		currentxclk = 0;
	}

	switch (xclksel) {
	case 0:
		isp_reg_and_or(dev, OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_CTRL,
			       ~ISPTCTRL_CTRL_DIVA_MASK,
			       divisor << ISPTCTRL_CTRL_DIVA_SHIFT);
		DPRINTK_ISPCTRL("isp_set_xclk(): cam_xclka set to %d Hz\n",
				currentxclk);
		break;
	case 1:
		isp_reg_and_or(dev, OMAP3_ISP_IOMEM_MAIN, ISP_TCTRL_CTRL,
			       ~ISPTCTRL_CTRL_DIVB_MASK,
			       divisor << ISPTCTRL_CTRL_DIVB_SHIFT);
		DPRINTK_ISPCTRL("isp_set_xclk(): cam_xclkb set to %d Hz\n",
				currentxclk);
		break;
	default:
		DPRINTK_ISPCTRL("ISP_ERR: isp_set_xclk(): Invalid requested "
				"xclk. Must be 0 (A) or 1 (B).\n");
		return -EINVAL;
	}

	return currentxclk;
}
EXPORT_SYMBOL(isp_set_xclk);

/**
 * isp_power_settings - Sysconfig settings, for Power Management.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @idle: Consider idle state.
 *
 * Sets the power settings for the ISP, and SBL bus.
 **/
static void isp_power_settings(struct device *dev, int idle)
{
	if (idle) {
		isp_reg_writel(dev,
			       (ISP_SYSCONFIG_MIDLEMODE_SMARTSTANDBY <<
				ISP_SYSCONFIG_MIDLEMODE_SHIFT),
			       OMAP3_ISP_IOMEM_MAIN, ISP_SYSCONFIG);
		if (omap_rev() == OMAP3430_REV_ES1_0) {
			isp_reg_writel(dev, ISPCSI1_AUTOIDLE |
				       (ISPCSI1_MIDLEMODE_SMARTSTANDBY <<
					ISPCSI1_MIDLEMODE_SHIFT),
				       OMAP3_ISP_IOMEM_CSI2A,
				       ISP_CSIA_SYSCONFIG);
			isp_reg_writel(dev, ISPCSI1_AUTOIDLE |
				       (ISPCSI1_MIDLEMODE_SMARTSTANDBY <<
					ISPCSI1_MIDLEMODE_SHIFT),
				       OMAP3_ISP_IOMEM_CCP2,
				       ISP_CSIB_SYSCONFIG);
		}
		isp_reg_writel(dev, ISPCTRL_SBL_AUTOIDLE, OMAP3_ISP_IOMEM_MAIN,
			       ISP_CTRL);

	} else {
		isp_reg_writel(dev,
			       (ISP_SYSCONFIG_MIDLEMODE_FORCESTANDBY <<
				ISP_SYSCONFIG_MIDLEMODE_SHIFT),
			       OMAP3_ISP_IOMEM_MAIN, ISP_SYSCONFIG);
		if (omap_rev() == OMAP3430_REV_ES1_0) {
			isp_reg_writel(dev, ISPCSI1_AUTOIDLE |
				       (ISPCSI1_MIDLEMODE_FORCESTANDBY <<
					ISPCSI1_MIDLEMODE_SHIFT),
				       OMAP3_ISP_IOMEM_CSI2A,
				       ISP_CSIA_SYSCONFIG);

			isp_reg_writel(dev, ISPCSI1_AUTOIDLE |
				       (ISPCSI1_MIDLEMODE_FORCESTANDBY <<
					ISPCSI1_MIDLEMODE_SHIFT),
				       OMAP3_ISP_IOMEM_CCP2,
				       ISP_CSIB_SYSCONFIG);
		}

		isp_reg_writel(dev, ISPCTRL_SBL_AUTOIDLE, OMAP3_ISP_IOMEM_MAIN,
			       ISP_CTRL);
	}
}

/**
 * isp_configure_interface - Configures ISP Control I/F related parameters.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @config: Pointer to structure containing the desired configuration for the
 *          ISP.
 *
 * Configures ISP control register (ISP_CTRL) with the values specified inside
 * the config structure. Controls:
 * - Selection of parallel or serial input to the preview hardware.
 * - Data lane shifter.
 * - Pixel clock polarity.
 * - 8 to 16-bit bridge at the input of CCDC module.
 * - HS or VS synchronization signal detection
 *
 * Returns 0 on success, otherwise, will return other negative error value.
 **/
int isp_configure_interface(struct device *dev,
			    struct isp_interface_config *config)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	u32 ispctrl_val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_MAIN, ISP_CTRL);
	int r;

	isp->config = config;

	ispctrl_val &= ISPCTRL_SHIFT_MASK;
	ispctrl_val |= config->dataline_shift << ISPCTRL_SHIFT_SHIFT;
	ispctrl_val &= ~ISPCTRL_PAR_CLK_POL_INV;

	ispctrl_val &= ISPCTRL_PAR_SER_CLK_SEL_MASK;

	isp_buf_init(dev);

	switch (config->ccdc_par_ser) {
	case ISP_PARLL:
		ispctrl_val |= ISPCTRL_PAR_SER_CLK_SEL_PARALLEL;
		ispctrl_val |= config->u.par.par_clk_pol
			<< ISPCTRL_PAR_CLK_POL_SHIFT;
		ispctrl_val &= ~ISPCTRL_PAR_BRIDGE_BENDIAN;
		ispctrl_val |= config->u.par.par_bridge
			<< ISPCTRL_PAR_BRIDGE_SHIFT;
		break;
	case ISP_CSIA:
		ispctrl_val |= ISPCTRL_PAR_SER_CLK_SEL_CSIA;
		ispctrl_val &= ~ISPCTRL_PAR_BRIDGE_BENDIAN;

		if (config->u.csi.crc)
			isp_csi2_ctrl_config_ecc_enable(&isp->isp_csi2, true);

		isp_csi2_ctrl_config_vp_out_ctrl(&isp->isp_csi2,
						 config->u.csi.vpclk);
		switch (isp->pipeline.csia_out) {
		case CSI2_MEM:
			isp_csi2_ctrl_config_vp_only_enable(&isp->isp_csi2,
							    false);
			isp_csi2_ctrl_config_vp_clk_enable(&isp->isp_csi2,
							   false);
			break;
		case CSI2_VP:
			isp_csi2_ctrl_config_vp_only_enable(&isp->isp_csi2,
							    true);
			isp_csi2_ctrl_config_vp_clk_enable(&isp->isp_csi2,
							   true);
			break;
		case CSI2_MEM_VP:
			isp_csi2_ctrl_config_vp_only_enable(&isp->isp_csi2,
							    false);
			isp_csi2_ctrl_config_vp_clk_enable(&isp->isp_csi2,
							   true);
			break;
		default:
			return -EINVAL;
		}
		isp_csi2_ctrl_update(&isp->isp_csi2, false);

		isp_csi2_ctx_config_format(&isp->isp_csi2, 0,
					   isp->pipeline.in_pix.pixelformat);
		isp_csi2_ctx_update(&isp->isp_csi2, 0, false);

		isp_csi2_irq_complexio1_set(&isp->isp_csi2, 1);
		isp_csi2_irq_status_set(&isp->isp_csi2, 1);

		isp_csi2_enable(&isp->isp_csi2, 1);
		if (isp->pipeline.csia_out != CSI2_MEM)
			isp_csi2_ctx_config_enabled(&isp->isp_csi2, 0, true);
		break;
	case ISP_CSIB:
		ispctrl_val |= ISPCTRL_PAR_SER_CLK_SEL_CSIB;

		/* FIXME: Provide cleaner mechanism for shared port usage */
		if (config->u.csi.use_mem_read)
			ispctrl_val |= ISPCTRL_SBL_SHARED_RPORTA
				       | ISPCTRL_SBL_RD_RAM_EN;
		r = isp_csi_configure_interface(&isp->isp_csi, &config->u.csi);
		if (r)
			return r;
		break;
	case ISP_NONE:
		ispctrl_val &= ~ISPCTRL_SBL_SHARED_RPORTA;
		isp_reg_writel(dev, ispctrl_val,
			       OMAP3_ISP_IOMEM_MAIN, ISP_CTRL);
		return 0;
	default:
		return -EINVAL;
	}

	ispctrl_val &= ~ISPCTRL_SYNC_DETECT_VSRISE;
	ispctrl_val |= config->hsvs_syncdetect;

	isp_reg_writel(dev, ispctrl_val, OMAP3_ISP_IOMEM_MAIN, ISP_CTRL);

	/* Set sensor specific fields in CCDC and Previewer module. */
	ispccdc_set_wenlog(&isp->isp_ccdc, config->wenlog);
	ispccdc_set_raw_offset(&isp->isp_ccdc, config->raw_fmt_in);

	isp->mclk = config->cam_mclk;
	isp_enable_mclk(dev);

	isp_adjust_bandwidth(dev);

	return 0;
}
EXPORT_SYMBOL(isp_configure_interface);

void isp_hist_dma_done(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_irq *irqdis = &isp->irq;

	isp_hist_enable(&isp->isp_hist, 1);
	if (ispccdc_busy(&isp->isp_ccdc)) {
		/* Histogram cannot be enabled in this frame anymore */
		isp_hist_enable(&isp->isp_hist, 0);
		if (isp_hist_busy(&isp->isp_hist))
			isp_hist_mark_invalid_buf(&isp->isp_hist);
	}
	if (irqdis->isp_callbk[CBK_CATCHALL]) {
		irqdis->isp_callbk[CBK_CATCHALL](
			HIST_DONE,
			irqdis->isp_callbk_arg1[CBK_CATCHALL],
			irqdis->isp_callbk_arg2[CBK_CATCHALL]);
	}
}

static void isp_buf_process(struct device *dev, struct isp_bufs *bufs);

/**
 * isp_isr - Interrupt Service Routine for Camera ISP module.
 * @irq: Not used currently.
 * @_pdev: Pointer to the platform device associated with the OMAP3 ISP.
 *
 * Handles the corresponding callback if plugged in.
 *
 * Returns IRQ_HANDLED when IRQ was correctly handled, or IRQ_NONE when the
 * IRQ wasn't handled.
 **/
static irqreturn_t isp_isr(int irq, void *_pdev)
{
	struct device *dev = &((struct platform_device *)_pdev)->dev;
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_irq *irqdis = &isp->irq;
	struct isp_bufs *bufs = &isp->bufs;
	struct isp_buf *buf;
	unsigned long flags;
	u32 irqstatus = 0;
	u32 irqenable = 0;
	u32 sbl_pcr;
	int wait_hs_vs = 0;
	int ret;

	irqstatus = isp_reg_readl(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0STATUS);
	isp_reg_writel(dev, irqstatus, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0STATUS);

	if ((isp->running == ISP_STOPPED) &&
		!irqdis->isp_callbk[CBK_RESZ_DONE])
		return IRQ_NONE;

	irqenable = isp_reg_readl(dev, OMAP3_ISP_IOMEM_MAIN, ISP_IRQ0ENABLE);
	irqstatus &= irqenable;

	if (irqstatus & CCDC_VD1)
		isp->isp_ccdc.lsc_request_enable =
					isp->isp_ccdc.lsc_request_user;

	/* Handle first LSC states */
	if ((irqstatus & LSC_PRE_ERR) || (irqstatus & LSC_DONE) ||
	    (irqstatus & CCDC_VD1))
		ispccdc_lsc_state_handler(&isp->isp_ccdc, irqstatus);

	if ((isp->running == ISP_STOPPING) &&
		!(irqdis->isp_callbk[CBK_RESZ_DONE] &&
			(irqstatus & RESZ_DONE)))
		goto out_stopping_isp;

	spin_lock_irqsave(&isp->lock, flags);
	wait_hs_vs = bufs->wait_hs_vs;
	if (irqstatus & CCDC_VD0 && bufs->wait_hs_vs)
		bufs->wait_hs_vs--;

	/* Decrement value also with CSI2 Rx only case*/
	if ((irqstatus & CSIA) &&
	    (isp->pipeline.modules == OMAP_ISP_CSIARX) && bufs->wait_hs_vs) {
		u32 csi2_irqstatus = isp_reg_readl(dev,
						   OMAP3_ISP_IOMEM_CSI2A,
						   ISPCSI2_IRQSTATUS);
		isp_reg_writel(dev, csi2_irqstatus,
			       OMAP3_ISP_IOMEM_CSI2A, ISPCSI2_IRQSTATUS);

		if (csi2_irqstatus & ISPCSI2_IRQSTATUS_COMPLEXIO1_ERR_IRQ) {
			u32 cpxio1_irqstatus = isp_reg_readl(dev,
						OMAP3_ISP_IOMEM_CSI2A,
						ISPCSI2_COMPLEXIO1_IRQSTATUS);
		isp_reg_writel(dev, cpxio1_irqstatus,
			       OMAP3_ISP_IOMEM_CSI2A,
			       ISPCSI2_COMPLEXIO1_IRQSTATUS);
		}

		if (csi2_irqstatus & ISPCSI2_IRQSTATUS_CONTEXT(0)) {
			u32 ctxirqstatus = isp_reg_readl(dev,
						OMAP3_ISP_IOMEM_CSI2A,
						ISPCSI2_CTX_IRQSTATUS(0));
			isp_reg_writel(dev, ctxirqstatus,
				       OMAP3_ISP_IOMEM_CSI2A,
				       ISPCSI2_CTX_IRQSTATUS(0));
			if (ctxirqstatus & ISPCSI2_CTX_IRQSTATUS_FE_IRQ)
				bufs->wait_hs_vs--;
		}
		goto out_ignore_buff;
	}
	/*
	 * We need to wait for the first HS_VS interrupt from CCDC.
	 * Otherwise our frame (and everything else) might be bad.
	 */
	switch (wait_hs_vs) {
	case 1:
		/*
		 * Enable preview for the first time. We just have
		 * missed the start-of-frame so we can do it now.
		 */
		if (irqstatus & CCDC_VD0) {
			/*
			 * For some reason resizer is always busy after boot up
			 * do not check resizer busy now.
			 */
			if ((isp->pipeline.modules & OMAP_ISP_RESIZER) &&
			     !ispresizer_busy(&isp->isp_res) &&
			     !ispresizer_is_enabled(&isp->isp_res)) {
				ispresizer_config_shadow_registers(
					&isp->isp_res);
				ispresizer_enable(&isp->isp_res, 1);
			}
			if ((isp->pipeline.modules & OMAP_ISP_PREVIEW) &&
			     !isppreview_busy(&isp->isp_prev) &&
			     !isppreview_is_enabled(&isp->isp_prev)) {
				isppreview_config_shadow_registers(
					&isp->isp_prev);
				isppreview_enable(&isp->isp_prev, 1);
			}
		}
	default:
		/*
		 * For some sensors (like stingray), after a _restart_
		 * from sw standby state, starting couple of frames
		 * are erroneous. From stingray datasheet:
		 *  "When sensor restarts, Normal image can get 2 frames after"
		 *
		 * So while we wait for HS_VS, check cnd clear the CSIA and
		 * CSIB error interrupts, if any
		 */
		if (irqstatus & CSIA)
			isp_csi2_isr(&isp->isp_csi2);

		if (!(irqstatus & RESZ_DONE) || CCDC_PREV_RESZ_CAPTURE(isp))
			goto out_ignore_buff;
	case 0:
		break;
	}
	buf = ISP_BUF_DONE(bufs);

	if (irqstatus & LSC_PRE_ERR) {
		/* Mark buffer faulty. */
		buf->vb_state = VIDEOBUF_ERROR;
		dev_dbg(dev, "lsc prefetch error \n");
	}

	if (irqstatus & CSIA) {
		int ret = isp_csi2_isr(&isp->isp_csi2);
		if (ret)
			buf->vb_state = VIDEOBUF_ERROR;
	}

	if (irqstatus & IRQ0STATUS_CSIB_LC0_IRQ) {
		static const u32 ISPCSI1_LC01_ERROR =
			ISPCSI1_LC01_IRQSTATUS_LC0_FIFO_OVF_IRQ |
			ISPCSI1_LC01_IRQSTATUS_LC0_CRC_IRQ |
			ISPCSI1_LC01_IRQSTATUS_LC0_FSP_IRQ |
			ISPCSI1_LC01_IRQSTATUS_LC0_FW_IRQ |
			ISPCSI1_LC01_IRQSTATUS_LC0_FSC_IRQ |
			ISPCSI1_LC01_IRQSTATUS_LC0_SSC_IRQ;
		u32 ispcsi1_lc01_irqstatus, ispcsi1_lcm_irqstatus;

		ispcsi1_lc01_irqstatus = isp_reg_readl(dev,
						       OMAP3_ISP_IOMEM_CCP2,
						       ISPCSI1_LC01_IRQSTATUS);
		isp_reg_writel(dev, ispcsi1_lc01_irqstatus,
			       OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LC01_IRQSTATUS);
		if (ispcsi1_lc01_irqstatus & ISPCSI1_LC01_ERROR) {
			buf->vb_state = VIDEOBUF_ERROR;
			dev_dbg(dev, "CCP2 err:%x\n", ispcsi1_lc01_irqstatus);
		}

		ispcsi1_lcm_irqstatus = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2,
						  ISPCSI1_LCM_IRQSTATUS);
		isp_reg_writel(dev, ispcsi1_lcm_irqstatus, OMAP3_ISP_IOMEM_CCP2,
			       ISPCSI1_LCM_IRQSTATUS);
		if (ispcsi1_lcm_irqstatus &
		    ISPCSI1_LCM_IRQSTATUS_LCM_OCPERROR) {
			buf->vb_state = VIDEOBUF_ERROR;
			dev_err(dev, "CCP2 err:%x\n", ispcsi1_lcm_irqstatus);
		}
		if (ispcsi1_lcm_irqstatus & ISPCSI1_LCM_IRQSTATUS_LCM_EOF)
			dev_err(dev, "CCP2 EOF\n");
	}

	if (irqstatus & IRQ0STATUS_CSIB_LCM_IRQ) {
		isp_reg_writel(dev, ISPCSI1_LCM_IRQSTATUS_LCM_OCPERROR |
				    ISPCSI1_LCM_IRQSTATUS_LCM_EOF,
				    OMAP3_ISP_IOMEM_CCP2,
				    ISPCSI1_LCM_IRQSTATUS);
	}

	if (irqstatus & RESZ_DONE) {
		if (irqdis->isp_callbk[CBK_RESZ_DONE])
			irqdis->isp_callbk[CBK_RESZ_DONE](
				RESZ_DONE,
				irqdis->isp_callbk_arg1[CBK_RESZ_DONE],
				irqdis->isp_callbk_arg2[CBK_RESZ_DONE]);
		else if (CCDC_PREV_RESZ_CAPTURE(isp)) {
			isp_buf_process(dev, bufs);
		}
	}

	if (irqstatus & CCDC_VD1) {
		ispccdc_config_shadow_registers(&isp->isp_ccdc);
		/*
		 * If CCDC is writing to memory stop CCDC here
		 * preventig to write to any of our buffers.
		 */
		if (CCDC_CAPTURE(isp) || isp->config->u.csi.use_mem_read)
			ispccdc_enable(&isp->isp_ccdc, 0);
	}

	if (irqstatus & CCDC_VD0) {
		if (CCDC_CAPTURE(isp))
			isp_buf_process(dev, bufs);

		/* Enabling configured statistic modules */
		if (!(irqstatus & H3A_AWB_DONE))
			isph3a_aewb_try_enable(&isp->isp_h3a);
		if (!(irqstatus & H3A_AF_DONE))
			isp_af_try_enable(&isp->isp_af);
		if (!(irqstatus & HIST_DONE))
			isp_hist_try_enable(&isp->isp_hist);
	}

	if (irqstatus & PREV_DONE) {
		if (irqdis->isp_callbk[CBK_PREV_DONE])
			irqdis->isp_callbk[CBK_PREV_DONE](
				PREV_DONE,
				irqdis->isp_callbk_arg1[CBK_PREV_DONE],
				irqdis->isp_callbk_arg2[CBK_PREV_DONE]);
		else {
			if (CCDC_PREV_RESZ_CAPTURE(isp)) {
				if (ispresizer_busy(&isp->isp_res)) {
					buf->vb_state = VIDEOBUF_ERROR;
					dev_dbg(dev, "resizer busy.\n");
				} else if (!ISP_BUFS_IS_EMPTY(bufs)) {
					ispresizer_config_shadow_registers(
						&isp->isp_res);
					ispresizer_enable(&isp->isp_res, 1);
				}
			}
			if (!ISP_BUFS_IS_EMPTY(bufs)) {
				isppreview_config_shadow_registers(
					&isp->isp_prev);
				isppreview_enable(&isp->isp_prev, 1);
			}
			if (CCDC_PREV_CAPTURE(isp))
				isp_buf_process(dev, bufs);
		}
	}

	/*
	 * Handle shared buffer logic overflows for video buffers.
	 * ISPSBL_PCR_CCDCPRV_2_RSZ_OVF can be safely ignored.
	 */
	sbl_pcr = isp_reg_readl(dev, OMAP3_ISP_IOMEM_SBL, ISPSBL_PCR) &
		~ISPSBL_PCR_CCDCPRV_2_RSZ_OVF;
	isp_reg_writel(dev, sbl_pcr, OMAP3_ISP_IOMEM_SBL, ISPSBL_PCR);
	if (sbl_pcr & (ISPSBL_PCR_RSZ1_WBL_OVF
		       | ISPSBL_PCR_RSZ2_WBL_OVF
		       | ISPSBL_PCR_RSZ3_WBL_OVF
		       | ISPSBL_PCR_RSZ4_WBL_OVF
		       | ISPSBL_PCR_PRV_WBL_OVF
		       | ISPSBL_PCR_CCDC_WBL_OVF
		       | ISPSBL_PCR_CSIA_WBL_OVF
		       | ISPSBL_PCR_CSIB_WBL_OVF)) {
		buf->vb_state = VIDEOBUF_ERROR;
		isp->isp_af.buf_err = 1;
		isp->isp_h3a.buf_err = 1;
		isp_hist_mark_invalid_buf(&isp->isp_hist);
		dev_dbg(dev, "sbl overflow, sbl_pcr = %8.8x\n", sbl_pcr);
	}

	if (sbl_pcr & ISPSBL_PCR_H3A_AF_WBL_OVF) {
		dev_dbg(dev, "af: sbl overflow detected.\n");
		isp->isp_af.buf_err = 1;
	}

	if (sbl_pcr & ISPSBL_PCR_H3A_AEAWB_WBL_OVF) {
		dev_dbg(dev, "h3a: sbl overflow detected.\n");
		isp->isp_h3a.buf_err = 1;
	}

	if (irqstatus & H3A_AWB_DONE) {
		isph3a_aewb_enable(&isp->isp_h3a, 0);
		/* If it's busy we can't process this buffer anymore */
		if (!isph3a_aewb_busy(&isp->isp_h3a)) {
			ret = isph3a_aewb_buf_process(&isp->isp_h3a);
			isph3a_aewb_config_registers(&isp->isp_h3a);
		} else {
			ret = -1;
			dev_dbg(dev, "h3a: cannot process buffer, device is "
				     "busy.\n");
		}
		if (ret)
			irqstatus &= ~H3A_AWB_DONE;
		isph3a_aewb_enable(&isp->isp_h3a, 1);
	}

	if (irqstatus & H3A_AF_DONE) {
		isp_af_enable(&isp->isp_af, 0);
		/* If it's busy we can't process this buffer anymore */
		if (!isp_af_busy(&isp->isp_af)) {
			ret = isp_af_buf_process(&isp->isp_af);
			isp_af_config_registers(&isp->isp_af);
		} else {
			ret = -1;
			dev_dbg(dev, "af: cannot process buffer, device is "
				     "busy.\n");
		}
		if (ret)
			irqstatus &= ~H3A_AF_DONE;
		isp_af_enable(&isp->isp_af, 1);
	}

	if (irqstatus & HIST_DONE) {
		isp_hist_enable(&isp->isp_hist, 0);
		/* If it's busy we can't process this buffer anymore */
		if (!isp_hist_busy(&isp->isp_hist)) {
			ret = isp_hist_buf_process(&isp->isp_hist);
			isp_hist_config_registers(&isp->isp_hist);
		} else {
			dev_dbg(dev, "hist: cannot process buffer, device is "
				     "busy.\n");
			/* current and next buffer might have invalid data */
			isp_hist_mark_invalid_buf(&isp->isp_hist);
			irqstatus &= ~HIST_DONE;
			ret = HIST_NO_BUF;
		}
		if (ret != HIST_BUF_WAITING_DMA)
			isp_hist_enable(&isp->isp_hist, 1);
		if (ret != HIST_BUF_DONE)
			irqstatus &= ~HIST_DONE;
	}

	if (irqdis->isp_callbk[CBK_CATCHALL]) {
		irqdis->isp_callbk[CBK_CATCHALL](
			irqstatus,
			irqdis->isp_callbk_arg1[CBK_CATCHALL],
			irqdis->isp_callbk_arg2[CBK_CATCHALL]);
	}

out_ignore_buff:
	spin_unlock_irqrestore(&isp->lock, flags);

	if (irqstatus & LSC_PRE_COMP) {
		ispccdc_lsc_pref_comp_handler(&isp->isp_ccdc);
		if (isp->config->u.csi.use_mem_read) {
			isp_adjust_bandwidth(dev);
			isp_csi_lcm_readport_enable(&isp->isp_csi, 1);
		}
	}

out_stopping_isp:
	isp_flush(dev);
#if 1
	{
		static const struct {
			int num;
			char *name;
		} bits[] = {
			{ 31, "HS_VS_IRQ" },
			{ 30, "SEC_ERR_IRQ" },
			{ 29, "OCP_ERR_IRQ" },
			{ 28, "MMU_ERR_IRQ" },
			{ 27, "res27" },
			{ 26, "res26" },
			{ 25, "OVF_IRQ" },
			{ 24, "RSZ_DONE_IRQ" },
			{ 23, "res23" },
			{ 22, "res22" },
			{ 21, "CBUFF_IRQ" },
			{ 20, "PRV_DONE_IRQ" },
			{ 19, "CCDC_LSC_PREFETCH_ERROR" },
			{ 18, "CCDC_LSC_PREFETCH_COMPLETED" },
			{ 17, "CCDC_LSC_DONE" },
			{ 16, "HIST_DONE_IRQ" },
			{ 15, "res15" },
			{ 14, "res14" },
			{ 13, "H3A_AWB_DONE_IRQ" },
			{ 12, "H3A_AF_DONE_IRQ" },
			{ 11, "CCDC_ERR_IRQ" },
			{ 10, "CCDC_VD2_IRQ" },
			{  9, "CCDC_VD1_IRQ" },
			{  8, "CCDC_VD0_IRQ" },
			{  7, "res7" },
			{  6, "res6" },
			{  5, "res5" },
			{  4, "CSIB_IRQ" },
			{  3, "CSIB_LCM_IRQ" },
			{  2, "res2" },
			{  1, "res1" },
			{  0, "CSIA_IRQ" },
		};
		int i;
		for (i = 0; i < ARRAY_SIZE(bits); i++) {
			if ((1 << bits[i].num) & irqstatus)
				DPRINTK_ISPCTRL("%s ", bits[i].name);
		}
		DPRINTK_ISPCTRL("\n");
	}
#endif

	return IRQ_HANDLED;
}

/* Device name, needed for resource tracking layer */
struct device_driver camera_drv = {
	.name = "camera"
};

struct device camera_dev = {
	.driver = &camera_drv,
};

/**
 * isp_tmp_buf_free - Free buffer for CCDC->PRV->RSZ datapath workaround.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_tmp_buf_free(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	if (isp->tmp_buf) {
		iommu_vfree(isp->iommu, isp->tmp_buf);
		isp->tmp_buf = 0;
		isp->tmp_buf_size = 0;
		isp->tmp_buf_offset = 0;
	}
}

/**
 * isp_tmp_buf_alloc - Allocate buffer for CCDC->PRV->RSZ datapath workaround.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @size: Byte size of the buffer to allocate
 *
 * Returns 0 if successful, or -ENOMEM if there's no available memory.
 **/
static u32 isp_tmp_buf_alloc(struct device *dev, struct isp_pipeline *pipe)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	u32 da;
	size_t size = PAGE_ALIGN(isp->pipeline.prv.out.image.width *
				 isp->pipeline.prv.out.image.height *
				 ISP_BYTES_PER_PIXEL);

	if (isp->tmp_buf_size >= size)
		return 0;

	isp_tmp_buf_free(dev);

	da = iommu_vmalloc(isp->iommu, 0, size, IOMMU_FLAG);
	if (IS_ERR_VALUE(da)) {
		dev_err(dev, "iommu_vmap mapping failed\n");
		return -ENOMEM;
	}
	isp->tmp_buf = da;
	isp->tmp_buf_size = size;

	isppreview_set_outaddr(&isp->isp_prev, isp->tmp_buf);
	ispresizer_set_inaddr(&isp->isp_res, isp->tmp_buf, NULL);

	return 0;
}

/**
 * isp_start - Set ISP in running state
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
void isp_start(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	isp->running = ISP_RUNNING;

#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	if (isp->dfs_isp)
		isp_dfs_dump(isp);
#endif
	return;
}
EXPORT_SYMBOL(isp_start);

#define ISP_STATISTICS_BUSY			\
	()
#define ISP_STOP_TIMEOUT	msecs_to_jiffies(1000)

/**
 * __isp_disable_modules - Disable ISP submodules with a timeout to be idle.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @suspend: If 0, disable modules; if 1, send modules to suspend state.
 *
 * Returns 0 if stop/suspend left in idle state all the submodules properly,
 * or returns 1 if a general Reset is required to stop/suspend the submodules.
 **/
static int __isp_disable_modules(struct device *dev, int suspend)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	unsigned long timeout = jiffies + ISP_STOP_TIMEOUT;
	int reset = 0;

	/* We need to disble the first LSC module */
	timeout = jiffies + ISP_STOP_TIMEOUT;
	while (ispccdc_lsc_delay_stop(&isp->isp_ccdc)) {
		if (time_after(jiffies, timeout)) {
			printk(KERN_ERR "%s: can't stop lsc "
					"disabling lsc anyway. \n", __func__);
			reset = 1;
			break;
		}
		msleep(1);
	}
	/* We can disable lsc now */
	ispccdc_enable_lsc(&isp->isp_ccdc, 0);

	/*
	 * We need to stop all the modules after CCDC or they'll
	 * never stop since they may not get a full frame from CCDC.
	 */
	if (suspend) {
		isp_af_suspend(&isp->isp_af);
		isph3a_aewb_suspend(&isp->isp_h3a);
		isp_hist_suspend(&isp->isp_hist);
	} else {
		isp_af_enable(&isp->isp_af, 0);
		isph3a_aewb_enable(&isp->isp_h3a, 0);
		isp_hist_enable(&isp->isp_hist, 0);
	}

	timeout = jiffies + ISP_STOP_TIMEOUT;
	while (isp_af_busy(&isp->isp_af)
	       || isph3a_aewb_busy(&isp->isp_h3a)
	       || isp_hist_busy(&isp->isp_hist)
	       || isppreview_busy(&isp->isp_prev)
	       || ispresizer_busy(&isp->isp_res)) {
		if (time_after(jiffies, timeout)) {
			dev_info(dev, "can't stop non-ccdc modules.\n");
			reset = 1;
			break;
		}
		msleep(1);
	}

	/* Let's stop CCDC now. */
	ispccdc_enable(&isp->isp_ccdc, 0);

	timeout = jiffies + ISP_STOP_TIMEOUT;
	while (ispccdc_busy(&isp->isp_ccdc)) {
		if (time_after(jiffies, timeout)) {
			dev_info(dev, "can't stop ccdc module.\n");
			reset = 1;
			break;
		}
		msleep(1);
	}

	if (!reset) {
		DPRINTK_ISPCTRL(KERN_INFO
			"(%s) isp_complete_reset \n", __func__);
		isp_complete_reset = 1;
	}
	isp_csi_if_enable(&isp->isp_csi, 0);
	isp_csi_lcm_readport_enable(&isp->isp_csi, 0);
	isp_csi2_enable(&isp->isp_csi2, 0);
	isp_buf_init(dev);

	return reset;
}

/**
 * isp_stop_modules - Stop ISP submodules.
 * @dev: Device pointer specific to the OMAP3 ISP.
 *
 * Returns 0 if stop left in idle state all the submodules properly,
 * or returns 1 if a general Reset is required to stop the submodules.
 **/
static int isp_stop_modules(struct device *dev)
{
	return __isp_disable_modules(dev, 0);
}

/**
 * isp_suspend_modules - Suspend ISP submodules.
 * @dev: Device pointer specific to the OMAP3 ISP.
 *
 * Returns 0 if suspend left in idle state all the submodules properly,
 * or returns 1 if a general Reset is required to suspend the submodules.
 **/
static int isp_suspend_modules(struct device *dev)
{
	return __isp_disable_modules(dev, 1);
}

/**
 * isp_resume_modules - Resume ISP submodules.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_resume_modules(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	isp_hist_resume(&isp->isp_hist);
	isph3a_aewb_resume(&isp->isp_h3a);
	isp_af_resume(&isp->isp_af);

	if (isp->running == ISP_RUNNING) {
		if (isp->pipeline.modules & OMAP_ISP_CCDC)
			ispccdc_enable(&isp->isp_ccdc, 1);
		if (isp->pipeline.modules & OMAP_ISP_RESIZER)
			ispresizer_enable(&isp->isp_res, 1);
		if (isp->pipeline.modules & OMAP_ISP_PREVIEW)
			isppreview_enable(&isp->isp_prev, 1);
		isp_csi_if_enable(&isp->isp_csi, 1);
		isp_csi2_enable(&isp->isp_csi2, 1);
	}
}

int isp_get_used_modules(struct isp_device *dev)
{
	if (dev)
		return dev->pipeline.modules;
	else
		return 0;
}
EXPORT_SYMBOL(isp_get_used_modules);

/**
 * isp_reset - Reset ISP with a timeout wait for idle.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_reset(struct device *dev)
{
	unsigned long timeout = 0;

	isp_reg_writel(dev,
		       isp_reg_readl(dev, OMAP3_ISP_IOMEM_MAIN, ISP_SYSCONFIG)
		       | ISP_SYSCONFIG_SOFTRESET,
		       OMAP3_ISP_IOMEM_MAIN, ISP_SYSCONFIG);
	while (!(isp_reg_readl(dev, OMAP3_ISP_IOMEM_MAIN,
			       ISP_SYSSTATUS) & 0x1)) {
		if (timeout++ > 10000) {
			dev_alert(dev, "cannot reset ISP\n");
			break;
		}
		udelay(1);
	}
}

/**
 * isp_stop - Stop ISP.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
void isp_stop(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int reset;

	isp->running = ISP_STOPPING;
	isp_disable_interrupts(dev);
	synchronize_irq(((struct isp_device *)dev_get_drvdata(dev))->irq_num);
	reset = isp_stop_modules(dev);
	isp->running = ISP_STOPPED;
	if (!reset)
		return;

	isp_save_ctx(dev);
	isp_reset(dev);
	isp_restore_ctx(dev);
}
EXPORT_SYMBOL(isp_stop);

/**
 * isp_set_buf - Program output buffer address based on current pipeline.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @buf: Pointer to ISP buffer structure.
 **/
static void isp_set_buf(struct device *dev, struct isp_buf *buf)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	if (isp->pipeline.modules == OMAP_ISP_CSIARX) {
		isp_csi2_ctx_config_data_offset(&isp->isp_csi2, 0, 0);
		isp_csi2_ctx_config_ping_addr(&isp->isp_csi2, 0, buf->isp_addr);
		isp_csi2_ctx_config_pong_addr(&isp->isp_csi2, 0, buf->isp_addr);
		isp_csi2_ctx_update(&isp->isp_csi2, 0, false);
	} else if (CCDC_PREV_RESZ_CAPTURE(isp))
		ispresizer_set_outaddr(&isp->isp_res, buf->isp_addr);
	else if (CCDC_PREV_CAPTURE(isp))
		isppreview_set_outaddr(&isp->isp_prev, buf->isp_addr);
	else if (CCDC_CAPTURE(isp))
		ispccdc_set_outaddr(&isp->isp_ccdc, buf->isp_addr);

}

/**
 * isp_try_pipeline - Retrieve and simulate resulting internal ISP pipeline.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @pipe: Pointer to ISP pipeline structure to fill back.
 *
 * Returns the closest possible output size based on silicon limitations
 * detailed through the pipe structure.
 *
 * If the input can't be read, it'll return -EINVAL. Returns 0 on success.
 **/
static int isp_try_pipeline(struct device *dev,
			    struct isp_pipeline *pipe,
			    enum isp_interface_type sensor_isp_if)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct v4l2_pix_format *pix_input = &pipe->in_pix;
	struct v4l2_pix_format *pix_output = &pipe->out_pix;
	unsigned int wanted_width = pix_output->width;
	unsigned int wanted_height = pix_output->height;
	int ifmt;
	int rval;

	if ((pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10 ||
	     pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10DPCM8 ||
	     pix_input->pixelformat == V4L2_PIX_FMT_SRGGB10 ||
	     pix_input->pixelformat == V4L2_PIX_FMT_SBGGR10 ||
	     pix_input->pixelformat == V4L2_PIX_FMT_SGBRG10) &&
	    (pix_output->pixelformat == V4L2_PIX_FMT_YUYV ||
	     pix_output->pixelformat == V4L2_PIX_FMT_UYVY)) {
		if (pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10 ||
		    pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10DPCM8)
			pipe->ccdc_in = CCDC_RAW_GRBG;
		if (pix_input->pixelformat == V4L2_PIX_FMT_SRGGB10)
			pipe->ccdc_in = CCDC_RAW_RGGB;
		if (pix_input->pixelformat == V4L2_PIX_FMT_SBGGR10)
			pipe->ccdc_in = CCDC_RAW_BGGR;
		if (pix_input->pixelformat == V4L2_PIX_FMT_SGBRG10)
			pipe->ccdc_in = CCDC_RAW_GBRG;
		pipe->ccdc_out = CCDC_OTHERS_VP;
		pipe->prv.in.path = PRV_RAW_CCDC;
		if ((pix_output->width == 1280) &&
		    (pix_output->height == 720)) {
			pipe->modules = OMAP_ISP_PREVIEW |
					OMAP_ISP_CCDC;
			pipe->prv.out.path = PREVIEW_MEM;
		} else {
			pipe->modules = OMAP_ISP_PREVIEW |
					OMAP_ISP_RESIZER |
					OMAP_ISP_CCDC;
			if (isp->revision <= ISP_REVISION_2_0) {
				pipe->prv.out.path = PREVIEW_MEM;
				pipe->rsz.in.path = RSZ_MEM_YUV;
			} else {
				pipe->prv.out.path = PREVIEW_RSZ;
				pipe->rsz.in.path = RSZ_OTFLY_YUV;
			}
		}
	} else {
		pipe->modules = OMAP_ISP_CCDC;
		if (pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10 ||
		    pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10DPCM8 ||
		    pix_input->pixelformat == V4L2_PIX_FMT_SRGGB10 ||
		    pix_input->pixelformat == V4L2_PIX_FMT_SBGGR10 ||
		    pix_input->pixelformat == V4L2_PIX_FMT_SGBRG10) {
			pipe->ccdc_out = CCDC_OTHERS_VP_MEM;
			if (pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10 ||
			    pix_input->pixelformat == V4L2_PIX_FMT_SGRBG10DPCM8)
				pipe->ccdc_in = CCDC_RAW_GRBG;
			if (pix_input->pixelformat == V4L2_PIX_FMT_SRGGB10)
				pipe->ccdc_in = CCDC_RAW_RGGB;
			if (pix_input->pixelformat == V4L2_PIX_FMT_SBGGR10)
				pipe->ccdc_in = CCDC_RAW_BGGR;
			if (pix_input->pixelformat == V4L2_PIX_FMT_SGBRG10)
				pipe->ccdc_in = CCDC_RAW_GBRG;
		} else if (pix_input->pixelformat == V4L2_PIX_FMT_YUYV ||
			   pix_input->pixelformat == V4L2_PIX_FMT_UYVY) {
			pipe->ccdc_in = CCDC_YUV_SYNC;
			pipe->ccdc_out = CCDC_OTHERS_MEM;
		} else
			return -EINVAL;
	}



	if (sensor_isp_if == ISP_CSIA)
		pipe->modules |= OMAP_ISP_CSIARX;

	if (pipe->modules & OMAP_ISP_CSIARX) {
		pipe->csia_in_w = pix_input->width;
		pipe->csia_in_h = pix_input->height;
		rval = isp_csi2_try_pipeline(&isp->isp_csi2, pipe);
		if (rval) {
			dev_dbg(dev, "the dimensions %dx%d are not"
				" supported\n", pix_input->width,
				pix_input->height);
			return rval;
		}
		pix_output->width = pipe->csia_out_w_img;
		pix_output->height = pipe->csia_out_h;
		pix_output->bytesperline =
			pipe->csia_out_w * ISP_BYTES_PER_PIXEL;
	}

	if (pipe->modules & OMAP_ISP_CCDC) {
		if (pipe->modules & OMAP_ISP_CSIARX) {
			pipe->ccdc_in_w = pipe->csia_out_w_img;
			pipe->ccdc_in_h = pipe->csia_out_h;
		} else {
			pipe->ccdc_in_w = pix_input->width;
			pipe->ccdc_in_h = pix_input->height;
		}
		rval = ispccdc_try_pipeline(&isp->isp_ccdc, pipe);
		if (rval) {
			dev_dbg(dev, "the dimensions %dx%d are not"
				" supported\n", pix_input->width,
				pix_input->height);
			return rval;
		}
		pix_output->width = pipe->ccdc_out_w_img;
		pix_output->height = pipe->ccdc_out_h;
		pix_output->bytesperline =
			pipe->ccdc_out_w * ISP_BYTES_PER_PIXEL;
	}

	if (pipe->modules & OMAP_ISP_PREVIEW) {
		if (pipe->prv.in.path == PRV_RAW_MEM)
			pipe->prv.in.image.width = pipe->ccdc_out_w;
		else
			pipe->prv.in.image.width = pipe->ccdc_out_w_img;
		pipe->prv.in.image.height = pipe->ccdc_out_h;
		pipe->prv.in.image.pixelformat = pix_input->pixelformat;

		pipe->prv.in.crop.left = pipe->ccdc_in_h_st;
		pipe->prv.in.crop.top = pipe->ccdc_in_v_st;
		pipe->prv.in.crop.width = pipe->ccdc_out_w_img;
		pipe->prv.in.crop.height = pipe->ccdc_out_h;

		pipe->prv.out.image.width = wanted_width;
		pipe->prv.out.image.height = wanted_height;
		pipe->prv.out.image.pixelformat = pix_output->pixelformat;
		pipe->prv.out.crop = pipe->prv.in.crop;

		rval = isppreview_try_pipeline(&isp->isp_prev, &pipe->prv);
		if (rval) {
			dev_dbg(dev, "the dimensions %dx%d are not"
				" supported\n", pix_input->width,
				pix_input->height);
			return rval;
		}
		pix_output->width = pipe->prv.out.crop.width;
		pix_output->height = pipe->prv.out.crop.height;
		pix_output->bytesperline = pipe->prv.out.image.width *
					   ISP_BYTES_PER_PIXEL;
	}

	if (pipe->modules & OMAP_ISP_RESIZER) {
		pipe->rsz.out.image.width = wanted_width;
		pipe->rsz.out.image.height = wanted_height;

		if (pipe->rsz.in.path == RSZ_OTFLY_YUV) {
			pipe->rsz.in.image.width =
				pipe->prv.out.crop.width;
			pipe->rsz.in.image.height =
				pipe->prv.out.crop.height;
		} else {
			pipe->rsz.in.image.width = pipe->prv.out.image.width;
			pipe->rsz.in.image.height = pipe->prv.out.image.height;
		}
		pipe->rsz.in.image.pixelformat = pix_output->pixelformat;
		pipe->rsz.in.image.bytesperline = pix_output->bytesperline;

		pipe->rsz.in.crop.left = pipe->rsz.in.crop.top = 0;
		pipe->rsz.in.crop.width = pipe->prv.out.crop.width;
		pipe->rsz.in.crop.height = pipe->prv.out.crop.height;

		rval = ispresizer_try_pipeline(&isp->isp_res, &pipe->rsz);
		if (rval) {
			dev_dbg(dev, "The dimensions %dx%d are not"
				" supported\n", pix_input->width,
				pix_input->height);
			return rval;
		}

		pix_output->width = pipe->rsz.out.image.width;
		pix_output->height = pipe->rsz.out.image.height;
		pix_output->bytesperline = pipe->rsz.out.image.bytesperline;
	}

	pix_output->field = V4L2_FIELD_NONE;
	pix_output->sizeimage =
		PAGE_ALIGN(pix_output->bytesperline * pix_output->height);
	pix_output->priv = 0;

	for (ifmt = 0; ifmt < NUM_ISP_CAPTURE_FORMATS; ifmt++) {
		if (pix_output->pixelformat == isp_formats[ifmt].pixelformat)
			break;
	}
	if (ifmt == NUM_ISP_CAPTURE_FORMATS)
		pix_output->pixelformat = V4L2_PIX_FMT_YUYV;

	switch (pix_output->pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		pix_output->colorspace = V4L2_COLORSPACE_JPEG;
		break;
	default:
		pix_output->colorspace = V4L2_COLORSPACE_SRGB;
	}

	return 0;
}

/**
 * isp_s_pipeline - Configure internal ISP pipeline.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @pix_input: Pointer to pixel format to use as input in the ISP.
 * @pix_output: Pointer to pixel format to use as output in the ISP.
 *
 * Returns the closest possible output size based on silicon limitations.
 *
 * If the input can't be read, it'll return -EINVAL. Returns 0 on success.
 **/
static int isp_s_pipeline(struct device *dev,
			  struct v4l2_pix_format *pix_input,
			  struct v4l2_pix_format *pix_output,
			  enum isp_interface_type sensor_isp_if)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_pipeline pipe;
	int rval;

	isp_release_resources(dev);

	pipe.in_pix = *pix_input;
	pipe.out_pix = *pix_output;

	rval = isp_try_pipeline(dev, &pipe, sensor_isp_if);
	if (rval)
		return rval;

	ispccdc_request(&isp->isp_ccdc);
	ispccdc_s_pipeline(&isp->isp_ccdc, &pipe);

	if (pipe.modules & OMAP_ISP_PREVIEW) {
		isppreview_request(&isp->isp_prev);
		isppreview_s_pipeline(&isp->isp_prev, &pipe.prv);
	}

	if (pipe.modules & OMAP_ISP_RESIZER) {
		ispresizer_request(&isp->isp_res);
		ispresizer_s_pipeline(&isp->isp_res, &pipe.rsz);
	}

	isp->pipeline = pipe;
	*pix_input = isp->pipeline.in_pix;
	*pix_output = isp->pipeline.out_pix;

	return 0;
}

void isp_set_hs_vs(struct device *dev, int hs_vs)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_bufs *bufs = &isp->bufs;

	bufs->wait_hs_vs = hs_vs;
	return;
}
EXPORT_SYMBOL(isp_set_hs_vs);

/**
 * isp_vbq_sync - keep the video buffers coherent between cpu and isp
 *
 * The typical operation required here is Cache Invalidation across
 * the (user space) buffer address range. And this _must_ be done
 * at QBUF stage (and *only* at QBUF).
 *
 * We try to use optimal cache invalidation function:
 * - dmac_inv_range:
 *    - used when the number of pages are _low_.
 *    - it becomes quite slow as the number of pages increase.
 *       - for 648x492 viewfinder (150 pages) it takes 1.3 ms.
 *       - for 5 Mpix buffer (2491 pages) it takes between 25-50 ms.
 *
 * - flush_cache_all:
 *    - used when the number of pages are _high_.
 *    - time taken in the range of 500-900 us.
 *    - has a higher penalty but, as whole dcache + icache is invalidated
 **/
/**
 * FIXME: dmac_inv_range crashes randomly on the user space buffer
 *        address. Fall back to flush_cache_all for now.
 */
#define ISP_CACHE_FLUSH_PAGES_MAX       0

static int isp_vbq_sync(struct videobuf_buffer *vb)
{
	struct videobuf_dmabuf *dma = videobuf_to_dma(vb);

	if (!vb->baddr || !dma || !dma->nr_pages ||
	    dma->nr_pages > ISP_CACHE_FLUSH_PAGES_MAX)
		flush_cache_all();
	else {
		dmac_inv_range((void *)vb->baddr,
			       (void *)vb->baddr + vb->bsize);
		outer_inv_range(vb->baddr, vb->baddr + vb->bsize);
	}

	return 0;
}

/**
 * isp_csi2_eof - End-of-Frame handler for CSI2.
 *
 * Code taken from isp_vbq_done():RESZ_DONE [ISP.C]
 */
void isp_csi2_eof_done(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_bufs *bufs = &isp->bufs;

	isp_buf_process(dev, bufs);
}

/**
 * isp_buf_init - Initialize the internal buffer queue handling.
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_buf_init(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_bufs *bufs = &isp->bufs;
	int sg;

	isp_complete_reset = 1;
	bufs->queue = 0;
	bufs->done = 0;
	bufs->wait_hs_vs = isp->config->wait_hs_vs;
	for (sg = 0; sg < NUM_BUFS; sg++) {
		if (bufs->buf[sg].vb) {
			bufs->buf[sg].vb->state = VIDEOBUF_ERROR;
			bufs->buf[sg].complete(bufs->buf[sg].vb,
					       bufs->buf[sg].priv);
		}
		bufs->buf[sg].complete = NULL;
		bufs->buf[sg].vb = NULL;
		bufs->buf[sg].priv = NULL;
	}
}

/**
 * isp_buf_process - Do final handling when a buffer has been processed.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @bufs: Pointer to ISP buffer handling structure.
 *
 * Updates the pointers accordingly depending of the internal pipeline.
 **/
static void isp_buf_process(struct device *dev, struct isp_bufs *bufs)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_buf *buf;
	int last;

	if (ISP_BUFS_IS_EMPTY(bufs))
		return;

	if (CCDC_CAPTURE(isp)) {
		if (ispccdc_sbl_wait_idle(&isp->isp_ccdc, 1000)) {
			ispccdc_enable(&isp->isp_ccdc, 1);
			dev_info(dev, "ccdc won't become idle!\n");
			return;
		}
	}

	/* We had at least one buffer in queue. */
	buf = ISP_BUF_DONE(bufs);
	last = ISP_BUFS_IS_LAST(bufs);

	if (!last) {
		/* Set new buffer address. */
		isp_set_buf(dev, ISP_BUF_NEXT_DONE(bufs));
		if (CCDC_CAPTURE(isp))
			ispccdc_enable(&isp->isp_ccdc, 1);
	} else {
		/* Tell ISP not to write any of our buffers. */
		if (CCDC_CAPTURE(isp))
			isp_disable_interrupts(dev);
		if (isp->pipeline.modules == OMAP_ISP_CSIARX) {
			isp_csi2_ctx_config_enabled(&isp->isp_csi2, 0, false);
			isp_csi2_ctx_update(&isp->isp_csi2, 0, false);
			isp_csi2_irq_ctx_set(&isp->isp_csi2, false);
		}
	}

	/* Mark the current buffer as done. */
	ISP_BUF_MARK_DONE(bufs);

	DPRINTK_ISPCTRL(KERN_ALERT "%s: finish %d mmu %p\n", __func__,
			(bufs->done - 1 + NUM_BUFS) % NUM_BUFS,
			(bufs->buf+((bufs->done - 1 + NUM_BUFS)
				    % NUM_BUFS))->isp_addr);

	/*
	 * We want to dequeue a buffer from the video buffer
	 * queue. Let's do it!
	 */
	buf->vb->state = buf->vb_state;
	buf->complete(buf->vb, buf->priv);
	buf->vb = NULL;
}

/**
 * isp_buf_queue - Queue a buffer into the internal ISP queue list.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @vb: Pointer to video buffer to queue.
 * @complete: Pointer to function to call when buffer is completely processed.
 * @priv: Pointer to private paramemter to send to complete function.
 *
 * Always returns 0.
 **/
int isp_buf_queue(struct device *dev, struct videobuf_buffer *vb,
		  void (*complete)(struct videobuf_buffer *vb, void *priv),
		  void *priv)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	unsigned long flags;
	struct isp_buf *buf;
	struct videobuf_dmabuf *dma = videobuf_to_dma(vb);
	const struct scatterlist *sglist = dma->sglist;
	struct isp_bufs *bufs = &isp->bufs;
	int sglen = dma->sglen;

	if (isp->running != ISP_RUNNING) {
		vb->state = VIDEOBUF_ERROR;
		complete(vb, priv);

		return 0;
	}

	BUG_ON(sglen < 0 || !sglist);

	isp_vbq_sync(vb);

	spin_lock_irqsave(&isp->lock, flags);

	BUG_ON(ISP_BUFS_IS_FULL(bufs));

	buf = ISP_BUF_QUEUE(bufs);

	buf->isp_addr = bufs->isp_addr_capture[vb->i];
	buf->complete = complete;
	buf->vb = vb;
	buf->priv = priv;
	buf->vb_state = VIDEOBUF_DONE;
	buf->vb->state = VIDEOBUF_ACTIVE;

	if (ISP_BUFS_IS_EMPTY(bufs)) {
		/*
		 * We must wait for the HS_VS since before that the
		 * CCDC may trigger interrupts even if it's not
		 * receiving a frame.
		 */
		bufs->wait_hs_vs++;
		isp_enable_interrupts(dev);
		isp_set_buf(dev, buf);
		isp_af_try_enable(&isp->isp_af);
		isph3a_aewb_try_enable(&isp->isp_h3a);
		isp_hist_try_enable(&isp->isp_hist);
		/*
		 * We use memory Read chanel we need to start engines in
		 * different way
		 */
		if (isp->config->u.csi.use_mem_read) {
			bufs->wait_hs_vs = 0;
			/*
			 * In case of pipeline with temporary buffer, "Resizer"
			 * will be enabled when "Preview" is done.
			 */
			if (isp->revision > ISP_REVISION_2_0) {
				if (isp->pipeline.modules & OMAP_ISP_RESIZER) {
					ispresizer_config_shadow_registers(
						&isp->isp_res);
					ispresizer_enable(&isp->isp_res, 1);
				}
			}
			if (isp->pipeline.modules & OMAP_ISP_PREVIEW) {
				if (!isppreview_busy(&isp->isp_prev))
					isppreview_config_shadow_registers(
								&isp->isp_prev);
				isppreview_enable(&isp->isp_prev, 1);
			}

			if (isp->pipeline.modules & OMAP_ISP_CCDC)
					ispccdc_enable(&isp->isp_ccdc, 1);

			if (ispccdc_is_enabled(&isp->isp_ccdc)) {
				isp_adjust_bandwidth(dev);
				isp_csi_lcm_readport_enable(&isp->isp_csi, 1);
			}
		} else {
			if (isp->pipeline.modules & OMAP_ISP_CCDC)
				ispccdc_enable(&isp->isp_ccdc, 1);

		if (isp->pipeline.modules == OMAP_ISP_CSIARX) {
			isp_csi2_irq_ctx_set(&isp->isp_csi2, 1);
				isp_csi2_ctx_config_enabled(&isp->isp_csi2, 0,
							    true);
			isp_csi2_ctx_update(&isp->isp_csi2, 0, false);
			}
		}
	}

	ISP_BUF_MARK_QUEUED(bufs);

	spin_unlock_irqrestore(&isp->lock, flags);

	DPRINTK_ISPCTRL(KERN_ALERT "%s: queue %d vb %d, mmu %p\n", __func__,
			(bufs->queue - 1 + NUM_BUFS) % NUM_BUFS, vb->i,
			buf->isp_addr);

	return 0;
}
EXPORT_SYMBOL(isp_buf_queue);

/**
 * isp_vbq_setup - Do ISP specific actions when the VB wueue is set.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @vbq: Pointer to video buffer queue.
 * @cnt: Pointer to buffer count size of the queue list.
 * @size: Pointer to the bytesize of every video buffer queue entry.
 *
 * Currently, this just allocates the temporary buffer used for the
 * ISP Workaround when having CCDC->PRV->RSZ internal datapath.
 **/
int isp_vbq_setup(struct device *dev, struct videobuf_queue *vbq,
		  unsigned int *cnt, unsigned int *size)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	if (CCDC_PREV_RESZ_CAPTURE(isp) &&
		isp->revision <= ISP_REVISION_2_0)
		return isp_tmp_buf_alloc(dev, &isp->pipeline);

	return 0;
}
EXPORT_SYMBOL(isp_vbq_setup);

/**
 * ispmmu_vmap - Wrapper for Virtual memory mapping of a scatter gather list
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @sglist: Pointer to source Scatter gather list to allocate.
 * @sglen: Number of elements of the scatter-gatter list.
 *
 * Returns a resulting mapped device address by the ISP MMU, or -ENOMEM if
 * we ran out of memory.
 **/
dma_addr_t ispmmu_vmap(struct device *dev, const struct scatterlist *sglist,
		       int sglen)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int err;
	u32 da;
	struct sg_table *sgt;
	unsigned int i;
	struct scatterlist *sg, *src = (struct scatterlist *)sglist;

	/*
	 * convert isp sglist to iommu sgt
	 * FIXME: should be fixed in the upper layer?
	 */
	sgt = kmalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt)
		return -ENOMEM;
	err = sg_alloc_table(sgt, sglen, GFP_KERNEL);
	if (err)
		goto err_sg_alloc;

	for_each_sg(sgt->sgl, sg, sgt->nents, i)
		sg_set_buf(sg, phys_to_virt(sg_dma_address(src + i)),
			   sg_dma_len(src + i));

	da = iommu_vmap(isp->iommu, 0, sgt, IOMMU_FLAG);
	if (IS_ERR_VALUE(da))
		goto err_vmap;

	return (dma_addr_t)da;

err_vmap:
	sg_free_table(sgt);
err_sg_alloc:
	kfree(sgt);
	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(ispmmu_vmap);

/**
 * ispmmu_vunmap - Unmap a device address from the ISP MMU
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @da: Device address generated from a ispmmu_vmap call.
 **/
void ispmmu_vunmap(struct device *dev, dma_addr_t da)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct sg_table *sgt;

	sgt = iommu_vunmap(isp->iommu, (u32)da);
	if (!sgt)
		return;
	sg_free_table(sgt);
	kfree(sgt);
}
EXPORT_SYMBOL_GPL(ispmmu_vunmap);

/**
 * isp_vbq_prepare - Videobuffer queue prepare.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @vbq: Pointer to videobuf_queue structure.
 * @vb: Pointer to videobuf_buffer structure.
 * @field: Requested Field order for the videobuffer.
 *
 * Returns 0 if successful, or -EIO if the ispmmu was unable to map a
 * scatter-gather linked list data space.
 **/
int isp_vbq_prepare(struct device *dev, struct videobuf_queue *vbq,
		    struct videobuf_buffer *vb, enum v4l2_field field)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	unsigned int isp_addr;
	struct videobuf_dmabuf *vdma;
	struct isp_bufs *bufs = &isp->bufs;

	int err = 0;

	vdma = videobuf_to_dma(vb);

	isp_addr = ispmmu_vmap(dev, vdma->sglist, vdma->sglen);

	if (IS_ERR_VALUE(isp_addr))
		err = -EIO;
	else
		bufs->isp_addr_capture[vb->i] = isp_addr;

	return err;
}
EXPORT_SYMBOL(isp_vbq_prepare);

/**
 * isp_vbq_release - Videobuffer queue release.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @vbq: Pointer to videobuf_queue structure.
 * @vb: Pointer to videobuf_buffer structure.
 **/
void isp_vbq_release(struct device *dev, struct videobuf_queue *vbq,
		     struct videobuf_buffer *vb)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_bufs *bufs = &isp->bufs;

	ispmmu_vunmap(dev, bufs->isp_addr_capture[vb->i]);
	bufs->isp_addr_capture[vb->i] = (dma_addr_t)NULL;
	return;
}
EXPORT_SYMBOL(isp_vbq_release);

/**
 * isp_queryctrl - Query V4L2 control from existing controls in ISP.
 * @a: Pointer to v4l2_queryctrl structure. It only needs the id field filled.
 *
 * Returns 0 if successful, or -EINVAL if not found in ISP.
 **/
int isp_queryctrl(struct v4l2_queryctrl *a)
{
	int i;

	if (a->id & V4L2_CTRL_FLAG_NEXT_CTRL) {
		a->id &= ~V4L2_CTRL_FLAG_NEXT_CTRL;
		i = find_next_vctrl(a->id);
	} else {
		i = find_vctrl(a->id);
	}

	if (i < 0)
		return -EINVAL;

	*a = video_control[i].qc;
	return 0;
}
EXPORT_SYMBOL(isp_queryctrl);

/**
 * isp_queryctrl - Query V4L2 control from existing controls in ISP.
 * @a: Pointer to v4l2_queryctrl structure. It only needs the id field filled.
 *
 * Returns 0 if successful, or -EINVAL if not found in ISP.
 **/
int isp_querymenu(struct v4l2_querymenu *a)
{
	int i;

	i = find_vmenu(a->id, a->index);

	if (i < 0)
		return -EINVAL;

	*a = video_menu[i];
	return 0;
}
EXPORT_SYMBOL(isp_querymenu);

/**
 * isp_g_ctrl - Get value of the desired V4L2 control.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @a: V4L2 control to read actual value from.
 *
 * Return 0 if successful, or -EINVAL if chosen control is not found.
 **/
int isp_g_ctrl(struct device *dev, struct v4l2_control *a)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	u8 current_value;
	int rval = 0;

	if (!isp->ref_count)
		return -EINVAL;

	switch (a->id) {
	case V4L2_CID_BRIGHTNESS:
		isppreview_query_brightness(&isp->isp_prev, &current_value);
		a->value = current_value / ISPPRV_BRIGHT_UNITS;
		break;
	case V4L2_CID_CONTRAST:
		isppreview_query_contrast(&isp->isp_prev, &current_value);
		a->value = current_value / ISPPRV_CONTRAST_UNITS;
		break;
	case V4L2_CID_COLORFX:
		isppreview_get_color(&isp->isp_prev, &current_value);
		a->value = current_value;
		break;
	case V4L2_CID_PRIVATE_OMAP3ISP_CSI2MEM:
		a->value = isp->isp_csi2.force_mem_out ? 1 : 0;
		break;
	default:
		rval = -EINVAL;
		break;
	}

	return rval;
}
EXPORT_SYMBOL(isp_g_ctrl);

/**
 * isp_s_ctrl - Set value of the desired V4L2 control.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @a: V4L2 control to read actual value from.
 *
 * Return 0 if successful, -EINVAL if chosen control is not found or value
 * is out of bounds, -EFAULT if copy_from_user or copy_to_user operation fails
 * from camera abstraction layer related controls or the transfered user space
 * pointer via the value field is not set properly.
 **/
int isp_s_ctrl(struct device *dev, struct v4l2_control *a)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int rval = 0;
	u8 new_value = a->value;

	if (!isp->ref_count)
		return -EINVAL;

	switch (a->id) {
	case V4L2_CID_BRIGHTNESS:
		if (a->value > ISPPRV_BRIGHT_HIGH)
			rval = -EINVAL;
		else
			isppreview_update_brightness(&isp->isp_prev,
						     &new_value);
		break;
	case V4L2_CID_CONTRAST:
		if (a->value > ISPPRV_CONTRAST_HIGH)
			rval = -EINVAL;
		else
			isppreview_update_contrast(&isp->isp_prev, &new_value);
		break;
	case V4L2_CID_COLORFX:
		if (a->value > V4L2_COLORFX_SEPIA)
			rval = -EINVAL;
		else
			isppreview_set_color(&isp->isp_prev, &new_value);
		break;
	case V4L2_CID_PRIVATE_OMAP3ISP_CSI2MEM:
		/* NOTE: User must call again VIDIOC_S_FMT to make this
			 effective */
		isp->isp_csi2.force_mem_out = new_value ? true : false;
		break;
	default:
		rval = -EINVAL;
		break;
	}

	return rval;
}
EXPORT_SYMBOL(isp_s_ctrl);

/**
 * isp_handle_private - Handle all private ioctls for isp module.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @cmd: ioctl cmd value
 * @arg: ioctl arg value
 *
 * Return 0 if successful, -EINVAL if chosen cmd value is not handled or value
 * is out of bounds, -EFAULT if ioctl arg value is not valid.
 * Function simply routes the input ioctl cmd id to the appropriate handler in
 * the isp module.
 **/
int isp_handle_private(struct device *dev, struct mutex *vdev_mutex, int cmd,
		       void *arg)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int rval = 0;

	if (!isp->ref_count)
		return -EINVAL;

	switch (cmd) {
	case VIDIOC_PRIVATE_ISP_CCDC_CFG:
		mutex_lock(vdev_mutex);
		rval = ispccdc_config(&isp->isp_ccdc, arg);
		mutex_unlock(vdev_mutex);
		break;
	case VIDIOC_PRIVATE_ISP_PRV_CFG:
		mutex_lock(vdev_mutex);
		rval = isppreview_config(&isp->isp_prev, arg);
		mutex_unlock(vdev_mutex);
		break;
	case VIDIOC_PRIVATE_ISP_AEWB_CFG: {
		struct isph3a_aewb_config *params;
		params = (struct isph3a_aewb_config *)arg;

		mutex_lock(vdev_mutex);
		rval = isph3a_aewb_config(&isp->isp_h3a, params);
		mutex_unlock(vdev_mutex);
		if (rval)
			return rval;

		/* Check errata i421 */
		if (isp->isp_af.enabled && params && params->aewb_enable) {
			rval = isp_validate_errata_i421(dev, params,
					&isp->isp_af.config.paxel_config);
		}
	}
		break;
	case VIDIOC_PRIVATE_ISP_AEWB_REQ: {
		struct isph3a_aewb_data *data;
		data = (struct isph3a_aewb_data *)arg;
		rval = isph3a_aewb_request_statistics(&isp->isp_h3a, data);
	}
		break;
	case VIDIOC_PRIVATE_ISP_HIST_CFG: {
		struct isp_hist_config *params;
		params = (struct isp_hist_config *)arg;
		mutex_lock(vdev_mutex);
		rval = isp_hist_config(&isp->isp_hist, params);
		mutex_unlock(vdev_mutex);
	}
		break;
	case VIDIOC_PRIVATE_ISP_HIST_REQ: {
		struct isp_hist_data *data;
		data = (struct isp_hist_data *)arg;
		rval = isp_hist_request_statistics(&isp->isp_hist, data);
	}
		break;
	case VIDIOC_PRIVATE_ISP_AF_CFG: {
		struct af_configuration *params;
		params = (struct af_configuration *)arg;

		mutex_lock(vdev_mutex);
		rval = isp_af_config(&isp->isp_af, params);
		mutex_unlock(vdev_mutex);
		if (rval)
			return rval;

		/* Check errata i421 */
		if (isp->isp_h3a.enabled && params && params->af_config) {
			rval = isp_validate_errata_i421(dev,
					&isp->isp_h3a.aewb_config_local,
					&params->paxel_config);
		}
	}
		break;
	case VIDIOC_PRIVATE_ISP_AF_REQ: {
		struct isp_af_data *data;
		data = (struct isp_af_data *)arg;
		rval = isp_af_request_statistics(&isp->isp_af, data);
	}
		break;
	default:
		rval = -EINVAL;
		break;
	}
	return rval;
}
EXPORT_SYMBOL(isp_handle_private);

/**
 * isp_enum_fmt_cap - Get more information of chosen format index and type
 * @f: Pointer to structure containing index and type of format to read from.
 *
 * Returns 0 if successful, or -EINVAL if format index or format type is
 * invalid.
 **/
int isp_enum_fmt_cap(struct v4l2_fmtdesc *f)
{
	int index = f->index;
	enum v4l2_buf_type type = f->type;
	int rval = -EINVAL;

	if (index >= NUM_ISP_CAPTURE_FORMATS)
		goto err;

	memset(f, 0, sizeof(*f));
	f->index = index;
	f->type = type;

	switch (f->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		rval = 0;
		break;
	default:
		goto err;
	}

	f->flags = isp_formats[index].flags;
	strncpy(f->description, isp_formats[index].description,
		sizeof(f->description));
	f->pixelformat = isp_formats[index].pixelformat;
err:
	return rval;
}
EXPORT_SYMBOL(isp_enum_fmt_cap);

/**
 * isp_g_fmt_cap - Get current output image format.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @pix: Pointer to V4L2 format structure to return current output format
 **/
void isp_g_fmt_cap(struct device *dev, struct v4l2_pix_format *pix)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	*pix = isp->pipeline.out_pix;
	return;
}
EXPORT_SYMBOL(isp_g_fmt_cap);

/**
 * isp_s_fmt_cap - Set I/O formats and crop, and configure pipeline in ISP
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @pix_input: Pointer to V4L2 format structure to represent current input.
 * @pix_output: Pointer to V4L2 format structure to represent current output.
 *
 * Returns 0 if successful, -EINVAL if ISP hasn't been opened, or return
 * value of isp_s_pipeline if there is an error.
 **/
int isp_s_fmt_cap(struct device *dev, struct v4l2_pix_format *pix_input,
		  struct v4l2_pix_format *pix_output,
		  enum isp_interface_type sensor_isp_if)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	if (!isp->ref_count)
		return -EINVAL;

	return isp_s_pipeline(dev, pix_input, pix_output, sensor_isp_if);
}
EXPORT_SYMBOL(isp_s_fmt_cap);

/**
 * isp_get_buf_offset - Gets offset of start of crop.
 *
 * Returns the offset (in bytes) of the start of the crop rectangle.
 **/
unsigned long isp_get_buf_offset(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	return isp->tmp_buf_offset;
}
EXPORT_SYMBOL(isp_get_buf_offset);

/**
 * isp_g_bounds - Get bounds of resizer source engine.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @bounds: Pointer to V4L2 rect structure to be filled.
 *
 * Always returns 0.
 **/
int isp_g_bounds(struct device *dev, struct v4l2_rect *bounds)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	if (isp->pipeline.modules & OMAP_ISP_CCDC) {
		bounds->left = 0;
		bounds->top = 0;
		bounds->width = isp->pipeline.ccdc_out_w_img;
		bounds->height = isp->pipeline.ccdc_out_h;
	}

	if (isp->pipeline.modules & OMAP_ISP_PREVIEW)
		*bounds = isp->pipeline.prv.out.crop;

	return 0;
}
EXPORT_SYMBOL(isp_g_bounds);

/**
 * isp_g_crop - Get crop rectangle size and position.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @crop: Pointer to V4L2 crop structure to be filled.
 *
 * Always returns 0.
 **/
int isp_g_crop(struct device *dev, struct v4l2_crop *crop)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	memset(&crop->c, 0, sizeof(struct v4l2_rect));

	if (isp->pipeline.modules & OMAP_ISP_CCDC) {
		crop->c.left = 0;
		crop->c.top = 0;
		crop->c.width = isp->pipeline.ccdc_out_w_img;
		crop->c.height = isp->pipeline.ccdc_out_h;
	}

	if (isp->pipeline.modules & OMAP_ISP_PREVIEW)
		crop->c = isp->pipeline.prv.out.crop;

	if (isp->pipeline.modules & OMAP_ISP_RESIZER)
		crop->c = isp->pipeline.rsz.in.crop;

	return 0;
}
EXPORT_SYMBOL(isp_g_crop);

/**
 * isp_s_crop - Set crop rectangle size and position.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @a: Pointer to V4L2 crop structure with desired parameters.
 *
 * Always returns 0.
 *
 * FIXME: Hardcoded to configure always the resizer, which could not be always
 *        the case.
 **/
int isp_s_crop(struct device *dev, struct v4l2_crop *a)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int rval = 0;

	if (isp->pipeline.modules & OMAP_ISP_RESIZER)
		rval = ispresizer_config_crop(&isp->isp_res,
					      &isp->pipeline.rsz, a);
	else
		rval = isp_g_crop(dev, a);

	return rval;
}
EXPORT_SYMBOL(isp_s_crop);

/**
 * isp_try_fmt_cap - Try desired input/output image formats
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @pix_input: Pointer to V4L2 pixel format structure for input image.
 * @pix_output: Pointer to V4L2 pixel format structure for output image.
 *
 * Returns 0 if successful, or return value of either isp_try_size or
 * isp_try_fmt if there is an error.
 **/
int isp_try_fmt_cap(struct device *dev, struct v4l2_pix_format *pix_input,
		    struct v4l2_pix_format *pix_output,
		    enum isp_interface_type sensor_isp_if)
{
	struct isp_pipeline pipe;
	int rval;

	pipe.in_pix = *pix_input;
	pipe.out_pix = *pix_output;

	rval = isp_try_pipeline(dev, &pipe, sensor_isp_if);
	if (rval)
		return rval;

	*pix_input = pipe.in_pix;
	*pix_output = pipe.out_pix;

	return 0;
}
EXPORT_SYMBOL(isp_try_fmt_cap);

/**
 * isp_set_ccdc_vp_clock - Set adjust CCDC Video Port clock
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @ccdc_clk: Clock suitable for the current sensor mode.
 **/
void isp_set_ccdc_vp_clock(struct device *dev, u32 ccdc_clk)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	isp->ccdc_clk = ccdc_clk;

	return;
}
EXPORT_SYMBOL(isp_set_ccdc_vp_clock);

/**
 * isp_save_ctx - Saves ISP, CCDC, HIST, H3A, PREV, RESZ & MMU context.
 * @dev: Device pointer specific to the OMAP3 ISP.
 *
 * Routine for saving the context of each module in the ISP.
 * CCDC, HIST, H3A, PREV, RESZ and MMU.
 **/
static void isp_save_ctx(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	isp_save_context(dev, isp_reg_list);
	ispccdc_save_context(dev);
	if (isp->iommu)
		iommu_save_ctx(isp->iommu);
	isp_hist_save_context(dev);
	isph3a_save_context(dev);
	isppreview_save_context(dev);
	ispresizer_save_context(dev);
	ispcsi2_save_context(dev);
}

/**
 * isp_restore_ctx - Restores ISP, CCDC, HIST, H3A, PREV, RESZ & MMU context.
 * @dev: Device pointer specific to the OMAP3 ISP.
 *
 * Routine for restoring the context of each module in the ISP.
 * CCDC, HIST, H3A, PREV, RESZ and MMU.
 **/
static void isp_restore_ctx(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	isp_restore_context(dev, isp_reg_list);
	ispccdc_restore_context(dev);
	if (isp->iommu)
		iommu_restore_ctx(isp->iommu);
	isp_hist_restore_context(dev);
	isph3a_restore_context(dev);
	isppreview_restore_context(dev);
	ispresizer_restore_context(dev);
	ispcsi2_restore_context(dev);
}

/**
 * isp_enable_clocks - Enable ISP clocks
 * @dev: Device pointer specific to the OMAP3 ISP.
 *
 * Return 0 if successful, or clk_enable return value if any of tthem fails.
 **/
static int isp_enable_clocks(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int r;

	r = clk_enable(isp->cam_ick);
	if (r) {
		dev_err(dev, "clk_enable cam_ick failed\n");
		goto out_clk_enable_ick;
	}
	r = clk_enable(isp->csi2_fck);
	if (r) {
		dev_err(dev, "clk_enable csi2_fck failed\n");
		goto out_clk_enable_csi2_fclk;
	}
	return 0;

out_clk_enable_csi2_fclk:
	clk_disable(isp->cam_ick);
out_clk_enable_ick:
	return r;
}

int isp_enable_mclk(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	int r;
	unsigned long curr_mclk, curr_dpll4_m5, ratio;

	/* Check ratio between DPLL4_M5 and CAM_MCLK */
	curr_mclk = clk_get_rate(isp->cam_mclk);
	curr_dpll4_m5 = clk_get_rate(isp->dpll4_m5_ck);

	/* Protection for potential Zero division, or zero-ratio result */
	if (!curr_mclk || !curr_dpll4_m5)
		BUG();

	ratio = curr_mclk / curr_dpll4_m5;

	r = clk_set_rate(isp->dpll4_m5_ck, isp->mclk / ratio);
		if (r) {
			dev_err(dev, "clk_set_rate for dpll4_m5_ck failed\n");
			return r;
	}
	r = clk_enable(isp->cam_mclk);
	if (r) {
		dev_err(dev, "clk_enable cam_mclk failed\n");
		return r;
	}
	return 0;
}

void isp_disable_mclk(struct isp_device *isp)
{
	if (isp->cam_mclk->usecount != 0)
		clk_disable(isp->cam_mclk);
}

/**
 * isp_disable_clocks - Disable ISP clocks
 * @dev: Device pointer specific to the OMAP3 ISP.
 **/
static void isp_disable_clocks(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);

	clk_disable(isp->cam_ick);
	clk_disable(isp->csi2_fck);
}

/**
 * isp_get - Acquire the ISP resource.
 *
 * Initializes the clocks for the first acquire.
 *
 * Returns pointer for isp device structure.
 **/
struct device *isp_get(void)
{
	struct platform_device *pdev = omap3isp_pdev;
	struct isp_device *isp;
	static int has_context;
	int ret_err = 0;

	if (!pdev)
		return NULL;
	isp = platform_get_drvdata(pdev);

	DPRINTK_ISPCTRL("isp_get: old %d\n", isp->ref_count);
	mutex_lock(&(isp->isp_mutex));
	if (isp->ref_count == 0) {
		ret_err = isp_enable_clocks(&pdev->dev);
		if (ret_err)
			goto out_err;
		/* We don't want to restore context before saving it! */
		if (has_context)
			isp_restore_ctx(&pdev->dev);
		else
			has_context = 1;
		/* HACK: Allow multiple opens meanwhile a better solution is
		 *       found for the case of different devices sharing ISP
		 *       settings. */
/*	} else {
		mutex_unlock(&isp_obj.isp_mutex);
		return -EBUSY; */
	}
	isp->ref_count++;
	mutex_unlock(&(isp->isp_mutex));

	DPRINTK_ISPCTRL("isp_get: new %d\n", isp->ref_count);
	/* FIXME: ISP should register as v4l2 device to store its priv data */
	return &pdev->dev;

out_err:
	mutex_unlock(&(isp->isp_mutex));
	return NULL;
}
EXPORT_SYMBOL(isp_get);

/**
 * isp_put - Releases the ISP resource.
 *
 * Releases the clocks also for the last release.
 *
 * Return resulting reference count, or -EBUSY if ISP structure is not
 * allocated.
 **/
int isp_put(void)
{
	struct platform_device *pdev = omap3isp_pdev;
	struct isp_device *isp = platform_get_drvdata(pdev);

	if (!isp)
		return -EBUSY;

	DPRINTK_ISPCTRL("isp_put: old %d\n", isp->ref_count);
	mutex_lock(&(isp->isp_mutex));
	if (isp->ref_count) {
		if (--isp->ref_count == 0) {
			isp_save_ctx(&pdev->dev);
			if (isp->revision <= ISP_REVISION_2_0)
				isp_tmp_buf_free(&pdev->dev);
			isp_release_resources(&pdev->dev);
			isp_disable_clocks(&pdev->dev);
		}
	}
	mutex_unlock(&(isp->isp_mutex));
	DPRINTK_ISPCTRL("isp_put: new %d\n", isp->ref_count);
	return isp->ref_count;
}
EXPORT_SYMBOL(isp_put);

/**
 * isp_save_context - Saves the values of the ISP module registers.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @reg_list: Structure containing pairs of register address and value to
 *            modify on OMAP.
 **/
void isp_save_context(struct device *dev, struct isp_reg *reg_list)
{
	struct isp_reg *next = reg_list;

	for (; next->reg != ISP_TOK_TERM; next++)
		next->val = isp_reg_readl(dev, next->mmio_range, next->reg);
}

/**
 * isp_restore_context - Restores the values of the ISP module registers.
 * @dev: Device pointer specific to the OMAP3 ISP.
 * @reg_list: Structure containing pairs of register address and value to
 *            modify on OMAP.
 **/
void isp_restore_context(struct device *dev, struct isp_reg *reg_list)
{
	struct isp_reg *next = reg_list;

	for (; next->reg != ISP_TOK_TERM; next++)
		isp_reg_writel(dev, next->val, next->mmio_range, next->reg);
}

/**
 * isp_remove - Remove ISP platform device
 * @pdev: Pointer to ISP platform device
 *
 * Always returns 0.
 **/
static int isp_remove(struct platform_device *pdev)
{
	struct isp_device *isp = platform_get_drvdata(pdev);
	int i;

	if (!isp)
		return 0;
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	isp_dfs_shutdown();
#endif
	isp_csi2_cleanup(&pdev->dev);
	isp_csi_cleanup(&pdev->dev);
	isp_af_exit(&pdev->dev);
	isp_resizer_cleanup(&pdev->dev);
	isp_preview_cleanup(&pdev->dev);
	isp_get();
	if (isp->iommu)
		iommu_put(isp->iommu);
	isp_put();
	isph3a_aewb_cleanup(&pdev->dev);
	isp_hist_cleanup(&pdev->dev);
	isp_ccdc_cleanup(&pdev->dev);

	clk_put(isp->cam_ick);
	clk_put(isp->cam_mclk);
	clk_put(isp->dpll4_m5_ck);
	clk_put(isp->csi2_fck);
	clk_put(isp->l3_ick);

	free_irq(isp->irq_num, isp);

	for (i = 0; i <= OMAP3_ISP_IOMEM_CSI2PHY2; i++) {
		if (isp->mmio_base[i]) {
			iounmap((void *)isp->mmio_base[i]);
			isp->mmio_base[i] = 0;
		}

		if (isp->mmio_base_phys[i]) {
			release_mem_region(isp->mmio_base_phys[i],
					   isp->mmio_size[i]);
			isp->mmio_base_phys[i] = 0;
		}
	}

	omap3isp_pdev = NULL;

	kfree(isp);

	return 0;
}

#ifdef CONFIG_PM

/**
 * isp_suspend - Suspend routine for the ISP
 * @pdev: Pointer to Platform device
 * @state: New power state
 *
 * Always returns 0.
 **/
static int isp_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct isp_device *isp = platform_get_drvdata(pdev);
	int reset;

	DPRINTK_ISPCTRL("isp_suspend: starting\n");

	WARN_ON(mutex_is_locked(&isp->isp_mutex));

	if (isp->ref_count == 0)
		goto out;

	isp_disable_interrupts(&pdev->dev);
	reset = isp_suspend_modules(&pdev->dev);
	isp_save_ctx(&pdev->dev);
	if (reset)
		isp_reset(&pdev->dev);

	isp_disable_clocks(&pdev->dev);
	isp_disable_mclk(isp);

out:
	DPRINTK_ISPCTRL("isp_suspend: done\n");

	return 0;
}

/**
 * isp_resume - Resume routine for the ISP
 * @pdev: Pointer to platform device
 *
 * Returns 0 if successful, or isp_enable_clocks return value otherwise.
 **/
static int isp_resume(struct platform_device *pdev)
{
	struct isp_device *isp = platform_get_drvdata(pdev);
	int ret_err = 0;

	DPRINTK_ISPCTRL("isp_resume: starting\n");

	if (isp->ref_count == 0)
		goto out;

	ret_err = isp_enable_clocks(&pdev->dev);
	if (ret_err)
		goto out;
	ret_err = isp_enable_mclk(&pdev->dev);
	if (ret_err)
		goto out;
	isp_restore_ctx(&pdev->dev);
	isp_resume_modules(&pdev->dev);

out:
	DPRINTK_ISPCTRL("isp_resume: done \n");

	return ret_err;
}

#else

#define isp_suspend	NULL
#define isp_resume	NULL

#endif /* CONFIG_PM */

static u64 raw_dmamask = DMA_BIT_MASK(32);

/**
 * isp_probe - Probe ISP platform device
 * @pdev: Pointer to ISP platform device
 *
 * Returns 0 if successful,
 *   -ENOMEM if no memory available,
 *   -ENODEV if no platform device resources found
 *     or no space for remapping registers,
 *   -EINVAL if couldn't install ISR,
 *   or clk_get return error value.
 **/
static int isp_probe(struct platform_device *pdev)
{
	struct isp_device *isp;
	int ret_err = 0;
	int i;

	isp = kzalloc(sizeof(*isp), GFP_KERNEL);
	if (!isp) {
		dev_err(&pdev->dev, "could not allocate memory\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, isp);

	isp->dev = &pdev->dev;

	for (i = 0; i <= OMAP3_ISP_IOMEM_CSI2PHY2; i++) {
		struct resource *mem;
		/* request the mem region for the camera registers */
		mem = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!mem) {
			dev_err(isp->dev, "no mem resource?\n");
			ret_err = -ENODEV;
			goto out_free_mmio;
		}

		if (!request_mem_region(mem->start, mem->end - mem->start + 1,
					pdev->name)) {
			dev_err(isp->dev,
				"cannot reserve camera register I/O region\n");
			ret_err = -ENODEV;
			goto out_free_mmio;
		}
		isp->mmio_base_phys[i] = mem->start;
		isp->mmio_size[i] = mem->end - mem->start + 1;

		/* map the region */
		isp->mmio_base[i] = (unsigned long)
			ioremap_nocache(isp->mmio_base_phys[i],
					isp->mmio_size[i]);
		if (!isp->mmio_base[i]) {
			dev_err(isp->dev,
				"cannot map camera register I/O region\n");
			ret_err = -ENODEV;
			goto out_free_mmio;
		}
	}

	isp->irq_num = platform_get_irq(pdev, 0);
	if (isp->irq_num <= 0) {
		dev_err(isp->dev, "no irq for camera?\n");
		ret_err = -ENODEV;
		goto out_free_mmio;
	}

	isp->mclk = CM_CAM_MCLK_HZ / 2;

	isp->cam_ick = clk_get(&camera_dev, "cam_ick");
	if (IS_ERR(isp->cam_ick)) {
		dev_err(isp->dev, "clk_get cam_ick failed\n");
		ret_err = PTR_ERR(isp->cam_ick);
		goto out_free_mmio;
	}
	isp->cam_mclk = clk_get(&camera_dev, "cam_mclk");
	if (IS_ERR(isp->cam_mclk)) {
		dev_err(isp->dev, "clk_get cam_mclk failed\n");
		ret_err = PTR_ERR(isp->cam_mclk);
		goto out_clk_get_mclk;
	}
	isp->dpll4_m5_ck = clk_get(&camera_dev, "dpll4_m5_ck");
	if (IS_ERR(isp->dpll4_m5_ck)) {
		dev_err(isp->dev, "clk_get dpll4_m5_ck failed\n");
		ret_err = PTR_ERR(isp->dpll4_m5_ck);
		goto out_clk_get_dpll4_m5_ck;
	}
	isp->csi2_fck = clk_get(&camera_dev, "csi2_96m_fck");
	if (IS_ERR(isp->csi2_fck)) {
		dev_err(isp->dev, "clk_get csi2_96m_fck failed\n");
		ret_err = PTR_ERR(isp->csi2_fck);
		goto out_clk_get_csi2_fclk;
	}
	isp->l3_ick = clk_get(&camera_dev, "l3_ick");
	if (IS_ERR(isp->l3_ick)) {
		dev_err(isp->dev, "clk_get l3_ick failed\n");
		ret_err = PTR_ERR(isp->l3_ick);
		goto out_clk_get_l3_ick;
	}

	if (request_irq(isp->irq_num, isp_isr, IRQF_SHARED,
			"Omap 3 Camera ISP", pdev)) {
		dev_err(isp->dev, "could not install isr\n");
		ret_err = -EINVAL;
		goto out_request_irq;
	}

	isp->ref_count = 0;
	omap3isp_pdev = pdev;

	mutex_init(&(isp->isp_mutex));
	spin_lock_init(&isp->lock);
	spin_lock_init(&isp->h3a_lock);

	isp->dev->dma_mask = &raw_dmamask;
	isp->dev->coherent_dma_mask = DMA_BIT_MASK(32);

	isp_get();
	/* Get ISP revision */
	isp->revision = isp_reg_readl(isp->dev,
				      OMAP3_ISP_IOMEM_MAIN, ISP_REVISION);
	dev_info(isp->dev, "Revision %d.%d found\n",
		 (isp->revision & 0xF0) >> 4, isp->revision & 0xF);

	isp->iommu = iommu_get("isp");
	if (IS_ERR(isp->iommu)) {
		ret_err = PTR_ERR(isp->iommu);
		isp->iommu = NULL;
	}
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	isp_dfs_setup(isp);
#endif
	isp_put();
	if (!isp->iommu)
		goto out_iommu_get;

	isp_ccdc_init(&pdev->dev);
	isp_hist_init(&pdev->dev);
	isph3a_aewb_init(&pdev->dev);
	isp_preview_init(&pdev->dev);
	isp_resizer_init(&pdev->dev);
	isp_af_init(&pdev->dev);
	isp_csi_init(&pdev->dev);
	isp_csi2_init(&pdev->dev);

	isp_get();
	isp_power_settings(&pdev->dev, 1);
	isp_put();

	return 0;

out_iommu_get:
	free_irq(isp->irq_num, isp);
	omap3isp_pdev = NULL;
out_request_irq:
	clk_put(isp->l3_ick);
out_clk_get_l3_ick:
	clk_put(isp->csi2_fck);
out_clk_get_csi2_fclk:
	clk_put(isp->dpll4_m5_ck);
out_clk_get_dpll4_m5_ck:
	clk_put(isp->cam_mclk);
out_clk_get_mclk:
	clk_put(isp->cam_ick);
out_free_mmio:
	for (i = 0; i <= OMAP3_ISP_IOMEM_CSI2PHY2; i++) {
		if (isp->mmio_base[i]) {
			iounmap((void *)isp->mmio_base[i]);
			isp->mmio_base[i] = 0;
		}

		if (isp->mmio_base_phys[i]) {
			release_mem_region(isp->mmio_base_phys[i],
					   isp->mmio_size[i]);
			isp->mmio_base_phys[i] = 0;
		}
	}

	kfree(isp);
	return ret_err;
}

static struct platform_driver omap3isp_driver = {
	.probe = isp_probe,
	.remove = isp_remove,
	.suspend = isp_suspend,
	.resume = isp_resume,
	.driver = {
		.name = "omap3isp",
	},
};

/**
 * isp_init - ISP module initialization.
 **/
static int __init isp_init(void)
{
	return platform_driver_register(&omap3isp_driver);
}

/**
 * isp_cleanup - ISP module cleanup.
 **/
static void __exit isp_cleanup(void)
{
	platform_driver_unregister(&omap3isp_driver);
}

module_init(isp_init);
module_exit(isp_cleanup);

MODULE_AUTHOR("Texas Instruments");
MODULE_DESCRIPTION("ISP Control Module Library");
MODULE_LICENSE("GPL");
