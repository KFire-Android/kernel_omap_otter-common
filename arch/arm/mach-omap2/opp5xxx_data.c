/*
 * OMAP5 OPP table definitions.
 *
 * Copyright (C) 2010-2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Vishwanath BS
 *	Nishanth Menon
 *	Kevin Hilman
 *	Thara Gopinath
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

#include <plat/cpu.h>

#include "control.h"
#include "omap_opp_data.h"
#include "pm.h"

/*
 * Structures containing OMAP5430 voltage supported and various
 * voltage dependent data for each VDD.
 */

#define OMAP5430_VDD_MPU_OPP_LOW		 950000
#define OMAP5430_VDD_MPU_OPP_NOM		1040000
#define OMAP5430_VDD_MPU_OPP_HIGH		1220000
#define OMAP5430_VDD_MPU_OPP_SB			1220000

struct omap_volt_data omap54xx_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_LOW, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_LOW, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_NOM, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_NOM, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_HIGH, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_HIGH, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP5430_VDD_MPU_OPP_SB, OMAP54XX_CONTROL_FUSE_MPU_SVT_OPP_SB, 0xfa, 0x27),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP5430_VDD_MM_OPP_LOW			 950000
#define OMAP5430_VDD_MM_OPP_NOM			1040000
#define OMAP5430_VDD_MM_OPP_OD			1220000

struct omap_volt_data omap54xx_vdd_mm_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP5430_VDD_MM_OPP_LOW, OMAP54XX_CONTROL_FUSE_MM_SVT_OPP_LOW, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP5430_VDD_MM_OPP_NOM, OMAP54XX_CONTROL_FUSE_MM_SVT_OPP_NOM, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP5430_VDD_MM_OPP_OD, OMAP54XX_CONTROL_FUSE_MM_SVT_OPP_OD, 0xfa, 0x23),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP5430_VDD_CORE_OPP_LOW		 950000
#define OMAP5430_VDD_CORE_OPP_NOM		1040000

struct omap_volt_data omap54xx_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP5430_VDD_CORE_OPP_LOW, OMAP54XX_CONTROL_FUSE_CORE_OPP_LOW, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP5430_VDD_CORE_OPP_NOM, OMAP54XX_CONTROL_FUSE_CORE_OPP_NOM, 0xf9, 0x16),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

/* Dependency of domains are as follows for OMAP5430 (OPP based):
 *
 *	MPU	IVA	CORE
 *	50	50	50+
 *	50	100+	100
 *	100+	50	100
 *	100+	100+	100
 */

/* OMAP 5430 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap54xx_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_LOW, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_LOW},
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_NOM, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_HIGH, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP5430_VDD_MPU_OPP_SB, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
};
struct omap_vdd_dep_info omap54xx_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap54xx_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap54xx_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 5430 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap54xx_vdd_mm_core_dep_data[] = {
	{.main_vdd_volt = OMAP5430_VDD_MM_OPP_LOW, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_LOW},
	{.main_vdd_volt = OMAP5430_VDD_MM_OPP_NOM, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
	{.main_vdd_volt = OMAP5430_VDD_MM_OPP_OD, .dep_vdd_volt = OMAP5430_VDD_CORE_OPP_NOM},
};

struct omap_vdd_dep_info omap54xx_vddmm_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap54xx_vdd_mm_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap54xx_vdd_mm_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

static struct device_info mpu_dev_info = {
	.hwmod_name	= "mpu",
	.clk_name	= "dpll_mpu_ck",
	.voltdm_name	= "mpu",
};

static struct device_info l3_dev_info = {
	.hwmod_name	= "l3_main_1",
	.clk_name	= "virt_l3_ck",
	.voltdm_name	= "core",
};

static struct device_info hsi_dev_info = {
	.hwmod_name	= "hsi",
	.clk_name	= "hsi_fck",
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

static struct device_info mmc1_dev_info = {
	.hwmod_name	= "mmc1",
	.clk_name	= "mmc1_fck",
	.voltdm_name	= "core",
};

static struct device_info mmc2_dev_info = {
	.hwmod_name	= "mmc2",
	.clk_name	= "mmc2_fck",
	.voltdm_name	= "core",
};


static struct device_info iva_dev_info = {
	.hwmod_name	= "iva",
	.clk_name	= "dpll_iva_h12x2_ck",
	.voltdm_name	= "mm",
};

static struct device_info dsp_dev_info = {
	.hwmod_name	= "dsp_c0",
	.clk_name	= "dpll_iva_h11x2_ck",
	.voltdm_name	= "mm",
};

static struct device_info gpu_dev_info = {
	.hwmod_name	= "gpu",
	.clk_name	= "dpll_per_h14x2_ck",
	.voltdm_name	= "mm",
};

static struct omap_opp_def __initdata omap54xx_opp_def_list[] = {
#ifdef CONFIG_MACH_OMAP_5430ZEBU
	/* MPU OPP1 - OPPLOW */
	OPP_INITIALIZER(&mpu_dev_info, true, 550000000, OMAP5430_VDD_MPU_OPP_LOW),
	/* MPU OPP2 - OPPNOM */
	OPP_INITIALIZER(&mpu_dev_info, true, 1100000000, OMAP5430_VDD_MPU_OPP_NOM),
	/* MPU OPP3 - OPP-HIGH */
	OPP_INITIALIZER(&mpu_dev_info, false, 1500000000, OMAP5430_VDD_MPU_OPP_HIGH),
#else
	/* MPU OPP1 - OPPLOW */
	OPP_INITIALIZER(&mpu_dev_info, true, 400000000, OMAP5430_VDD_MPU_OPP_LOW),
	/* MPU OPP2 - OPPNOM */
	OPP_INITIALIZER(&mpu_dev_info, true, 800000000, OMAP5430_VDD_MPU_OPP_NOM),
	/* MPU OPP3 - OPP-HIGH */
	OPP_INITIALIZER(&mpu_dev_info, false, 1100000000, OMAP5430_VDD_MPU_OPP_HIGH),
#endif
	/* L3 OPP1 - OPPLOW */
	OPP_INITIALIZER(&l3_dev_info, true, 133000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* L3 OPP2 - OPPNOM */
	OPP_INITIALIZER(&l3_dev_info, true, 266000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* HSI OPP1 - OPPLOW */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* HSI OPP3 - OPPNOM */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* CORE_IPU_ISS_BOOST_CLK OPP1 - OPPLOW */
	OPP_INITIALIZER(&ipu_dev_info, true, 212800000, OMAP5430_VDD_CORE_OPP_LOW),
	/* CORE_IPU_ISS_BOOST_CLK OPP2 - OPPNOM */
	OPP_INITIALIZER(&ipu_dev_info, true, 425600000, OMAP5430_VDD_CORE_OPP_NOM),

	/* CORE_ISS_MAIN_CLK OPP1 - OPPLOW */
	OPP_INITIALIZER(&iss_dev_info, true, 152000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* CORE_ISS_MAIN_CLK OPP2 - OPPNOM */
	OPP_INITIALIZER(&iss_dev_info, true, 304000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* FDIF OPP1 - OPPLOW */
	OPP_INITIALIZER(&fdif_dev_info, true, 64000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* FDIF OPP3 - OPPNOM */
	OPP_INITIALIZER(&fdif_dev_info, true, 128000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* ABE OPP1 - OPPLOW */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP5430_VDD_CORE_OPP_LOW),
	/* ABE OPP2 - OPPNOM */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP5430_VDD_CORE_OPP_NOM),

	/* MMC1 OPP1 - OPPLOW */
	OPP_INITIALIZER(&mmc1_dev_info, true, 96000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* MMC1 OPP2 - OPPNOM */
	OPP_INITIALIZER(&mmc1_dev_info, true, 192000000, OMAP5430_VDD_CORE_OPP_NOM),

	/* MMC2 OPP1 - OPPLOW */
	OPP_INITIALIZER(&mmc2_dev_info, true, 96000000, OMAP5430_VDD_CORE_OPP_LOW),
	/* MMC2 OPP2 - OPPNOM */
	OPP_INITIALIZER(&mmc2_dev_info, true, 192000000, OMAP5430_VDD_CORE_OPP_NOM),


	/* IVA OPP1 - OPPLOW */
	OPP_INITIALIZER(&iva_dev_info, true, 194200000, OMAP5430_VDD_MM_OPP_LOW),
	/* IVA OPP2 - OPPNOM */
	OPP_INITIALIZER(&iva_dev_info, true, 388300000, OMAP5430_VDD_MM_OPP_NOM),
	/* IVA OPP3 - OPP-OD */
	OPP_INITIALIZER(&iva_dev_info, false, 532000000, OMAP5430_VDD_MM_OPP_OD),

	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER(&dsp_dev_info, true, 233000000, OMAP5430_VDD_MM_OPP_LOW),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER(&dsp_dev_info, true, 466000000, OMAP5430_VDD_MM_OPP_NOM),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER(&dsp_dev_info, false, 532000000, OMAP5430_VDD_MM_OPP_OD),

	/* SGX OPP1 - OPPLOW */
	OPP_INITIALIZER(&gpu_dev_info, true, 192000000, OMAP5430_VDD_MM_OPP_LOW),
	/* SGX OPP2 - OPPNOM */
	OPP_INITIALIZER(&gpu_dev_info, true, 384000000, OMAP5430_VDD_MM_OPP_NOM),
	/* SGX OPP3 - OPPOV */
	OPP_INITIALIZER(&gpu_dev_info, false, 532000000, OMAP5430_VDD_MM_OPP_OD),
};

/**
 * omap5_opp_init() - initialize omap4 opp table
 */
static int __init omap5_opp_init(void)
{
	int r = -ENODEV;

	if (!cpu_is_omap54xx())
		return r;

	r = omap_init_opp_table(omap54xx_opp_def_list,
			ARRAY_SIZE(omap54xx_opp_def_list));

	return r;
}
device_initcall(omap5_opp_init);
