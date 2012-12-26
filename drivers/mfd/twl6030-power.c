/*
 * Handling for Resource Mapping for TWL6030 Family of chips
 *
 * Copyright (C) 2011-2012 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
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
#include <linux/pm.h>
#include <linux/i2c/twl.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>

#include <asm/mach-types.h>

#define PREQ_RES_ASS_A		0
#define PREQ_RES_ASS_B		1
#define PREQ_RES_ASS_C		2
#define PREQ_RES_ASS_REG_CNT	9

#define VREG_GRP		0
#define MSK_TRANSITION_APP_SHIFT	0x5

static u8 dev_on_group = DEV_GRP_NULL;

/**
 * struct twl6030_resource_map - describe the resource mapping for TWL6030
 * @name:	name of the resource
 * @res_id:	resource ID
 * @base_addr:	base address
 * @group:	which device group can control this resource?
 */
struct twl6030_resource_map {
	char *name;
	u8 res_id;
	u8 base_addr;
	u8 group;
};

/**
 * struct twl6032_resource_map - describe the resource mapping for TWL6032
 * @name:	name of the resource
 * @res_id:	resource ID
 * @reg_type:	type of the Resources Assignment register:
 *	0: PREQx_RES_ASS_A register
 *	1: PREQx_RES_ASS_B register
 *	2: PREQx_RES_ASS_C register
 * @group:	resource control group
 * @mask:	bit mask of the resource in PREQx_RES_ASS_x registers
 */
struct twl6032_resource_map {
	char *name;
	u8 res_id;
	u8 reg_type;
	u8 group;
	u8 mask;
};

#define TWL6032_RES_DATA(ID, NAME, REG_TYPE, GROUP, MASK) \
		{.res_id = ID, .name = NAME, .reg_type = REG_TYPE,\
		.group = GROUP, .mask = MASK}

/* list of all s/w modifiable resources in TWL6030 */
static __devinitdata struct twl6030_resource_map twl6030_res_map[] = {
{.res_id = RES_V1V29, .name = "V1V29", .base_addr = 0x40, .group = DEV_GRP_P1,},
{.res_id = RES_V1V8, .name = "V1V8", .base_addr = 0x46, .group = DEV_GRP_P1,},
{.res_id = RES_V2V1, .name = "V2V1", .base_addr = 0x4c, .group = DEV_GRP_P1,},
{.res_id = RES_VDD1, .name = "CORE1", .base_addr = 0x52, .group = DEV_GRP_P1,},
{.res_id = RES_VDD2, .name = "CORE2", .base_addr = 0x58, .group = DEV_GRP_P1,},
{.res_id = RES_VDD3, .name = "CORE3", .base_addr = 0x5e, .group = DEV_GRP_P1,},
{.res_id = RES_VMEM, .name = "VMEM", .base_addr = 0x64, .group = DEV_GRP_P1,},
/* VANA cannot be modified */
{.res_id = RES_VUAX1, .name = "VUAX1", .base_addr = 0x84, .group = DEV_GRP_P1,},
{.res_id = RES_VAUX2, .name = "VAUX2", .base_addr = 0x88, .group = DEV_GRP_P1,},
{.res_id = RES_VAUX3, .name = "VAUX3", .base_addr = 0x8c, .group = DEV_GRP_P1,},
{.res_id = RES_VCXIO, .name = "VCXIO", .base_addr = 0x90, .group = DEV_GRP_P1,},
{.res_id = RES_VDAC, .name = "VDAC", .base_addr = 0x94, .group = DEV_GRP_P1,},
{.res_id = RES_VMMC1, .name = "VMMC", .base_addr = 0x98, .group = DEV_GRP_P1,},
{.res_id = RES_VPP, .name = "VPP", .base_addr = 0x9c, .group = DEV_GRP_P1,},
/* VRTC cannot be modified */
{.res_id = RES_VUSBCP, .name = "VUSB", .base_addr = 0xa0, .group = DEV_GRP_P1,},
{.res_id = RES_VSIM, .name = "VSIM", .base_addr = 0xa4, .group = DEV_GRP_P1,},
{.res_id = RES_REGEN, .name = "REGEN1", .base_addr = 0xad,
							.group = DEV_GRP_P1,},
{.res_id = RES_REGEN2, .name = "REGEN2", .base_addr = 0xb0,
							.group = DEV_GRP_P1,},
{.res_id = RES_SYSEN, .name = "SYSEN", .base_addr = 0xb3, .group = DEV_GRP_P1,},
/* NRES_PWRON cannot be modified */
/* 32KCLKAO cannot be modified */
{.res_id = RES_32KCLKG, .name = "32KCLKG", .base_addr = 0xbc,
							.group = DEV_GRP_P1,},
{.res_id = RES_32KCLKAUDIO, .name = "32KCLKAUDIO", .base_addr = 0xbf,
							.group = DEV_GRP_P1,},
/* BIAS cannot be modified */
/* VBATMIN_HI cannot be modified */
/* RC6MHZ cannot be modified */
/* TEMP cannot be modified */
};

/* list of all s/w modifiable resources in TWL6032 */
static __devinitdata struct twl6032_resource_map twl6032_res_map[] = {
/* PREQx_RES_ASS_A register resources */
TWL6032_RES_DATA(RES_LDOUSB, "VUSB", PREQ_RES_ASS_A, DEV_GRP_P1, BIT(5)),
TWL6032_RES_DATA(RES_SMPS5, "SMPS5", PREQ_RES_ASS_A, DEV_GRP_P1, BIT(4)),
TWL6032_RES_DATA(RES_SMPS4, "SMPS4", PREQ_RES_ASS_A, DEV_GRP_P1, BIT(3)),
TWL6032_RES_DATA(RES_SMPS3, "SMPS3", PREQ_RES_ASS_A, DEV_GRP_P1, BIT(2)),
TWL6032_RES_DATA(RES_SMPS2, "SMPS2", PREQ_RES_ASS_A, DEV_GRP_P1, BIT(1)),
TWL6032_RES_DATA(RES_SMPS1, "SMPS1", PREQ_RES_ASS_A, DEV_GRP_P1, BIT(0)),
/* PREQx_RES_ASS_B register resources */
TWL6032_RES_DATA(RES_LDOLN, "LDOLN", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(7)),
TWL6032_RES_DATA(RES_LDO7, "LDO7", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(6)),
TWL6032_RES_DATA(RES_LDO6, "LDO6", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(5)),
TWL6032_RES_DATA(RES_LDO5, "LDO5", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(4)),
TWL6032_RES_DATA(RES_LDO4, "LDO4", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(3)),
TWL6032_RES_DATA(RES_LDO3, "LDO3", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(2)),
TWL6032_RES_DATA(RES_LDO2, "LDO2", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(1)),
TWL6032_RES_DATA(RES_LDO1, "LDO1", PREQ_RES_ASS_B, DEV_GRP_P1, BIT(0)),
/* PREQx_RES_ASS_C register resources */
TWL6032_RES_DATA(RES_VSYSMIN_HI, "VSYSMIN_HI", PREQ_RES_ASS_C, DEV_GRP_P1, BIT(5)),
TWL6032_RES_DATA(RES_32KCLKG, "32KCLKG", PREQ_RES_ASS_C, DEV_GRP_P1, BIT(4)),
TWL6032_RES_DATA(RES_32KCLKAUDIO, "32KCLKAUDIO", PREQ_RES_ASS_C, DEV_GRP_P1, BIT(3)),
TWL6032_RES_DATA(RES_SYSEN, "SYSEN", PREQ_RES_ASS_C, DEV_GRP_P1, BIT(2)),
TWL6032_RES_DATA(RES_REGEN2, "REGEN2", PREQ_RES_ASS_C, DEV_GRP_P1, BIT(1)),
TWL6032_RES_DATA(RES_REGEN, "REGEN1", PREQ_RES_ASS_C, DEV_GRP_P1, BIT(0)),
};

static __devinitdata struct twl4030_system_config twl6030_sys_config[] = {
	{.name = "DEV_ON", .group =  DEV_GRP_P1,},
};

/* Actual power groups that TWL understands */
#define P3_GRP_6030	BIT(2)		/* secondary processor, modem, etc */
#define P2_GRP_6030	BIT(1)		/* "peripherals" */
#define P1_GRP_6030	BIT(0)		/* CPU/Linux */

static __devinit void twl6030_process_system_config(void)
{
	u8 grp;
	int r;

	struct twl4030_system_config *sys_config;
	sys_config = twl6030_sys_config;

	while (sys_config && sys_config->name) {
		if (!strcmp(sys_config->name, "DEV_ON")) {
			dev_on_group = sys_config->group;
			break;
		}
		sys_config++;
	}
	if (dev_on_group == DEV_GRP_NULL)
		pr_err("%s: Couldn't find DEV_ON resource configuration!"
			" MOD & CON group would be kept active.\n", __func__);

	if (dev_on_group) {
		r = twl_i2c_read_u8(TWL_MODULE_PM_MASTER, &grp,
				TWL6030_PHOENIX_DEV_ON);
		if (r) {
			pr_err("%s: Error(%d) reading  {addr=0x%02x}",
				__func__, r, TWL6030_PHOENIX_DEV_ON);
			/*
			 * On error resetting to 0, so that all the process
			 * groups are kept active.
			 */
			dev_on_group = DEV_GRP_NULL;
		} else {
			/*
			 * Unmapped processor groups are disabled by writing
			 * 1 to corresponding group in DEV_ON.
			 */
			grp |= (dev_on_group & DEV_GRP_P1) ? 0 : P1_GRP_6030;
			grp |= (dev_on_group & DEV_GRP_P2) ? 0 : P2_GRP_6030;
			grp |= (dev_on_group & DEV_GRP_P3) ? 0 : P3_GRP_6030;
			dev_on_group = grp;
		}

		/*
		 *  unmask PREQ transition Executes ACT2SLP and SLP2ACT sleep
		 *   sequence
		 */
		r = twl_i2c_read_u8(TWL_MODULE_PM_MASTER, &grp,
				TWL6030_PM_MASTER_MSK_TRANSITION);
		if (r) {
			pr_err("%s: Error (%d) reading"
				" TWL6030_MSK_TRANSITION\n", __func__, r);
			return;
		}

		grp &= (dev_on_group << MSK_TRANSITION_APP_SHIFT);

		r = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, grp,
					TWL6030_PM_MASTER_MSK_TRANSITION);
		if (r)
			pr_err("%s: Error (%d) writing to"
				" TWL6030_MSK_TRANSITION\n", __func__, r);
	}
}

static __devinit void twl6030_program_map(void)
{
	struct twl6030_resource_map *res = twl6030_res_map;
	int r, i;

	for (i = 0; i < ARRAY_SIZE(twl6030_res_map); i++) {
		u8 grp = 0;

		/* map back from generic device id to TWL6030 ID */
		grp |= (res->group & DEV_GRP_P1) ? P1_GRP_6030 : 0;
		grp |= (res->group & DEV_GRP_P2) ? P2_GRP_6030 : 0;
		grp |= (res->group & DEV_GRP_P3) ? P3_GRP_6030 : 0;

		r = twl_i2c_write_u8(TWL6030_MODULE_ID0, grp,
				     res->base_addr);
		if (r)
			pr_err("%s: Error(%d) programming map %s {addr=0x%02x},"
			       "grp=0x%02X\n", __func__, r, res->name,
			       res->base_addr, res->group);
		res++;
	}
}

static __devinit void twl6032_program_map(void)
{
	struct twl6032_resource_map *res = twl6032_res_map;
	int r, i;
	u8 grp, val[1 + PREQ_RES_ASS_REG_CNT] = {0};
	/**
	 * val array structure:
	 * val[0]: address byte for twl_i2c_write
	 * val[1]-val[3]: PREQ1_RES_ASS_A-PREQ1_RES_ASS_C Registers
	 * val[4]-val[6]: PREQ2_RES_ASS_A-PREQ2_RES_ASS_C Registers
	 * val[7]-val[9]: PREQ3_RES_ASS_A-PREQ3_RES_ASS_C Registers
	 */

	/* fill array according to the resource bit fields
	 * in PMIC registers */
	for (i = 0; i < ARRAY_SIZE(twl6032_res_map); i++) {
		for (grp = 0; grp < DEV_GRP_CNT; grp++)
			/* map back from generic device id to TWL6032 mask */
			val[1 + grp * DEV_GRP_CNT + res->reg_type] |=
				(res->group & BIT(grp)) ? res->mask : 0;
		res++;
	}

	/* load full array to PMIC registers */
	r = twl_i2c_write(TWL6030_MODULE_ID0, &val[0],
		TWL6032_PREQ1_RES_ASS_A, PREQ_RES_ASS_REG_CNT);
	if (r)
		pr_err("%s: Error(%d) programming TWL6032 PREQ Assignment Registers {start addr=0x%02x}\n",
		       __func__, r, TWL6032_PREQ1_RES_ASS_A);
}

static __devinit void twl6030_update_system_map
			(struct twl4030_system_config *sys_list)
{
	int i;
	struct twl4030_system_config *sys_res;

	while (sys_list && sys_list->name)  {
		sys_res = twl6030_sys_config;
		for (i = 0; i < ARRAY_SIZE(twl6030_sys_config); i++) {
			if (!strcmp(sys_res->name, sys_list->name))
				sys_res->group = sys_list->group &
					(DEV_GRP_P1 | DEV_GRP_P2 | DEV_GRP_P3);
			sys_res++;
		}
		sys_list++;
	}
}

static __devinit void twl6030_update_map(struct twl4030_resconfig *res_list)
{
	int i, res_idx = 0;
	struct twl6030_resource_map *res;

	while (res_list->resource != TWL4030_RESCONFIG_UNDEF) {
		res = twl6030_res_map;
		for (i = 0; i < ARRAY_SIZE(twl6030_res_map); i++) {
			if (res->res_id == res_list->resource) {
				res->group = res_list->devgroup &
				    (DEV_GRP_P1 | DEV_GRP_P2 | DEV_GRP_P3);
				break;
			}
			res++;
		}

		if (i == ARRAY_SIZE(twl6030_res_map)) {
			pr_err("%s: in platform_data resource index %d, cannot"
			       " find match for resource 0x%02x. NO Update!\n",
			       __func__, res_idx, res_list->resource);
		}
		res_list++;
		res_idx++;
	}
}

static __devinit void twl6032_update_map(struct twl4030_resconfig *res_list)
{
	int i, res_idx = 0;
	struct twl6032_resource_map *res;

	while (res_list->resource != TWL4030_RESCONFIG_UNDEF) {
		res = twl6032_res_map;
		for (i = 0; i < ARRAY_SIZE(twl6032_res_map); i++) {
			if (res->res_id == res_list->resource) {
				res->group = res_list->devgroup &
					(DEV_GRP_P1 | DEV_GRP_P2 | DEV_GRP_P3);
				break;
			}
			res++;
		}

		if (i == ARRAY_SIZE(twl6032_res_map)) {
			pr_err("%s: in platform_data resource index %d, cannot find match for resource 0x%02x. NO Update!\n",
			       __func__, res_idx, res_list->resource);
		}
		res_list++;
		res_idx++;
	}
}

static int twl6030_power_notifier_cb(struct notifier_block *notifier,
					unsigned long pm_event,  void *unused)
{
	int r = 0;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		r = twl_i2c_write_u8(TWL_MODULE_PM_MASTER, dev_on_group,
				TWL6030_PHOENIX_DEV_ON);
		if (r)
			pr_err("%s: Error(%d) programming {addr=0x%02x}",
				__func__, r, TWL6030_PHOENIX_DEV_ON);
		break;
	}

	return notifier_from_errno(r);
}

static struct notifier_block twl6030_power_pm_notifier = {
	.notifier_call = twl6030_power_notifier_cb,
};

/**
 * twl6030_power_init() - Update the power map to reflect connectivity of board
 * @power_data:	power resource map to update (OPTIONAL) - use this if a resource
 *		is used by other devices other than APP (DEV_GRP_P1)
 */
void __devinit twl6030_power_init(struct twl4030_power_data *power_data,
		unsigned long features)
{
	int r;

	if (power_data && (!power_data->resource_config &&
					!power_data->sys_config)) {
		pr_err("%s: power data from platform without configuration!\n",
		       __func__);
		return;
	}

	if (power_data && power_data->resource_config) {
		if (features & TWL6032_SUBCLASS)
			twl6032_update_map(power_data->resource_config);
		else
			twl6030_update_map(power_data->resource_config);
	}

	if (power_data && power_data->sys_config)
		twl6030_update_system_map(power_data->sys_config);

	twl6030_process_system_config();

	if (features & TWL6032_SUBCLASS)
		twl6032_program_map();
	else
		twl6030_program_map();

	r = register_pm_notifier(&twl6030_power_pm_notifier);
	if (r)
		pr_err("%s: twl6030 power registration failed!\n", __func__);

	return;
}
