/*
 * OMAP5 PRCM Debugging
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ASM_MACH_OMAP2_PRCM_DEBUG_H
#define __ARCH_ASM_MACH_OMAP2_PRCM_DEBUG_H

#define PRCMDEBUG_LASTSLEEP	(1 << 0)
#define PRCMDEBUG_ON		(1 << 1)

#include <linux/types.h>
#include "iomap.h"

/* DPLL */
struct d_dpll_info {
	char *name;
	void __iomem *clkmodereg;
	void __iomem *autoidlereg;
	void __iomem *idlestreg;
	struct d_dpll_derived *derived[];
};

struct d_dpll_derived {
	char *name;
	void __iomem *gatereg;
	u32 gatemask;
};

/* Other internal generators */
struct d_intgen_info {
	char *name;
	void __iomem *gatereg;
	u32 gatemask;
};

/* Modules */
#define MOD_MASTER (1 << 0)
#define MOD_SLAVE (1 << 1)
#define MOD_MODE (1 << 2)

struct d_mod_info {
	char *name;
	void __iomem *clkctrl;
	int flags;
	int optclk;
};

/* Clock domains */
struct d_clkd_info {
	char *name;
	const u8 prcm_partition;
	const s16 cm_inst;
	const u16 clkdm_offs;
	int activity;
	struct d_dpll_info *dplls[20];
	struct d_intgen_info *intgens[20];
	struct d_mod_info *mods[];
};

/* Power domains */
struct d_pwrd_info {
	char *name;
	long prminst;
	int pwrst;
	struct d_clkd_info *cds[];
};

/* Voltage domains */
#define N_VDDS 4

struct d_vdd_info {
	char *name;
	int auto_ctrl_shift;
	int auto_ctrl_mask;
	struct d_pwrd_info **pds;
};

struct d_prcm_regs_info {
	u32 dpll_en_mask;
	u32 dpll_autoidle_mask;
	u32 clkctrl_stbyst_mask;
	u32 clkctrl_stbyst_shift;
	u32 clkctrl_idlest_mask;
	u32 clkctrl_idlest_shift;
	u32 clkctrl_module_mode_mask;
	u32 clkctrl_module_mode_shift;
	u32 clkstctrl_offset;
	u32 clktrctrl_mask;
	u32 clktrctrl_shift;
	u32 prm_pwrstatest_mask;
	u32 prm_pwrstatest_shift;
	u32 prm_lastpwrstatentered_mask;
	u32 prm_lastpwrstatentered_shift;
	u32 prm_logicstatest_mask;
	u32 prm_logicstatest_shift;
	u32 prm_dev_instance;
	u32 prm_volctrl_offset;
	u32 prm_io_st_mask;
	u32 control_padconf_wakeupevent0;
	u32 control_wkup_padconf_wakeupevent0;
};

#ifdef CONFIG_PM_DEBUG
extern void omap_prcmdebug_dump(int flags);
extern void print_prcm_wakeirq(int irq, const char *action_when);
#ifdef CONFIG_ARCH_OMAP4
extern struct d_vdd_info d_vddinfo_omap4[];
extern struct d_prcm_regs_info *d_prcm_regs_omap4;
#else
#define d_vddinfo_omap4	NULL;
#define d_prcm_regs_omap4 NULL;
#endif

#ifdef CONFIG_ARCH_OMAP5
extern struct d_vdd_info d_vddinfo_omap5[];
extern struct d_prcm_regs_info *d_prcm_regs_omap5;
#else
#define d_vddinfo_omap5	NULL;
#define d_prcm_regs_omap5 NULL;
#endif

#else
static inline void omap_prcmdebug_dump(int flags) { }
static inline void print_prcm_wakeirq(int irq, const char *action_when) { }
#endif

#endif /* __ARCH_ASM_MACH_OMAP2_PRCM_DEBUG_H */
