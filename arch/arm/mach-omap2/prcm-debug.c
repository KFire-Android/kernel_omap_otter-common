/*
 * OMAP4+ PRCM Debugging
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * Copyright (C) 2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Madhusudhan Chittim
 *
 * Original code from Android Icecream sandwich source code for OMAP4,
 * Modified by TI for supporting OMAP4+.
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

#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>

#include "clock.h"
#include "cminst44xx.h"
#include "prm44xx.h"
#include "powerdomain.h"
#include "powerdomain-private.h"

#include "prcm-debug.h"

/* Display strings */

static char *vddauto_s[] = {"disabled", "SLEEP", "RET", "reserved"};

static char *pwrstate_s[] = {"OFF", "RET", "INACTIVE", "ON"};

static char *logic_s[] = {"OFF", "ON"};

static char *cmtrctrl_s[] = {"NOSLEEP", "SW_SLEEP", "SW_WKUP", "HW_AUTO"};

static char *modmode_s[] = {"DISABLED", "AUTO", "ENABLED", "3"};

static char *modstbyst_s[] = {"ON", "STBY"};

static char *modidlest_s[] = {"ON", "TRANSITION", "IDLE", "DISABLED"};

#define d_pr(sf, fmt, args...)				\
	{						\
		if (sf)					\
			seq_printf(sf, fmt , ## args);	\
		else					\
			pr_info(fmt , ## args);		\
	}

#define d_pr_ctd(sf, fmt, args...)			\
	{						\
		if (sf)					\
			seq_printf(sf, fmt , ## args);	\
		else					\
			pr_cont(fmt , ## args);		\
	}

static struct d_vdd_info *d_vdd;
static struct d_prcm_regs_info *d_prcm_regs;


static void prcmdebug_dump_dpll(struct seq_file *sf,
				struct d_dpll_info *dpll,
				int flags)
{
	u32 en = __raw_readl(dpll->clkmodereg) & d_prcm_regs->dpll_en_mask;
	u32 autoidle = __raw_readl(dpll->autoidlereg) &
				d_prcm_regs->dpll_autoidle_mask;
	struct d_dpll_derived **derived;

	if (flags & (PRCMDEBUG_LASTSLEEP | PRCMDEBUG_ON) &&
	    (en == DPLL_LOW_POWER_BYPASS))
		return;

	d_pr(sf, "         %s auto=0x%x en=0x%x", dpll->name, autoidle, en);

	if (dpll->idlestreg)
		d_pr_ctd(sf, " st=0x%x",	__raw_readl(dpll->idlestreg));

	d_pr_ctd(sf, "\n");

	derived = dpll->derived;

	while (*derived) {
		u32 enabled = __raw_readl((*derived)->gatereg) &
			(*derived)->gatemask;

		if (!(flags & (PRCMDEBUG_LASTSLEEP | PRCMDEBUG_ON)) ||
		    enabled)
			d_pr(sf, "            %s enabled=0x%x\n",
			     (*derived)->name, enabled);
		derived++;
	}
}


static void prcmdebug_dump_intgen(struct seq_file *sf,
				  struct d_intgen_info *intgen,
				  int flags)
{
	u32 enabled = __raw_readl(intgen->gatereg) & intgen->gatemask;

	if (flags & (PRCMDEBUG_LASTSLEEP | PRCMDEBUG_ON) && !enabled)
		return;

	d_pr(sf, "         %s enabled=0x%x\n", intgen->name, enabled);
}

static void prcmdebug_dump_mod(struct seq_file *sf, struct d_mod_info *mod,
	int flags)
{
	u32 clkctrl = __raw_readl(mod->clkctrl);
	u32 stbyst = (clkctrl & d_prcm_regs->clkctrl_stbyst_mask) >>
				d_prcm_regs->clkctrl_stbyst_shift;
	u32 idlest = (clkctrl & d_prcm_regs->clkctrl_idlest_mask) >>
				d_prcm_regs->clkctrl_idlest_shift;
	u32 optclk = clkctrl & mod->optclk;

	if (flags & (PRCMDEBUG_LASTSLEEP | PRCMDEBUG_ON) &&
	    (!(mod->flags & MOD_MASTER) || stbyst == 1) &&
	    (!(mod->flags & MOD_SLAVE) || idlest == 2 || idlest == 3) &&
	    !optclk)
		return;

	if (flags & PRCMDEBUG_LASTSLEEP &&
	    (mod->flags & MOD_MODE &&
	     ((clkctrl & d_prcm_regs->clkctrl_module_mode_mask) >>
	       d_prcm_regs->clkctrl_module_mode_shift) == 1 /* AUTO */) &&
	    (!(mod->flags & MOD_SLAVE) || idlest == 0) /* ON */ &&
	    !optclk)
		return;

	d_pr(sf, "         %s", mod->name);

	if (mod->flags & MOD_MODE)
		d_pr_ctd(sf, " mode=%s",
			 modmode_s[(clkctrl &
				   d_prcm_regs->clkctrl_module_mode_mask) >>
				   d_prcm_regs->clkctrl_module_mode_shift]);

	if (mod->flags & MOD_MASTER)
		d_pr_ctd(sf, " stbyst=%s",
			 modstbyst_s[stbyst]);

	if (mod->flags & MOD_SLAVE)
		d_pr_ctd(sf, " idlest=%s",
			 modidlest_s[idlest]);

	if (optclk)
		d_pr_ctd(sf, " optclk=0x%x", optclk);

	d_pr_ctd(sf, "\n");
}

static void prcmdebug_dump_real_cd(struct seq_file *sf, struct d_clkd_info *cd,
	int flags)
{
	u32 clktrctrl =
		omap4_cminst_read_inst_reg(cd->prcm_partition, cd->cm_inst,
					   cd->clkdm_offs +
					   d_prcm_regs->clkstctrl_offset);
	u32 mode = (clktrctrl & d_prcm_regs->clktrctrl_mask) >>
			d_prcm_regs->clktrctrl_shift;
	u32 activity = clktrctrl & cd->activity;

	if (flags & PRCMDEBUG_LASTSLEEP && mode == 3 /* HW_AUTO */)
		return;

	d_pr(sf, "      %s mode=%s", cd->name, cmtrctrl_s[mode]);

	d_pr_ctd(sf, " activity=0x%x", activity);

	d_pr_ctd(sf, "\n");
}

static void prcmdebug_dump_cd(struct seq_file *sf, struct d_clkd_info *cd,
	int flags)
{
	struct d_mod_info **mod;
	struct d_intgen_info **intgen;
	struct d_dpll_info **dpll;

	if (cd->cm_inst != -1)
		prcmdebug_dump_real_cd(sf, cd, flags);
	else if (!(flags & PRCMDEBUG_LASTSLEEP))
		d_pr(sf, "      %s\n", cd->name);

	mod = cd->mods;

	while (*mod) {
		prcmdebug_dump_mod(sf, *mod, flags);
		mod++;
	}

	dpll = cd->dplls;

	while (*dpll) {
		prcmdebug_dump_dpll(sf, *dpll, flags);
		dpll++;
	}

	intgen = cd->intgens;

	while (*intgen) {
		prcmdebug_dump_intgen(sf, *intgen, flags);
		intgen++;
	}
}

static void prcmdebug_dump_pd(struct seq_file *sf, struct d_pwrd_info *pd,
	int flags)
{
	u32 pwrstst, currst, prevst;
	struct d_clkd_info **cd;

	if (pd->pwrst != -1 && pd->prminst != -1) {
		pwrstst = omap4_prm_read_inst_reg(pd->prminst, pd->pwrst);
		currst = (pwrstst & d_prcm_regs->prm_pwrstatest_mask) >>
				d_prcm_regs->prm_pwrstatest_shift;
		prevst = (pwrstst & d_prcm_regs->prm_lastpwrstatentered_mask) >>
				d_prcm_regs->prm_lastpwrstatentered_shift;

		if (flags & PRCMDEBUG_LASTSLEEP &&
		    (prevst == PWRDM_POWER_OFF || prevst == PWRDM_POWER_RET))
			return;

		if (flags & PRCMDEBUG_ON &&
		    (currst == PWRDM_POWER_OFF || currst == PWRDM_POWER_RET))
			return;

		d_pr(sf, "   %s curr=%s prev=%s logic=%s\n", pd->name,
		     pwrstate_s[currst],
		     pwrstate_s[prevst],
		     logic_s[(pwrstst & d_prcm_regs->prm_logicstatest_mask) >>
				d_prcm_regs->prm_logicstatest_shift]);
	} else {
		if (flags & PRCMDEBUG_LASTSLEEP)
			return;

		d_pr(sf, "   %s\n", pd->name);
	}

	cd = pd->cds;

	while (*cd) {
		prcmdebug_dump_cd(sf, *cd, flags);
		cd++;
	}
}

static int _prcmdebug_dump(struct seq_file *sf, int flags)
{
	int i;
	u32 prm_voltctrl;
	struct d_pwrd_info **pd;

	if (!d_vdd || !d_prcm_regs) {
		pr_err("PRCM Debug is not initialized properly!!!\n");
		return -EINVAL;
	}

	prm_voltctrl = omap4_prm_read_inst_reg(d_prcm_regs->prm_dev_instance,
					       d_prcm_regs->prm_volctrl_offset);
	for (i = 0; i < N_VDDS; i++) {
		if (!(flags & PRCMDEBUG_LASTSLEEP)) {
			d_pr(sf, "%s", d_vdd[i].name);

			if (d_vdd[i].auto_ctrl_shift != -1) {
				int auto_ctrl =
					(prm_voltctrl &
					 d_vdd[i].auto_ctrl_mask) >>
					d_vdd[i].auto_ctrl_shift;
				d_pr_ctd(sf, " auto=%s\n",
					 vddauto_s[auto_ctrl]);
			} else {
				d_pr_ctd(sf, " (no auto)\n");
			}
		}

		pd = d_vdd[i].pds;

		while (*pd) {
			prcmdebug_dump_pd(sf, *pd, flags);
			pd++;
		}
	}

	return 0;
}

void omap_prcmdebug_dump(int flags)
{
	_prcmdebug_dump(NULL, flags);
}

static int prcmdebug_all_dump(struct seq_file *sf, void *private)
{
	_prcmdebug_dump(sf, 0);
	return 0;
}

static int prcmdebug_all_open(struct inode *inode, struct file *file)
{
	return single_open(file, prcmdebug_all_dump, NULL);
}


static const struct file_operations prcmdebug_all_fops = {
	.open = prcmdebug_all_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int prcmdebug_on_dump(struct seq_file *sf, void *private)
{
	_prcmdebug_dump(sf, PRCMDEBUG_ON);
	return 0;
}

static int prcmdebug_on_open(struct inode *inode, struct file *file)
{
	return single_open(file, prcmdebug_on_dump, NULL);
}

static const struct file_operations prcmdebug_on_fops = {
	.open = prcmdebug_on_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init prcmdebug_init(void)
{
	if (cpu_is_omap44xx()) {
		d_vdd = d_vddinfo_omap4;
		d_prcm_regs = d_prcm_regs_omap4;
	} else if (cpu_is_omap54xx()) {
		d_vdd = d_vddinfo_omap5;
		d_prcm_regs = d_prcm_regs_omap5;
	} else {
		pr_err("%s: Only OMAP4 and OMAP5 supported\n", __func__);
		return -ENODEV;
	}

	if (IS_ERR_OR_NULL(debugfs_create_file("prcm", S_IRUGO, NULL, NULL,
					       &prcmdebug_all_fops)))
		pr_err("%s: failed to create prcm file\n", __func__);

	if (IS_ERR_OR_NULL(debugfs_create_file("prcm-on", S_IRUGO, NULL, NULL,
					       &prcmdebug_on_fops)))
		pr_err("%s: failed to create prcm-on file\n", __func__);

	return 0;
}

arch_initcall(prcmdebug_init);
