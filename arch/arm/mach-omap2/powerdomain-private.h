/*
 * Internal powerdomain header
 *
 * Copyright (C) 2012 Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __ARCH_ARM_MACH_OMAP2_POWERDOMAIN_PRIVATE_H
#define __ARCH_ARM_MACH_OMAP2_POWERDOMAIN_PRIVATE_H

#define PWRDM_POWER_RET		0x1

#define _PWRDM_STATE_COUNT_IDX(s) (((s) == PWRDM_POWER_CSWR ||	\
				   (s) == PWRDM_POWER_OSWR) ?	\
				   PWRDM_POWER_RET : (s))

#endif /* __ARCH_ARM_MACH_OMAP2_POWERDOMAIN_PRIVATE_H */
