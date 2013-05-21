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
#include "abb.h"

/*
 * STD_FUSE_OPP_DPLL_1 contains info about ABB trim type for MPU/IVA.
 * This probably is an ugly location to put the DPLL trim details.. but,
 * alternatives are even less attractive :( shrug..
 *
 * CONTROL_STD_FUSE_OPP_DPLL_1 bit fields:
 * Bit #|       Name       |        Description     |        Comment          |  Device
 * -----+------------------+------------------------+-------------------------|----------
 * 18-19|MPU_DPLL_TRIM_FREQ| 0 - 2.0GHz             | If RBB is not trimmed,  | OMAP4460
 *      |                  | 1 - 2.4GHz             | but MPU DPLL is trimmed |
 *      |                  | 2 - Reserved           | to 2.4GHz of higher,    |
 *      |                  | 3 - 3.0GHz             | it is recommended to    |
 *      |                  |                        | enable FBB for MPU at   |
 *      |                  |                        | OPPTB and OPPNT         |
 * -----+------------------+------------------------+-------------------------|----------
 *  20  |    MPU_RBB_TB    | 0 - RBB is trimmed     | If trimmed RBB can be   | OMAP4460/
 *      |                  | 1 - RBB is not trimmed | enabled at OPPTB on MPU | OMAP4470
 * -----+------------------+                        +-------------------------|----------
 *  21  |    IVA_RBB_TB    |                        | If trimmed RBB can be   | OMAP4460/
 *      |                  |                        | enabled at OPPTB on IVA | OMAP4470
 * -----+------------------+                        +-------------------------|----------
 *  22  |    MPU_RBB_NT    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPNT on MPU |
 * -----+------------------+                        +-------------------------|----------
 *  23  |    MPU_RBB_SB    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPSB on MPU |
 * -----+------------------+                        +-------------------------|----------
 *  24  |    IVA_RBB_NT    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPNT on IVA |
 * -----+------------------+                        +-------------------------|----------
 *  25  |    IVA_RBB_SB    |                        | If trimmed RBB can be   | OMAP4470
 *      |                  |                        | enabled at OPPSB on IVA |
 */
#define OMAP4460_MPU_OPP_DPLL_TRIM		BIT(18)
#define OMAP4460_MPU_OPP_DPLL_TURBO_RBB		BIT(20)
#define OMAP4460_IVA_OPP_DPLL_TURBO_RBB		BIT(21)
#define OMAP4470_MPU_OPP_DPLL_NITRO_RBB		BIT(22)
#define OMAP4470_MPU_OPP_DPLL_NITROSB_RBB	BIT(23)
#define OMAP4470_IVA_OPP_DPLL_NITRO_RBB		BIT(24)
#define OMAP4470_IVA_OPP_DPLL_NITROSB_RBB	BIT(25)

/**
 * struct omap4_ldo_abb_trim_data - describe ABB trim bits for specific voltage
 * @volt_data:		voltage table
 * @volt_nominal:	voltage for which ABB type should be
 *			modified according to trim bits.
 * @rbb_trim_mask:	If this bit is set in trim register,
 *			ABB type should be modified to RBB.
 * @fbb_trim_mask:	If this bit is set in trim register,
 *			ABB type should be modified to FBB.
 */
struct omap4_ldo_abb_trim_data {
	struct omap_volt_data *volt_data;
	u32 volt_nominal;
	u32 rbb_trim_mask;
	u32 fbb_trim_mask;
};

/*
 * Structures containing OMAP4430 voltage supported and various
 * voltage dependent data for each VDD.
 */

#define OMAP4430_VDD_MPU_OPP50_UV		1025000
#define OMAP4430_VDD_MPU_OPP100_UV		1200000
#define OMAP4430_VDD_MPU_OPPTURBO_UV		1325000
#define OMAP4430_VDD_MPU_OPPNITRO_UV		1388000
/*
 * The 1mV difference is introduced just to distinguish OPPNITROSB in
 * OPP dependencies tables.
 */
#define OMAP4430_VDD_MPU_OPPNITROSB_UV		1389000

struct omap_volt_data omap443x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPP50_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPP100_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_MPU_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITROSB, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

#define OMAP4430_VDD_IVA_OPP50_UV		950000
#define OMAP4430_VDD_IVA_OPP100_UV		1114000
#define OMAP4430_VDD_IVA_OPPTURBO_UV		1291000

struct omap_volt_data omap443x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPP50_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPP100_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4430_VDD_IVA_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

#define OMAP4430_VDD_CORE_OPP50_UV		962000
#define OMAP4430_VDD_CORE_OPP100_UV		1127000

struct omap_volt_data omap443x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4430_VDD_CORE_OPP50_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(OMAP4430_VDD_CORE_OPP100_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
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
	{.main_vdd_volt = OMAP4430_VDD_MPU_OPPNITROSB_UV, .dep_vdd_volt = OMAP4430_VDD_CORE_OPP100_UV},
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

static struct device_info dsp_dev_info = {
	.hwmod_name	= "dsp_c0",
	.clk_name	= "virt_dpll_dsp_ck",
	.voltdm_name	= "iva",
};

static struct omap_opp_def omap443x_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&mpu_dev_info, true, 300000000, OMAP4430_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&mpu_dev_info, true, 600000000, OMAP4430_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&mpu_dev_info, true, 800000000, OMAP4430_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-NT */
	OPP_INITIALIZER(&mpu_dev_info, true, 1008000000, OMAP4430_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP5 - OPP-SB */
	OPP_INITIALIZER(&mpu_dev_info, false, 1200000000, OMAP4430_VDD_MPU_OPPNITROSB_UV),
	/* L3 OPP1 - OPP50 */
	OPP_INITIALIZER(&l3_dev_info, true, 100000000, OMAP4430_VDD_CORE_OPP50_UV),
	/* L3 OPP2 - OPP100, OPP-Turbo, OPP-SB */
	OPP_INITIALIZER(&l3_dev_info, true, 200000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* IVA OPP1 - OPP50 */
	OPP_INITIALIZER(&iva_dev_info, true, 133000000, OMAP4430_VDD_IVA_OPP50_UV),
	/* IVA OPP2 - OPP100 */
	OPP_INITIALIZER(&iva_dev_info, true, 266100000, OMAP4430_VDD_IVA_OPP100_UV),
	/* IVA OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&iva_dev_info, true, 332000000, OMAP4430_VDD_IVA_OPPTURBO_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4430_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4430_VDD_IVA_OPP100_UV),
	/* FDIF OPP1 - OPP50 */
	OPP_INITIALIZER(&fdif_dev_info, true, 64000000, OMAP4430_VDD_CORE_OPP50_UV),
	/* FDIF OPP2 - OPP100 */
	OPP_INITIALIZER(&fdif_dev_info, true, 128000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER(&dsp_dev_info, true, 232750000, OMAP4430_VDD_IVA_OPP50_UV),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER(&dsp_dev_info, true, 465500000, OMAP4430_VDD_IVA_OPP100_UV),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER(&dsp_dev_info, true, 496000000, OMAP4430_VDD_IVA_OPPTURBO_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4430_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4430_VDD_CORE_OPP100_UV),
	/* SGX OPP1 - OPP50 */
	OPP_INITIALIZER(&gpu_dev_info, true, 153600000, OMAP4430_VDD_CORE_OPP50_UV),
	/* SGX OPP2 - OPP100 */
	OPP_INITIALIZER(&gpu_dev_info, true, 307200000, OMAP4430_VDD_CORE_OPP100_UV),
	/* TODO: add DSS */
};

#define OMAP4460_VDD_MPU_OPP50_UV		1025000
#define OMAP4460_VDD_MPU_OPP100_UV		1203000
#define OMAP4460_VDD_MPU_OPPTURBO_UV		1317000
#define OMAP4460_VDD_MPU_OPPNITRO_UV		1380000
#define OMAP4460_VDD_MPU_OPPNITROSB_UV		1390000

struct omap_volt_data omap446x_vdd_mpu_volt_data[] = {
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_MPU_OPP50_UV, 10000, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPP100_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_MPU_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITROSB, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

#define OMAP4460_VDD_IVA_OPP50_UV		 950000
#define OMAP4460_VDD_IVA_OPP100_UV		1140000
#define OMAP4460_VDD_IVA_OPPTURBO_UV		1291000
#define OMAP4460_VDD_IVA_OPPNITRO_UV		1375000
#define OMAP4460_VDD_IVA_OPPNITROSB_UV		1376000

struct omap_volt_data omap446x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_IVA_OPP50_UV, 13000, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPP100_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITRO, 0xfa, 0x23, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(OMAP4460_VDD_IVA_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITROSB, 0xfa, 0x23, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

#define OMAP4460_VDD_CORE_OPP50_UV		 962000
#define OMAP4460_VDD_CORE_OPP100_UV		1127000
#define OMAP4460_VDD_CORE_OPP100_OV_UV		1250000

struct omap_volt_data omap446x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_CORE_OPP50_UV, 38000, OMAP44XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_CORE_OPP100_UV, 13000, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE_MARGIN(OMAP4460_VDD_CORE_OPP100_OV_UV, 13000, OMAP44XX_CONTROL_FUSE_CORE_OPP100OV, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
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

static struct omap4_ldo_abb_trim_data __initdata omap446x_ldo_abb_trim_data[] = {
	{.volt_data = omap446x_vdd_mpu_volt_data, .volt_nominal = OMAP4460_VDD_MPU_OPPTURBO_UV, .rbb_trim_mask = OMAP4460_MPU_OPP_DPLL_TURBO_RBB,
			.fbb_trim_mask = OMAP4460_MPU_OPP_DPLL_TRIM},
	{.volt_data = omap446x_vdd_iva_volt_data, .volt_nominal = OMAP4460_VDD_IVA_OPPTURBO_UV, .rbb_trim_mask = OMAP4460_IVA_OPP_DPLL_TURBO_RBB},
	{.volt_data = NULL},
};

static struct device_info omap4460_mpu_dev_info = {
	.hwmod_name	= "mpu",
	.clk_name	= "virt_dpll_mpu_ck",
	.voltdm_name	= "mpu",
};

static struct omap_opp_def omap446x_opp_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 350000000, OMAP4460_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 700000000, OMAP4460_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 920000000, OMAP4460_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 1200000000, OMAP4460_VDD_MPU_OPPNITRO_UV),
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
	OPP_INITIALIZER(&iva_dev_info, true, 430000000, OMAP4460_VDD_IVA_OPPNITRO_UV),
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
	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER(&dsp_dev_info, true, 232750000, OMAP4460_VDD_IVA_OPP50_UV),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER(&dsp_dev_info, true, 465500000, OMAP4460_VDD_IVA_OPP100_UV),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER(&dsp_dev_info, true, 496000000, OMAP4460_VDD_IVA_OPPTURBO_UV),
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
	/* TODO: Add DSS */
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
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPP50_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPP100_UV, OMAP44XX_CONTROL_FUSE_MPU_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITRO, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_MPU_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_MPU_OPPNITROSB, 0xfa, 0x27, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

#define OMAP4470_VDD_IVA_OPP50_UV		 962000
#define OMAP4470_VDD_IVA_OPP100_UV		1137000
#define OMAP4470_VDD_IVA_OPPTURBO_UV		1287000
#define OMAP4470_VDD_IVA_OPPNITRO_UV		1375000
#define OMAP4470_VDD_IVA_OPPNITROSB_UV		1380000

struct omap_volt_data omap447x_vdd_iva_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPP50_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP50, 0xf4, 0x0c, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPP100_UV, OMAP44XX_CONTROL_FUSE_IVA_OPP100, 0xf9, 0x16, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPPTURBO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPTURBO, 0xfa, 0x23, OMAP_ABB_NOMINAL_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPPNITRO_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITRO, 0xfa, 0x23, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(OMAP4470_VDD_IVA_OPPNITROSB_UV, OMAP44XX_CONTROL_FUSE_IVA_OPPNITROSB, 0xfa, 0x23, OMAP_ABB_FAST_OPP),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
};

#define OMAP4470_VDD_CORE_OPP50_UV		 980000
#define OMAP4470_VDD_CORE_OPP100_UV		1126000
#define OMAP4470_VDD_CORE_OPP100_OV_UV		1190000

struct omap_volt_data omap447x_vdd_core_volt_data[] = {
	VOLT_DATA_DEFINE(OMAP4470_VDD_CORE_OPP50_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP50, 0xf4, 0x0c, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(OMAP4470_VDD_CORE_OPP100_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP100, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(OMAP4470_VDD_CORE_OPP100_OV_UV, OMAP44XX_CONTROL_FUSE_CORE_OPP100OV, 0xf9, 0x16, OMAP_ABB_NO_LDO),
	VOLT_DATA_DEFINE(0, 0, 0, 0, 0),
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
	{.main_vdd_volt = OMAP4470_VDD_IVA_OPP100_UV, .dep_vdd_volt = OMAP4470_VDD_CORE_OPP100_UV},
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

static struct omap4_ldo_abb_trim_data __initdata omap447x_ldo_abb_trim_data[] = {
	{.volt_data = omap447x_vdd_mpu_volt_data, .volt_nominal = OMAP4470_VDD_MPU_OPPTURBO_UV, .rbb_trim_mask = OMAP4460_MPU_OPP_DPLL_TURBO_RBB},
	{.volt_data = omap447x_vdd_mpu_volt_data, .volt_nominal = OMAP4470_VDD_MPU_OPPNITRO_UV, .rbb_trim_mask = OMAP4470_MPU_OPP_DPLL_NITRO_RBB},
	{.volt_data = omap447x_vdd_mpu_volt_data, .volt_nominal = OMAP4470_VDD_MPU_OPPNITROSB_UV, .rbb_trim_mask = OMAP4470_MPU_OPP_DPLL_NITROSB_RBB},
	{.volt_data = omap447x_vdd_iva_volt_data, .volt_nominal = OMAP4470_VDD_IVA_OPPTURBO_UV, .rbb_trim_mask = OMAP4460_IVA_OPP_DPLL_TURBO_RBB},
	{.volt_data = omap447x_vdd_iva_volt_data, .volt_nominal = OMAP4470_VDD_IVA_OPPNITRO_UV, .rbb_trim_mask = OMAP4470_IVA_OPP_DPLL_NITRO_RBB},
	{.volt_data = omap447x_vdd_iva_volt_data, .volt_nominal = OMAP4470_VDD_IVA_OPPNITROSB_UV, .rbb_trim_mask = OMAP4470_IVA_OPP_DPLL_NITROSB_RBB},
	{.volt_data = NULL},
};

static struct device_info bb2d_dev_info = {
	.hwmod_name	= "bb2d",
	.clk_name	= "dpll_per_m6x2_ck",
	.voltdm_name	= "core",
};

#define OMAP4470_LP_OPP_SET_CORE_DPLL_FREQ 1600000000

static struct omap_opp_def omap447x_opp_low_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 396800000, OMAP4470_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 800000000, OMAP4470_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 1100000000, OMAP4470_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 1300000000, OMAP4470_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP5 - OPP-Nitro SpeedBin */
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
	OPP_INITIALIZER(&iva_dev_info, true, 430000000, OMAP4470_VDD_IVA_OPPNITRO_UV),
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
	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER(&dsp_dev_info, true, 232750000, OMAP4470_VDD_IVA_OPP50_UV),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER(&dsp_dev_info, true, 465500000, OMAP4470_VDD_IVA_OPP100_UV),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER(&dsp_dev_info, true, 496000000, OMAP4470_VDD_IVA_OPPTURBO_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4470_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4470_VDD_IVA_OPP100_UV),
	/* TODO: Add DSS */
};

static struct omap_opp_def omap447x_opp_high_def_list[] = {
	/* MPU OPP1 - OPP50 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 396800000, OMAP4470_VDD_MPU_OPP50_UV),
	/* MPU OPP2 - OPP100 */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 800000000, OMAP4470_VDD_MPU_OPP100_UV),
	/* MPU OPP3 - OPP-Turbo */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 1100000000, OMAP4470_VDD_MPU_OPPTURBO_UV),
	/* MPU OPP4 - OPP-Nitro */
	OPP_INITIALIZER(&omap4460_mpu_dev_info, true, 1300000000, OMAP4470_VDD_MPU_OPPNITRO_UV),
	/* MPU OPP5 - OPP-Nitro SpeedBin */
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
	OPP_INITIALIZER(&iva_dev_info, true, 430000000, OMAP4470_VDD_IVA_OPPNITRO_UV),
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
	/* DSP OPP1 - OPP50 */
	OPP_INITIALIZER(&dsp_dev_info, true, 232750000, OMAP4470_VDD_IVA_OPP50_UV),
	/* DSP OPP2 - OPP100 */
	OPP_INITIALIZER(&dsp_dev_info, true, 465500000, OMAP4470_VDD_IVA_OPP100_UV),
	/* DSP OPP3 - OPPTB */
	OPP_INITIALIZER(&dsp_dev_info, true, 496000000, OMAP4470_VDD_IVA_OPPTURBO_UV),
	/* HSI OPP1 - OPP50 */
	OPP_INITIALIZER(&hsi_dev_info, true, 96000000, OMAP4470_VDD_CORE_OPP50_UV),
	/* HSI OPP2 - OPP100 */
	OPP_INITIALIZER(&hsi_dev_info, true, 192000000, OMAP4470_VDD_CORE_OPP100_UV),
	/* ABE OPP1 - OPP50 */
	OPP_INITIALIZER(&abe_dev_info, true, 98304000, OMAP4470_VDD_IVA_OPP50_UV),
	/* ABE OPP2 - OPP100 */
	OPP_INITIALIZER(&abe_dev_info, true, 196608000, OMAP4470_VDD_IVA_OPP100_UV),
	/* TODO: Add DSS */
};

/*
 * opp_def_list_enable_opp() - enable opp by dev_info and frequency
 */
static int __init opp_def_list_enable_opp(struct omap_opp_def *list,
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
 * omap4_abb_update() - update the ABB map for a specific voltage in table
 * @vtable:	voltage table to update
 * @voltage:	voltage whose voltage data needs update
 * @opp_sel:	what ABB type should we update it to?
 */
static void __init omap4_abb_update(struct omap_volt_data *vtable,
				    unsigned long voltage, int opp_sel)
{
	/* scan through and update the voltage table */
	while (vtable->volt_nominal) {
		if (vtable->volt_nominal == voltage) {
			vtable->opp_sel = opp_sel;
			return;
		}
		vtable++;
	}
	/* WARN noticably to get the developer to fix */
	WARN(1, "%s: voltage %ld could not be set to ABB %d\n",
	     __func__, voltage, opp_sel);
}

/**
 * omap4_abb_trim_update() - update the ABB mapping quirks for OMAP4460/4470
 * @trim_data:	trim data to adjust
 */
static void __init omap4_abb_trim_update(
		struct omap4_ldo_abb_trim_data *trim_data)
{
	u32 reg;
	int opp_sel;

	if (!trim_data) {
		pr_err("%s: Trim data is not valid\n", __func__);
		return;
	}

	reg = omap_ctrl_readl(OMAP4_CTRL_MODULE_CORE_STD_FUSE_OPP_DPLL_1);

	/*
	 * Update ABB configuration if at least one of trim bits is set
	 * Leave default configuration in opposite case.
	 */
	for (; trim_data->volt_data; trim_data++) {
		if (reg & trim_data->rbb_trim_mask)
			opp_sel = OMAP_ABB_SLOW_OPP;
		else if (reg & trim_data->fbb_trim_mask)
			opp_sel = OMAP_ABB_FAST_OPP;
		else
			continue;

		omap4_abb_update(trim_data->volt_data, trim_data->volt_nominal,
				 opp_sel);
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

	if (cpu_is_omap443x()) {
		if (omap4_has_perf_silicon()) {
			opp_def_list_enable_opp(omap443x_opp_def_list,
					ARRAY_SIZE(omap443x_opp_def_list),
					&mpu_dev_info,
					1200000000, true);
		}

		r = omap_init_opp_table(omap443x_opp_def_list,
			ARRAY_SIZE(omap443x_opp_def_list));
	} else if (cpu_is_omap446x()) {
		omap4_abb_trim_update(omap446x_ldo_abb_trim_data);
		if (omap4_has_perf_silicon()) {
			opp_def_list_enable_opp(omap446x_opp_def_list,
					ARRAY_SIZE(omap446x_opp_def_list),
					&omap4460_mpu_dev_info,
					1500000000, true);
			opp_def_list_enable_opp(omap446x_opp_def_list,
					ARRAY_SIZE(omap446x_opp_def_list),
					&iva_dev_info,
					500000000, true);
		}
		r = omap_init_opp_table(omap446x_opp_def_list,
			ARRAY_SIZE(omap446x_opp_def_list));
	} else if (cpu_is_omap447x()) {
		struct clk *dpll_core_ck;
		unsigned long rate = 0;

		omap4_abb_trim_update(omap447x_ldo_abb_trim_data);

		dpll_core_ck = clk_get(NULL, "dpll_core_ck");
		BUG_ON(IS_ERR_OR_NULL(dpll_core_ck));

		rate = clk_get_rate(dpll_core_ck);
		clk_put(dpll_core_ck);

		if (rate > OMAP4470_LP_OPP_SET_CORE_DPLL_FREQ / 2) {
			if (omap4_has_perf_silicon()) {
				opp_def_list_enable_opp(
					omap447x_opp_high_def_list,
					ARRAY_SIZE(omap447x_opp_high_def_list),
					&omap4460_mpu_dev_info,
					1500000000, true);
				opp_def_list_enable_opp(
					omap447x_opp_high_def_list,
					ARRAY_SIZE(omap447x_opp_high_def_list),
					&iva_dev_info,
					500000000, true);
			}
			r = omap_init_opp_table(omap447x_opp_high_def_list,
					ARRAY_SIZE(omap447x_opp_high_def_list));
		} else {
			if (omap4_has_perf_silicon()) {
				opp_def_list_enable_opp(
					omap447x_opp_low_def_list,
					ARRAY_SIZE(omap447x_opp_low_def_list),
					&omap4460_mpu_dev_info,
					1500000000, true);
				opp_def_list_enable_opp(
					omap447x_opp_low_def_list,
					ARRAY_SIZE(omap447x_opp_low_def_list),
					&iva_dev_info,
					500000000, true);
			}

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
