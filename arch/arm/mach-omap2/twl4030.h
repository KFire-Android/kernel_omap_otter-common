/*
 * Copyright (C) 2010 Texas Instruments, Inc.
 * Lesly A M <leslyam@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __ARCH_ARM_MACH_OMAP3_TWL4030_SCRIPT_H
#define __ARCH_ARM_MACH_OMAP3_TWL4030_SCRIPT_H

#include <linux/i2c/twl.h>
#include "voltage.h"

#ifdef CONFIG_TWL4030_POWER
extern void twl4030_get_scripts(struct twl4030_power_data *t2scripts_data);
/* extern void twl4030_get_vc_timings(struct prm_setup_vc *setup_vc); */
#else
extern void twl4030_get_scripts(struct twl4030_power_data *t2scripts_data) {}
/* extern void twl4030_get_vc_timings(struct prm_setup_vc *setup_vc) {} */
#endif

#endif
