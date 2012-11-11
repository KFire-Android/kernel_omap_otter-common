/*
 * OMAP4 OPP table definitions.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
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
#include <linux/clk.h>

#include <plat/cpu.h>

#include "control.h"
#include "omap_opp_data.h"
#include "pm.h"

/*
 * Structures containing OMAP4430 voltage supported and various
 * voltage dependent data for each VDD.
 */

#define OMAP4430_VDD_MPU_OPP50_UV		1025000
#define OMAP4430_VDD_MPU_OPP100_UV		1200000
#define OMAP4430_VDD_MPU_OPPTURBO_UV		1325000
#define OMAP4430_VDD_MPU_OPPNITRO_UV		1388000

struct omap_volt_data omap443x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPP50_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPP100_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP4430_VDD_IVA_OPP50_UV		950000
#define OMAP4430_VDD_IVA_OPP100_UV		1114000
#define OMAP4430_VDD_IVA_OPPTURBO_UV		1291000

struct omap_volt_data omap443x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPP50_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPP100_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP4430_VDD_CORE_OPP50_UV		962000
#define OMAP4430_VDD_CORE_OPP100_UV		1127000

struct omap_volt_data omap443x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_CORE_OPP50_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4430_VDD_CORE_OPP100_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

/* Dependency of domains are as follows for OMAP4430 (OPP based):
 *
 *	MPU	IVA	CORE
 *	50	50	50+
 *	50	100+	100
 *	100+	50	100
 *	100+	100+	100
 */

/* OMAP 4430 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap443x_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPP50_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPP100_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPPTURBO_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPPNITRO_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap443x_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap443x_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap443x_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 4430 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap443x_vdd_iva_core_dep_data[] = {
	{.main_vdd_volt = OMAP4430_VDD_IVA_OPP50_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4430_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4430_VDD_IVA_OPPTURBO_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap443x_vddiva_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap443x_vdd_iva_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap443x_vdd_iva_core_dep_data),
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

static struct device_info iva_dev_info = {
	.hwmod_name	= "iva",
	.clk_name	= "virt_dpll_iva_ck",
	.voltdm_name	= "iva",
};

static struct device_info abe_dev_info = {
	.hwmod_name	= "aess",
	.clk_name	= "abe_clk",
	.voltdm_name	= "iva",
};

static struct device_info hsi_dev_info = {
	.hwmod_name	= "hsi",
	.clk_name	= "hsi_fck",
	.voltdm_name	= "core",
};

static struct device_info gpu_dev_info = {
	.hwmod_name	= "gpu",
	.clk_name	= "dpll_per_m7x2_ck",
	.voltdm_name	= "core",
};

static struct device_info fdif_dev_info = {
	.hwmod_name	= "fdif",
	.clk_name	= "fdif_fck",
	.voltdm_name	= "core",
};

static struct omap_opp_def __initdata omap443x_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&mpu_dev_info, true, 300000000, OMAP4430_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&mpu_dev_info, true, 600000000, OMAP4430_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&mpu_dev_info, true, 800000000, OMAP4430_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-NT. False while ABB is not added */
	OPP_INITIALIZER(&mpu_dev_info, false, 1008000000, OMAP4430_VDD_MPU_OPPNITRO_UV),
	/* L3 OPP1 - OPP50 */
	OPP_INITIALIZER(&l3_dev_info, true, 100000000, OMAP4430_VDD_CORE_OPP50_UV),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OPP_INITIALIZER(&l3_dev_info, true, 200000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* IVA OPP1 - OPP50 */
	OPP_INITIALIZER(&iva_dev_info, true, 133000000, OMAP4430_VDD_IVA_OPP50_UV),
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER(&iva_dev_info, true, 266100000, OMAP4430_VDD_IVA_OPP100_UV),
	/* IVA OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&iva_dev_info, false, 332000000, OMAP4430_VDD_IVA_OPPTURBO_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4430_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4430_VDD_IVA_OPP100_UV),
	/* FDIF OPP1 - OPP50 */
	OPP_INITIALIZER(&fdif_dev_info, true, 64000000, OMAP4430_VDD_CORE_OPP50_UV),
	/* FDIF OPP2 - OPP100 */
	OPP_INITIALIZER(&fdif_dev_info, true, 128000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4430_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* SGX OPP1 - OPP50 */
	OPP_INITIALIZER(&gpu_dev_info, true, 153600000, OMAP4430_VDD_CORE_OPP50_UV),
	/* SGX OPP2 - OPP100 */
	OPP_INITIALIZER(&gpu_dev_info, true, 307200000, OMAP4430_VDD_CORE_OPP100_UV),
	/* TODO: add DSP */
};

#define OMAP4460_VDD_MPU_OPP50_UV		1025000
#define OMAP4460_VDD_MPU_OPP100_UV		1203000
#define OMAP4460_VDD_MPU_OPPTURBO_UV		1317000
#define OMAP4460_VDD_MPU_OPPNITRO_UV		1380000
#define OMAP4460_VDD_MPU_OPPNITROSB_UV		1390000

struct omap_volt_data omap446x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_MPU_OPP50_UV, 10000, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPP100_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITROSB, 0xfa, 0x27),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP4460_VDD_IVA_OPP50_UV		 950000
#define OMAP4460_VDD_IVA_OPP100_UV		1140000
#define OMAP4460_VDD_IVA_OPPTURBO_UV		1291000
#define OMAP4460_VDD_IVA_OPPNITRO_UV		1375000
#define OMAP4460_VDD_IVA_OPPNITROSB_UV		1376000

struct omap_volt_data omap446x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_IVA_OPP50_UV, 13000, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPP100_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITRO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITROSB, 0xfa, 0x23),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP4460_VDD_CORE_OPP50_UV		 962000
#define OMAP4460_VDD_CORE_OPP100_UV		1127000
#define OMAP4460_VDD_CORE_OPP100_OV_UV		1250000

struct omap_volt_data omap446x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_CORE_OPP50_UV, 38000, OMAP44XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_CORE_OPP100_UV, 13000, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_CORE_OPP100_OV_UV, 13000, OMAP44XX_CONTROL_FUSE_CORE_OPP100OV, 0xf9, 0x16),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

/* OMAP 4460 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap446x_vdd_mpu_core_dep_data[] = {
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPP50_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPP100_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPPTURBO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPPNITRO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_MPU_OPPNITROSB_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap446x_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap446x_vdd_mpu_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap446x_vdd_mpu_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 4460 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap446x_vdd_iva_core_dep_data[] = {
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPP50_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPPTURBO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPPNITRO_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4460_VDD_IVA_OPPNITROSB_UV, .dep_vdd_volt = OMAP4460_VDD_CORE_OPP100_UV},
};

struct omap_vdd_dep_info omap446x_vddiva_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap446x_vdd_iva_core_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap446x_vdd_iva_core_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

static struct device_info omap4460_mpu_dev_info = {
	.hwmod_name	= "mpu",
	.clk_name	= "virt_dpll_mpu_ck",
	.voltdm_name	= "mpu",
};

static struct omap_opp_def __initdata omap446x_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 350000000, OMAP4460_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 700000000, OMAP4460_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 920000000, OMAP4460_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1200000000, OMAP4460_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP4 - OPP-Nitro SpeedBin */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1500000000, OMAP4460_VDD_MPU_OPPNITROSB_UV),
	/* L3 OPP1 - OPP50 */
	OPP_INITIALIZER(&l3_dev_info, true, 100000000, OMAP4460_VDD_CORE_OPP50_UV),
	/* L3 OPP2 - OPP100 */
	OPP_INITIALIZER(&l3_dev_info, true, 200000000, OMAP4460_VDD_CORE_OPP100_UV),
	/* IVA OPP1 - OPP50 */
	OPP_INITIALIZER(&iva_dev_info, true, 133000000, OMAP4460_VDD_IVA_OPP50_UV),
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER(&iva_dev_info, true, 266100000, OMAP4460_VDD_IVA_OPP100_UV),
	/* IVA OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&iva_dev_info, true, 332000000, OMAP4460_VDD_IVA_OPPTURBO_UV),
	/* IVA OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&iva_dev_info, false, 430000000, OMAP4460_VDD_IVA_OPPNITRO_UV),
	/* IVA OPP5 - OPP-Nitro SpeedBin*/
	OPP_INITIALIZER(&iva_dev_info, false, 500000000, OMAP4460_VDD_IVA_OPPNITROSB_UV),
	/* FDIF OPP1 - OPP50 */
	OPP_INITIALIZER(&fdif_dev_info, true, 64000000, OMAP4460_VDD_CORE_OPP50_UV),
	/* FDIF OPP2 - OPP100 */
	OPP_INITIALIZER(&fdif_dev_info, true, 128000000, OMAP4460_VDD_CORE_OPP100_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4460_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4460_VDD_CORE_OPP100_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4460_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4460_VDD_IVA_OPP100_UV),
	/* SGX OPP1 - OPP50 */
	OPP_INITIALIZER(&gpu_dev_info, true, 153600000, OMAP4460_VDD_CORE_OPP50_UV),
	/* SGX OPP2 - OPP100 */
	OPP_INITIALIZER(&gpu_dev_info, true, 307200000, OMAP4460_VDD_CORE_OPP100_UV),
	/* SGX OPP3 - OPP100_OV */
	OPP_INITIALIZER(&gpu_dev_info, true, 384000000, OMAP4460_VDD_CORE_OPP100_OV_UV),
	/* TODO: Add DSP, DSS */
};

/*
 * Structures containing OMAP4470 voltage supported and various
 * voltage dependent data for each VDD.
 */
#define OMAP4470_VDD_MPU_OPP50_UV		1037000
#define OMAP4470_VDD_MPU_OPP100_UV		1200000
#define OMAP4470_VDD_MPU_OPPTURBO_UV		1312000
#define OMAP4470_VDD_MPU_OPPNITRO_UV		1375000
#define OMAP4470_VDD_MPU_OPPNITROSB_UV		1387000

struct omap_volt_data omap447x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPP50_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPP100_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITROSB, 0xfa, 0x27),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP4470_VDD_IVA_OPP50_UV		 962000
#define OMAP4470_VDD_IVA_OPP100_UV		1137000
#define OMAP4470_VDD_IVA_OPPTURBO_UV		1287000
#define OMAP4470_VDD_IVA_OPPNITRO_UV		1375000
#define OMAP4470_VDD_IVA_OPPNITROSB_UV		1380000

struct omap_volt_data omap447x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPP50_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPP100_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITRO, 0xfa, 0x23),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITROSB, 0xfa, 0x23),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

#define OMAP4470_VDD_CORE_OPP50_UV		 925000
#define OMAP4470_VDD_CORE_OPP100_UV		1126000
#define OMAP4470_VDD_CORE_OPP100_OV_UV		1190000

struct omap_volt_data omap447x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4470_VDD_CORE_OPP50_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c),
	VOLT_DATA_DEFINE(OMAP4470_VDD_CORE_OPP100_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16),
	VOLT_DATA_DEFINE(OMAP4470_VDD_CORE_OPP100_OV_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP100OV, 0xf9, 0x16),
	VOLT_DATA_DEFINE(0, 0, 0, 0),
};

/* OMAP 4470 MPU Core VDD dependency table */
static struct omap_vdd_dep_volt omap447x_vdd_mpu_core_low_dep_data[] = {
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPP50_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPP100_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPPTURBO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPPNITRO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPPNITROSB_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
};

static struct omap_vdd_dep_volt omap447x_vdd_mpu_core_high_dep_data[] = {
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPP50_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPP100_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPPTURBO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPPNITRO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
	{.main_vdd_volt = OMAP4470_VDD_MPU_OPPNITROSB_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
};

struct omap_vdd_dep_info omap447x_vddmpu_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap447x_vdd_mpu_core_high_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap447x_vdd_mpu_core_high_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

/* OMAP 4470 MPU IVA VDD dependency table */
static struct omap_vdd_dep_volt omap447x_vdd_iva_core_low_dep_data[] = {
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPP50_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPPTURBO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPPNITRO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPPNITROSB_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
};

static struct omap_vdd_dep_volt omap447x_vdd_iva_core_high_dep_data[] = {
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPP50_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP50_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPPTURBO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPPNITRO_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPPNITROSB_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_OV_UV},
};

struct omap_vdd_dep_info omap447x_vddiva_dep_info[] = {
	{
		.name	= "core",
		.dep_table = omap447x_vdd_iva_core_high_dep_data,
		.nr_dep_entries = ARRAY_SIZE(omap447x_vdd_iva_core_high_dep_data),
	},
	{.name = NULL, .dep_table = NULL, .nr_dep_entries = 0},
};

static struct device_info bb2d_dev_info = {
	.hwmod_name	= "bb2d",
	.clk_name	= "dpll_per_m6x2_ck",
	.voltdm_name	= "core",
};

#define OMAP4470_LP_OPP_SET_CORE_DPLL_FREQ 1600000000

static struct omap_opp_def __initdata omap447x_opp_low_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 396800000, OMAP4470_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 800000000, OMAP4470_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo. False while ABB and DCC are not supported */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1100000000, OMAP4470_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-Nitro. False while ABB and DCC are not supported */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1300000000, OMAP4470_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP4 - OPP-Nitro SpeedBin */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1500000000, OMAP4470_VDD_MPU_OPPNITROSB_UV),
	/* L3 OPP1 - OPP50 */
	OPP_INITIALIZER(&l3_dev_info, true, 100000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* L3 OPP2 - OPP100 */
	OPP_INITIALIZER(&l3_dev_info, true, 200000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* IVA OPP1 - OPP50 */
	OPP_INITIALIZER(&iva_dev_info, true, 133000000, OMAP4470_VDD_IVA_OPP50_UV),
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER(&iva_dev_info, true, 266100000, OMAP4470_VDD_IVA_OPP100_UV),
	/* IVA OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&iva_dev_info, true, 332000000, OMAP4470_VDD_IVA_OPPTURBO_UV),
	/* IVA OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&iva_dev_info, false, 430000000, OMAP4470_VDD_IVA_OPPNITRO_UV),
	/* IVA OPP5 - OPP-Nitro SpeedBin */
	OPP_INITIALIZER(&iva_dev_info, false, 500000000, OMAP4470_VDD_IVA_OPPNITROSB_UV),
	/* SGX OPP1 - OPP50 */
	OPP_INITIALIZER(&gpu_dev_info, true, 153600000, OMAP4470_VDD_CORE_OPP50_UV),
	/* SGX OPP2 - OPP100 */
	OPP_INITIALIZER(&gpu_dev_info, true, 307200000, OMAP4470_VDD_CORE_OPP100_UV),
	/* SGX OPP3 - OPPOV */
	OPP_INITIALIZER(&gpu_dev_info, true, 384000000, OMAP4470_VDD_CORE_OPP100_OV_UV),
	/* BB2D OPP1 - OPP50 */
	OPP_INITIALIZER(&bb2d_dev_info, true, 153600000, OMAP4470_VDD_CORE_OPP50_UV),
	/* BB2D OPP2 - OPP100 */
	OPP_INITIALIZER(&bb2d_dev_info, true, 307200000, OMAP4470_VDD_CORE_OPP100_UV),
	/* BB2D OPP3 - OPP-OV */
	OPP_INITIALIZER(&bb2d_dev_info, true, 384000000, OMAP4470_VDD_CORE_OPP100_OV_UV),
	/* FDIF OPP1 - OPP25 */
	OPP_INITIALIZER(&fdif_dev_info, true, 32000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* FDIF OPP2 - OPP50 */
	OPP_INITIALIZER(&fdif_dev_info, true, 64000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* FDIF OPP3 - OPP100 */
	OPP_INITIALIZER(&fdif_dev_info, true, 128000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4470_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4470_VDD_IVA_OPP100_UV),
	/* TODO: add DSP */
};

static struct omap_opp_def __initdata omap447x_opp_high_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 396800000, OMAP4470_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 800000000, OMAP4470_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo. False while ABB and DCC are not supported */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1100000000, OMAP4470_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-Nitro. False while ABB and DCC are not supported */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1300000000, OMAP4470_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP4 - OPP-Nitro SpeedBin */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, false, 1500000000, OMAP4470_VDD_MPU_OPPNITROSB_UV),
	/* L3 OPP1 - OPP50 */
	OPP_INITIALIZER(&l3_dev_info, true, 116000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* L3 OPP2 - OPP-OV */
	OPP_INITIALIZER(&l3_dev_info, true, 233000000, OMAP4470_VDD_CORE_OPP100_OV_UV),
	/* IVA OPP1 - OPP50 */
	OPP_INITIALIZER(&iva_dev_info, true, 133000000, OMAP4470_VDD_IVA_OPP50_UV),
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER(&iva_dev_info, true, 266100000, OMAP4470_VDD_IVA_OPP100_UV),
	/* IVA OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&iva_dev_info, true, 332000000, OMAP4470_VDD_IVA_OPPTURBO_UV),
	/* IVA OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&iva_dev_info, false, 430000000, OMAP4470_VDD_IVA_OPPNITRO_UV),
	/* IVA OPP5 - OPP-Nitro SpeedBin */
	OPP_INITIALIZER(&iva_dev_info, false, 500000000, OMAP4470_VDD_IVA_OPPNITROSB_UV),
	/* SGX OPP1 - OPP50 */
	OPP_INITIALIZER(&gpu_dev_info, true, 192000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* SGX OPP2 - OPP100 */
	OPP_INITIALIZER(&gpu_dev_info, true, 307200000, OMAP4470_VDD_CORE_OPP100_UV),
	/* SGX OPP3 - OPPOV */
	OPP_INITIALIZER(&gpu_dev_info, true, 384000000, OMAP4470_VDD_CORE_OPP100_OV_UV),
	/* BB2D OPP1 - OPP50 */
	OPP_INITIALIZER(&bb2d_dev_info, true, 192000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* BB2D OPP2 - OPP100 */
	OPP_INITIALIZER(&bb2d_dev_info, true, 307200000, OMAP4470_VDD_CORE_OPP100_UV),
	/* BB2D OPP3 - OPP-OV */
	OPP_INITIALIZER(&bb2d_dev_info, true, 384000000, OMAP4470_VDD_CORE_OPP100_OV_UV),
	/* FDIF OPP1 - OPP50 */
	OPP_INITIALIZER(&fdif_dev_info, true, 64000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* FDIF OPP2 - OPP100 */
	OPP_INITIALIZER(&fdif_dev_info, true, 128000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4470_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4470_VDD_IVA_OPP100_UV),
	/* TODO: add DSP */
};

/*
 * omap4_replace_dep_table() - replace the dep_table for dep_info by vdd name
 */
static void omap4_replace_dep_table(char *vdd_name,
					 struct omap_vdd_dep_info *dep_info,
					 struct omap_vdd_dep_volt *dep_table)
{
	while (dep_info->name) {
		if (!strcmp(vdd_name, dep_info->name)) {
			dep_info->dep_table = dep_table;
			break;
		}
		dep_info++;
	}
}

/**
 * omap4_opp_init() - initialize omap4 opp table
 */
int __init omap4_opp_init(void)
{
	int r = -ENODEV;

	if (!cpu_is_omap44xx())
		return r;

	if (cpu_is_omap443x())
		r = omap_init_opp_table(omap443x_opp_def_list,
			ARRAY_SIZE(omap443x_opp_def_list));
	else if (cpu_is_omap446x())
		r = omap_init_opp_table(omap446x_opp_def_list,
			ARRAY_SIZE(omap446x_opp_def_list));
	else if (cpu_is_omap447x()) {
		struct clk *dpll_core_ck;
		unsigned long rate = 0;
		dpll_core_ck = clk_get(NULL, "dpll_core_ck");
		BUG_ON(IS_ERR_OR_NULL(dpll_core_ck));

		rate = clk_get_rate(dpll_core_ck);
		clk_put(dpll_core_ck);

		if (rate > OMAP4470_LP_OPP_SET_CORE_DPLL_FREQ / 2)
			r = omap_init_opp_table(omap447x_opp_high_def_list,
					ARRAY_SIZE(omap447x_opp_high_def_list));
		else {
			omap4_replace_dep_table("core",
					omap447x_vddmpu_dep_info,
					omap447x_vdd_mpu_core_low_dep_data);
			omap4_replace_dep_table("core",
					omap447x_vddiva_dep_info,
					omap447x_vdd_iva_core_low_dep_data);
			r = omap_init_opp_table(omap447x_opp_low_def_list,
					ARRAY_SIZE(omap447x_opp_low_def_list));
		}
	}

	return r;
}
device_initcall(omap4_opp_init);
