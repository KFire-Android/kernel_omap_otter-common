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

/**
 * struct omap_volt_data - Omap voltage specific data.
 * @volt_nominal:	The possible voltage value in uV
 * @voltage_calibrated:	The Calibrated voltage value in uV
 * @voltage_dynamic_nominal:	The run time optimized nominal voltage for
 *			the device. Dynamic nominal is the nominal voltage
 *			specialized for that OPP on the device in uV.
 * @volt_margin:	Additional sofware margin to add to OPP calibrated
 *			voltage
 * @sr_efuse_offs:	The offset of the efuse register(from system
 *			control module base address) from where to read
 *			the n-target value for the smartreflex module.
 * @sr_errminlimit:	Error min limit value for smartreflex. This value
 *			differs at differnet opp and thus is linked
 *			with voltage.
 * @vp_errgain:		Error gain value for the voltage processor. This
 *			field also differs according to the voltage/opp.
 */
struct omap_volt_data {
	u32	volt_nominal;
	u32	volt_calibrated;
	u32	volt_dynamic_nominal;
	u32	volt_margin;
	u32	sr_efuse_offs;
	u32	lvt_sr_efuse_offs;
	u8	sr_errminlimit;
	u8	vp_errgain;
	u8	opp_sel;
};
struct voltagedomain;

struct voltagedomain *voltdm_lookup(const char *name);
int voltdm_scale(struct voltagedomain *voltdm,
		 struct omap_volt_data *target_volt);
struct omap_volt_data *omap_voltage_get_curr_vdata(struct voltagedomain *voltdm);

#endif
