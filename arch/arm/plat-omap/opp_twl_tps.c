/*
 * opp_twl_tps.c - TWL/TPS-specific functions for the OPP code
 *
 * Copyright (C) 2009 Texas Instruments Incorporated.
 * Nishanth Menon
 * Copyright (C) 2009 Nokia Corporation
 * Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * XXX This code should be part of some other TWL/TPS code.
 */

#include <linux/i2c/twl.h>

#include <plat/opp_twl_tps.h>

static bool is_offset_valid;
static u8 smps_offset;

#define REG_SMPS_OFFSET		0xE0

/**
 * omap_twl_vsel_to_vdc - convert TWL/TPS VSEL value to microvolts DC
 * @vsel: TWL/TPS VSEL value to convert
 *
 * Returns the microvolts DC that the TWL/TPS family of PMICs should
 * generate when programmed with @vsel.
 */
unsigned long omap_twl_vsel_to_uv(const u8 vsel)
{
	if (twl_class_is_6030()) {
		if (!is_offset_valid) {
			twl_i2c_read_u8(TWL6030_MODULE_ID0, &smps_offset, 0xE0);
			is_offset_valid = true;
		}

		if (smps_offset & 0x8) {
			return ((((vsel - 1) * 125) + 7000)) * 100;
		} else {
			if (vsel == 0x3A)
				return 1350000;
			return ((((vsel - 1) * 125) + 6000)) * 100;
		}
	}

	return (((vsel * 125) + 6000)) * 100;
}

/**
 * omap_twl_uv_to_vsel - convert microvolts DC to TWL/TPS VSEL value
 * @uv: microvolts DC to convert
 *
 * Returns the VSEL value necessary for the TWL/TPS family of PMICs to
 * generate an output voltage equal to or greater than @uv microvolts DC.
 */
u8 omap_twl_uv_to_vsel(unsigned long uv)
{
	/* Round up to higher voltage */
	if (twl_class_is_6030()) {
		if (!is_offset_valid) {
			twl_i2c_read_u8(TWL6030_MODULE_ID0, &smps_offset, 0xE0);
			is_offset_valid = true;
		}

		if (smps_offset & 0x8) {
			return DIV_ROUND_UP(uv - 700000, 12500) + 1;
		} else {
			if (uv == 1350000)
				return 0x3A;
			return DIV_ROUND_UP(uv - 600000, 12500) + 1;
		}
	}

	return DIV_ROUND_UP(uv - 600000, 12500);
}
