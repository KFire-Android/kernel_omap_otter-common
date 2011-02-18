/*
 * linux/arch/arm/mach-omap2/voltage.h
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Contacts:
 * Rajendra Nayak <rnayak@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_OMAP2_VOLTAGE_H
#define __ARCH_ARM_MACH_OMAP2_VOLTAGE_H

struct vc_config {
	u8	hsmcode;
	u8	sclh;
	u8	scll;
	u8	hssclh;
	u8	hsscll;
};

int omap4_voltage_scale(u8 v_domain, u32 vsel);
#endif
