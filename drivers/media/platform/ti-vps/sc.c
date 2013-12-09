/*
 * Scaler library
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

#include "sc.h"
#include "sc_coeff.h"

void sc_set_regs_bypass(struct sc_data *sc, u32 *sc_reg0)
{
	u32 val = 0;

	val |= CFG_SC_BYPASS;

	*sc_reg0 = val;
}

void sc_dump_regs(struct sc_data *sc)
{
	struct device *dev = &sc->pdev->dev;

	u32 read_reg(struct sc_data *sc, int offset)
	{
		return ioread32(sc->base + offset);
	}

#define DUMPREG(r) dev_dbg(dev, "%-35s %08x\n", #r, read_reg(sc, CFG_##r))

	DUMPREG(SC0);
	DUMPREG(SC1);
	DUMPREG(SC2);
	DUMPREG(SC3);
	DUMPREG(SC4);
	DUMPREG(SC5);
	DUMPREG(SC6);
	DUMPREG(SC8);
	DUMPREG(SC9);
	DUMPREG(SC10);
	DUMPREG(SC11);
	DUMPREG(SC12);
	DUMPREG(SC13);
	DUMPREG(SC17);
	DUMPREG(SC18);
	DUMPREG(SC19);
	DUMPREG(SC20);
	DUMPREG(SC21);
	DUMPREG(SC22);
	DUMPREG(SC23);
	DUMPREG(SC24);
	DUMPREG(SC25);

#undef DUMPREG
}

/*
 * set the horizontal scaler coefficients according to the ratio of output to
 * input widths, after accounting for up to two levels of decimation
 */
void sc_set_hs_coeffs(struct sc_data *sc, void *addr, unsigned int src_w,
		unsigned int dst_w)
{
	int sixteenths;
	int idx;
	int i, j;
	u16 *coeff_h = addr;
	u16 *blk_cp;
	const u16 *cp;

	if (dst_w > src_w) {
		idx = HS_UP_SCALE;
	} else {
		if ((dst_w << 1) < src_w)
			dst_w <<= 1;	/* first level decimation */
		if ((dst_w << 1) < src_w)
			dst_w <<= 1;	/* second level decimation */

		if (dst_w == src_w) {
			idx = HS_LE_16_16_SCALE;
		} else {
			sixteenths = (dst_w << 4) / src_w;
			if (sixteenths < 8)
				sixteenths = 8;
			idx = HS_LT_9_16_SCALE + sixteenths - 8;
		}
	}

	if (idx == sc->hs_index)
		return;

	cp = scaler_hs_coeffs[idx];
	blk_cp = coeff_h;

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 7; j++)
			*blk_cp++ = *cp++;

		blk_cp++;
	}

	sc->hs_index = idx;

	sc->load_coeff_h = true;
}

/*
 * set the vertical scaler coefficients according to the ratio of output to
 * input heights
 */
void sc_set_vs_coeffs(struct sc_data *sc, void *addr, unsigned int src_h,
		unsigned int dst_h)
{
	int sixteenths;
	int idx;
	int i, j;
	u16 *coeff_v = addr;
	u16 *blk_cp;
	const u16 *cp;

	if (dst_h > src_h) {
		idx = VS_UP_SCALE;
	} else if (dst_h == src_h) {
		idx = VS_1_TO_1_SCALE;
	} else {
		sixteenths = (dst_h << 4) / src_h;
		if (sixteenths < 8)
			sixteenths = 8;
		idx = VS_LT_9_16_SCALE + sixteenths - 8;
	}

	if (idx == sc->vs_index)
		return;

	cp = scaler_vs_coeffs[idx];
	blk_cp = coeff_v;

	for (i = 0; i < 64; i++) {
		for (j = 0; j < 5; j++)
			*blk_cp++ = *cp++;

		blk_cp += 3;
	}

	sc->vs_index = idx;
	sc->load_coeff_v = true;
}

struct sc_data *sc_create(struct platform_device *pdev)
{
	struct sc_data *sc;

	dev_dbg(&pdev->dev, "sc_create\n");

	sc = devm_kzalloc(&pdev->dev, sizeof(*sc), GFP_KERNEL);
	if (!sc) {
		dev_err(&pdev->dev, "couldn't alloc sc_data\n");
		return ERR_PTR(-ENOMEM);
	}

	sc->pdev = pdev;

	sc->res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "vpe_sc");
	if (sc->res == NULL) {
		dev_err(&pdev->dev, "missing platform resources data\n");
		return ERR_PTR(-ENODEV);
	}

	sc->base = devm_request_and_ioremap(&pdev->dev, sc->res);
	if (!sc->base) {
		dev_err(&pdev->dev, "failed to ioremap\n");
		return ERR_PTR(-ENOMEM);
	}

	return sc;
}
