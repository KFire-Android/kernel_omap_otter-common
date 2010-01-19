/*
 * dmm_ll_drv.c
 *
 * DMM driver support functions for TI OMAP processors.
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/io.h>

/* #include "dmm_def.h" */
/* #include "dmm_2d_alloc.h" */
/* #include "dmm_prv.h" */
#include "dmm.h"
#include "dmm_reg.h"
#include "../tiler/tiler_def.h"

#if 0
#define regdump(x, y) printk(KERN_NOTICE "%s()::%d:%s=(0x%08x)\n", \
				__func__, __LINE__, x, (s32)y);
#else
#define regdump(x, y)
#endif

static void pat_ctrl_set(struct pat_ctrl *ctrl, s8 id)
{
	void __iomem *reg = NULL;
	u32 reg_val = 0x0;
	u32 new_val = 0x0;
	u32 bit_field = 0x0;
	u32 field_pos = 0x0;

	/* set DMM_PAT_CTRL register */
	/* TODO: casting as u32 */

	reg = (void __iomem *)(
			(u32)dmm_base |
			(u32)DMM_PAT_CTRL__0);
	reg_val = __raw_readl(reg);

	bit_field = BITFIELD(31, 28);
	field_pos = 28;
	new_val = (reg_val & (~(bit_field))) |
			((((u32)ctrl->initiator) <<
							field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(16, 16);
	field_pos = 16;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->sync) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(9, 8);
	field_pos = 8;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->lut_id) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(6, 4);
	field_pos = 4;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->direction) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	reg_val = __raw_readl(reg);
	bit_field = BITFIELD(0, 0);
	field_pos = 0;
	new_val = (reg_val & (~(bit_field))) |
		((((u32)ctrl->start) << field_pos) & bit_field);
	__raw_writel(new_val, reg);

	dsb();
}

/* ========================================================================== */
/**
 *  dmm_pat_refill_area_status_get()
 *
 * @brief  Gets the status for the selected PAT area.
 *
 * @param dmmPatAreaSel - signed long - [in] Selects which PAT area status will
 *  be queried.
 *
 * @param areaStatus - dmmPATStatusT* - [out] Structure containing the PAT area
 * status that will be filled by dmmPatRefillAreaStatusGet().
 *
 * @return errorCodeT
 *
 * @pre There is no pre conditions.
 *
 * @post If the query fails the provided areaStatus structure is not updated
 *  at all!
 *
 * @see errorCodeT, dmmPATStatusT for further detail.
 */
/* ========================================================================== */
enum errorCodeT dmm_pat_refill_area_status_get(signed long dmmPatAreaStatSel,
		struct dmmPATStatusT *areaStatus)
{
	enum errorCodeT eCode = DMM_NO_ERROR;

	u32 stat = 0xffffffff;
	void __iomem *statreg = (void __iomem *)
				((u32)dmm_base | 0x4c0ul);

	if (dmmPatAreaStatSel >= 0 && dmmPatAreaStatSel <= 3) {
		stat = __raw_readl(statreg);

		areaStatus->remainingLinesCounter = stat & BITFIELD(24, 16);
		areaStatus->error = (enum dmmPATStatusErrT)
				    (stat & BITFIELD(15, 10));
		areaStatus->linkedReconfig = stat & BITFIELD(4, 4);
		areaStatus->done = stat & BITFIELD(3, 3);
		areaStatus->engineRunning = stat & BITFIELD(2, 2);
		areaStatus->validDescriptor = stat & BITFIELD(1, 1);
		areaStatus->ready = stat & BITFIELD(0, 0);

	} else {
		eCode = DMM_WRONG_PARAM;
		printk(KERN_ALERT "wrong parameter\n");
	}

	return eCode;
}

/* ========================================================================== */
/**
 *  dmm_pat_refill()
 *
 * @brief  Initiate a PAT area refill (or terminate an ongoing - consult
 * documentation).
 *
 * @param patDesc - PATDescrT* - [in] Pointer to a PAT area descriptor that's
 * needed to extract settings from for the refill procedure initation.
 *
 * @param dmmPatAreaSel - signed long - [in] Selects which PAT area will be
 *  configured for a area refill procedure.
 *
 * @param refillType - dmmPATRefillMethodT - [in] Selects the refill method -
 * manual or automatic.
 *
 * @param forcedRefill - int - [in] Selects if forced refill should be used
 * effectively terminating any ongoing area refills related to the selected area
 *
 * @return errorCodeT
 *
 * @pre If forced mode is not used, no refills should be ongoing for the
 *  selected area - error status returned if this occurs.
 *
 * @post If non valid data is provided for patDesc and the refill engines fail
 * to perform the request, an error status is returned.
 *
 * @see errorCodeT,  PATDescrT, dmmPATRefillMethodT, s32 for further detail
 */
/* ========================================================================== */
s32 dmm_pat_refill(struct pat *patDesc, s32 dmmPatAreaSel,
			enum pat_mode refillType, s32 forced_refill)
{
	enum errorCodeT eCode = DMM_NO_ERROR;
	void __iomem *reg = NULL;
	u32 regval = 0xffffffff;
	u32 writeval = 0xffffffff;
	u32 f = 0xffffffff; /* field */
	u32 fp = 0xffffffff; /* field position */
	struct pat pat_desc = {0};

	struct dmmPATStatusT areaStat;
	if (forced_refill == 0) {
		eCode = dmm_pat_refill_area_status_get(
				dmmPatAreaSel, &areaStat);

		if (eCode == DMM_NO_ERROR) {
			if (areaStat.ready == 0 || areaStat.engineRunning) {
				printk(KERN_ALERT "hw not ready\n");
				eCode = DMM_HRDW_NOT_READY;
			}
		}
	}

	if (dmmPatAreaSel < 0 || dmmPatAreaSel > 3) {
		eCode = DMM_WRONG_PARAM;
	} else if (eCode == DMM_NO_ERROR) {
		if (refillType == AUTO) {
			reg = (void __iomem *)
				((u32)dmm_base |
				(0x500ul + 0x0));
			regval = __raw_readl(reg);
			f  = BITFIELD(31, 4);
			fp = 4;
			writeval = (regval & (~(f))) |
				   ((((u32)patDesc) << fp) & f);
			__raw_writel(writeval, reg);
		} else if (refillType == MANUAL) {
			/* check that the DMM_PAT_STATUS register has not
			 * reported an error.
			 */
			reg = (void __iomem *)(
					(u32)dmm_base |
						(u32)DMM_PAT_STATUS__0);
			regval = __raw_readl(reg);
			if ((regval & 0xFC00) != 0) {
				while (1) {
					printk(KERN_ERR "%s()::%d: ERROR\n",
						__func__, __LINE__);
				}
			}

			/* set DESC register to NULL */
			reg = (void __iomem *)
				((u32)dmm_base |
				(0x500ul + 0x0));
			regval = __raw_readl(reg);
			f  = BITFIELD(31, 4);
			fp = 4;
			writeval = (regval & (~(f))) |
				   ((((u32)NULL) << fp) & f);
			__raw_writel(writeval, reg);
			reg = (void __iomem *)
				((u32)dmm_base |
				(0x500ul + 0x4));
			regval = __raw_readl(reg);

			/* set area to be refilled */
			f  = BITFIELD(30, 24);
			fp = 24;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.y1) << fp) & f);
			__raw_writel(writeval, reg);

			regval = __raw_readl(reg);
			f  = BITFIELD(23, 16);
			fp = 16;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.x1) << fp) & f);
			__raw_writel(writeval, reg);

			regval = __raw_readl(reg);
			f  = BITFIELD(14, 8);
			fp = 8;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.y0) << fp) & f);
			__raw_writel(writeval, reg);

			regval = __raw_readl(reg);
			f  = BITFIELD(7, 0);
			fp = 0;
			writeval = (regval & (~(f))) |
				   ((((char)patDesc->area.x0) << fp) & f);
			__raw_writel(writeval, reg);

			printk(KERN_NOTICE
				"\nx0=(%d),y0=(%d),x1=(%d),y1=(%d)\n",
				(char)patDesc->area.x0,
				(char)patDesc->area.y0,
				(char)patDesc->area.x1,
				(char)patDesc->area.y1);
			dsb();

			/* First, clear the DMM_PAT_IRQSTATUS register */
			reg = (void __iomem *)(
					(u32)dmm_base |
					(u32)DMM_PAT_IRQSTATUS);
			__raw_writel(0xFFFFFFFF, reg);
			dsb();

			reg = (void __iomem *)(
					(u32)dmm_base |
					(u32)DMM_PAT_IRQSTATUS_RAW);
			regval = 0xFFFFFFFF;

			while (regval != 0x0) {
				regval = __raw_readl(reg);
				regdump("DMM_PAT_IRQSTATUS_RAW", regval);
			}

			/* fill data register */
			reg = (void __iomem *)
			((u32)dmm_base | (0x500ul + 0xc));
			regval = __raw_readl(reg);

			/* Apply 4 bit lft shft to counter the 4 bit rt shft */
			f  = BITFIELD(31, 4);
			fp = 4;
			writeval = (regval & (~(f))) | ((((u32)
					(patDesc->data >> 4)) << fp) & f);
			__raw_writel(writeval, reg);
			dsb();

			/* read back PAT_DATA__0 to see if it got set */
			regval = 0x0;
			while (regval != patDesc->data) {
				regval = __raw_readl(reg);
				regdump("DMM_PAT_DATA__0", regval);
			}

			pat_desc.ctrl.start = 1;
			pat_desc.ctrl.direction = 0;
			pat_desc.ctrl.lut_id = 0;
			pat_desc.ctrl.sync = 0;
			pat_desc.ctrl.initiator = 0;
			pat_ctrl_set(&pat_desc.ctrl, 0);

			/* Now, check if PAT_IRQSTATUS_RAW has been set after
			the PAT has been refilled */
			reg = (void __iomem *)(
					(u32)dmm_base |
					(u32)DMM_PAT_IRQSTATUS_RAW);
			regval = 0x0;
			while ((regval & 0x3) != 0x3) {
				regval = __raw_readl(reg);
				regdump("DMM_PAT_IRQSTATUS_RAW", regval);
			}
		} else {
			eCode = DMM_WRONG_PARAM;
		}
		if (eCode == DMM_NO_ERROR) {
			eCode = dmm_pat_refill_area_status_get(dmmPatAreaSel,
							       &areaStat);

			if (eCode == DMM_NO_ERROR) {
				if (areaStat.validDescriptor == 0) {
					eCode = DMM_HRDW_CONFIG_FAILED;
					printk(KERN_ALERT "hw config fail\n");
				}
			}

			while (!areaStat.done && !areaStat.ready &&
					eCode == DMM_NO_ERROR) {
				eCode = dmm_pat_refill_area_status_get(
						dmmPatAreaSel,
						&areaStat);
			}

			if (areaStat.error) {
				eCode = DMM_HRDW_CONFIG_FAILED;
				printk(KERN_ALERT "hw config fail\n");
			}
		}
	}

	return eCode;
}
EXPORT_SYMBOL(dmm_pat_refill);

