/*
 * OMAP4 powerdomain control
 *
 * Copyright (C) 2009-2010 Texas Instruments, Inc.
 * Copyright (C) 2007-2009 Nokia Corporation
 *
 * Written by Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <plat/cpu.h>
#include <plat/powerdomain.h>
#include <plat/clockdomain.h>
#include <plat/prcm.h>

#include "pm.h"
#include "cm.h"
#include "cm-regbits-34xx.h"
#include "cm-regbits-44xx.h"
#include "prm.h"
#include "prm-regbits-34xx.h"
#include "prm-regbits-44xx.h"

int omap4_pwrdm_set_next_pwrst(struct powerdomain *pwrdm, u8 pwrst)
{
	prm_rmw_mod_reg_bits(OMAP_POWERSTATE_MASK,
				(pwrst << OMAP_POWERSTATE_SHIFT),
				pwrdm->prcm_offs, OMAP4_PM_PWSTCTRL);
	return 0;
}

int omap4_pwrdm_read_next_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs,
				OMAP4_PM_PWSTCTRL, OMAP_POWERSTATE_MASK);
}

int omap4_pwrdm_read_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs,
				OMAP4_PM_PWSTST, OMAP_POWERSTATEST_MASK);
}

int omap4_pwrdm_read_prev_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs, OMAP4_PM_PWSTST,
				OMAP4430_LASTPOWERSTATEENTERED_MASK);
}

struct pwrdm_functions omap4_pwrdm_functions = {
	.pwrdm_set_next_pwrst	= omap4_pwrdm_set_next_pwrst,
	.pwrdm_read_next_pwrst	= omap4_pwrdm_read_next_pwrst,
	.pwrdm_read_pwrst	= omap4_pwrdm_read_pwrst,
	.pwrdm_read_prev_pwrst	= omap4_pwrdm_read_prev_pwrst,
};
