/*
 * Helper module for board specific I2C bus registration
 *
 * Copyright (C) 2009 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <plat/cpu.h>
#include <plat/i2c.h>
#include "common.h"
#include <plat/omap_hwmod.h>

#include "mux.h"

/* In register I2C_CON, Bit 15 is the I2C enable bit */
#define I2C_EN					BIT(15)
#define OMAP2_I2C_CON_OFFSET			0x24
#define OMAP4_I2C_CON_OFFSET			0xA4

/* Maximum microseconds to wait for OMAP module to softreset */
#define MAX_MODULE_SOFTRESET_WAIT	10000

void __init omap2_i2c_mux_pins(int bus_id)
{
	char mux_name[sizeof("i2c2_scl.i2c2_scl")];

	/* First I2C bus is not muxable */
	if (bus_id == 1)
		return;

	sprintf(mux_name, "i2c%i_scl.i2c%i_scl", bus_id, bus_id);
	omap_mux_init_signal(mux_name, OMAP_PIN_INPUT);
	sprintf(mux_name, "i2c%i_sda.i2c%i_sda", bus_id, bus_id);
	omap_mux_init_signal(mux_name, OMAP_PIN_INPUT);
}

/**
 * omap_i2c_reset - reset the omap i2c module.
 * @oh: struct omap_hwmod *
 *
 * The i2c moudle in omap2, omap3 had a special sequence to reset. The
 * sequence is:
 * - Disable the I2C.
 * - Write to SOFTRESET bit.
 * - Enable the I2C.
 * - Poll on the RESETDONE bit.
 * The sequence is implemented in below function. This is called for 2420,
 * 2430 and omap3.
 */
int omap_i2c_reset(struct omap_hwmod *oh)
{
	u32 v;
	u16 i2c_con;
	int c = 0;

	if (oh->class->rev == OMAP_I2C_IP_VERSION_2) {
		i2c_con = OMAP4_I2C_CON_OFFSET;
	} else if (oh->class->rev == OMAP_I2C_IP_VERSION_1) {
		i2c_con = OMAP2_I2C_CON_OFFSET;
	} else {
		WARN(1, "Cannot reset I2C block %s: unsupported revision\n",
		     oh->name);
		return -EINVAL;
	}

	/* Disable I2C */
	v = omap_hwmod_read(oh, i2c_con);
	v &= ~I2C_EN;
	omap_hwmod_write(v, oh, i2c_con);

	/* Write to the SOFTRESET bit */
	omap_hwmod_softreset(oh);

	/* Enable I2C */
	v = omap_hwmod_read(oh, i2c_con);
	v |= I2C_EN;
	omap_hwmod_write(v, oh, i2c_con);

	/* Poll on RESETDONE bit */
	omap_test_timeout((omap_hwmod_read(oh,
				oh->class->sysc->syss_offs)
				& SYSS_RESETDONE_MASK),
				MAX_MODULE_SOFTRESET_WAIT, c);

	if (c == MAX_MODULE_SOFTRESET_WAIT)
		pr_warning("%s: %s: softreset failed (waited %d usec)\n",
			__func__, oh->name, MAX_MODULE_SOFTRESET_WAIT);
	else
		pr_debug("%s: %s: softreset in %d usec\n", __func__,
			oh->name, c);

	return 0;
}

/**
 * OMAP5 I2C weak pull-up (pad pull-up) enable/disable handling API
 *
 * omap5_i2c_weak_pullup - setup weak pull-ups for I2C lines
 * @bus_id: bus id counting from number 1
 * @enable: enable weak pull-up - true, disable - false
 *
 */
void omap5_i2c_weak_pullup(int bus_id, bool enable)
{
	u16 r_scl = 0, r_sda = 0;
	struct omap_mux_partition *p_scl = NULL, *p_sda = NULL;
	struct omap_mux *mux_scl = NULL, *mux_sda = NULL;

	if (!cpu_is_omap54xx())
		return;

	if (bus_id < 1 || bus_id > 5) {
		pr_err("%s:Incorrect I2C port (%d)\n",
				__func__, bus_id);
		return;
	}

	switch (bus_id) {
	case 1:
		omap_mux_get_by_name("i2c1_pmic_scl", &p_scl, &mux_scl);
		omap_mux_get_by_name("i2c1_pmic_sda", &p_sda, &mux_sda);
		break;
	case 2:
		omap_mux_get_by_name("i2c2_scl", &p_scl, &mux_scl);
		omap_mux_get_by_name("i2c2_sda", &p_sda, &mux_sda);
		break;
	case 3:
		omap_mux_get_by_name("i2c3_scl", &p_scl, &mux_scl);
		omap_mux_get_by_name("i2c3_sda", &p_sda, &mux_sda);
		break;
	case 4:
		omap_mux_get_by_name("i2c4_scl", &p_scl, &mux_scl);
		omap_mux_get_by_name("i2c4_sda", &p_sda, &mux_sda);
		break;
	case 5:
		omap_mux_get_by_name("i2c5_scl", &p_scl, &mux_scl);
		omap_mux_get_by_name("i2c5_sda", &p_sda, &mux_sda);
		break;
	default:
		break;
	}
	if (p_scl && p_sda && mux_scl && mux_sda) {
		r_scl = omap_mux_read(p_scl, mux_scl->reg_offset);
		r_sda = omap_mux_read(p_sda, mux_sda->reg_offset);
		if (enable) {
			r_scl |= 0x1 << 3;
			r_sda |= 0x1 << 3;
		} else {
			r_scl &= ~(0x1 << 3);
			r_sda &= ~(0x1 << 3);
		}
		omap_mux_write(p_scl, r_scl, mux_scl->reg_offset);
		omap_mux_write(p_sda, r_sda, mux_sda->reg_offset);
	} else {
		pr_err("%s:Failed to configure mux for weak pull-up for"
			"I2C port (%d)\n", __func__, bus_id);
	}
}
