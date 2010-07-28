/*
 *  OMAP DMA controller register offsets.
 *
 *  Copyright (C) 2003 Nokia Corporation
 *  Author: Juha Yrjölä <juha.yrjola@nokia.com>
 *
 *  Copyright (C) 2009 Texas Instruments
 *  Added OMAP4 support - Santosh Shilimkar <santosh.shilimkar@ti.com>
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

#ifndef __ASM_ARCH_OMAP2_DMA_H
#define __ASM_ARCH_OMAP2_DMA_H

/* OMAP2 Plus register offset's */
#define OMAP_DMA4_REVISION		0x00
#define OMAP_DMA4_GCR			0x78
#define OMAP_DMA4_IRQSTATUS_L0		0x08
#define OMAP_DMA4_IRQSTATUS_L1		0x0c
#define OMAP_DMA4_IRQSTATUS_L2		0x10
#define OMAP_DMA4_IRQSTATUS_L3		0x14
#define OMAP_DMA4_IRQENABLE_L0		0x18
#define OMAP_DMA4_IRQENABLE_L1		0x1c
#define OMAP_DMA4_IRQENABLE_L2		0x20
#define OMAP_DMA4_IRQENABLE_L3		0x24
#define OMAP_DMA4_SYSSTATUS		0x28
#define OMAP_DMA4_OCP_SYSCONFIG		0x2c
#define OMAP_DMA4_CAPS_0		0x64
#define OMAP_DMA4_CAPS_2		0x6c
#define OMAP_DMA4_CAPS_3		0x70
#define OMAP_DMA4_CAPS_4		0x74

/* Should be part of hwmod data base ? */
#define OMAP_DMA4_LOGICAL_DMA_CH_COUNT	32	/* REVISIT: Is this 32 + 2? */

/* Common channel specific registers for omap2 */
#define OMAP_DMA4_CH_BASE(n)		(0x60 * (n) + 0x80)
#define OMAP_DMA4_CCR(n)		(0x60 * (n) + 0x80)
#define OMAP_DMA4_CLNK_CTRL(n)		(0x60 * (n) + 0x84)
#define OMAP_DMA4_CICR(n)		(0x60 * (n) + 0x88)
#define OMAP_DMA4_CSR(n)		(0x60 * (n) + 0x8c)
#define OMAP_DMA4_CSDP(n)		(0x60 * (n) + 0x90)
#define OMAP_DMA4_CEN(n)		(0x60 * (n) + 0x94)
#define OMAP_DMA4_CFN(n)		(0x60 * (n) + 0x98)
#define OMAP_DMA4_CSEI(n)		(0x60 * (n) + 0xa4)
#define OMAP_DMA4_CSFI(n)		(0x60 * (n) + 0xa8)
#define OMAP_DMA4_CDEI(n)		(0x60 * (n) + 0xac)
#define OMAP_DMA4_CDFI(n)		(0x60 * (n) + 0xb0)
#define OMAP_DMA4_CSAC(n)		(0x60 * (n) + 0xb4)
#define OMAP_DMA4_CDAC(n)		(0x60 * (n) + 0xb8)

/* Channel specific registers only on omap2 */
#define OMAP_DMA4_CSSA(n)		(0x60 * (n) + 0x9c)
#define OMAP_DMA4_CDSA(n)		(0x60 * (n) + 0xa0)
#define OMAP_DMA4_CCEN(n)		(0x60 * (n) + 0xbc)
#define OMAP_DMA4_CCFN(n)		(0x60 * (n) + 0xc0)
#define OMAP_DMA4_COLOR(n)		(0x60 * (n) + 0xc4)

/* Additional registers available on OMAP4 */
#define OMAP_DMA4_CDP(n)		(0x60 * (n) + 0xd0)
#define OMAP_DMA4_CNDP(n)		(0x60 * (n) + 0xd4)
#define OMAP_DMA4_CCDN(n)		(0x60 * (n) + 0xd8)

#endif /* __ASM_ARCH_OMAP2_DMA_H */
