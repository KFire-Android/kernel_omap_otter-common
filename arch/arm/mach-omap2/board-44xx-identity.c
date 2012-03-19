/* OMAP Identity file for OMAP4 boards.
 *
 * Copyright (C) 2011 Google, Inc.
 * Copyright (C) 2010 Texas Instruments
 *
 * Based on mach-omap2/board-omap4panda.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <mach/hardware.h>
#include <mach/omap4-common.h>
#include <mach/id.h>

#include <plat/omap_apps_brd_id.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

#include "control.h"

static ssize_t omap4_soc_family_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "OMAP%04x\n", GET_OMAP_TYPE);
}

static ssize_t omap4_soc_revision_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "ES%d.%d\n", (GET_OMAP_REVISION() >> 4) & 0xf,
		       GET_OMAP_REVISION() & 0xf);
}

static const char *omap_types[] = {
	[OMAP2_DEVICE_TYPE_TEST]	= "TST",
	[OMAP2_DEVICE_TYPE_EMU]		= "EMU",
	[OMAP2_DEVICE_TYPE_SEC]		= "HS",
	[OMAP2_DEVICE_TYPE_GP]		= "GP",
	[OMAP2_DEVICE_TYPE_BAD]		= "BAD",
};

static ssize_t omap4_soc_type_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", omap_types[omap_type()]);
}

static const char *app_board_rev_types[] = {
	[OMAP4_TABLET_1_0_ID]		= "Tablet 1.0",
	[OMAP4_TABLET_1_1_ID]		= "Tablet 1.1",
	[OMAP4_TABLET_1_2_ID]		= "Tablet 1.2",
	[OMAP4_TABLET_2_0_ID]		= "Tablet 2.0",
	[OMAP4_TABLET_2_1_ID]		= "Tablet 2.1",
	[OMAP4_TABLET_2_1_1_ID]		= "Tablet 2.1.1",
	[OMAP4_BLAZE_ID]		= "Blaze/SDP",
	[OMAP4_PANDA_ID]		= "Panda",
	[OMAP4_MAX_ID]			= "Unknown",
};

static ssize_t omap4_prod_id_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct  omap_die_id opi;
	omap_get_production_id(&opi);
	return sprintf(buf, "%08X-%08X\n", opi.id_1, opi.id_0);
}

static ssize_t omap4_die_id_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct  omap_die_id opi;
	omap_get_die_id(&opi);
	return sprintf(buf, "%08X-%08X-%08X-%08X\n", opi.id_3,
						opi.id_2, opi.id_1, opi.id_0);
}

static ssize_t omap4_board_rev_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	int apps_brd_rev = 0;

	apps_brd_rev = omap_get_board_id();

	if (apps_brd_rev < 0)
		return -EINVAL;

	if (apps_brd_rev > OMAP4_MAX_ID)
		apps_brd_rev = OMAP4_MAX_ID;

	return sprintf(buf, "%s\n", app_board_rev_types[apps_brd_rev]);
}

static ssize_t omap4_soc_type_max_freq(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	char *max_freq = "unknown";

	if (omap4_has_mpu_1_5ghz())
		max_freq = "1.5Ghz";
	else if (omap4_has_mpu_1_3ghz())
		max_freq = "1.3Ghz";
	else if (omap4_has_mpu_1_2ghz())
		max_freq = "1.2Ghz";
	else
		max_freq = "1.0Ghz";

	return sprintf(buf, "%s\n", max_freq);
}

static ssize_t omap4_soc_dpll_trimmed(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	int trimmed;
	char *trim_freq = "2.0GhZ";

	trimmed = omap_readl(0x4a002268) & ((1 << 18) | (1 << 19));

	if ((trimmed & (1 << 18)) && (trimmed & (1 << 19)))
		trim_freq = "3.0GHz";
	else if (trimmed & (1 << 18))
		trim_freq = "2.4GHz";

	pr_info("%s:DPLL is trimmed to %s\n", __func__, trim_freq);

	return sprintf(buf, "%s\n", trim_freq);
}

static ssize_t omap4_soc_rbb_trimmed(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	int trimmed = 1;
	int reg = 0;

	if (cpu_is_omap446x()) {
		/* read eFuse register here */
		reg = omap_ctrl_readl(OMAP4_CTRL_MODULE_CORE_STD_FUSE_OPP_DPLL_1);
		trimmed = reg & (1 << 20);
		pr_info("%s:RBB is %s set\n", __func__, trimmed ? "" : "not");

		return sprintf(buf, "%s\n", trimmed ? "true" : "false");
	}

	return sprintf(buf, "%s\n", "Not Available");
}

#define OMAP4_SOC_ATTR_RO(_name, _show) \
	struct kobj_attribute omap4_soc_prop_attr_##_name = \
		__ATTR(_name, S_IRUGO, _show, NULL)

#define OMAP4_BOARD_ATTR_RO(_name, _show) \
	struct kobj_attribute omap4_board_prop_attr_##_name = \
		__ATTR(_name, S_IRUGO, _show, NULL)

static OMAP4_SOC_ATTR_RO(family, omap4_soc_family_show);
static OMAP4_SOC_ATTR_RO(revision, omap4_soc_revision_show);
static OMAP4_SOC_ATTR_RO(type, omap4_soc_type_show);
static OMAP4_SOC_ATTR_RO(max_freq, omap4_soc_type_max_freq);
static OMAP4_SOC_ATTR_RO(dpll_trimmed, omap4_soc_dpll_trimmed);
static OMAP4_SOC_ATTR_RO(rbb_trimmed, omap4_soc_rbb_trimmed);
static OMAP4_SOC_ATTR_RO(production_id, omap4_prod_id_show);
static OMAP4_SOC_ATTR_RO(die_id, omap4_die_id_show);

static OMAP4_BOARD_ATTR_RO(board_rev, omap4_board_rev_show);

static struct attribute *omap4_soc_prop_attrs[] = {
	&omap4_soc_prop_attr_family.attr,
	&omap4_soc_prop_attr_revision.attr,
	&omap4_soc_prop_attr_type.attr,
	&omap4_soc_prop_attr_max_freq.attr,
	&omap4_soc_prop_attr_dpll_trimmed.attr,
	&omap4_soc_prop_attr_rbb_trimmed.attr,
	&omap4_soc_prop_attr_production_id.attr,
	&omap4_soc_prop_attr_die_id.attr,
	NULL,
};

static struct attribute *omap4_board_prop_attrs[] = {
	&omap4_board_prop_attr_board_rev.attr,
	NULL,
};

static struct attribute_group omap4_soc_prop_attr_group = {
	.attrs = omap4_soc_prop_attrs,
};

static struct attribute_group omap4_board_prop_attr_group = {
	.attrs = omap4_board_prop_attrs,
};

void __init omap4_create_board_props(void)
{
	struct kobject *board_props_kobj;
	struct kobject *soc_kobj = NULL;
	struct kobject *board_kobj = NULL;
	int ret = 0;

	board_props_kobj = kobject_create_and_add("board_properties", NULL);
	if (!board_props_kobj)
		goto err_board_obj;

	soc_kobj = kobject_create_and_add("soc", board_props_kobj);
	if (!soc_kobj)
		goto err_soc_obj;

	board_kobj = kobject_create_and_add("board", board_props_kobj);
	if (!board_kobj)
		goto err_app_board_obj;

	ret = sysfs_create_group(soc_kobj, &omap4_soc_prop_attr_group);
	if (ret)
		goto err_soc_sysfs_create;

	ret = sysfs_create_group(board_kobj, &omap4_board_prop_attr_group);
	if (ret)
		goto err_board_sysfs_create;

	return;

err_board_sysfs_create:
	sysfs_remove_group(soc_kobj, &omap4_soc_prop_attr_group);
err_soc_sysfs_create:
	kobject_put(board_kobj);
err_app_board_obj:
	kobject_put(soc_kobj);
err_soc_obj:
	kobject_put(board_props_kobj);
err_board_obj:
	if (!board_props_kobj || !soc_kobj || ret)
		pr_err("failed to create board_properties\n");
}
