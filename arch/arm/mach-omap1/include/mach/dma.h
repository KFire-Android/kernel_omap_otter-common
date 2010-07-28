/*
 *  OMAP DMA controller register offsets.
 *
 *  Copyright (C) 2003 Nokia Corporation
 *  Author: Juha Yrjölä <juha.yrjola@nokia.com>
 *
 *  Copyright (C) 2010 Texas Instruments
 *  Converted DMA library into platform driver by Manjunatha GK <manjugk@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __ASM_ARCH_OMAP1_DMA_H
#define __ASM_ARCH_OMAP1_DMA_H
/* Hardware registers for omap1 */
#define OMAP1_DMA_BASE			(0xfffed800)

#define OMAP1_DMA_GCR			0x400
#define OMAP1_DMA_GSCR			0x404
#define OMAP1_DMA_GRST			0x408
#define OMAP1_DMA_HW_ID			0x442
#define OMAP1_DMA_PCH2_ID		0x444
#define OMAP1_DMA_PCH0_ID		0x446
#define OMAP1_DMA_PCH1_ID		0x448
#define OMAP1_DMA_PCHG_ID		0x44a
#define OMAP1_DMA_PCHD_ID		0x44c
#define OMAP1_DMA_CAPS_0_U		0x44e
#define OMAP1_DMA_CAPS_0_L		0x450
#define OMAP1_DMA_CAPS_1_U		0x452
#define OMAP1_DMA_CAPS_1_L		0x454
#define OMAP1_DMA_CAPS_2		0x456
#define OMAP1_DMA_CAPS_3		0x458
#define OMAP1_DMA_CAPS_4		0x45a
#define OMAP1_DMA_PCH2_SR		0x460
#define OMAP1_DMA_PCH0_SR		0x480
#define OMAP1_DMA_PCH1_SR		0x482
#define OMAP1_DMA_PCHD_SR		0x4c0

#define OMAP1_LOGICAL_DMA_CH_COUNT	17

/* Common channel specific registers for omap1 */
#define OMAP1_DMA_CH_BASE(n)		(0x40 * (n) + 0x00)
#define OMAP1_DMA_CSDP(n)		(0x40 * (n) + 0x00)
#define OMAP1_DMA_CCR(n)		(0x40 * (n) + 0x02)
#define OMAP1_DMA_CICR(n)		(0x40 * (n) + 0x04)
#define OMAP1_DMA_CSR(n)		(0x40 * (n) + 0x06)
#define OMAP1_DMA_CEN(n)		(0x40 * (n) + 0x10)
#define OMAP1_DMA_CFN(n)		(0x40 * (n) + 0x12)
#define OMAP1_DMA_CSFI(n)		(0x40 * (n) + 0x14)
#define OMAP1_DMA_CSEI(n)		(0x40 * (n) + 0x16)
#define OMAP1_DMA_CPC(n)		(0x40 * (n) + 0x18)	/* 15xx only */
#define OMAP1_DMA_CSAC(n)		(0x40 * (n) + 0x18)
#define OMAP1_DMA_CDAC(n)		(0x40 * (n) + 0x1a)
#define OMAP1_DMA_CDEI(n)		(0x40 * (n) + 0x1c)
#define OMAP1_DMA_CDFI(n)		(0x40 * (n) + 0x1e)
#define OMAP1_DMA_CLNK_CTRL(n)		(0x40 * (n) + 0x28)

/* Channel specific registers only on omap1 */
#define OMAP1_DMA_CSSA_L(n)		(0x40 * (n) + 0x08)
#define OMAP1_DMA_CSSA_U(n)		(0x40 * (n) + 0x0a)
#define OMAP1_DMA_CDSA_L(n)		(0x40 * (n) + 0x0c)
#define OMAP1_DMA_CDSA_U(n)		(0x40 * (n) + 0x0e)
#define OMAP1_DMA_COLOR_L(n)		(0x40 * (n) + 0x20)
#define OMAP1_DMA_COLOR_U(n)		(0x40 * (n) + 0x22)
#define OMAP1_DMA_CCR2(n)		(0x40 * (n) + 0x24)
#define OMAP1_DMA_LCH_CTRL(n)		(0x40 * (n) + 0x2a)

/* Dummy defines to support multi omap code */
/* Channel specific registers */
#define OMAP_DMA4_CCR(n)		0
#define OMAP_DMA4_CSDP(n)		0
#define OMAP_DMA4_CEN(n)		0
#define OMAP_DMA4_CFN(n)		0
#define OMAP_DMA4_COLOR(n)		0
#define OMAP_DMA4_CSSA(n)		0
#define OMAP_DMA4_CSEI(n)		0
#define OMAP_DMA4_CSFI(n)		0
#define OMAP_DMA4_CDSA(n)		0
#define OMAP_DMA4_CDEI(n)		0
#define OMAP_DMA4_CDFI(n)		0
#define OMAP_DMA4_CSR(n)		0
#define OMAP_DMA4_CICR(n)		0
#define OMAP_DMA4_CLNK_CTRL(n)		0
#define OMAP_DMA4_CH_BASE(n)		0
#define OMAP_DMA4_CDAC(n)		0
#define OMAP_DMA4_CSAC(n)		0

/* Common registers */
#define OMAP_DMA4_IRQENABLE_L0		0
#define OMAP_DMA4_OCP_SYSCONFIG		0
#define OMAP_DMA4_GCR			0
#define OMAP_DMA4_IRQSTATUS_L0		0
#define OMAP_DMA4_CAPS_2		0
#define OMAP_DMA4_CAPS_3		0
#define OMAP_DMA4_CAPS_4		0
#define OMAP_DMA4_REVISION		0

#define OMAP_DMA4_CCR2(n)		0
#define OMAP_DMA4_LCH_CTRL(n)		0
#define OMAP_DMA4_COLOR_L(n)		0
#define OMAP_DMA4_COLOR_U(n)		0
#define OMAP1_DMA_COLOR(n)		0
#define OMAP_DMA4_CSSA_U(n)		0
#define OMAP_DMA4_CSSA_L(n)		0
#define OMAP1_DMA_CSSA(n)		0
#define OMAP_DMA4_CDSA_U(n)		0
#define OMAP_DMA4_CDSA_L(n)		0
#define OMAP1_DMA_CDSA(n)		0
#define OMAP_DMA4_CPC(n)		0

#define OMAP1_DMA_IRQENABLE_L0		0
#define OMAP1_DMA_IRQENABLE_L0		0
#define OMAP1_DMA_IRQSTATUS_L0		0
#define OMAP1_DMA_OCP_SYSCONFIG		0
#define OMAP_DMA4_HW_ID			0
#define OMAP_DMA4_CAPS_0_U		0
#define OMAP_DMA4_CAPS_0_L		0
#define OMAP_DMA4_CAPS_1_U		0
#define OMAP_DMA4_CAPS_1_L		0
#define OMAP_DMA4_GSCR			0
#define OMAP1_DMA_REVISION		0

struct omap_dma_lch {
	int next_lch;
	int dev_id;
	u16 saved_csr;
	u16 enabled_irqs;
	const char *dev_name;
	void (*callback)(int lch, u16 ch_status, void *data);
	void *data;
	long flags;
};

/* Dummy function */
extern void omap_clear_dma_sglist_mode(int lch);
#endif /* __ASM_ARCH_OMAP1_DMA_H */
