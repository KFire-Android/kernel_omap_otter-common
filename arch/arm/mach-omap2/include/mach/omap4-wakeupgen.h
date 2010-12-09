/*
 * OMAP4 Wakeupgen header file
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Written by Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef OMAP_ARCH_OMAP4_WAKEUPGEN_H
#define OMAP_ARCH_OMAP4_WAKEUPGEN_H

#define OMAP4_WKG_CONTROL_0			0x00
#define OMAP4_WKG_ENB_A_0			0x10
#define OMAP4_WKG_ENB_B_0			0x14
#define OMAP4_WKG_ENB_C_0			0x18
#define OMAP4_WKG_ENB_D_0			0x1C
#define OMAP4_WKG_ENB_SECURE_A_0		0x20
#define OMAP4_WKG_ENB_SECURE_B_0		0x24
#define OMAP4_WKG_ENB_SECURE_C_0		0x28
#define OMAP4_WKG_ENB_SECURE_D_0		0x2C
#define OMAP4_WKG_ENB_A_1			0x410
#define OMAP4_WKG_ENB_B_1			0x414
#define OMAP4_WKG_ENB_C_1			0x418
#define OMAP4_WKG_ENB_D_1			0x41C
#define OMAP4_WKG_ENB_SECURE_A_1		0x420
#define OMAP4_WKG_ENB_SECURE_B_1		0x424
#define OMAP4_WKG_ENB_SECURE_C_1		0x428
#define OMAP4_WKG_ENB_SECURE_D_1		0x42C
#define OMAP4_AUX_CORE_BOOT_0			0x800
#define OMAP4_AUX_CORE_BOOT_1			0x804
#define OMAP4_PTMSYNCREQ_MASK			0xC00
#define OMAP4_PTMSYNCREQ_EN			0xC04
#define OMAP4_TIMESTAMPCYCLELO			0xC08
#define OMAP4_TIMESTAMPCYCLEHI			0xC0C

extern void __iomem *wakeupgen_base;

extern void omap4_wakeupgen_set_interrupt(unsigned int cpu, unsigned int irq);
extern void omap4_wakeupgen_clear_interrupt(unsigned int cpu, unsigned int irq);
extern void omap4_wakeupgen_set_all(unsigned int cpu);
extern void omap4_wakeupgen_clear_all(unsigned int cpu);
extern void omap4_wakeupgen_save(void);
extern void omap4_wakeupgen_restore(void);
#endif
