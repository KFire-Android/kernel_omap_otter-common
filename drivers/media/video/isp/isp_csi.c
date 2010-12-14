/*
 * isp_csi.c
 *
 * Driver Library for ISP CSI1/CCP2 Control module in TI's OMAP3 Camera ISP
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

#include <media/v4l2-common.h>
#include <linux/delay.h>

#include "isp.h"
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
#include "ispcsi1_dfs.h"
#endif

/**
 * Constraints of memory read channel horizontal/vertical
 * framing register
 */
#define MIN_CCP2_LCM_HSIZE_SKIP		0
#define MIN_CCP2_LCM_HSIZE_COUNT	1
#define MAX_CCP2_LCM_HSIZE_COUNT	ISPCSI1_LCM_HSIZE_COUNT_MASK

#define MIN_CCP2_LCM_VSIZE_SKIP		0
#define MIN_CCP2_LCM_VSIZE_COUNT	1
#define MAX_CCP2_LCM_VSIZE_COUNT	ISPCSI1_LCM_VSIZE_COUNT_MASK

#define BIT_SET(var, shift, mask, val)		\
	do {					\
		var = (var & ~(mask << shift))	\
			| (val << shift);	\
	} while (0)

/**
 * isp_csi_if_enable - Enable CSI1/CCP2 interface.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @enable: Enable flag.
 **/
void isp_csi_if_enable(struct isp_csi_device *isp_csi, u8 enable)
{
	struct device *dev = to_device(isp_csi);

#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	struct isp_device *isp = to_isp_device(isp_csi);
	if (enable) {
		if (isp->dfs_csi1)
			ispcsi1_dfs_dump(isp);
	}
#endif
	isp_reg_and_or(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL,
		       ~ISPCSI1_CTRL_IF_EN,
		       enable ? ISPCSI1_CTRL_IF_EN : 0);

	isp_csi->if_enabled = enable ? true : false;
}

/**
 * isp_csi_reset - Reset CSI1/CCP2 interface.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @enable: Enable flag.
 **/
static void isp_csi_reset(struct isp_csi_device *isp_csi)
{
	struct device *dev = to_device(isp_csi);
	u32 i = 0, reg;

	isp_reg_or(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_SYSCONFIG,
		   ISPCSI1_SYSCONFIG_SOFT_RESET);
	while (!(isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_SYSSTATUS) &
		 ISPCSI1_SYSSTATUS_RESET_DONE)) {
		udelay(10);
		if (i++ > 10)
			break;
	}
	if (!(isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_SYSSTATUS) &
	      ISPCSI1_SYSSTATUS_RESET_DONE)) {
		dev_warn(dev,
		       "timeout waiting for csi reset\n");
	}

	reg = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2,
			    ISPCSI1_SYSCONFIG);
	reg &= ~ISPCSI1_SYSCONFIG_MSTANDBY_MODE_MASK;
	reg |= ISPCSI1_SYSCONFIG_MSTANDBY_MODE_NO;
	reg &= ~ISPCSI1_SYSCONFIG_AUTO_IDLE;
	isp_reg_writel(dev, reg, OMAP3_ISP_IOMEM_CCP2,
		       ISPCSI1_SYSCONFIG);
}

/**
 * isp_csi_set_vp_cfg - Configure CSI1/CCP2 Videport.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @vp_cfg: Pointer to videoport configuration structure.
 *
 * Returns 0 if successful, -EBUSY if the interface is enabled,
 * and -EINVAL if wrong parameters are passed.
 **/
static int isp_csi_set_vp_cfg(struct isp_csi_device *isp_csi,
			      struct isp_csi_vp_cfg *vp_cfg)
{
	struct device *dev = to_device(isp_csi);
	u32 val;

	if (isp_csi->if_enabled) {
		dev_err(dev, "CSI1/CCP2: VP cannot be configured when"
			     " interface is enabled.");
		return -EBUSY;
	}

	if (vp_cfg->write_polarity > 1) {
		dev_err(dev, "CSI1/CCP2: Wrong VP clock polarity value."
			     " Must be 0 or 1");
		return -EINVAL;
	}

	if (vp_cfg->divider > 3) {
		dev_err(dev, "CSI1/CCP2: Wrong divisor value. Must be"
			     " between 0 and 3");
		return -EINVAL;
	}

	val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL);
	/* Mask VP related fields */
	val &= ~(ISPCSI1_CTRL_VP_ONLY_EN |
		 (ISPCSI1_CTRL_VP_OUT_CTRL_MASK <<
		  ISPCSI1_CTRL_VP_OUT_CTRL_SHIFT) |
		 ISPCSI1_CTRL_VP_CLK_POL);

	if (vp_cfg->no_ocp)
		val |= ISPCSI1_CTRL_VP_ONLY_EN;

	val |= (vp_cfg->divider << ISPCSI1_CTRL_VP_OUT_CTRL_SHIFT);

	if (vp_cfg->write_polarity)
		val |= ISPCSI1_CTRL_VP_CLK_POL;

	isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL);

	return 0;
}

/**
 * isp_csi_set_vp_cfg - Configure CSI1/CCP2 Videport.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @vp_cfg: Pointer to videoport configuration structure.
 *
 * Returns 0 if successful, -EBUSY if the interface is enabled,
 * and -EINVAL if wrong parameters are passed.
 **/
int isp_csi_set_vp_freq(struct isp_csi_device *isp_csi, u16 vp_freq)
{
	struct device *dev = to_device(isp_csi);
	u32 val;

	val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL);

	val &= ~ISPCSI1_CTRL_FRACDIV_MASK;
	val |= (vp_freq << ISPCSI1_CTRL_FRACDIV_SHIFT);

	isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL);

	return 0;
}

/**
 * isp_csi_lcm_s_src_ofst - Configures the Read address line offset.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @offset: Line Offset for the input image.
 **/
int isp_csi_lcm_s_src_ofst(struct isp_csi_device *isp_csi, u32 offset)
{
	struct device *dev = to_device(isp_csi);

	if ((offset & ISP_32B_BOUNDARY_BUF) != offset) {
		dev_err(dev, "csi/ccp2: Offset should be in 32 byte "
		       "boundary\n");
		return -EINVAL;
	}

	isp_csi->lcm_src_ofst = offset;
	isp_reg_writel(dev, offset, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_SRC_OFST);
	return 0;
}

/**
 * isp_csi_lcm_s_src_addr - Sets memory address to read in LCM.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @addr: 32bit memory address aligned on 32byte boundary.
 *
 * Configures the memory address from which the input frame is to be read.
 **/
int isp_csi_lcm_s_src_addr(struct isp_csi_device *isp_csi, u32 addr)
{
	struct device *dev = to_device(isp_csi);

	if ((addr & ISP_32B_BOUNDARY_BUF) != addr) {
		dev_err(dev, "csi/ccp2: Address should be in 32 byte "
		       "boundary\n");
		return -EINVAL;
	}

	isp_csi->lcm_src_addr = addr;
	isp_reg_writel(dev, addr, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_SRC_ADDR);
	return 0;
}

/**
 * isp_csi_lcm_validate_src_region - Validates input image region to read.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @rect: v4l2 rectangle with demanded dimensions.
 *
 * Configures the region from which the input frame is to be read.
 **/
int isp_csi_lcm_validate_src_region(struct isp_csi_device *isp_csi,
				    struct v4l2_rect rect)
{
	struct device *dev = to_device(isp_csi);

	if ((rect.height < MIN_CCP2_LCM_VSIZE_COUNT) ||
	    (rect.height > MAX_CCP2_LCM_VSIZE_COUNT)) {
		dev_err(dev, "csi/ccp2: Invalid line count.\n");
		return -EINVAL;
	}
	if ((rect.top < MIN_CCP2_LCM_VSIZE_SKIP) ||
	    (rect.top >= rect.height)) {
		dev_err(dev, "csi/ccp2: Invalid start line.\n");
		return -EINVAL;
	}

	if ((rect.width < MIN_CCP2_LCM_HSIZE_COUNT) ||
	    (rect.width > MAX_CCP2_LCM_HSIZE_COUNT)) {
		dev_err(dev, "csi/ccp2: Invalid horizontal pixel count.\n");
		return -EINVAL;
	}

	if ((rect.left < MIN_CCP2_LCM_HSIZE_SKIP) ||
	    (rect.left >= rect.width)) {
		dev_err(dev, "csi/ccp2: Invalid horizontal start pixel.\n");
		return -EINVAL;
	}
	return 0;
}

/**
 * isp_csi_lcm_s_src_region - Sets input image region to read.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @rect: v4l2 rectangle with demanded dimensions.
 *
 * Configures the region from which the input frame is to be read.
 **/
int isp_csi_lcm_s_src_region(struct isp_csi_device *isp_csi,
			     struct v4l2_rect rect)
{
	struct device *dev = to_device(isp_csi);
	int ret = 0;
	u16 hwords;

	ret = isp_csi_lcm_validate_src_region(isp_csi, rect);
	if (ret)
		return ret;

	if (!isp_csi->lcm_src_addr || !isp_csi->lcm_src_ofst) {
		dev_err(dev, "csi/ccp2: Reading address and offset must be "
			     "specified first.\n");
		return -EINVAL;
	}

	isp_reg_writel(dev, isp_csi->lcm_src_addr +
			    (rect.top * isp_csi->lcm_src_ofst),
		       OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_SRC_ADDR);

	isp_reg_writel(dev,
		       (rect.left & ISPCSI1_LCM_HSIZE_SKIP_MASK) <<
			ISPCSI1_LCM_HSIZE_SKIP_SHIFT,
		       OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_HSIZE);

	isp_reg_writel(dev,
		       (rect.width & ISPCSI1_LCM_HSIZE_COUNT_MASK) <<
			ISPCSI1_LCM_HSIZE_COUNT_SHIFT,
		       OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_HSIZE);

	isp_reg_writel(dev,
		       (rect.height & ISPCSI1_LCM_VSIZE_COUNT_MASK) <<
			ISPCSI1_LCM_VSIZE_COUNT_SHIFT,
		       OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_VSIZE);

	/*
	 * Calculate hwords based on formula
	 * HWORDS = 4 x ceil( ((SKIP + COUNT) x bits_per_pixel)/(8 x 32))
	 */
	hwords = ((rect.left + rect.width) >> 2);
	isp_reg_writel(dev, (hwords &  ISPCSI1_LCM_PREFETCH_HWORDS_MASK) <<
			ISPCSI1_LCM_PREFETCH_HWORDS_SHIFT,
			OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_PREFETCH);

	isp_csi->lcm_src_rect = rect;
	return 0;
}

/**
 * isp_csi_lcm_s_src_fmt - Sets pixel format to read.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @format: Pixel format to read from memory.
 **/
int isp_csi_lcm_s_src_fmt(struct isp_csi_device *isp_csi, u32 format)
{
	struct device *dev = to_device(isp_csi);
	u32 format_val, format_decmp;
	u32 val;

	switch (format) {
	case V4L2_PIX_FMT_SGRBG10:
		format_val = 0x3;
		format_decmp = 0;
		break;
	case V4L2_PIX_FMT_SGRBG10DPCM8:
		format_val = 0x3;
		format_decmp = 0x2;
		break;
	default:
		dev_err(dev, "isp_csi_lcm_s_src_fmt: unsupported format\n");
		return -EINVAL;
	}

	isp_csi->lcm_src_fmt = format;

	val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL);
	val &= ~(ISPCSI1_LCM_CTRL_SRC_FORMAT_MASK <<
		 ISPCSI1_LCM_CTRL_SRC_FORMAT_SHIFT);
	val &= ~(ISPCSI1_LCM_CTRL_SRC_DECOMPR_MASK <<
		 ISPCSI1_LCM_CTRL_SRC_DECOMPR_SHIFT);

	val |= (format_val & ISPCSI1_LCM_CTRL_SRC_FORMAT_MASK) <<
	       ISPCSI1_LCM_CTRL_SRC_FORMAT_SHIFT;
	val |= (format_decmp & ISPCSI1_LCM_CTRL_SRC_DECOMPR_MASK) <<
	       ISPCSI1_LCM_CTRL_SRC_DECOMPR_SHIFT;
	isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL);
	return 0;
}

/**
 * isp_csi_lcm_s_dst_fmt - Sets pixel format to output.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @format: Pixel format to read from memory.
 **/
int isp_csi_lcm_s_dst_fmt(struct isp_csi_device *isp_csi, u32 format)
{
	struct device *dev = to_device(isp_csi);
	u32 format_val, format_decmp;
	u32 val;

	switch (format) {
	case V4L2_PIX_FMT_SGRBG10:
		format_val = 0x3;
		format_decmp = 0;
		break;
	case V4L2_PIX_FMT_SGRBG10DPCM8:
		format_val = 0x3;
		format_decmp = 0x2;
		break;
	default:
		dev_err(dev, "isp_csi_lcm_s_src_fmt: unsupported format\n");
		return -EINVAL;
	}

	isp_csi->lcm_dst_fmt = format;

	val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL);
	val &= ~(ISPCSI1_LCM_CTRL_DST_FORMAT_MASK <<
		 ISPCSI1_LCM_CTRL_DST_FORMAT_SHIFT);
	val &= ~(ISPCSI1_LCM_CTRL_DST_DECOMPR_MASK <<
		 ISPCSI1_LCM_CTRL_DST_DECOMPR_SHIFT);

	val |= (format_val & ISPCSI1_LCM_CTRL_DST_FORMAT_MASK) <<
	       ISPCSI1_LCM_CTRL_DST_FORMAT_SHIFT;
	val |= (format_decmp & ISPCSI1_LCM_CTRL_DST_DECOMPR_MASK) <<
	       ISPCSI1_LCM_CTRL_DST_DECOMPR_SHIFT;
	isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL);
	return 0;
}

int isp_csi_lcm_readport_enable(struct isp_csi_device *isp_csi, bool enable)
{
	struct device *dev = to_device(isp_csi);

	isp_reg_and_or(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL,
		       ~ISPCSI1_LCM_CTRL_CHAN_EN,
		       enable ? ISPCSI1_LCM_CTRL_CHAN_EN : 0);

	isp_csi->lcm_enabled = enable;

	return 0;
}

/**
 * isp_csi_configure_interface - Initialize CSI1/CCP2 interface.
 * @isp_csi: Pointer to ISP CSI/CCP2 device.
 * @config: Pointer to ISP CSI/CCP2 interface config structure.
 *
 * This will analize the parameters passed by the interface config
 * structure, and configure the respective registers for proper CSI1/CCP2
 * config.
 *
 * Returns -EINVAL if wrong format, -EIO if strobe is choosen in CSI1 mode, or
 * 0 on success.
 **/
int isp_csi_configure_interface(struct isp_csi_device *isp_csi,
				struct isp_csi_interface_cfg *config)
{
	struct device *dev = to_device(isp_csi);
	struct isp_csi_vp_cfg new_vp_cfg;
	u32 val, reg;
	int format;

	/* Reset the CSI and wait for reset to complete */
	isp_csi_reset(isp_csi);

	/* Configure Videoport */
	new_vp_cfg.no_ocp = false;
	new_vp_cfg.divider = config->vpclk;
	new_vp_cfg.write_polarity = 1;
	isp_csi_set_vp_cfg(isp_csi, &new_vp_cfg);

	val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL);
	/* I/O cell output is parallel
	 * (no effect, but errata says should be enabled for class 1/2)
	 */
	val |= ISPCSI1_CTRL_IO_OUT_SEL;

	/* Data/strobe physical layer */
	val &= ~(ISPCSI1_CTRL_PHY_SEL | ISPCSI1_CTRL_INV);
	if (config->signalling)
		val |= ISPCSI1_CTRL_PHY_SEL;
	if (config->strobe_clock_inv)
		val |= ISPCSI1_CTRL_INV;
	val |= ISPCSI1_CTRL_MODE_CCP2;

	if (config->use_mem_read) {

		/* Check if there's a buffer to read from */
		if (!isp_csi->lcm_src_addr || !isp_csi->lcm_src_ofst) {
			dev_err(dev, "isp_csi_configure_interface: Mem input"
				     " selected without specifying a read"
				     " buffer address/offset\n");
			return -EINVAL;
		}
		isp_csi_lcm_s_src_addr(isp_csi, isp_csi->lcm_src_addr);

		isp_csi_lcm_s_src_ofst(isp_csi, isp_csi->lcm_src_ofst);

		isp_csi_lcm_readport_enable(isp_csi, 0);

		isp_csi_if_enable(isp_csi, 0);

		isp_reg_and_or(dev, OMAP3_ISP_IOMEM_MAIN, ISP_CTRL,
			       ~((ISPCTRL_SBL_CBUFF0_BCF_CTRL_MASK <<
				 ISPCTRL_SBL_CBUFF0_BCF_CTRL_SHIFT) |
				 (ISPCTRL_SBL_CBUFF1_BCF_CTRL_MASK <<
				 ISPCTRL_SBL_CBUFF1_BCF_CTRL_SHIFT)),
				ISPCTRL_SBL_CBUFF1_BCF_CTRL_STALL_RESPONSE |
				ISPCTRL_SBL_CBUFF0_BCF_CTRL_STALL_RESPONSE);


		/* Set burst size to 32 x 64 bit bursts */
		isp_reg_and_or(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL,
			       ~(ISPCSI1_LCM_CTRL_BURST_SIZE_MASK <<
				 ISPCSI1_LCM_CTRL_BURST_SIZE_SHIFT),
			       (0x5 <<
				ISPCSI1_LCM_CTRL_BURST_SIZE_SHIFT));

		isp_reg_or(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL,
			   ISPCSI1_CTRL_FRAME);

		/* Mem Logical channel - Set format and generic configuration */
		isp_csi_lcm_s_src_fmt(isp_csi, config->format);

		isp_csi_lcm_s_dst_fmt(isp_csi, config->format);

		/* Make Receiver output to the videoport (to CCDC) */
		isp_reg_and(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_CTRL,
			    ~ISPCSI1_LCM_CTRL_DST_PORT_MEM);

		isp_csi_lcm_s_src_region(isp_csi, config->mem_src_rect);

		/* Clear status bits for Mem logical channel */
		isp_reg_writel(dev, ISPCSI1_LCM_IRQSTATUS_LCM_OCPERROR,
			       OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_IRQSTATUS);

		/* Enable IRQs for Mem logical channel */
		isp_reg_or(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_LCM_IRQENABLE,
			   ISPCSI1_LCM_IRQSTATUS_LCM_OCPERROR |
			   ISPCSI1_LCM_IRQSTATUS_LCM_EOF);
	} else {
		switch (config->format) {
		case V4L2_PIX_FMT_SGRBG10:
			format = 0x16;		/* RAW10+VP */
			break;
		case V4L2_PIX_FMT_SGRBG10DPCM8:
			format = 0x12;		/* RAW8+DPCM10+VP */
			break;
		default:
			dev_err(dev, "isp_csi_configure_interface: bad csi"
				     " format\n");
			return -EINVAL;
		}

		/* Logical channel #0 - Set format and generic configuration */
		reg = ISPCSI1_LCx_CTRL(0);
		val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, reg);
		BIT_SET(val, ISPCSI1_LCx_CTRL_FORMAT_SHIFT,
			ISPCSI1_LCx_CTRL_FORMAT_MASK, format);
		/* Enable setting of frame regions of interest */
		val |= ISPCSI1_LCx_CTRL_REGION_EN;
		val &= ~(ISPCSI1_LCx_CTRL_CRC_EN);
		if (config->crc)
			val |= ISPCSI1_LCx_CTRL_CRC_EN;
		isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, reg);

		/* Logical channel #0 - Set pixel data region */
		reg = ISPCSI1_LCx_DAT_START(0);
		val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, reg);
		BIT_SET(val, ISPCSI1_LCx_DAT_START_VERT_SHIFT,
			ISPCSI1_LCx_DAT_START_VERT_MASK, config->data_start);
		isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, reg);

		reg = ISPCSI1_LCx_DAT_SIZE(0);
		val = isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, reg);
		BIT_SET(val, ISPCSI1_LCx_DAT_SIZE_VERT_SHIFT,
			ISPCSI1_LCx_DAT_SIZE_VERT_MASK, config->data_size);
		isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2, reg);

		/* Clear status bits for logical channel #0 */
		val = ISPCSI1_LC01_IRQSTATUS_LC0_FIFO_OVF_IRQ |
		      ISPCSI1_LC01_IRQSTATUS_LC0_CRC_IRQ |
		      ISPCSI1_LC01_IRQSTATUS_LC0_FSP_IRQ |
		      ISPCSI1_LC01_IRQSTATUS_LC0_FW_IRQ |
		      ISPCSI1_LC01_IRQSTATUS_LC0_FSC_IRQ |
		      ISPCSI1_LC01_IRQSTATUS_LC0_SSC_IRQ;

		/* Clear IRQ status bits for logical channel #0 */
		isp_reg_writel(dev, val, OMAP3_ISP_IOMEM_CCP2,
			       ISPCSI1_LC01_IRQSTATUS);

		/* Enable IRQs for logical channel #0 */
		isp_reg_or(dev, OMAP3_ISP_IOMEM_CCP2,
			   ISPCSI1_LC01_IRQENABLE, val);

		/* Enable CSI1 */
		isp_csi_if_enable(isp_csi, 1);

		if (!(isp_reg_readl(dev, OMAP3_ISP_IOMEM_CCP2, ISPCSI1_CTRL) &
		      ISPCSI1_CTRL_MODE_CCP2)) {
			dev_warn(dev, "OMAP3 CSI1 bus not available\n");
			if (config->signalling) {
				/* Strobe mode requires CCP2 */
				return -EIO;
			}
		}
	}

	return 0;
}

/**
 * isp_csi_init - Routine for module driver init
 **/
int __init isp_csi_init(struct device *dev)
{
	struct isp_device *isp = dev_get_drvdata(dev);
	struct isp_csi_device *isp_csi = &isp->isp_csi;

	isp_csi->if_enabled = false;
	isp_csi->vp_cfg.no_ocp = false;
	isp_csi->vp_cfg.divider = 1;
	isp_csi->vp_cfg.write_polarity = 0;
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	ispcsi1_dfs_setup(isp);
#endif
	return 0;
}

/**
 * isp_csi_cleanup - Module Cleanup.
 **/
void isp_csi_cleanup(struct device *dev)
{
#ifdef CONFIG_VIDEO_OMAP34XX_ISP_DEBUG_FS
	struct isp_device *isp = dev_get_drvdata(dev);

	ispcsi1_dfs_shutdown(isp);
#endif
}
