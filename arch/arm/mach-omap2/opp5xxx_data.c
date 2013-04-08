/*
 * OMAP5 OPP table definitions.
 *
 * Copyright (C) 2010-2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Vishwanath BS
 *	Nishanth Menon
 *	Kevin Hilman
 *	Thara Gopinath
 *	Andrii Tseglytskyi
 * Copyright (C) 2010-2011 Nokia Corporation.
 *      Eduardo Valentin
 *      Paul Walmsley
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/opp.h>
#include <linux/clk.h>

#include <plat/cpu.h>
#include <plat/omap_device.h>
#include <plat/clock.h>

#include "control.h"
#include "omap_opp_data.h"
#include "pm.h"
#include "abb.h"

/*
 * Structures containing OMAP543X ES2.0 voltage supported and various
 * voltage dependent data for each VDD.
 */

#define OMAP54XX_VDD_MPU_OPP_LOW		 880000
#define OMAP54XX_VDD_MPU_OPP_NOM		1060000
#define OMAP54XX_VDD_MPU_OPP_HIGH		1250000
#define OMAP54XX_VDD_MPU_OPP_SB			1260000

struct omap_volt_data omap543x_vdd_mpu_volt_data[] = {
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MPU_OPP_LOW, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_LOW, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPP_LOW, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MPU_OPP_NOM, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_NOM, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPP_NOM, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MPU_OPP_HIGH, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_HIGH, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPP_HIGH, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MPU_OPP_SB, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_SB, OMAP54XX_CONTROL_FUSE_MPU_LVT_OPP_SB, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	OMAP5_VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

#define OMAP54XX_VDD_MM_OPP_LOW			 880000
#define OMAP54XX_VDD_MM_OPP_NOM			1025000
#define OMAP54XX_VDD_MM_OPP_OD			1120000

struct omap_volt_data omap543x_vdd_mm_volt_data[] = {
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MM_OPP_LOW, OMAP54XX_CONTROL_FUSE_MM_SVT_OPP_LOW, OMAP54XX_CONTROL_FUSE_MM_LVT_OPP_LOW, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MM_OPP_NOM, OMAP54XX_CONTROL_FUSE_MM_SVT_OPP_NOM, OMAP54XX_CONTROL_FUSE_MM_LVT_OPP_NOM, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(OMAP54XX_VDD_MM_OPP_OD, OMAP54XX_CONTROL_FUSE_MM_SVT_OPP_OD, OMAP54XX_CONTROL_FUSE_MM_LVT_OPP_OD, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	OMAP5_VOLT_DATA_DEFINE(0, 0, 0, 0, 0, 0),
};

#define OMAP54XX_VDD_CORE_OPP_NOM		1040000

struct omap_volt_data omap543x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP54XX_VDD_CORE_OPP_NOM, OMAP54XX_CONTROL_FUSE_CORE_OPP_NOM, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

/* Dependency of domains are as follows for OMAP543x (OPP based):
 *	MPU	IVA	CORE
 *	50	50	100
 *	50	100+	100
 *	100+	50	100
 *	100+	100+	100
 */

/* OMAP 5430 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap543x_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP54XX_VDD_MPU_OPP_LOW, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP54XX_VDD_MPU_OPP_NOM, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP54XX_VDD_MPU_OPP_HIGH, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP54XX_VDD_MPU_OPP_SB, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
};
struct omap_vdd_dep_info omap543x_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap543x_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap543x_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 5430 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap543x_vdd_mm_core_dep_data[] = {
	{.main_vdd_volt = OMAP54XX_VDD_MM_OPP_LOW, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP54XX_VDD_MM_OPP_NOM, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP54XX_VDD_MM_OPP_OD, .dep_vdd_volt = OMAP54XX_VDD_CORE_OPP_NOM},
};

struct omap_vdd_dep_info omap543x_vddmm_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap543x_vdd_mm_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap543x_vdd_mm_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

static struct device_info mpu_dev_info = {
	.hwmod_name	= "mpu",
	.clk_name	= "virt_dpll_mpu_ck",
	.voltdm_name	= "mpu",
};

static struct device_info l3_dev_info = {
	.hwmod_name	= "l3_main_1",
	.clk_name	= "virt_l3_ck",
	.voltdm_name	= "core",
};

static struct device_info hsi_dev_info = {
	.hwmod_name	= "hsi",
	.clk_name       = "hsi_fclk",
	.voltdm_name	= "core",
};

static struct device_info ipu_dev_info = {
	.hwmod_name	= "ipu_c0",
	.clk_name	= "dpll_core_h22x2_ck",
	.voltdm_name	= "core",
};
static struct device_info iss_dev_info = {
	.hwmod_name	= "iss",
	.clk_name	= "dpll_core_h23x2_ck",
	.voltdm_name	= "core",
};

static struct device_info fdif_dev_info = {
	.hwmod_name	= "fdif",
	.clk_name	= "fdif_fclk",
	.voltdm_name	= "core",
};

static struct device_info abe_dev_info = {
	.hwmod_name	= "aess",
	.clk_name	= "abe_clk",
	.voltdm_name	= "core",
};

static struct device_info iva_dev_info = {
	.hwmod_name	= "iva",
	.clk_name	= "virt_dpll_iva_ck",
	.voltdm_name	= "mm",
};

static struct device_info dsp_dev_info = {
	.hwmod_name	= "dsp_c0",
	.clk_name	= "virt_dpll_dsp_ck",
	.voltdm_name	= "mm",
};

static struct device_info gpu_dev_info = {
	.hwmod_name	= "gpu",
	.clk_name	= "dpll_core_h14x2_ck",
	.voltdm_name	= "mm",
};

static struct omap_opp_def omap543x_opp_def_list[] = {
	/* MPU OPP1 - OPPLOW */
	OPP_INITIALIZER(&mpu_dev_info, true, 499200000, OMAP54XX_VDD_MPU_OPP_LOW),
	/* MPU OPP2 - OPPNOM: */
	OPP_INITIALIZER(&mpu_dev_info, true, 1000000000, OMAP54XX_VDD_MPU_OPP_NOM),
	/* MPU OPP3 - OPP-HIGH */
	OPP_INITIALIZER(&mpu_dev_info, true, 1500000000, OMAP54XX_VDD_MPU_OPP_HIGH),
	/* MPU OPP4 - OPP-SPEEDBIN */
	OPP_INITIALIZER(&mpu_dev_info, false, 1699200000, OMAP54XX_VDD_MPU_OPP_SB),

	/* L3 OPP2 - OPPNOM */
	OPP_INITIALIZER(&l3_dev_info, true, 265920000, OMAP54XX_VDD_CORE_OPP_NOM),

	/* HSI OPP2 - OPPNOM */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP54XX_VDD_CORE_OPP_NOM),

	/* CORE_IPU_ISS_BOOST_CLK OPP2 - OPPNOM */
	OPP_INITIALIZER(&ipu_dev_info, true, 425500000, OMAP54XX_VDD_CORE_OPP_NOM),

	/* CORE_ISS_MAIN_CLK OPP2 - OPPNOM */
	OPP_INITIALIZER(&iss_dev_info, true, 303900000, OMAP54XX_VDD_CORE_OPP_NOM),

	/* FDIF OPP2 - OPPNOM */
	OPP_INITIALIZER(&fdif_dev_info, true, 256000000, OMAP54XX_VDD_CORE_OPP_NOM),

	/* ABE OPP2 - OPPNOM */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP54XX_VDD_CORE_OPP_NOM),

	/* IVA OPP1 - OPPLOW */
	OPP_INITIALIZER(&iva_dev_info, true, 194100000, OMAP54XX_VDD_MM_OPP_LOW),
	/* IVA OPP2 - OPPNOM */
	OPP_INITIALIZER(&iva_dev_info, true, 388300000, OMAP54XX_VDD_MM_OPP_NOM),
	/* IVA OPP3 - OPP-OD */
	OPP_INITIALIZER(&iva_dev_info, true, 531200000, OMAP54XX_VDD_MM_OPP_OD),

	/* DSP OPP1 - OPPLOW */
	OPP_INITIALIZER(&dsp_dev_info, true, 233000000, OMAP54XX_VDD_MM_OPP_LOW),
	/* DSP OPP2 - OPPNOM */
	OPP_INITIALIZER(&dsp_dev_info, true, 466000000, OMAP54XX_VDD_MM_OPP_NOM),
	/* DSP OPP3 - OPP-OD */
	OPP_INITIALIZER(&dsp_dev_info, true, 531200000, OMAP54XX_VDD_MM_OPP_OD),

	/* NOTE ON SGX OPP: Configuration 1 - From CORE DPLL */
	/* SGX OPP1 - OPPLOW */
	OPP_INITIALIZER(&gpu_dev_info, true, 212700000, OMAP54XX_VDD_MM_OPP_LOW),
	/* SGX OPP2 - OPPNOM */
	OPP_INITIALIZER(&gpu_dev_info, true, 425470000, OMAP54XX_VDD_MM_OPP_NOM),
	/* SGX OPP3 - OPP-OD */
	OPP_INITIALIZER(&gpu_dev_info, true, 531840000, OMAP54XX_VDD_MM_OPP_OD),
};

/* OPP table modification function for use as needed */
static int __init __maybe_unused opp_def_list_modify_opp(
					  struct omap_opp_def *list,
					  unsigned int size,
					  struct device_info *dev_info,
					  unsigned long opp_freq, bool state)
{
	int i;

	for (i = 0; i < size; i++) {
		struct omap_opp_def *entry = &list[i];
		if (entry->dev_info == dev_info && entry->freq == opp_freq) {
			entry->default_available = state;
			return 0;
		}
	}
	WARN(1, "Unable to find opp for %s, frequency %ld\n",
	     dev_info->hwmod_name, opp_freq);

	return -EINVAL;
}

/**
 * omap5_opp_init() - initialize omap4 opp table
 */
int __init omap5_opp_init(void)
{
	int r = -ENODEV;

	if (!cpu_is_omap54xx())
		return r;

	/*
	 * XXX: NOTE: OPP_HIGH is supported on all samples now
	 * TBD: support for runtime enable of MPU OPP_SB
	 */

	r = omap_init_opp_table(omap543x_opp_def_list,
				ARRAY_SIZE(omap543x_opp_def_list));

	return r;
}
