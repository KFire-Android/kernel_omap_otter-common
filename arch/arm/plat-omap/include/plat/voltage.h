/*
 * OMAP Voltage Management Routines
 *
 * Copyright (C) 2011, Texas Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_OMAP_VOLTAGE_H
#define __ARCH_ARM_OMAP_VOLTAGE_H

struct voltagedomain;
struct omap_volt_data;

struct voltagedomain *voltdm_lookup(const char *name);
int voltdm_scale(struct voltagedomain *voltdm,
		 struct omap_volt_data *target_volt);
struct omap_volt_data *omap_voltage_get_curr_vdata(struct voltagedomain *voltdm);

#endif
