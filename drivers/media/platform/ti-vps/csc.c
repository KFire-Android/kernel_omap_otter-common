/*
 * Color space converter library
 *
 * Copyright (c) 2013 Texas Instruments Inc.
 *
 * David Griego, <dagriego@biglakesoftware.com>
 * Dale Farnsworth, <dale@farnsworth.org>
 * Archit Taneja, <archit@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/videodev2.h>

#include "csc.h"

struct csc_coeffs {
	unsigned short	a0;
	unsigned short	b0;
	unsigned short	c0;
	unsigned short	a1;
	unsigned short	b1;
	unsigned short	c1;
	unsigned short	a2;
	unsigned short	b2;
	unsigned short	c2;
	unsigned short	d0;
	unsigned short	d1;
	unsigned short	d2;
};

/* consider this struct later to get custom CSC coefficients from userspace */
struct colorspace_coeffs {
	struct csc_coeffs   sd;
	struct csc_coeffs   hd;
};

/* support only YUV to RGB color space conversion for now */
#define	CSC_COEFFS_LIMITED_RANGE_Y2R	0
#define	CSC_COEFFS_FULL_RANGE_Y2R	1

/* default colorspace coefficients */
static struct colorspace_coeffs colorspace_coeffs[2] = {
	[CSC_COEFFS_LIMITED_RANGE_Y2R] = {
		{
			/* CSC_COEFF_SDTV_LIMITED_RANGE_Y2R */
			0x0400, 0x0000, 0x057D, 0x0400, 0x1EA7, 0x1D35,
			0x0400, 0x06EF, 0x1FFE, 0x0D40, 0x0210, 0x0C88,
		},
		{
			/* CSC_COEFF_HDTV_LIMITED_RANGE_Y2R */
			0x0400, 0x0000, 0x0629, 0x0400, 0x1F45, 0x1E2B,
			0x0400, 0x0742, 0x0000, 0x0CEC, 0x0148, 0x0C60,
		},
	},
	[CSC_COEFFS_FULL_RANGE_Y2R] = {
		{
			/* CSC_COEFF_SDTV_FULL_RANGE_Y2R */
			0x04A8, 0x1FFE, 0x0662, 0x04A8, 0x1E6F, 0x1CBF,
			0x04A8, 0x0812, 0x1FFF, 0x0C84, 0x0220, 0x0BAC,
		},
		{
			/* CSC_MODE_HDTV_FULL_RANGE_Y2R   */
			0x04A8, 0x0000, 0x072C, 0x04A8, 0x1F26, 0x1DDE,
			0x04A8, 0x0873, 0x0000, 0x0C20, 0x0134, 0x0B7C,
		},
	},
};

void csc_dump_regs(struct csc_data *csc)
{
	struct device *dev = &csc->pdev->dev;

	u32 read_reg(struct csc_data *csc, int offset)
	{
		return ioread32(csc->base + offset);
	}

#define DUMPREG(r) dev_dbg(dev, "%-35s %08x\n", #r, read_reg(csc, CSC_##r))

	DUMPREG(CSC00);
	DUMPREG(CSC01);
	DUMPREG(CSC02);
	DUMPREG(CSC03);
	DUMPREG(CSC04);
	DUMPREG(CSC05);

#undef DUMPREG
}

void csc_set_coeff_bypass(struct csc_data *csc, u32 *csc_reg5)
{
	*csc_reg5 |= CSC_BYPASS;
}

/*
 * set the color space converter coefficient shadow register values
 */
void csc_set_coeff(struct csc_data *csc, u32 *csc_reg0,
		enum v4l2_colorspace src_colorspace,
		enum v4l2_colorspace dst_colorspace)
{
	u32 *csc_reg5 = csc_reg0 + 5;
	u32 *shadow_csc = csc_reg0;
	struct colorspace_coeffs *sd_hd_coeffs;
	unsigned short *coeff, *end_coeff;
	/* use full range for now, a ctrl ioctl would be nice here */
	int sel = CSC_COEFFS_FULL_RANGE_Y2R;

	/*
	 * VPE requires CSC only if the output colorspace is RGB, this might
	 * change when VIP uses CSC
	 */
	if (dst_colorspace != V4L2_COLORSPACE_SRGB) {
		*csc_reg5 |= CSC_BYPASS;
		return;
	}

	sd_hd_coeffs = &colorspace_coeffs[sel];

	if (src_colorspace == V4L2_COLORSPACE_SMPTE170M)
		coeff = &sd_hd_coeffs->sd.a0;
	else
		coeff = &sd_hd_coeffs->hd.a0;

	end_coeff = coeff + sizeof(struct csc_coeffs) / sizeof(unsigned short);

	for ( ; coeff < end_coeff; coeff += 2)
		*shadow_csc++ = (*(coeff + 1) << 16) | *coeff;
}

struct csc_data *csc_create(struct platform_device *pdev)
{
	struct csc_data *csc;

	dev_dbg(&pdev->dev, "csc_create\n");

	csc = devm_kzalloc(&pdev->dev, sizeof(*csc), GFP_KERNEL);
	if (!csc) {
		dev_err(&pdev->dev, "couldn't alloc csc_data\n");
		return ERR_PTR(-ENOMEM);
	}

	csc->pdev = pdev;

	csc->res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"vpe_csc");
	if (csc->res == NULL) {
		dev_err(&pdev->dev, "missing platform resources data\n");
		return ERR_PTR(-ENODEV);
	}

	csc->base = devm_request_and_ioremap(&pdev->dev, csc->res);
	if (!csc->base) {
		dev_err(&pdev->dev, "failed to ioremap\n");
		return ERR_PTR(-ENOMEM);
	}

	return csc;
}
