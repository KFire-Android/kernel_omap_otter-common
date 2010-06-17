/*
 * OMAP2 and OMAP3 powerdomain control
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
#include <linux/errno.h>
#include <linux/delay.h>
#include <plat/powerdomain.h>
#include <plat/prcm.h>
#include "powerdomains.h"

int omap2_pwrdm_set_next_pwrst(struct powerdomain *pwrdm, u8 pwrst)
{
	return prm_rmw_mod_reg_bits(OMAP_POWERSTATE_MASK,
				(pwrst << OMAP_POWERSTATE_SHIFT),
				pwrdm->prcm_offs, OMAP2_PM_PWSTCTRL);
}

int omap2_pwrdm_read_next_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs,
				OMAP2_PM_PWSTCTRL, OMAP_POWERSTATE_MASK);
}

int omap2_pwrdm_read_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs,
				OMAP2_PM_PWSTST, OMAP_POWERSTATEST_MASK);
}

int omap2_pwrdm_read_prev_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs, OMAP3430_PM_PREPWSTST,
				OMAP3430_LASTPOWERSTATEENTERED_MASK);
}

int omap2_pwrdm_set_logic_retst(struct powerdomain *pwrdm, u8 pwrst)
{
	u32 v;

	v = pwrst << __ffs(OMAP3430_LOGICL1CACHERETSTATE_MASK);
	prm_rmw_mod_reg_bits(OMAP3430_LOGICL1CACHERETSTATE_MASK, v,
				pwrdm->prcm_offs, OMAP2_PM_PWSTCTRL);

	return 0;
}

int omap2_pwrdm_read_logic_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs, OMAP2_PM_PWSTST,
					OMAP3430_LOGICSTATEST_MASK);
}


int omap2_pwrdm_read_prev_logic_pwrst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs, OMAP3430_PM_PREPWSTST,
					OMAP3430_LASTLOGICSTATEENTERED_MASK);
}

int omap2_pwrdm_read_logic_retst(struct powerdomain *pwrdm)
{
	return prm_read_mod_bits_shift(pwrdm->prcm_offs, OMAP2_PM_PWSTCTRL,
					OMAP3430_LOGICSTATEST_MASK);
}

struct pwrdm_functions omap2_pwrdm_functions = {
	.pwrdm_set_next_pwrst	= omap2_pwrdm_set_next_pwrst,
	.pwrdm_read_next_pwrst	= omap2_pwrdm_read_next_pwrst,
	.pwrdm_read_pwrst	= omap2_pwrdm_read_pwrst,
	.pwrdm_read_prev_pwrst	= omap2_pwrdm_read_prev_pwrst,
	.pwrdm_set_logic_retst	= omap2_pwrdm_set_logic_retst,
	.pwrdm_read_logic_pwrst	= omap2_pwrdm_read_logic_pwrst,
	.pwrdm_read_prev_logic_pwrst	= omap2_pwrdm_read_prev_logic_pwrst,
	.pwrdm_read_logic_retst	= omap2_pwrdm_read_logic_retst,
};
