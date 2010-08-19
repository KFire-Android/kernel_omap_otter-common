/*
 * omap4-common.h: OMAP4 specific common header file
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *
 * Author:
 *	Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef OMAP_ARCH_OMAP4_COMMON_H
#define OMAP_ARCH_OMAP4_COMMON_H

/*
 * SAR BANK offsets from base address OMAP44XX_SAR_RAM_BASE
 */
#define SAR_BANK1_OFFSET		0x0000
#define SAR_BANK2_OFFSET		0x1000
#define SAR_BANK3_OFFSET		0x2000
#define SAR_BANK4_OFFSET		0x3000

/*
 * Scratch pad memory offsets from SAR_BANK1
 */
#define CPU0_SAVE_OFFSET		0xb00
#define CPU1_SAVE_OFFSET		0xc00
#define MMU_OFFSET			0xd00
#define SCU_OFFSET			0xd20
#define L2X0_OFFSET			0xd28
#define CPU0_TWD_OFFSET			0xd30
#define CPU1_TWD_OFFSET			0xd38
#define OMAP_TYPE_OFFSET		0xd48

/*
 * Secure low power context save/restore API index
 */
#define HAL_SAVESECURERAM_INDEX		0x1a
#define HAL_SAVEHW_INDEX		0x1b
#define HAL_SAVEALL_INDEX		0x1c
#define HAL_SAVEGIC_INDEX		0x1d

/*
 * Secure HAL API flags
 */
#define FLAG_START_CRITICAL		0x4
#define FLAG_IRQFIQ_MASK		0x3
#define FLAG_IRQ_ENABLE			0x2
#define FLAG_FIQ_ENABLE			0x1
#define NO_FLAG				0x0

#ifndef __ASSEMBLER__
/*
 * wfi used in low power code. Directly opcode is used instead
 * of instruction to avoid mulit-omap build break
 */
#define do_wfi()			\
		__asm__ __volatile__ (".word	0xe320f003" : : : "memory")

#ifdef CONFIG_CACHE_L2X0
extern void __iomem *l2cache_base;
#endif

#ifdef CONFIG_SMP
extern void __iomem *scu_base;
#endif

extern void __iomem *gic_cpu_base_addr;
extern void __iomem *gic_dist_base_addr;
extern void __iomem *sar_ram_base;

extern void __init gic_init_irq(void);
extern void omap_smc1(u32 fn, u32 arg);
extern u32 omap_smc2(u32 id, u32 falg, u32 pargs);
extern u32 omap4_secure_dispatcher(u32 idx, u32 flag, u32 nargs,
				u32 arg1, u32 arg2, u32 arg3, u32 arg4);
extern void __init omap4_mpuss_init(void);
extern void omap4_enter_lowpower(unsigned int cpu, unsigned int power_state);
extern void __omap4_cpu_suspend(unsigned int cpu, unsigned int save_state);
extern unsigned long *omap4_cpu_wakeup_addr(void);

#endif

#endif
