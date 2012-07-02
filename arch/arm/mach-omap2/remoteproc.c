/*
 * Remote processor machine-specific module for OMAP4
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/memblock.h>
#include <plat/common.h>
#include <plat/omap_device.h>
#include <plat/omap_hwmod.h>
#include <plat/remoteproc.h>
#include <plat/dsp.h>
#include <plat/io.h>
#include "cm2_44xx.h"
#include "cm1_44xx.h"
#include "cm-regbits-44xx.h"

#define OMAP4430_CM_M3_M3_CLKCTRL (OMAP4430_CM2_BASE + OMAP4430_CM2_CORE_INST \
		+ OMAP4_CM_DUCATI_DUCATI_CLKCTRL_OFFSET)

#define OMAP4430_CM_DSP_DSP_CLKCTRL (OMAP4430_CM1_BASE \
		+ OMAP4430_CM1_TESLA_INST + OMAP4_CM_TESLA_TESLA_CLKCTRL_OFFSET)

#define CONTROL_DSP_BOOTADDR (0x4A002304)

static void dump_ipu_registers(struct rproc *rproc)
{
	unsigned long flags;
	char buf[64];
	struct pt_regs regs;

	if (!rproc->cdump_buf1)
		return;

	remoteproc_fill_pt_regs(&regs,
			(struct exc_regs *)rproc->cdump_buf1);

	pr_info("REGISTER DUMP FOR REMOTEPROC %s\n", rproc->name);
	pr_info("PC is at %08lx\n", instruction_pointer(&regs));
	pr_info("LR is at %08lx\n", regs.ARM_lr);
	pr_info("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n"
	       "sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs.ARM_pc, regs.ARM_lr, regs.ARM_cpsr,
		regs.ARM_sp, regs.ARM_ip, regs.ARM_fp);
	pr_info("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs.ARM_r10, regs.ARM_r9,
		regs.ARM_r8);
	pr_info("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs.ARM_r7, regs.ARM_r6,
		regs.ARM_r5, regs.ARM_r4);
	pr_info("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs.ARM_r3, regs.ARM_r2,
		regs.ARM_r1, regs.ARM_r0);

	flags = regs.ARM_cpsr;
	buf[0] = flags & PSR_N_BIT ? 'N' : 'n';
	buf[1] = flags & PSR_Z_BIT ? 'Z' : 'z';
	buf[2] = flags & PSR_C_BIT ? 'C' : 'c';
	buf[3] = flags & PSR_V_BIT ? 'V' : 'v';
	buf[4] = '\0';

	pr_info("Flags: %s  IRQs o%s  FIQs o%s\n",
		buf, interrupts_enabled(&regs) ? "n" : "ff",
		fast_interrupts_enabled(&regs) ? "n" : "ff");
}

static void dump_dsp_registers(struct rproc *rproc)
{
	struct exc_dspRegs *regs;

	regs = (struct exc_dspRegs *)rproc->cdump_buf0;

	pr_info("REGISTER DUMP FOR REMOTEPROC %s\n", rproc->name);
	pr_info("PC is at %08x\n", regs->IRP);
	pr_info("SP is at %08x\n", regs->b15);
	pr_info("pc : [<%08x>]    sp : [<%08x>]", regs->IRP, regs->b15);
}

static struct rproc_ops ipu_ops = {
	.dump_registers = dump_ipu_registers,
};

static struct rproc_ops dsp_ops = {
	.dump_registers = dump_dsp_registers,
};

static struct omap_rproc_timers_info ipu_timers[] = {
	{ .id = 3 },
	{ .id = 4 },
#ifdef CONFIG_REMOTEPROC_WATCHDOG
	{ .id = 9 },
	{ .id = 11 },
#endif
};

static struct omap_rproc_timers_info dsp_timers[] = {
	{ .id = 5 },
#ifdef CONFIG_REMOTEPROC_WATCHDOG
	{ .id = 6 },
#endif
};

static struct omap_rproc_pdata omap4_rproc_data[] = {
	{
		.name		= "dsp",
		.iommu_name	= "tesla",
		.firmware	= "tesla-dsp.bin",
		.oh_name	= "dsp_c0",
		.clkdm_name	= "tesla_clkdm",
		.timers		= dsp_timers,
		.timers_cnt	= ARRAY_SIZE(dsp_timers),
		.idle_addr	= OMAP4430_CM_DSP_DSP_CLKCTRL,
		.idle_mask	= OMAP4430_STBYST_MASK,
		.suspend_mask	= ~0,
		.sus_timeout	= 5000,
		.sus_mbox_name	= "mailbox-2",
		.boot_reg	= CONTROL_DSP_BOOTADDR,
		.ops		= &dsp_ops,
	},
	{
		.name		= "ipu",
		.iommu_name	= "ducati",
		.firmware	= "ducati-m3.bin",
		.oh_name	= "ipu_c0",
		.oh_name_opt	= "ipu_c1",
		.clkdm_name	= "ducati_clkdm",
		.timers		= ipu_timers,
		.timers_cnt	= ARRAY_SIZE(ipu_timers),
		.idle_addr	= OMAP4430_CM_M3_M3_CLKCTRL,
		.idle_mask	= OMAP4430_STBYST_MASK,
		.suspend_mask	= ~0,
		.sus_timeout	= 5000,
		.sus_mbox_name	= "mailbox-1",
		.ops		= &ipu_ops,
	},
};

static struct omap_device_pm_latency omap_rproc_latency[] = {
	{
		OMAP_RPROC_DEFAULT_PM_LATENCY,
	},
};

static struct rproc_mem_pool *omap_rproc_get_pool(const char *name)
{
	struct rproc_mem_pool *pool = NULL;
	phys_addr_t paddr1;
	phys_addr_t paddr2;
	u32 len1;
	u32 len2;

	/* get ipu mempool  */
	if (!strcmp("ipu", name)) {
		paddr1 = omap_ipu_get_mempool_base(
						OMAP_RPROC_MEMPOOL_STATIC);
		paddr2 = omap_ipu_get_mempool_base(
						OMAP_RPROC_MEMPOOL_DYNAMIC);
		len1 = omap_ipu_get_mempool_size(OMAP_RPROC_MEMPOOL_STATIC);
		len2 = omap_ipu_get_mempool_size(OMAP_RPROC_MEMPOOL_DYNAMIC);
	/* get dsp mempool*/
	} else if (!strcmp("dsp", name)) {
		paddr1 = omap_dsp_get_mempool_tbase(
						OMAP_RPROC_MEMPOOL_STATIC);
		paddr2 = omap_dsp_get_mempool_tbase(
						OMAP_RPROC_MEMPOOL_DYNAMIC);
		len1 = omap_dsp_get_mempool_tsize(OMAP_RPROC_MEMPOOL_STATIC);
		len2 = omap_dsp_get_mempool_tsize(OMAP_RPROC_MEMPOOL_DYNAMIC);
	} else
		return pool;

	if (!paddr1 && !paddr2) {
		pr_err("%s - no carveout memory is available at all\n", name);
		return pool;
	}
	if (!paddr1 || !len1)
		pr_warn("%s - static memory is unavailable: 0x%x, 0x%x\n",
			name, paddr1, len1);
	if (!paddr2 || !len2)
		pr_warn("%s - carveout memory is unavailable: 0x%x, 0x%x\n",
			name, paddr2, len2);

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (pool) {
		pool->st_base = paddr1;
		pool->st_size = len1;
		pool->mem_base = paddr2;
		pool->mem_size = len2;
		pool->cur_base = paddr2;
		pool->cur_size = len2;
	}

	return pool;
}

static int __init omap_rproc_init(void)
{
	const char *pdev_name = "omap-rproc";
	struct omap_hwmod *oh[2];
	struct omap_device *od;
	int i, ret = 0, oh_count;

	/* names like ipu_cx/dsp_cx might show up on other OMAPs, too */
	if (!cpu_is_omap44xx())
		return 0;

	for (i = 0; i < ARRAY_SIZE(omap4_rproc_data); i++) {
		const char *oh_name = omap4_rproc_data[i].oh_name;
		const char *oh_name_opt = omap4_rproc_data[i].oh_name_opt;
		oh_count = 0;

		if (omap_total_ram_size() == SZ_512M) {
			if (!strcmp("ipu", omap4_rproc_data[i].name))
				omap4_rproc_data[i].firmware =
					"ducati-m3.512MB.bin";
			else if (!strcmp("dsp", omap4_rproc_data[i].name))
				omap4_rproc_data[i].firmware =
					"tesla-dsp.512MB.bin";
		}

		oh[0] = omap_hwmod_lookup(oh_name);

		if (!oh[0]) {
			pr_err("could not look up %s\n", oh_name);
			continue;
		}
		oh_count++;

		if (oh_name_opt) {
			oh[1] = omap_hwmod_lookup(oh_name_opt);
			if (!oh[1]) {
				pr_err("could not look up %s\n", oh_name_opt);
				continue;
			}
			oh_count++;
		}

		omap4_rproc_data[i].memory_pool =
				omap_rproc_get_pool(omap4_rproc_data[i].name);
		od = omap_device_build_ss(pdev_name, i, oh, oh_count,
					&omap4_rproc_data[i],
					sizeof(struct omap_rproc_pdata),
					omap_rproc_latency,
					ARRAY_SIZE(omap_rproc_latency),
					false);
		if (IS_ERR(od)) {
			pr_err("Could not build omap_device for %s:%s\n",
							pdev_name, oh_name);
			ret = PTR_ERR(od);
		}
	}

	return ret;
}
/* must be ready in time for device_initcall users */
subsys_initcall(omap_rproc_init);
