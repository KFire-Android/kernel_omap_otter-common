/*:
 * Address mappings and base address for OMAP5 interconnects
 * and peripherals.
 *
 * Copyright (C) 2011 Texas Instruments
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *	Sricharan <r.sricharan@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ASM_ARCH_OMAP54XX_H
#define __ASM_ARCH_OMAP54XX_H

/*
 * Please place only base defines here and put the rest in device
 * specific headers.
 */
#define OMAP54XX_LLIA_BASE		0x3c000000
#define OMAP54XX_LLIB_BASE		0x3c800000
#define L4_54XX_BASE			0x4a000000
#define L4_WK_54XX_BASE			0x4ae00000
#define L4_PER_54XX_BASE		0x48000000
#define L4_EMU_54XX_BASE		0x54000000
#define L3_54XX_BASE			0x44000000
#define L3_54XX_BASE_CLK1		L3_54XX_BASE
#define L3_54XX_BASE_CLK2		0x44800000
#define L3_54XX_BASE_CLK3		0x45000000
#define OMAP54XX_EMIF1_BASE		0x4c000000
#define OMAP54XX_EMIF2_BASE		0x4d000000
#define OMAP54XX_DMM_BASE		0x4e000000
#define OMAP54XX_32KSYNCT_BASE		0x4ae04000
#define OMAP54XX_CM_CORE_AON_BASE	0x4a004000
#define OMAP54XX_CM_CORE_BASE		0x4a008000
#define OMAP54XX_PRM_BASE		0x4ae06000
#define OMAP54XX_SCRM_BASE		0x4ae0a000
#define OMAP54XX_PRCM_MPU_BASE		0x48243000
#define OMAP54XX_GPMC_BASE		0x50000000
#define OMAP543x_SCM_BASE		0x4a002000
#define OMAP543x_CTRL_BASE		0x4a002800
#define OMAP54XX_SCU_BASE		0x48210000
#define OMAP54XX_LOCAL_TWD_BASE		0x48210600
#define OMAP54XX_WKUPGEN_BASE		0x48281000
#define OMAP54XX_SAR_RAM_BASE		0x4ae26000
#define OMAP54XX_SAR_ROM_BASE		0x4a05e000
#define OMAP54XX_GIC_DIST_BASE		0x48211000
#define OMAP54XX_GIC_CPU_BASE		0x48212000
#define OMAP54XX_C2C_BASE		0x5c000000
#define OMAP54XX_USBTLL_BASE		(L4_54XX_BASE + 0x62000)
#define OMAP54XX_UHH_CONFIG_BASE	(L4_54XX_BASE + 0x64000)

#endif /* __ASM_ARCH_OMAP555554XX_H */
