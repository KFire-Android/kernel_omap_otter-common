/*
 * clock4xxx.c - OMAP4xxx-specific clock functions
 *
 * Copyright (C) 2011 Texas Instruments, Inc.
 *
 * Contacts:
 * Mike Turquette <mturquette@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#undef DEBUG

#include <linux/clk.h>
#include "clock.h"

/*
 * PRCM bug WA: DPLL_ABE must be enabled when performing a warm reset or
 * things go badly...
 */
void omap4_clk_prepare_for_reboot(void)
{
	struct clk *dpll_abe_ck;

	dpll_abe_ck = clk_get(NULL, "dpll_abe_ck");

	omap3_noncore_dpll_enable(dpll_abe_ck);
}
