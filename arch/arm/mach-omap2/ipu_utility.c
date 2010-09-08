/*
 * OMAP IPU_PM utility
 *
 * Copyright (C) 2010 Texas Instruments Inc.
 *
 * Written by Paul Hunt <hunt@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <asm/io.h>
#include <plat/io.h>

#define ICONT1_ITCM_BASE	0x5A008000
#define ICONT2_ITCM_BASE	0x5A018000

static void __iomem *icont_itcm_ptr;

/* Op-codes for simple boot sequence ending in WFI */
static const u32 ICONT_Boot_WFI[] = {
	0xEA000006,
	0xEAFFFFFE,
	0xEAFFFFFE,
	0xEAFFFFFE,
	0xEAFFFFFE,
	0xEAFFFFFE,
	0xEAFFFFFE,
	0xEAFFFFFE,
	0xE3A00000,
	0xEE070F9A,
	0xEE070F90,
	0xE3A00000,
	0xEAFFFFFE,
	0xEAFFFFF1,
};

static const u32 ICONT_Boot_WFI_length = ARRAY_SIZE(ICONT_Boot_WFI);

static void load_ivahd_idle_boot_code(void)
{
	int i;

	icont_itcm_ptr = ioremap((u32) ICONT1_ITCM_BASE, 0x1000);
	for (i = 0; i < ICONT_Boot_WFI_length; i++)
		__raw_writel(ICONT_Boot_WFI[i], (icont_itcm_ptr + i));
	iounmap(icont_itcm_ptr);

	icont_itcm_ptr = ioremap((u32) ICONT2_ITCM_BASE, 0x1000);
	for (i = 0; i < ICONT_Boot_WFI_length; i++)
		__raw_writel(ICONT_Boot_WFI[i], (icont_itcm_ptr + i));
	iounmap(icont_itcm_ptr);
}
EXPORT_SYMBOL(load_ivahd_idle_boot_code);
