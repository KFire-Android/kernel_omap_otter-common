/*
 * omap-dmic.h  --  OMAP Digital Microphone Controller
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *	   David Lambert <dlambert@ti.com>
 *	   Misael Lopez Cruz <misael.lopez@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _OMAP_DMIC_H
#define _OMAP_DMIC_H

#include <sound/soc.h>

#define OMAP44XX_DMIC_L3_BASE	0x4902e000

#define DMIC_REVISION		0x00
#define DMIC_SYSCONFIG		0x10
#define DMIC_IRQSTATUS_RAW	0x24
#define DMIC_IRQSTATUS		0x28
#define DMIC_IRQENABLE_SET	0x2C
#define DMIC_IRQENABLE_CLR	0x30
#define DMIC_IRQWAKE_EN		0x34
#define DMIC_DMAENABLE_SET	0x38
#define DMIC_DMAENABLE_CLR	0x3C
#define DMIC_DMAWAKEEN		0x40
#define DMIC_CTRL		0x44
#define DMIC_DATA		0x48
#define DMIC_FIFO_CTRL		0x4C
#define DMIC_FIFO_DMIC1R_DATA	0x50
#define DMIC_FIFO_DMIC1L_DATA	0x54
#define DMIC_FIFO_DMIC2R_DATA	0x58
#define DMIC_FIFO_DMIC2L_DATA	0x5C
#define DMIC_FIFO_DMIC3R_DATA	0x60
#define DMIC_FIFO_DMIC3L_DATA	0x64

/*
 * DMIC_IRQ bit fields
 * IRQSTATUS_RAW, IRQSTATUS, IRQENABLE_SET, IRQENABLE_CLR
 */

#define DMIC_IRQ		(1 << 0)
#define DMIC_IRQ_FULL		(1 << 1)
#define DMIC_IRQ_ALMST_EMPTY	(1 << 2)
#define DMIC_IRQ_EMPTY		(1 << 3)
#define DMIC_IRQ_MASK		0x07

/*
 * DMIC_DMAENABLE bit fields
 */

#define DMIC_DMA_ENABLE		0x1

/*
 * DMIC_CTRL bit fields
 */

#define DMIC_UP1_ENABLE		0x0001
#define DMIC_UP2_ENABLE		0x0002
#define DMIC_UP3_ENABLE		0x0004
#define DMIC_FORMAT		0x0008
#define DMIC_POLAR1		0x0010
#define DMIC_POLAR2		0x0020
#define DMIC_POLAR3		0x0040
#define DMIC_POLAR_MASK		0x0070
#define DMIC_CLK_DIV_SHIFT	7
#define DMIC_CLK_DIV_MASK	0x0380
#define	DMIC_RESET		0x0400

#define DMIC_ENABLE_MASK	0x007

#define DMICOUTFORMAT_LJUST	(0 << 3)
#define DMICOUTFORMAT_RJUST	(1 << 3)

/*
 * DMIC_FIFO_CTRL bit fields
 */

#define DMIC_THRES_MAX		0xF

enum omap_dmic_clk {
	OMAP_DMIC_SYSCLK_PAD_CLKS,		/* PAD_CLKS */
	OMAP_DMIC_SYSCLK_SLIMBLUS_CLKS,		/* SLIMBUS_CLK */
	OMAP_DMIC_SYSCLK_SYNC_MUX_CLKS,		/* DMIC_SYNC_MUX_CLK */
};

/* DMIC dividers */
enum omap_dmic_div {
	OMAP_DMIC_CLKDIV,
};

struct omap_dmic_link {
	int irq_mask;
	int threshold;
	int format;
	int channels;
	int polar;
};

#endif
