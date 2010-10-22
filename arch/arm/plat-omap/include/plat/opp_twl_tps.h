/*
 * opp_twl_tps.h - TWL/TPS-specific headers for the OPP code
 *
 * Copyright (C) 2009 Texas Instruments Incorporated.
 *	Nishanth Menon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * XXX This code belongs as part of some other TWL/TPS code.
 */
#ifndef _ARCH_ARM_PLAT_OMAP_OPP_TWL_TPS_H
#define _ARCH_ARM_PLAT_OMAP_OPP_TWL_TPS_H

#include <linux/kernel.h>

unsigned long omap_twl_vsel_to_uv(const u8 vsel);
u8 omap_twl_uv_to_vsel(unsigned long uV);
u8 omap_twl_onforce_cmd(const u8 vsel);
u8 omap_twl_on_cmd(const u8 vsel);
u8 omap_twl_sleepforce_cmd(const u8 vsel);
u8 omap_twl_sleep_cmd(const u8 vsel);

#endif
