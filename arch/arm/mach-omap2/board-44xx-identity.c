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

#include <plat/omap_apps_brd_id.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>

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
	[OMAP4_TABLET_1_0]		= "Tablet",
	[OMAP4_TABLET_2_0]		= "Tablet2",
	[OMAP4_BLAZE_ID]		= "Blaze/SDP",
	[OMAP4_PANDA_ID]		= "Panda",
};

static ssize_t omap4_board_rev_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	int apps_brd_rev = 0;

	apps_brd_rev = omap_get_board_version();
	if (apps_brd_rev < 0)
		return -EINVAL;

	return sprintf(buf, "%s\n", app_board_rev_types[apps_brd_rev]);
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

static OMAP4_BOARD_ATTR_RO(board_rev, omap4_board_rev_show);

static struct attribute *omap4_soc_prop_attrs[] = {
	&omap4_soc_prop_attr_family.attr,
	&omap4_soc_prop_attr_revision.attr,
	&omap4_soc_prop_attr_type.attr,
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
