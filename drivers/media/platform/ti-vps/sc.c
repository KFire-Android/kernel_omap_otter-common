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

void sc_config_scaler(struct sc_data *sc, u32 *sc_reg0, u32 *sc_reg8,
		u32 *sc_reg17, unsigned int src_w, unsigned int src_h,
		unsigned int dst_w, unsigned int dst_h)
{
	struct device *dev = &sc->pdev->dev;
	u32 val = 0;
	int dcm_x, dcm_shift;
	bool use_rav;
	unsigned long lltmp;
	u32 lin_acc_inc, lin_acc_inc_u;
	u32 col_acc_offset;
	u16 factor = 0;
	int row_acc_init_rav = 0, row_acc_init_rav_b = 0;
	u32 row_acc_inc = 0, row_acc_offset = 0, row_acc_offset_b = 0;
	u32 *sc_reg1 = sc_reg0 + 1;
	u32 *sc_reg2 = sc_reg1 + 1;
	u32 *sc_reg3 = sc_reg2 + 1;
	u32 *sc_reg4 = sc_reg3 + 1;
	u32 *sc_reg5 = sc_reg4 + 1;
	u32 *sc_reg6 = sc_reg5 + 1;
	u32 *sc_reg9 = sc_reg8 + 1;
	u32 *sc_reg12 = sc_reg9 + 3;
	u32 *sc_reg13 = sc_reg12 + 1;
	u32 *sc_reg24 = sc_reg17 + 7;

	/* clear all the features(they may get enabled elsewhere later) */
	val &= ~(CFG_SELFGEN_FID | CFG_TRIM | CFG_ENABLE_SIN2_VER_INTP |
		CFG_INTERLACE_I | CFG_DCM_4X | CFG_DCM_2X | CFG_AUTO_HS |
		CFG_ENABLE_EV | CFG_USE_RAV | CFG_INVT_FID | CFG_SC_BYPASS |
		CFG_INTERLACE_O | CFG_Y_PK_EN | CFG_HP_BYPASS);

	if (src_w == dst_w && src_h == dst_h) {
		val |= CFG_SC_BYPASS;
		*sc_reg0 = val;
		return;
	}

	/* we only support linear scaling for now */
	val |= CFG_LINEAR;

	/* configure horizontal scaler */

	/* enable 2X or 4X decimation */
	dcm_x = src_w / dst_w;
	if (dcm_x > 4) {
		val |= CFG_DCM_4X;
		dcm_shift = 2;
	} else if (dcm_x > 2) {
		val |= CFG_DCM_2X;
		dcm_shift = 1;
	} else {
		dcm_shift = 0;
	}

	lltmp = dst_w - 1;
	lin_acc_inc = div64_u64(((u64)(src_w >> dcm_shift) - 1) << 24, lltmp);
	lin_acc_inc_u = 0;
	col_acc_offset = 0;

	dev_dbg(dev, "hs config: src_w = %d, dst_w = %d, decimation = %s, lin_acc_inc = %08x\n",
		src_w, dst_w,
		dcm_shift == 2 ? "4x" : (dcm_shift == 1 ? "2x" : "none"),
		lin_acc_inc);

	/* configure vertical scaler */

	/* use RAV for vertical scaler if vertical downscaling is > 4x */
	if (dst_h < (src_h >> 2)) {
		use_rav = true;
		val |= CFG_USE_RAV;
	} else {
		use_rav = false;
	}

	if (use_rav) {
		/* use RAV */
		factor = (u16) ((dst_h << 10) / src_h);

		row_acc_init_rav = factor + ((1 + factor) >> 1);
		if (row_acc_init_rav >= 1024)
			row_acc_init_rav -= 1024;

		row_acc_init_rav_b = row_acc_init_rav +
				(1 + (row_acc_init_rav >> 1)) -
				(1024 >> 1);

		if (row_acc_init_rav_b < 0) {
			row_acc_init_rav_b += row_acc_init_rav;
			row_acc_init_rav *= 2;
		}

	} else {
		/* use polyphase */
		row_acc_inc = ((src_h - 1) << 16) / (dst_h - 1);
		row_acc_offset = 0;
		row_acc_offset_b = 0;
	}

	dev_dbg(dev, "vs config: src_h = %d, dst_h = %d, use_rav = %d factor = %d, row_acc_init = %08x, row_acc_init_b = %08x row_acc_inc %08x\n",
		src_h, dst_h, use_rav, factor,
		row_acc_init_rav, row_acc_init_rav_b, row_acc_inc);

	*sc_reg0 = val;
	*sc_reg1 = row_acc_inc;
	*sc_reg2 = row_acc_offset;
	*sc_reg3 = row_acc_offset_b;

	*sc_reg4 = ((lin_acc_inc_u & CFG_LIN_ACC_INC_U_MASK) <<
			CFG_LIN_ACC_INC_U_SHIFT) | (dst_w << CFG_TAR_W_SHIFT) |
			(dst_h << CFG_TAR_H_SHIFT);

	*sc_reg5 = (src_w << CFG_SRC_W_SHIFT) | (src_h << CFG_SRC_H_SHIFT);

	*sc_reg6 = (row_acc_init_rav_b << CFG_ROW_ACC_INIT_RAV_B_SHIFT) |
		(row_acc_init_rav << CFG_ROW_ACC_INIT_RAV_SHIFT);

	*sc_reg9 = lin_acc_inc;

	*sc_reg12 = col_acc_offset << CFG_COL_ACC_OFFSET_SHIFT;

	*sc_reg13 = factor;

	*sc_reg24 = (src_w << CFG_ORG_W_SHIFT) | (src_h << CFG_ORG_H_SHIFT);
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
